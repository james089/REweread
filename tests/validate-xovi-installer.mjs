import assert from 'node:assert/strict';
import { mkdtempSync, writeFileSync, readFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { spawnSync } from 'node:child_process';

const dir = mkdtempSync(join(tmpdir(), 'xovi-installer-'));
try {
  const log = join(dir, 'calls');
  writeFileSync(join(dir, 'ssh'), '#!/bin/sh\necho check >> "$CALL_LOG"\nexit 42\n', { mode: 0o755 });
  writeFileSync(join(dir, 'scp'), '#!/bin/sh\necho UNSAFE_WRITE >> "$CALL_LOG"\nexit 99\n', { mode: 0o755 });
  for (const script of ['install-xovi-autostart.sh', 'install-weread-qt-appload.sh']) {
    writeFileSync(log, '');
    const result = spawnSync('/bin/bash', [`scripts/${script}`], {
      env: { ...process.env, PATH: `${dir}:${process.env.PATH}`, MOVE_HOST: 'invalid-test-target', CALL_LOG: log },
      encoding: 'utf8',
    });
    assert.equal(result.status, 42, `${script} must abort on failed read-only preflight: ${result.stderr}`);
    assert.equal(readFileSync(log, 'utf8'), 'check\n', `${script} must not upload after failed preflight`);
  }
  const source = readFileSync('scripts/install-xovi-autostart.sh', 'utf8');
  assert(source.includes('Environment="LD_PRELOAD="'));
  assert(source.includes('Environment="XOVI_ROOT="'));
  assert(!source.includes('Environment="LD_PRELOAD=/home/root/xovi/xovi.so"'));
  assert(source.includes('ExecStart=/bin/sh /home/root/xovi/reweread-guard.sh --system'));
  assert(source.indexOf("'sh -s -- --check'") < source.indexOf('"${SCP[@]}"'));
  assert(!source.includes('reboot'));
  console.log('xovi installer fail-closed checks ok');
} finally {
  rmSync(dir, { recursive: true, force: true });
}
