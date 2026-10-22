#pragma once

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QVariantList>

class DownloadStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString bookId READ bookId NOTIFY changed)
    Q_PROPERTY(QString progressText READ progressText NOTIFY changed)
    Q_PROPERTY(QString lastError READ lastError NOTIFY changed)
    Q_PROPERTY(QVariantList downloads READ downloads NOTIFY changed)
    Q_PROPERTY(int queuedCount READ queuedCount NOTIFY changed)
    Q_PROPERTY(QString cacheStatusText READ cacheStatusText NOTIFY changed)

public:
    explicit DownloadStore(QObject *parent = nullptr);

    bool running() const;
    QString state() const;
    QString bookId() const;
    Q_INVOKABLE bool hasCompleteDownload(const QString &bookId) const;
    QString progressText() const;
    QString lastError() const;
    QVariantList downloads() const;
    int queuedCount() const;
    QString cacheStatusText() const;

    Q_INVOKABLE void downloadBook(const QString &bookId, const QString &title);
    Q_INVOKABLE void downloadOpeningChapter(const QString &bookId, const QString &title);
    Q_INVOKABLE void downloadBooks(const QVariantList &books);
    Q_INVOKABLE void cancelDownload();
    Q_INVOKABLE void clearDownloadQueue();
    Q_INVOKABLE void deleteDownload(const QString &bookId, const QString &title);
    Q_INVOKABLE void clearReaderCache();
    Q_INVOKABLE void resumeDownloadQueue();
    Q_INVOKABLE void repairBookImages(const QString &bookId, const QString &title);

signals:
    void changed();
    void downloadRemoved(const QString &bookId);
    void epubReady(const QString &bookId, const QString &title);
    void openingChapterReady(const QString &bookId, const QString &title);

private:
    QString dataDir() const;
    QString safeName(const QString &value) const;
    QString bookDir(const QString &bookId) const;
    QString findFullEpub(const QString &bookId) const;
    QString findOpeningEpub(const QString &bookId) const;
    QString findReadableEpub(const QString &bookId) const;
    QString downloadsFilePath() const;
    void loadDownloads();
    void saveDownloads() const;
    void restoreQueuedDownloads();
    void recordDownload(const QString &state, const QString &progress, const QString &error);
    void recordDownloadRow(const QString &bookId, const QString &title, const QString &state, const QString &progress,
                           const QString &error, bool openingChapter = false);
    void enqueueDownload(const QString &bookId, const QString &title, bool openingChapter = false);
    void startDownloadProcess(const QString &bookId, const QString &title, bool force = false, bool openingChapter = false);
    void startNextQueuedDownload();
    void clearExpandedCache(const QString &bookId) const;
    bool hasHelperResult(const QString &state) const;
    void appendOutput(QByteArray &buffer, const QByteArray &bytes);
    void flushOutput(QByteArray &buffer);
    void processOutputLine(const QString &line);
    void finishProcess(int exitCode, QProcess::ExitStatus exitStatus);
    void setState(const QString &state, const QString &progress, const QString &error = QString());

    QProcess m_process;
    QString m_bookId;
    QString m_title;
    QString m_state;
    QString m_progressText;
    QString m_lastError;
    QString m_helperResultState;
    QString m_helperResultPath;
    QByteArray m_stdoutBuffer;
    QByteArray m_stderrBuffer;
    int m_completedChapters = 0;
    int m_totalChapters = 0;
    bool m_openingChapterMode = false;
    QVariantList m_downloads;
    QVariantList m_queue;
    QString m_cacheStatusText;
};
