#include "controller.h"
#include "qrcodegen.hpp"
#include <QCoreApplication>
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
    : QObject(parent), m_covers(this, coverRoot), m_image(image), m_workerProgram(workerProgram) {
    m_status = QStringLiteral("微信读书下载");
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
    if (busy()) {
        m_process.terminate();
        if (!m_process.waitForFinished(1500)) { m_process.kill(); m_process.waitForFinished(500); }
    }
    restoreImporter();
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
    m_opened = true; emit changed();
    if (!busy()) start("status");
}
QString DownloaderController::coverSource(const QString &id, const QString &url) {
    return m_opened && m_loggedIn ? m_covers.source(id, url) : QString();
}
void DownloaderController::closePage() {
    m_covers.stop();
    m_opened = false; cancel();
    m_qrImage.clear(); m_image->update({}); emit changed();
}
void DownloaderController::cancel() {
    clearQueue();
    if (busy()) {
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
void DownloaderController::refresh() { if (!busy()) start(m_loggedIn ? "shelf" : "status"); }
QString DownloaderController::currentTitle() const {
    for (const auto &row : m_books) {
        const auto book = row.toMap();
        if (book.value("bookId") == m_currentId) return book.value("title").toString();
    }
    return {};
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
    ++m_generation;
    m_operation = operation; m_currentId = id; m_error = false; m_final = false; m_loginRequired = false; m_refreshNeeded = false; m_buffer.clear();
    m_status = operation == "login" ? QStringLiteral("正在创建登录二维码…")
        : operation == "shelf" ? QStringLiteral("正在同步书库和导入状态…")
        : operation == "import" ? QStringLiteral("正在下载《%1》…").arg(currentTitle()) : QStringLiteral("正在检查独立账号…");
    if (operation == "import") { m_covers.pause(); bookState("preparing"); }
    QProcessEnvironment env;
    env.insert("PATH", "/usr/bin:/bin:/usr/sbin:/sbin"); env.insert("HOME", "/home/root");
    env.insert("LANG", "C.UTF-8"); env.insert("LC_ALL", "C.UTF-8");
    m_process.setProcessEnvironment(env);
    m_process.setStandardErrorFile(QProcess::nullDevice());
    m_process.setProgram(m_workerProgram);
    QStringList args{operation}; if (!id.isEmpty()) args.append(id);
    m_process.setArguments(args); m_process.start(); emit changed();
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
            if (scope != m_scope) m_books.clear();
            m_scope = scope; m_books = row.value("books").toArray().toVariantList();
            m_covers.begin(m_scope, m_books);
            ++m_coverRevision; emit coversChanged(); emit booksChanged();
            m_refreshNeeded = row.value("stale").toBool();
            m_status = row.value("cached").toBool() ? QStringLiteral("已显示本地书架，导入状态已核对") : QStringLiteral("书库已更新"); m_final = true;
        } else if (state == "progress") {
            int total = row.value("total").toInt(), done = row.value("done").toInt();
            int percent = total > 0 ? qBound(0, int(qint64(done) * 100 / total), 99) : 0;
            bookState("downloading", percent);
            m_status = QStringLiteral("正在下载《%1》：%2 / %3 章").arg(currentTitle()).arg(done).arg(total);
        } else if (state == "downloading" || state == "validating" || state == "importing") {
            bookState(state);
            m_status = state == "validating" ? QStringLiteral("正在校验《%1》…").arg(currentTitle())
                : state == "importing" ? QStringLiteral("正在导入《%1》…").arg(currentTitle()) : QStringLiteral("正在下载《%1》…").arg(currentTitle());
        } else if (state == "needs_importer") {
            if (enableImporter()) QTimer::singleShot(1000, this, [this] {
                if (busy()) m_process.write("go\n");
            });
            else { m_error = true; m_status = QStringLiteral("系统导入服务不可用或有其他传输正在运行"); m_process.terminate(); }
        } else if (state == "imported" || state == "pending" || state == "conflict") {
            bookState(state); m_final = true;
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
            m_status = row.value("code") == "native_rejected"
                ? QStringLiteral("系统拒绝了上传，EPUB 缓存已保留，可重新导入")
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
    if (!m_opened) return;
    QTimer::singleShot(0, this, [this, operation, good] {
        if (!m_opened || busy()) return;
        if (good && (operation == "login" || operation == "status" || operation == "logout")) {
            if (m_loginRequired) login();
            else if (operation == "status") start("cached-shelf"); else refresh();
        } else if (!good && m_loginRequired) login();
        else if (good && operation == "cached-shelf" && m_refreshNeeded) refresh();
        else if (good) next();
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
