#include "defaults.h"
#include <QQmlEngine>
#include <QDebug>
#include <QtQml/qqml.h>
#include <dlfcn.h>

extern "C" void _xovi_construct() {
    qmlRegisterSingletonType<NativeReaderDefaults>("org.reweread.NativeDefaults", 1, 0, "ReaderDefaults",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new NativeReaderDefaults; });
    qInfo("native-reader-defaults: registered");
}
extern "C" char _xovi_shouldLoad() {
    return dlsym(RTLD_DEFAULT, "_Z21qRegisterResourceDataiPKhS0_S0_") != nullptr;
}
extern "C" __attribute__((section(".xovi_info"))) const int EXTENSIONVERSION = 0x00000100;
__attribute__((section(".xovi"))) const char *LINKTABLENAMES = "Ephony\0\0";
__attribute__((section(".xovi"))) const void *LINKTABLEVALUES[] = {(void *)1, (void *)0};
