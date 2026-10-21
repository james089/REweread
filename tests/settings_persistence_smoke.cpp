#include <QCoreApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QUrl>
#include <QVariant>
#include <memory>
#include <cstdlib>
int main(int argc,char **argv) {
 QCoreApplication app(argc,argv);
 if(argc!=3) return 2;
 QQmlEngine engine;
 engine.rootContext()->setContextProperty("settingsUrl",QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])));
 QQmlComponent component(&engine);
 component.setData(R"(
import QtQml
import QtCore
QtObject {
 id: root
 property string fontChoice: "霞鹜文楷"
 property int fontSize: 42
 property Settings storage: Settings {
  location: settingsUrl
  category: "reader"
  property alias fontChoice: root.fontChoice
  property alias fontSize: root.fontSize
 }
 function save() {
  storage.setValue("fontChoice",root.fontChoice)
  storage.setValue("fontSize",root.fontSize)
  storage.sync()
 }
}
)",QUrl());
 std::unique_ptr<QObject> object(component.create());
 if(!object) {qCritical("settings QML creation failed");return 3;}
 if(QString::fromLocal8Bit(argv[2])=="write") {
  object->setProperty("fontChoice",QStringLiteral("思源宋体"));object->setProperty("fontSize",50);
  if(!QMetaObject::invokeMethod(object.get(),"save")) return 4;
  qInfo("settings-persistence-write passed");
  std::_Exit(0); // No Settings destructor: verify explicit disk sync survives termination.
 }
 if(object->property("fontChoice").toString()!=QStringLiteral("思源宋体") || object->property("fontSize").toInt()!=50) {qCritical("settings failed to restore after restart");return 5;}
 qInfo("settings-persistence-restart passed");
 return 0;
}
