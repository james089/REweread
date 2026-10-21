#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cstdlib>

#define private public
#include "notes_store.h"
#undef private

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

void writeSession(const QString &path, const QString &vid, bool loggedOut = false) {
    QJsonObject cookies;
    cookies.insert(QStringLiteral("wr_vid"), vid);
    cookies.insert(QStringLiteral("wr_skey"), QStringLiteral("test-session-only"));
    QJsonObject session;
    session.insert(QStringLiteral("cookies"), cookies);
    session.insert(QStringLiteral("logged_out"), loggedOut);
    QFile file(path);
    expect(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "cannot create isolated test session");
    expect(file.write(QJsonDocument(session).toJson(QJsonDocument::Compact)) >= 0, "cannot write isolated test session");
}

void writeInvalidUtf8Session(const QString &path, const QString &vid) {
    QByteArray session = QByteArrayLiteral("{\"cookies\":{\"wr_vid\":\"") + vid.toUtf8()
        + QByteArrayLiteral("\",\"wr_skey\":\"test\",\"opaque_cookie\":\"");
    session.append(static_cast<char>(0xff));
    session += QByteArrayLiteral("\"},\"logged_out\":false}");
    QFile file(path);
    expect(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "cannot create invalid UTF-8 note session");
    expect(file.write(session) == session.size(), "cannot write invalid UTF-8 note session");
}

void writeBrokenSession(const QString &path) {
    QFile file(path);
    expect(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "cannot create broken note session");
    expect(file.write(QByteArrayLiteral("{\"cookies\":")) > 0, "cannot write broken note session");
}

QString scopeFor(const QString &vid) {
    return QString::fromLatin1(QCryptographicHash::hash(vid.toUtf8(), QCryptographicHash::Sha256).toHex());
}

void writeAccountACache(const QString &path, const QString &scope) {
    QVariantMap notebook;
    notebook.insert(QStringLiteral("kind"), QStringLiteral("notebook"));
    notebook.insert(QStringLiteral("bookId"), QStringLiteral("fixture-book"));
    QVariantMap entry;
    entry.insert(QStringLiteral("rows"), QVariantList{notebook});
    QVariantMap account;
    account.insert(QStringLiteral("notebooks"), entry);
    QVariantMap accounts;
    accounts.insert(scope, account);
    QVariantMap root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("accounts"), accounts);
    QFile file(path);
    expect(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "cannot create isolated note cache");
    expect(file.write(QJsonDocument(QJsonObject::fromVariantMap(root)).toJson(QJsonDocument::Compact)) >= 0,
           "cannot write isolated note cache");
}
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    expect(temporary.isValid(), "cannot create temporary test directory");
    const QString sessionPath = temporary.filePath(QStringLiteral("session.json"));
    qputenv("RM_WEREAD_DATA_DIR", temporary.path().toUtf8());
    qputenv("RM_WEREAD_SESSION_PATH", sessionPath.toUtf8());

    NotesStore quietStore;
    expect(!quietStore.socialEnabled(), "personal build must disable public reading activity");
    quietStore.refreshPopularMarks(QStringLiteral("fixture-book"));
    quietStore.refreshPopularReviews(QStringLiteral("fixture-book"), QStringLiteral("1"), QStringLiteral("1-2"));
    quietStore.bufferPopularMarksForContext(QStringLiteral("fixture-book"), QStringLiteral("chapter:1"));
    expect(!quietStore.running() && quietStore.m_mode.isEmpty() && quietStore.popularMarks().isEmpty(),
           "disabled public activity must not start a helper or load cached marks");

    const QString accountA = QStringLiteral("fixture-account-a");
    const QString accountB = QStringLiteral("fixture-account-b");
    writeAccountACache(temporary.filePath(QStringLiteral("personal-notes-cache.json")), scopeFor(accountA));
    writeSession(sessionPath, accountA);

    NotesStore store;
    store.reloadAccountScope();
    store.refreshNotebooks();
    expect(store.notebooks().size() == 1, "account A must load only its cached personal notebooks");

    writeInvalidUtf8Session(sessionPath, QStringLiteral("10001"));
    expect(store.currentAccountScope() == scopeFor(QStringLiteral("10001")),
           "notes must derive a scope from an ASCII numeric vid after the UTF-8 fallback");
    writeInvalidUtf8Session(sessionPath, QStringLiteral("invalid-id"));
    expect(store.currentAccountScope().isEmpty(),
           "notes must reject a non-decimal identity after the UTF-8 fallback");
    writeBrokenSession(sessionPath);
    expect(store.currentAccountScope().isEmpty(),
           "notes must reject syntax errors instead of applying the UTF-8 fallback");
    writeSession(sessionPath, accountA);

    NotesStore splitStore;
    splitStore.reloadAccountScope();
    expect(splitStore.preparePersonalNotesScope(), "account A must prepare an isolated personal request");
    splitStore.m_mode = QStringLiteral("notebooks");
    const QByteArray row = QByteArrayLiteral("{\"state\":\"row\",\"kind\":\"notebook\",\"bookId\":\"fixture-book\",\"title\":\"")
        + QStringLiteral("汉").toUtf8() + QByteArrayLiteral("\"}\n");
    const int hanOffset = row.indexOf(QStringLiteral("汉").toUtf8());
    expect(hanOffset > 0, "UTF-8 fixture must contain a Chinese character");
    splitStore.appendOutput(row.left(hanOffset + 1), splitStore.m_stdoutBuffer);
    expect(splitStore.m_personalRequestRows.isEmpty(), "a partial UTF-8/JSON stdout chunk must not create a row");
    splitStore.appendOutput(row.mid(hanOffset + 1), splitStore.m_stdoutBuffer);
    splitStore.appendOutput(QByteArrayLiteral("{\"state\":\"done\",\"count\":1}"), splitStore.m_stdoutBuffer, true);
    splitStore.finishProcess(0, QProcess::NormalExit);
    expect(splitStore.notebooks().size() == 1, "split UTF-8 row plus final no-newline done must commit exactly once");

    NotesStore incompleteStore;
    incompleteStore.reloadAccountScope();
    expect(incompleteStore.preparePersonalNotesScope(), "account A must prepare a missing-row request");
    incompleteStore.m_mode = QStringLiteral("notebooks");
    incompleteStore.appendOutput(QByteArrayLiteral("{\"state\":\"done\",\"count\":1}"), incompleteStore.m_stdoutBuffer, true);
    incompleteStore.finishProcess(0, QProcess::NormalExit);
    expect(incompleteStore.statusText() == QStringLiteral("微信笔记数据不完整"),
           "done count without all rows must reject cache replacement");

    writeSession(sessionPath, accountB);
    store.reloadAccountScope();
    expect(store.notebooks().isEmpty() && store.bookNotes().isEmpty(),
           "switching to account B must synchronously clear account A note models");

    writeSession(sessionPath, accountB, true);
    store.reloadAccountScope();
    expect(store.notebooks().isEmpty() && store.bookNotes().isEmpty(),
           "logged-out state must not retain account A note models");
    expect(store.statusText() == QStringLiteral("请先登录微信读书"),
           "logged-out state must report that no personal account cache is available");
    return 0;
}
