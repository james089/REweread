import assert from 'node:assert/strict';
import { mkdtempSync, mkdirSync, writeFileSync, chmodSync, existsSync, readFileSync, rmSync, symlinkSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { execFileSync, spawn, spawnSync } from 'node:child_process';

const root = mkdtempSync(join(tmpdir(), 'xovi-guard-'));
const guard = join(process.cwd(), 'scripts/xovi-guarded-start.sh');
const sh = '/bin/sh';
const hash = p => execFileSync('shasum', ['-a', '256', p], { encoding: 'utf8' }).split(/\s+/)[0];
const treeHash = dir => execFileSync('sh', ['-c', 'cd "$1" && find . -type f -print | LC_ALL=C sort | while IFS= read -r item; do shasum -a 256 "$item"; done | shasum -a 256 | awk \'{print $1}\'', 'sh', dir], { encoding: 'utf8' }).trim();
const put = (p, s, mode) => { mkdirSync(join(p, '..'), { recursive: true }); writeFileSync(p, s); if (mode) chmodSync(p, mode); };
const xovi = join(root, 'xovi');
const os = join(root, 'version');
const stock = join(root, 'stock');
const active = join(xovi, 'services/xochitl.service/extensions.d');
const resources = join(xovi, 'exthome/qt-resource-rebuilder');
const receipt = join(xovi, 'compatibility-receipt.env');
const marker = join(xovi, '.xovi-injection-in-flight');
put(os, '3.27.3.0\n');
put(stock, '#!/bin/sh\nprintf "stock:$LD_PRELOAD:$XOVI_ROOT:$*\\n" >> "$XOVI_GUARD_LOG"\nexit 0\n', 0o755);
put(join(xovi, 'xovi.so'), 'xovi');
put(join(xovi, 'extensions.d/qt-resource-rebuilder.so'), 'qrr');
put(join(active, 'appload.so'), 'appload');
put(join(active, 'qt-resource-rebuilder.so'), 'qrr');
put(join(resources, 'hashtab'), 'hashtab');
put(join(resources, 'resource.rcc'), 'rcc');
const activeLink = join(root, 'active-link');
const resourceLink = join(root, 'resource-link');
symlinkSync(active, activeLink);
symlinkSync(resources, resourceLink);
const baseEnv = { ...process.env, XOVI_GUARD_ROOT: xovi, XOVI_GUARD_STOCK_BIN: stock, XOVI_GUARD_OS_ID_FILE: os, XOVI_GUARD_RECEIPT: receipt, XOVI_GUARD_MARKER: marker, XOVI_GUARD_ACTIVE_EXTENSIONS: activeLink, XOVI_GUARD_RESOURCE_TREE: resourceLink, XOVI_GUARD_TEST_MODE: '1', XOVI_GUARD_CHILD_CMD: stock, XOVI_GUARD_LOG: join(root, 'log') };
function receiptText() { return [
  'format=1', `os_identity=${readFileSync(os, 'utf8').trim()}`, `os_identity_sha256=${hash(os)}`,
  `xochitl_sha256=${hash(stock)}`, `xovi_sha256=${hash(join(xovi, 'xovi.so'))}`,
  `appload_sha256=${hash(join(active, 'appload.so'))}`, `qrr_sha256=${hash(join(xovi, 'extensions.d/qt-resource-rebuilder.so'))}`,
  `resource_inventory_sha256=${treeHash(resources)}`,
].join('\n') + '\n'; }
function run(args = [], env = baseEnv) { return spawnSync(sh, [guard, ...args], { env, encoding: 'utf8' }); }
function installReceipt() { put(receipt, receiptText()); }

let r = run(['--check']);
assert.notEqual(r.status, 0, 'missing receipt must fail closed');
assert.match(r.stderr, /not an exact approved match/);
installReceipt();
r = run(['--check']);
assert.equal(r.status, 0, `exact receipt should pass read-only preflight: ${r.stderr}`);
rmSync(join(resources, 'hashtab'));
r = run(['--check']);
assert.notEqual(r.status, 0, 'missing hashtab must fail');
put(join(resources, 'hashtab'), 'hashtab'); installReceipt();

writeFileSync(os, '3.28.0.172\n');
r = run(['--check']);
assert.notEqual(r.status, 0, 'OS identity mutation must fail');
writeFileSync(os, '3.27.3.0\n'); installReceipt();
writeFileSync(join(active, 'appload.so'), 'changed');
r = run(['--check']);
assert.notEqual(r.status, 0, 'active AppLoad mutation must fail');
writeFileSync(join(active, 'appload.so'), 'appload'); installReceipt();
writeFileSync(join(active, 'qt-resource-rebuilder.so'), 'changed');
r = run(['--check']);
assert.notEqual(r.status, 0, 'active resource-rebuilder mutation must fail');
writeFileSync(join(active, 'qt-resource-rebuilder.so'), 'qrr'); installReceipt();
writeFileSync(join(resources, 'resource.rcc'), 'changed');
r = run(['--check']);
assert.notEqual(r.status, 0, 'resource inventory mutation must fail');
writeFileSync(join(resources, 'resource.rcc'), 'rcc'); installReceipt();
put(join(active, 'unreviewed.so'), 'unreviewed');
r = run(['--check']);
assert.notEqual(r.status, 0, 'unreviewed active extension must fail');
rmSync(join(active, 'unreviewed.so')); installReceipt();

put(marker, 'stale\n');
r = run();
assert.equal(r.status, 0, 'stale marker must execute stock GUI');
assert.match(readFileSync(baseEnv.XOVI_GUARD_LOG, 'utf8'), /stock::/);
assert.ok(existsSync(marker), 'stale marker must remain for explicit recovery');
rmSync(marker);
// A normal injected child exits cleanly and clears the newly-created marker.
r = run(['--system']);
assert.equal(r.status, 0, `clean injected child exits successfully: ${r.stderr}`);
assert.ok(!existsSync(marker), 'clean child must clear in-flight marker after exit');
assert.match(readFileSync(baseEnv.XOVI_GUARD_LOG, 'utf8'), /:--system\n/, '--system must reach xochitl unchanged');

const crash = join(root, 'crash');
put(crash, '#!/bin/sh\nexit 7\n', 0o755);
r = run([], { ...baseEnv, XOVI_GUARD_CHILD_CMD: crash });
assert.equal(r.status, 0, 'crash must be hidden behind stock fallback');
assert.ok(existsSync(marker), 'crash must preserve marker to block next injection');
rmSync(marker);

const sleeper = join(root, 'sleeper');
put(sleeper, '#!/bin/sh\ntrap "exit 0" TERM\nwhile :; do sleep 1; done\n', 0o755);
const child = spawn(sh, [guard], { env: { ...baseEnv, XOVI_GUARD_CHILD_CMD: sleeper }, stdio: 'ignore' });
await new Promise(resolve => setTimeout(resolve, 120));
child.kill('SIGTERM');
const status = await new Promise(resolve => child.on('exit', resolve));
assert.equal(status, 0, 'SIGTERM should be forwarded and return normally');
assert.ok(!existsSync(marker), 'orderly SIGTERM must clear marker only after the child exits');

const blockingParent = join(root, 'not-a-directory');
put(blockingParent, 'file');
r = run([], { ...baseEnv, XOVI_GUARD_MARKER: join(blockingParent, 'marker') });
assert.equal(r.status, 0, `unwritable marker path must fall back to stock GUI: ${r.stderr}`);

console.log('xovi guarded start ok');
