#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTimer>
#define private public
#include "reader_store.h"
#undef private

static QByteArray bytes(const QString &path) {
    QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {}; return f.readAll();
}
static QJsonObject rows(const QString &path) { return QJsonDocument::fromJson(bytes(path)).object(); }
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir tmp; Q_ASSERT(tmp.isValid());
    qputenv("RM_WEREAD_DATA_DIR", tmp.path().toUtf8());
    const QString path = tmp.filePath("reader-progress.json");
    QFile initial(path); Q_ASSERT(initial.open(QIODevice::WriteOnly));
    initial.write("{\"untouched\":{\"textOffset\":99}}"); initial.close();
    const QByteArray original = bytes(path);
    {
        ReaderStore store;
        store.m_progressSaveTimer.setInterval(20);
        store.queueProgress("a", 1, 10, 50);
        store.queueProgress("a", 2, 10, 100);
        Q_ASSERT(bytes(path) == original); // No synchronous turn-time disk write.
        Q_ASSERT(store.savedTextOffset("a") == 100); // Resume sees latest queued position.
        QEventLoop loop; QTimer::singleShot(60, &loop, &QEventLoop::quit); loop.exec();
        Q_ASSERT(rows(path).value("a").toObject().value("textOffset").toInt() == 100);
        Q_ASSERT(rows(path).value("untouched").toObject().value("textOffset").toInt() == 99);
        store.queueProgress("a", 3, 10, 150);
        store.queueProgress("b", 2, 5, 200);
        store.saveProgress("a", 4, 10, 160); // Exit/sleep/sync forces all queued books out.
        Q_ASSERT(rows(path).value("a").toObject().value("textOffset").toInt() == 160);
        Q_ASSERT(rows(path).value("b").toObject().value("textOffset").toInt() == 200);
        store.queueProgress("c", 1, 8, 330);
    }
    Q_ASSERT(rows(path).value("c").toObject().value("textOffset").toInt() == 330);
    Q_ASSERT(QFile::remove(path)); Q_ASSERT(QDir().mkpath(path)); // Simulate write failure.
    {
        ReaderStore store; store.queueProgress("retry", 1, 3, 80); store.flushPendingProgress();
        Q_ASSERT(store.savedTextOffset("retry") == 80);
        Q_ASSERT(QDir(path).removeRecursively()); store.flushPendingProgress();
        Q_ASSERT(rows(path).value("retry").toObject().value("textOffset").toInt() == 80);
    }
    qInfo("reader progress coalescing behavior ok");
}
