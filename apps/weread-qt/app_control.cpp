#include "app_control.h"
#include "direct_ink_framebuffer.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QImage>
#include <QSaveFile>
#include <QTimer>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QWindow>

AppControl::AppControl(QObject *parent)
    : QObject(parent) {
}

void AppControl::quitToSystem() {
    if (!qEnvironmentVariableIsEmpty("RM_WEREAD_QT_SELFTEST") &&
        qEnvironmentVariable("RM_WEREAD_QT_SELFTEST") != QStringLiteral("exit-ui")) {
        QCoreApplication::quit();
        return;
    }
    if (m_quitting) return;
    m_quitting = true;
    emit quitRequested();
    // Still return to the system if the display cannot produce another frame.
    QTimer::singleShot(2500, this, &AppControl::finishQuitToSystem);
}

void AppControl::finishQuitToSystem() {
    QCoreApplication::quit();
}

bool AppControl::saveSelfTestScreen(const QString &label) {
    // Opt-in diagnostics only: copy the existing CPU framebuffer, never ask
    // the epaper scenegraph to create an unsupported render target.
    if (qEnvironmentVariableIsEmpty("RM_WEREAD_QT_SELFTEST") ||
        qEnvironmentVariable("RM_WEREAD_SELFTEST_SCREEN") != "1") return false;
    auto &buffer = DirectInkFramebuffer::instance();
    const int width = buffer.width(), height = buffer.height();
    const QByteArray pixels = buffer.snapshot(QRect(0, 0, width, height));
    if (width <= 0 || height <= 0 || pixels.size() != width * height * 4) return false;
    const QImage image(reinterpret_cast<const uchar *>(pixels.constData()),
                       width, height, width * 4, QImage::Format_RGB32);
    if (!label.isEmpty() && label != QStringLiteral("reader-preview")) return false;
    QSaveFile output(label.isEmpty() ? QStringLiteral("/tmp/rm-weread-selftest-screen.png")
        : QStringLiteral("/tmp/rm-weread-selftest-reader-preview.png"));
    if (!output.open(QIODevice::WriteOnly)) return false;
    output.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return image.save(&output, "PNG") && output.commit();
}

QString AppControl::readerInkSelfTestDigest(int x, int y, int width, int height) const {
    if (qEnvironmentVariable("RM_WEREAD_QT_SELFTEST") != QStringLiteral("reader-ink")) {
        return {};
    }
    auto &buffer = DirectInkFramebuffer::instance();
    const QRect requested(x, y, width, height);
    const QRect available(0, 0, buffer.width(), buffer.height());
    const QRect area = requested.intersected(available);
    const QByteArray pixels = buffer.snapshot(area);
    if (area.isEmpty() || pixels.size() != area.width() * area.height() * 4) {
        return {};
    }
    return QString::fromLatin1(QCryptographicHash::hash(pixels,
        QCryptographicHash::Sha256).toHex());
}

// Exercise the real Qt pointer/grab routing, only on the isolated UI fixture.
bool AppControl::injectSelfTestSwipe(int x1, int y1, int x2, int y2) {
    if (qEnvironmentVariable("RM_WEREAD_QT_SELFTEST") != QStringLiteral("exit-ui")) return false;
    const auto windows = QGuiApplication::topLevelWindows();
    QWindow *window = nullptr;
    for (auto *candidate : windows) if (candidate->isVisible()) { window = candidate; break; }
    if (!window) return false;
    auto send = [&](QEvent::Type type, QPointF point, Qt::MouseButton button, Qt::MouseButtons buttons) {
        QMouseEvent event(type, point, window->mapToGlobal(point), button, buttons, Qt::NoModifier);
        static ulong timestamp = 100;
        event.setTimestamp(timestamp += 30);
        QCoreApplication::sendEvent(window, &event);
    };
    send(QEvent::MouseButtonPress, QPointF(x1, y1), Qt::LeftButton, Qt::LeftButton);
    for (int step = 1; step <= 8; ++step)
        send(QEvent::MouseMove, QPointF(x1 + (x2 - x1) * step / 8.0, y1 + (y2 - y1) * step / 8.0), Qt::NoButton, Qt::LeftButton);
    send(QEvent::MouseButtonRelease, QPointF(x2, y2), Qt::LeftButton, Qt::NoButton);
    return true;
}
