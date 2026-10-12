import fs from 'node:fs';

function read(path) {
  return fs.readFileSync(path, 'utf8');
}

function assert(condition, message) {
  if (!condition) throw new Error(message);
}

const cache = read('apps/weread-move/lib/chapter_cache.lua');
const manager = read('apps/weread-move/lib/download_manager.lua');
const sha256 = read('apps/weread-move/lib/sha256.lua');
const runner = read('tests/run-all.mjs');

assert(cache.includes('function ChapterCache:write_chapter'), 'chapter cache must write individual chapter XHTML');
assert(cache.includes('function ChapterCache:chapters_dir'), 'chapter cache must expose a per-book chapters directory');
assert(cache.includes('"chapters"'), 'chapter cache must store chapters under a chapters directory');
assert(cache.includes('os.rename'), 'chapter cache writes must be atomic');
assert(cache.includes('function ChapterCache:load_download_manifest'), 'chapter cache must load a resumable-download manifest');
assert(cache.includes('function ChapterCache:save_download_manifest'), 'chapter cache must atomically save a resumable-download manifest');
assert(cache.includes('function ChapterCache:load_verified_download_chapter'), 'chapter cache must verify cached XHTML and assets before reuse');
assert(manager.includes('require("chapter_cache")'), 'download manager must depend on chapter cache');
assert(manager.includes('require("sha256")'), 'download manager must use the bundled SHA-256 helper for account cache isolation');
assert(manager.includes('return "sha256-" .. Sha256.hex'), 'account cache scope must be SHA-256 based');
assert(!manager.includes('return "vid-" .. checksum'), 'account cache scope must not use a collision-prone checksum');
assert(sha256.includes('function Sha256.hex'), 'bundled SHA-256 helper must expose hex hashing');
assert(manager.includes('chapter_cache:write_chapter'), 'download manager must persist chapter XHTML after successful fetch');
assert(runner.includes('validate-chapter-cache.mjs'), 'run-all must include chapter cache validation');

console.log('chapter cache ok');
