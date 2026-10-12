#include "notes_store.h"
#include "session_json_reader.h"

#include <QDateTime>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringList>

namespace {
constexpr qint64 kSocialCacheTtlSeconds = 24 * 60 * 60;
}

NotesStore::NotesStore(QObject *parent)
    : QObject(parent),
      m_running(false),
      m_statusText(QStringLiteral("微信笔记未同步")) {
    m_processTimeoutTimer.setSingleShot(true);
    m_processTimeoutTimer.setInterval(18000);
    connect(&m_processTimeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_process.state() != QProcess::NotRunning) {
            m_process.kill();
        }
        setState(false, QStringLiteral("笔记加载超时"));
    });
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        appendOutput(m_process.readAllStandardOutput(), m_stdoutBuffer);
    });
    connect(&m_process, &QProcess::readyReadStandardError, this, [this]() {
        appendOutput(m_process.readAllStandardError(), m_stderrBuffer);
    });
    connect(&m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &NotesStore::finishProcess);
}

bool NotesStore::running() const {
    return m_running;
}

QString NotesStore::statusText() const {
    return m_statusText;
}

QVariantList NotesStore::notebooks() const {
    return m_notebooks;
}

QVariantList NotesStore::bookNotes() const {
    return m_bookNotes;
}

QVariantList NotesStore::popularMarks() const {
    return m_popularMarks;
}

QString NotesStore::socialCachePath() const {
    const QByteArray overrideDir = qgetenv("RM_WEREAD_DATA_DIR");
    const QString dataDir = overrideDir.isEmpty()
        ? QStringLiteral("/home/root/.local/share/rm-weread")
        : QString::fromUtf8(overrideDir);
    return QDir(dataDir).filePath(QStringLiteral("social-comments-cache.json"));
}

QString NotesStore::personalNotesCachePath() const {
    const QByteArray overrideDir = qgetenv("RM_WEREAD_DATA_DIR");
    const QString dataDir = overrideDir.isEmpty()
        ? QStringLiteral("/home/root/.local/share/rm-weread")
        : QString::fromUtf8(overrideDir);
    return QDir(dataDir).filePath(QStringLiteral("personal-notes-cache.json"));
}

QString NotesStore::currentAccountScope() const {
    const QByteArray sessionOverride = qgetenv("RM_WEREAD_SESSION_PATH");
    const QString sessionPath = sessionOverride.isEmpty()
        ? QStringLiteral("/home/root/.local/share/rm-weread/session.json")
        : QString::fromUtf8(sessionOverride);
    QFile file(sessionPath);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const SessionJsonReader::Result parsed = SessionJsonReader::parse(file.readAll());
    const QJsonDocument document = parsed.document;
    if (!document.isObject()) return {};
    const QJsonObject session = document.object();
    if (session.value(QStringLiteral("logged_out")).toBool()) return {};
    const QJsonObject cookies = session.value(QStringLiteral("cookies")).toObject();
    const QString vid = cookies.value(QStringLiteral("wr_vid")).toString().trimmed();
    const QString sessionKey = cookies.value(QStringLiteral("wr_skey")).toString().trimmed();
    if (vid.isEmpty() || sessionKey.isEmpty()
        || (parsed.usedLatin1Fallback && !SessionJsonReader::isAsciiDecimalIdentifier(vid))) return {};
    return QString::fromLatin1(QCryptographicHash::hash(vid.toUtf8(), QCryptographicHash::Sha256).toHex());
}

bool NotesStore::isPersonalMode() const {
    return m_mode == QStringLiteral("notebooks") || m_mode == QStringLiteral("book");
}

bool NotesStore::personalRequestStillCurrent() const {
    return !m_personalRequestScope.isEmpty()
        && m_personalRequestScope == m_personalNotesScope
        && m_personalRequestScope == currentAccountScope();
}

void NotesStore::reloadAccountScope() {
    const QString scope = currentAccountScope();
    if (scope == m_personalNotesScope) {
        if (scope.isEmpty()) {
            m_notebooks.clear();
            m_bookNotes.clear();
            m_bookNotesBookId.clear();
            m_personalRequestRows.clear();
            setState(false, QStringLiteral("请先登录微信读书"));
        }
        return;
    }
    m_personalNotesScope = scope;
    m_personalRequestScope.clear();
    m_personalRequestRows.clear();
    m_personalRequestDone = false;
    m_personalRequestFailed = false;
    m_personalRequestExpectedCount = -1;
    m_notebooks.clear();
    m_bookNotes.clear();
    m_bookNotesBookId.clear();
    if (isPersonalMode() && m_process.state() != QProcess::NotRunning) {
        m_processTimeoutTimer.stop();
        m_process.kill();
    }
    if (scope.isEmpty()) setState(false, QStringLiteral("请先登录微信读书"));
    else emit changed();
}

bool NotesStore::preparePersonalNotesScope() {
    reloadAccountScope();
    if (m_personalNotesScope.isEmpty()) return false;
    m_personalRequestScope = m_personalNotesScope;
    m_personalRequestRows.clear();
    m_personalRequestDone = false;
    m_personalRequestFailed = false;
    m_personalRequestExpectedCount = -1;
    return true;
}

QVariantMap NotesStore::loadPersonalNotesCache() const {
    QFile file(personalNotesCachePath());
    if (!file.open(QIODevice::ReadOnly)) return {};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object().toVariantMap() : QVariantMap();
}

void NotesStore::savePersonalNotesCache(const QVariantMap &cache) const {
    const QString path = personalNotesCachePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (!file.open(QIODevice::WriteOnly)) return;
    if (file.write(QJsonDocument(QJsonObject::fromVariantMap(cache)).toJson(QJsonDocument::Compact)) < 0) {
        file.cancelWriting();
        return;
    }
    if (file.commit()) QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

bool NotesStore::loadCachedNotebooks() {
    if (m_personalNotesScope.isEmpty()) return false;
    const QVariantMap accounts = loadPersonalNotesCache().value(QStringLiteral("accounts")).toMap();
    const QVariantMap account = accounts.value(m_personalNotesScope).toMap();
    const QVariantMap entry = account.value(QStringLiteral("notebooks")).toMap();
    if (!entry.contains(QStringLiteral("rows"))) return false;
    m_notebooks = entry.value(QStringLiteral("rows")).toList();
    setState(false, m_notebooks.isEmpty() ? QStringLiteral("暂无微信笔记（缓存）") : QStringLiteral("微信笔记书籍 %1 本（缓存）").arg(m_notebooks.size()));
    return true;
}

bool NotesStore::loadCachedBookNotes(const QString &bookId) {
    if (m_personalNotesScope.isEmpty()) return false;
    const QVariantMap accounts = loadPersonalNotesCache().value(QStringLiteral("accounts")).toMap();
    const QVariantMap account = accounts.value(m_personalNotesScope).toMap();
    const QVariantMap books = account.value(QStringLiteral("books")).toMap();
    const QVariantMap entry = books.value(bookId).toMap();
    if (!entry.contains(QStringLiteral("rows"))) return false;
    m_bookNotes = entry.value(QStringLiteral("rows")).toList();
    m_bookNotesBookId = bookId;
    setState(false, m_bookNotes.isEmpty() ? QStringLiteral("本书暂无微信笔记（缓存）") : QStringLiteral("本书笔记 %1 条（缓存）").arg(m_bookNotes.size()));
    return true;
}

void NotesStore::persistNotebooksCache() {
    if (!personalRequestStillCurrent()) return;
    QVariantMap cache = loadPersonalNotesCache();
    QVariantMap accounts = cache.value(QStringLiteral("accounts")).toMap();
    QVariantMap account = accounts.value(m_personalRequestScope).toMap();
    QVariantMap entry;
    entry.insert(QStringLiteral("updatedAt"), QDateTime::currentSecsSinceEpoch());
    entry.insert(QStringLiteral("rows"), m_notebooks);
    account.insert(QStringLiteral("notebooks"), entry);
    accounts.insert(m_personalRequestScope, account);
    cache.insert(QStringLiteral("version"), 1);
    cache.insert(QStringLiteral("accounts"), accounts);
    savePersonalNotesCache(cache);
}

void NotesStore::persistBookNotesCache() {
    if (!personalRequestStillCurrent() || m_bookNotesBookId.isEmpty()) return;
    QVariantMap cache = loadPersonalNotesCache();
    QVariantMap accounts = cache.value(QStringLiteral("accounts")).toMap();
    QVariantMap account = accounts.value(m_personalRequestScope).toMap();
    QVariantMap books = account.value(QStringLiteral("books")).toMap();
    QVariantMap entry;
    entry.insert(QStringLiteral("updatedAt"), QDateTime::currentSecsSinceEpoch());
    entry.insert(QStringLiteral("rows"), m_bookNotes);
    books.insert(m_bookNotesBookId, entry);
    account.insert(QStringLiteral("books"), books);
    accounts.insert(m_personalRequestScope, account);
    cache.insert(QStringLiteral("version"), 1);
    cache.insert(QStringLiteral("accounts"), accounts);
    savePersonalNotesCache(cache);
}

QVariantMap NotesStore::loadSocialCache() const {
    QFile file(socialCachePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object().toVariantMap() : QVariantMap();
}

void NotesStore::saveSocialCache(const QVariantMap &cache) const {
    const QString path = socialCachePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(QJsonDocument(QJsonObject::fromVariantMap(cache)).toJson(QJsonDocument::Compact));
}

bool NotesStore::socialCacheEntryFresh(const QVariantMap &entry) const {
    const qint64 updatedAt = entry.value(QStringLiteral("updatedAt")).toLongLong();
    return updatedAt > 0 && QDateTime::currentSecsSinceEpoch() - updatedAt <= kSocialCacheTtlSeconds;
}

QString NotesStore::popularContextCacheKey(const QString &bookId, const QString &contextKey) const {
    return bookId + QStringLiteral("|") + contextKey;
}

QString NotesStore::popularReviewCacheKey(const QString &bookId, const QString &chapterUid, const QString &range) const {
    return bookId + QStringLiteral("|") + chapterUid + QStringLiteral("|") + range;
}

bool NotesStore::loadCachedPopularMarks(const QString &bookId, const QString &contextKey) {
    const QVariantMap cache = loadSocialCache();
    const QVariantMap contexts = cache.value(QStringLiteral("contexts")).toMap();
    const QString key = popularContextCacheKey(bookId, contextKey);
    if (!contexts.contains(key)) {
        return false;
    }
    const QVariantMap entry = contexts.value(key).toMap();
    if (!socialCacheEntryFresh(entry)) {
        return false;
    }
    m_mode = QStringLiteral("popular");
    m_popularBookId = bookId;
    m_popularContextKey = contextKey;
    m_popularMarks = entry.value(QStringLiteral("marks")).toList();
    setState(false, m_popularMarks.isEmpty()
        ? QStringLiteral("本页暂无划线评论（缓存）")
        : QStringLiteral("划线评论 %1 处（缓存）").arg(m_popularMarks.size()));
    return true;
}

void NotesStore::persistPopularMarksCache() {
    if (m_popularBookId.isEmpty() || m_popularContextKey.isEmpty()) {
        return;
    }
    QVariantMap cache = loadSocialCache();
    QVariantMap contexts = cache.value(QStringLiteral("contexts")).toMap();
    QVariantMap entry;
    entry.insert(QStringLiteral("updatedAt"), QDateTime::currentSecsSinceEpoch());
    entry.insert(QStringLiteral("marks"), m_popularMarks);
    contexts.insert(popularContextCacheKey(m_popularBookId, m_popularContextKey), entry);
    cache.insert(QStringLiteral("version"), 1);
    cache.insert(QStringLiteral("contexts"), contexts);
    saveSocialCache(cache);
}

bool NotesStore::loadCachedPopularReviews(const QString &bookId, const QString &chapterUid, const QString &range) {
    const QVariantMap cache = loadSocialCache();
    const QVariantMap reviews = cache.value(QStringLiteral("reviews")).toMap();
    const QString key = popularReviewCacheKey(bookId, chapterUid, range);
    if (!reviews.contains(key)) {
        return false;
    }
    const QVariantMap entry = reviews.value(key).toMap();
    if (!socialCacheEntryFresh(entry)) {
        return false;
    }
    const QVariantList items = entry.value(QStringLiteral("items")).toList();
    for (int i = 0; i < m_popularMarks.size(); ++i) {
        QVariantMap mark = m_popularMarks.at(i).toMap();
        if (mark.value(QStringLiteral("chapterUid")).toString() == chapterUid
                && mark.value(QStringLiteral("range")).toString() == range) {
            mark.insert(QStringLiteral("reviews"), items);
            m_popularMarks[i] = mark;
            setState(false, items.isEmpty()
                ? QStringLiteral("暂无评论详情（缓存）")
                : QStringLiteral("评论 %1 条（缓存）").arg(items.size()));
            return true;
        }
    }
    return false;
}

void NotesStore::persistPopularReviewsCache() {
    if (m_popularReviewBookId.isEmpty() || m_popularReviewChapterUid.isEmpty() || m_popularReviewRange.isEmpty()) {
        return;
    }
    QVariantList items;
    for (const QVariant &value : m_popularMarks) {
        const QVariantMap mark = value.toMap();
        if (mark.value(QStringLiteral("chapterUid")).toString() == m_popularReviewChapterUid
                && mark.value(QStringLiteral("range")).toString() == m_popularReviewRange) {
            items = mark.value(QStringLiteral("reviews")).toList();
            break;
        }
    }
    QVariantMap cache = loadSocialCache();
    QVariantMap reviews = cache.value(QStringLiteral("reviews")).toMap();
    QVariantMap entry;
    entry.insert(QStringLiteral("updatedAt"), QDateTime::currentSecsSinceEpoch());
    entry.insert(QStringLiteral("items"), items);
    reviews.insert(popularReviewCacheKey(m_popularReviewBookId, m_popularReviewChapterUid, m_popularReviewRange), entry);
    cache.insert(QStringLiteral("version"), 1);
    cache.insert(QStringLiteral("reviews"), reviews);
    saveSocialCache(cache);
}

void NotesStore::refreshNotebooks() {
    if (m_running) {
        return;
    }
    if (!preparePersonalNotesScope()) {
        return;
    }
    m_mode = QStringLiteral("notebooks");
    loadCachedNotebooks();
    startHelper(QStringList() << QStringLiteral("notebooks"), QStringLiteral("正在同步微信笔记..."));
}

void NotesStore::refreshBookNotes(const QString &bookId) {
    if (m_running) {
        return;
    }

    const QString trimmed = bookId.trimmed();
    if (trimmed.isEmpty()) {
        setState(false, QStringLiteral("先打开一本书"));
        return;
    }

    if (!preparePersonalNotesScope()) {
        return;
    }
    m_mode = QStringLiteral("book");
    if (!loadCachedBookNotes(trimmed) && m_bookNotesBookId != trimmed) {
        m_bookNotes.clear();
        m_bookNotesBookId = trimmed;
        emit changed();
    }
    startHelper(QStringList() << QStringLiteral("book") << trimmed, QStringLiteral("正在加载本书笔记..."));
}

void NotesStore::refreshPopularMarks(const QString &bookId) {
    const QString trimmed = bookId.trimmed();
    if (trimmed.isEmpty()) {
        setState(false, QStringLiteral("先打开一本书"));
        return;
    }

    if (m_running) {
        m_pendingPopularBookId = trimmed;
        m_pendingPopularContextKey = m_popularContextKey;
        return;
    }

    m_mode = QStringLiteral("popular");
    m_popularBookId = trimmed;
    if (m_popularContextKey.isEmpty()) {
        m_popularContextKey = QStringLiteral("book");
    }
    m_popularMarks.clear();
    emit changed();
    QStringList args;
    const QRegularExpression chapterUidKey(QStringLiteral("^chapterUid:([^:]+)"));
    const QRegularExpressionMatch chapterUidMatch = chapterUidKey.match(m_popularContextKey);
    const QRegularExpression chapterIndexKey(QStringLiteral("^chapterIndex:(\\d+)"));
    const QRegularExpressionMatch chapterIndexMatch = chapterIndexKey.match(m_popularContextKey);
    const QRegularExpression chapterKey(QStringLiteral("^chapter:(\\d+)"));
    const QRegularExpressionMatch chapterMatch = chapterKey.match(m_popularContextKey);
    const QRegularExpression pageKey(QStringLiteral(":page:(\\d+)-(\\d+)"));
    const QRegularExpressionMatch pageMatch = pageKey.match(m_popularContextKey);
    const QString pageStart = pageMatch.hasMatch() ? pageMatch.captured(1) : QString();
    const QString pageEnd = pageMatch.hasMatch() ? pageMatch.captured(2) : QString();
    if (chapterUidMatch.hasMatch()) {
        args << QStringLiteral("chapter") << trimmed << chapterUidMatch.captured(1);
    } else if (chapterIndexMatch.hasMatch()) {
        args << QStringLiteral("chapter") << trimmed << QStringLiteral("index:%1").arg(chapterIndexMatch.captured(1));
    } else if (chapterMatch.hasMatch()) {
        args << QStringLiteral("chapter") << trimmed << chapterMatch.captured(1);
    } else {
        args << QStringLiteral("popular") << trimmed;
    }
    if (!pageStart.isEmpty() && !pageEnd.isEmpty() && args.size() >= 3 && args.first() == QStringLiteral("chapter")) {
        args << pageStart << pageEnd;
    }
    startHelper(args, QStringLiteral("正在加载划线评论..."));
}

void NotesStore::refreshPopularReviews(const QString &bookId, const QString &chapterUid, const QString &range) {
    const QString trimmedBookId = bookId.trimmed();
    const QString trimmedChapterUid = chapterUid.trimmed();
    const QString trimmedRange = range.trimmed();
    if (trimmedBookId.isEmpty() || trimmedChapterUid.isEmpty() || trimmedRange.isEmpty()) {
        return;
    }
    if (loadCachedPopularReviews(trimmedBookId, trimmedChapterUid, trimmedRange)) {
        return;
    }
    if (m_running) {
        return;
    }
    m_popularReviewBookId = trimmedBookId;
    m_popularReviewChapterUid = trimmedChapterUid;
    m_popularReviewRange = trimmedRange;
    m_mode = QStringLiteral("popular_review");
    startHelper(QStringList()
                    << QStringLiteral("range_reviews")
                    << trimmedBookId
                    << trimmedChapterUid
                    << trimmedRange,
                QStringLiteral("正在加载这处评论..."));
}

void NotesStore::bufferPopularMarks(const QString &bookId) {
    bufferPopularMarksForContext(bookId, QStringLiteral("book"));
}

void NotesStore::bufferPopularMarksForContext(const QString &bookId, const QString &contextKey) {
    const QString trimmed = bookId.trimmed();
    const QString key = contextKey.trimmed().isEmpty() ? QStringLiteral("book") : contextKey.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    if (popularMarksBuffered(trimmed) && m_popularContextKey == key) {
        return;
    }
    if (loadCachedPopularMarks(trimmed, key)) {
        return;
    }
    if (m_running) {
        if (m_mode == QStringLiteral("popular")) {
            if (m_popularBookId == trimmed && m_popularContextKey == key) {
                return;
            }
            m_pendingPopularBookId = trimmed;
            m_pendingPopularContextKey = key;
            m_cancelledForPopularRestart = true;
            m_processTimeoutTimer.stop();
            if (m_process.state() != QProcess::NotRunning) {
                m_process.kill();
            }
            setState(true, QStringLiteral("正在切换划线评论..."));
            return;
        }
        m_pendingPopularBookId = trimmed;
        m_pendingPopularContextKey = key;
        return;
    }
    m_popularContextKey = key;
    refreshPopularMarks(trimmed);
}

void NotesStore::cancelPopularMarks() {
    if (m_mode != QStringLiteral("popular")) {
        return;
    }
    m_pendingPopularBookId.clear();
    m_pendingPopularContextKey.clear();
    m_popularBookId.clear();
    m_popularContextKey.clear();
    m_popularMarks.clear();
    if (m_process.state() != QProcess::NotRunning) {
        m_cancelledForPopularRestart = true;
        m_processTimeoutTimer.stop();
        m_process.kill();
        setState(true, QStringLiteral("划线评论已暂停"));
    } else {
        setState(false, QStringLiteral("划线评论已暂停"));
    }
    emit changed();
}

bool NotesStore::popularMarksBuffered(const QString &bookId) const {
    const QString trimmed = bookId.trimmed();
    return !trimmed.isEmpty()
        && m_popularBookId == trimmed
        && !m_popularMarks.isEmpty();
}

void NotesStore::startHelper(const QStringList &arguments, const QString &statusText) {
    const QString luajit = QStringLiteral("/home/root/xovi/exthome/appload/koreader/luajit");
    const QString helper = QStringLiteral("/home/root/weread-qt/helper/tools/fetch-notes.lua");
    if (!QFileInfo::exists(luajit) || !QFileInfo::exists(helper)) {
        setState(false, QStringLiteral("笔记组件缺失"));
        return;
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("RM_WEREAD_APP_DIR"), QStringLiteral("/home/root/weread-qt/helper"));
    env.insert(QStringLiteral("KO_DIR"), QStringLiteral("/home/root/xovi/exthome/appload/koreader"));
    if (!arguments.isEmpty() && arguments.first() == QStringLiteral("popular")) {
        env.insert(QStringLiteral("RM_WEREAD_MARK_LIMIT"), QStringLiteral("80"));
    }
    if (!arguments.isEmpty() && arguments.first() == QStringLiteral("chapter")) {
        env.insert(QStringLiteral("RM_WEREAD_SKIP_REVIEWS"), QStringLiteral("1"));
    }
    m_process.setProcessEnvironment(env);
    m_process.setProgram(luajit);
    m_process.setArguments(QStringList() << helper << arguments);
    setState(true, statusText);
    m_process.start();
    if (!m_process.waitForStarted(3000)) {
        m_processTimeoutTimer.stop();
        setState(false, QStringLiteral("笔记组件无法启动"));
    } else {
        m_processTimeoutTimer.start();
    }
}

void NotesStore::appendOutput(const QByteArray &chunk, QByteArray &buffer, bool finalChunk) {
    buffer.append(chunk);
    for (;;) {
        const int newline = buffer.indexOf('\n');
        if (newline < 0) {
            break;
        }
        const QByteArray line = buffer.left(newline);
        buffer.remove(0, newline + 1);
        appendOutputLine(line);
    }
    if (finalChunk && !buffer.trimmed().isEmpty()) {
        appendOutputLine(buffer);
    }
    if (finalChunk) {
        buffer.clear();
    }
}

void NotesStore::appendOutputLine(const QByteArray &rawLine) {
    const QByteArray line = rawLine.trimmed();
    if (line.isEmpty()) {
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(line);
    if (!document.isObject()) {
        return;
    }

    const QJsonObject object = document.object();
    const QString state = object.value(QStringLiteral("state")).toString();
    if (m_cancelledForPopularRestart && m_mode == QStringLiteral("popular")) {
        return;
    }
    if (isPersonalMode() && !personalRequestStillCurrent()) {
        return;
    }
    if (state == QStringLiteral("row")) {
        appendRow(object.toVariantMap());
    } else if (state == QStringLiteral("done")) {
        if (isPersonalMode()) {
            m_personalRequestDone = true;
            m_personalRequestExpectedCount = object.value(QStringLiteral("count")).toInt(-1);
        }
        const int count = object.value(QStringLiteral("count")).toInt();
        if (m_mode == QStringLiteral("popular")) {
            m_statusText = count > 0 ? QStringLiteral("划线评论 %1 处").arg(count) : QStringLiteral("暂无划线评论");
        } else if (m_mode == QStringLiteral("popular_review")) {
            m_statusText = count > 0 ? QStringLiteral("评论 %1 条").arg(count) : QStringLiteral("暂无评论详情");
        } else if (m_mode == QStringLiteral("book")) {
            m_statusText = count > 0 ? QStringLiteral("本书笔记 %1 条").arg(count) : QStringLiteral("本书暂无微信笔记");
        } else {
            m_statusText = count > 0 ? QStringLiteral("微信笔记书籍 %1 本").arg(count) : QStringLiteral("暂无微信笔记");
        }
    } else if (state == QStringLiteral("error")) {
        if (isPersonalMode()) {
            m_personalRequestFailed = true;
        }
        setState(m_running, QStringLiteral("微信笔记加载失败"));
    }
}

void NotesStore::finishProcess(int exitCode, QProcess::ExitStatus exitStatus) {
    m_processTimeoutTimer.stop();
    if (m_process.isOpen()) {
        appendOutput(m_process.readAllStandardOutput(), m_stdoutBuffer, true);
        appendOutput(m_process.readAllStandardError(), m_stderrBuffer, true);
    } else {
        appendOutput({}, m_stdoutBuffer, true);
        appendOutput({}, m_stderrBuffer, true);
    }
    const bool cancelledForPopularRestart = m_cancelledForPopularRestart;
    const bool personalMode = isPersonalMode();
    const bool personalRequestCurrent = !personalMode || personalRequestStillCurrent();
    const bool completedPersonalRequest = !personalMode || (m_personalRequestDone
        && !m_personalRequestFailed
        && m_personalRequestExpectedCount >= 0
        && m_personalRequestRows.size() == m_personalRequestExpectedCount);
    const bool succeeded = exitStatus == QProcess::NormalExit && exitCode == 0 && completedPersonalRequest;
    m_cancelledForPopularRestart = false;
    if (succeeded && personalMode && personalRequestCurrent) {
        if (m_mode == QStringLiteral("notebooks")) {
            m_notebooks = m_personalRequestRows;
            persistNotebooksCache();
        } else if (m_mode == QStringLiteral("book")) {
            m_bookNotes = m_personalRequestRows;
            persistBookNotesCache();
        }
    } else if (succeeded && m_mode == QStringLiteral("popular")) {
        persistPopularMarksCache();
    } else if (succeeded && m_mode == QStringLiteral("popular_review")) {
        persistPopularReviewsCache();
    }
    if (personalMode && !personalRequestCurrent) {
        m_notebooks.clear();
        m_bookNotes.clear();
        m_bookNotesBookId.clear();
        setState(false, QStringLiteral("请先登录微信读书"));
    } else if (cancelledForPopularRestart && m_mode == QStringLiteral("popular")) {
        setState(false, QStringLiteral("划线评论已暂停"));
    } else if (succeeded) {
        if (m_statusText.contains(QStringLiteral("正在"))) {
            if (m_mode == QStringLiteral("popular")) {
                setState(false, QStringLiteral("划线评论已同步"));
            } else if (m_mode == QStringLiteral("popular_review")) {
                setState(false, QStringLiteral("评论已加载"));
            } else {
                setState(false, m_mode == QStringLiteral("book") ? QStringLiteral("本书笔记已同步") : QStringLiteral("微信笔记已同步"));
            }
        } else {
            setState(false, m_statusText);
        }
    } else if (personalMode && m_personalRequestDone && !m_personalRequestFailed) {
        setState(false, QStringLiteral("微信笔记数据不完整"));
    } else if (!m_statusText.contains(QStringLiteral("失败"))) {
        setState(false, QStringLiteral("微信笔记加载失败"));
    } else {
        setState(false, m_statusText);
    }

    if (m_mode == QStringLiteral("popular_review")) {
        m_popularReviewBookId.clear();
        m_popularReviewChapterUid.clear();
        m_popularReviewRange.clear();
    }

    if (personalMode) {
        m_personalRequestScope.clear();
        m_personalRequestRows.clear();
        m_personalRequestDone = false;
        m_personalRequestFailed = false;
        m_personalRequestExpectedCount = -1;
    }

    if (!m_pendingPopularBookId.isEmpty() && !m_running) {
        const QString bookId = m_pendingPopularBookId;
        const QString contextKey = m_pendingPopularContextKey;
        m_pendingPopularBookId.clear();
        m_pendingPopularContextKey.clear();
        bufferPopularMarksForContext(bookId, contextKey);
    }
}

void NotesStore::appendRow(const QVariantMap &row) {
    const QString kind = row.value(QStringLiteral("kind")).toString();
    const bool notifyChanged = kind != QStringLiteral("popular_mark")
        && kind != QStringLiteral("popular_review");
    if (kind == QStringLiteral("notebook")) {
        if (isPersonalMode()) {
            m_personalRequestRows.append(row);
        } else {
            m_notebooks.append(row);
        }
    } else if (kind == QStringLiteral("highlight") || kind == QStringLiteral("thought")) {
        if (isPersonalMode()) {
            m_personalRequestRows.append(row);
        } else {
            m_bookNotes.append(row);
        }
    } else if (kind == QStringLiteral("popular_mark")) {
        QVariantMap mark = row;
        mark.insert(QStringLiteral("reviews"), QVariantList());
        m_popularMarks.append(mark);
    } else if (kind == QStringLiteral("popular_review")) {
        const QString chapterUid = row.value(QStringLiteral("chapterUid")).toString();
        const QString range = row.value(QStringLiteral("range")).toString();
        QVariantMap review;
        review.insert(QStringLiteral("author"), row.value(QStringLiteral("author")));
        review.insert(QStringLiteral("content"), row.value(QStringLiteral("text")));
        for (int i = 0; i < m_popularMarks.size(); ++i) {
            QVariantMap mark = m_popularMarks.at(i).toMap();
            if (mark.value(QStringLiteral("chapterUid")).toString() == chapterUid
                    && mark.value(QStringLiteral("range")).toString() == range) {
                QVariantList reviews = mark.value(QStringLiteral("reviews")).toList();
                reviews.append(review);
                mark.insert(QStringLiteral("reviews"), reviews);
                m_popularMarks[i] = mark;
                break;
            }
        }
    }
    if (notifyChanged) {
        if (!isPersonalMode()) {
            emit changed();
        }
    }
}

void NotesStore::setState(bool running, const QString &statusText) {
    m_running = running;
    m_statusText = statusText;
    emit changed();
}
