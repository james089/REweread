#include "import_registry.h"
#include "epub.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <stdexcept>
#include <sys/stat.h>

namespace WereadDownloader {
namespace {
void need(bool value, const char *reason) { if (!value) throw std::runtime_error(reason); }
bool uuid(const QString &s) {
    return QRegularExpression(QStringLiteral("\\A[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}\\z")).match(s).hasMatch();
}
QString now() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); }
}
ImportRegistry::ImportRegistry(const QString &root)
    : m_path(root + QStringLiteral("/imports.json")), m_lock(root + QStringLiteral("/imports.lock")) {
    need(!QFileInfo(root).isSymLink(), "private root is a symlink");
    need(QDir().mkpath(root), "cannot create private root");
    const auto path = QFile::encodeName(root);
    need(::chmod(path.constData(), 0700) == 0, "cannot protect private root");
    // QLockFile removes a demonstrably dead local owner, never a merely slow job.
    m_lock.setStaleLockTime(0);
    need(m_lock.tryLock(0), "another import worker is active");
    QFileInfo info(m_path);
    need(!info.isSymLink(), "registry is a symlink");
    if (!info.exists()) return;
    need(info.isFile() && info.size() < 4 * 1024 * 1024, "invalid registry file");
    QFile f(m_path); need(f.open(QIODevice::ReadOnly), "cannot read registry");
    QJsonParseError error;
    auto doc = QJsonDocument::fromJson(f.readAll(), &error);
    need(error.error == QJsonParseError::NoError && doc.isObject(), "corrupt registry");
    const auto object = doc.object();
    need(object.value("version").toInt() == 1 && object.value("books").isObject(), "unsupported registry");
    m_books = object.value("books").toObject();
    const QSet<QString> states{"preparing", "pending", "imported", "removed", "failed", "conflict"};
    for (auto it = m_books.begin(); it != m_books.end(); ++it) {
        auto row = it.value().toObject();
        need(Epub::validBookId(it.key()) && states.contains(row.value("state").toString()), "invalid registry record");
        if (row.value("state") == "imported") need(uuid(row.value("uuid").toString()), "invalid native document id");
    }
    // A crash before the persisted submission boundary is safe to retry.
    bool recovered = false;
    for (const auto &id : m_books.keys()) {
        auto row = m_books.value(id).toObject();
        if (row.value("state") == "preparing") {
            row["state"] = "failed"; row["updatedAt"] = now(); m_books[id] = row; recovered = true;
        }
    }
    if (recovered) save();
}
void ImportRegistry::save() {
    QSaveFile f(m_path); f.setDirectWriteFallback(false);
    auto bytes = QJsonDocument(QJsonObject{{"version", 1}, {"books", m_books}}).toJson(QJsonDocument::Compact);
    need(f.open(QIODevice::WriteOnly) && f.setPermissions(QFile::ReadOwner | QFile::WriteOwner)
        && f.write(bytes) == bytes.size() && f.commit(), "cannot persist import transaction");
}
QJsonObject ImportRegistry::record(const QString &id) const { return m_books.value(id).toObject(); }
QMap<QString, QString> ImportRegistry::knownDocuments() const {
    QMap<QString, QString> result;
    for (const auto &id : m_books.keys()) {
        const auto document = record(id).value("uuid").toString();
        if (uuid(document)) result.insert(document, id);
    }
    return result;
}
bool ImportRegistry::needsSourceScan() const {
    for (const auto &id : m_books.keys()) {
        const auto state = record(id).value("state").toString();
        if (state == "pending" || state == "conflict") return true;
    }
    return false;
}
void ImportRegistry::reserve(const QString &id, const QString &scope) {
    need(Epub::validBookId(id), "invalid book id");
    need(QRegularExpression(QStringLiteral("\\A[a-f0-9]{64}\\z")).match(scope).hasMatch(), "invalid account scope");
    auto row = record(id);
    auto state = row.value("state").toString();
    need(row.isEmpty() || state == "failed" || state == "removed", "book is already imported or awaiting reconciliation");
    m_books[id] = QJsonObject{{"state", "preparing"}, {"accountScope", scope}, {"updatedAt", now()}};
    save();
}
void ImportRegistry::prepareSubmission(const QString &id, const QString &path) {
    auto row = record(id);
    need(row.value("state") == "preparing", "invalid import transition");
    need(Epub(path).sourceBookId() == id, "EPUB lacks matching source identity");
    QFile f(path); need(f.open(QIODevice::ReadOnly), "cannot read submission file");
    QCryptographicHash hash(QCryptographicHash::Sha256);
    need(hash.addData(&f), "cannot hash submission");
    row["sha256"] = QString::fromLatin1(hash.result().toHex());
    // Persist BEFORE sending any request bytes. Failure/timeout after this point
    // remains pending until a source-ID scan proves what the system imported.
    row["state"] = "pending"; row["updatedAt"] = now();
    m_books[id] = row; save();
}
void ImportRegistry::failBeforeSubmission(const QString &id) {
    auto row = record(id);
    need(row.value("state") == "preparing", "cannot retry an uncertain submission");
    row["state"] = "failed"; row["updatedAt"] = now(); m_books[id] = row; save();
}
void ImportRegistry::rejectSubmission(const QString &id) {
    auto row = record(id);
    need(row.value("state") == "pending", "no pending submission to reject");
    row["state"] = "failed"; row["updatedAt"] = now(); m_books[id] = row; save();
}
void ImportRegistry::reconcile(const LibrarySnapshot &snapshot) {
    need(snapshot.complete, "native library scan is incomplete");
    QMap<QString, QVector<NativeDocument>> bySource;
    QSet<QString> seen;
    for (const auto &doc : snapshot.documents) {
        need(uuid(doc.uuid) && !seen.contains(doc.uuid), "invalid or duplicate native document id");
        seen.insert(doc.uuid);
        if (doc.sourceBookId.isEmpty()) continue; // Historical books are out of scope.
        need(Epub::validBookId(doc.sourceBookId), "invalid native source identity");
        bySource[doc.sourceBookId].append(doc);
    }
    // Recover records directly from stamped native EPUBs, including after loss
    // of the mapping file. Source identity is global; credentials remain scoped.
    QSet<QString> ids;
    for (const auto &key : m_books.keys()) ids.insert(key);
    for (auto it = bySource.begin(); it != bySource.end(); ++it) ids.insert(it.key());
    bool changed = false;
    for (const auto &id : ids) {
        auto row = record(id);
        const auto previous = row;
        QVector<NativeDocument> live;
        for (const auto &doc : bySource.value(id)) if (!doc.deleted) live.append(doc);
        if (live.size() > 1) { row["state"] = "conflict"; row.remove("uuid"); }
        else if (live.size() == 1) { row["state"] = "imported"; row["uuid"] = live.first().uuid; }
        else if (row.value("state") == "imported" || (!bySource.value(id).isEmpty() && row.isEmpty())) {
            row["state"] = "removed"; row.remove("uuid");
        }
        // No matching document is not evidence that an uncertain upload failed.
        if (row != previous) {
            row["updatedAt"] = now(); m_books[id] = row; changed = true;
        }
    }
    if (changed) save();
}
}
