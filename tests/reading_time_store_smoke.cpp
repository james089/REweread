#include "reading_time_store.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cstdlib>

namespace {
[[noreturn]] void fail(const char *message) {
    qCritical("%s", message);
    std::exit(1);
}

void expect(bool condition, const char *message) {
    if (!condition) {
        fail(message);
    }
}

void saveSession(const QString &path, const QString &vid) {
    QJsonObject cookies;
    cookies.insert(QStringLiteral("wr_vid"), vid);
    QJsonObject root;
    root.insert(QStringLiteral("cookies"), cookies);
    root.insert(QStringLiteral("logged_out"), false);
    QFile file(path);
    expect(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "cannot create test session");
    expect(file.write(QJsonDocument(root).toJson(QJsonDocument::Compact)) >= 0, "cannot write test session");
}

void saveInvalidUtf8Session(const QString &path, const QString &vid) {
    QByteArray session = QByteArrayLiteral("{\"cookies\":{\"wr_vid\":\"") + vid.toUtf8()
        + QByteArrayLiteral("\",\"opaque_cookie\":\"");
    session.append(static_cast<char>(0xff));
    session += QByteArrayLiteral("\"},\"logged_out\":false}");
    QFile file(path);
    expect(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "cannot create invalid UTF-8 test session");
    expect(file.write(session) == session.size(), "cannot write invalid UTF-8 test session");
}

void saveBrokenSession(const QString &path) {
    QFile file(path);
    expect(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "cannot create broken test session");
    expect(file.write(QByteArrayLiteral("{\"cookies\":")) > 0, "cannot write broken test session");
}

qint64 utcMilliseconds(int year, int month, int day, int hour, int minute, int second = 0) {
    return QDateTime(QDate(year, month, day), QTime(hour, minute, second), Qt::UTC).toMSecsSinceEpoch();
}

qint64 persistedStatisticMilliseconds(const QString &path, const QString &date) {
    QFile file(path);
    expect(file.open(QIODevice::ReadOnly), "cannot read persisted statistics");
    const QJsonObject days = QJsonDocument::fromJson(file.readAll()).object()
        .value(QStringLiteral("days")).toObject();
    return days.value(date).toObject().value(QStringLiteral("milliseconds")).toVariant().toLongLong();
}
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    expect(temporary.isValid(), "cannot create temporary directory");
    const QByteArray dataDir = temporary.path().toUtf8();
    const QByteArray sessionPath = temporary.filePath(QStringLiteral("session.json")).toUtf8();
    qputenv("RM_WEREAD_DATA_DIR", dataDir);
    qputenv("RM_WEREAD_SESSION_PATH", sessionPath);

    ReadingTimeStore noAccount;
    expect(!noAccount.accountReady(), "missing session must not create an account ledger");
    noAccount.snapshotPending(QStringLiteral("book"), 2000);
    expect(noAccount.pendingMilliseconds(QStringLiteral("book")) == 0, "missing account must not write pending time");

    saveSession(QString::fromUtf8(sessionPath), QStringLiteral("test-account-a"));
    ReadingTimeStore store;
    expect(store.accountReady(), "session account must load");
    int accountAGeneration = store.accountGeneration();
    expect(accountAGeneration > 0, "loaded account must have a generation");

    // An unrelated opaque cookie byte must not make an otherwise valid ASCII
    // numeric account identity disappear. The source session is not rewritten.
    saveInvalidUtf8Session(QString::fromUtf8(sessionPath), QStringLiteral("10001"));
    store.reloadAccount();
    expect(store.accountReady(), "invalid UTF-8 in an unrelated cookie must use the in-memory fallback");
    expect(store.pendingMilliseconds(QStringLiteral("book")) == 0,
           "the fallback account must not inherit the previous account ledger");
    const int invalidUtf8Generation = store.accountGeneration();
    saveInvalidUtf8Session(QString::fromUtf8(sessionPath), QStringLiteral("not-a-number"));
    store.reloadAccount();
    expect(!store.accountReady(), "fallback must reject a non-decimal account identity");
    saveBrokenSession(QString::fromUtf8(sessionPath));
    store.reloadAccount();
    expect(!store.accountReady(), "syntax errors must not use the UTF-8 fallback");
    saveSession(QString::fromUtf8(sessionPath), QStringLiteral("test-account-a"));
    store.reloadAccount();
    expect(store.accountReady() && store.accountGeneration() != invalidUtf8Generation,
           "strict parsing must restore the original account without fallback state");
    accountAGeneration = store.accountGeneration();

    // The statistics are separate from pending sync time. This two-second
    // interval straddles fixed UTC+8 midnight (16:00 UTC) exactly.
    const qint64 cstMidnight = utcMilliseconds(2026, 2, 1, 16, 0);
    expect(store.recordObservedInterval(QStringLiteral("book"), 2000, cstMidnight + 1000, accountAGeneration),
           "observed interval must accept the active account generation");
    expect(store.statistics().value(QStringLiteral("totalMs")).toLongLong() == 2000,
           "statistics must retain observed time independently");
    const qint64 monthBoundary = utcMilliseconds(2026, 2, 28, 16, 0);
    expect(store.recordObservedInterval(QStringLiteral("second-book"), 2000, monthBoundary + 1000, accountAGeneration),
           "month-boundary interval must be accepted");
    expect(!store.recordObservedInterval(QStringLiteral("book"), 0, cstMidnight, accountAGeneration),
           "zero observations must be rejected");
    expect(!store.recordObservedInterval(QStringLiteral("book"), -1, cstMidnight, accountAGeneration),
           "negative observations must be rejected");
    expect(!store.recordObservedInterval(QStringLiteral("book"), 5001, cstMidnight, accountAGeneration),
           "observations beyond the clock guard must be rejected");
    expect(!store.recordObservedInterval(QStringLiteral("book"), 1000, cstMidnight, accountAGeneration - 1),
           "a stale account generation must be rejected");
    store.snapshotPending(QStringLiteral("book"), 3500);
    expect(store.pendingMilliseconds(QStringLiteral("book")) == 3500, "snapshot must retain milliseconds");
    const qint64 dayMs = 24LL * 60 * 60 * 1000;
    store.snapshotPending(QStringLiteral("offline-backlog"), 25LL * dayMs);
    expect(store.pendingMilliseconds(QStringLiteral("offline-backlog")) == 25LL * dayMs,
           "offline time beyond one day must remain pending");
    store.snapshotPending(QStringLiteral("offline-backlog"), 400LL * dayMs);
    expect(store.pendingMilliseconds(QStringLiteral("offline-backlog")) == 365LL * dayMs,
           "excessive offline time must saturate instead of being cleared");
    const QDir ledgerRoot(temporary.filePath(QStringLiteral("reading-time")));
    const QFileInfoList scopes = ledgerRoot.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    expect(scopes.size() == 1, "account ledger must be created in one hashed directory");
    const QFileInfo ledger(scopes.first().filePath() + QStringLiteral("/ledger.json"));
    expect(ledger.exists(), "account ledger file must exist");
    const QFileDevice::Permissions permissions = ledger.permissions();
    expect(!(permissions & (QFileDevice::ReadGroup | QFileDevice::ReadOther
                            | QFileDevice::WriteGroup | QFileDevice::WriteOther)),
           "account ledger must not be group or world readable/writable");
    const QFileInfo statisticsFile(scopes.first().filePath() + QStringLiteral("/statistics.json"));
    expect(statisticsFile.exists(), "statistics file must persist with the ten-second ledger snapshot");
    expect(!(statisticsFile.permissions() & (QFileDevice::ReadGroup | QFileDevice::ReadOther
                                              | QFileDevice::WriteGroup | QFileDevice::WriteOther)),
           "statistics file must not be group or world readable/writable");
    expect(persistedStatisticMilliseconds(statisticsFile.filePath(), QStringLiteral("2026-02-01")) == 1000,
           "UTC+8 split must retain the first day segment");
    expect(persistedStatisticMilliseconds(statisticsFile.filePath(), QStringLiteral("2026-02-02")) == 1000,
           "UTC+8 split must retain the next day segment");
    expect(persistedStatisticMilliseconds(statisticsFile.filePath(), QStringLiteral("2026-02-28")) == 1000,
           "UTC+8 split must retain the final day of a month");
    expect(persistedStatisticMilliseconds(statisticsFile.filePath(), QStringLiteral("2026-03-01")) == 1000,
           "UTC+8 split must start a new month at midnight");
    const QString token = store.beginSubmission(QStringLiteral("book"), 2);
    expect(!token.isEmpty(), "whole pending seconds must reserve a submission");
    expect(store.pendingMilliseconds(QStringLiteral("book")) == 1500, "reservation must deduct only submitted seconds");
    expect(store.uncertainMilliseconds(QStringLiteral("book")) == 2000, "reservation must persist uncertainty first");

    saveSession(QString::fromUtf8(sessionPath), QStringLiteral("test-account-b"));
    // Simulate a stale QML clock snapshot racing with a login switch. The
    // store itself must reload the new scope but reject the old milliseconds.
    store.snapshotPending(QStringLiteral("book"), 9999);
    expect(store.pendingMilliseconds(QStringLiteral("book")) == 0, "account switch must not load prior pending time");
    expect(store.accountGeneration() != accountAGeneration, "account switch must advance the generation");
    expect(store.statistics().value(QStringLiteral("totalMs")).toLongLong() == 0,
           "statistics must not cross account scopes");
    store.completeSubmission(token, true);

    saveSession(QString::fromUtf8(sessionPath), QStringLiteral("test-account-a"));
    store.reloadAccount();
    expect(store.pendingMilliseconds(QStringLiteral("book")) == 1500, "old account pending time must survive another account session");
    expect(store.uncertainMilliseconds(QStringLiteral("book")) == 0, "late success must clear only its original scope");
    expect(store.statistics().value(QStringLiteral("totalMs")).toLongLong() == 4000,
           "late submission acknowledgements must not clear local statistics");

    saveSession(QString::fromUtf8(sessionPath), QStringLiteral("test-account-b"));
    expect(store.beginSubmission(QStringLiteral("book"), 1).isEmpty(), "stale submission after a session change must be rejected");
    expect(store.pendingMilliseconds(QStringLiteral("book")) == 0, "stale submission must not transfer old pending time to the new account");

    saveSession(QString::fromUtf8(sessionPath), QStringLiteral("test-account-a"));
    store.reloadAccount();

    const QString lateSuccessToken = store.beginSubmission(QStringLiteral("book"), 1);
    expect(!lateSuccessToken.isEmpty(), "remaining duration must reserve a late-success request");
    saveSession(QString::fromUtf8(sessionPath), QStringLiteral("test-account-b"));
    // Do not reload the store before completing: this catches a result that
    // arrives after the session changed on disk but before QML was notified.
    store.completeSubmission(lateSuccessToken, true);
    saveSession(QString::fromUtf8(sessionPath), QStringLiteral("test-account-a"));
    store.reloadAccount();
    expect(store.uncertainMilliseconds(QStringLiteral("book")) == 0,
           "late success must clear uncertainty in its original ledger");

    store.snapshotPending(QStringLiteral("book"), 1500);
    const QString failedToken = store.beginSubmission(QStringLiteral("book"), 1);
    expect(!failedToken.isEmpty(), "remaining duration must reserve a retry request");
    int changedAfterFailure = 0;
    QObject::connect(&store, &ReadingTimeStore::changed, [&changedAfterFailure]() {
        ++changedAfterFailure;
    });
    store.completeSubmission(failedToken, false);
    expect(store.uncertainMilliseconds(QStringLiteral("book")) == 1000, "failed result must remain uncertain instead of replaying");
    expect(store.hasUncertainSubmission(), "uncertain state must be exposed");
    expect(changedAfterFailure == 1, "failed result must immediately notify the UI about uncertainty");

    // A fresh instance must restore only explicitly recorded statistics; a
    // legacy pending ledger cannot be reinterpreted as historical reading.
    ReadingTimeStore restarted;
    expect(restarted.statistics().value(QStringLiteral("totalMs")).toLongLong() == 4000,
           "statistics must survive process restart");
    qputenv("RM_WEREAD_QT_SELFTEST", "1");
    expect(!restarted.recordObservedInterval(QStringLiteral("book"), 1000, cstMidnight, restarted.accountGeneration()),
           "self-test mode must not add reading statistics");
    qunsetenv("RM_WEREAD_QT_SELFTEST");

    // Version-one sync ledgers had only pending/uncertain values. They must
    // remain readable but cannot become invented reading history.
    const QString legacyVid = QStringLiteral("test-account-legacy");
    const QString legacyScope = QString::fromLatin1(
        QCryptographicHash::hash(legacyVid.toUtf8(), QCryptographicHash::Sha256).toHex());
    const QString legacyDir = temporary.filePath(QStringLiteral("reading-time/%1").arg(legacyScope));
    expect(QDir().mkpath(legacyDir), "cannot create legacy ledger directory");
    QJsonObject legacyBook;
    legacyBook.insert(QStringLiteral("pendingMs"), 7000);
    QJsonObject legacyBooks;
    legacyBooks.insert(QStringLiteral("legacy-book"), legacyBook);
    QJsonObject legacyLedger;
    legacyLedger.insert(QStringLiteral("version"), 1);
    legacyLedger.insert(QStringLiteral("books"), legacyBooks);
    QFile legacyFile(QDir(legacyDir).filePath(QStringLiteral("ledger.json")));
    expect(legacyFile.open(QIODevice::WriteOnly), "cannot create legacy ledger");
    expect(legacyFile.write(QJsonDocument(legacyLedger).toJson(QJsonDocument::Compact)) >= 0,
           "cannot write legacy ledger");
    legacyFile.close();
    saveSession(QString::fromUtf8(sessionPath), legacyVid);
    restarted.reloadAccount();
    expect(restarted.pendingMilliseconds(QStringLiteral("legacy-book")) == 7000,
           "version-one pending duration must remain compatible");
    expect(restarted.statistics().value(QStringLiteral("totalMs")).toLongLong() == 0,
           "legacy pending duration must not be converted into reading history");
    return 0;
}
