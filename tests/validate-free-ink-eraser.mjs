import fs from 'node:fs';

function read(file) {
  return fs.readFileSync(file, 'utf8');
}

function assert(condition, message) {
  if (!condition) throw new Error(message);
}

const qml = read('apps/weread-qt/Main.qml');
const header = read('apps/weread-qt/reader_store.h');
const source = read('apps/weread-qt/reader_store.cpp');

assert(header.includes('erasePageStrokesAtPath'),
  'ReaderStore must expose a focused free-ink eraser operation');
assert(source.includes('int ReaderStore::erasePageStrokesAtPath'),
  'ReaderStore must implement the free-ink eraser operation');
assert(source.includes('freeStrokeHitsEraser') && source.includes('lineSegmentsWithinDistance'),
  'eraser hit testing must cover both taps and swept paths');
assert(source.includes('QStringLiteral("tool")).toString() == QStringLiteral("free")'),
  'free-ink erasing must not delete other annotation kinds');
assert(source.includes('stroke.value(QStringLiteral("pageIndex")).toInt() == safePageIndex'),
  'free-ink erasing must remain page-scoped');
assert(qml.includes('root.flushPendingFreeInkStrokes()\n        readerStore.erasePageStrokesAtPath'),
  'eraser must flush queued handwriting before checking intersections');
assert(qml.includes('root.readerMarkerTool === "eraser"\n                ? root.freeNotePointFromStylus'),
  'eraser starts must use the full free-ink page area rather than body-text clamping');
assert(qml.includes('root.readerMarkerTool === "free" || root.readerMarkerTool === "eraser"'),
  'eraser moves must retain free-ink page coordinates');
assert(qml.includes('var touchedBody = false') && qml.includes('if (touchedBody)'),
  'margin erasing must not accidentally clear an edge text highlight');

console.log('free ink eraser validation ok');
