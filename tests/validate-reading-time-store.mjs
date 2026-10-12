import fs from 'node:fs';

const header = fs.readFileSync('apps/weread-qt/reading_time_store.h', 'utf8');
const source = fs.readFileSync('apps/weread-qt/reading_time_store.cpp', 'utf8');
const sessionReader = fs.readFileSync('apps/weread-qt/session_json_reader.h', 'utf8');
const notes = fs.readFileSync('apps/weread-qt/notes_store.cpp', 'utf8');
const main = fs.readFileSync('apps/weread-qt/main.cpp', 'utf8');
const cmake = fs.readFileSync('apps/weread-qt/CMakeLists.txt', 'utf8');
const qml = fs.readFileSync('apps/weread-qt/Main.qml', 'utf8');

function assert(condition, message) {
  if (!condition) throw new Error(message);
}

assert(header.includes('Q_PROPERTY(bool accountReady'), 'ledger must make account availability visible to QML');
assert(header.includes('Q_PROPERTY(bool hasUncertainSubmission'), 'ledger must expose unresolved submissions');
assert(header.includes('Q_PROPERTY(QVariantMap statistics') && header.includes('Q_PROPERTY(int accountGeneration'), 'ledger must expose scoped reading statistics and a generation guard to QML');
assert(header.includes('snapshotPending') && header.includes('pendingMillisecondsByBook'), 'ledger must snapshot and restore per-book pending duration');
assert(header.includes('recordObservedInterval') && source.includes('kMaxObservedIntervalMilliseconds = 5000'), 'statistics must accept only bounded observed clock intervals');
assert(header.includes('beginSubmission') && header.includes('completeSubmission'), 'ledger must reserve and complete a submission explicitly');
assert(source.includes('QCryptographicHash::Sha256') && source.includes('wr_vid'), 'ledger scope must derive from session wr_vid using SHA-256');
assert(!source.includes('m_vid') && !source.includes('scope.insert') && !source.includes('"scope"'), 'ledger must not retain or serialize the raw account identity');
assert(source.includes('QSaveFile') && source.includes('file.commit()'), 'ledger writes must be atomic');
assert(source.includes('kOwnerReadWrite') && source.includes('setPermissions'), 'ledger file must be owner-readable and owner-writable only');
assert(source.includes('scopeStillCurrent(m_scope)') && source.includes('ensureCurrentAccount()'), 'snapshots must recheck the active disk session before writing');
assert(source.includes('scopeBeforeSnapshot != m_scope') && source.includes('scopeBeforeSubmission != m_scope'), 'stale QML input after an account switch must be rejected rather than written to the new scope');
assert(source.includes('m_pendingMs.value(id) - reservedMs') && source.includes('m_uncertainMs.insert'), 'submission must atomically reserve pending duration as uncertain');
assert(source.includes('365LL * 24 * 60 * 60 * 1000') && source.includes('qMin(milliseconds, kMaxReasonableMilliseconds)'), 'long offline duration must saturate at an int-safe annual cap rather than clear');
assert(source.includes('if (!succeeded)') && source.includes('intentionally retained'), 'failed submission must not replay uncertain duration');
assert(source.includes('updateStatus();\n            emit changed();') && source.includes('updatedUncertain = m_uncertainMs'), 'failed results must notify the UI and successful clears must persist before mutating the live ledger');
assert(source.includes('reservation.scope == m_scope') && source.includes('loadLedgerForScope(reservation.scope'), 'late results must be scoped to their original account ledger');
assert(source.includes('emit accountReloaded()'), 'account changes must notify QML to restore the matching clock state');
assert(source.includes('statistics.json') && source.includes('saveStatisticsForScope') && source.includes('loadStatisticsForScope'), 'statistics must be persisted separately from sync reservations');
assert(source.includes('utcPlus8Day') && source.includes('nextUtcPlus8Midnight') && source.includes('8 * 60 * 60'), 'statistics must split days on fixed UTC+8 boundaries');
assert(source.includes('RM_WEREAD_QT_SELFTEST') && source.includes('bookStatisticsKey'), 'self-tests must not write statistics and book identifiers must be hashed');
assert(sessionReader.includes('QJsonParseError::IllegalUTF8String') && sessionReader.includes('QString::fromLatin1(bytes).toUtf8()'), 'session parsing may recover only from invalid UTF-8 in memory');
assert(sessionReader.includes('isAsciiDecimalIdentifier') && source.includes('parsed.usedLatin1Fallback') && notes.includes('parsed.usedLatin1Fallback'), 'UTF-8 recovery must accept only ASCII decimal identities in both scoped stores');
assert(fs.existsSync('tests/reading_time_store_smoke.cpp'), 'ledger must include a focused persistence and account-switch smoke test');
assert(main.includes('ReadingTimeStore readingTimeStore') && main.includes('setContextProperty("readingTimeStore"'), 'the ledger must be exposed to QML');
assert(cmake.includes('reading_time_store.cpp') && cmake.includes('reading_time_store.h'), 'the ledger must be part of the device build');
assert(qml.includes('interval: 10000') && qml.includes('root.snapshotReadingClock()'), 'active reading must snapshot the ledger every ten seconds');
assert(qml.includes('ReadingClock.acknowledge(root.readingClockState, root.currentBookId, elapsedSeconds)') && qml.includes('readingTimeStore.completeSubmission'), 'the UI must acknowledge a reservation immediately and complete only confirmed submissions');
assert(qml.includes('readingTimeStore.recordObservedInterval(accepted.book, accepted.ms,') && qml.includes('accepted.generation'), 'daily stats must receive each accepted clock interval with its original account generation');
assert(qml.includes('function snapshotReadingClock(bookId)') && qml.includes('if (root.selfTestMode !== "")'), 'self-tests must not snapshot real reading time');

console.log('reading time store source contracts ok');
