import fs from 'node:fs';

const header = fs.readFileSync('apps/weread-qt/progress_sync_store.h', 'utf8');
const source = fs.readFileSync('apps/weread-qt/progress_sync_store.cpp', 'utf8');
const fetchProgress = fs.readFileSync('apps/weread-move/tools/fetch-progress.lua', 'utf8');
const syncProgress = fs.readFileSync('apps/weread-move/tools/sync-progress.lua', 'utf8');
const progressPosition = fs.readFileSync('apps/weread-move/lib/progress_position.lua', 'utf8');
const readerHeader = fs.readFileSync('apps/weread-qt/reader_store.h', 'utf8');
const readerSource = fs.readFileSync('apps/weread-qt/reader_store.cpp', 'utf8');
const mainQml = fs.readFileSync('apps/weread-qt/Main.qml', 'utf8');

function assert(condition, message) {
  if (!condition) throw new Error(message);
}

assert(header.includes('progressSynced(const QString &bookId, int elapsedSeconds)'), 'successful pushes must identify the book and acknowledged elapsed seconds');
assert(header.includes('progressFinished(const QString &bookId, int elapsedSeconds, bool succeeded)'), 'every progress helper request must report a terminal outcome');
assert(header.includes('QTimer m_processTimeoutTimer'), 'progress helpers must have a bounded timeout');
assert(header.includes('processOutputLine') && header.includes('finishProcess'), 'progress helper output and completion must be handled separately');
assert(header.includes('QByteArray m_stdoutBuffer') && header.includes('QByteArray m_stderrBuffer'), 'stdout and stderr must retain independent raw-byte buffers');
assert(source.includes('appendOutput(m_stdoutBuffer, m_process.readAllStandardOutput(), true)'), 'stdout must remain raw bytes until line decoding');
assert(source.includes('appendOutput(m_stderrBuffer, m_process.readAllStandardError(), false)'), 'stderr must remain raw bytes until line decoding');
assert(source.includes('QString::fromUtf8(lineBytes)') && source.includes('buffer.indexOf(\'\\r\')'), 'progress helper output must decode UTF-8 only after a complete byte line');
assert(source.includes('!m_helperFailed') && source.includes('m_helperDone') && source.includes('exitCode == 0'), 'progress success must require done output and a normal zero exit');
assert(source.includes('QProcess::FailedToStart'), 'progress helper launch failures must surface without a false success');
assert(source.includes('m_processTimeoutTimer') && source.includes('m_timedOut'), 'progress helper timeouts must be surfaced without a false success');
assert(source.includes('emit progressSynced(m_bookId, m_elapsedSeconds)'), 'only a confirmed push may acknowledge elapsed seconds');
assert(source.indexOf('const bool succeeded') < source.indexOf('emit progressSynced(m_bookId, m_elapsedSeconds)'), 'elapsed acknowledgement must remain behind the success gate');
assert(source.includes('flushOutput(m_stdoutBuffer, true)') && source.includes('flushOutput(m_stderrBuffer, false)'), 'final output buffers must be flushed without treating stderr as a protocol');
assert(source.includes('if (parseProtocol)') && source.includes('stderr may contain arbitrary Lua/HTTP diagnostics'), 'only stdout may change progress helper protocol state');
assert(source.includes('emit progressFinished(m_bookId, m_elapsedSeconds, false)') && source.includes('emit progressFinished(m_bookId, m_elapsedSeconds, true)'), 'push failures and successes must both terminate their reservation');
assert(header.includes('progressPulledDetailed(const QString &bookId, const QVariantMap &remotePosition)'), 'manual cloud-position adoption needs a detailed pull signal');
assert(source.includes('QJsonDocument::fromJson') && source.includes('row.toVariantMap()'), 'structured helper progress output must be parsed without splitting user summaries');
assert(source.includes('emit progressPulledDetailed(m_bookId, m_pulledPosition)'), 'a detailed pull must be emitted only after helper completion');
assert(fetchProgress.includes('window.__INITIAL_STATE__') && fetchProgress.includes('progress.book') && fetchProgress.includes('ProgressPosition.from_progress_book'), 'progress pull must read one coherent saved-progress record from Web reader state');
assert(fetchProgress.includes('Json.encode') && fetchProgress.includes('chapterOffset'), 'progress pull must emit structured chapter metadata');
assert(fetchProgress.includes('ProgressPosition.from_progress_book(progress_book)') && !fetchProgress.includes('current.chapterUid or progress_book.chapterUid'), 'detailed progress must use one coherent saved-progress record rather than mixed browser state');
assert(progressPosition.includes('function ProgressPosition.from_progress_book') && progressPosition.includes('summary == ""'), 'incomplete saved-progress tuples must be rejected before detailed adoption');
assert(syncProgress.includes('[chapterUid chapterIdx chapterOffset]'), 'progress upload must document optional exact chapter metadata');
assert(syncProgress.includes('has_verified_position'), 'exact chapter metadata must be gated as one verified set');
assert(syncProgress.includes('positionMode='), 'percentage-only compatibility uploads must be explicitly distinguishable from exact uploads');
assert(syncProgress.includes('ReadSyncProtocol.utf8_prefix_bytes') && syncProgress.includes('ReadSyncProtocol.response_succeeded'), 'sync helper must preserve UTF-8 summaries and require a business acknowledgement');
assert(readerHeader.includes('textAnchorForRemoteProgress'), 'ReaderStore must expose safe remote-position anchoring');
assert(readerSource.includes('wantedSummary.size() < 4') && readerSource.includes('secondMatch'), 'remote position must reject short or ambiguous summaries');
assert(readerSource.includes('chapterOffset as a local text') || readerHeader.includes('chapterOffset as a local text'), 'ReaderStore must document that server offsets are not local offsets');
assert(readerSource.includes('numericChapterUidsFromOpf') && readerSource.includes('OEBPS/content.opf'), 'older full-book EPUBs must recover reversible numeric chapter UIDs from their OPF manifest');
assert(mainQml.includes('function onProgressPulledDetailed(bookId, remotePosition)'), 'QML must receive detailed cloud positions');
assert(mainQml.includes('readerStore.textAnchorForRemoteProgress'), 'QML must resolve cloud positions through the safe summary anchor');
assert(mainQml.includes('readerProgressPullIntent === "manual"'), 'only an explicit pull may replace an existing local position');
assert(mainQml.includes('readerManualProgressFallbackTimer'), 'manual percentage fallback must wait for the detailed signal');
assert(mainQml.includes('function onProgressFinished(bookId, elapsedSeconds, succeeded)'), 'QML must immediately retain failed time reservations as uncertain');
assert(mainQml.includes('root.selfTestMode === ""') && mainQml.includes('root.selfTestMode !== "" || root.currentBookId'), 'self-tests must not accumulate or upload real reading activity');

console.log('progress sync store validation ok');
