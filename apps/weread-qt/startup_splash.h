#pragma once

#include <QElapsedTimer>
#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QQuickPaintedItem>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>

// This path deliberately constructs no account stores, helpers or Main.qml.
class StartupSplashItem final : public QQuickPaintedItem {
public:
    explicit StartupSplashItem(QQuickItem *parent) : QQuickPaintedItem(parent) {
        setOpaquePainting(true);
    }
    void paint(QPainter *painter) override {
        painter->fillRect(boundingRect(), Qt::white);
        painter->setPen(Qt::black);
        QFont font(QStringLiteral("Noto Sans CJK SC"));
        font.setPixelSize(34);
        font.setWeight(QFont::DemiBold);
        painter->setFont(font);
        painter->drawText(QRectF(24, height() / 2 - 70, width() - 48, 90),
                          Qt::AlignCenter, QStringLiteral("正在启动 REweread…"));
        font.setPixelSize(23);
        font.setWeight(QFont::Normal);
        painter->setFont(font);
        painter->drawText(QRectF(24, height() / 2 + 30, width() - 48, 60),
                          Qt::AlignCenter, QStringLiteral("请稍候"));
    }
};

inline int runStartupSplash(QGuiApplication &app) {
    QElapsedTimer clock;
    clock.start();
    QQuickWindow window;
    window.setColor(Qt::white);
    const QSize size = app.primaryScreen() ? app.primaryScreen()->size() : QSize(954, 1696);
    window.resize(size);
    StartupSplashItem item(window.contentItem());
    item.setSize(size);
    bool shown = false;
    QObject::connect(&window, &QQuickWindow::frameSwapped, &app, [&]() {
        if (shown) return;
        shown = true;
        qInfo("startup-splash frame-ms=%lld", clock.elapsed());
        // Release the display before the full app starts; E Ink retains the frame.
        QTimer::singleShot(250, &app, [&]() {
            if (qEnvironmentVariable("RM_WEREAD_QT_SELFTEST") == QStringLiteral("startup-splash")) {
                const QString preview = qEnvironmentVariable("RM_WEREAD_SPLASH_PREVIEW");
                if (!preview.isEmpty()) {
                    QImage image = window.grabWindow();
                    if (image.isNull()) {
                        image = QImage(size, QImage::Format_ARGB32_Premultiplied);
                        QPainter painter(&image);
                        item.paint(&painter);
                    }
                    qInfo("startup-splash preview-saved=%d", image.save(preview));
                }
            }
            app.quit();
        });
    });
    QTimer::singleShot(3000, &app, [&]() {
        if (!shown) { qWarning("startup-splash timeout"); app.exit(2); }
    });
    window.showFullScreen();
    return app.exec();
}
