#include <QCoreApplication>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSettings>
#include <QTimer>
#include <QQuickWindow>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <cstdio>
#include <memory>

#include "account_store.h"
#include "ai_reply_store.h"
#include "app_control.h"
#include "book_catalog_store.h"
#include "discover_store.h"
#include "direct_ink_framebuffer.h"
#include "ink_canvas_item.h"
#include "download_store.h"
#include "frontlight_store.h"
#include "network_store.h"
#include "notes_store.h"
#include "ocr_setup_server.h"
#include "ocr_store.h"
#include "power_store.h"
#include "progress_sync_store.h"
#include "qr_image_provider.h"
#include "reader_store.h"
#include "reading_time_store.h"
#include "shelf_store.h"
#include "stylus_store.h"

namespace {

bool writeReaderInkSelfTestFile(const QString &path, const QByteArray &contents) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(contents) == contents.size();
}

bool prepareReaderInkSelfTestBook(const QString &dataDir) {
    const QString bookDir = QDir(dataDir).filePath(QStringLiteral("books/ink-probe"));
    const QString expanded = QDir(bookDir).filePath(QStringLiteral("epub-expanded/OEBPS"));
    QByteArray body("<html><head><title>Ink probe</title></head><body><h1>Ink probe</h1>");
    const QByteArray paragraph("<p>这是隔离手写笔迹测试文本，用于生成多个真实阅读页面并验证翻页、擦除与清页。"
                               "页面内容仅存在于临时目录，不会读取或修改用户书籍。</p>");
    for (int index = 0; index < 900; ++index) {
        body.append(paragraph);
    }
    body.append("</body></html>");
    return writeReaderInkSelfTestFile(QDir(bookDir).filePath(QStringLiteral("foo.epub")), QByteArray("probe"))
        && writeReaderInkSelfTestFile(QDir(expanded).filePath(QStringLiteral("content.opf")),
                                      QByteArray("<package><metadata/></package>"))
        && writeReaderInkSelfTestFile(QDir(expanded).filePath(QStringLiteral("text/ch1.xhtml")), body);
}

} // namespace

int main(int argc, char *argv[]) {
    QElapsedTimer startupClock;
    startupClock.start();
    std::fputs("rm-weread: initializing Qt platform\n", stderr);
    QGuiApplication app(argc, argv);
    std::fputs("rm-weread: Qt platform ready\n", stderr);
    QCoreApplication::setApplicationName("WeRead Move");
    QCoreApplication::setOrganizationName("rm-weread");
    std::unique_ptr<QTemporaryDir> readerInkSelfTestDir;
    if (qEnvironmentVariable("RM_WEREAD_QT_SELFTEST") == QStringLiteral("reader-ink")) {
        // This mode is a real QML/ReaderStore exercise, but it must never
        // enumerate or write the normal rm-weread directory.
        readerInkSelfTestDir = std::make_unique<QTemporaryDir>(QStringLiteral("/tmp/rm-weread-reader-ink-XXXXXX"));
        if (!readerInkSelfTestDir->isValid()
                || !prepareReaderInkSelfTestBook(readerInkSelfTestDir->path())) {
            std::fputs("reader-ink-selftest=setup-failed\n", stderr);
            return 2;
        }
        qputenv("RM_WEREAD_DATA_DIR", readerInkSelfTestDir->path().toUtf8());
        qputenv("RM_WEREAD_SESSION_PATH", QDir(readerInkSelfTestDir->path())
                .filePath(QStringLiteral("session.json")).toUtf8());
        if (!writeReaderInkSelfTestFile(qEnvironmentVariable("RM_WEREAD_SESSION_PATH"),
                                        QByteArray("{\"logged_out\":true}"))) {
            std::fputs("reader-ink-selftest=session-setup-failed\n", stderr);
            return 2;
        }
    }
    if (!qEnvironmentVariableIsEmpty("RM_WEREAD_QT_SELFTEST")) {
        // QML's device-provided Settings type has no fileName property.
        // Isolate preferences below it so interrupted UI tests cannot change
        // the user's font, layout or persona choices.
        const QString testSettings = QStringLiteral("/tmp/rm-weread-selftest-settings");
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, testSettings);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, testSettings);
    }

    // Capture the device-owned auxiliary framebuffer before the Qt Quick
    // scenegraph asks for the same singleton. Unsupported builds safely fall
    // back to the normal QQuickPaintedItem path.
    if (qEnvironmentVariableIntValue("RM_WEREAD_DISABLE_DIRECT_INK") != 1) {
        std::fputs("rm-weread: initializing direct ink\n", stderr);
        DirectInkFramebuffer::instance().initialize();
    } else {
        std::fputs("rm-weread: direct ink disabled for compatibility diagnosis\n", stderr);
    }

    if (qEnvironmentVariable("RM_WEREAD_QT_SELFTEST") == QStringLiteral("direct-ink-probe")) {
        // No stores or user data are opened in this mode. Restore the small
        // temporary test stroke from an in-memory framebuffer snapshot.
        auto &ink = DirectInkFramebuffer::instance();
        const QRect area(40, 80, 160, 80);
        const QByteArray background = ink.snapshot(area);
        if (!ink.available() || background.size() != area.width() * area.height() * 4) {
            std::fputs("direct-ink-probe=unavailable\n", stderr);
            return 2;
        }
        DirectInkFramebuffer::OverlayPixels overlay;
        QRect dirty;
        if (!ink.drawBlackLineWithOverlay(QPointF(60, 100), QPointF(175, 140),
                                          3, &overlay, 8192, &dirty)
                || !ink.drawBlackLineWithOverlay(QPointF(60, 140), QPointF(175, 100),
                                                 3, &overlay, 8192, &dirty)) {
            ink.restoreBlackOverlay(&overlay);
            ink.refreshMonoFast(area);
            std::fputs("direct-ink-probe=draw-failed\n", stderr);
            return 3;
        }
        ink.refreshMonoFast(area);
        QTimer::singleShot(800, &app, [&app, &ink, area, background, overlay]() mutable {
            ink.restoreBlackOverlay(&overlay);
            ink.refreshMonoFast(area);
            const bool restored = ink.snapshot(area) == background;
            std::fputs(restored ? "direct-ink-probe=ok\n" : "direct-ink-probe=failed\n", stderr);
            app.exit(restored ? 0 : 3);
        });
        return app.exec();
    }

    if (qEnvironmentVariable("RM_WEREAD_QT_SELFTEST") == QStringLiteral("ink-lifecycle")) {
        // Synthetic page only: never construct account/book stores or touch
        // existing notes. Exercise the real transparent Qt canvas + vendor ink.
        QQuickWindow window;
        window.setColor(Qt::white);
        window.setGeometry(0, 0, 954, 1696);
        InkCanvasItem canvas(window.contentItem());
        canvas.setWidth(954);
        canvas.setHeight(1696);
        window.showFullScreen();
        auto &ink = DirectInkFramebuffer::instance();
        if (!ink.available()) return 2;
        const QRect area(40, 80, 180, 120);
        QByteArray baseline;
        const QVariantList points{QVariantMap{{"x", 60}, {"y", 100}},
                                  QVariantMap{{"x", 175}, {"y", 140}}};
        const QVariantList storedStroke{QVariantMap{{"points", points}, {"tool", "free"},
                                                    {"colorValue", "#000000"}, {"lineWidth", 4}}};
        int stage = 0;
        bool passed = true;
        QTimer steps;
        steps.setInterval(1800);
        QObject::connect(&steps, &QTimer::timeout, &app, [&] {
            if (stage == 0) {
                baseline = ink.snapshot(area);
                canvas.beginStroke(60, 100, Qt::black, 4);
                canvas.appendPoint(175, 140);
            } else if (stage == 1) {
                passed = passed && ink.snapshot(area) != baseline;
                canvas.clearLive();
            } else if (stage == 2) {
                passed = passed && ink.snapshot(area) == baseline;
                canvas.setStrokes(storedStroke);
            } else if (stage == 3) {
                passed = passed && ink.snapshot(area) != baseline;
                canvas.setStrokes({});
            } else if (stage == 4) {
                passed = passed && ink.snapshot(area) == baseline;
                canvas.beginStroke(60, 100, Qt::black, 4);
                canvas.appendPoint(175, 140);
                canvas.setVisible(false);
            } else if (stage == 5) {
                passed = passed && ink.snapshot(area) == baseline;
                canvas.setVisible(true);
                canvas.beginStroke(60, 100, Qt::black, 4);
                canvas.appendPoint(175, 140);
                canvas.setStrokes(storedStroke);
                canvas.finishStroke();
            } else if (stage == 6) {
                passed = passed && ink.snapshot(area) != baseline;
                std::fputs(passed ? "ink-lifecycle-finalize-handoff=ok\n"
                                  : "ink-lifecycle-finalize-handoff=failed\n", stderr);
                canvas.setStrokes({});
            } else {
                passed = passed && ink.snapshot(area) == baseline;
                std::fputs(passed ? "ink-lifecycle=ok\n" : "ink-lifecycle=failed\n", stderr);
                steps.stop();
                app.exit(passed ? 0 : 3);
            }
            std::fprintf(stderr, "ink-lifecycle-stage=%d passed=%d\n", stage, int(passed));
            ++stage;
        });
        steps.start();
        return app.exec();
    }

    ShelfStore shelfStore;
    ReaderStore readerStore;
    ReadingTimeStore readingTimeStore;
    DownloadStore downloadStore;
    DiscoverStore discoverStore;
    FrontlightStore frontlightStore;
    NetworkStore networkStore;
    NotesStore notesStore;
    OcrStore ocrStore;
    AiReplyStore aiReplyStore;
    OcrSetupServer ocrSetupServer(&ocrStore, &aiReplyStore);
    PowerStore powerStore;
    ProgressSyncStore progressSyncStore;
    AccountStore accountStore;
    BookCatalogStore bookCatalogStore;
    AppControl appControl;
    StylusStore stylusStore;

    QQmlApplicationEngine engine;
    engine.addImageProvider("wereadqr", new QrImageProvider);
    engine.rootContext()->setContextProperty("shelfStore", &shelfStore);
    engine.rootContext()->setContextProperty("readerStore", &readerStore);
    engine.rootContext()->setContextProperty("readingTimeStore", &readingTimeStore);
    engine.rootContext()->setContextProperty("downloadStore", &downloadStore);
    engine.rootContext()->setContextProperty("discoverStore", &discoverStore);
    engine.rootContext()->setContextProperty("frontlightStore", &frontlightStore);
    engine.rootContext()->setContextProperty("networkStore", &networkStore);
    engine.rootContext()->setContextProperty("notesStore", &notesStore);
    engine.rootContext()->setContextProperty("ocrStore", &ocrStore);
    engine.rootContext()->setContextProperty("aiReplyStore", &aiReplyStore);
    engine.rootContext()->setContextProperty("ocrSetupServer", &ocrSetupServer);
    engine.rootContext()->setContextProperty("powerStore", &powerStore);
    engine.rootContext()->setContextProperty("progressSyncStore", &progressSyncStore);
    engine.rootContext()->setContextProperty("accountStore", &accountStore);
    engine.rootContext()->setContextProperty("bookCatalogStore", &bookCatalogStore);
    engine.rootContext()->setContextProperty("appControl", &appControl);
    engine.rootContext()->setContextProperty("stylusStore", &stylusStore);
    engine.rootContext()->setContextProperty(
        "runtimeSelfTestMode",
        qEnvironmentVariable("RM_WEREAD_QT_SELFTEST"));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule("WeReadMove", "Main");
    std::fprintf(stderr, "rm-weread: qml ready elapsed-ms=%lld\n", startupClock.elapsed());
    if (!engine.rootObjects().isEmpty()) {
        if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {
            QObject::connect(window, &QQuickWindow::frameSwapped, &app,
                [&startupClock, reported = false]() mutable {
                    if (!reported) {
                        reported = true;
                        std::fprintf(stderr, "rm-weread: first frame elapsed-ms=%lld\n", startupClock.elapsed());
                    }
                }, Qt::QueuedConnection);
        }
    }
    return app.exec();
}
