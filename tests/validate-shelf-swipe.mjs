import fs from 'node:fs';

function assert(condition, message) {
  if (!condition) throw new Error(message);
}

const qml = fs.readFileSync('apps/weread-qt/Main.qml', 'utf8');
const gridStart = qml.indexOf('id: shelfGrid');
const gridEnd = qml.indexOf('id: emptyShelfState', gridStart);
const grid = qml.slice(gridStart, gridEnd);

assert(gridStart >= 0 && gridEnd > gridStart, 'shelf grid must exist');
assert(grid.includes('id: shelfPageSwipeHandler'), 'shelf grid must have a dedicated swipe handler');
assert(grid.includes('acceptedDevices: PointerDevice.TouchScreen | PointerDevice.Mouse'), 'shelf swipe must accept the Move touch device even when Qt exposes it as a mouse pointer');
assert(grid.includes('grabPermissions: PointerHandler.CanTakeOverFromAnything'), 'shelf swipe must take over after a cover press becomes a horizontal drag');
assert(grid.includes('target: null'), 'shelf swipe must detect gestures without moving the grid');
assert(grid.includes('PageGesture.swipeDelta(swipeDx, swipeDy)') && grid.includes('swipeDx = activeTranslation.x') && grid.includes('swipeDy = activeTranslation.y'), 'shelf must retain the last active displacement because device Qt resets it before release');
assert(grid.includes('TapHandler {') && grid.includes('gesturePolicy: TapHandler.DragThreshold'), 'cover taps must cancel after a drag rather than exclusively holding the swipe');
assert(!grid.includes('MouseArea {\n\t\t                        anchors.fill: parent'), 'cover delegates must not monopolize touch with the old mouse area');
assert(!grid.includes('NumberAnimation') && !grid.includes('PropertyAnimation'), 'shelf page changes must not animate on e-ink');

console.log('shelf swipe ok');

const vm = await import('node:vm');
const gesture = {};
vm.runInNewContext(fs.readFileSync('apps/weread-qt/PageGesture.js', 'utf8'), gesture);
for (const side of ['left', 'right']) {
  assert(gesture.readerDelta(side, -60, 25) === 1, 'left swipe must advance from either screen half');
  assert(gesture.readerDelta(side, 60, -25) === -1, 'right swipe must go back from either half');
  assert(gesture.readerDelta(side, -160, 120) === 1, 'diagonal but primarily horizontal swipe must work');
  assert(gesture.readerDelta(side, 25, 0) === 0, 'ambiguous short drag must not turn');
  assert(gesture.readerDelta(side, 30, 140) === 0, 'vertical gesture must not turn');
  assert(gesture.readerDelta(side, 8, -12) === (side === 'left' ? -1 : 1), 'jittery tap must keep side-based paging');
}
for (let i = 0; i < 5; ++i) {
  assert(gesture.swipeDelta(-80, 15) === 1 && gesture.swipeDelta(80, -15) === -1, 'successive opposite swipes must not accumulate');
}
assert(gesture.swipeDelta(10, 0) === 0 && gesture.swipeDelta(50, 120) === 0, 'shelf jitter and vertical gestures must not turn');
const control = fs.readFileSync('apps/weread-qt/app_control.cpp', 'utf8');
const inject = control.slice(control.indexOf('bool AppControl::injectSelfTestSwipe'));
assert(inject.includes('RM_WEREAD_QT_SELFTEST") != QStringLiteral("exit-ui")'), 'pointer injection must be unavailable during normal reading');
assert(inject.includes('QEvent::MouseButtonPress') && inject.includes('QEvent::MouseMove') && inject.includes('QEvent::MouseButtonRelease'), 'device routing test must deliver a complete pointer sequence');
assert(qml.includes('shelf-swipe-selftest=') && qml.includes('reader-swipe-selftest='), 'device tests must assert shelf and reader navigation independently');
