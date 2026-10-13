import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';

const qml = fs.readFileSync('apps/weread-qt/Main.qml', 'utf8');
function method(name) {
  const start = qml.indexOf(`function ${name}(`);
  assert(start >= 0, `${name} must exist`);
  const opening = qml.indexOf('{', start);
  let depth = 1, end = opening + 1;
  while (depth && end < qml.length) {
    if (qml[end] === '{') depth++;
    if (qml[end] === '}') depth--;
    end++;
  }
  return qml.slice(start, end);
}
const opened = [];
const root = {
  screenName: 'reader', currentBookId: 'reading-book',
  pendingOpenBookId: '', pendingCatalogBookId: '',
  readerPendingContinueDownload: false, readerPendingContinueBookId: '',
  enterReaderForBook: id => opened.push(id),
  enterReaderForCatalogChapter: book => opened.push(`catalog:${book.bookId}`),
  continueReaderAfterFullDownload: id => opened.push(`continue:${id}`),
};
const downloads = [];
const context = vm.createContext({root, shelfStore: {reload() {}}, downloadStore: {
  downloadOpeningChapter: id => downloads.push(id),
  downloadBook: id => downloads.push(id),
}});
const complete = vm.runInContext(`(${method('onEpubReady')})`, context);
const openingComplete = vm.runInContext(`(${method('onOpeningChapterReady')})`, context);
complete('background-book', 'Background');
assert.deepEqual(opened, [], 'background download must not replace the current reader');
root.pendingOpenBookId = 'requested-book';
complete('another-book', 'Another');
assert.deepEqual(opened, [], 'unrelated completion must not consume a pending open');
complete('requested-book', 'Requested');
assert.deepEqual(opened, ['requested-book']);
assert.equal(root.pendingOpenBookId, '', 'a completed open request must be consumed');
openingComplete('queued-opening', 'Queued');
assert.equal(opened.length, 1, 'restored opening downloads must not navigate without current intent');
root.pendingOpenBookId = 'opening-book';
openingComplete('opening-book', 'Opening');
assert.equal(opened.at(-1), 'opening-book');
root.pendingCatalogBookId = 'catalog-book';
complete('catalog-book', 'Catalog');
assert.equal(opened.at(-1), 'catalog:catalog-book');
root.readerPendingContinueDownload = true;
root.readerPendingContinueBookId = 'reading-book';
complete('reading-book', 'Reading');
assert.equal(opened.at(-1), 'continue:reading-book');
root.screenName = 'shelf';
const count = opened.length;
complete('reading-book', 'Reading');
assert.equal(opened.length, count, 'continuation completion must not reopen a reader the user left');
vm.runInContext(`(${method('openOrDownloadBook')})`, context)({bookId: 'uncached', title: 'Uncached', localEpubPath: ''});
assert.equal(root.pendingOpenBookId, 'uncached', 'read action must record an explicit open intent');
assert.equal(downloads.at(-1), 'uncached');
console.log('download navigation behavior ok');
