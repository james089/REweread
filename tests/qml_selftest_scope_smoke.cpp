#include <QCoreApplication>
#include <QFile>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QDebug>
#include <memory>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QFile source("apps/weread-qt/Main.qml");
    if (!source.open(QIODevice::ReadOnly)) return 2;
    const auto match = QRegularExpression(QStringLiteral(
        "(?:readonly\\s+)?property\\s+string\\s+selfTestMode\\s*:[^\\n]+(?:\\n[ \\t]+\\?[^\\n]+)?"))
        .match(QString::fromUtf8(source.readAll()));
    if (!match.hasMatch()) return 3;
    // Evaluate the actual production root property, not a copied expression.
    const QByteArray qml = "import QtQml 2.0\nQtObject { id: root; "
        + match.captured().toUtf8() + "\n"
        "property bool normal: root.selfTestMode === \"\"; "
        "property bool inkAllowed: root.selfTestMode === \"\" || root.selfTestMode === \"reader-ink\" }";
    for (const QString &mode : {QString(), QStringLiteral("reader-ink"), QStringLiteral("reader-image")}) {
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("runtimeSelfTestMode", mode);
        QQmlComponent component(&engine);
        component.setData(qml, QUrl());
        std::unique_ptr<QObject> root(component.create());
        if (!root) { qWarning() << component.errors(); return 4; }
        if (root->property("selfTestMode").toString() != mode
            || root->property("normal").toBool() != mode.isEmpty()
            || root->property("inkAllowed").toBool() != (mode.isEmpty() || mode == "reader-ink")) return 5;
    }
    qInfo("QML self-test scope behavior ok");
}
