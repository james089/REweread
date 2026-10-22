#pragma once
#include <QObject>
#include <QJsonObject>
#include <QPointer>
#include <QProcess>
#include <QQuickImageProvider>
#include <QQuickItem>
#include <QVariantList>
#include <QStringList>
#include <QMap>
#include <QSet>
#include "cover_store.h"

class LoginImage : public QQuickImageProvider {
public:
    LoginImage() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString &, QSize *size, const QSize &) override;
    void update(const QString &url);
private:
    QImage m_image;
};

class DownloaderController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool opened READ opened NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool canQueue READ canQueue NOTIFY changed)
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString accountName READ accountName NOTIFY changed)
    Q_PROPERTY(QString qrImage READ qrImage NOTIFY changed)
    Q_PROPERTY(QVariantList books READ books NOTIFY booksChanged)
    Q_PROPERTY(int coverRevision READ coverRevision NOTIFY coversChanged)
public:
    explicit DownloaderController(LoginImage *image, QObject *parent = nullptr,
        const QString &workerProgram = QStringLiteral("/home/root/weread-downloader/weread-downloader-worker"),
        const QString &coverRoot = QStringLiteral("/home/root/.local/share/weread-downloader"));
    ~DownloaderController() override;
    bool opened() const { return m_opened; }
    bool busy() const { return m_process.state() != QProcess::NotRunning; }
    bool canQueue() const { return m_loggedIn && (!busy() || (m_operation == "import" && !m_error)); }
    bool loggedIn() const { return m_loggedIn; }
    QString status() const { return m_status; }
    QString accountName() const { return m_accountName; }
    QString qrImage() const { return m_qrImage; }
    QVariantList books() const { return m_books; }
    int coverRevision() const { return m_coverRevision; }
    Q_INVOKABLE QString coverSource(const QString &id, const QString &url);
    Q_INVOKABLE void openPage();
    Q_INVOKABLE void closePage();
    Q_INVOKABLE void login();
    Q_INVOKABLE void logout();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void importBooks(const QVariantList &ids);
    Q_INVOKABLE void cancelQueued(const QString &id);
    Q_INVOKABLE void clearCache(const QString &id);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void uploadProgress(const QString &id);
    Q_INVOKABLE void downloadProgress(const QString &id);
    Q_INVOKABLE void completeRestore(const QString &id, bool ok);
    Q_INVOKABLE void attachSettings(QObject *settings);
    Q_INVOKABLE void placeBelowAppLoad(QQuickItem *item);
signals:
    void changed();
    void coversChanged();
    void booksChanged();
    void bookUpdated(const QString &id, const QVariantMap &book);
    void restoreRequested(const QString &uuid, int page, const QString &id);
    void recentReadRequested(const QString &uuid);
private:
    void start(const QString &operation, const QString &id = {});
    void readOutput();
    void startProgress();
    void stopProgress();
    void readProgress();
    void applyProgress(const QJsonObject &row);
    void finish(int code, QProcess::ExitStatus status);
    void next();
    void bookState(const QString &state, int progress = -1);
    void updateBook(const QString &id, const QString &state, int progress = -1);
    void clearQueue();
    QString currentTitle() const;
    bool enableImporter();
    bool restoreImporter();
    CoverStore m_covers;
    int m_coverRevision = 0;
    LoginImage *m_image;
    QProcess m_process, m_progressProcess;
    QByteArray m_progressBuffer;
    QString m_restoreId;
    QSet<QString> m_autoRestoreTried;
    QPointer<QObject> m_settings;
    QByteArray m_buffer;
    QString m_operation, m_currentId, m_status, m_qrImage, m_scope, m_accountName;
    QStringList m_queue;
    QMap<QString, QString> m_queuedStates;
    QVariantList m_books;
    bool m_opened = false, m_loggedIn = false, m_error = false;
    bool m_refreshNeeded = false;
    bool m_shelfAfterAction = false;
    bool m_final = false, m_loginRequired = false, m_ownsImporter = false;
    quint64 m_qrRevision = 0;
    quint64 m_generation = 0;
    const QString m_workerProgram;
};
