#include "reading_time_store.h"
#include "session_json_reader.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QSet>

#include <utility>

namespace {
// Keep a long offline backlog without risking the int-second bridge used by
// QML and the sync helper (365 days is only 31,536,000 seconds).
constexpr qint64 kMaxReasonableMilliseconds = 365LL * 24 * 60 * 60 * 1000;
// ReadingClock rejects samples longer than five seconds. Keep the same hard
// bound at this public boundary so a stale QML callback cannot turn a sleep
// period into observed reading time.
constexpr qint64 kMaxObservedIntervalMilliseconds = 5000;
const QFileDevice::Permissions kOwnerReadWrite = QFileDevice::ReadOwner | QFileDevice::WriteOwner;
}

ReadingTimeStore::ReadingTimeStore(QObject *parent)
    : QObject(parent),
      m_statusText(QStringLiteral("阅读时长账本未加载")) {
    reloadAccount();
}

bool ReadingTimeStore::accountReady() const {
    return !m_scope.isEmpty();
}

bool ReadingTimeStore::hasUncertainSubmission() const {
    for (auto it = m_uncertainMs.cbegin(); it != m_uncertainMs.cend(); ++it) {
        if (it.value() > 0) {
            return true;
        }
    }
    return false;
}

QString ReadingTimeStore::statusText() const {
    return m_statusText;
}

QVariantMap ReadingTimeStore::statistics() const {
    const QDate today = QDateTime::currentDateTimeUtc().addSecs(8 * 60 * 60).date();
    const QString todayKey = today.toString(Qt::ISODate);
    const QString monthPrefix = today.toString(QStringLiteral("yyyy-MM"));
    qint64 monthMs = 0;
    qint64 totalMs = 0;
    int monthDays = 0;
    QSet<QString> monthBooks;
    int totalDays = 0;
    QSet<QString> totalBooks;
    QString firstDate;

    for (auto it = m_statistics.cbegin(); it != m_statistics.cend(); ++it) {
        if (it.value().milliseconds <= 0) {
            continue;
        }
        totalMs += it.value().milliseconds;
        ++totalDays;
        totalBooks.unite(it.value().bookKeys);
        if (firstDate.isEmpty() || it.key() < firstDate) {
            firstDate = it.key();
        }
        if (it.key().startsWith(monthPrefix)) {
            monthMs += it.value().milliseconds;
            ++monthDays;
            monthBooks.unite(it.value().bookKeys);
        }
    }

    QVariantList days;
    for (int offset = 6; offset >= 0; --offset) {
        const QString key = today.addDays(-offset).toString(Qt::ISODate);
        QVariantMap day;
        day.insert(QStringLiteral("date"), key);
        day.insert(QStringLiteral("ms"), m_statistics.value(key).milliseconds);
        days.append(day);
    }

    QVariantMap result;
    result.insert(QStringLiteral("todayMs"), m_statistics.value(todayKey).milliseconds);
    result.insert(QStringLiteral("monthMs"), monthMs);
    result.insert(QStringLiteral("totalMs"), totalMs);
    result.insert(QStringLiteral("monthLabel"), today.toString(QStringLiteral("yyyy年MM月")));
    // The unqualified counts are deliberately for the visible month card.
    result.insert(QStringLiteral("activeDays"), monthDays);
    result.insert(QStringLiteral("booksRead"), monthBooks.size());
    result.insert(QStringLiteral("firstDate"), firstDate);
    result.insert(QStringLiteral("totalActiveDays"), totalDays);
    result.insert(QStringLiteral("totalBooksRead"), totalBooks.size());
    result.insert(QStringLiteral("days"), days);
    return result;
}

int ReadingTimeStore::accountGeneration() const {
    return m_accountGeneration;
}

QString ReadingTimeStore::dataDir() const {
    const QByteArray overrideDir = qgetenv("RM_WEREAD_DATA_DIR");
    return overrideDir.isEmpty()
        ? QStringLiteral("/home/root/.local/share/rm-weread")
        : QString::fromUtf8(overrideDir);
}

QString ReadingTimeStore::sessionPath() const {
    const QByteArray overridePath = qgetenv("RM_WEREAD_SESSION_PATH");
    return overridePath.isEmpty()
        ? QDir(dataDir()).filePath(QStringLiteral("session.json"))
        : QString::fromUtf8(overridePath);
}

QString ReadingTimeStore::resolvedScope() const {
    QFile session(sessionPath());
    if (!session.open(QIODevice::ReadOnly)) {
        return {};
    }
    const SessionJsonReader::Result parsed = SessionJsonReader::parse(session.readAll());
    const QJsonDocument document = parsed.document;
    if (!document.isObject()) {
        return {};
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("logged_out")).toBool(false)) {
        return {};
    }
    const QString vid = root.value(QStringLiteral("cookies")).toObject()
                            .value(QStringLiteral("wr_vid")).toString().trimmed();
    if (vid.isEmpty() || (parsed.usedLatin1Fallback
                          && !SessionJsonReader::isAsciiDecimalIdentifier(vid))) {
        return {};
    }
    return QString::fromLatin1(QCryptographicHash::hash(vid.toUtf8(), QCryptographicHash::Sha256).toHex());
}

bool ReadingTimeStore::ensureCurrentAccount() {
    const QString scope = resolvedScope();
    if (scope == m_scope) {
        if (scope.isEmpty() && m_statusText != QStringLiteral("未识别已登录账号，阅读时长仅临时保留")) {
            clearCurrentLedger(QStringLiteral("未识别已登录账号，阅读时长仅临时保留"));
            emit changed();
        }
        return !scope.isEmpty();
    }
    // Statistics are independent of the sync ledger and may have been
    // accumulated since the last ten-second snapshot. Save them directly to
    // their original hashed scope before dropping the in-memory account.
    if (!m_scope.isEmpty() && !m_statistics.isEmpty()) {
        saveStatisticsForScope(m_scope, m_statistics);
    }
    m_scope = scope;
    m_pendingMs.clear();
    m_uncertainMs.clear();
    m_statistics.clear();
    ++m_accountGeneration;
    if (m_scope.isEmpty()) {
        clearCurrentLedger(QStringLiteral("未识别已登录账号，阅读时长仅临时保留"));
    } else {
        loadCurrentLedger();
    }
    emit changed();
    emit accountReloaded();
    return !m_scope.isEmpty();
}

bool ReadingTimeStore::scopeStillCurrent(const QString &scope) const {
    return !scope.isEmpty() && scope == resolvedScope();
}

void ReadingTimeStore::reloadAccount() {
    ensureCurrentAccount();
}

QString ReadingTimeStore::ledgerPath(const QString &scope) const {
    return QDir(dataDir()).filePath(QStringLiteral("reading-time/%1/ledger.json").arg(scope));
}

QString ReadingTimeStore::statisticsPath(const QString &scope) const {
    return QDir(dataDir()).filePath(QStringLiteral("reading-time/%1/statistics.json").arg(scope));
}

bool ReadingTimeStore::loadLedgerForScope(const QString &scope, QHash<QString, qint64> *pending,
                                          QHash<QString, qint64> *uncertain) const {
    pending->clear();
    uncertain->clear();
    QFile file(ledgerPath(scope));
    if (!file.open(QIODevice::ReadOnly)) {
        return true;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return false;
    }
    const QJsonObject books = document.object().value(QStringLiteral("books")).toObject();
    for (auto it = books.constBegin(); it != books.constEnd(); ++it) {
        const QJsonObject row = it.value().toObject();
        const qint64 pendingMs = positiveMilliseconds(row.value(QStringLiteral("pendingMs")).toVariant().toLongLong());
        const qint64 uncertainMs = positiveMilliseconds(row.value(QStringLiteral("uncertainMs")).toVariant().toLongLong());
        if (pendingMs > 0) {
            pending->insert(it.key(), pendingMs);
        }
        if (uncertainMs > 0) {
            uncertain->insert(it.key(), uncertainMs);
        }
    }
    return true;
}

bool ReadingTimeStore::saveLedgerForScope(const QString &scope, const QHash<QString, qint64> &pending,
                                          const QHash<QString, qint64> &uncertain) const {
    if (scope.isEmpty()) {
        return false;
    }
    const QString path = ledgerPath(scope);
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }
    QJsonObject books;
    QSet<QString> ids;
    for (auto it = pending.cbegin(); it != pending.cend(); ++it) ids.insert(it.key());
    for (auto it = uncertain.cbegin(); it != uncertain.cend(); ++it) ids.insert(it.key());
    for (const QString &bookId : std::as_const(ids)) {
        const qint64 pendingMs = positiveMilliseconds(pending.value(bookId));
        const qint64 uncertainMs = positiveMilliseconds(uncertain.value(bookId));
        if (pendingMs <= 0 && uncertainMs <= 0) {
            continue;
        }
        QJsonObject row;
        row.insert(QStringLiteral("pendingMs"), static_cast<double>(pendingMs));
        row.insert(QStringLiteral("uncertainMs"), static_cast<double>(uncertainMs));
        books.insert(bookId, row);
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("books"), books);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.setPermissions(kOwnerReadWrite);
    if (file.write(QJsonDocument(root).toJson(QJsonDocument::Compact)) < 0
        || !file.commit()) {
        return false;
    }
    QFile permissions(path);
    permissions.setPermissions(kOwnerReadWrite);
    return true;
}

bool ReadingTimeStore::loadStatisticsForScope(const QString &scope,
                                              QHash<QString, DailyStatistics> *statistics) const {
    statistics->clear();
    QFile file(statisticsPath(scope));
    if (!file.open(QIODevice::ReadOnly)) {
        return true;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return false;
    }
    const QJsonObject days = document.object().value(QStringLiteral("days")).toObject();
    for (auto it = days.constBegin(); it != days.constEnd(); ++it) {
        const QDate date = QDate::fromString(it.key(), Qt::ISODate);
        const QJsonObject row = it.value().toObject();
        const qint64 milliseconds = positiveMilliseconds(row.value(QStringLiteral("milliseconds")).toVariant().toLongLong());
        if (!date.isValid() || milliseconds <= 0) {
            continue;
        }
        DailyStatistics value;
        value.milliseconds = milliseconds;
        const QJsonArray bookKeys = row.value(QStringLiteral("bookKeys")).toArray();
        for (const QJsonValue &bookKey : bookKeys) {
            const QString key = bookKey.toString();
            // Only accept SHA-256 hex keys. Never carry a book identifier
            // from a malformed statistics file into this independent store.
            if (key.size() == 64) {
                value.bookKeys.insert(key);
            }
        }
        statistics->insert(it.key(), value);
    }
    return true;
}

bool ReadingTimeStore::saveStatisticsForScope(const QString &scope,
                                              const QHash<QString, DailyStatistics> &statistics) const {
    if (scope.isEmpty()) {
        return false;
    }
    const QString path = statisticsPath(scope);
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }
    QJsonObject days;
    for (auto it = statistics.cbegin(); it != statistics.cend(); ++it) {
        const qint64 milliseconds = positiveMilliseconds(it.value().milliseconds);
        if (QDate::fromString(it.key(), Qt::ISODate).isValid() && milliseconds > 0) {
            QJsonArray bookKeys;
            for (const QString &bookKey : it.value().bookKeys) {
                bookKeys.append(bookKey);
            }
            QJsonObject row;
            row.insert(QStringLiteral("milliseconds"), static_cast<double>(milliseconds));
            row.insert(QStringLiteral("bookKeys"), bookKeys);
            days.insert(it.key(), row);
        }
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("days"), days);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.setPermissions(kOwnerReadWrite);
    if (file.write(QJsonDocument(root).toJson(QJsonDocument::Compact)) < 0 || !file.commit()) {
        return false;
    }
    QFile permissions(path);
    permissions.setPermissions(kOwnerReadWrite);
    return true;
}

void ReadingTimeStore::loadCurrentLedger() {
    if (!loadLedgerForScope(m_scope, &m_pendingMs, &m_uncertainMs)) {
        m_pendingMs.clear();
        m_uncertainMs.clear();
        m_statistics.clear();
        m_statusText = QStringLiteral("阅读时长账本无法读取");
        return;
    }
    if (!loadStatisticsForScope(m_scope, &m_statistics)) {
        m_statistics.clear();
        m_statusText = QStringLiteral("阅读统计无法读取");
        return;
    }
    updateStatus();
}

void ReadingTimeStore::clearCurrentLedger(const QString &statusText) {
    m_pendingMs.clear();
    m_uncertainMs.clear();
    m_statistics.clear();
    m_statusText = statusText;
}

bool ReadingTimeStore::saveCurrentLedger() {
    if (!scopeStillCurrent(m_scope)) {
        ensureCurrentAccount();
        return false;
    }
    const bool saved = saveLedgerForScope(m_scope, m_pendingMs, m_uncertainMs)
        && saveStatisticsForScope(m_scope, m_statistics);
    if (!saved) {
        m_statusText = QStringLiteral("阅读时长账本保存失败");
    } else {
        updateStatus();
    }
    emit changed();
    return saved;
}

bool ReadingTimeStore::saveCurrentStatistics() {
    if (!scopeStillCurrent(m_scope)) {
        ensureCurrentAccount();
        return false;
    }
    const bool saved = saveStatisticsForScope(m_scope, m_statistics);
    if (!saved) {
        m_statusText = QStringLiteral("阅读统计保存失败");
    }
    emit changed();
    return saved;
}

QVariantMap ReadingTimeStore::asVariantMap(const QHash<QString, qint64> &values) {
    QVariantMap result;
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (it.value() > 0) {
            result.insert(it.key(), it.value());
        }
    }
    return result;
}

QVariantMap ReadingTimeStore::pendingMillisecondsByBook() const {
    return asVariantMap(m_pendingMs);
}

qint64 ReadingTimeStore::pendingMilliseconds(const QString &bookId) const {
    return m_pendingMs.value(bookId.trimmed());
}

qint64 ReadingTimeStore::uncertainMilliseconds(const QString &bookId) const {
    return m_uncertainMs.value(bookId.trimmed());
}

qint64 ReadingTimeStore::positiveMilliseconds(qint64 milliseconds) {
    return milliseconds > 0 ? qMin(milliseconds, kMaxReasonableMilliseconds) : 0;
}

QString ReadingTimeStore::utcPlus8Day(qint64 timestampMilliseconds) {
    return QDateTime::fromMSecsSinceEpoch(timestampMilliseconds, Qt::UTC)
        .addSecs(8 * 60 * 60).date().toString(Qt::ISODate);
}

qint64 ReadingTimeStore::nextUtcPlus8Midnight(qint64 timestampMilliseconds) {
    const QDate cstDate = QDateTime::fromMSecsSinceEpoch(timestampMilliseconds, Qt::UTC)
        .addSecs(8 * 60 * 60).date();
    return QDateTime(cstDate.addDays(1), QTime(0, 0), Qt::UTC).toMSecsSinceEpoch()
        - 8LL * 60 * 60 * 1000;
}

QString ReadingTimeStore::bookStatisticsKey(const QString &bookId) {
    return QString::fromLatin1(QCryptographicHash::hash(bookId.toUtf8(), QCryptographicHash::Sha256).toHex());
}

bool ReadingTimeStore::recordObservedInterval(const QString &bookId, qint64 milliseconds,
                                              qint64 endTimestampMilliseconds, int generation) {
    const QString id = bookId.trimmed();
    const QString scopeBeforeRecord = m_scope;
    if (id.isEmpty() || milliseconds <= 0 || milliseconds > kMaxObservedIntervalMilliseconds
        || endTimestampMilliseconds <= 0 || generation != m_accountGeneration
        || !qgetenv("RM_WEREAD_QT_SELFTEST").isEmpty()
        || !ensureCurrentAccount() || scopeBeforeRecord != m_scope
        || generation != m_accountGeneration) {
        return false;
    }

    const QString key = bookStatisticsKey(id);
    qint64 segmentStart = endTimestampMilliseconds - milliseconds;
    qint64 remaining = milliseconds;
    while (remaining > 0) {
        const qint64 boundary = nextUtcPlus8Midnight(segmentStart);
        const qint64 segmentEnd = qMin(endTimestampMilliseconds, boundary);
        const qint64 segmentMs = segmentEnd - segmentStart;
        if (segmentMs <= 0) {
            return false;
        }
        DailyStatistics &day = m_statistics[utcPlus8Day(segmentStart)];
        day.milliseconds = qMin(kMaxReasonableMilliseconds, day.milliseconds + segmentMs);
        day.bookKeys.insert(key);
        remaining -= segmentMs;
        segmentStart = segmentEnd;
    }
    emit changed();
    return true;
}

void ReadingTimeStore::snapshotPending(const QString &bookId, qint64 milliseconds) {
    const QString id = bookId.trimmed();
    const QString scopeBeforeSnapshot = m_scope;
    if (id.isEmpty() || !ensureCurrentAccount() || scopeBeforeSnapshot != m_scope) {
        return;
    }
    const qint64 safeMs = positiveMilliseconds(milliseconds);
    if (safeMs > 0) {
        m_pendingMs.insert(id, safeMs);
    } else {
        m_pendingMs.remove(id);
    }
    saveCurrentLedger();
}

QString ReadingTimeStore::beginSubmission(const QString &bookId, int requestedSeconds) {
    const QString id = bookId.trimmed();
    const QString scopeBeforeSubmission = m_scope;
    if (id.isEmpty() || requestedSeconds <= 0 || !ensureCurrentAccount()
        || scopeBeforeSubmission != m_scope) {
        return {};
    }
    const qint64 eligibleMs = (m_pendingMs.value(id) / 1000) * 1000;
    const qint64 reservedMs = qMin(eligibleMs, static_cast<qint64>(requestedSeconds) * 1000);
    if (reservedMs <= 0) {
        return {};
    }
    m_pendingMs.insert(id, m_pendingMs.value(id) - reservedMs);
    m_uncertainMs.insert(id, m_uncertainMs.value(id) + reservedMs);
    if (!saveCurrentLedger()) {
        loadCurrentLedger();
        emit changed();
        return {};
    }

    const QByteArray entropy = m_scope.toUtf8() + '\0' + id.toUtf8() + '\0'
        + QByteArray::number(QDateTime::currentMSecsSinceEpoch()) + '\0'
        + QByteArray::number(QRandomGenerator::global()->generate64()) + '\0'
        + QByteArray::number(m_nextReservation++);
    const QString token = QString::fromLatin1(QCryptographicHash::hash(entropy, QCryptographicHash::Sha256).toHex());
    m_reservations.insert(token, Reservation{m_scope, id, reservedMs});
    emit submissionReserved(token, id, static_cast<int>(reservedMs / 1000));
    return token;
}

void ReadingTimeStore::completeSubmission(const QString &token, bool succeeded) {
    const auto it = m_reservations.find(token);
    if (it == m_reservations.end()) {
        return;
    }
    const Reservation reservation = it.value();
    m_reservations.erase(it);
    if (!succeeded) {
        // The durable uncertainty is intentionally retained to avoid a duplicate report.
        if (reservation.scope == m_scope) {
            updateStatus();
            emit changed();
        }
        return;
    }

    if (reservation.scope == m_scope && scopeStillCurrent(reservation.scope)) {
        QHash<QString, qint64> updatedUncertain = m_uncertainMs;
        updatedUncertain.insert(reservation.bookId,
                                qMax<qint64>(0, updatedUncertain.value(reservation.bookId) - reservation.milliseconds));
        if (updatedUncertain.value(reservation.bookId) <= 0) {
            updatedUncertain.remove(reservation.bookId);
        }
        if (!saveLedgerForScope(reservation.scope, m_pendingMs, updatedUncertain)) {
            m_statusText = QStringLiteral("阅读时长账本保存失败");
            emit changed();
            return;
        }
        m_uncertainMs = updatedUncertain;
        updateStatus();
        emit changed();
        return;
    }

    // A result arriving after an account switch may clear uncertainty only in
    // the original hashed ledger. It never loads or changes the current account.
    QHash<QString, qint64> pending;
    QHash<QString, qint64> uncertain;
    if (!loadLedgerForScope(reservation.scope, &pending, &uncertain)) {
        return;
    }
    uncertain.insert(reservation.bookId,
                     qMax<qint64>(0, uncertain.value(reservation.bookId) - reservation.milliseconds));
    if (uncertain.value(reservation.bookId) <= 0) {
        uncertain.remove(reservation.bookId);
    }
    if (saveLedgerForScope(reservation.scope, pending, uncertain)
        && reservation.scope == m_scope) {
        // The disk session changed while this request was in flight. Refresh
        // only after the original ledger has been updated successfully.
        ensureCurrentAccount();
    }
}

void ReadingTimeStore::updateStatus() {
    if (!accountReady()) {
        m_statusText = QStringLiteral("未识别已登录账号，阅读时长仅临时保留");
    } else if (hasUncertainSubmission()) {
        m_statusText = QStringLiteral("部分阅读时长提交结果未确认，不会自动重复提交");
    } else {
        m_statusText = QStringLiteral("阅读时长已本地保存");
    }
}
