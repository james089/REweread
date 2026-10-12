#pragma once

#include <QPointF>
#include <QRect>
#include <QByteArray>
#include <QHash>

#include <mutex>

// Minimal direct-ink bridge for the reMarkable e-paper engine. The bridge is
// optional: desktop builds and unsupported firmware keep using Qt rendering.
class DirectInkFramebuffer final {
public:
    // Every InkCanvas owns an overlay. It records only pixels first changed
    // by direct ink, never a whole-screen snapshot.
    struct OverlayPixels {
        QHash<quint64, quint32> originals;
        bool capacityExceeded = false;
    };

    static DirectInkFramebuffer &instance();

    bool initialize();
    bool available() const;
    int width() const;
    int height() const;

    // Draws opaque black ink into the vendor-owned RGB32 auxiliary buffer.
    // The returned rectangle is in framebuffer coordinates and still needs to
    // be submitted with refreshMonoFast().
    QRect drawBlackLine(const QPointF &from, const QPointF &to, int radius);
    // Capture first-touched pixels and draw as one mutex-protected operation.
    // If the bounded overlay would overflow, no part of this line is written.
    bool drawBlackLineWithOverlay(const QPointF &from, const QPointF &to, int radius,
                                  OverlayPixels *overlay, qsizetype maximumPixels,
                                  QRect *dirty);
    // Restore only pixels still black, so an already repainted page is never
    // overwritten by stale pixels from a previous page.
    QRect restoreBlackOverlay(OverlayPixels *overlay);
    QByteArray snapshot(const QRect &rect);
    void restore(const QRect &rect, const QByteArray &pixels);
    void dissolve(const QRect &rect, const QByteArray &background, int stage, int stages);
    void refreshMonoFast(const QRect &dirty);

private:
    DirectInkFramebuffer() = default;

    void putBlackPixel(int x, int y);
    void stampBlack(int centerX, int centerY, int radius);

    mutable std::mutex m_mutex;
    bool m_attempted = false;
    bool m_available = false;
    void *m_vendorLibrary = nullptr;
    void *m_vendorInstance = nullptr;
    unsigned char *m_pixels = nullptr;
    int m_width = 0;
    int m_height = 0;
    int m_stride = 0;
    unsigned long (*m_swap)(void *, QRect, int, int, int) = nullptr;
    unsigned long (*m_swapModern)(void *, QRect, int, int) = nullptr;
};
