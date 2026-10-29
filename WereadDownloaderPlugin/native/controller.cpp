#include "controller.h"
#include "../src/exit_sync_state.h"
#include "qrcodegen.hpp"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>
#include <QUuid>
#include <algorithm>
#include <sys/stat.h>

namespace {
const QString root = QStringLiteral("/home/root/.local/share/weread-downloader");
const QString restorePath = root + QStringLiteral("/restore-web-disabled");
bool peerActive() {
    for (const auto &path : {"/run/rmtool-native-import-control/active", "/run/rmtool-qr-upload-ui",
                            "/home/root/.rmtool-qr-upload-web-state"}) if (QFileInfo::exists(path)) return true;
    return false;
}
bool importable(const QString &state) { return state == "remote" || state == "failed" || state == "removed"; }
QString dayInZone(const QString &zoneId, qint64 timestampMs=QDateTime::currentMSecsSinceEpoch()) {
    const QTimeZone zone(zoneId.toUtf8());
    return WereadDownloader::ReadingTimeStore::dayInTimeZone(timestampMs, !zoneId.isEmpty() && zone.isValid()?zone:QTimeZone::utc());
}
QJsonObject privateObject(const QString &path) {
    QFile file(path);
    if(QFileInfo(file).isSymLink() || !file.open(QIODevice::ReadOnly) || file.size()>8*1024*1024)return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
QJsonObject cloudTimeSnapshot(const QString &root,const QString &scope,const QString &zone) {
    auto snapshot=privateObject(root+"/accounts/"+scope+"/cloud-reading-time-v1.json");
    if(scope.isEmpty() || snapshot.value("accountScope")!=scope || snapshot.value("version")!=1)return {};
    if(snapshot.value("timeZone")!=zone)snapshot["days"]=QJsonObject();
    return snapshot;
}
}
QImage LoginImage::requestImage(const QString &, QSize *size, const QSize &) {
    if (size) *size = m_image.size();
    return m_image;
}
void LoginImage::update(const QString &url) {
    m_image = QImage();
    if (url.isEmpty()) return;
    const auto qr = qrcodegen::QrCode::encodeText(url.toUtf8().constData(), qrcodegen::QrCode::Ecc::MEDIUM);
    const int modules = qr.getSize(), scale = 6;
    m_image = QImage((modules + 8) * scale, (modules + 8) * scale, QImage::Format_RGB32);
    m_image.fill(Qt::white);
    QPainter paint(&m_image);
    for (int y = 0; y < modules; ++y) for (int x = 0; x < modules; ++x)
        if (qr.getModule(x, y)) paint.fillRect((x + 4) * scale, (y + 4) * scale, scale, scale, Qt::black);
}
DownloaderController::DownloaderController(LoginImage *image, QObject *parent, const QString &workerProgram, const QString &coverRoot)
    : QObject(parent), m_covers(this, coverRoot), m_image(image), m_workerProgram(workerProgram),
      m_dataRoot(coverRoot),m_readingTime(coverRoot) {
    m_status = QStringLiteral("微信读书下载");
    static const QStringList cities={
        QStringLiteral("美国 · 贝克岛"),QStringLiteral("美属萨摩亚 · 帕果帕果"),QStringLiteral("美国 · 檀香山"),
        QStringLiteral("美国 · 阿达克"),QStringLiteral("美国 · 安克雷奇"),QStringLiteral("美国 · 凤凰城"),
        QStringLiteral("美国 · 丹佛"),QStringLiteral("美国 · 芝加哥"),QStringLiteral("美国 · 纽约"),
        QStringLiteral("阿根廷 · 布宜诺斯艾利斯"),QStringLiteral("巴西 · 费尔南多·迪诺罗尼亚"),QStringLiteral("佛得角 · 普拉亚"),
        QStringLiteral("加纳 · 阿克拉"),QStringLiteral("阿尔及利亚 · 阿尔及尔"),QStringLiteral("南非 · 约翰内斯堡"),
        QStringLiteral("沙特阿拉伯 · 利雅得"),QStringLiteral("阿联酋 · 迪拜"),QStringLiteral("巴基斯坦 · 卡拉奇"),
        QStringLiteral("孟加拉国 · 达卡"),QStringLiteral("泰国 · 曼谷"),QStringLiteral("中国 · 上海"),
        QStringLiteral("日本 · 东京"),QStringLiteral("澳大利亚 · 布里斯班"),QStringLiteral("新喀里多尼亚 · 努美阿"),
        QStringLiteral("斐济 · 苏瓦"),QStringLiteral("汤加 · 努库阿洛法"),QStringLiteral("基里巴斯 · 基里蒂马蒂")
    };
    m_readingTimeZones.clear();m_readingTimeZoneLabels.clear();
    for(int offset=-12;offset<=14;++offset) {
        const auto zone=QTimeZone::fromSecondsAheadOfUtc(offset*3600);
        if(!zone.isValid())continue;
        const auto id=QString::fromUtf8(zone.id());
        const auto hours=qAbs(offset);
        const auto label=QStringLiteral("%1 (UTC%2%3%4)").arg(cities.at(offset+12),offset<0?QStringLiteral("-"):QStringLiteral("+"))
            .arg(hours,2,10,QChar('0')).arg(QStringLiteral(":00"));
        m_readingTimeZones.append(id);m_readingTimeZoneLabels.append(label);
    }
    const auto timeSettings=privateObject(m_dataRoot+"/reading-time-settings-v1.json");
    const auto savedZone=timeSettings.value("timeZone").toString();
    if(!savedZone.isEmpty() && m_readingTimeZones.contains(savedZone))m_readingTimeZoneId=savedZone;
    else if(!savedZone.isEmpty() && QTimeZone(savedZone.toUtf8()).isValid()) {
        const auto oldOffset=QTimeZone(savedZone.toUtf8()).offsetFromUtc(QDateTime::currentDateTimeUtc());
        const auto roundedOffset=qBound(-12,qRound(oldOffset/3600.0),14);
        m_readingTimeZoneId=QString::fromUtf8(QTimeZone::fromSecondsAheadOfUtc(roundedOffset*3600).id());
    }
    m_readingTime.setTimeZone(QTimeZone(m_readingTimeZoneId.toUtf8()));
    const auto localToday=QDate::fromString(dayInZone(m_readingTimeZoneId),Qt::ISODate);
    m_readingDate=localToday.toString(Qt::ISODate);
    m_readingWeekStart=localToday.addDays(1-localToday.dayOfWeek());
    m_exitClock.start();reloadNativeBooks();
    if(QFileInfo(m_dataRoot).isDir())m_nativeBookWatcher.addPath(m_dataRoot);
    connect(&m_nativeBookWatcher,&QFileSystemWatcher::directoryChanged,this,[this]{reloadNativeBooks();});
    connect(&m_autoSyncProcess,&QProcess::readyReadStandardOutput,this,&DownloaderController::readExitSync);
    connect(&m_autoSyncProcess,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,&DownloaderController::finishExitSync);
    connect(&m_autoSyncProcess,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        if(error==QProcess::FailedToStart)finishExitSync(-1,QProcess::CrashExit);
    });
    connect(&m_statsProcess,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus status){
        const auto lines=m_statsProcess.readAllStandardOutput().trimmed().split('\n');
        const auto result=lines.isEmpty()?QJsonObject():QJsonDocument::fromJson(lines.last()).object();
        const bool good=code==0 && status==QProcess::NormalExit && result.value("state")=="cloud_time_ready" && result.value("accountScope")==m_scope;
        m_statisticsStatus=good?QStringLiteral("云端统计已更新"):result.value("code")=="statistics_auth_failed"?QStringLiteral("统计 API 鉴权失败，请打开设置核对或更换 API Key；已有数据保留"):QStringLiteral("云端统计读取失败，请检查统计 API 配置；已有数据保留");emit readingStatsChanged();
    });
    connect(&m_statsProcess,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){if(error==QProcess::FailedToStart){m_statisticsStatus=QStringLiteral("统计组件无法启动");emit readingStatsChanged();}});
    m_entryRetryTimer.setSingleShot(true);m_entryTimeoutTimer.setSingleShot(true);
    connect(&m_entryRetryTimer,&QTimer::timeout,this,&DownloaderController::tryReadingEntry);
    connect(&m_entryTimeoutTimer,&QTimer::timeout,this,[this]{m_entryTimedOut=true;m_entryProcess.kill();});
    connect(&m_entryProcess,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,&DownloaderController::finishReadingEntry);
    connect(&m_entryProcess,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        if(error==QProcess::FailedToStart)finishReadingEntry(-1,QProcess::CrashExit);
    });
    m_readingTimer.setInterval(5000);
    connect(&m_readingTimer,&QTimer::timeout,this,&DownloaderController::checkpointReadingTime);
    m_readingClockTimer.setInterval(30000);
    connect(&m_readingClockTimer,&QTimer::timeout,this,&DownloaderController::readingClockChanged);
    m_readingClockTimer.start();
    connect(&m_progressProcess, &QProcess::readyReadStandardOutput, this, &DownloaderController::readProgress);
    connect(&m_progressProcess, qOverload<int,QProcess::ExitStatus>(&QProcess::finished), this, [this](int,QProcess::ExitStatus){readProgress();QTimer::singleShot(0,this,&DownloaderController::processExitQueue);});
    m_covers.ready = [this] { ++m_coverRevision; emit coversChanged(); };
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &DownloaderController::readOutput);
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &DownloaderController::finish);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_error = true; m_status = QStringLiteral("下载组件无法启动，请检查安装");
            clearQueue(); restoreImporter(); emit changed();
        }
    });
}
DownloaderController::~DownloaderController() {
    m_entryNeeded=false;m_entryRetryTimer.stop();m_entryTimeoutTimer.stop();disconnect(&m_entryProcess,nullptr,this,nullptr);
    checkpointReadingTime();m_readingTime.close();
    disconnect(&m_autoSyncProcess,nullptr,this,nullptr);
    if(m_autoSyncProcess.state()!=QProcess::NotRunning){m_autoSyncProcess.terminate();if(!m_autoSyncProcess.waitForFinished(1500)){m_autoSyncProcess.kill();m_autoSyncProcess.waitForFinished(500);}}
    if(m_entryProcess.state()!=QProcess::NotRunning){m_entryProcess.kill();m_entryProcess.waitForFinished(1000);}
    if(m_statsProcess.state()!=QProcess::NotRunning){m_statsProcess.kill();m_statsProcess.waitForFinished(1000);}
    stopProgress();
    if (busy()) {
        m_process.terminate();
        if (!m_process.waitForFinished(1500)) { m_process.kill(); m_process.waitForFinished(500); }
    }
    restoreImporter();
}
void DownloaderController::notifyNativeBooks(){++m_nativeBookRevision;emit nativeBooksChanged();}
void DownloaderController::reloadNativeBooks(){
    const auto books=privateObject(m_dataRoot+"/imports.json").value("books").toObject();
    QMap<QString,QJsonObject> imported;
    for(auto i=books.begin();i!=books.end();++i){
        auto row=i.value().toObject();const auto uuid=QUuid(row.value("uuid").toString());
        if(row.value("state")!="imported" || uuid.isNull()
           || !QRegularExpression("\\A[a-f0-9]{64}\\z").match(row.value("accountScope").toString()).hasMatch()
           || !QRegularExpression("\\A[A-Za-z0-9_-]{1,128}\\z").match(i.key()).hasMatch())continue;
        row["bookId"]=i.key();imported[uuid.toString(QUuid::WithoutBraces)]=row;
    }
    if(imported!=m_nativeBooks){m_nativeBooks=imported;notifyNativeBooks();}
}
QString DownloaderController::nativeBookBadge(const QString &uuid) const {
    const auto key=QUuid(uuid).toString(QUuid::WithoutBraces);
    if(!m_nativeBooks.contains(key))return {};
    const auto row=m_nativeBooks.value(key);
    if(m_exitQueue.contains(key) || (m_exitUuid==key && m_autoSyncProcess.state()!=QProcess::NotRunning)
       || (m_process.state()!=QProcess::NotRunning && (m_operation=="sync-progress" || m_operation=="upload-time")
           && m_currentId==row.value("bookId").toString() && m_scope==row.value("accountScope").toString()))return "sync";
    return m_exitErrors.contains(key)?"error":"weread";
}
void DownloaderController::processExitQueue(){
    if(busy() || m_entryProcess.state()!=QProcess::NotRunning || !m_readingAppActive || m_readingLocked || m_deviceSleeping || m_exitQueue.isEmpty())return;
    const auto remaining=m_exitNotBefore.value(m_exitQueue.first())-m_exitClock.elapsed();
    if(remaining>0){QTimer::singleShot(int(qMin<qint64>(remaining,1500)),this,&DownloaderController::processExitQueue);return;}
    stopProgress();reloadNativeBooks();
    const auto uuid=m_exitQueue.takeFirst();m_exitNotBefore.remove(uuid);const auto job=m_exitJobs.take(uuid);
    auto provenance=job;provenance.remove("expectedPage");provenance.remove("openingSignature");provenance.remove("readingSession");
    if(uuid==m_readingUuid || m_nativeBooks.value(uuid)!=provenance){notifyNativeBooks();QTimer::singleShot(0,this,&DownloaderController::processExitQueue);return;}
    m_exitUuid=uuid;m_exitJob=job;m_exitBuffer.clear();m_exitTimings={};m_exitSuccess=false;m_exitCanceled=false;m_exitWorkerClock.start();
    QProcessEnvironment env;env.insert("PATH","/usr/bin:/bin:/usr/sbin:/sbin");env.insert("HOME","/home/root");
    env.insert("LANG","C.UTF-8");env.insert("LC_ALL","C.UTF-8");
    env.insert("WEREAD_TIME_ZONE",m_readingTimeZoneId);env.insert("TZ",m_readingTimeZoneId);
    env.insert("WEREAD_READING_SESSION",job.value("readingSession").toString());
    m_autoSyncProcess.setProcessEnvironment(env);m_autoSyncProcess.setStandardErrorFile(QProcess::nullDevice());
    qInfo("WereadDownloaderPlugin: exit-sync started");
    m_autoSyncProcess.start(m_workerProgram,QStringList{"exit-sync",job.value("bookId").toString(),job.value("accountScope").toString(),uuid,QString::number(job.value("expectedPage").toInt(-1)),job.value("openingSignature").toString()});
    const auto generation=++m_exitGeneration;
    QTimer::singleShot(180000,this,[this,generation]{if(generation==m_exitGeneration && m_autoSyncProcess.state()!=QProcess::NotRunning)stopExitSync();});
    notifyNativeBooks();emit changed();
}
void DownloaderController::stopExitSync(){
    m_autoSyncProcess.terminate();const auto generation=m_exitGeneration;
    QTimer::singleShot(2000,this,[this,generation]{if(generation==m_exitGeneration && m_autoSyncProcess.state()!=QProcess::NotRunning)m_autoSyncProcess.kill();});
}
void DownloaderController::readExitSync(){
    m_exitBuffer+=m_autoSyncProcess.readAllStandardOutput();
    if(m_exitBuffer.size()>1024*1024){m_exitBuffer.clear();stopExitSync();return;}
    int end;
    while((end=m_exitBuffer.indexOf('\n'))>=0){
        const auto row=QJsonDocument::fromJson(m_exitBuffer.left(end)).object();m_exitBuffer.remove(0,end+1);
        if((row.value("state")=="progress_synced" || row.value("state")=="exit_sync_skipped") && row.value("bookId")==m_exitJob.value("bookId")
           && row.value("accountScope")==m_exitJob.value("accountScope") && QUuid(row.value("uuid").toString())==QUuid(m_exitUuid)){
            if(row.value("state")=="exit_sync_skipped")qInfo("WereadDownloaderPlugin: exit-sync position-unchanged");
            if(row.value("timings").isObject())m_exitTimings=row.value("timings").toObject();
            m_exitSuccess=!row.contains("timeUploaded") || row.value("timeUploaded").toBool();
            if(row.value("recentMarked").toBool())emit recentReadRequested(m_exitUuid);
            applyProgress(row);
            m_syncNotice=row.value("cloudPositionPreserved").toBool()
                ? (row.value("timeUploadedSeconds").toInt()>0 ? QStringLiteral("云端位置已保留；本次阅读时长已提交") : QStringLiteral("云端位置已保留；本次无可提交时长")) : QString();
            if(!m_syncNotice.isEmpty()) {m_status=m_syncNotice;qInfo("WereadDownloaderPlugin: exit-sync cloud-position-preserved");}
        }else if(row.value("state")=="error")m_exitSuccess=false;
    }
}
void DownloaderController::finishExitSync(int code,QProcess::ExitStatus status){
    if(m_exitUuid.isEmpty())return;
    readExitSync();const auto uuid=m_exitUuid;
    const bool success=code==0 && status==QProcess::NormalExit && m_exitSuccess && !m_exitCanceled;
    if(!m_exitCanceled){if(success)m_exitErrors.remove(uuid);else m_exitErrors.insert(uuid);}
    if(success)m_shelfAfterAction=true;
    const char *matchMode=m_exitTimings.value("matchMode").toString()=="nearby"?"nearby":"full";
    const char *sourceMode=m_exitTimings.value("sourceMode").toString()=="local-cache"?"local-cache":"network";
    qInfo("WereadDownloaderPlugin: exit-sync timing total_ms=%lld lua_total_ms=%d page_text_ms=%d cache_key_ms=%d catalog_ms=%d local_chapter_ms=%d match_mode=%s local_source_ms=%d source_mode=%s chapter_fetch_ms=%d cloud_read_ms=%d position_write_ms=%d position_verify_ms=%d recent_ms=%d reading_time_ms=%d",
        static_cast<long long>(m_exitWorkerClock.elapsed()),m_exitTimings.value("totalLuaMs").toInt(),m_exitTimings.value("pageTextMs").toInt(),
        m_exitTimings.value("cacheKeyMs").toInt(),m_exitTimings.value("catalogMs").toInt(),m_exitTimings.value("localChapterMs").toInt(),matchMode,
        m_exitTimings.value("localSourceMs").toInt(),sourceMode,m_exitTimings.value("chapterFetchMs").toInt(),m_exitTimings.value("cloudReadMs").toInt(),
        m_exitTimings.value("positionWriteMs").toInt(),m_exitTimings.value("positionVerifyMs").toInt(),m_exitTimings.value("recentMs").toInt(),
        m_exitTimings.value("readingTimeMs").toInt());
    qInfo("WereadDownloaderPlugin: exit-sync finished success=%d canceled=%d",success,m_exitCanceled);
    m_exitUuid.clear();m_exitJob={};m_exitBuffer.clear();notifyNativeBooks();emit changed();emit readingStatsChanged();
    // Background failures stay passive: never open QR login or interrupt reading.
    QTimer::singleShot(0,this,[this]{
        if(busy())return;
        if(!m_exitQueue.isEmpty())processExitQueue();
        else if(m_opened)start(m_loggedIn?"shelf":"status");
    });
}
QString DownloaderController::readingSessionStatus() const {
    if(m_readingUuid.isEmpty() || !m_readingTracked)return {};
    QString entry;
    if(!m_entryNeeded)entry=QStringLiteral("开始阅读已上传");
    else if(m_entryTerminal)entry=QStringLiteral("开始阅读上传失败，请检查登录或书籍状态");
    else if(!m_readingWifiConnected)entry=QStringLiteral("等待 Wi-Fi 联网，上传已暂停");
    else if(m_entryProcess.state()!=QProcess::NotRunning)entry=QStringLiteral("正在上传开始阅读（第 %1 次）").arg(m_entryAttempts);
    else if(m_deviceSleeping || m_readingSuspended || !m_readingAppActive || m_readingLocked || !m_readingTime.active())entry=QStringLiteral("开始阅读上传等待恢复阅读");
    else entry=m_entryAttempts?QStringLiteral("开始阅读上传失败，等待重试"):QStringLiteral("等待上传开始阅读");
    const auto seconds=qMax<qint64>(0,m_readingTime.totalSeconds()-m_readingStartSeconds);
    return entry+QStringLiteral("\n本次本机阅读 %1分%2秒 · %3").arg(seconds/60).arg(seconds%60)
        .arg(m_readingTime.active()?QStringLiteral("计时中"):QStringLiteral("计时暂停"));
}
void DownloaderController::checkpointReadingTime() {
    if(m_readingClock.isValid())m_readingTime.advance(m_readingClock.restart());
    m_readingTime.save();emit readingSessionChanged();
    if(!m_readingTime.active())m_readingTimer.stop();
    if(m_opened)emit readingStatsChanged();
}
void DownloaderController::updateReadingTimer() {
    m_readingTime.paused(m_opened || m_readingSuspended || !m_readingAppActive || m_readingLocked);
    m_readingClock.start();
    if(m_readingTime.active())m_readingTimer.start();else m_readingTimer.stop();
    updateReadingEntry();
}
void DownloaderController::updateReadingEntry() {
    emit readingSessionChanged();
    if(!m_entryNeeded || m_entryTerminal || m_readingUuid.isEmpty() || m_readingSession.isEmpty()
       || !m_readingWifiConnected || m_opened || m_deviceSleeping || m_readingSuspended || !m_readingAppActive || m_readingLocked || !m_readingTime.active()) {
        m_entryRetryTimer.stop();return;
    }
    if(m_entryProcess.state()!=QProcess::NotRunning || m_entryRetryTimer.isActive())return;
    const int delays[]={0,5000,15000,30000,60000};
    m_entryRetryTimer.start(delays[qMin(m_entryAttempts,4)]);
}
void DownloaderController::tryReadingEntry() {
    if(!m_entryNeeded || m_entryTerminal || m_readingUuid.isEmpty() || !m_readingWifiConnected || m_opened || m_deviceSleeping
       || m_readingSuspended || !m_readingAppActive || m_readingLocked || !m_readingTime.active())return;
    if(m_entryProcess.state()!=QProcess::NotRunning)return;
    if(busy()){m_entryRetryTimer.start(5000);return;}
    reloadNativeBooks();const auto book=m_nativeBooks.value(m_readingUuid);
    if(book.value("bookId").toString().isEmpty()){m_entryTerminal=true;return;}
    m_entryAttemptSession=m_readingSession;m_entryBookId=book.value("bookId").toString();
    m_entryTimedOut=false;++m_entryAttempts;
    QProcessEnvironment env;env.insert("PATH","/usr/bin:/bin:/usr/sbin:/sbin");env.insert("HOME","/home/root");
    env.insert("LANG","C.UTF-8");env.insert("LC_ALL","C.UTF-8");
    env.insert("WEREAD_TIME_ZONE",m_readingTimeZoneId);env.insert("WEREAD_READING_SESSION",m_readingSession);
    m_entryProcess.setProcessEnvironment(env);m_entryProcess.setStandardErrorFile(QProcess::nullDevice());
    m_entryTimeoutTimer.start(25000);
    m_entryProcess.start(m_workerProgram,QStringList{"begin-reading",m_entryBookId});
    emit readingSessionChanged();
}
void DownloaderController::finishReadingEntry(int code,QProcess::ExitStatus status) {
    m_entryTimeoutTimer.stop();const auto session=m_entryAttemptSession;m_entryAttemptSession.clear();
    const auto output=m_entryProcess.readAllStandardOutput().trimmed();
    if(session.isEmpty() || session!=m_readingSession){QTimer::singleShot(0,this,&DownloaderController::processExitQueue);return;}
    const auto result=output.size()<=4096?QJsonDocument::fromJson(output.split('\n').last()).object():QJsonObject();
    const bool success=code==0 && status==QProcess::NormalExit && result.value("state")=="reading_session_started"
        && result.value("bookId")==m_entryBookId;
    const auto reason=result.value("code").toString();
    m_entryTerminal=reason=="login_required" || reason=="account_changed" || reason=="progress_not_located"
        || reason=="time_cache_unavailable" || reason=="progress_cache_unavailable";
    if(success)m_entryNeeded=false;
    const char *kind=success?"ready":m_entryTimedOut?"timeout":m_entryTerminal?"blocked":"request-failed";
    const QStringList safeCodes={"login_required","account_changed","progress_not_located","time_cache_unavailable",
        "progress_cache_unavailable","progress_network_failed","cloud_progress_unavailable","time_upload_rejected",
        "helper_timeout","progress_failed","operation_failed","registry_busy"};
    const auto safeCode=safeCodes.contains(reason)?reason.toLatin1():QByteArray("unknown");
    qInfo("WereadDownloaderPlugin: entry attempt=%d result=%s code=%s retry=%d",m_entryAttempts,kind,
        success?"none":safeCode.constData(),!success && !m_entryTerminal && m_entryNeeded);
    updateReadingEntry();QTimer::singleShot(0,this,&DownloaderController::processExitQueue);
}
void DownloaderController::readingStarted(const QString &uuid,int page) {
    m_syncNotice.clear();
    checkpointReadingTime();
    const auto books=privateObject(m_dataRoot+"/imports.json").value("books").toObject();
    reloadNativeBooks();
    const auto nativeId=QUuid(uuid);
    m_readingUuid=nativeId.isNull()?QString():nativeId.toString(QUuid::WithoutBraces);
    m_readingOpeningSignature=WereadDownloader::exitPositionSignature("/home/root/.local/share/remarkable/xochitl/"+m_readingUuid,page);
    m_exitQueue.removeAll(m_readingUuid);m_exitJobs.remove(m_readingUuid);m_exitNotBefore.remove(m_readingUuid);
    if(!m_readingUuid.isEmpty() && m_exitUuid==m_readingUuid && m_autoSyncProcess.state()!=QProcess::NotRunning){m_exitCanceled=true;stopExitSync();}
    m_readingTime.close();
    bool matched=false;
    for(auto i=books.begin();i!=books.end();++i) {
        auto row=i.value().toObject();auto scope=row.value("accountScope").toString();
        if(nativeId.isNull() || QUuid(row.value("uuid").toString())!=nativeId || row.value("state")!="imported"
            || !QRegularExpression("\\A[a-f0-9]{64}\\z").match(scope).hasMatch()
            || !QRegularExpression("\\A[A-Za-z0-9_-]{1,128}\\z").match(i.key()).hasMatch())continue;
        QString title;
        const auto shelf=privateObject(m_dataRoot+"/accounts/"+scope+"/shelf.json").value("books").toArray();
        for(const auto &b:shelf)if(b.toObject().value("bookId")==i.key()) {title=b.toObject().value("title").toString();break;}
        matched=m_readingTime.open(scope,i.key(),row.value("uuid").toString(),title);break;
    }
    m_readingSession=QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_entryRetryTimer.stop();m_entryTimeoutTimer.stop();m_entryAttemptSession.clear();
    if(m_entryProcess.state()!=QProcess::NotRunning){m_entryProcess.kill();m_entryProcess.waitForFinished(500);}
    m_readingTracked=matched;m_readingStartSeconds=m_readingTime.totalSeconds();
    m_entryNeeded=matched;m_entryTerminal=false;m_entryAttempts=0;
    m_readingSuspended=false;updateReadingTimer();
    qInfo("WereadDownloaderPlugin: reading-clock matched=%d active=%d app=%d locked=%d",matched,m_readingTime.active(),m_readingAppActive,m_readingLocked);
}
void DownloaderController::readingStopped(int expectedPage){
    m_entryRetryTimer.stop();m_entryNeeded=false;
    checkpointReadingTime();m_readingTime.close();m_readingTimer.stop();emit readingStatsChanged();
    reloadNativeBooks();
    const auto uuid=m_readingUuid;m_readingUuid.clear();emit readingSessionChanged();
    if(!uuid.isEmpty() && m_nativeBooks.contains(uuid)) {
        m_exitJobs[uuid]=m_nativeBooks.value(uuid);m_exitJobs[uuid]["expectedPage"]=expectedPage;m_exitJobs[uuid]["openingSignature"]=m_readingOpeningSignature;m_exitJobs[uuid]["readingSession"]=m_readingSession;m_exitNotBefore[uuid]=m_exitClock.elapsed()+1500;
        if(!m_exitQueue.contains(uuid))m_exitQueue.append(uuid);
        qInfo("WereadDownloaderPlugin: exit-sync queued");
        m_exitErrors.remove(uuid);notifyNativeBooks();
        // Let native close/save callbacks settle before reading its persisted position.
        QTimer::singleShot(1500,this,&DownloaderController::processExitQueue);
    }
}
void DownloaderController::readingActivity(){checkpointReadingTime();m_readingTime.activity();updateReadingTimer();}
void DownloaderController::setReadingWifiConnected(bool value){m_readingWifiConnected=value;updateReadingEntry();}
void DownloaderController::setReadingDeviceSleeping(bool value){m_deviceSleeping=value;updateReadingEntry();if(!value)QTimer::singleShot(0,this,&DownloaderController::processExitQueue);}
void DownloaderController::setReadingSuspended(bool value){checkpointReadingTime();if(m_readingSuspended && !value)m_readingTime.activity();m_readingSuspended=value;updateReadingTimer();if(!value)QTimer::singleShot(0,this,&DownloaderController::processExitQueue);}
void DownloaderController::setReadingApplicationActive(bool value){checkpointReadingTime();m_readingAppActive=value;updateReadingTimer();if(value)QTimer::singleShot(0,this,&DownloaderController::processExitQueue);}
void DownloaderController::setReadingLocked(bool value){checkpointReadingTime();if(m_readingLocked && !value)m_readingTime.activity();m_readingLocked=value;updateReadingTimer();if(!value)QTimer::singleShot(0,this,&DownloaderController::processExitQueue);}
QStringList DownloaderController::readingDates() const {
    QSet<QString> dates{dayInZone(m_readingTimeZoneId)};
    const auto days=cloudTimeSnapshot(m_dataRoot,m_scope,m_readingTimeZoneId).value("days").toObject();
    for(auto i=days.begin();i!=days.end();++i)if(QDate::fromString(i.key(),Qt::ISODate).isValid())dates.insert(i.key());
    QStringList result=dates.values();std::sort(result.begin(),result.end(),std::greater<QString>());result.prepend("");return result;
}
void DownloaderController::setReadingDate(const QString &date) {
    if(date==m_readingDate)return;
    const auto parsed=QDate::fromString(date,Qt::ISODate);
    if(!date.isEmpty() && date!="undated" && (!parsed.isValid() || parsed.toString(Qt::ISODate)!=date))return;
    m_readingDate=date;
    if(parsed.isValid())m_readingWeekStart=parsed.addDays(1-parsed.dayOfWeek());
    emit readingStatsChanged();refreshCloudStatistics();
}
QVariantList DownloaderController::readingWeekStats() const {
    static const QStringList weekdays{QStringLiteral("周一"),QStringLiteral("周二"),QStringLiteral("周三"),QStringLiteral("周四"),QStringLiteral("周五"),QStringLiteral("周六"),QStringLiteral("周日")};
    const auto days=cloudTimeSnapshot(m_dataRoot,m_scope,m_readingTimeZoneId).value("days").toObject();
    int totals[7]={-1,-1,-1,-1,-1,-1,-1};
    for(auto d=days.begin();d!=days.end();++d) {
        const auto date=QDate::fromString(d.key(),Qt::ISODate);
        if(!date.isValid() || date<m_readingWeekStart || date>=m_readingWeekStart.addDays(7))continue;
        totals[date.dayOfWeek()-1]=qMax(0,d.value().toInt());
    }
    QVariantList result;
    for(int i=0;i<7;++i) {
        const auto date=m_readingWeekStart.addDays(i);
        result.append(QVariantMap{{"date",date.toString(Qt::ISODate)},{"day",weekdays.at(i)},
            {"shortDate",date.toString(QStringLiteral("MM/dd"))},{"seconds",totals[i]}});
    }
    return result;
}
QString DownloaderController::readingWeekLabel() const {
    if(!m_readingWeekStart.isValid())return {};
    return m_readingWeekStart.toString(QStringLiteral("yyyy/MM/dd"))+QStringLiteral(" – ")+m_readingWeekStart.addDays(6).toString(QStringLiteral("MM/dd"));
}
int DownloaderController::readingWeekTotalSeconds() const {
    int total=0;for(const auto &row:readingWeekStats())total+=qMax(0,row.toMap().value("seconds").toInt());return total;
}
int DownloaderController::readingWeekMaximumSeconds() const {
    int maximum=0;for(const auto &row:readingWeekStats())maximum=qMax(maximum,row.toMap().value("seconds").toInt());return maximum;
}
QString DownloaderController::readingWeekPeakLabel() const {
    int maximum=0;QString label;
    for(const auto &row:readingWeekStats()) {const auto value=row.toMap();const auto seconds=value.value("seconds").toInt();if(seconds>maximum){maximum=seconds;label=value.value("day").toString()+QStringLiteral(" · ")+value.value("shortDate").toString();}}
    return maximum>0?label:QStringLiteral("暂无阅读");
}
int DownloaderController::readingDateTotalSeconds() const {
    if(m_readingDate.isEmpty())return cloudReadingTotalSeconds();
    const auto days=cloudTimeSnapshot(m_dataRoot,m_scope,m_readingTimeZoneId).value("days").toObject();
    return days.contains(m_readingDate)?days.value(m_readingDate).toInt(-1):-1;
}
QString DownloaderController::readingClock() const {
    return QDateTime::currentDateTimeUtc().toTimeZone(QTimeZone(m_readingTimeZoneId.toUtf8()))
        .toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}
void DownloaderController::shiftReadingWeek(int weeks) {
    if(weeks==0 || !m_readingWeekStart.isValid())return;
    m_readingWeekStart=m_readingWeekStart.addDays(weeks*7);
    const auto selected=QDate::fromString(m_readingDate,Qt::ISODate);
    if(selected.isValid())m_readingDate=selected.addDays(weeks*7).toString(Qt::ISODate);
    emit readingStatsChanged();refreshCloudStatistics();
}
void DownloaderController::shiftReadingDate(int days) {
    if(days==0)return;
    auto selected=QDate::fromString(m_readingDate,Qt::ISODate);
    if(!selected.isValid())selected=QDate::fromString(dayInZone(m_readingTimeZoneId),Qt::ISODate);
    selected=selected.addDays(days);m_readingDate=selected.toString(Qt::ISODate);
    m_readingWeekStart=selected.addDays(1-selected.dayOfWeek());
    emit readingStatsChanged();refreshCloudStatistics();
}
void DownloaderController::setReadingTimeZone(const QString &zoneId) {
    if(zoneId==m_readingTimeZoneId || !m_readingTimeZones.contains(zoneId) || !QTimeZone(zoneId.toUtf8()).isValid())return;
    const auto previous=m_readingTimeZoneId;
    m_readingTimeZoneId=zoneId;
    if(!saveReadingTimeZone()) {
        m_readingTimeZoneId=previous;
        m_status=QStringLiteral("时区设置保存失败");emit changed();return;
    }
    m_readingTime.setTimeZone(QTimeZone(zoneId.toUtf8()));
    const auto localToday=QDate::fromString(dayInZone(m_readingTimeZoneId),Qt::ISODate);
    m_readingDate=localToday.toString(Qt::ISODate);m_readingWeekStart=localToday.addDays(1-localToday.dayOfWeek());
    emit readingTimeZoneChanged();emit readingClockChanged();emit readingStatsChanged();refreshCloudStatistics();
}
bool DownloaderController::saveReadingTimeZone() const {
    const auto path=m_dataRoot+"/reading-time-settings-v1.json";
    if(QFileInfo(path).isSymLink() || QFileInfo(m_dataRoot).isSymLink())return false;
    QSaveFile file(path);file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFile::ReadOwner|QFile::WriteOwner))return false;
    const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"timeZone",m_readingTimeZoneId}}).toJson(QJsonDocument::Compact);
    if(file.write(bytes)!=bytes.size() || !file.commit())return false;
    return true;
}
bool DownloaderController::statisticsConfigured() const {
    const auto cfg=privateObject(m_dataRoot+"/accounts/"+m_scope+"/statistics-api-v1.json");
    return !m_scope.isEmpty() && cfg.value("accountScope")==m_scope && cfg.value("apiKey").toString().size()>=8;
}
int DownloaderController::cloudReadingTotalSeconds() const {
    const auto cloud=privateObject(m_dataRoot+"/accounts/"+m_scope+"/cloud-reading-time-v1.json");
    return cloud.value("accountScope")==m_scope?cloud.value("totalSeconds").toInt(-1):-1;
}
QString DownloaderController::statisticsApiKeyForEditing() const {
    if(!m_opened || !m_loggedIn || m_scope.isEmpty())return {};
    const auto cfg=privateObject(m_dataRoot+"/accounts/"+m_scope+"/statistics-api-v1.json");
    return cfg.value("accountScope")==m_scope?cfg.value("apiKey").toString():QString();
}
void DownloaderController::setStatisticsApiKey(const QString &key) {
    if(m_scope.isEmpty() || !m_loggedIn || key.size()<8 || key.size()>4096 || key.contains('\n') || key.contains('\r'))return;
    const auto path=m_dataRoot+"/accounts/"+m_scope+"/statistics-api-v1.json";
    if(QFileInfo(path).isSymLink())return;
    QSaveFile file(path);file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFile::ReadOwner|QFile::WriteOwner))return;
    const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"accountScope",m_scope},{"apiKey",key.trimmed()}}).toJson(QJsonDocument::Compact);
    if(file.write(bytes)!=bytes.size() || !file.commit())return;
    m_statisticsStatus=QStringLiteral("统计 API 配置已保存");emit readingStatsChanged();refreshCloudStatistics();
}
void DownloaderController::refreshCloudStatistics() {
    if(!m_opened || !m_loggedIn || m_scope.isEmpty())return;
    if(!statisticsConfigured()){m_statisticsStatus=QStringLiteral("请先配置插件独立的统计 API");emit readingStatsChanged();return;}
    if(m_statsProcess.state()!=QProcess::NotRunning){m_statsProcess.kill();m_statsProcess.waitForFinished(500);}
    QProcessEnvironment env;env.insert("PATH","/usr/bin:/bin:/usr/sbin:/sbin");env.insert("HOME","/home/root");env.insert("LANG","C.UTF-8");env.insert("LC_ALL","C.UTF-8");
    env.insert("WEREAD_TIME_ZONE",m_readingTimeZoneId);
    auto date=QDate::fromString(m_readingDate,Qt::ISODate);if(!date.isValid())date=m_readingWeekStart.isValid()?m_readingWeekStart:QDate::fromString(dayInZone(m_readingTimeZoneId),Qt::ISODate);
    env.insert("WEREAD_STATISTICS_BASE_TIME",QString::number(QDateTime(date,QTime(12,0),QTimeZone(m_readingTimeZoneId.toUtf8())).toSecsSinceEpoch()));
    m_statsProcess.setProcessEnvironment(env);m_statsProcess.setStandardErrorFile(QProcess::nullDevice());
    m_statisticsStatus=QStringLiteral("正在读取云端阅读统计…");m_statsProcess.start(m_workerProgram,QStringList{"cloud-time"});
    const auto generation=++m_statisticsGeneration;
    QTimer::singleShot(60000,this,[this,generation]{if(generation==m_statisticsGeneration && m_statsProcess.state()!=QProcess::NotRunning)m_statsProcess.kill();});
    emit readingStatsChanged();
}
QVariantList DownloaderController::readingStats() const {
    const auto local=privateObject(m_dataRoot+"/accounts/"+m_scope+"/local-reading-time-v1.json").value("books").toObject();
    QVariantList result;
    for(const auto &v:m_books){
        auto row=v.toMap();if(row.value("importState")!="imported")continue;
        const auto record=local.value(row.value("bookId").toString()).toObject();
        const auto value=m_readingDate.isEmpty()?record.value("seconds"):record.value("days").toObject().value(m_readingDate);
        const double seconds=value.toDouble(-1);
        if(!value.isDouble() || seconds<=0 || seconds>315360000 || seconds!=qint64(seconds))continue;
        row["localSeconds"]=int(seconds);result.append(row);
    }
    std::stable_sort(result.begin(),result.end(),[](const QVariant &a,const QVariant &b){
        return a.toMap().value("localSeconds").toInt()>b.toMap().value("localSeconds").toInt();
    });
    return result;
}
void DownloaderController::uploadReadingTime(const QString &id) {
    Q_UNUSED(id);m_status=QStringLiteral("手动上传阅读时长已停用");emit changed();
}
void DownloaderController::attachSettings(QObject *settings) {
    if (m_settings || !settings || !settings->property("webInterfaceEnabled").isValid()) return;
    m_settings = settings;
    if (!restoreImporter()) m_status = QStringLiteral("系统导入设置待恢复，请暂停其他传输后重试");
    qInfo("WereadDownloaderPlugin: host-ready");
    emit changed();
}
void DownloaderController::placeBelowAppLoad(QQuickItem *item) {
    QPointer<QQuickItem> guarded = item;
    // Run after all independently loaded QMD components have been instantiated.
    QTimer::singleShot(0, this, [guarded] {
        if (!guarded || !guarded->parentItem()) return;
        for (auto *sibling : guarded->parentItem()->childItems()) {
            if (sibling->objectName() == "openAppLoad") {
                guarded->stackAfter(sibling); qInfo("WereadDownloaderPlugin: sidebar-ready"); return;
            }
        }
        guarded->setVisible(false); // No silent fallback to an unrelated position.
    });
}
void DownloaderController::openPage() {
    if (m_opened) return;
    checkpointReadingTime();
    qInfo("WereadDownloaderPlugin: page-open");
    m_opened = true; emit changed();
    updateReadingTimer();emit readingStatsChanged();
    if (!busy()) start("status");
}
QString DownloaderController::coverSource(const QString &id, const QString &url) {
    return m_opened && m_loggedIn ? m_covers.source(id, url) : QString();
}
void DownloaderController::closePage() {
    m_shelfAfterAction=false;
    stopProgress();
    m_covers.stop();
    m_opened = false; cancel();
    updateReadingTimer();
    m_qrImage.clear(); m_image->update({}); emit changed();
}
void DownloaderController::cancel() {
    clearQueue();
    if(m_opened){
        m_exitQueue.clear();m_exitJobs.clear();m_exitNotBefore.clear();
        if(m_autoSyncProcess.state()!=QProcess::NotRunning){m_exitCanceled=true;stopExitSync();}
        notifyNativeBooks();
    }
    if (m_process.state()!=QProcess::NotRunning) {
        const auto generation = m_generation;
        m_process.terminate();
        QTimer::singleShot(2000, this, [this, generation] { if (busy() && generation == m_generation) m_process.kill(); });
    }
    m_status = QStringLiteral("已取消等待任务；已提交的书籍将在刷新后核实"); emit changed();
}
void DownloaderController::login() {
    if (busy()) return;
    m_covers.stop(); m_books.clear(); m_loggedIn = false; m_scope.clear(); emit booksChanged();
    m_accountName.clear(); m_qrImage.clear(); m_image->update({}); start("login");
}
void DownloaderController::logout() { if (m_loggedIn && !busy()) start("logout"); }
void DownloaderController::refresh() { if (!busy()) { m_autoRestoreTried.clear(); start(m_loggedIn ? "shelf" : "status"); } }
QString DownloaderController::currentTitle() const {
    for (const auto &row : m_books) {
        const auto book = row.toMap();
        if (book.value("bookId") == m_currentId) return book.value("title").toString();
    }
    return {};
}
void DownloaderController::stopProgress() {
    if (m_progressProcess.state() != QProcess::NotRunning) {
        m_progressProcess.terminate();
        if (!m_progressProcess.waitForFinished(1500)) { m_progressProcess.kill(); m_progressProcess.waitForFinished(500); }
    }
    m_progressBuffer.clear();
}
void DownloaderController::startProgress() {
    refreshCloudStatistics();
    if(!m_exitQueue.isEmpty()){processExitQueue();return;}
    if (!m_opened || !m_loggedIn || busy() || m_progressProcess.state()!=QProcess::NotRunning) return;
    bool imported=false;
    for (const auto &row:m_books) if (row.toMap().value("importState")=="imported") imported=true;
    if (!imported) return;
    QProcessEnvironment env;
    env.insert("PATH","/usr/bin:/bin:/usr/sbin:/sbin"); env.insert("HOME","/home/root");
    env.insert("LANG","C.UTF-8"); env.insert("LC_ALL","C.UTF-8");
    m_progressProcess.setProcessEnvironment(env); m_progressProcess.setStandardErrorFile(QProcess::nullDevice());
    m_progressProcess.start(m_workerProgram,QStringList{"reading-progress"});
}
void DownloaderController::applyProgress(const QJsonObject &row) {
    if (!m_opened || !m_loggedIn || row.value("accountScope").toString()!=m_scope) return;
    const auto id=row.value("bookId").toString();
    for(int i=0;i<m_books.size();++i) {
        auto book=m_books[i].toMap();
        if(book.value("bookId")!=id || book.value("importState")!="imported") continue;
        for(const auto &key:{"cloudProgress","localProgress","cloudProgressStale","cloudChapter","localChapter","restorePending","restoreAutomatic"})
            if(row.contains(key)) book[key]=row.value(key).toVariant();
        m_books[i]=book; emit bookUpdated(id,book);emit readingStatsChanged();
    }
}
void DownloaderController::readProgress() {
    m_progressBuffer+=m_progressProcess.readAllStandardOutput();
    if(m_progressBuffer.size()>8*1024*1024) { m_progressProcess.kill();m_progressBuffer.clear();return; }
    int end;
    while((end=m_progressBuffer.indexOf('\n'))>=0) {
        auto row=QJsonDocument::fromJson(m_progressBuffer.left(end)).object();m_progressBuffer.remove(0,end+1);
        if(row.value("state")=="reading_progress") {
            applyProgress(row);
            const auto id=row.value("bookId").toString();
            if(row.value("accountScope").toString()==m_scope && row.value("restorePending").toBool() && row.value("restoreAutomatic").toBool() && !m_autoRestoreTried.contains(id)) {
                QTimer::singleShot(0,this,[this,id]{ if(m_opened && m_loggedIn && !busy() && !m_autoRestoreTried.contains(id)) downloadProgress(id); });
            }
        }
    }
}
void DownloaderController::uploadProgress(const QString &id) {
    if (!m_opened || !m_loggedIn || busy()) return;
    for(const auto &row:m_books) {
        auto book=row.toMap();
        if(book.value("bookId")==id && book.value("importState")=="imported") {
            m_autoRestoreTried.insert(id);
            start("sync-progress",id);return;
        }
    }
}
void DownloaderController::downloadProgress(const QString &id) {
    if (!m_opened || !m_loggedIn || busy()) return;
    for (const auto &row:m_books) {
        const auto book=row.toMap();
        if(book.value("bookId")==id && book.value("importState")=="imported") {
            m_autoRestoreTried.insert(id); start("restore-progress",id); return;
        }
    }
}
void DownloaderController::completeRestore(const QString &id,bool ok) {
    if (busy() && id==m_restoreId) { m_process.write(ok?"ok\n":"no\n");m_restoreId.clear(); }
}
void DownloaderController::importBooks(const QVariantList &ids) {
    if (!m_opened || !canQueue()) return;
    for (const auto &value : ids) {
        const auto id = value.toString();
        if (!QRegularExpression("\\A[A-Za-z0-9_-]{1,128}\\z").match(id).hasMatch() || m_queue.contains(id)) continue;
        for (const auto &row : m_books) {
            auto book = row.toMap();
            if (book.value("bookId") == id && importable(book.value("importState").toString())) {
                m_queuedStates[id] = book.value("importState").toString();
                m_queue.append(id); updateBook(id, "queued"); break;
            }
        }
    }
    emit changed(); next();
}
void DownloaderController::clearCache(const QString &id) {
    if (!m_opened || !m_loggedIn || busy() || !m_queue.isEmpty()) return;
    for (const auto &row : m_books) {
        const auto book = row.toMap();
        const auto state = book.value("importState").toString();
        if (book.value("bookId") == id && (state == "removed" || state == "imported")
            && book.value("hasDownloadCache", true).toBool()) { start("clear-cache", id); return; }
    }
}
void DownloaderController::cancelQueued(const QString &id) {
    if (!m_queue.removeOne(id)) return;
    updateBook(id, m_queuedStates.take(id));
    emit changed();
}
void DownloaderController::clearQueue() {
    const auto previous = m_queuedStates;
    m_queue.clear(); m_queuedStates.clear();
    for (auto it = previous.cbegin(); it != previous.cend(); ++it) updateBook(it.key(), it.value());
}
void DownloaderController::next() {
    if (!m_opened || busy()) return;
    if (!m_queue.isEmpty()) {
        const auto id = m_queue.takeFirst(); m_queuedStates.remove(id); start("import", id);
    }
}
void DownloaderController::start(const QString &operation, const QString &id) {
    if (busy()) return;
    if(operation=="shelf" || operation=="logout")m_shelfAfterAction=false;
    stopProgress();
    if(operation=="sync-progress")for(auto i=m_nativeBooks.begin();i!=m_nativeBooks.end();++i)
        if(i.value().value("bookId").toString()==id && i.value().value("accountScope").toString()==m_scope){m_exitQueue.removeAll(i.key());m_exitJobs.remove(i.key());m_exitNotBefore.remove(i.key());}
    ++m_generation;
    m_operation = operation; m_currentId = id; m_error = false; m_final = false; m_loginRequired = false; m_refreshNeeded = false; m_buffer.clear();
    m_status = operation == "login" ? QStringLiteral("正在创建登录二维码…")
        : operation == "shelf" ? QStringLiteral("正在同步书库和导入状态…")
        : operation == "sync-progress" ? QStringLiteral("正在同步《%1》阅读进度…").arg(currentTitle())
        : operation == "upload-time" ? QStringLiteral("正在上传阅读时长…")
        : operation == "restore-progress" ? QStringLiteral("正在恢复《%1》云端阅读位置…").arg(currentTitle())
        : operation == "clear-cache" ? QStringLiteral("正在清理《%1》缓存…").arg(currentTitle())
        : operation == "import" ? QStringLiteral("正在下载《%1》…").arg(currentTitle()) : QStringLiteral("正在检查独立账号…");
    if (operation == "import") { m_covers.pause(); bookState("preparing"); }
    QProcessEnvironment env;
    env.insert("PATH", "/usr/bin:/bin:/usr/sbin:/sbin"); env.insert("HOME", "/home/root");
    env.insert("LANG", "C.UTF-8"); env.insert("LC_ALL", "C.UTF-8");
    env.insert("WEREAD_TIME_ZONE",m_readingTimeZoneId);env.insert("TZ",m_readingTimeZoneId);
    m_process.setProcessEnvironment(env);
    m_process.setStandardErrorFile(QProcess::nullDevice());
    m_process.setProgram(m_workerProgram);
    QStringList args{operation}; if (!id.isEmpty()) args.append(id);
    m_process.setArguments(args); m_process.start(); emit changed();notifyNativeBooks();
}
void DownloaderController::bookState(const QString &state, int progress) { updateBook(m_currentId, state, progress); }
void DownloaderController::updateBook(const QString &id, const QString &state, int progress) {
    for (int i = 0; i < m_books.size(); ++i) {
        auto book = m_books[i].toMap();
        if (book.value("bookId") != id) continue;
        book["importState"] = state; if (progress >= 0) book["progress"] = progress;
        m_books[i] = book;
        emit bookUpdated(id, book);
    }
}
void DownloaderController::readOutput() {
    m_buffer += m_process.readAllStandardOutput();
    if (m_buffer.size() > 8 * 1024 * 1024) { m_error = true; m_process.kill(); return; }
    int lineEnd;
    while ((lineEnd = m_buffer.indexOf('\n')) >= 0) {
        auto line = m_buffer.left(lineEnd); m_buffer.remove(0, lineEnd + 1);
        auto row = QJsonDocument::fromJson(line).object();
        const auto state = row.value("state").toString();
        if (state == "qr") {
            const auto url = row.value("url").toString();
            if (!QRegularExpression("\\Ahttps://weread\\.qq\\.com/web/confirm\\?uid=[A-Za-z0-9_-]{1,512}\\z").match(url).hasMatch()) continue;
            m_image->update(url); m_qrImage = "image://weread-downloader-qr/login?" + QString::number(++m_qrRevision);
            m_status = QStringLiteral("请用微信扫描二维码，确认登录微信读书");
            emit changed();
        } else if (state == "authenticated") {
            m_accountName = row.value("accountName").toString();
            m_loggedIn = true; m_final = true; m_qrImage.clear(); m_image->update({});
        } else if (state == "login_required" || state == "logged_out") {
            m_accountName.clear();
            m_covers.stop(); m_books.clear(); m_scope.clear();
            emit booksChanged();
            m_loggedIn = false; m_loginRequired = true; m_final = true;
        } else if (state == "shelf") {
            auto scope = row.value("accountScope").toString();
            if (scope != m_scope) {m_books.clear();m_syncNotice.clear();}
            m_scope = scope; m_books = row.value("books").toArray().toVariantList();
            m_covers.begin(m_scope, m_books);
            ++m_coverRevision; emit coversChanged(); emit booksChanged();
            m_refreshNeeded = row.value("stale").toBool();
            m_status = !m_syncNotice.isEmpty()?m_syncNotice:row.value("cached").toBool() ? QStringLiteral("已显示本地书架，导入状态已核对") : QStringLiteral("书库已更新"); m_final = true;
        } else if (state == "restore_position") {
            m_restoreId = row.value("bookId").toString();
            m_status = QStringLiteral("正在恢复云端阅读位置…");
            emit restoreRequested(row.value("uuid").toString(), row.value("page").toInt(-1), m_restoreId);
        } else if (state == "restore_skipped") {
            m_autoRestoreTried.insert(m_currentId);
            m_status = QStringLiteral("已导入；云端位置暂未恢复，可点击下载进度重试"); m_final = true;
        } else if (state == "progress_synced" || state == "progress_restored") {
            applyProgress(row); m_final = true;
            m_syncNotice=row.value("cloudPositionPreserved").toBool()?QStringLiteral("云端位置已保留"):QString();
            m_status = !m_syncNotice.isEmpty()?m_syncNotice:state == "progress_synced" ? QStringLiteral("阅读进度已同步到微信读书") : QStringLiteral("已恢复云端阅读位置");
            if(state=="progress_synced") {
                m_shelfAfterAction=true;
                const auto uuid=row.value("uuid").toString();
                if(QRegularExpression("\\A[0-9a-fA-F-]{36}\\z").match(uuid).hasMatch()) emit recentReadRequested(uuid);
                if(!row.value("cloudPositionPreserved").toBool() && row.value("recentMarked").isBool() && !row.value("recentMarked").toBool())
                    m_status+=QStringLiteral("；云端最近阅读标记未完成");
                if(row.value("timeUploaded").isBool() && !row.value("timeUploaded").toBool())
                    m_status+=QStringLiteral("；阅读时长待核对或重试");
                if(row.value("timeRemainingSeconds").toInt()>0)
                    m_status+=QStringLiteral("；还有 %1 秒阅读时长待上传").arg(row.value("timeRemainingSeconds").toInt());
            }
        } else if(state=="time_uploaded") {
            m_final=true;
            const auto uploaded=qMax(0,row.value("uploadedSeconds").toInt());
            const auto deferred=qMax(0,row.value("deferredSeconds").toInt());
            const auto today=qMax(0,row.value("remainingSeconds").toInt()-deferred);
            m_status=QStringLiteral("本次已提交 %1分%2秒；当日剩余 %3分%4秒")
                .arg(uploaded/60).arg(uploaded%60).arg(today/60).arg(today%60);
            if(deferred>0)m_status+=QStringLiteral("；往日未传 %1分%2秒（暂不支持补传）").arg(deferred/60).arg(deferred%60);
            emit readingStatsChanged();
        } else if(state=="recent_mark_failed") {
            m_status+=QStringLiteral("；云端最近阅读标记未完成");
        } else if (state == "progress") {
            int total = row.value("total").toInt(), done = row.value("done").toInt();
            int percent = total > 0 ? qBound(0, int(qint64(done) * 100 / total), 99) : 0;
            bookState("downloading", percent);
            m_status = QStringLiteral("正在下载《%1》：%2 / %3 章").arg(currentTitle()).arg(done).arg(total);
        } else if (state == "downloading" || state == "validating" || state == "importing") {
            bookState(state);
            m_status = state == "validating" ? QStringLiteral("正在校验《%1》…").arg(currentTitle())
                : state == "importing" ? QStringLiteral("正在导入《%1》…").arg(currentTitle()) : QStringLiteral("正在下载《%1》…").arg(currentTitle());
        } else if (state == "cache_cleared" && m_operation == "clear-cache") {
            for (int i = 0; i < m_books.size(); ++i) {
                auto book = m_books[i].toMap();
                if (book.value("bookId") == m_currentId) {
                    book["hasDownloadCache"] = false; m_books[i] = book;
                    emit bookUpdated(m_currentId, book);
                }
            }
            m_final = true;
            m_status = QStringLiteral("插件下载缓存已删除；重新导入时会重新下载正文");
        } else if (state == "needs_importer") {
            if (enableImporter()) QTimer::singleShot(1000, this, [this] {
                if (busy()) m_process.write("go\n");
            });
            else { m_error = true; m_status = QStringLiteral("系统导入服务不可用或有其他传输正在运行"); m_process.terminate(); }
        } else if (state == "imported" || state == "pending" || state == "conflict") {
            bookState(state); m_final = true;
            const auto uuid=row.value("uuid").toString();
            if(state=="imported") m_shelfAfterAction=true;
            if(state=="imported" && QRegularExpression("\\A[0-9a-fA-F-]{36}\\z").match(uuid).hasMatch()) emit recentReadRequested(uuid);
            m_status = state == "imported" ? QStringLiteral("已导入系统书库根目录")
                : QStringLiteral("系统结果待核实；请刷新，不会自动重复导入");
        } else if (state == "error") {
            m_error = true;
            if (row.value("code") == "login_required") {
                m_covers.stop(); m_loggedIn = false; m_books.clear(); m_scope.clear();
                m_accountName.clear();
                emit booksChanged();
                m_loginRequired = true; m_final = true;
                m_status = QStringLiteral("登录已失效，正在返回登录页…");
                continue;
            }
            if (row.value("importState") == "failed") { bookState("failed"); m_final = true; }
            m_status = row.value("code")=="time_upload_uncertain" ? QStringLiteral("进度/时长请求结果待核对，已暂停重传以免重复累计")
                : m_operation=="upload-time" ? QStringLiteral("时长上传未完成，记录已保留，请刷新后重试")
                : (m_operation == "sync-progress" || m_operation == "restore-progress")
                ? (row.value("code") == "cloud_progress_changed" ? QStringLiteral("云端进度已变化，请刷新后再同步")
                   : row.value("code") == "progress_sync_unconfirmed" ? QStringLiteral("同步结果待核对，请刷新查看云端进度")
                   : row.value("code") == "cloud_progress_unavailable" ? QStringLiteral("云端进度尚未读取，请刷新后重试")
                   : row.value("code") == "progress_network_failed" ? QStringLiteral("进度读取失败，请检查网络后重试")
                   : row.value("code") == "time_upload_rejected" ? QStringLiteral("微信读书拒绝了进度或时长写入，请刷新核实后重试")
                   : row.value("code") == "progress_source_map_missing" ? QStringLiteral("此书旧缓存缺少章节映射；先点“删除缓存”，再重新导入")
                   : row.value("code") == "progress_layout_unsupported" ? QStringLiteral("当前书籍的分页结构暂不支持同步")
                   : m_operation == "restore-progress" ? QStringLiteral("云端位置无法唯一匹配本地正文，请刷新后重试")
                   : QStringLiteral("暂不能准确定位阅读位置，请确认已读到正文并刷新后重试"))
                : row.value("code") == "native_rejected"
                ? QStringLiteral("系统拒绝了上传，EPUB 缓存已保留，可重新导入")
                : row.value("code") == "cover_unavailable" ? QStringLiteral("封面获取失败，正文缓存已保留，可重试")
                : m_operation == "shelf" && m_loggedIn
                ? QStringLiteral("已登录，但书架加载失败。请检查网络后点击刷新")
                : m_operation == "login" ? QStringLiteral("登录未完成，请检查网络后重新扫码登录")
                : QStringLiteral("操作未完成，请刷新核实状态后重试");
        }
    }
    emit changed();
}
void DownloaderController::finish(int code, QProcess::ExitStatus exitStatus) {
    readOutput();
    if (!restoreImporter()) { m_error = true; m_status = QStringLiteral("导入设置尚未恢复，已停止后续任务"); clearQueue(); }
    const bool good = code == 0 && exitStatus == QProcess::NormalExit && m_final && !m_error;
    const auto operation = m_operation;
    if (!good) {
        clearQueue();
        if (operation == "import" && !m_final) bookState("pending"); // Refresh resolves safe pre-submit failures too.
        if (!m_error) m_status = QStringLiteral("操作已中断，请刷新核实状态");
    }
    if (operation == "import" && m_queue.isEmpty() && m_opened && m_loggedIn) m_covers.begin(m_scope, m_books);
    emit changed();
    emit readingStatsChanged();
    if(operation=="sync-progress" || operation=="upload-time") {
        for(auto i=m_nativeBooks.begin();i!=m_nativeBooks.end();++i)if(i.value().value("bookId").toString()==m_currentId && i.value().value("accountScope").toString()==m_scope){
            if(good)m_exitErrors.remove(i.key());else m_exitErrors.insert(i.key());
        }
    }
    notifyNativeBooks();QTimer::singleShot(50,this,&DownloaderController::processExitQueue);
    if (!m_opened || (good && operation=="upload-time")) return;
    QTimer::singleShot(0, this, [this, operation, good] {
        if (!m_opened || busy()) return;
        if (good && (operation == "login" || operation == "status" || operation == "logout")) {
            if (m_loginRequired) login();
            else if (operation == "status") start("cached-shelf"); else refresh();
        } else if (!good && m_loginRequired) login();
        else if (good && operation == "cached-shelf" && m_refreshNeeded) refresh();
        else if (good) {
            next();
            if(!busy() && m_shelfAfterAction && m_loggedIn) {m_shelfAfterAction=false;start("shelf");}
            else if (!busy()) startProgress();
        }
        else if (operation == "restore-progress" && m_loggedIn) startProgress();
    });
}
bool DownloaderController::enableImporter() {
    if (!m_settings || peerActive() || !restoreImporter()) return false;
    if (m_settings->property("webInterfaceEnabled").toBool()) return true;
    if (QFileInfo(root).isSymLink() || !QDir().mkpath(root) || ::chmod(QFile::encodeName(root).constData(), 0700)) return false;
    QSaveFile marker(restorePath); marker.setDirectWriteFallback(false);
    if (!marker.open(QIODevice::WriteOnly) || !marker.setPermissions(QFile::ReadOwner | QFile::WriteOwner)
        || marker.write("weread-downloader:restore-disabled:v1\n") != 38 || !marker.commit()) return false;
    m_ownsImporter = true;
    return m_settings->setProperty("webInterfaceEnabled", true);
}
bool DownloaderController::restoreImporter() {
    const QFileInfo info(restorePath);
    if (!info.exists() && !info.isSymLink()) { m_ownsImporter = false; return true; }
    if (!m_settings || peerActive() || !info.isFile() || info.isSymLink() || info.size() > 128) return false;
    QFile file(restorePath);
    if (!file.open(QIODevice::ReadOnly) || file.readAll() != "weread-downloader:restore-disabled:v1\n") return false;
    file.close();
    if (!m_settings->setProperty("webInterfaceEnabled", false) || !QFile::remove(restorePath)) return false;
    m_ownsImporter = false; return true;
}
