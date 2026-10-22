// Run locally with Qt Core (no device or network needed):
// moc apps/weread-qt/download_store.h -o /tmp/moc_download_store.cpp
// c++ -std=c++17 -fPIC tests/download_store_result_protocol_smoke.cpp \
//   apps/weread-qt/download_store.cpp /tmp/moc_download_store.cpp ...QtCore...
//
// A previous full EPUB must survive a forced repair whose current helper run
// only produces a partial EPUB.  The partial file remains the active result.

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#define private public
#include "../apps/weread-qt/download_store.h"
#undef private

static void writeFile(const QString &path, const QByteArray &contents) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    Q_ASSERT(file.open(QIODevice::WriteOnly));
    Q_ASSERT(file.write(contents) == contents.size());
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir root;
    Q_ASSERT(root.isValid());
    qputenv("RM_WEREAD_DATA_DIR", root.path().toUtf8());

    const QString bookId = QStringLiteral("中文-book");
    const QString bookDir = root.filePath(QStringLiteral("books/") + bookId);
    const QString oldFull = bookDir + QStringLiteral("/book-full.epub");
    const QString newPartial = bookDir + QStringLiteral("/中文-partial.epub");
    writeFile(oldFull, "old full EPUB");
    writeFile(newPartial, "new partial EPUB");

    DownloadStore store;
    auto writeStatus = [&](const QString &state, int failed = 0, const QString &path = QString()) {
        const QJsonObject row{{QStringLiteral("downloadState"), state},
                              {QStringLiteral("failedChapterCount"), failed},
                              {QStringLiteral("fullFile"), path.isEmpty() ? oldFull : path}};
        writeFile(root.filePath(QStringLiteral("book-status.json")),
                  QJsonDocument(QJsonObject{{bookId, row}}).toJson());
    };
    Q_ASSERT(!store.hasCompleteDownload(bookId)); // Filename alone is insufficient.
    writeStatus(QStringLiteral("full"));
    Q_ASSERT(store.hasCompleteDownload(bookId));
    writeStatus(QStringLiteral("partial"));
    Q_ASSERT(store.hasCompleteDownload(bookId)); // A prior completed file survives a partial repair.
    writeStatus(QStringLiteral("downloading"));
    Q_ASSERT(store.hasCompleteDownload(bookId));
    store.downloadBook(bookId, QStringLiteral("Fixture"));
    Q_ASSERT(!store.running() && store.state() == QStringLiteral("done")); // No helper/network.
    writeStatus(QStringLiteral("partial"), 0, newPartial);
    Q_ASSERT(!store.hasCompleteDownload(bookId));
    writeStatus(QStringLiteral("full"), 1);
    Q_ASSERT(store.hasCompleteDownload(bookId)); // Failed new chapters do not revoke the prior full file.
    writeFile(root.filePath(QStringLiteral("book-status.json")),
              QJsonDocument(QJsonObject{{bookId, QJsonObject{{"downloadState", "partial"}}}}).toJson());
    Q_ASSERT(!store.hasCompleteDownload(bookId)); // No completion record: filename alone does not pass.
    writeStatus(QStringLiteral("full"));
    writeFile(oldFull, "");
    Q_ASSERT(!store.hasCompleteDownload(bookId));
    writeFile(oldFull, "old full EPUB");
    store.m_bookId = bookId;
    store.m_title = QStringLiteral("Book");
    store.processOutputLine(QStringLiteral("state=downloading index=3/12 failed=0"));
    Q_ASSERT(store.downloads().first().toMap().value(QStringLiteral("completedChapters")).toInt() == 3);
    Q_ASSERT(store.downloads().first().toMap().value(QStringLiteral("totalChapters")).toInt() == 12);
    store.recordDownloadRow(QStringLiteral("other-book"), QStringLiteral("Other"), QStringLiteral("queued"), QString(), QString());
    Q_ASSERT(store.downloads().first().toMap().value(QStringLiteral("completedChapters")).toInt() == 0);
    store.processOutputLine(QStringLiteral("state=downloading index=99/12 failed=0"));
    Q_ASSERT(store.downloads().first().toMap().value(QStringLiteral("completedChapters")).toInt() == 12);

    // Split inside the UTF-8 bytes for \"中\" and omit the final newline.  The
    // process completion flush must still recover the original Chinese path.
    const QByteArray result = (QStringLiteral("result state=partial path=") + newPartial).toUtf8();
    const int split = result.indexOf(QByteArray::fromHex("e4b8ad")) + 1;
    Q_ASSERT(split > 0);
    store.appendOutput(store.m_stdoutBuffer, result.left(split));
    store.appendOutput(store.m_stdoutBuffer, result.mid(split));
    store.finishProcess(0, QProcess::NormalExit);

    Q_ASSERT(store.state() == QStringLiteral("partial"));
    Q_ASSERT(!store.downloads().isEmpty());
    const QVariantMap row = store.downloads().first().toMap();
    Q_ASSERT(row.value(QStringLiteral("state")).toString() == QStringLiteral("partial"));
    Q_ASSERT(row.value(QStringLiteral("completedChapters")).toInt() == 12);
    Q_ASSERT(row.value(QStringLiteral("totalChapters")).toInt() == 12);
    Q_ASSERT(row.value(QStringLiteral("localEpubPath")).toString() == newPartial);
    Q_ASSERT(QFileInfo::exists(oldFull));

    QTemporaryDir outsideRoot;
    Q_ASSERT(outsideRoot.isValid());
    const QString outside = outsideRoot.filePath(QStringLiteral("outside.epub"));
    const QString outsideLink = bookDir + QStringLiteral("/outside-link.epub");
    writeFile(outside, "not this book");
    Q_ASSERT(QFile::link(outside, outsideLink));
    store.m_helperResultState = QStringLiteral("partial");
    store.m_helperResultPath = outsideLink;
    Q_ASSERT(!store.hasHelperResult(QStringLiteral("partial")));
    writeStatus(QStringLiteral("full"), 0, outsideLink);
    Q_ASSERT(!store.hasCompleteDownload(bookId));
    int removedSignals = 0;
    QObject::connect(&store, &DownloadStore::downloadRemoved, [&](const QString &id) {
        Q_ASSERT(id == bookId);
        Q_ASSERT(!QFileInfo::exists(bookDir));
        ++removedSignals;
    });
    store.deleteDownload(bookId, QStringLiteral("Fixture"));
    Q_ASSERT(removedSignals == 1);
    Q_ASSERT(QFileInfo::exists(outside));
    return 0;
}
