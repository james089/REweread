import fs from 'node:fs';
import path from 'node:path';

const root = process.cwd();
const read = (relativePath) => fs.readFileSync(path.join(root, relativePath), 'utf8');
const assert = (condition, message) => {
  if (!condition) throw new Error(message);
};

const header = read('apps/weread-qt/notes_store.h');
const source = read('apps/weread-qt/notes_store.cpp');

assert(fs.existsSync(path.join(root, 'tests/personal_notes_cache_smoke.cpp')), 'personal notes cache must include an isolated Qt account-switch smoke test');

assert(header.includes('Q_INVOKABLE void reloadAccountScope()'), 'NotesStore must expose an account-scope refresh for login/logout wiring');
assert(header.includes('QString currentAccountScope() const'), 'personal notes must derive a private account scope');
assert(header.includes('QVariantList m_personalRequestRows'), 'personal note rows must be staged until a successful helper exit');
assert(header.includes('QByteArray m_stdoutBuffer') && header.includes('QByteArray m_stderrBuffer'), 'stdout and stderr must keep independent raw-byte line buffers');
assert(source.includes('QCryptographicHash::Sha256') && source.includes('cookies.value(QStringLiteral("wr_vid"))'), 'personal cache scope must be a SHA-256 hash of the account identifier');
assert(source.includes('personal-notes-cache.json'), 'personal notes need a dedicated cache file');
assert(source.includes('QSaveFile file(path)') && source.includes('QFileDevice::ReadOwner | QFileDevice::WriteOwner'), 'personal cache must be atomically written with owner-only permissions');
assert(source.includes('session.value(QStringLiteral("logged_out")).toBool()'), 'logged-out session state must not resolve an old account cache');

const reloadStart = source.indexOf('void NotesStore::reloadAccountScope()');
const reloadEnd = source.indexOf('bool NotesStore::preparePersonalNotesScope()', reloadStart);
const reload = source.slice(reloadStart, reloadEnd);
assert(reload.includes('m_notebooks.clear()') && reload.includes('m_bookNotes.clear()') && reload.includes('m_personalRequestRows.clear()'), 'a changed account scope must immediately clear all personal-note memory');
assert(reload.includes('m_process.kill()'), 'an account switch must stop an in-flight personal request');

const finishStart = source.indexOf('void NotesStore::finishProcess');
const finishEnd = source.indexOf('void NotesStore::appendRow', finishStart);
const finish = source.slice(finishStart, finishEnd);
assert(finish.includes('exitStatus == QProcess::NormalExit && exitCode == 0 && completedPersonalRequest') && finish.includes('m_personalRequestDone && !m_personalRequestFailed') && finish.includes('m_personalRequestRows.size() == m_personalRequestExpectedCount'), 'personal cache updates require a matching done count plus a zero normal helper exit');
assert(finish.includes('m_notebooks = m_personalRequestRows') && finish.includes('m_bookNotes = m_personalRequestRows'), 'only completed staged rows may replace the displayed/cache rows');
assert(finish.includes('personalMode && !personalRequestCurrent') && finish.includes('请先登录微信读书'), 'an invalidated account request must not surface another account data');

const rowStart = source.indexOf('void NotesStore::appendRow');
const rowEnd = source.indexOf('void NotesStore::setState', rowStart);
const appendRow = source.slice(rowStart, rowEnd);
assert(appendRow.includes('m_personalRequestRows.append(row)'), 'personal helper rows must not mutate the cache-backed model before completion');
assert(appendRow.includes('!isPersonalMode()'), 'streamed personal rows must not trigger e-ink redraws while a request is incomplete');
assert(source.includes('buffer.indexOf(\'\\n\')') && source.includes('appendOutput(m_process.readAllStandardOutput(), m_stdoutBuffer, true)'), 'helper output must preserve split UTF-8/JSON rows until newline or process completion');

console.log('personal notes cache validation ok');
