// Isolated ReaderStore free-ink eraser behavior test.  It uses only a fresh
// QTemporaryDir via RM_WEREAD_DATA_DIR; no device, account, network, or user
// data is opened.

#include "reader_store.h"

#include <QCoreApplication>
#include <QTemporaryDir>

#include <cstdlib>

namespace {
[[noreturn]] void fail(const char *message) {
    qCritical("%s", message);
    std::exit(1);
}

void expect(bool condition, const char *message) {
    if (!condition) fail(message);
}

QVariantMap point(int x, int y) {
    return {{QStringLiteral("x"), x}, {QStringLiteral("y"), y},
            {QStringLiteral("pressure"), 1.0}};
}

QVariantMap stroke(const QString &tool, const QVariantList &points, int width = 4) {
    return {{QStringLiteral("tool"), tool},
            {QStringLiteral("colorName"), QStringLiteral("test")},
            {QStringLiteral("colorValue"), QStringLiteral("#111111")},
            {QStringLiteral("lineWidth"), width},
            {QStringLiteral("points"), points}};
}

int countTool(const QVariantList &strokes, const QString &tool) {
    int result = 0;
    for (const QVariant &value : strokes) {
        if (value.toMap().value(QStringLiteral("tool")).toString() == tool) ++result;
    }
    return result;
}
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    expect(temporary.isValid(), "cannot create temporary ReaderStore data directory");
    qputenv("RM_WEREAD_DATA_DIR", temporary.path().toUtf8());

    const QString firstBook = QStringLiteral("eraser-book-a");
    const QString secondBook = QStringLiteral("eraser-book-b");
    const int page = 3;
    ReaderStore store;

    const QVariantList horizontal = {point(100, 100), point(160, 100)};
    const QVariantList vertical = {point(200, 170), point(200, 230)};
    const QVariantList distant = {point(100, 300), point(160, 300)};
    store.addPageStrokesBatch(firstBook, QStringLiteral("test"), page, 10,
                              {stroke(QStringLiteral("free"), horizontal),
                               stroke(QStringLiteral("marker"), horizontal),
                               stroke(QStringLiteral("free"), vertical),
                               stroke(QStringLiteral("free"), distant)});
    store.addPageStrokesBatch(firstBook, QStringLiteral("test"), page + 1, 10,
                              {stroke(QStringLiteral("free"), horizontal)});
    store.addPageStrokesBatch(secondBook, QStringLiteral("test"), page, 10,
                              {stroke(QStringLiteral("free"), horizontal)});

    expect(store.erasePageStrokesAtPath(firstBook, page, {}, 20) == 0,
           "an empty eraser path must not modify strokes");

    // A one-point eraser must hit the horizontal free stroke but preserve the
    // marker, the next page, and another book.
    expect(store.erasePageStrokesAtPath(firstBook, page, {point(130, 100)}, 20) == 1,
           "single-point eraser must remove its touched free stroke");
    store.loadStrokesForPage(firstBook, page);
    expect(countTool(store.pageStrokes(), QStringLiteral("free")) == 2,
           "point eraser must leave unrelated current-page free strokes");
    expect(countTool(store.pageStrokes(), QStringLiteral("marker")) == 1,
           "free-ink eraser must preserve a marker stroke");

    // A swept horizontal eraser crossing a vertical stroke must hit it.
    expect(store.erasePageStrokesAtPath(firstBook, page,
                                        {point(170, 200), point(230, 200)}, 12) == 1,
           "swept eraser must remove a crossing free stroke");
    // Collinear paths that are far apart must not count as an intersection.
    expect(store.erasePageStrokesAtPath(firstBook, page,
                                        {point(250, 300), point(310, 300)}, 4) == 0,
           "separated collinear eraser path must not remove free ink");
    // A non-intersecting path must leave the final free stroke untouched.
    expect(store.erasePageStrokesAtPath(firstBook, page,
                                        {point(100, 350), point(160, 350)}, 10) == 0,
           "non-intersecting eraser must not remove free ink");

    store.loadStrokesForPage(firstBook, page);
    expect(countTool(store.pageStrokes(), QStringLiteral("free")) == 1,
           "only the distant current-page free stroke should remain");
    store.loadStrokesForPage(firstBook, page + 1);
    expect(countTool(store.pageStrokes(), QStringLiteral("free")) == 1,
           "eraser must not affect another page");
    store.loadStrokesForPage(secondBook, page);
    expect(countTool(store.pageStrokes(), QStringLiteral("free")) == 1,
           "eraser must not affect another book");

    // Reconstructing the store proves the accepted deletion was persisted to
    // the temporary JSON ledger, rather than only updating the live QML list.
    ReaderStore reloaded;
    reloaded.loadStrokesForPage(firstBook, page);
    expect(countTool(reloaded.pageStrokes(), QStringLiteral("free")) == 1,
           "free-ink eraser result must survive ReaderStore reload");
    expect(countTool(reloaded.pageStrokes(), QStringLiteral("marker")) == 1,
           "marker preservation must survive ReaderStore reload");
    return 0;
}
