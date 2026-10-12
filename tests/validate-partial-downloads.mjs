import fs from 'node:fs';

function read(path) {
  return fs.readFileSync(path, 'utf8');
}

function assert(condition, message) {
  if (!condition) throw new Error(message);
}

const manager = read('apps/weread-move/lib/download_manager.lua');
const detail = read('apps/weread-move/views/book_detail_view.lua');
const downloads = read('apps/weread-move/views/download_queue_view.lua');
const app = read('apps/weread-move/app.lua');
const downloadStore = read('apps/weread-qt/download_store.cpp');
const downloadStoreHeader = read('apps/weread-qt/download_store.h');
const shelfStore = read('apps/weread-qt/shelf_store.cpp');
const resultProtocolSmoke = read('tests/download_store_result_protocol_smoke.cpp');

assert(manager.includes('#job.failed > 0'), 'full-download finalization must inspect failed chapters');
assert(manager.includes('failedChapterCount'), 'partial downloads must persist failed chapter count');
assert(manager.includes('downloadState = partial_state'), 'partial EPUBs must not be marked full');
assert(detail.includes('self.status.downloadState == "partial"'), 'book detail must label partial full-book files distinctly');
assert(downloads.includes('已下载部分'), 'downloads list must show partial full-book files distinctly');
assert(app.includes('status.downloadState == "full"'), 'app must only short-circuit full-download action for real full downloads');
assert(manager.includes('partial_state == "full" and "full" or "partial"'), 'partial downloads must not be written as full EPUBs');
assert(manager.includes('fullFile = partial_state == "full"'), 'partial downloads must not replace the full-file status');
assert(downloadStore.includes('部分章节已下载，可继续下载'), 'Qt download completion must report readable partial downloads distinctly');
assert(!downloadStore.includes('line.startsWith(QStringLiteral("done path="))'), 'helper output must not mark an unverified EPUB as done before process completion');
assert(downloadStore.includes('hasHelperResult(QStringLiteral("full"))'), 'Qt must only mark this invocation complete from its explicit full result');
assert(downloadStore.includes('hasHelperResult(QStringLiteral("partial"))'), 'Qt must keep this invocation partial even if an older full EPUB exists');
assert(downloadStoreHeader.includes('QByteArray m_stdoutBuffer') && downloadStoreHeader.includes('QByteArray m_stderrBuffer'), 'Qt must buffer stdout and stderr bytes independently before UTF-8 decoding');
assert(downloadStore.includes('flushOutput(m_stdoutBuffer)') && downloadStore.includes('flushOutput(m_stderrBuffer)'), 'Qt must flush an unterminated final helper result during process completion');
assert(downloadStore.includes('canonicalFilePath()'), 'Qt must canonicalize the helper path before trusting it');
const finishProcess = downloadStore.slice(downloadStore.indexOf('void DownloadStore::finishProcess'), downloadStore.indexOf('void DownloadStore::setState'));
assert(!finishProcess.includes('findFullEpub(m_bookId)'), 'completion must not discover an older full EPUB and mislabel a new partial result as done');
assert(resultProtocolSmoke.includes('oldFull') && resultProtocolSmoke.includes('newPartial'), 'behavior smoke must model a previous full EPUB and a new partial EPUB');
assert(resultProtocolSmoke.includes('store.state() == QStringLiteral("partial")'), 'behavior smoke must assert the current result remains partial');
assert(resultProtocolSmoke.includes('localEpubPath")).toString() == newPartial'), 'behavior smoke must keep the current partial path active');
assert(resultProtocolSmoke.includes('outsideLink') && resultProtocolSmoke.includes('!store.hasHelperResult'), 'behavior smoke must reject a symlinked helper path outside the current book');
assert(shelfStore.includes('findPartialEpub'), 'shelf must expose readable partial EPUBs without marking them full');
assert(shelfStore.includes('downloadState = QStringLiteral("partial")'), 'shelf must label partial EPUBs as partial');

console.log('partial downloads ok');
