#pragma once
#include <QByteArray>
#include <QString>
#include <QVector>

namespace WereadDownloader {
// Bounded, non-extracting ZIP reader/writer. No archive paths reach the filesystem.
class Epub {
public:
    explicit Epub(const QString &path);
    QString sourceBookId() const;
    bool hasCover() const;
    void stamp(const QString &bookId, const QString &outputPath, const QByteArray &cover = {});
    static bool validBookId(const QString &id);
private:
    struct Entry {
        QByteArray name, packed;
        quint16 method;
        quint32 crc, size;
    };
    QByteArray unpack(const Entry &entry) const;
    QByteArray read(const QByteArray &name) const;
    QByteArray packagePath() const;
    void validate() const;
    QVector<Entry> m_entries;
};
}
