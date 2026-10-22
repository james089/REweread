#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#define private public
#include "shelf_store.h"
#undef private

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    Q_ASSERT(temp.isValid());
    qputenv("RM_WEREAD_DATA_DIR", temp.path().toUtf8());
    QDir().mkpath(temp.filePath("books/fixture"));
    const QString path = temp.filePath("books/fixture/book-full.epub");
    QFile file(path);
    Q_ASSERT(file.open(QIODevice::WriteOnly));
    file.write("synthetic EPUB"); file.close();
    ShelfStore shelf;
    const QVariantMap raw{{"bookId", "fixture"}, {"title", "Fixture"}, {"downloadState", "full"}};
    Q_ASSERT(shelf.normalizeBook(raw, 0).value("downloadState").toString() == "full");
    Q_ASSERT(QFile::remove(path));
    const QVariantMap deleted = shelf.normalizeBook(raw, 0);
    Q_ASSERT(deleted.value("downloadState").toString() == "remote");
    Q_ASSERT(deleted.value("localEpubPath").toString().isEmpty());
    Q_ASSERT(deleted.value("downloadActionText").toString() == QStringLiteral("全部下载"));
    qInfo("shelf deleted-file state behavior ok");
}
