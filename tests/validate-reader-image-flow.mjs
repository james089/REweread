import fs from 'node:fs';
import vm from 'node:vm';

function assert(value, message) {
  if (!value) throw new Error(message);
}

const store = fs.readFileSync('apps/weread-qt/reader_store.cpp', 'utf8');
const qml = fs.readFileSync('apps/weread-qt/Main.qml', 'utf8');

assert(store.includes('imageEntriesFromChapter'), 'ReaderStore must retain ordered image entries, not only a URL list');
assert(!store.includes('seenImages'), 'reused image assets must remain visible at every XHTML occurrence');
assert(store.includes('plainTextFromXhtml(xhtml.left(match.capturedStart(0)))'), 'image anchors must derive from XHTML source order');
assert(store.includes('sourceTextStart'), 'trailing image source offsets must remain inspectable');
assert(store.includes('qBound(0, imageRow.value(QStringLiteral("textStart")).toInt(), text.size())'), 'trailing images must retain their exact chapter-end anchor');
assert(qml.includes('function readerTailImageForTextLength(textLength)'), 'QML must create a display-only page for trailing images');
assert(qml.includes('function readerAppendTailImagePage(starts, images, textLength)'), 'all pagination paths must append a trailing image page once');
assert(qml.includes('function readerNextImageStartAfter(start)'), 'page measurement must detect the next distinct image anchor');
assert(qml.includes('nextImageStart > safeStart && nextImageStart < measuredEnd'), 'text pages must stop before a following image anchor');
assert(qml.includes('function readerImageEntriesFor(image)'), 'QML must support grouped adjacent images');
assert(qml.includes('grouped.entries = entries'), 'same-anchor image entries must remain grouped and visible');
assert(qml.includes('readerImageTextTopFor(image)'), 'image pages must use measured proportional image height rather than a fixed top boundary');
assert(qml.includes('function readerImageGroupScale(image)'), 'same-anchor image groups must share one safe height budget');
assert(qml.includes('function readerImageGroupSpacing(image)'), 'large image groups must reduce inter-image spacing before exceeding the page');
assert(qml.includes('if (imageStart > offset)'), 'a text page must end at the next image anchor instead of moving that image to page top early');
assert(qml.includes('currentReaderImageEntries'), 'reader must render the current image group');
assert(qml.includes('model: root.currentReaderImageEntries'), 'all adjacent same-anchor images must be rendered');
assert(qml.includes('readerImageEntryHeight(modelData,\n                                                            root.readerPageImages'), 'image delegates must use the group scale, not an unscaled single-image height');
assert(qml.includes('currentReaderPageText = text.slice(start, root.currentReaderTextEnd).trim()'), 'page cache must keep the original body slice');
assert(!qml.includes('root.currentReaderImageCaption + "\\n\\n" + text.slice'), 'captions must not be injected into body text or line-box offsets');
assert(qml.includes('appControl.saveSelfTestScreen()'), 'image self-test must support an opt-in framebuffer screenshot');
assert(!qml.includes('reader-image-selftest=ok page=" + page + " source="'), 'image self-test must not log local image paths');
assert(qml.includes('root.readerFontFamily,'), 'pagination signature must include the effective font family');
assert(qml.includes(': "Noto Sans CJK SC"'), 'font loader fallback must remain deterministic when a requested face is unavailable');

function groupAt(entries, start, end) {
  const first = entries.find((entry) => entry.textStart >= start && entry.textStart < end);
  return first ? entries.filter((entry) => entry.textStart === first.textStart) : [];
}

function qmlFunction(name) {
  const start = qml.indexOf(`function ${name}(`);
  assert(start >= 0, `missing QML function ${name}`);
  const brace = qml.indexOf('{', start);
  let depth = 0;
  for (let index = brace; index < qml.length; index += 1) {
    if (qml[index] === '{') depth += 1;
    if (qml[index] === '}') depth -= 1;
    if (depth === 0) return qml.slice(start, index + 1);
  }
  throw new Error(`unterminated QML function ${name}`);
}

const runtime = {
  height: 1696,
  readerContentBottom: 1400,
  readerImageTopMargin: 72,
  readerTextTopMargin: 96,
  readerStore: { imageSources: [] },
  readerEstimatedLinePixels: () => 48,
  readerTextWidth: () => 800,
};
runtime.root = runtime;
vm.createContext(runtime);
for (const name of [
  'readerImageEntrySource', 'readerImageEntryTextStart', 'readerImageEntriesFor',
  'readerImageNaturalHeight', 'readerImageGroupSpacing', 'readerImageGroupScale',
  'readerImageEntryHeight', 'readerImageTextTopFor', 'readerTailImageForTextLength',
  'readerAppendTailImagePage', 'readerNextImageStartAfter',
  'readerCompactTextLength', 'readerDocumentPositionForTextOffset',
]) vm.runInContext(qmlFunction(name), runtime);

runtime.clamp = (value, low, high) => Math.max(low, Math.min(high, value));
runtime.readerStore.bodyText = 'abcdefghij';
runtime.currentReaderTextStart = 0;
runtime.currentReaderTextEnd = 10;
runtime.currentReaderImageCaption = 'An image description not rendered in the document';
runtime.readerBodyText = { length: 10, getText: (start, end) => 'abcdefghij'.slice(start, end), positionToRectangle: () => ({}) };
assert(runtime.readerDocumentPositionForTextOffset(3) === 3,
  'image metadata captions must not shift document positions used for highlights and notes');

function tailGroup(entries, textLength) {
  return entries.filter((entry) => entry.textStart === textLength);
}

function groupLayout(naturalHeights, contentBottom, imageTop, lineHeight, spacing = 10) {
  const available = Math.max(0, contentBottom - imageTop - lineHeight * 2 - 10);
  const effectiveSpacing = naturalHeights.length < 2
    ? 0
    : Math.min(spacing, Math.floor(available / (naturalHeights.length - 1)));
  const gaps = Math.max(0, naturalHeights.length - 1) * effectiveSpacing;
  const budget = Math.max(0, available - gaps);
  const natural = naturalHeights.reduce((sum, height) => sum + height, 0);
  const scale = Math.min(1, budget / Math.max(1, natural));
  return naturalHeights.reduce((sum, height) => sum + Math.floor(height * scale), 0) + gaps + 10;
}

function pageEndBeforeImage(start, measuredEnd, entries) {
  const next = entries.map((entry) => entry.textStart)
    .filter((textStart) => textStart > start && textStart < measuredEnd)
    .sort((a, b) => a - b)[0];
  return next === undefined ? measuredEnd : next;
}

const adjacent = [
  { source: 'a', textStart: 8 },
  { source: 'a', textStart: 8 },
  { source: 'b', textStart: 21 },
];
assert(groupAt(adjacent, 0, 12).length === 2, 'two adjacent same-anchor images must form one visible group');
assert(groupAt(adjacent, 9, 24).length === 1, 'the following image must still be reachable after the group');
assert(tailGroup([...adjacent, { source: 'tail-a', textStart: 40 }, { source: 'tail-b', textStart: 40 }], 40).length === 2,
  'all trailing same-anchor images must be represented by the terminal page');
assert(tailGroup([{ source: 'cover', textStart: 0 }], 0).length === 1,
  'an image-only chapter remains representable at zero without moving its anchor');
assert(groupLayout([900, 900, 900], 1400, 72, 48) <= 1400 - 72 - 48 * 2,
  'the scaled image group plus its gaps and body gap must fit before the footer-safe text rows');
assert(groupLayout(Array(10).fill(900), 1400, 72, 48) <= 1400 - 72 - 48 * 2,
  'ten same-anchor illustrations must reduce spacing and fit within the page budget');
assert(groupLayout(Array(100).fill(900), 1400, 72, 48) <= 1400 - 72 - 48 * 2,
  'hundreds of same-anchor illustrations must never overlap the footer even when scaled to zero rows');
assert(pageEndBeforeImage(0, 60, adjacent) === 8,
  'a text page must end at the first following image anchor rather than skip it');
assert(pageEndBeforeImage(8, 60, adjacent) === 21,
  'an image page must stop at the next distinct anchor, not at a same-anchor sibling');

runtime.readerStore.imageSources = [...adjacent, { source: 'tail-a', textStart: 40, sourceOrdinal: 3 }, { source: 'tail-b', textStart: 40, sourceOrdinal: 4 }];
assert(runtime.readerTailImageForTextLength(40).entries.length === 2,
  'extracted QML tail-page function must preserve all trailing same-anchor entries');
const starts = [0, 8, 21];
const images = [{}, adjacent[0], adjacent[2]];
assert(runtime.readerAppendTailImagePage(starts, images, 40), 'extracted QML tail-page function must append a missing terminal image page');
assert(!runtime.readerAppendTailImagePage(starts, images, 40), 'extracted QML tail-page function must not duplicate a terminal image page');
assert(runtime.readerNextImageStartAfter(8) === 21, 'extracted QML next-anchor function must ignore same-anchor siblings');
for (const count of [3, 10, 100]) {
  const entries = Array.from({ length: count }, (_, index) => ({ source: `i${index}`, textStart: 8, pixelWidth: 1, pixelHeight: 900 }));
  const group = { ...entries[0], entries };
  const total = entries.reduce((sum, entry) => sum + runtime.readerImageEntryHeight(entry, group), 0)
    + runtime.readerImageGroupSpacing(group) * (count - 1) + 10;
  assert(total <= runtime.readerContentBottom - runtime.readerImageTopMargin - runtime.readerEstimatedLinePixels() * 2,
    `extracted QML group ${count} must fit before footer-safe text rows`);
}

console.log('reader image flow ok');
