#pragma once
#include <QJsonObject>
#include <QMap>
#include <QLockFile>
#include <QString>
#include <QVector>

namespace WereadDownloader {
struct NativeDocument {
    QString uuid;
    QString sourceBookId;
    bool deleted = false;
};
struct LibrarySnapshot {
    QVector<NativeDocument> documents;
    // Only an entire successful scan can prove that a document was removed.
    bool complete = false;
};

// Hold this instance throughout download/submission/reconciliation. A second
// worker cannot submit the same book, even from another account or UI process.
class ImportRegistry {
public:
    explicit ImportRegistry(const QString &privateRoot);
    QJsonObject record(const QString &bookId) const;
    QMap<QString, QString> knownDocuments() const;
    bool needsSourceScan() const;
    void reserve(const QString &bookId, const QString &accountScope);
    void prepareSubmission(const QString &bookId, const QString &epubPath);
    void failBeforeSubmission(const QString &bookId);
    void rejectSubmission(const QString &bookId);
    void reconcile(const LibrarySnapshot &snapshot);
private:
    void save();
    QString m_path;
    QLockFile m_lock;
    QJsonObject m_books;
};
}
