#include "defaults.h"
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QtQml/qqml.h>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <cstdio>
#include <cstdlib>

static void check(bool ok) { if (!ok) std::abort(); }
static QString testLibraryPath;

class Controller final : public QObject {
    Q_OBJECT
public:
    int calls = 0;
    QString lastValue;
    Q_INVOKABLE void setViewBackgroundFilter(const QString &, const QString &value) {
        ++calls;
        lastValue = value;
    }
};

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    QTemporaryDir dir;
    check(dir.isValid());
    NativeReaderDefaults defaults(dir.path(), nullptr);
    const QString id = QStringLiteral("00000000-0000-4000-8000-000000000001");
    const auto path = dir.filePath(id + ".content");
    auto run = [&](QJsonObject object, bool expected, const QString &current = QString()) {
        const QByteArray bytes = QJsonDocument(object).toJson();
        { QFile file(path); check(file.open(QIODevice::WriteOnly)); check(file.write(bytes) == bytes.size()); }
        check(defaults.needsContrastDefault(id, current) == expected);
        QFile unchanged(path); check(unchanged.open(QIODevice::ReadOnly)); check(unchanged.readAll() == bytes);
    };
    check(!defaults.needsContrastDefault(id, "")); // Missing file: no mutation.
    run({{"fileType", "pdf"}}, true);
    run({{"fileType", "epub"}}, true);
    run({{"fileType", "notebook"}}, false);
    run({}, false);
    for (const auto &value : {QJsonValue(""), QJsonValue("off"), QJsonValue("fullpage"), QJsonValue(QJsonValue::Null), QJsonValue(17)})
        run({{"fileType", "pdf"}, {"viewBackgroundFilter", value}}, false);
    run({{"fileType", "pdf"}}, false, "fullpage"); // Unsaved live selection wins.
    check(!defaults.needsContrastDefault("../" + id, ""));
    { QFile file(path); check(file.open(QIODevice::WriteOnly)); file.write("{broken"); }
    check(!defaults.needsContrastDefault(id, ""));
    check(QFile::remove(path));
    const auto other = dir.filePath("other");
    { QFile file(other); check(file.open(QIODevice::WriteOnly)); file.write("{\"fileType\":\"pdf\"}"); }
    check(QFile::link(other, path));
    check(!defaults.needsContrastDefault(id, ""));
    check(QFile::remove(path));

    // Exercise the actual QMD-inserted handler against a synthetic native controller.
    check(argc == 2);
    QFile patch(QString::fromLocal8Bit(argv[1]));
    check(patch.open(QIODevice::ReadOnly));
    const QString qmd = QString::fromUtf8(patch.readAll());
    const QString marker = QStringLiteral("INSERT {");
    const int begin = qmd.indexOf(marker) + marker.size();
    const int end = qmd.lastIndexOf(QStringLiteral("        }"));
    check(begin >= marker.size() && end > begin);
    testLibraryPath = dir.path();
    qmlRegisterSingletonType<NativeReaderDefaults>("org.reweread.NativeDefaults", 1, 0, "ReaderDefaults",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new NativeReaderDefaults(testLibraryPath, nullptr); });
    Controller controller;
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("DocumentController", &controller);
    QQmlComponent component(&engine);
    const QString source = "import QtQuick 2.15\nimport org.reweread.NativeDefaults 1.0\n"
        "Item { id: root; signal documentOpened(var document)\n" + qmd.mid(begin, end - begin) + "\n}";
    component.setData(source.toUtf8(), QUrl("file:///synthetic-defaults.qml"));
    if (component.isError()) std::fputs(component.errorString().toUtf8().constData(), stderr);
    QScopedPointer<QObject> root(component.create());
    check(bool(root));
    auto opened = [&] {
        QVariant document = QVariantMap{{"id", id}, {"viewBackgroundFilter", ""}};
        check(QMetaObject::invokeMethod(root.data(), "documentOpened", Q_ARG(QVariant, document)));
    };
    run({{"fileType", "pdf"}}, true);
    opened();
    check(controller.calls == 1 && controller.lastValue == "off");
    for (const auto &value : {"", "off", "fullpage"}) {
        run({{"fileType", "pdf"}, {"viewBackgroundFilter", value}}, false);
        opened();
        check(controller.calls == 1);
    }
    run({{"fileType", "notebook"}}, false);
    opened();
    check(controller.calls == 1);
    std::puts("native reader default policy and QMD handler ok (synthetic metadata only)");
}

#include "tests.moc"
