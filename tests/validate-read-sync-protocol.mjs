import { spawnSync } from 'node:child_process';
import fs from 'node:fs';

const source = fs.readFileSync('apps/weread-move/lib/read_sync_protocol.lua', 'utf8');
const helper = fs.readFileSync('apps/weread-move/tools/sync-progress.lua', 'utf8');
const smoke = fs.readFileSync('tests/read_sync_protocol_smoke.lua', 'utf8');

function assert(condition, message) {
  if (!condition) throw new Error(message);
}

assert(source.includes('function ReadSyncProtocol.utf8_prefix_bytes'), 'read sync protocol must provide a UTF-8-safe byte prefix');
assert(source.includes('function ReadSyncProtocol.elapsed_seconds'), 'read sync protocol must preserve explicit elapsed-second values');
assert(source.includes('function ReadSyncProtocol.response_succeeded'), 'read sync protocol must require a business acknowledgement');
assert(source.includes('response.succ == true or response.succ == 1') && source.includes('tonumber(response.errCode) == 0'), 'acknowledge observed Web succ=1 and legacy errCode=0 forms');
assert(source.includes('response.errCode ~= nil and tonumber(response.errCode) ~= 0') && source.includes('response.success == false or response.success == 0'), 'explicit business rejections must override success flags');
assert(source.includes('first == 0xE0') && source.includes('first == 0xED') && source.includes('first == 0xF4'), 'UTF-8 prefix must reject overlong, surrogate, and out-of-range boundaries');
assert(helper.includes('require("read_sync_protocol")'), 'sync helper must use the shared protocol guards');
assert(helper.includes('ReadSyncProtocol.utf8_prefix_bytes((arg and arg[3]) or "", 20)'), 'sync helper must not byte-split a summary');
assert(helper.includes('ReadSyncProtocol.elapsed_seconds(arg and arg[4])'), 'sync helper must preserve a zero elapsed value');
assert(helper.includes('ReadSyncProtocol.response_succeeded(response)'), 'sync helper must reject unacknowledged HTTP-success responses');
assert(smoke.includes('春风又绿江南岸') && smoke.includes('🙂') && smoke.includes('elapsed_seconds(0)'), 'Lua smoke test must cover Chinese, emoji, and zero elapsed values');

// Execute the helper's acknowledgement-to-exit behavior when Lua is available.
for (const runtime of ['luajit', 'lua']) {
  const result = spawnSync(runtime, ['tests/sync_progress_helper_smoke.lua'], {encoding:'utf8'});
  if (result.error?.code === 'ENOENT') continue;
  assert(result.status === 0, result.stderr || result.stdout);
  process.stdout.write(result.stdout);
  break;
}
console.log('read sync protocol validation ok');
