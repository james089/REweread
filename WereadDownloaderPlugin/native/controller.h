#pragma once
#include <QObject>
#include <QDate>
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
#include "reading_time_store.h"
#include <QTimer>
#include <QElapsedTimer>
#include <QFileSystemWatcher>
#include <QTimeZone>

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
    Q_PROPERTY(QString readingSessionStatus READ readingSessionStatus NOTIFY readingSessionChanged)
    Q_PROPERTY(bool nativePullAvailable READ nativePullAvailable NOTIFY readingSessionChanged)
    Q_PROPERTY(bool nativePullVisible READ nativePullVisible NOTIFY nativePullChanged)
    Q_PROPERTY(bool nativePullBusy READ nativePullBusy NOTIFY nativePullChanged)
    Q_PROPERTY(bool nativePullApplying READ nativePullApplying NOTIFY nativePullChanged)
    Q_PROPERTY(bool nativePullCanConfirm READ nativePullCanConfirm NOTIFY nativePullChanged)
    Q_PROPERTY(QString nativePullMessage READ nativePullMessage NOTIFY nativePullChanged)
    Q_PROPERTY(bool opened READ opened NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool timeUploadActive READ timeUploadActive NOTIFY changed)
    Q_PROPERTY(int timeUploadDone READ timeUploadDone NOTIFY changed)
    Q_PROPERTY(int timeUploadTotal READ timeUploadTotal NOTIFY changed)
    Q_PROPERTY(int nativeBookRevision READ nativeBookRevision NOTIFY nativeBooksChanged)
    Q_PROPERTY(bool canQueue READ canQueue NOTIFY changed)
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString accountName READ accountName NOTIFY changed)
    Q_PROPERTY(QString qrImage READ qrImage NOTIFY changed)
    Q_PROPERTY(QVariantList books READ books NOTIFY booksChanged)
    Q_PROPERTY(int coverRevision READ coverRevision NOTIFY coversChanged)
    Q_PROPERTY(bool statisticsConfigured READ statisticsConfigured NOTIFY readingStatsChanged)
    Q_PROPERTY(QString statisticsStatus READ statisticsStatus NOTIFY readingStatsChanged)
    Q_PROPERTY(int cloudReadingTotalSeconds READ cloudReadingTotalSeconds NOTIFY readingStatsChanged)
    Q_PROPERTY(QVariantList readingStats READ readingStats NOTIFY readingStatsChanged)
    Q_PROPERTY(QStringList readingDates READ readingDates NOTIFY readingStatsChanged)
    Q_PROPERTY(QString readingDate READ readingDate WRITE setReadingDate NOTIFY readingStatsChanged)
    Q_PROPERTY(QString readingTimeZone READ readingTimeZone WRITE setReadingTimeZone NOTIFY readingTimeZoneChanged)
    Q_PROPERTY(QStringList readingTimeZones READ readingTimeZones CONSTANT)
    Q_PROPERTY(QStringList readingTimeZoneLabels READ readingTimeZoneLabels CONSTANT)
    Q_PROPERTY(QString readingClock READ readingClock NOTIFY readingClockChanged)
    Q_PROPERTY(QVariantList readingWeekStats READ readingWeekStats NOTIFY readingStatsChanged)
    Q_PROPERTY(QString readingWeekLabel READ readingWeekLabel NOTIFY readingStatsChanged)
    Q_PROPERTY(int readingWeekTotalSeconds READ readingWeekTotalSeconds NOTIFY readingStatsChanged)
    Q_PROPERTY(int readingWeekMaximumSeconds READ readingWeekMaximumSeconds NOTIFY readingStatsChanged)
    Q_PROPERTY(QString readingWeekPeakLabel READ readingWeekPeakLabel NOTIFY readingStatsChanged)
    Q_PROPERTY(int readingDateTotalSeconds READ readingDateTotalSeconds NOTIFY readingStatsChanged)
public:
    explicit DownloaderController(LoginImage *image, QObject *parent = nullptr,
        const QString &workerProgram = QStringLiteral("/home/root/weread-downloader/weread-downloader-worker"),
        const QString &coverRoot = QStringLiteral("/home/root/.local/share/weread-downloader"));
    ~DownloaderController() override;
    QString readingSessionStatus() const;
    bool nativePullAvailable() const { return m_readingTracked && !m_readingUuid.isEmpty(); }
    bool nativePullVisible() const { return m_nativePullVisible; }
    bool nativePullBusy() const { return m_nativePullBusy; }
    bool nativePullApplying() const { return m_nativePullApplying; }
    bool nativePullCanConfirm() const { return m_nativePullReady && !m_nativePullBusy; }
    QString nativePullMessage() const { return m_nativePullMessage; }
    Q_INVOKABLE void requestNativePull();
    Q_INVOKABLE void confirmNativePull();
    Q_INVOKABLE void cancelNativePull();
    Q_INVOKABLE void completeNativePullRestore(const QString &id, bool ok);
    bool opened() const { return m_opened; }
    bool busy() const { return m_process.state() != QProcess::NotRunning || m_autoSyncProcess.state() != QProcess::NotRunning || m_entryProcess.state() != QProcess::NotRunning || m_nativePullBusy; }
    int nativeBookRevision() const { return m_nativeBookRevision; }
    Q_INVOKABLE QString nativeBookBadge(const QString &uuid) const;
    bool canQueue() const { return m_loggedIn && (!busy() || (m_process.state()!=QProcess::NotRunning && m_operation == "import" && !m_error)); }
    bool loggedIn() const { return m_loggedIn; }
    QString status() const { return m_autoSyncProcess.state()!=QProcess::NotRunning ? QStringLiteral("正在后台上传微信读书进度…") : m_status; }
    bool timeUploadActive() const { return m_operation=="upload-time" && m_process.state()!=QProcess::NotRunning; }
    int timeUploadDone() const { return m_timeUploadDone; }
    int timeUploadTotal() const { return m_timeUploadTotal; }
    QString accountName() const { return m_accountName; }
    QString qrImage() const { return m_qrImage; }
    QVariantList books() const { return m_books; }
    int coverRevision() const { return m_coverRevision; }
    bool statisticsConfigured() const;
    QString statisticsStatus() const { return m_statisticsStatus; }
    int cloudReadingTotalSeconds() const;
    Q_INVOKABLE QString statisticsApiKeyForEditing() const;
    Q_INVOKABLE void setStatisticsApiKey(const QString &key);
    Q_INVOKABLE void refreshCloudStatistics();
    QVariantList readingStats() const;
    QStringList readingDates() const;
    QString readingDate() const { return m_readingDate; }
    void setReadingDate(const QString &date);
    QString readingTimeZone() const { return m_readingTimeZoneId; }
    QStringList readingTimeZones() const { return m_readingTimeZones; }
    QStringList readingTimeZoneLabels() const { return m_readingTimeZoneLabels; }
    QString readingClock() const;
    void setReadingTimeZone(const QString &zoneId);
    QVariantList readingWeekStats() const;
    QString readingWeekLabel() const;
    int readingWeekTotalSeconds() const;
    int readingWeekMaximumSeconds() const;
    QString readingWeekPeakLabel() const;
    int readingDateTotalSeconds() const;
    Q_INVOKABLE void shiftReadingWeek(int weeks);
    Q_INVOKABLE void shiftReadingDate(int days);
    Q_INVOKABLE QString coverSource(const QString &id, const QString &url);
    Q_INVOKABLE void openPage();
    Q_INVOKABLE void closePage();
    Q_INVOKABLE void login();
    Q_INVOKABLE void logout();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void importBooks(const QVariantList &ids);
    Q_INVOKABLE void clearCache(const QString &id);
    Q_INVOKABLE void cancelQueued(const QString &id);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void uploadProgress(const QString &id);
    Q_INVOKABLE void downloadProgress(const QString &id);
    Q_INVOKABLE void completeRestore(const QString &id, bool ok);
    Q_INVOKABLE void attachSettings(QObject *settings);
    Q_INVOKABLE void placeBelowAppLoad(QQuickItem *item);
    Q_INVOKABLE void uploadReadingTime(const QString &id);
    Q_INVOKABLE void readingStarted(const QString &uuid, int page = -1);
    Q_INVOKABLE void readingStopped(int expectedPage = -1);
    Q_INVOKABLE void readingActivity();
    Q_INVOKABLE void setReadingSuspended(bool suspended);
    Q_INVOKABLE void setReadingDeviceSleeping(bool sleeping);
    Q_INVOKABLE void setReadingWifiConnected(bool connected);
    Q_INVOKABLE void setReadingApplicationActive(bool active);
    Q_INVOKABLE void setReadingLocked(bool locked);
signals:
    void nativePullChanged();
    void nativePullRestoreRequested(const QString &uuid, int page, const QString &bookId);
    void readingSessionChanged();
    void changed();
    void coversChanged();
    void booksChanged();
    void bookUpdated(const QString &id, const QVariantMap &book);
    void restoreRequested(const QString &uuid, int page, const QString &id);
    void recentReadRequested(const QString &uuid);
    void readingStatsChanged();
    void readingTimeZoneChanged();
    void readingClockChanged();
    void nativeBooksChanged();
private:
    void startNativePullWorker(bool apply);
    void readNativePull();
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
    void checkpointReadingTime();
    bool saveReadingTimeZone() const;
    void updateReadingTimer();
    void updateReadingEntry();
    void tryReadingEntry();
    void finishReadingEntry(int code, QProcess::ExitStatus status);
    void reloadNativeBooks();
    void processExitQueue();
    void finishExitSync(int code, QProcess::ExitStatus status);
    void readExitSync();
    void stopExitSync();
    void notifyNativeBooks();
    QString currentTitle() const;
    bool enableImporter();
    bool restoreImporter();
    CoverStore m_covers;
    int m_coverRevision = 0;
    LoginImage *m_image;
    QProcess m_process, m_progressProcess, m_autoSyncProcess, m_entryProcess, m_statsProcess;
    QFileSystemWatcher m_nativeBookWatcher;
    QMap<QString,QJsonObject> m_nativeBooks;
    QMap<QString,QJsonObject> m_exitJobs;
    QMap<QString,qint64> m_exitNotBefore;
    QElapsedTimer m_exitClock;
    QElapsedTimer m_exitWorkerClock;
    QStringList m_exitQueue;
    QSet<QString> m_exitErrors;
    QString m_readingUuid,m_exitUuid,m_readingOpeningSignature,m_readingSession;
    QString m_entryAttemptSession,m_entryBookId;
    QTimer m_entryRetryTimer,m_entryTimeoutTimer,m_exitTimeoutTimer;
    int m_entryAttempts=0;
    bool m_entryNeeded=false,m_entryTerminal=false,m_entryTimedOut=false;
    QString m_readingTimeZoneId=QStringLiteral("UTC");
    QStringList m_readingTimeZones;
    QStringList m_readingTimeZoneLabels;
    QDate m_readingWeekStart;
    QJsonObject m_exitJob;
    QJsonObject m_exitTimings;
    QByteArray m_exitBuffer;
    bool m_exitSuccess=false,m_exitCanceled=false;
    int m_nativeBookRevision=0;
    quint64 m_exitGeneration=0;
    QByteArray m_progressBuffer;
    QString m_restoreId;
    QSet<QString> m_autoRestoreTried;
    QPointer<QObject> m_settings;
    QByteArray m_buffer;
    QString m_operation, m_currentId, m_status, m_qrImage, m_scope, m_accountName, m_syncNotice;
    QStringList m_queue;
    QMap<QString, QString> m_queuedStates;
    QVariantList m_books;
    bool m_opened = false, m_loggedIn = false, m_error = false;
    bool m_refreshNeeded = false;
    bool m_shelfAfterAction = false;
    bool m_final = false, m_loginRequired = false, m_ownsImporter = false;
    quint64 m_qrRevision = 0;
    quint64 m_generation = 0,m_statisticsGeneration = 0;
    const QString m_workerProgram;
    QString m_readingDate,m_statisticsStatus;
    const QString m_dataRoot;
    WereadDownloader::ReadingTimeStore m_readingTime;
    QTimer m_readingTimer;
    QTimer m_readingClockTimer;
    QElapsedTimer m_readingClock;
    bool m_readingWifiConnected=false,m_readingTracked=false;
    qint64 m_readingStartSeconds=0;
    int m_reportCount=0;
    qint64 m_reportSeconds=0;
    int m_timeUploadDone=0,m_timeUploadTotal=0;
    QProcess m_nativePullProcess;
    QByteArray m_nativePullBuffer;
    QString m_nativePullUuid,m_nativePullBook,m_nativePullScope,m_nativePullToken,m_nativePullMessage;
    bool m_nativePullVisible=false,m_nativePullBusy=false,m_nativePullReady=false,m_nativePullApplying=false,m_nativePullSuccess=false;
    quint64 m_nativePullGeneration=0;
    bool m_reportFailed=false;
    bool m_deviceSleeping=false,m_readingSuspended=true,m_readingAppActive=true,m_readingLocked=false;
};
