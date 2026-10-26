#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QEventLoop>
#include <QTimer>
#include <QFile>
#include <QScopedPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <cstdio>
#include <cstdlib>

static void checkAt(bool value, int line) {
    if (!value) { std::fprintf(stderr, "TOC routing assertion failed at line %d\n", line); std::abort(); }
}
#define check(value) checkAt((value), __LINE__)
static QString fragment(const QString &source, const QString &name) {
    const QString first = "// " + name + "Begin";
    const QString last = "// " + name + "End";
    const int begin = source.indexOf(first);
    const int end = source.indexOf(last, begin);
    check(begin >= 0 && end > begin);
    return source.mid(begin + first.size(), end - begin - first.size());
}
static void nextTurn() {
    QEventLoop loop;
    QTimer::singleShot(50, &loop, &QEventLoop::quit);
    loop.exec();
}

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    check(argc == 2);
    QFile patch(QString::fromLocal8Bit(argv[1]));
    check(patch.open(QIODevice::ReadOnly));
    const QString source = QString::fromUtf8(patch.readAll());
    // Execute the shipped navigation code, not a second implementation of it.
    const QString qml = QStringLiteral(R"(
import QtQuick 2.15
Item {
    id: pages
    visible: false
    property var document: ({ id: "synthetic-document" })
    property int bookReturnCount: 0
    property alias tocActive: tableOfContentsLoader.active
    property QtObject documentView: QtObject {
        function closeOverview() { pages.visible = false; pages.bookReturnCount += 1; }
    }
    function openDirect() { visible = true; tableOfContentsLoader.active = true; }
    function openOverviewOnly() { visible = true; }
    function openTocInOverview() { tableOfContentsLoader.active = true; }
    function leaveForChapter() { visible = false; tableOfContentsLoader.active = false; }
    Loader {
        id: tableOfContentsLoader
        active: false
        onActiveChanged: {
    )") + fragment(source, "NativeReaderTocActivation") + "\n}\n}\n"
        + fragment(source, "NativeReaderTocOrigin") + "\n}";
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(qml.toUtf8(), QUrl("file:///synthetic-toc-navigation.qml"));
    if (component.isError()) std::fputs(component.errorString().toUtf8().constData(), stderr);
    QScopedPointer<QObject> pages(component.create());
    check(bool(pages));
    QQuickWindow window;
    auto *item = qobject_cast<QQuickItem *>(pages.data());
    check(item != nullptr);
    item->setParentItem(window.contentItem());
    window.show();
    nextTurn();
    auto invoke = [&](const char *method) { check(QMetaObject::invokeMethod(pages.data(), method)); };
    auto state = [&](const char *name) { return pages->property(name).toBool(); };

    // Both direct entry points follow openOverview + activate TOC synchronously.
    invoke("openDirect");
    check(state("nativeReaderTocReturnsToBook") && state("tocActive"));
    nextTurn();
    check(state("nativeReaderTocReturnsToBook"));
    invoke("nativeReaderFinishToc");
    check(!state("visible") && !state("tocActive"));
    check(pages->property("bookReturnCount").toInt() == 1);

    // A separate interaction in an existing overview must return to overview.
    invoke("openOverviewOnly");
    nextTurn();
    invoke("openTocInOverview");
    check(!state("nativeReaderTocReturnsToBook"));
    invoke("nativeReaderFinishToc");
    check(state("visible") && !state("tocActive"));
    check(pages->property("bookReturnCount").toInt() == 1);

    // Chapter selection/close must not carry a direct-entry flag to a later visit.
    invoke("leaveForChapter");
    invoke("openDirect");
    invoke("leaveForChapter");
    check(!state("nativeReaderTocReturnsToBook"));
    invoke("openOverviewOnly");
    nextTurn();
    invoke("openTocInOverview");
    invoke("nativeReaderFinishToc");
    check(state("visible") && pages->property("bookReturnCount").toInt() == 1);

    // Document replacement must clear the previous document's return destination.
    invoke("leaveForChapter");
    invoke("openDirect");
    pages->setProperty("document", QVariantMap{{"id", "second-synthetic-document"}});
    check(!state("nativeReaderTocReturnsToBook") && !state("nativeReaderOverviewJustOpened"));
    std::puts("TOC return routing ok: direct, overview, repeated visit and document replacement");
}
