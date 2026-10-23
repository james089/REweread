#include "epub.h"
#include "cache_cleanup.h"
#include "reading_progress.h"
#include "exit_sync_state.h"
#include "import_registry.h"
#include "native_address.h"
#include "native_http.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QThread>
#include <QTimer>
#include <cstdio>
#include <csignal>
#include <stdexcept>
#include <sys/stat.h>
#ifdef __linux__
#include <sys/prctl.h>
#include <signal.h>
#include <poll.h>
#include <unistd.h>
#endif

using namespace WereadDownloader;
namespace {
volatile std::sig_atomic_t canceled = 0;
void onSignal(int) { canceled = 1; }
const QString root = QStringLiteral("/home/root/.local/share/weread-downloader");
const QString installRoot = QStringLiteral("/home/root/weread-downloader");
const QString libraryRoot = QStringLiteral("/home/root/.local/share/remarkable/xochitl");
void need(bool value, const char *code) { if (!value) throw std::runtime_error(code); }
void emitRow(const QJsonObject &row) {
    const auto b = QJsonDocument(row).toJson(QJsonDocument::Compact) + '\n';
    need(std::fwrite(b.constData(), 1, size_t(b.size()), stdout) == size_t(b.size()), "ipc_closed");
    std::fflush(stdout);
}
QJsonObject readJson(const QString &path, qint64 limit = 128 * 1024) {
    QFileInfo info(path);
    need(info.isFile() && !info.isSymLink() && info.size() <= limit, "invalid_private_file");
    QFile f(path); need(f.open(QIODevice::ReadOnly), "file_unavailable");
    QJsonParseError e;
    auto doc = QJsonDocument::fromJson(f.readAll(), &e);
    need(e.error == QJsonParseError::NoError && doc.isObject(), "invalid_json");
    return doc.object();
}
QJsonObject helper(const QString &operation, const QString &id = {}, const QString &expectedScope = {}) {
    QProcess process;
    // An allowlist prevents XOVI preload and credential/debugging environment
    // variables from reaching the upstream helper process.
    QProcessEnvironment env;
    env.insert("PATH", "/usr/bin:/bin:/usr/sbin:/sbin");
    env.insert("HOME", "/home/root");
    env.insert("LANG", "C.UTF-8"); env.insert("LC_ALL", "C.UTF-8");
    env.insert("RM_WEREAD_APP_DIR", "/home/root/weread-downloader/helper");
    env.insert("KO_DIR", "/home/root/xovi/exthome/appload/koreader");
    env.insert("RM_WEREAD_LOGIN_TIMEOUT", "180");
    process.setProcessEnvironment(env);
    process.setStandardErrorFile(QProcess::nullDevice());
    process.setProgram("/home/root/xovi/exthome/appload/koreader/luajit");
    QStringList args{installRoot + "/lua/adapter.lua", operation};
    if (!id.isEmpty()) args.append(id);
    if (!expectedScope.isEmpty()) args.append(expectedScope);
    process.setArguments(args);
    process.start(); need(process.waitForStarted(5000), "helper_unavailable");
    QElapsedTimer elapsed; elapsed.start();
    QByteArray pending;
    QJsonObject final;
    bool protocolError = false;
    auto consume = [&] {
        pending += process.readAllStandardOutput();
        if (pending.size() > 8 * 1024 * 1024) { protocolError = true; process.kill(); return; }
        int newline;
        while ((newline = pending.indexOf('\n')) >= 0) {
            auto line = pending.left(newline); pending.remove(0, newline + 1);
            auto object = QJsonDocument::fromJson(line).object();
            auto state = object.value("state").toString();
            if (state == "reading_progress") {
                try {
                    const auto id = object.value("bookId").toString();
                    need(Epub::validBookId(id), "invalid_book_id");
                    const auto registry = readJson(root + "/imports.json", 4*1024*1024).value("books").toObject();
                    const auto record = registry.value(id).toObject();
                    if (record.value("state") != "imported" || record.value("accountScope") != object.value("accountScope")) continue;
                    const auto uuid = record.value("uuid").toString();
                    need(QRegularExpression("\\A[0-9a-fA-F-]{36}\\z").match(uuid).hasMatch(), "invalid_document_id");
                    object["localProgress"] = nativeReadingPercent(readJson(libraryRoot+'/'+uuid+".metadata"),readJson(libraryRoot+'/'+uuid+".content",8*1024*1024));
                    emitRow(object);
                } catch (const std::exception &) {}
            } else if (state == "qr" || state == "progress") emitRow(object);
            else if (!state.isEmpty()) final = object;
        }
    };
    while (process.state() != QProcess::NotRunning) {
        process.waitForReadyRead(250); consume();
        if (canceled) { process.kill(); process.waitForFinished(1000); throw std::runtime_error("operation_canceled"); }
        const qint64 maximum = (operation == "download" || operation == "reading-progress") ? 2 * 60 * 60 * 1000 : 4 * 60 * 1000;
        if (elapsed.elapsed() > maximum) { process.kill(); process.waitForFinished(3000); throw std::runtime_error("helper_timeout"); }
    }
    consume();
    need(!protocolError && pending.trimmed().isEmpty() && process.exitStatus() == QProcess::NormalExit
        && process.exitCode() == 0 && !final.isEmpty(), "helper_failed");
    if (operation != "status" && operation != "login" && final.value("state") == "login_required")
        throw std::runtime_error("login_required");
    return final;
}
LibrarySnapshot scan(const ImportRegistry &registry, bool inspectUnknown = true) {
    const QFileInfo rootInfo(libraryRoot);
    need(rootInfo.isDir() && !rootInfo.isSymLink() && rootInfo.isReadable(), "library_unavailable");
    const auto known = registry.knownDocuments();
    const auto entries = QDir(libraryRoot).entryInfoList({"*.metadata"}, QDir::Files | QDir::NoSymLinks);
    const QRegularExpression uuid("\\A[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}\\z");
    LibrarySnapshot result;
    for (const auto &entry : entries) {
        need(!canceled, "operation_canceled");
        auto id = entry.completeBaseName();
        if (!uuid.match(id).hasMatch()) continue;
        auto metadata = readJson(entry.filePath());
        if (metadata.value("type") != "DocumentType") continue;
        QString source = known.value(id);
        const auto epubPath = libraryRoot + '/' + id + ".epub";
        if (inspectUnknown && source.isEmpty() && QFileInfo(epubPath).isFile()) {
            // Only plugin-tagged EPUBs participate. Invalid/unsupported historical
            // books are ignored; a pending transaction is still never retried.
            try { source = Epub(epubPath).sourceBookId(); } catch (const std::exception &) {}
        }
        const bool deleted = metadata.value("deleted").toBool() || metadata.value("parent") == "trash";
        result.documents.append({id, source, deleted});
    }
    result.complete = true;
    return result;
}
QByteArray coverImage(const QString &scope, const QString &id, const QString &url) {
    const auto account = root + "/accounts/" + scope;
    const auto cached = account + "/covers-v2/" + id + '-' + QString::fromLatin1(QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Sha256).toHex()) + ".png";
    const QFileInfo info(cached);
    if (info.isFile() && !info.isSymLink() && info.size() > 0 && info.size() <= 4 * 1024 * 1024 && info.canonicalFilePath().startsWith(account + '/')) {
        QFile file(cached); if (file.open(QIODevice::ReadOnly)) return file.readAll();
    }
    // The existing public-only fetcher enforces HTTPS/CDN redirects and limits.
    // No account cookie is passed to this independent image request.
    QProcess process; QProcessEnvironment env;
    env.insert("PATH", "/usr/bin:/bin:/usr/sbin:/sbin"); env.insert("HOME", "/home/root");
    env.insert("LANG", "C.UTF-8"); env.insert("LC_ALL", "C.UTF-8");
    process.setProcessEnvironment(env); process.setStandardErrorFile(QProcess::nullDevice());
    process.start("/home/root/xovi/exthome/appload/koreader/luajit", QStringList{installRoot + "/lua/fetch-cover.lua", url});
    need(process.waitForStarted(5000), "cover_unavailable");
    QByteArray bytes; QElapsedTimer elapsed; elapsed.start();
    while (process.state() != QProcess::NotRunning) {
        process.waitForReadyRead(100); bytes += process.readAllStandardOutput();
        if (canceled || elapsed.elapsed() > 65000 || bytes.size() > 4 * 1024 * 1024 + 128) { process.kill(); process.waitForFinished(1000); throw std::runtime_error("cover_unavailable"); }
    }
    bytes += process.readAllStandardOutput();
    const QByteArray marker("WRDP-COVER\n"); const auto end = bytes.indexOf('\n', marker.size());
    bool valid = false; const auto count = bytes.mid(marker.size(), end - marker.size()).toInt(&valid);
    need(process.exitCode() == 0 && process.exitStatus() == QProcess::NormalExit && bytes.startsWith(marker) && end > 0 && valid && count > 0 && count <= 4 * 1024 * 1024 && bytes.size() == end + 1 + count, "cover_unavailable");
    const auto image = bytes.mid(end + 1);
    need(image.startsWith(QByteArray::fromHex("89504e470d0a1a0a")) || image.startsWith(QByteArray::fromHex("ffd8ff")), "cover_unavailable");
    return image;
}
bool restoreProgress(const QString &id, bool afterImport) {
    auto result = helper("restore-progress", id);
    auto failed = [&] {
        if (afterImport) emitRow({{"state","restore_skipped"}});
        else emitRow({{"state","error"},{"code","progress_not_located"}});
        return false;
    };
    if (result.value("state") != "restore_position") {
        if (!afterImport && result.value("state") == "error") { emitRow(result); return false; }
        return failed();
    }
    const auto uuid = result.value("uuid").toString();
    if (!QRegularExpression("\\A[0-9a-fA-F-]{36}\\z").match(uuid).hasMatch()) return failed();
    const auto base = libraryRoot+'/'+uuid;
    const int page = result.value("page").toInt(-1), count = result.value("pageCount").toInt();
    if (page < 0 || page >= count || count > 5000) return failed();
    for (const auto &ext : {"metadata", "content"}) {
        const auto path=base+'.'+ext; QFile file(path);
        if (QFileInfo(path).isSymLink() || !file.open(QIODevice::ReadOnly) || file.size()>8*1024*1024) return failed();
        const auto hash=QString::fromLatin1(QCryptographicHash::hash(file.readAll(),QCryptographicHash::Sha256).toHex());
        if(hash!=result.value(QString::fromLatin1(ext)+"Hash").toString()) return failed();
    }
    emitRow(result);
#ifdef __linux__
    pollfd input{STDIN_FILENO,POLLIN,0};char command[4]{};
    if (::poll(&input,1,15000)<=0 || ::read(STDIN_FILENO,command,3)!=3 || QByteArray(command,3)!="ok\n") return failed();
#else
    return failed();
#endif
    bool saved=false;
    for(int i=0;i<30;++i) {
        if(readJson(base+".metadata").value("lastOpenedPage").toInt(-1)==page) { saved=true;break; }
        if(canceled) return failed();
        QThread::msleep(100);
    }
    if(!saved) return failed();
    auto complete=helper("restore-complete",id);
    complete["localProgress"]=result.value("localProgress");emitRow(complete);return true;
}
int request(QNetworkAccessManager &manager, QNetworkRequest request, NativeUploadBody *body = nullptr, bool *rejected = nullptr) {
    return nativeRequest(manager, request, body, rejected, &canceled);
}
void importerHandshake() {
    emitRow({{"state", "needs_importer"}});
#ifdef __linux__
    pollfd input{STDIN_FILENO, POLLIN, 0};
    need(::poll(&input, 1, 15000) > 0 && (input.revents & POLLIN), "importer_not_enabled");
    char command[4]{};
    need(::read(STDIN_FILENO, command, 3) == 3 && QByteArray(command, 3) == "go\n", "importer_not_enabled");
#else
    throw std::runtime_error("device_required");
#endif
}
}
int main(int argc, char **argv) {
    ::umask(0077);
    std::signal(SIGTERM, onSignal); std::signal(SIGINT, onSignal);
#ifdef SIGHUP
    std::signal(SIGHUP, onSignal);
#endif
#ifdef __linux__
    const auto parent = ::getppid();
    ::prctl(PR_SET_PDEATHSIG, SIGTERM);
    if (::getppid() != parent) return 1;
#endif
    QCoreApplication app(argc, argv);
    try {
        need(argc == 2 || argc == 3 || argc == 5 || argc == 6 || argc == 7, "invalid_command");
        const auto requested = QString::fromLocal8Bit(argv[1]);
        const bool exitSync=requested=="exit-sync";
        need(exitSync?(argc>=5 && argc<=7):argc<=3,"invalid_command");
        const auto expectedScope=exitSync?QString::fromLocal8Bit(argv[3]):QString();
        const auto expectedUuid=exitSync?QString::fromLocal8Bit(argv[4]):QString();
        bool pageValid=true;const int expectedPage=exitSync && argc>=6?QString::fromLocal8Bit(argv[5]).toInt(&pageValid):-1;
        need(pageValid && expectedPage>=-1 && expectedPage<5000,"invalid_command");
        const auto openingSignature=exitSync && argc==7?QString::fromLocal8Bit(argv[6]):QString();
        need(openingSignature.isEmpty() || QRegularExpression("\\A[a-f0-9]{64}\\z").match(openingSignature).hasMatch(),"invalid_command");
        if(exitSync)need(QRegularExpression("\\A[a-f0-9]{64}\\z").match(expectedScope).hasMatch()
            && QRegularExpression("\\A[0-9a-f-]{36}\\z").match(expectedUuid).hasMatch(),"invalid_command");
        const auto operation = exitSync?QStringLiteral("sync-progress"):requested;
        need(QDir().mkpath(root) && !QFileInfo(root).isSymLink(), "private_root_unavailable");
        need(::chmod(QFile::encodeName(root).constData(), 0700) == 0, "private_root_unavailable");
        if (operation == "reading-progress") { emitRow(helper(operation)); return 0; }
        ImportRegistry registry(root); // One foreground operation, including login.
        need(NativeAddress::recover(root), "address_restore_failed");
        if (operation == "status" || operation == "login" || operation == "logout") {
            emitRow(helper(operation)); return 0;
        }
        if (operation == "shelf" || operation == "cached-shelf") {
            auto row = helper(operation);
            registry.reconcile(scan(registry, operation == "shelf" || registry.needsSourceScan()));
            auto books = row.value("books").toArray();
            for (int i = 0; i < books.size(); ++i) {
                auto book = books.at(i).toObject();
                auto state = registry.record(book.value("bookId").toString()).value("state").toString();
                book["importState"] = state.isEmpty() ? "remote" : state;
                book["localProgress"] = -1;
                auto record = registry.record(book.value("bookId").toString());
                if (state == "imported" && record.value("accountScope") == row.value("accountScope")) {
                    const auto uuid = record.value("uuid").toString();
                    try { book["localProgress"] = nativeReadingPercent(readJson(libraryRoot + '/' + uuid + ".metadata"), readJson(libraryRoot + '/' + uuid + ".content", 8*1024*1024)); }
                    catch (const std::exception &) {}
                }
                book["hasDownloadCache"] = QFileInfo(root + "/accounts/" + row.value("accountScope").toString() + "/books/" + book.value("bookId").toString()).isDir();
                books[i] = book;
            }
            row["books"] = books; emitRow(row); return 0;
        }
        need((operation == "import" || operation == "repair-cover" || operation == "clear-cache" || operation == "sync-progress" || operation == "restore-progress" || operation == "upload-time") && (exitSync || argc == 3), "invalid_command");
        const auto id = QString::fromLocal8Bit(argv[2]);
        need(Epub::validBookId(id), "invalid_book_id");
        auto identity = helper("status");
        need(identity.value("state") == "authenticated", "login_required");
        if(exitSync)need(identity.value("accountScope")==expectedScope,"account_changed");
        registry.reconcile(scan(registry));
        if(operation=="upload-time") {
            const auto record=registry.record(id);
            need((record.value("state")=="imported" || record.value("state")=="removed")
                && record.value("accountScope")==identity.value("accountScope"),"progress_not_located");
            emitRow(helper(operation,id));return 0;
        }
        if (operation == "sync-progress" || operation == "restore-progress") {
            const auto record = registry.record(id);
            need(record.value("state") == "imported" && record.value("accountScope") == identity.value("accountScope"), "progress_not_located");
            if(exitSync)need(record.value("uuid")==expectedUuid && record.value("accountScope")==expectedScope,"account_changed");
            const auto path = libraryRoot + '/' + record.value("uuid").toString() + ".epub";
            need(!QFileInfo(path).isSymLink() && Epub(path).sourceBookId() == id, "progress_not_located");
            if(exitSync && expectedPage>=0){
                const auto metadata=libraryRoot+'/'+expectedUuid+".metadata";bool settled=false;
                for(int i=0;i<50 && !canceled;++i){
                    if(readJson(metadata).value("lastOpenedPage").toInt(-1)==expectedPage){settled=true;break;}
                    QThread::msleep(100);
                }
                need(settled && !canceled,"progress_local_changed");
            }
            if (operation == "restore-progress") {restoreProgress(id, false);return 0;}
            const auto base=libraryRoot+'/'+record.value("uuid").toString();
            const auto signature=exitSync?exitPositionSignature(base):QString();
            const auto account=root+"/accounts/"+identity.value("accountScope").toString();
            // Compare this reading session's opening/closing position. Never
            // skip a move back to a formerly uploaded page merely because an
            // old upload receipt happens to match it.
            const bool unchanged=exitSync && !signature.isEmpty() && signature==openingSignature;
            if(unchanged){
                const auto localPath=account+"/local-reading-time-v1.json",sentPath=account+"/uploaded-reading-time-v1.json";
                const auto local=QFileInfo::exists(localPath)?readJson(localPath,8*1024*1024).value("books").toObject().value(id).toObject():QJsonObject();
                const auto sent=QFileInfo::exists(sentPath)?readJson(sentPath,8*1024*1024).value("books").toObject().value(id).toObject():QJsonObject();
                const auto date=QDate::currentDate().toString(Qt::ISODate);
                const auto pending=local.value("days").toObject().value(date).toDouble()-sent.value("confirmedDays").toObject().value(date).toDouble();
                bool timeOk=!sent.value("pending").isObject();
                if(pending>0 && timeOk)timeOk=helper("upload-time",id,expectedScope).value("state")=="time_uploaded";
                emitRow({{"state","exit_sync_skipped"},{"bookId",id},{"uuid",record.value("uuid")},
                    {"accountScope",identity.value("accountScope")},{"timeUploaded",timeOk},{"recentMarked",false}});return 0;
            }
            auto synced=helper(operation,id,expectedScope);
            emitRow(synced);return 0;
        }
        if (operation == "clear-cache") {
            need(registry.record(id).value("state") == "removed", "book_not_removed");
            const auto scope = identity.value("accountScope").toString();
            need(QRegularExpression("\\A[a-f0-9]{64}\\z").match(scope).hasMatch(), "invalid_account_scope");
            need(!QFileInfo(root + "/accounts").isSymLink(), "invalid_cache_path");
            clearDownloadCache(root + "/accounts/" + scope, id);
            emitRow({{"state", "cache_cleared"}}); return 0;
        }
        if (operation == "import") registry.reserve(id, identity.value("accountScope").toString());
        QString path;
        try {
            need(QStorageInfo(root).bytesAvailable() >= 256 * 1024 * 1024, "insufficient_space");
            emitRow({{"state", "downloading"}});
            const auto downloaded = helper("download", id);
            const auto scope = downloaded.value("accountScope").toString();
            need(scope == identity.value("accountScope").toString(), "account_changed");
            const auto cache = QFileInfo(root + "/accounts/" + scope + "/books").canonicalFilePath();
            const auto bookRoot = QFileInfo(cache + '/' + id).canonicalFilePath();
            const auto source = QFileInfo(downloaded.value("path").toString()).canonicalFilePath();
            need(!cache.isEmpty() && !bookRoot.isEmpty() && bookRoot.startsWith(cache + '/')
                && source.startsWith(bookRoot + '/') && !source.isEmpty(), "invalid_download_path");
            emitRow({{"state", "validating"}});
            const auto coverUrl = downloaded.value("coverUrl").toString();
            if (!Epub(source).hasCover() && !coverUrl.isEmpty()) {
                Epub(source).stamp(id, source, coverImage(scope, id, coverUrl));
                need(Epub(source).hasCover(), "cover_unavailable");
            }
            if (operation == "repair-cover") { emitRow({{"state", "cover_repaired"}, {"hasCover", Epub(source).hasCover()}}); return 0; }
            path = root + "/submission-" + id + ".epub";
            Epub(source).stamp(id, path);
            registry.reconcile(scan(registry)); // Catch concurrent external import.
            need(registry.record(id).value("state") == "preparing", "already_imported");
            NativeAddress address(root);
            address.prepare();
            importerHandshake();
            QNetworkAccessManager manager;
            bool ready = false;
            for (int attempt = 0; attempt < 3 && !canceled; ++attempt) {
                if (request(manager, QNetworkRequest(QUrl("http://10.11.99.1/"))) == 200) { ready = true; break; }
                QThread::msleep(500);
            }
            need(ready, "native_importer_unavailable");
            // No asynchronous event can alter the source file between validation
            // and opening: it is private, generated by this exclusive worker.
            auto *multipart = new NativeUploadBody(path, id);
            need(multipart->isOpen(), "submission_unavailable");
            need(helper("prepare-restore", id).value("state") == "restore_prepared", "progress_cache_unavailable");
            registry.prepareSubmission(id, path);
            emitRow({{"state", "importing"}});
            bool rejected = false;
            const int status = request(manager, QNetworkRequest(QUrl("http://10.11.99.1/upload")), multipart, &rejected);
            if (rejected) {
                registry.rejectSubmission(id);
                need(address.restore(), "address_restore_failed");
                emitRow({{"state", "error"}, {"code", "native_rejected"}, {"importState", "failed"}});
                QFile::remove(path); return 1;
            }
            if (status != 200 && status != 201) {
                need(address.restore(), "address_restore_failed");
                emitRow({{"state", "pending"}}); return 0;
            }
            for (int i = 0; i < 30; ++i) {
                registry.reconcile(scan(registry));
                if (registry.record(id).value("state") == "imported") break;
                QThread::msleep(1000);
            }
            emitRow({{"state", registry.record(id).value("state")},{"uuid",registry.record(id).value("uuid")}});
            need(address.restore(), "address_restore_failed");
            QFile::remove(path);
            if (registry.record(id).value("state") == "imported") {
                try { restoreProgress(id, true); } catch (const std::exception &) { emitRow({{"state","restore_skipped"}}); }
                bool marked=false;
                try { marked=helper("mark-recent",id).value("state")=="recent_marked"; } catch(const std::exception &) {}
                emitRow({{"state",marked ? "recent_marked" : "recent_mark_failed"}});
            }
        } catch (const std::exception &error) {
            if (registry.record(id).value("state") == "preparing") registry.failBeforeSubmission(id);
            if (!path.isEmpty()) QFile::remove(path);
            if (QString::fromUtf8(error.what()) == "cover_unavailable" && registry.record(id).value("state") == "failed") {
                emitRow({{"state", "error"}, {"code", "cover_unavailable"}, {"importState", "failed"}}); return 1;
            }
            throw;
        }
        return 0;
    } catch (const std::exception &error) {
        // Only locally defined codes may reach the UI; helper/native raw output
        // never becomes an exception string on this boundary.
        const auto text = QString::fromUtf8(error.what());
        const bool code = QRegularExpression("\\A[a-z_]{1,64}\\z").match(text).hasMatch();
        emitRow({{"state", "error"}, {"code", code ? text : QStringLiteral("operation_failed")}});
        return 1;
    }
}
