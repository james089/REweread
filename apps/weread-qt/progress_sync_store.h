#pragma once

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <QVariantMap>

class ProgressSyncStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)

public:
    explicit ProgressSyncStore(QObject *parent = nullptr);

    bool running() const;
    QString statusText() const;

    // chapterUid/chapterIdx/chapterOffset may be supplied only when they
    // originate from a verified Web reader location. The four-argument form
    // remains a percentage-only compatibility upload; it does not claim a
    // precise chapter location.
    Q_INVOKABLE void syncProgress(const QString &bookId, double progress, const QString &summary, int elapsedSeconds = 0,
                                  const QString &chapterUid = QString(), int chapterIdx = -1, int chapterOffset = -1);
    Q_INVOKABLE void pullProgress(const QString &bookId);

signals:
    void changed();
    // Legacy percentage-only notification. Automatic opening may continue to
    // use its local-first policy with this signal.
    void progressPulled(const QString &bookId, double progress);
    // Emitted only after a successful helper exit and only when the Web reader
    // supplied a chapter UID plus a non-empty text summary. QML may offer this
    // as an explicit "use cloud position" action; it must not auto-overwrite a
    // saved local location.
    void progressPulledDetailed(const QString &bookId, const QVariantMap &remotePosition);
    void progressSynced(const QString &bookId, int elapsedSeconds);
    // Every helper request terminates with an explicit outcome. Callers use a
    // failed push to retain an already-reserved duration as uncertain rather
    // than waiting for a later unrelated sync attempt.
    void progressFinished(const QString &bookId, int elapsedSeconds, bool succeeded);

private:
    QString dataDir() const;
    void setState(bool running, const QString &statusText);
    void startHelper(const QString &helper, const QStringList &arguments, const QString &statusText, const QString &mode);
    void appendOutput(QByteArray &buffer, const QByteArray &bytes, bool parseProtocol);
    void flushOutput(QByteArray &buffer, bool parseProtocol);
    void processOutputLine(const QString &line);
    void finishProcess(int exitCode, QProcess::ExitStatus exitStatus);
    QString failureText(const QString &reason = QString()) const;
    void resetRequestState();

    QProcess m_process;
    QTimer m_processTimeoutTimer;
    bool m_running;
    QString m_statusText;
    QString m_mode;
    QString m_bookId;
    QByteArray m_stdoutBuffer;
    QByteArray m_stderrBuffer;
    bool m_helperDone = false;
    bool m_helperFailed = false;
    bool m_timedOut = false;
    bool m_processStartFailed = false;
    bool m_requestFinished = false;
    bool m_hasPulledProgress = false;
    double m_pulledProgress = 0.0;
    QVariantMap m_pulledPosition;
    int m_elapsedSeconds = 0;
};
