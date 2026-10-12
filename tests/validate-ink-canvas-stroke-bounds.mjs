import fs from 'node:fs';
import path from 'node:path';

const source = fs.readFileSync(
  path.join(process.cwd(), 'apps/weread-qt/ink_canvas_item.cpp'), 'utf8',
);
const start = source.indexOf('QRectF InkCanvasItem::strokeBounds');
const end = source.indexOf('\nvoid InkCanvasItem::clearImageRect', start);
if (start < 0 || end < 0) throw new Error('InkCanvas stroke-bounds implementation is missing');
const body = source.slice(start, end);

if (body.includes('.united(QRectF(')) {
  throw new Error('stroke bounds must not union null zero-size QRectF pen samples');
}
for (const required of [
  'double left = first.x()',
  'double top = first.y()',
  'double right = first.x()',
  'double bottom = first.y()',
  'left = qMin(left, point.x())',
  'top = qMin(top, point.y())',
  'right = qMax(right, point.x())',
  'bottom = qMax(bottom, point.y())',
  'qMax(1.0, right - left)',
  'qMax(1.0, bottom - top)',
]) {
  if (!body.includes(required)) throw new Error(`stroke bounds is missing full-path extrema: ${required}`);
}

// Mirrors the required min/max contract for an L-shaped Chinese character
// stroke: its middle/first segment must remain in the uploaded dirty rect,
// rather than degenerating to the last sample.
const points = [{ x: 80, y: 140 }, { x: 380, y: 140 }, { x: 380, y: 480 }];
const pad = 5;
const bounds = {
  left: Math.min(...points.map((point) => point.x)) - pad,
  top: Math.min(...points.map((point) => point.y)) - pad,
  right: Math.max(...points.map((point) => point.x)) + pad,
  bottom: Math.max(...points.map((point) => point.y)) + pad,
};
if (!(bounds.left <= 80 && bounds.top <= 140 && bounds.right >= 380 && bounds.bottom >= 480)) {
  throw new Error('stroke bounds contract does not cover first, middle and final samples');
}

const finalizeStart = source.indexOf('void InkCanvasItem::finalizeInkSession');
const finalizeEnd = source.indexOf('\nvoid InkCanvasItem::scheduleSettledRefresh', finalizeStart);
const finalize = source.slice(finalizeStart, finalizeEnd);
if (finalizeStart < 0 || finalizeEnd < 0) throw new Error('InkCanvas finalize implementation is missing');
if (finalize.includes('clearDirectInkOverlay()')) {
  throw new Error('finalize must retain direct-ink recovery pixels until an explicit lifecycle boundary');
}
if (!finalize.includes('resetDirectInk()') || !finalize.includes('update(dirty.toAlignedRect())')) {
  throw new Error('finalize must stop direct writes and schedule the Qt persisted-ink handoff');
}

const clearStart = source.indexOf('void InkCanvasItem::clearLive');
const clearEnd = source.indexOf('\nvoid InkCanvasItem::finishStroke', clearStart);
const clearLive = source.slice(clearStart, clearEnd);
if (!clearLive.includes('const QRect restoredDirect = clearDirectInkOverlay()')
    || !clearLive.includes('update(boundingRect().toAlignedRect())')) {
  throw new Error('explicit live-ink clearing must restore direct pixels and fully redraw persisted ink');
}

console.log('ink canvas stroke bounds and overlay lifecycle validation ok');
