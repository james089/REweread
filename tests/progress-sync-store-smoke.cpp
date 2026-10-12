// Standalone QtCore smoke test. Build it locally with moc plus
// progress_sync_store.cpp; it deliberately splits a UTF-8 summary across two
// simulated readyRead chunks.
#define private public
#include "../apps/weread-qt/progress_sync_store.h"
#undef private

#include <QCoreApplication>

#include <cstdio>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    ProgressSyncStore store;
    store.m_running = true;
    store.m_mode = QStringLiteral("pull");
    store.m_bookId = QStringLiteral("book-smoke");

    bool detailedSignal = false;
    QObject::connect(&store, &ProgressSyncStore::progressPulledDetailed,
                     [&detailedSignal](const QString &, const QVariantMap &position) {
        detailedSignal = position.value(QStringLiteral("summary")).toString()
            == QStringLiteral("春风又绿江南岸");
    });

    const QByteArray line = QStringLiteral(
        "{\"state\":\"done\",\"progress\":12,\"chapterUid\":\"123\","
        "\"chapterIdx\":4,\"chapterOffset\":56,\"summary\":\"春风又绿江南岸\"}\n").toUtf8();
    const QByteArray summaryPrefix = QStringLiteral("\"summary\":\"").toUtf8();
    const int split = line.indexOf(summaryPrefix) + summaryPrefix.size() + 1;
    if (split <= summaryPrefix.size() || split >= line.size()) {
        std::fputs("could not split UTF-8 summary\n", stderr);
        return 1;
    }

    store.appendOutput(store.m_stdoutBuffer, line.left(split), true);
    if (!store.m_pulledPosition.isEmpty()) {
        std::fputs("partial UTF-8 chunk was parsed early\n", stderr);
        return 1;
    }
    store.appendOutput(store.m_stdoutBuffer, line.mid(split), true);
    if (store.m_pulledPosition.value(QStringLiteral("summary")).toString()
        != QStringLiteral("春风又绿江南岸")) {
        std::fputs("UTF-8 summary was not preserved\n", stderr);
        return 1;
    }

    store.finishProcess(0, QProcess::NormalExit);
    if (!detailedSignal) {
        std::fputs("detailed signal was not emitted after successful completion\n", stderr);
        return 1;
    }

    ProgressSyncStore unterminatedStore;
    unterminatedStore.m_running = true;
    unterminatedStore.m_mode = QStringLiteral("pull");
    unterminatedStore.m_bookId = QStringLiteral("book-smoke");
    bool stderrCouldNotCompleteProtocol = false;
    QObject::connect(&unterminatedStore, &ProgressSyncStore::progressPulledDetailed,
                     [&stderrCouldNotCompleteProtocol](const QString &, const QVariantMap &) {
        stderrCouldNotCompleteProtocol = true;
    });
    unterminatedStore.appendOutput(unterminatedStore.m_stderrBuffer, line.left(line.size() - 1), false);
    if (!unterminatedStore.m_pulledPosition.isEmpty()) {
        std::fputs("unterminated line was parsed before process completion\n", stderr);
        return 1;
    }
    unterminatedStore.finishProcess(0, QProcess::NormalExit);
    if (stderrCouldNotCompleteProtocol) {
        std::fputs("stderr must not complete a progress protocol request\n", stderr);
        return 1;
    }
    return 0;
}
