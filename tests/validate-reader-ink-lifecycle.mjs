import assert from 'node:assert/strict';
import fs from 'node:fs';

const qml = fs.readFileSync('apps/weread-qt/Main.qml', 'utf8');
const main = fs.readFileSync('apps/weread-qt/main.cpp', 'utf8');
const control = fs.readFileSync('apps/weread-qt/app_control.cpp', 'utf8');
const ocr = fs.readFileSync('apps/weread-qt/ocr_store.cpp', 'utf8');
const ai = fs.readFileSync('apps/weread-qt/ai_reply_store.cpp', 'utf8');

assert(qml.includes('property string readerInkViewBookId'), 'reader ink must track its visible book/page key');
assert(qml.includes('readonly property string selfTestMode') && qml.includes('runtimeSelfTestMode'),
  'root.selfTestMode must be a real QML property rather than an undefined context lookup');
assert(main.includes('setContextProperty(\n        "runtimeSelfTestMode"'),
  'main must provide the distinct runtime self-test context property');
assert(qml.includes('function prepareReaderInkPageChange') && qml.includes('readerInkCanvas.clearLive()'),
  'a page-cache refresh must clear old live ink before changing rendered body fields');
assert(qml.includes('function loadReaderInkForCurrentPage') && qml.includes('readerStore.loadStrokesForPage(root.currentBookId, root.pageIndex)'),
  'every activated reader page must load its matching stroke cache');
assert(qml.includes('readerStore.strokesBookId === root.currentBookId') && qml.includes('readerStore.strokesPageIndex'),
  'visible strokes must reject a stale ReaderStore page cache');
assert(qml.includes('root.readerSuppressPageTurnUntilMs = Date.now() + 900'),
  'annotation pen press must suppress the following synthetic short-tap page turn');
assert(qml.includes('selfTestMode === "reader-ink"') && qml.includes('reader-ink-selftest=') && qml.includes('root.setReaderPage(1)'),
  'reader ink self-test must write, navigate, erase and clear through real Main.qml paths');
assert(qml.includes('readerInkSelfTestDigest') && qml.includes('samples: ['),
  'reader ink self-test must validate multiple framebuffer regions, including body content');
assert(main.includes('QTemporaryDir') && main.includes('prepareReaderInkSelfTestBook') && main.includes('RM_WEREAD_DATA_DIR'),
  'reader ink self-test must force a temporary data directory and a synthetic EPUB');
assert(main.includes('RM_WEREAD_SESSION_PATH') && main.includes('logged_out'),
  'reader ink self-test must override the session path with a temporary logged-out file');
assert(qml.includes('if (root.selfTestMode !== "reader-ink") {\n            readingTimeStore.reloadAccount()'),
  'reader ink self-test must not start account/session helpers at QML startup');
assert(ocr.includes('qgetenv("RM_WEREAD_DATA_DIR")') && ai.includes('qgetenv("RM_WEREAD_DATA_DIR")'),
  'OCR and AI credentials must honor the isolated reader-ink data directory');
assert(control.includes('RM_WEREAD_QT_SELFTEST") != QStringLiteral("reader-ink")') && control.includes('QCryptographicHash::Sha256'),
  'framebuffer digest must be scoped to reader-ink test mode and never expose pixels');

console.log('reader ink lifecycle source contracts ok');
