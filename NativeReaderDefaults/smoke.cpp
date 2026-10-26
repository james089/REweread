#include <QCoreApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <cstdio>
#include <dlfcn.h>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (argc != 2) return 2;
    void *library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) { std::fputs("default extension load failed\n", stderr); return 1; }
    auto construct = reinterpret_cast<void (*)()>(dlsym(library, "_xovi_construct"));
    if (!construct) return 1;
    construct();
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQml 2.15\nimport org.reweread.NativeDefaults 1.0\n"
                      "QtObject { property bool result: ReaderDefaults.needsContrastDefault(\"invalid-id\", \"\") }",
                      QUrl("file:///synthetic-smoke.qml"));
    if (component.isError()) { std::fputs(component.errorString().toUtf8().constData(), stderr); return 1; }
    QObject *object = component.create();
    if (!object || object->property("result").toBool()) return 1;
    delete object;
    std::puts("native defaults module/QML smoke ok (no library read or write)");
}
