#include "app_control.h"
#include "direct_ink_framebuffer.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QImage>
#include <QSaveFile>

AppControl::AppControl(QObject *parent)
    : QObject(parent) {
}

void AppControl::quitToSystem() {
    QCoreApplication::quit();
}

bool AppControl::saveSelfTestScreen() {
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
    QSaveFile output("/tmp/rm-weread-selftest-screen.png");
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
