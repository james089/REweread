#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <cstdio>
#include <dlfcn.h>

// Does not open the downloader, request a QR, read an account, or import a book.
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    if (argc != 2) return 2;
    auto *library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) { std::fputs("native library load failed\n", stderr); return 1; }
    auto construct = reinterpret_cast<void (*)()>(dlsym(library, "_xovi_construct"));
    if (!construct) return 1;
    construct();
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/weread-downloader/DownloaderPage.qml")));
    if (component.isError()) { std::fputs(component.errorString().toUtf8().constData(), stderr); return 1; }
    auto *page = qobject_cast<QQuickItem *>(component.create());
    if (!page) { std::fputs(component.errorString().toUtf8().constData(), stderr); return 1; }
    page->setWidth(954); page->setHeight(1696);
    QCoreApplication::processEvents();
    delete page;
    std::puts("native component load ok (no login or library operation)");
    return 0;
}
