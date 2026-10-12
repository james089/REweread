// Isolated framebuffer-overlay test. It never dlopens the device plugin or
// accesses a real framebuffer:
//   QT_PREFIX=... tests/run-direct-ink-overlay-smoke.sh

#include <QByteArray>
#include <QPointF>
#include <QRect>

#include <cassert>
#include <cstring>

#define private public
#include "direct_ink_framebuffer.h"
#undef private

namespace {
void setPixel(unsigned char *pixels, int stride, int x, int y,
              unsigned char blue, unsigned char green,
              unsigned char red, unsigned char alpha = 0xff) {
    unsigned char *pixel = pixels + y * stride + x * 4;
    pixel[0] = blue;
    pixel[1] = green;
    pixel[2] = red;
    pixel[3] = alpha;
}

bool isPixel(const unsigned char *pixels, int stride, int x, int y,
             unsigned char blue, unsigned char green, unsigned char red) {
    const unsigned char *pixel = pixels + y * stride + x * 4;
    return pixel[0] == blue && pixel[1] == green && pixel[2] == red && pixel[3] == 0xff;
}
}

int main() {
    constexpr int width = 24;
    constexpr int height = 16;
    constexpr int stride = width * 4;
    QByteArray pixels(stride * height, Qt::Uninitialized);
    std::memset(pixels.data(), 0xff, size_t(pixels.size()));

    DirectInkFramebuffer framebuffer;
    framebuffer.m_available = true;
    framebuffer.m_pixels = reinterpret_cast<unsigned char *>(pixels.data());
    framebuffer.m_width = width;
    framebuffer.m_height = height;
    framebuffer.m_stride = stride;

    DirectInkFramebuffer::OverlayPixels overlay;
    QRect dirty;
    assert(framebuffer.drawBlackLineWithOverlay(QPointF(3, 5), QPointF(13, 5), 1,
                                                &overlay, 512, &dirty));
    assert(!dirty.isEmpty());
    assert(framebuffer.drawBlackLineWithOverlay(QPointF(8, 2), QPointF(8, 10), 1,
                                                &overlay, 512, &dirty));
    assert(isPixel(reinterpret_cast<const unsigned char *>(pixels.constData()), stride,
                   8, 5, 0, 0, 0));

    // A scenegraph repaint after the direct write must not be replaced by the
    // stale saved background. The restore operation only changes still-black
    // direct-ink pixels.
    setPixel(reinterpret_cast<unsigned char *>(pixels.data()), stride, 8, 5, 7, 9, 11);
    const QRect restored = framebuffer.restoreBlackOverlay(&overlay);
    assert(!restored.isEmpty());
    assert(isPixel(reinterpret_cast<const unsigned char *>(pixels.constData()), stride,
                   3, 5, 0xff, 0xff, 0xff));
    assert(isPixel(reinterpret_cast<const unsigned char *>(pixels.constData()), stride,
                   8, 5, 7, 9, 11));
    assert(overlay.originals.isEmpty());

    DirectInkFramebuffer::OverlayPixels bounded;
    const QByteArray beforeCapacityFailure = pixels;
    assert(!framebuffer.drawBlackLineWithOverlay(QPointF(1, 1), QPointF(20, 1), 2,
                                                 &bounded, 1, &dirty));
    assert(bounded.capacityExceeded);
    assert(bounded.originals.isEmpty());
    assert(pixels == beforeCapacityFailure);

    return 0;
}
