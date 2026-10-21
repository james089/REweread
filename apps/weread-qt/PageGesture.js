// Coordinates are logical screen pixels. Ignore vertical scrolling and jitter.
function swipeDelta(dx, dy) {
    if (Math.abs(dx) < 36 || Math.abs(dx) < Math.abs(dy) * 1.15) return 0
    return dx < 0 ? 1 : -1
}
function readerDelta(side, dx, dy) {
    var swipe = swipeDelta(dx, dy)
    if (swipe !== 0) return swipe
    if (Math.abs(dx) <= 20 && Math.abs(dy) <= 20) return side === "left" ? -1 : 1
    return 0
}
