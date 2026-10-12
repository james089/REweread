#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>

// Keeps unsent reading duration in an account-scoped, local-only ledger.
// Account identifiers are never retained: the on-disk directory name is a
// SHA-256 digest of the active session's wr_vid.
class ReadingTimeStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool accountReady READ accountReady NOTIFY changed)
    Q_PROPERTY(bool hasUncertainSubmission READ hasUncertainSubmission NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(QVariantMap statistics READ statistics NOTIFY changed)
    Q_PROPERTY(int accountGeneration READ accountGeneration NOTIFY accountReloaded)

public:
    explicit ReadingTimeStore(QObject *parent = nullptr);

    bool accountReady() const;
    bool hasUncertainSubmission() const;
    QString statusText() const;
    QVariantMap statistics() const;
    int accountGeneration() const;

    // Re-evaluates the session identity. A changed account clears the in-memory
    // ledger before loading only that account's hashed directory.
    Q_INVOKABLE void reloadAccount();
    Q_INVOKABLE QVariantMap pendingMillisecondsByBook() const;
    Q_INVOKABLE qint64 pendingMilliseconds(const QString &bookId) const;
    Q_INVOKABLE qint64 uncertainMilliseconds(const QString &bookId) const;

    // Records one already-observed foreground reading interval. This is
    // deliberately independent from pending/uncertain sync time: a successful
    // cloud submission never removes local reading statistics. generation must
    // be the accountGeneration captured by the caller before the interval.
    // Timestamps are UTC epoch milliseconds; day boundaries use UTC+8.
    Q_INVOKABLE bool recordObservedInterval(const QString &bookId, qint64 milliseconds,
                                            qint64 endTimestampMilliseconds, int generation);

    // The caller supplies its complete current-clock value for one book every
    // ten seconds and before sleep/exit. This is a snapshot, not an increment.
    Q_INVOKABLE void snapshotPending(const QString &bookId, qint64 milliseconds);

    // Atomically moves whole seconds from pending to uncertain and returns an
    // opaque token. Empty means no eligible account or no whole seconds.
    Q_INVOKABLE QString beginSubmission(const QString &bookId, int requestedSeconds);
    // A successful result clears the matching uncertain reservation. A failed
    // result deliberately leaves it uncertain; it is never automatically replayed.
    Q_INVOKABLE void completeSubmission(const QString &token, bool succeeded);

signals:
    void changed();
    void accountReloaded();
    void submissionReserved(const QString &token, const QString &bookId, int seconds);

private:
    struct Reservation {
        QString scope;
        QString bookId;
        qint64 milliseconds = 0;
    };

    struct DailyStatistics {
        qint64 milliseconds = 0;
        QSet<QString> bookKeys;
    };

    QString dataDir() const;
    QString sessionPath() const;
    QString resolvedScope() const;
    bool ensureCurrentAccount();
    bool scopeStillCurrent(const QString &scope) const;
    QString ledgerPath(const QString &scope) const;
    QString statisticsPath(const QString &scope) const;
    bool loadLedgerForScope(const QString &scope, QHash<QString, qint64> *pending,
                            QHash<QString, qint64> *uncertain) const;
    bool saveLedgerForScope(const QString &scope, const QHash<QString, qint64> &pending,
                            const QHash<QString, qint64> &uncertain) const;
    bool loadStatisticsForScope(const QString &scope,
                                QHash<QString, DailyStatistics> *statistics) const;
    bool saveStatisticsForScope(const QString &scope,
                                const QHash<QString, DailyStatistics> &statistics) const;
    void loadCurrentLedger();
    void clearCurrentLedger(const QString &statusText);
    bool saveCurrentLedger();
    bool saveCurrentStatistics();
    void updateStatus();
    static QVariantMap asVariantMap(const QHash<QString, qint64> &values);
    static qint64 positiveMilliseconds(qint64 milliseconds);
    static QString utcPlus8Day(qint64 timestampMilliseconds);
    static qint64 nextUtcPlus8Midnight(qint64 timestampMilliseconds);
    static QString bookStatisticsKey(const QString &bookId);

    QString m_scope;
    QHash<QString, qint64> m_pendingMs;
    QHash<QString, qint64> m_uncertainMs;
    QHash<QString, DailyStatistics> m_statistics;
    QHash<QString, Reservation> m_reservations;
    quint64 m_nextReservation = 1;
    int m_accountGeneration = 0;
    QString m_statusText;
};
