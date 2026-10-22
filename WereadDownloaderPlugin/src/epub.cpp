#include "epub.h"
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUrl>
#include <QtEndian>
#include <algorithm>
#include <stdexcept>
#include <zlib.h>

namespace WereadDownloader {
namespace {
constexpr quint32 limit = 64 * 1024 * 1024;
const QString dc = QStringLiteral("http://purl.org/dc/elements/1.1/");
const QString opf = QStringLiteral("http://www.idpf.org/2007/opf");
const QString prefix = QStringLiteral("urn:weread:book:");
void need(bool test, const char *reason) { if (!test) throw std::runtime_error(reason); }
quint16 u16(const QByteArray &b, int p) {
    need(p >= 0 && p + 2 <= b.size(), "truncated ZIP");
    return qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(b.constData() + p));
}
quint32 u32(const QByteArray &b, int p) {
    need(p >= 0 && p + 4 <= b.size(), "truncated ZIP");
    return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(b.constData() + p));
}
void put16(QByteArray &b, quint16 n) { char t[2]; qToLittleEndian(n, t); b.append(t, 2); }
void put32(QByteArray &b, quint32 n) { char t[4]; qToLittleEndian(n, t); b.append(t, 4); }
bool safeName(const QByteArray &name) {
    if (name.isEmpty() || name.contains('\0') || name.contains('\\') || name.startsWith('/')
        || name.contains(':') || name.contains('\n') || name.contains('\r')) return false;
    for (const auto &part : name.split('/')) if (part == ".." || part == ".") return false;
    return true;
}
QDomDocument xml(const QByteArray &bytes) {
    need(!bytes.contains("<!DOCTYPE") && !bytes.contains("<!ENTITY"), "unsupported XML declaration");
    QDomDocument doc;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    need(bool(doc.setContent(bytes, QDomDocument::ParseOption::UseNamespaceProcessing)), "invalid EPUB XML");
#else
    need(doc.setContent(bytes, true), "invalid EPUB XML");
#endif
    return doc;
}
QByteArray resolve(const QByteArray &base, const QString &href) {
    QUrl url(href);
    need(url.isRelative() && !url.hasQuery() && !href.contains('\\'), "unsupported EPUB resource");
    QString decoded = QUrl::fromPercentEncoding(url.path(QUrl::FullyEncoded).toUtf8());
    need(!decoded.startsWith('/'), "absolute EPUB resource");
    auto name = QDir::cleanPath(QString::fromUtf8(base) + decoded).toUtf8();
    need(safeName(name), "unsafe EPUB resource");
    return name;
}
}

bool Epub::validBookId(const QString &id) {
    return QRegularExpression(QStringLiteral("\\A[A-Za-z0-9_-]{1,128}\\z")).match(id).hasMatch();
}
Epub::Epub(const QString &path) {
    QFileInfo info(path);
    need(info.isFile() && !info.isSymLink() && info.size() > 0 && info.size() <= limit, "invalid EPUB file");
    QFile f(path); need(f.open(QIODevice::ReadOnly), "cannot open EPUB");
    const auto b = f.read(limit + 1);
    need(b.size() <= int(limit), "EPUB too large");
    int end = -1;
    for (int i = b.size() - 22; i >= qMax(0, b.size() - 65557); --i) {
        if (b.mid(i, 4) == QByteArray("PK\5\6", 4) && i + 22 + u16(b, i + 20) == b.size()) { end = i; break; }
    }
    need(end >= 0, "missing ZIP directory");
    need(u16(b, end + 4) == 0 && u16(b, end + 6) == 0
        && u16(b, end + 8) == u16(b, end + 10), "multi-disk ZIP unsupported");
    const quint32 start = u32(b, end + 16), bytes = u32(b, end + 12);
    const int count = u16(b, end + 10);
    need(count > 0 && count < 10000 && quint64(start) + bytes == quint64(end), "invalid ZIP directory");
    int cursor = int(start);
    quint64 expanded = 0;
    QSet<QByteArray> names;
    QVector<QPair<quint32, quint32>> regions;
    for (int i = 0; i < count; ++i) {
        need(u32(b, cursor) == 0x02014b50 && cursor + 46 <= end, "bad ZIP entry");
        const quint16 flags = u16(b, cursor + 8), method = u16(b, cursor + 10);
        const quint32 crc = u32(b, cursor + 16), packed = u32(b, cursor + 20), size = u32(b, cursor + 24);
        const int length = u16(b, cursor + 28), extra = u16(b, cursor + 30), comment = u16(b, cursor + 32);
        const quint32 offset = u32(b, cursor + 42), mode = u32(b, cursor + 38) >> 16;
        if (i == 0) need(offset == 0, "EPUB mimetype must be first");
        need(cursor + 46 + length + extra + comment <= end && length > 0, "bad ZIP lengths");
        const auto name = b.mid(cursor + 46, length);
        need(safeName(name) && !names.contains(name), "unsafe or duplicate ZIP entry");
        names.insert(name);
        need((flags & ~quint16(0x808)) == 0 && (method == 0 || method == 8)
            && (mode & 0170000) != 0120000 && u16(b, cursor + 34) == 0, "unsupported ZIP entry");
        expanded += size;
        need(expanded <= limit && packed <= limit && offset < start, "EPUB expansion limit");
        need(u32(b, int(offset)) == 0x04034b50 && u16(b, int(offset) + 6) == flags
            && u16(b, int(offset) + 8) == method, "ZIP header mismatch");
        const quint32 data = offset + 30 + u16(b, int(offset) + 26) + u16(b, int(offset) + 28);
        need(quint64(data) + packed <= start && b.mid(int(offset) + 30, u16(b, int(offset) + 26)) == name,
            "ZIP payload mismatch");
        if (!(flags & 8)) need(u32(b, int(offset) + 14) == crc && u32(b, int(offset) + 18) == packed
            && u32(b, int(offset) + 22) == size, "ZIP size mismatch");
        regions.append({offset, data + packed});
        m_entries.append({name, b.mid(int(data), int(packed)), method, crc, size});
        cursor += 46 + length + extra + comment;
    }
    need(cursor == end, "ZIP directory length mismatch");
    std::sort(regions.begin(), regions.end());
    for (int i = 1; i < regions.size(); ++i) need(regions[i].first >= regions[i - 1].second, "overlapping ZIP data");
    need(m_entries.first().name == "mimetype" && m_entries.first().method == 0
        && regions.first().first == 0, "EPUB mimetype must be first and stored");
    // Verify CRCs for every entry, not just the XML metadata being edited.
    for (const auto &entry : m_entries) unpack(entry);
    validate();
}
QByteArray Epub::unpack(const Entry &e) const {
    QByteArray result;
    if (e.method == 0) result = e.packed;
    else {
        result.resize(int(e.size) + 1);
        z_stream stream{};
        stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(e.packed.constData()));
        stream.avail_in = uInt(e.packed.size());
        stream.next_out = reinterpret_cast<Bytef *>(result.data());
        stream.avail_out = uInt(result.size());
        need(inflateInit2(&stream, -MAX_WBITS) == Z_OK, "inflate setup failed");
        const int status = inflate(&stream, Z_FINISH);
        const bool good = status == Z_STREAM_END && stream.total_out == e.size && stream.avail_in == 0;
        inflateEnd(&stream);
        need(good, "invalid ZIP compressed data");
        result.resize(int(e.size));
    }
    need(result.size() == int(e.size) && crc32(0, reinterpret_cast<const Bytef *>(result.constData()), uInt(result.size())) == e.crc,
        "EPUB checksum mismatch");
    return result;
}
QByteArray Epub::read(const QByteArray &name) const {
    for (const auto &entry : m_entries) if (entry.name == name) return unpack(entry);
    throw std::runtime_error("missing EPUB resource");
}
QByteArray Epub::packagePath() const {
    auto doc = xml(read("META-INF/container.xml"));
    auto roots = doc.elementsByTagNameNS(QStringLiteral("urn:oasis:names:tc:opendocument:xmlns:container"), QStringLiteral("rootfile"));
    need(roots.size() == 1, "EPUB must have one package");
    auto path = roots.at(0).toElement().attribute(QStringLiteral("full-path")).toUtf8();
    need(safeName(path), "invalid EPUB package path");
    return path;
}
void Epub::validate() const {
    need(read("mimetype") == "application/epub+zip", "invalid EPUB mimetype");
    const auto path = packagePath();
    const auto base = path.left(path.lastIndexOf('/') + 1);
    auto doc = xml(read(path));
    need(doc.documentElement().localName() == "package" && doc.documentElement().namespaceURI() == opf, "invalid EPUB package");
    QMap<QString, QString> items;
    auto manifest = doc.elementsByTagNameNS(opf, QStringLiteral("item"));
    for (int i = 0; i < manifest.size(); ++i) {
        auto item = manifest.at(i).toElement();
        auto id = item.attribute(QStringLiteral("id"));
        need(!id.isEmpty() && !items.contains(id), "invalid manifest id");
        const auto href = item.attribute(QStringLiteral("href"));
        need(!href.isEmpty(), "empty manifest href");
        read(resolve(base, href));
        items.insert(id, item.attribute(QStringLiteral("media-type")));
    }
    auto spine = doc.elementsByTagNameNS(opf, QStringLiteral("itemref"));
    need(!spine.isEmpty(), "empty EPUB spine");
    for (int i = 0; i < spine.size(); ++i) {
        auto id = spine.at(i).toElement().attribute(QStringLiteral("idref"));
        need(items.value(id) == QStringLiteral("application/xhtml+xml"), "invalid EPUB spine resource");
    }
}
QString Epub::sourceBookId() const {
    auto doc = xml(read(packagePath()));
    auto ids = doc.elementsByTagNameNS(dc, QStringLiteral("identifier"));
    QString id;
    int count = 0, markers = 0;
    for (int i = 0; i < ids.size(); ++i) {
        auto text = ids.at(i).toElement().text().trimmed();
        if (text.startsWith(prefix)) { id = text.mid(prefix.size()); ++count; }
    }
    auto metas = doc.elementsByTagNameNS(opf, QStringLiteral("meta"));
    for (int i = 0; i < metas.size(); ++i) {
        auto e = metas.at(i).toElement();
        if (e.attribute(QStringLiteral("name")) == QStringLiteral("weread-downloader-plugin")
            && e.attribute(QStringLiteral("content")) == QStringLiteral("1")) ++markers;
    }
    if (!count && !markers) return {};
    need(count == 1 && markers == 1 && validBookId(id), "ambiguous EPUB source identity");
    return id;
}
bool Epub::hasCover() const {
    auto doc = xml(read(packagePath()));
    QString coverId;
    const auto metas = doc.elementsByTagNameNS(opf, QStringLiteral("meta"));
    for (int i = 0; i < metas.size(); ++i) {
        const auto e = metas.at(i).toElement();
        if (e.attribute("name") == "cover") coverId = e.attribute("content");
    }
    const auto items = doc.elementsByTagNameNS(opf, QStringLiteral("item"));
    for (int i = 0; i < items.size(); ++i) {
        const auto e = items.at(i).toElement();
        if (e.attribute("media-type").startsWith("image/") &&
            ((!coverId.isEmpty() && e.attribute("id") == coverId) || e.attribute("properties").split(' ').contains("cover-image"))) return true;
    }
    return false;
}
void Epub::stamp(const QString &bookId, const QString &outputPath, const QByteArray &cover) {
    need(validBookId(bookId), "invalid source book id");
    auto entries = m_entries;
    const auto old = sourceBookId();
    need(old.isEmpty() || old == bookId, "EPUB source identity conflict");
    const auto path = packagePath();
    auto doc = xml(read(path));
    auto list = doc.elementsByTagNameNS(opf, QStringLiteral("metadata"));
    need(list.size() == 1, "invalid EPUB metadata");
    auto metadata = list.at(0).toElement();
    if (old.isEmpty()) {
        auto id = doc.createElementNS(dc, QStringLiteral("dc:identifier"));
        id.appendChild(doc.createTextNode(prefix + bookId));
        metadata.appendChild(id);
        auto marker = doc.createElementNS(opf, QStringLiteral("meta"));
        marker.setAttribute(QStringLiteral("name"), QStringLiteral("weread-downloader-plugin"));
        marker.setAttribute(QStringLiteral("content"), QStringLiteral("1"));
        metadata.appendChild(marker);
    }
    if (!cover.isEmpty() && !hasCover()) {
        const bool png = cover.startsWith(QByteArray::fromHex("89504e470d0a1a0a"));
        need(cover.size() <= 4 * 1024 * 1024 && (png || cover.startsWith(QByteArray::fromHex("ffd8ff"))), "invalid cover image");
        auto manifests = doc.elementsByTagNameNS(opf, "manifest");
        auto spines = doc.elementsByTagNameNS(opf, "spine");
        need(manifests.size() == 1 && spines.size() == 1, "invalid cover package");
        auto manifest = manifests.at(0).toElement(); auto spine = spines.at(0).toElement();
        const auto base = path.left(path.lastIndexOf('/') + 1);
        QSet<QString> ids; QSet<QByteArray> names;
        const auto items = doc.elementsByTagNameNS(opf, "item");
        for (int i = 0; i < items.size(); ++i) ids.insert(items.at(i).toElement().attribute("id"));
        for (const auto &entry : entries) names.insert(entry.name);
        QString stem, imageHref, pageHref;
        for (int n = 1; ; ++n) {
            stem = "wrdp-cover-" + QString::number(n);
            imageHref = "images/" + stem + (png ? ".png" : ".jpg"); pageHref = "text/" + stem + ".xhtml";
            if (!ids.contains(stem) && !ids.contains(stem + "-image") && !names.contains(base + imageHref.toUtf8()) && !names.contains(base + pageHref.toUtf8())) break;
        }
        auto append = [&](const QByteArray &name, const QByteArray &bytes) {
            entries.append({name, bytes, 0, quint32(crc32(0, reinterpret_cast<const Bytef *>(bytes.constData()), uInt(bytes.size()))), quint32(bytes.size())});
        };
        append(base + imageHref.toUtf8(), cover);
        append(base + pageHref.toUtf8(), QByteArray("<?xml version=\"1.0\" encoding=\"UTF-8\"?><html xmlns=\"http://www.w3.org/1999/xhtml\"><head><title>Cover</title><style>html,body{margin:0;width:100%;height:100%;}img{display:block;width:100%;height:100%;object-fit:contain;}</style></head><body><img alt=\"Cover\" src=\"../") + imageHref.toUtf8() + "\"/></body></html>");
        auto addItem = [&](const QString &id, const QString &href, const QString &mime) {
            auto e = doc.createElementNS(opf, "item"); e.setAttribute("id", id); e.setAttribute("href", href); e.setAttribute("media-type", mime); manifest.appendChild(e); return e;
        };
        auto image = addItem(stem + "-image", imageHref, png ? "image/png" : "image/jpeg");
        if (doc.documentElement().attribute("version").startsWith('3')) image.setAttribute("properties", "cover-image");
        addItem(stem, pageHref, "application/xhtml+xml");
        auto reference = doc.createElementNS(opf, "itemref"); reference.setAttribute("idref", stem); spine.insertBefore(reference, spine.firstChild());
        auto meta = doc.createElementNS(opf, "meta"); meta.setAttribute("name", "cover"); meta.setAttribute("content", stem + "-image"); metadata.appendChild(meta);
        auto guides = doc.elementsByTagNameNS(opf, "guide");
        auto guide = guides.isEmpty() ? doc.createElementNS(opf, "guide") : guides.at(0).toElement();
        if (guides.isEmpty()) doc.documentElement().appendChild(guide);
        auto guideRef = doc.createElementNS(opf, "reference"); guideRef.setAttribute("type", "cover"); guideRef.setAttribute("title", "Cover"); guideRef.setAttribute("href", pageHref); guide.appendChild(guideRef);
    }
    const auto replacement = doc.toByteArray(-1);
    quint64 expanded = 0;
    for (const auto &entry : entries) expanded += entry.name == path ? quint64(replacement.size()) : entry.size;
    need(expanded <= limit && entries.size() < 10000, "EPUB expansion limit");
    QByteArray out, directory;
    // Preserve resource names and compressed bytes; only the OPF is rewritten.
    for (auto entry : entries) {
        if (entry.name == path) {
            entry.packed = replacement;
            entry.method = 0;
            entry.size = quint32(replacement.size());
            entry.crc = crc32(0, reinterpret_cast<const Bytef *>(replacement.constData()), uInt(replacement.size()));
        }
        const quint32 offset = quint32(out.size());
        put32(out, 0x04034b50); put16(out, 20); put16(out, 0x800); put16(out, entry.method);
        put16(out, 0); put16(out, 0x21); put32(out, entry.crc); put32(out, quint32(entry.packed.size()));
        put32(out, entry.size); put16(out, quint16(entry.name.size())); put16(out, 0);
        out += entry.name; out += entry.packed;
        put32(directory, 0x02014b50); put16(directory, 20); put16(directory, 20); put16(directory, 0x800);
        put16(directory, entry.method); put16(directory, 0); put16(directory, 0x21);
        put32(directory, entry.crc); put32(directory, quint32(entry.packed.size())); put32(directory, entry.size);
        put16(directory, quint16(entry.name.size())); put16(directory, 0); put16(directory, 0);
        put16(directory, 0); put16(directory, 0); put32(directory, 0); put32(directory, offset);
        directory += entry.name;
    }
    auto offset = quint32(out.size()); out += directory;
    put32(out, 0x06054b50); put16(out, 0); put16(out, 0);
    put16(out, quint16(entries.size())); put16(out, quint16(entries.size()));
    put32(out, quint32(directory.size())); put32(out, offset); put16(out, 0);
    need(out.size() <= int(limit) && !QFileInfo(outputPath).isSymLink(), "invalid EPUB output");
    QSaveFile file(outputPath); file.setDirectWriteFallback(false);
    need(file.open(QIODevice::WriteOnly) && file.setPermissions(QFile::ReadOwner | QFile::WriteOwner)
        && file.write(out) == out.size() && file.commit(), "cannot save EPUB");
    const Epub verified(outputPath);
    need(verified.sourceBookId() == bookId, "source identity readback failed");
    if (!cover.isEmpty()) need(verified.hasCover(), "cover readback failed");
}
}
