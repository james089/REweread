#include "controller.h"
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QDir>
#include <QCryptographicHash>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QtTest/qtesttouch.h>
#include <QtTest/qtestmouse.h>
#include <QTemporaryDir>
#include <QThread>
#include <QtQml/qqml.h>
#include <functional>
#include <iostream>
#include <stdexcept>

static QQuickItem *visualChild(QQuickItem *item, const QString &name) {
    if (item->objectName() == name) return item;
    for (auto *child : item->childItems()) if (auto *found = visualChild(child, name)) return found;
    return nullptr;
}
static DownloaderController *controller = nullptr;
static const char *phase = "setup";
static void checked(bool value, int line) { if (!value) throw std::runtime_error(std::string(phase) + " assertion failed at " + std::to_string(line)); }
#define check(value) checked(bool(value), __LINE__)
static void until(const std::function<bool()> &predicate, int timeout = 8000) {
    QElapsedTimer timer; timer.start();
    while (!predicate() && timer.elapsed() < timeout) { QCoreApplication::processEvents(); QThread::msleep(5); }
    if (!predicate() && controller) std::cerr << "state: " << controller->status().toStdString() << " busy=" << controller->busy() << " loggedIn=" << controller->loggedIn() << " books=" << controller->books().size() << "\n";
    check(predicate());
}
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    try {
        check(argc == 3);
        QTemporaryDir temporary; check(temporary.isValid());
        auto program = temporary.path() + "/fake-worker";
        QFile file(program); check(file.open(QIODevice::WriteOnly));
        file.write(R"SH(#!/bin/sh
script_root=${0%/*}
printf '%s\n' "$1" >> "$script_root/calls"
if [ "$1" = reading-progress ] && [ -f "$script_root/slow-progress" ]; then exec /bin/sleep 5; fi
case "$1" in
cloud-time) printf '%s\n' '{"state":"cloud_time_ready","accountScope":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}' ;;
reading-tick)
 if [ -f "$script_root/slow-report" ]; then exec /bin/sleep 5; fi
 if [ -f "$script_root/entry-fail-once" ]; then rm "$script_root/entry-fail-once"; printf '%s\n' '{"state":"error","code":"progress_network_failed"}'; exit 1; fi
 if [ -f "$script_root/entry-always-fail" ]; then printf '%s\n' '{"state":"error","code":"progress_network_failed"}'; exit 1; fi
 printf '%s\n' '{"state":"reading_session_reported","timeUploaded":true,"timeUploadedSeconds":60,"bookId":"'"$2"'"}' ;;
preview-native-restore)
 printf '%s\n' '{"state":"native_restore_preview","bookId":"'"$2"'","uuid":"'"$WEREAD_NATIVE_PULL_UUID"'","accountScope":"'"$WEREAD_NATIVE_PULL_SCOPE"'","token":"'"$WEREAD_NATIVE_PULL_TOKEN"'","chapterIndex":2,"chapterCount":4,"chapterName":"synthetic chapter","snapshot":"synthetic snapshot"}' ;;
apply-native-restore)
 printf '%s\n' '{"state":"restore_position","bookId":"'"$2"'","uuid":"'"$WEREAD_NATIVE_PULL_UUID"'","accountScope":"'"$WEREAD_NATIVE_PULL_SCOPE"'","page":2,"pageCount":4}'
 IFS= read -r response
 if [ "$response" != ok ]; then exit 1; fi
 printf '%s\n' '{"state":"progress_restored","bookId":"'"$2"'","accountScope":"'"$WEREAD_NATIVE_PULL_SCOPE"'"}' ;;
exit-sync)
 printf '%s\n' "$*" >> "$script_root/exit-args"
 /bin/sleep 0.3
 if [ -f "$script_root/exit-rejected" ]; then printf '%s\n' '{"state":"error","code":"login_required"}'; exit 1; fi
 printf '%s\n' '{"state":"progress_synced","bookId":"'"$2"'","accountScope":"'"$3"'","uuid":"'"$4"'","recentMarked":false,"timeUploaded":true,"cloudPositionPreserved":true,"timeUploadedSeconds":20}' ;;
status) if [ -f "$script_root/authenticated" ]; then printf '%s\n' '{"state":"authenticated","accountName":"synthetic account"}'; else printf '%s\n' '{"state":"login_required"}'; fi ;;
logout) rm -f "$script_root/authenticated"; printf '%s\n' '{"state":"logged_out"}' ;;
login)
 printf '%s\n' '{"state":"qr","url":"https://weread.qq.com/web/confirm?uid=example"}'
 if [ -f "$script_root/slow" ]; then /bin/sleep 3; fi
 touch "$script_root/authenticated"
 printf '%s\n' '{"state":"authenticated","accountName":"synthetic account"}' ;;
shelf|cached-shelf)
 state1=remote; state2=remote; state3=remote
 if [ -f "$script_root/imports" ]; then
  grep -qx synthetic-1 "$script_root/imports" && state1=imported
  grep -qx synthetic-2 "$script_root/imports" && state2=imported
  grep -qx synthetic-3 "$script_root/imports" && state3=imported
 fi
 if [ -f "$script_root/queue-fixture" ]; then
  printf '%s\n' '{"state":"shelf","accountScope":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","books":[{"bookId":"synthetic-1","title":"queue one","importState":"'"$state1"'"},{"bookId":"synthetic-2","title":"queue two","importState":"'"$state2"'"},{"bookId":"synthetic-3","title":"queue three","importState":"'"$state3"'"}]}'
  exit 0
 fi
 if [ -f "$script_root/cache-fixture" ]; then
  printf '%s\n' '{"state":"shelf","accountScope":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","books":[{"bookId":"synthetic-1","title":"synthetic removed","author":"synthetic author","importState":"removed","hasDownloadCache":true},{"bookId":"synthetic-2","title":"synthetic imported","author":"synthetic author","importState":"imported"}]}'
  exit 0
 fi
 if [ -f "$script_root/network-failed" ]; then rm "$script_root/network-failed"; printf '%s\n' '{"state":"error","code":"helper_failed"}'; exit 1; fi
 if [ -f "$script_root/expired" ]; then rm "$script_root/expired"; printf '%s\n' '{"state":"error","code":"login_required"}'; exit 1; fi
 printf '%s\n' '{"state":"shelf","accountScope":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","books":[{"bookId":"synthetic-1","title":"合成测试书：正文与插图","author":"测试作者","cover":"https://res.weread.qq.com/synthetic-cover.png","localCover":"","importState":"'"$state1"'"},{"bookId":"synthetic-2","title":"已导入的合成测试书","author":"测试作者","localCover":"","importState":"imported"}]}' ;;
reading-progress) if [ -f "$script_root/manual-restore-fixture" ]; then printf '%s\n' '{"state":"reading_progress","bookId":"synthetic-2","accountScope":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","localProgress":12.5,"cloudProgress":67.8,"restorePending":true,"restoreAutomatic":false}'; exit 0; fi; if [ -f "$script_root/restore-fixture" ]; then printf '%s\n' '{"state":"reading_progress","bookId":"synthetic-2","accountScope":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","localProgress":0,"cloudProgress":67.8,"restorePending":true,"restoreAutomatic":true}'; exit 0; fi; printf '%s\n' '{"state":"reading_progress","bookId":"synthetic-2","accountScope":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","localProgress":12.5,"cloudProgress":67.8,"cloudProgressStale":false,"localChapter":"Local chapter","cloudChapter":"Cloud chapter"}'; printf '%s\n' '{"state":"reading_progress","bookId":"synthetic-2","accountScope":"wrong","cloudProgress":99}' ;;
sync-progress) printf '%s\n' '{"state":"progress_synced","uuid":"12345678-1234-1234-1234-123456789abc","recentMarked":true,"bookId":"synthetic-2","accountScope":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","cloudProgress":12.5,"cloudProgressStale":false,"localChapter":"Local chapter","cloudChapter":"Cloud chapter"}' ;;
restore-progress)
 printf '%s\n' '{"state":"restore_position","bookId":"synthetic-2","uuid":"12345678-1234-1234-1234-123456789abc","page":1}'
 read ack
 test "$ack" = ok || exit 1
 rm -f "$script_root/restore-fixture"
 printf '%s\n' '{"state":"progress_restored","bookId":"synthetic-2","accountScope":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","localProgress":67.5,"cloudProgress":67.8,"restorePending":false}' ;;
clear-cache) printf '%s\n' '{"state":"cache_cleared"}' ;;
upload-time)
 printf '%s\n' '{"state":"time_upload_progress","bookId":"synthetic-2","accountScope":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","uploadedSeconds":60,"totalSeconds":125,"remainingSeconds":90}'
 sleep 0.2
 printf '%s\n' '{"version":1,"books":{"synthetic-2":{"confirmedSeconds":125,"confirmedDays":{"'"$(date +%Y-%m-%d)"'":125}}}}' > "$script_root/covers/accounts/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/uploaded-reading-time-v1.json"
 printf '%s\n' '{"state":"time_uploaded","bookId":"synthetic-2","uploadedSeconds":125,"remainingSeconds":25,"deferredSeconds":25}' ;;
import)
 printf '%s\n' "$2" >> "$script_root/imports"
 printf '%s\n' '{"state":"progress","done":5,"total":10}'
 /bin/sleep 0.1
 printf '%s\n' '{"state":"progress","done":8,"total":10}'
 /bin/sleep 0.1
 printf '%s\n' '{"state":"imported","uuid":"12345678-1234-1234-1234-123456789abc"}' ;;
esac
if [ "$1" = shelf ] && [ -f "$script_root/slow-shelf" ]; then /bin/sleep 0.5; fi
)SH");
        file.close(); check(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QProcess probe; probe.start(program, QStringList{"status"});
        check(probe.waitForFinished(5000));
        if (probe.exitCode() != 0) std::cerr << probe.readAllStandardError().toStdString();
        check(probe.exitCode() == 0);
        QQmlEngine engine;
        auto *image = new LoginImage;
        engine.addImageProvider("weread-downloader-qr", image);
        const auto coverRoot = temporary.path() + "/covers";
        const auto coverDirectory = coverRoot + "/accounts/" + QString(64, 'a') + "/covers-v2";
        check(QDir().mkpath(coverDirectory));
        const QString coverUrl = "https://res.weread.qq.com/synthetic-cover.png";
        const auto cachedCover = coverDirectory + "/synthetic-1-" + QString::fromLatin1(QCryptographicHash::hash(coverUrl.toUtf8(), QCryptographicHash::Sha256).toHex()) + ".png";
        QImage cover(4, 6, QImage::Format_RGB32); cover.fill(Qt::black); check(cover.save(cachedCover));
        controller = new DownloaderController(image, &engine, program, coverRoot);
        phase="imported-only offline reading clock";
        {QFile registry(coverRoot+"/imports.json");check(registry.open(QIODevice::WriteOnly));registry.write(R"({"books":{"synthetic-2":{"state":"imported","accountScope":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","uuid":"12345678-1234-1234-1234-123456789abc"}}})");}
        controller->readingStarted("{12345678-1234-1234-1234-123456789ABC}");
        QTest::qWait(1100);controller->setReadingApplicationActive(false);controller->readingStopped();
        {QFile ledger(coverRoot+"/accounts/"+QString(64,'a')+"/local-reading-time-v1.json");check(ledger.open(QIODevice::ReadOnly));
         const auto data=QJsonDocument::fromJson(ledger.readAll()).object().value("books").toObject();check(data.size()==1 && data.value("synthetic-2").toObject().value("seconds").toInt()>=1);}
        controller->readingStarted("not-an-imported-document");controller->readingStopped();
        phase="setup";
        int recentRequests=0;
        QObject::connect(controller,&DownloaderController::recentReadRequested,[&](const QString &uuid){
            check(uuid=="12345678-1234-1234-1234-123456789abc");++recentRequests;
        });
        bool firstShelfCover = false, sawShelf = false;
        QObject::connect(controller, &DownloaderController::booksChanged, [&] {
            if (!sawShelf && controller->books().size() == 2) {
                sawShelf = true;
                firstShelfCover = !controller->coverSource("synthetic-1", coverUrl).isEmpty();
            }
        });
        qmlRegisterSingletonType<DownloaderController>("org.reweread.Downloader", 1, 0, "Downloader",
            [](QQmlEngine *, QJSEngine *) -> QObject * { QQmlEngine::setObjectOwnership(controller, QQmlEngine::CppOwnership); return controller; });
        bool sawQr = false;
        QObject::connect(controller, &DownloaderController::changed, [&] { if (!controller->qrImage().isEmpty()) sawQr = true; });
        phase = "initial login";
        controller->openPage();
        until([] { return controller->loggedIn() && controller->books().size() == 2 && !controller->busy(); });
        check(sawQr && controller->qrImage().isEmpty());
        check(firstShelfCover && controller->accountName() == "synthetic account");
        auto callCount=[&](const QString &operation){QFile calls(temporary.path()+"/calls");
            if(!calls.open(QIODevice::ReadOnly))return 0;
            return int(QString::fromUtf8(calls.readAll()).split('\n').count(operation));
        };
        auto shelvesBeforeImport=callCount("shelf");
        phase = "import queue";
        controller->importBooks({"synthetic-1", "synthetic-1", "synthetic-2"});
        controller->importBooks({"synthetic-1"});
        until([&] { return !controller->busy() && callCount("shelf")==shelvesBeforeImport+1; });
        QFile imports(temporary.path() + "/imports"); check(imports.open(QIODevice::ReadOnly));
        check(imports.readAll() == "synthetic-1\n");
        check(controller->books()[0].toMap().value("importState") == "imported");
        check(recentRequests==1);
        phase = "live queue add and cancel";
        imports.close(); check(QFile::remove(temporary.path() + "/imports"));
        QFile queueFixture(temporary.path() + "/queue-fixture"); check(queueFixture.open(QIODevice::WriteOnly)); queueFixture.close();
        controller->refresh(); until([] { return !controller->busy() && controller->books().size() == 3; });
        shelvesBeforeImport=callCount("shelf");
        controller->importBooks({"synthetic-1"}); check(controller->busy() && controller->canQueue());
        controller->importBooks({"synthetic-2"}); controller->importBooks({"synthetic-3", "synthetic-3"});
        check(controller->books()[1].toMap().value("importState") == "queued");
        check(controller->books()[2].toMap().value("importState") == "queued");
        controller->cancelQueued("synthetic-2"); check(controller->books()[1].toMap().value("importState") == "remote");
        controller->cancelQueued("synthetic-1"); // Running work cannot be canceled as a queued item.
        controller->importBooks({"synthetic-2"});
        until([&] {
            if (controller->busy() || callCount("shelf")!=shelvesBeforeImport+1) return false;
            for (const auto &row : controller->books()) if (row.toMap().value("importState") != "imported") return false;
            return true;
        });
        QFile queuedImports(temporary.path() + "/imports"); check(queuedImports.open(QIODevice::ReadOnly));
        check(queuedImports.readAll() == "synthetic-1\nsynthetic-3\nsynthetic-2\n");
        check(QFile::remove(queueFixture.fileName()));
        check(QFile::remove(temporary.path()+"/imports"));
        phase = "cancel restart";
        QFile slow(temporary.path() + "/slow"); check(slow.open(QIODevice::WriteOnly)); slow.close();
        controller->login(); until([] { return controller->busy(); });
        controller->cancel(); until([] { return !controller->busy(); });
        controller->login();
        until([] { return controller->loggedIn() && controller->books().size() == 2 && !controller->busy(); });
        // A delayed kill belonging to the canceled process must not kill this login.
        phase = "sidebar order";
        QQmlComponent layout(&engine);
        layout.setData(R"QML(import QtQuick 2.15
import QtQuick.Layouts 1.15
ColumnLayout {
 width: 200; height: 200; spacing: 0
 Rectangle { objectName: "new"; Layout.preferredHeight: 40; Layout.fillWidth: true }
 Rectangle { objectName: "openAppLoad"; Layout.preferredHeight: 40; Layout.fillWidth: true }
 Rectangle { objectName: "other"; Layout.preferredHeight: 40; Layout.fillWidth: true }
})QML", QUrl());
        auto *column = qobject_cast<QQuickItem *>(layout.create()); check(column);
        auto *newItem = column->findChild<QQuickItem *>("new");
        auto *appLoad = column->findChild<QQuickItem *>("openAppLoad");
        check(newItem && appLoad);
        controller->placeBelowAppLoad(newItem);
        until([&] { return column->childItems().indexOf(newItem) == column->childItems().indexOf(appLoad) + 1; });
        delete column;
        phase = "rendering";
        QQmlComponent page(&engine, QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])));
        if (page.isError()) std::cerr << page.errorString().toStdString();
        auto *item = qobject_cast<QQuickItem *>(page.create()); check(item);
        QQuickWindow window; window.resize(954, 1696);
        item->setParentItem(window.contentItem()); item->setWidth(954); item->setHeight(1696);
        window.show();
        until([&] { auto *button = item->findChild<QQuickItem *>("refreshButton"); return button && button->width() >= 200 && button->height() >= 72; });
        for (const auto *name : {"refreshButton"}) {
            auto *button = item->findChild<QQuickItem *>(name); check(button);
            check(button->width() >= 200 && button->height() >= 72);
        }

        check(!visualChild(item, "loginButton"));
        check(!visualChild(item, "selectionModeButton"));
        check(!visualChild(item, "cancelButton")->isVisible());
        window.show();
        until([&] { return visualChild(item, "bookSelection") != nullptr; });
        auto *coverImage = visualChild(item, "bookCoverImage"); check(coverImage);
        until([&] { return coverImage->property("status").toInt() == 1; });
        phase="reading statistics tab";
        const auto testZone=controller->readingTimeZones().value(1);
        check(controller->readingTimeZones().size()==27 && controller->readingTimeZoneLabels().size()==27);
        check(controller->readingTimeZoneLabels().contains(QStringLiteral("美国 · 檀香山 (UTC-10:00)")));
        check(controller->readingTimeZoneLabels().contains(QStringLiteral("美国 · 凤凰城 (UTC-07:00)")));
        check(controller->readingTimeZoneLabels().contains(QStringLiteral("美国 · 丹佛 (UTC-06:00)")));
        check(controller->readingTimeZoneLabels().contains(QStringLiteral("美国 · 芝加哥 (UTC-05:00)")));
        check(controller->readingTimeZoneLabels().contains(QStringLiteral("美国 · 纽约 (UTC-04:00)")));
        for(const auto &zone:controller->readingTimeZones())check(QTimeZone(zone.toUtf8()).isValid());
        check(controller->readingTimeZone()=="UTC" && !testZone.isEmpty());
        controller->setReadingTimeZone(testZone);check(controller->readingTimeZone()==testZone);
        check(controller->readingClock().size()==16);
        controller->setReadingTimeZone("UTC");
        check(visualChild(item,"readingTimeZoneSelector"));
        check(visualChild(item,"readingClockLabel"));
        const auto timeDir=coverRoot+"/accounts/"+QString(64,'a');
        const auto readingToday=QDateTime::currentDateTimeUtc().date().toString(Qt::ISODate);
        {QFile stats(timeDir+"/local-reading-time-v1.json");check(stats.open(QIODevice::WriteOnly));stats.write(QJsonDocument(QJsonObject{{"version",1},{"books",QJsonObject{{"synthetic-2",QJsonObject{{"seconds",150},{"lastRead",100},{"title","Synthetic timed book"},{"uuid","12345678-1234-1234-1234-123456789abc"},{"days",QJsonObject{{readingToday,125},{"2000-01-01",20}}}}}}}}).toJson());}
        {QFile stats(timeDir+"/reading-progress-v1.json");check(stats.open(QIODevice::WriteOnly));stats.write(R"({"version":1,"books":{"synthetic-2":{"record":{"readingTime":3600,"updateTime":100}}}})");}
        {QFile stats(timeDir+"/cloud-reading-time-v1.json");check(stats.open(QIODevice::WriteOnly));stats.write(QJsonDocument(QJsonObject{{"version",1},{"accountScope",QString(64,'a')},{"totalSeconds",86400},{"timeZone","UTC"},{"days",QJsonObject{{readingToday,240},{"2000-01-01",42}}}}).toJson());}
        QMetaObject::invokeMethod(controller,"readingStatsChanged");
        check(QMetaObject::invokeMethod(visualChild(item,"readingTimeTabButton"),"clicked"));
        until([&]{return item->property("activeTab").toInt()==1 && visualChild(item,"readingTimeList")->isVisible();});
        check(controller->readingStats().size()==1);
        check(controller->readingWeekStats().size()==7 && controller->readingWeekTotalSeconds()==240);
        check(controller->readingWeekMaximumSeconds()==240 && controller->readingWeekPeakLabel()!="暂无阅读");
        check(controller->cloudReadingTotalSeconds()==86400 && controller->readingDateTotalSeconds()==240);
        check(controller->readingStats()[0].toMap().value("pendingSeconds").toInt()==150);
        check(controller->readingStats()[0].toMap().value("canUploadTime").toBool());
        check(controller->readingStats()[0].toMap().value("localSeconds").toInt()==125);
        controller->setReadingDate("2000-01-01");check(controller->readingDateTotalSeconds()==42);
        check(controller->readingStats().size()==1 && controller->readingStats()[0].toMap().value("localSeconds").toInt()==20);
        controller->setReadingDate("1999-12-31");check(controller->readingStats().isEmpty());
        controller->setReadingDate("");
        check(controller->readingStats().size()==1 && controller->readingStats()[0].toMap().value("localSeconds").toInt()==150);
        check(QMetaObject::invokeMethod(visualChild(item,"nextReadingDate"),"clicked"));
        check(controller->readingDate()==QDate::fromString(readingToday,Qt::ISODate).addDays(1).toString(Qt::ISODate));
        check(QMetaObject::invokeMethod(visualChild(item,"previousReadingDate"),"clicked"));check(controller->readingDate()==readingToday);
        const auto currentWeek=controller->readingWeekLabel();controller->shiftReadingWeek(-1);check(controller->readingWeekLabel()!=currentWeek);
        controller->shiftReadingWeek(1);
        until([&]{return visualChild(item,"uploadReadingTimeButton")!=nullptr;});
        check(visualChild(item,"statisticsApiButton"));
        check(QMetaObject::invokeMethod(visualChild(item,"statisticsApiButton"),"clicked"));
        auto *apiDialog=item->findChild<QObject *>("statisticsApiDialog");
        check(apiDialog);until([&]{return apiDialog->property("visible").toBool();});
        auto *apiContent=visualChild(item,"statisticsApiContent");check(apiContent);
        auto *apiCancel=visualChild(apiContent,"statisticsApiCancel");
        auto *apiSave=visualChild(apiContent,"statisticsApiSave");check(apiCancel && apiSave);
        check(qAbs(apiCancel->width()-apiSave->width())<1 && qAbs(apiCancel->height()-88)<1);
        check(apiCancel->mapToItem(apiContent,QPointF(0,apiCancel->height())).y()<=apiContent->height()+1);
        auto *apiPanel=visualChild(item,"statisticsApiPanel");check(apiPanel);
        check(qAbs(apiPanel->height()-apiContent->height()-80)<1);
        auto *apiInput=item->findChild<QQuickItem *>("statisticsApiInput");check(apiInput);
        auto *revealButton=visualChild(item,"statisticsApiReveal");check(revealButton);
        apiInput->setProperty("text","synthetic-display-check");
        check(apiInput->property("echoMode").toInt()==2);
        check(QMetaObject::invokeMethod(revealButton,"clicked"));QCoreApplication::processEvents();
        check(apiInput->property("echoMode").toInt()==0 && apiInput->property("text").toString()=="synthetic-display-check");
        check(QMetaObject::invokeMethod(revealButton,"clicked"));QCoreApplication::processEvents();
        check(apiInput->property("echoMode").toInt()==2);
        apiInput->forceActiveFocus();QCoreApplication::processEvents();
        check(apiDialog->property("visible").toBool() && controller->opened());
        // Match the stock keyboard's scene stacking; taps must bypass our shade.
        QQmlComponent keyboardProbe(&engine);
        keyboardProbe.setData("import QtQuick 2.15; Rectangle { width: 160; height: 80; z: 10000; property int taps: 0; MouseArea { anchors.fill: parent; onClicked: parent.taps++ } }",QUrl());
        QScopedPointer<QObject> keyboardObject(keyboardProbe.create());
        auto *keyboardItem=qobject_cast<QQuickItem *>(keyboardObject.data());check(keyboardItem);
        keyboardItem->setParentItem(window.contentItem());keyboardItem->setY(window.height()-100);
        QTest::mouseClick(&window,Qt::LeftButton,Qt::NoModifier,QPoint(40,window.height()-60));
        QCoreApplication::processEvents();check(keyboardItem->property("taps").toInt()==1);
        check(apiDialog->property("visible").toBool());
        keyboardItem->setParentItem(nullptr);

        check(QMetaObject::invokeMethod(apiDialog,"reject"));
        check(apiInput->property("text").toString().isEmpty() && !apiDialog->property("revealKey").toBool());

        for(const auto *name:{"previousReadingWeek","nextReadingWeek","previousReadingDate","nextReadingDate"}) {
            auto *button=visualChild(item,name);check(button && qAbs(button->height()-58)<1);
        }
        auto *zoneSelector=visualChild(item,"readingTimeZoneSelector");check(zoneSelector && qAbs(zoneSelector->height()-58)<1);
        controller->uploadReadingTime("synthetic-2");
        until([&]{return controller->timeUploadDone()==60 && controller->timeUploadTotal()==125;});
        check(controller->timeUploadActive() && controller->status().contains("48%"));
        check(visualChild(item,"timeUploadProgressBar")->isVisible());
        until([&]{return callCount("upload-time")==1 && !controller->busy();});
        check(controller->readingStats()[0].toMap().value("pendingSeconds").toInt()==25);
        auto writeReceipt=[&](const QJsonObject &receipt){QFile f(timeDir+"/uploaded-reading-time-v1.json");check(f.open(QIODevice::WriteOnly));f.write(QJsonDocument(QJsonObject{{"version",1},{"books",QJsonObject{{"synthetic-2",receipt}}}}).toJson());f.close();QMetaObject::invokeMethod(controller,"readingStatsChanged");};
        writeReceipt(QJsonObject{{"confirmedSeconds",150}});
        until([&]{auto *b=visualChild(item,"uploadReadingTimeButton");return b && !b->isEnabled();});
        check(controller->readingStats()[0].toMap().value("pendingSeconds").toInt()==0);
        controller->uploadReadingTime("synthetic-2");check(callCount("upload-time")==1);
        writeReceipt(QJsonObject{{"confirmedSeconds",125},{"pending",QJsonObject{{"seconds",25}}}});
        check(controller->readingStats()[0].toMap().value("timeUploadUncertain").toBool());
        check(!controller->readingStats()[0].toMap().value("canUploadTime").toBool());
        writeReceipt(QJsonObject{{"confirmedSeconds",125}});
        until([&]{auto *b=visualChild(item,"uploadReadingTimeButton");return b && b->isEnabled();});
        check(!controller->statisticsConfigured());
        controller->setStatisticsApiKey("synthetic-independent-statistics-key");
        until([&]{return controller->statisticsConfigured() && controller->statisticsStatus()==QStringLiteral("云端统计已更新");});
        check(QMetaObject::invokeMethod(visualChild(item,"statisticsApiButton"),"clicked"));
        check(apiInput->property("text").toString()=="synthetic-independent-statistics-key");
        check(apiInput->property("echoMode").toInt()==2);
        check(QMetaObject::invokeMethod(apiDialog,"reject"));
        QFile privateConfig(timeDir+"/statistics-api-v1.json");check(privateConfig.open(QIODevice::ReadOnly));
        const auto config=QJsonDocument::fromJson(privateConfig.readAll()).object();
        check(config.value("apiKey")=="synthetic-independent-statistics-key" && config.value("accountScope")==QString(64,'a'));
        check(!(privateConfig.permissions()&(QFileDevice::ReadGroup|QFileDevice::WriteGroup|QFileDevice::ReadOther|QFileDevice::WriteOther)));
        check(controller->cloudReadingTotalSeconds()==86400 && callCount("upload-time")==1);
        auto timeScreenshot=window.grabWindow();if(!timeScreenshot.isNull())check(timeScreenshot.save(QString::fromLocal8Bit(argv[2])+".time.png"));
        check(QMetaObject::invokeMethod(visualChild(item,"shelfTabButton"),"clicked"));
        phase="rendering";
        auto *selection = visualChild(item, "bookSelection");
        auto *importButton = visualChild(item, "bookImportButton");
        check(!item->property("multiSelect").toBool() && !selection->isVisible() && importButton->isVisible());
        auto *shelfItem = visualChild(item, "pagedShelf"); check(shelfItem);
        auto *touch = QTest::createTouchDevice();
        const auto longPressPoint = shelfItem->mapToScene(QPointF(400, 80)).toPoint();
        auto held = QTest::touchEvent(&window, touch, false);
        held.press(0, longPressPoint, &window).commit(); QTest::qWait(750);
        held.release(0, longPressPoint, &window).commit();
        until([&] { return selection->isVisible() && selection->width() >= 88; });
        auto *toolbarImportButton = visualChild(item, "importButton");
        check(item->property("multiSelect").toBool() && selection->isVisible() && toolbarImportButton->isVisible() && !importButton->isVisible());
        check(selection->width() >= 88 && selection->height() >= 112);
        check(importButton && importButton->width() >= 144 && importButton->height() >= 72);
        auto *status = visualChild(item, "bookImportStatus");
        check(status && status->opacity() == 0.5);
        auto *greenCheck = visualChild(item, "bookImportedCheck");
        check(greenCheck && greenCheck->property("color").value<QColor>() == QColor("#239B56"));
        check(greenCheck->width() == 44 && greenCheck->height() == 44);
        check(greenCheck->property("radius").toReal() == 22);
        item->setHeight(540); window.resize(954, 540);
        until([&] { return item->property("pageSize").toInt() == 1 && item->property("pageCount").toInt() == 2; });
        check(item->property("selectedIds").value<QJSValue>().toVariant().toList().contains("synthetic-1"));
        const auto from = shelfItem->mapToScene(QPointF(650, 80));
        auto sequence = QTest::touchEvent(&window, touch, false);
        sequence.press(0, from.toPoint(), &window).commit();
        for (int step = 1; step <= 6; ++step) {
            sequence.move(0, (from - QPointF(step * 30, 0)).toPoint(), &window).commit();
            QCoreApplication::processEvents();
        }
        sequence.release(0, (from - QPointF(180, 0)).toPoint(), &window).commit();
        QCoreApplication::processEvents();
        check(item->property("pageIndex").toInt() == 1);
        check(item->property("selectedIds").value<QJSValue>().toVariant().toList().contains("synthetic-1"));
        QMetaObject::invokeMethod(item, "turnPage", Q_ARG(QVariant, QVariant(1)));
        check(item->property("pageIndex").toInt() == 1);
        QMetaObject::invokeMethod(item, "turnPage", Q_ARG(QVariant, QVariant(-1)));
        check(item->property("pageIndex").toInt() == 0);
        auto swipe = [&](const QPoint &delta) {
            QTest::qWait(50);
            auto gesture = QTest::touchEvent(&window, touch, false);
            gesture.press(0, from.toPoint(), &window).commit();
            for (int step = 1; step <= 6; ++step) {
                gesture.move(0, from.toPoint() + delta * step / 6, &window).commit();
                QCoreApplication::processEvents();
            }
            gesture.release(0, from.toPoint() + delta, &window).commit();
            QTest::qWait(50);
        };
        swipe(QPoint(180,0)); check(item->property("pageIndex").toInt() == 0);
        swipe(QPoint(-40,0)); check(item->property("pageIndex").toInt() == 0);
        swipe(QPoint(0,100)); check(item->property("pageIndex").toInt() == 0);
        swipe(QPoint(-180,0)); check(item->property("pageIndex").toInt() == 1);
        swipe(QPoint(180,0)); check(item->property("pageIndex").toInt() == 0);
        check(QMetaObject::invokeMethod(visualChild(item, "exitMultiSelectButton"), "clicked")); QCoreApplication::processEvents();
        check(controller->opened() && !item->property("multiSelect").toBool());
        check(item->property("selectedIds").value<QJSValue>().toVariant().toList().isEmpty());
        check(!visualChild(item, "bookSelection")->isVisible() && !visualChild(item, "importButton")->isVisible());
        QElapsedTimer paint; paint.start(); until([&] { return paint.elapsed() > 250; });
        auto screenshot = window.grabWindow();
        if (!screenshot.isNull()) check(screenshot.save(QString::fromLocal8Bit(argv[2])));
        else std::cout << "headless platform supplied no screenshot; visual QA remains separate\n";
        phase = "reading progress and sync";
        QMetaObject::invokeMethod(item, "turnPage", Q_ARG(QVariant, QVariant(1)));
        until([&] { auto *button=visualChild(item,"uploadProgressButton"); return button && button->isVisible() && button->width()==144; });
        auto *syncButton=visualChild(item,"uploadProgressButton"); check(syncButton->height()==56);
        auto *readingLabel=visualChild(item,"bookReadingProgress");
        auto *cloudLabel=visualChild(item,"cloudReadingProgress");
        until([&] { return cloudLabel->property("text").toString().contains("67.8%"); });
        check(readingLabel->property("text").toString().contains("12.5%"));
        check(readingLabel->property("text").toString().contains("Local chapter"));
        check(cloudLabel->property("text").toString().contains("Cloud chapter"));
        check(cloudLabel->mapToScene(QPointF(0,cloudLabel->height())).y()
            < syncButton->mapToScene(QPointF()).y());
        check(qAbs(readingLabel->mapToScene(QPointF()).x()-syncButton->mapToScene(QPointF()).x())<2);
        auto progressScreenshot=window.grabWindow();
        if(!progressScreenshot.isNull()) check(progressScreenshot.save(QString::fromLocal8Bit(argv[2])+".progress.png"));
        phase = "sync action survives shelf loading while busy";
        QFile slowShelf(temporary.path()+"/slow-shelf");check(slowShelf.open(QIODevice::WriteOnly));slowShelf.close();
        controller->refresh();
        until([&]{auto *button=visualChild(item,"uploadProgressButton");return controller->busy() && button && button->isVisible() && !button->isEnabled();});
        until([]{return !controller->busy();});
        syncButton=visualChild(item,"uploadProgressButton");
        check(syncButton && syncButton->isVisible() && syncButton->isEnabled());
        check(QFile::remove(slowShelf.fileName()));
        const auto recentBeforeUpload=recentRequests;
        const auto shelvesBeforeUpload=callCount("shelf");
        check(QMetaObject::invokeMethod(syncButton,"clicked"));
        until([&] { return !controller->busy() && callCount("shelf")==shelvesBeforeUpload+1; });
        check(recentRequests==recentBeforeUpload+1);
        QMetaObject::invokeMethod(item, "turnPage", Q_ARG(QVariant, QVariant(-1)));
        phase = "automatic restore handshake";
        bool restored=false;
        const auto restoreConnection=QObject::connect(controller,&DownloaderController::restoreRequested,[&](const QString &uuid,int page,const QString &id){
            check(uuid=="12345678-1234-1234-1234-123456789abc" && page==1 && id=="synthetic-2");
            restored=true;controller->completeRestore(id,true);
        });
        phase = "explicit download progress";
        QMetaObject::invokeMethod(item, "turnPage", Q_ARG(QVariant, QVariant(1)));
        auto *downloadButton=visualChild(item,"downloadProgressButton");
        until([&]{return downloadButton && downloadButton->isVisible() && downloadButton->width()==144;});
        check(downloadButton->height()==56 && downloadButton->property("text")==QStringLiteral("下载进度"));
        check(QMetaObject::invokeMethod(downloadButton,"clicked"));
        until([&]{return restored && !controller->busy();});
        restored=false;
        QMetaObject::invokeMethod(item, "turnPage", Q_ARG(QVariant, QVariant(-1)));
        phase = "manual restore failure never auto retries on refresh";
        QFile manualRestoreFixture(temporary.path()+"/manual-restore-fixture");check(manualRestoreFixture.open(QIODevice::WriteOnly));manualRestoreFixture.close();
        controller->refresh();until([]{return !controller->busy();});QTest::qWait(300);
        check(!restored && !controller->busy());
        check(QFile::remove(manualRestoreFixture.fileName()));
        QFile restoreFixture(temporary.path()+"/restore-fixture");check(restoreFixture.open(QIODevice::WriteOnly));restoreFixture.close();
        controller->refresh();
        until([&]{return restored && !controller->busy() && controller->status().contains(QStringLiteral("已恢复"));});
        check(!QFile::exists(restoreFixture.fileName()));
        QObject::disconnect(restoreConnection);
        phase = "manual delete-cache action remains";
        QFile cacheFixture(temporary.path() + "/cache-fixture"); check(cacheFixture.open(QIODevice::WriteOnly)); cacheFixture.close();
        controller->refresh(); until([] { return !controller->busy(); });
        auto *cacheButton = visualChild(item, "deleteCacheButton");
        until([&] { return cacheButton && cacheButton->isVisible() && cacheButton->width() > 0; });
        check(cacheButton->property("text") == QStringLiteral("删除缓存"));
        until([&]{return cacheButton->width() == 144 && cacheButton->height() == 88;});
        check(QMetaObject::invokeMethod(cacheButton, "clicked")); until([] { return !controller->busy(); });
        check(!controller->books()[0].toMap().value("hasDownloadCache").toBool());
        check(controller->books()[0].toMap().value("importState") == "removed");
        check(!cacheButton->isVisible());
        check(QFile::remove(cacheFixture.fileName())); controller->refresh(); until([] { return !controller->busy(); });
        phase = "stable rows during download";
        int resets = 0; bool progressTitle = false;
        auto resetConnection = QObject::connect(controller, &DownloaderController::booksChanged, [&] { ++resets; });
        auto progressConnection = QObject::connect(controller, &DownloaderController::changed, [&] {
            if (controller->status().contains(QStringLiteral("5 / 10"))) progressTitle = controller->status().contains(QStringLiteral("合成测试书：正文与插图"));
        });
        auto *stableBadge = visualChild(item, "bookImportedCheck");
        controller->importBooks({"synthetic-1"});
        until([] { return !controller->busy(); });
        check(progressTitle && resets == 0 && stableBadge == visualChild(item, "bookImportedCheck"));
        QObject::disconnect(resetConnection); QObject::disconnect(progressConnection);
        phase = "account logout confirmation";
        auto *accountButton = visualChild(item, "accountButton"); check(accountButton);
        check(QMetaObject::invokeMethod(accountButton, "clicked"));
        auto *dialog = item->findChild<QObject *>("logoutDialog"); check(dialog && dialog->property("visible").toBool());
        check(QMetaObject::invokeMethod(dialog, "reject")); check(controller->loggedIn() && !controller->busy());
        bool loggedOut = false;
        const auto logoutConnection = QObject::connect(controller, &DownloaderController::changed, [&] { if (!controller->loggedIn()) loggedOut = true; });
        check(QMetaObject::invokeMethod(accountButton, "clicked")); check(QMetaObject::invokeMethod(dialog, "accept"));
        until([&] { return loggedOut && controller->loggedIn() && !controller->busy(); });
        QObject::disconnect(logoutConnection);
        controller->closePage(); check(!controller->opened() && controller->qrImage().isEmpty());
        controller->openPage();
        until([] { return !controller->busy() && controller->books().size() == 2; });
        QElapsedTimer cacheWait; cacheWait.start(); until([&] { return cacheWait.elapsed() > 100; });
        QFile calls(temporary.path() + "/calls"); check(calls.open(QIODevice::ReadOnly));
        auto history=calls.readAll(); history.replace("reading-progress\n", ""); history.replace("cloud-time\n", ""); check(history.endsWith("status\ncached-shelf\n"));
        QFile networkFailed(temporary.path() + "/network-failed"); check(networkFailed.open(QIODevice::WriteOnly)); networkFailed.close();
        controller->refresh(); until([] { return !controller->busy(); });
        check(controller->loggedIn() && controller->books().size() == 2);
        QFile expired(temporary.path() + "/expired"); check(expired.open(QIODevice::WriteOnly)); expired.close();
        bool returnedToLogin = false;
        QObject::connect(controller, &DownloaderController::changed, [&] { if (!controller->loggedIn()) returnedToLogin = true; });
        controller->refresh();
        until([&] { return returnedToLogin && controller->loggedIn() && !controller->busy() && controller->books().size() == 2; });
        controller->closePage();
        phase="periodic reports, wifi, sleep and close gates";
        const QString entryUuid="12345678-1234-1234-1234-123456789abc";
        auto reportTimer=controller->findChild<QTimer*>("readingReportTimer");check(reportTimer);
        auto tick=[&]{reportTimer->stop();check(QMetaObject::invokeMethod(reportTimer,"timeout",Qt::DirectConnection));};
        controller->setReadingLocked(false);controller->setReadingApplicationActive(true);controller->setReadingDeviceSleeping(false);
        auto entriesBefore=callCount("reading-tick");controller->setReadingWifiConnected(false);controller->readingStarted(entryUuid,42);
        tick();QTest::qWait(200);check(callCount("reading-tick")==entriesBefore);
        check(controller->readingSessionStatus().contains(QStringLiteral("Wi-Fi")));
        check(controller->readingSessionStatus().contains(QStringLiteral("0分0秒")));
        controller->setReadingWifiConnected(true);check(reportTimer->isActive() && reportTimer->interval()==60000);
        QTest::qWait(200);check(callCount("reading-tick")==entriesBefore);
        {QFile fail(temporary.path()+"/entry-fail-once");check(fail.open(QIODevice::WriteOnly));}
        tick();until([&]{return callCount("reading-tick")==entriesBefore+1 && reportTimer->isActive();});
        check(reportTimer->interval()==60000);check(controller->readingSessionStatus().contains(QStringLiteral("0分0秒")));
        tick();until([&]{return callCount("reading-tick")==entriesBefore+2 && reportTimer->isActive();});
        check(controller->readingSessionStatus().contains(QStringLiteral("60")));
        check(controller->readingSessionStatus().contains(QStringLiteral("1分0秒")));
        controller->readingActivity();tick();until([&]{return callCount("reading-tick")==entriesBefore+3 && reportTimer->isActive();});
        controller->setReadingWifiConnected(false);tick();QTest::qWait(200);check(callCount("reading-tick")==entriesBefore+3);
        controller->setReadingDeviceSleeping(true);controller->setReadingWifiConnected(true);tick();QTest::qWait(200);
        check(callCount("reading-tick")==entriesBefore+3);
        controller->setReadingDeviceSleeping(false);check(reportTimer->isActive() && reportTimer->interval()==60000);
        tick();until([&]{return callCount("reading-tick")==entriesBefore+4 && reportTimer->isActive();});
        {QFile slowReport(temporary.path()+"/slow-report");check(slowReport.open(QIODevice::WriteOnly));}
        tick();until([&]{return callCount("reading-tick")==entriesBefore+5 && controller->busy();});
        controller->setReadingDeviceSleeping(true);until([&]{return !controller->busy();});
        check(!reportTimer->isActive());QFile::remove(temporary.path()+"/slow-report");
        controller->setReadingDeviceSleeping(false);check(reportTimer->isActive());
        controller->readingStopped();tick();QTest::qWait(200);check(callCount("reading-tick")==entriesBefore+5);
        until([&]{return controller->nativeBookBadge(entryUuid)=="weread" && !controller->busy();});
        phase="native pull confirmation and submitted duration";
        controller->readingStarted(entryUuid,1);tick();until([&]{return !controller->busy() && reportTimer->isActive();});
        check(controller->readingSessionStatus().contains(QStringLiteral("1分0秒")));
        check(!controller->readingSessionStatus().contains(QStringLiteral("计时")));
        check(controller->nativePullAvailable());const auto appliesBefore=callCount("apply-native-restore");
        controller->requestNativePull();until([&]{return controller->nativePullCanConfirm();});
        check(controller->nativePullVisible() && controller->nativePullMessage().contains("synthetic snapshot") && controller->nativePullMessage().contains("2 / 4"));
        QQmlComponent nativeDialogComponent(&engine,QUrl::fromLocalFile(QFileInfo(QString::fromLocal8Bit(argv[1])).absolutePath()+"/NativeProgressDialog.qml"));
        if(nativeDialogComponent.isError())std::cerr<<nativeDialogComponent.errorString().toStdString();
        auto *nativeDialog=qobject_cast<QQuickItem*>(nativeDialogComponent.create());check(nativeDialog);
        nativeDialog->setParentItem(window.contentItem());nativeDialog->setWidth(954);nativeDialog->setHeight(1696);nativeDialog->setZ(20000);window.resize(954,1696);
        auto *pullCancel=visualChild(nativeDialog,"nativePullCancel");auto *pullConfirm=visualChild(nativeDialog,"nativePullConfirm");check(pullCancel && pullConfirm);
        until([&]{return pullCancel->isVisible() && pullConfirm->isVisible() && pullCancel->width()>100 && pullCancel->height()==88 && pullConfirm->height()==88;});
        check(qAbs(pullCancel->width()-pullConfirm->width())<1);
        QTest::qWait(120);auto pullScreenshot=window.grabWindow();if(!pullScreenshot.isNull())check(pullScreenshot.save(QString::fromLocal8Bit(argv[2])+".native-pull.png"));
        check(callCount("apply-native-restore")==appliesBefore);
        QTest::mouseClick(&window,Qt::LeftButton,Qt::NoModifier,pullCancel->mapToScene(QPointF(pullCancel->width()/2,pullCancel->height()/2)).toPoint());
        check(!controller->nativePullVisible());
        check(callCount("apply-native-restore")==appliesBefore);
        controller->requestNativePull();until([&]{return controller->nativePullCanConfirm();});
        bool nativeJump=false;
        auto nativeConnection=QObject::connect(controller,&DownloaderController::nativePullRestoreRequested,[&](const QString &uuid,int page,const QString &id){
            check(uuid==entryUuid && page==2);nativeJump=true;controller->completeNativePullRestore(id,true);
        });
        QTest::mouseClick(&window,Qt::LeftButton,Qt::NoModifier,pullConfirm->mapToScene(QPointF(pullConfirm->width()/2,pullConfirm->height()/2)).toPoint());
        until([&]{return !controller->nativePullBusy() && nativeJump;});
        check(callCount("apply-native-restore")==appliesBefore+1 && controller->nativePullMessage().contains(QStringLiteral("已用")));
        check(controller->readingSessionStatus().contains(QStringLiteral("1分0秒")));
        controller->cancelNativePull();QObject::disconnect(nativeConnection);nativeDialog->setParentItem(nullptr);delete nativeDialog;
        controller->readingStopped();until([&]{return !controller->busy() && controller->nativeBookBadge(entryUuid)=="weread";});
        check(!controller->nativePullAvailable());
        phase="background exit upload and native book badges";
        const QString nativeUuid="12345678-1234-1234-1234-123456789abc";
        check(controller->nativeBookBadge("unknown").isEmpty());
        controller->readingStarted(nativeUuid);controller->readingStopped();
        check(controller->nativeBookBadge(nativeUuid)=="sync"); // Close is queued while inactive.
        controller->readingStarted(nativeUuid); // Reopening drops that pending close.
        check(controller->nativeBookBadge(nativeUuid)=="weread");
        const auto loginBeforeAuto=callCount("login");const auto autoBefore=callCount("exit-sync");
        controller->setReadingDeviceSleeping(true);controller->setReadingApplicationActive(true);controller->readingStopped(42);
        QTest::qWait(1600);check(callCount("exit-sync")==autoBefore && controller->nativeBookBadge(nativeUuid)=="sync");
        controller->setReadingDeviceSleeping(false);
        until([&]{return controller->busy();});check(controller->nativeBookBadge(nativeUuid)=="sync");
        until([&]{return !controller->busy() && controller->nativeBookBadge(nativeUuid)=="weread";});
        check(callCount("exit-sync")==autoBefore+1 && callCount("login")==loginBeforeAuto && !controller->opened());
        check(controller->status().contains(QStringLiteral("云端位置已保留")));
        {QFile args(temporary.path()+"/exit-args");check(args.open(QIODevice::ReadOnly));check(args.readAll().contains(("exit-sync synthetic-2 "+QString(64,'a')+" "+nativeUuid+" 42").toUtf8()));}
        controller->readingStopped();QTest::qWait(1600);check(callCount("exit-sync")==autoBefore+1); // Duplicate close is a no-op.
        {QFile reject(temporary.path()+"/exit-rejected");check(reject.open(QIODevice::WriteOnly));}
        controller->readingStarted(nativeUuid);controller->readingStopped();
        until([&]{return controller->nativeBookBadge(nativeUuid)=="error";});
        check(callCount("login")==loginBeforeAuto && !controller->opened()); // Auth failure never opens QR.
        controller->readingStarted(nativeUuid);controller->readingStopped();controller->readingStarted(nativeUuid);
        QTest::qWait(1600);check(!controller->busy()); // Cancel queued work if reopened.
        QFile::remove(temporary.path()+"/exit-rejected");
        phase="exit upload preempts whole-shelf progress refresh";
        const auto progressBeforePriority=callCount("reading-progress");const auto exitsBeforePriority=callCount("exit-sync");
        {QFile slow(temporary.path()+"/slow-progress");check(slow.open(QIODevice::WriteOnly));}
        controller->openPage();until([&]{return callCount("reading-progress")>progressBeforePriority;});
        QElapsedTimer priorityWait;priorityWait.start();controller->readingStopped(42);
        until([&]{return callCount("exit-sync")>exitsBeforePriority;},4000);check(priorityWait.elapsed()<3500);
        QFile::remove(temporary.path()+"/slow-progress");
        until([&]{return !controller->busy() && controller->nativeBookBadge(nativeUuid)=="weread";});
        controller->closePage();controller->readingStarted(nativeUuid);
        controller->setReadingApplicationActive(false);controller->readingStopped();
        {QFile registry(coverRoot+"/imports.json");check(registry.open(QIODevice::WriteOnly));registry.write("{\"books\":{}}");}
        controller->setReadingApplicationActive(true);QTest::qWait(1600);
        check(controller->nativeBookBadge(nativeUuid).isEmpty() && !controller->busy()); // Removed source drops queued upload.
        phase="passive native badge component";
        QQuickWindow badgeWindow;badgeWindow.setColor(Qt::white);badgeWindow.resize(420,160);badgeWindow.show();
        QQmlComponent badgeComponent(&engine,QUrl::fromLocalFile(QFileInfo(QString::fromLocal8Bit(argv[1])).absolutePath()+"/WeReadBadge.qml"));
        if(badgeComponent.isError())std::cerr<<badgeComponent.errorString().toStdString();
        auto *badge=qobject_cast<QQuickItem*>(badgeComponent.create());check(badge);
        badge->setParentItem(badgeWindow.contentItem());badge->setWidth(96);badge->setHeight(96);badge->setProperty("badgeStatus","weread");QCoreApplication::processEvents();check(badge->isVisible());
        badge->setProperty("badgeStatus","sync");check(badge->isVisible());
        badge->setProperty("badgeStatus","error");check(badge->isVisible());
        badge->setProperty("badgeStatus","");check(!badge->isVisible());delete badge;
        const QStringList badgeStates{"weread","sync","error"};
        for(int i=0;i<badgeStates.size();++i){auto *b=qobject_cast<QQuickItem*>(badgeComponent.create());check(b);
            b->setParentItem(badgeWindow.contentItem());b->setParent(&badgeWindow);b->setWidth(96);b->setHeight(96);b->setX(24+i*132);b->setY(32);b->setProperty("badgeStatus",badgeStates[i]);}
        badgeWindow.show();QTest::qWait(100);const auto badgeScreenshot=badgeWindow.grabWindow();
        if(!badgeScreenshot.isNull())check(badgeScreenshot.save(QString::fromLocal8Bit(argv[2])+".badges.png"));
        std::cout << "touch paging, cached startup, auth expiry, import queue and native QML instantiation ok\n";
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
