#pragma once
#include <QFile>
#include <QIODevice>
#include <QUuid>
#include <cstring>
namespace WereadDownloader {
// The stock server requires title-case MIME part headers. Qt 6 normalizes
// QHttpPart headers to lower case, so stream the exact framing ourselves.
class NativeUploadBody : public QIODevice {
public:
    NativeUploadBody(const QString &path, const QString &bookId) : m_file(path) {
        m_boundary = "wrdp-" + QUuid::createUuid().toString(QUuid::Id128).toLatin1();
        m_prefix = "--" + m_boundary + "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"weread-"
            + bookId.toLatin1() + ".epub\"\r\nContent-Type: application/epub+zip\r\n\r\n";
        m_suffix = "\r\n--" + m_boundary + "--\r\n";
        if (m_file.open(QIODevice::ReadOnly)) QIODevice::open(QIODevice::ReadOnly);
    }
    QByteArray contentType() const { return "multipart/form-data; boundary=" + m_boundary; }
    qint64 size() const override { return m_prefix.size() + m_file.size() + m_suffix.size(); }
    bool isSequential() const override { return false; }
    bool seek(qint64 offset) override { return offset >= 0 && offset <= size() && QIODevice::seek(offset); }
protected:
    qint64 readData(char *data, qint64 maximum) override {
        qint64 cursor = pos(), count = 0;
        while (count < maximum && cursor < size()) {
            qint64 length;
            if (cursor < m_prefix.size()) {
                length = qMin(maximum - count, qint64(m_prefix.size()) - cursor);
                std::memcpy(data + count, m_prefix.constData() + cursor, size_t(length));
            } else if (cursor < m_prefix.size() + m_file.size()) {
                const auto offset = cursor - m_prefix.size();
                if (!m_file.seek(offset)) return -1;
                length = m_file.read(data + count, qMin(maximum - count, m_file.size() - offset));
                if (length <= 0) return -1;
            } else {
                const auto offset = cursor - m_prefix.size() - m_file.size();
                length = qMin(maximum - count, qint64(m_suffix.size()) - offset);
                std::memcpy(data + count, m_suffix.constData() + offset, size_t(length));
            }
            count += length; cursor += length;
        }
        return count;
    }
    qint64 writeData(const char *, qint64) override { return -1; }
private:
    QFile m_file;
    QByteArray m_boundary, m_prefix, m_suffix;
};
}
