import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {spawnSync} from 'node:child_process';
import assert from 'node:assert/strict';

const work = fs.mkdtempSync(path.join(os.tmpdir(), 'reweread-sdk-version-'));
try {
  fs.mkdirSync(path.join(work, 'scripts'));
  fs.mkdirSync(path.join(work, 'sdk'));
  fs.mkdirSync(path.join(work, 'bin'));
  fs.mkdirSync(path.join(work, 'apps/weread-qt/build'), {recursive: true});
  const retained = path.join(work, 'apps/weread-qt/build/retained');
  fs.writeFileSync(retained, 'existing good build');
  fs.copyFileSync('scripts/build-weread-qt.sh', path.join(work, 'scripts/build-weread-qt.sh'));
  fs.writeFileSync(path.join(work, 'scripts/fetch-build-dependencies.sh'), '#!/bin/sh\nexit 0\n', {mode: 0o755});
  fs.writeFileSync(path.join(work, 'sdk/environment-setup-cortexa55-remarkable-linux'), 'qmake() { echo 6.8.2; }\n');
  fs.writeFileSync(path.join(work, 'bin/cmake'), '#!/bin/sh\nexit 0\n', {mode: 0o755});
  const result = spawnSync('bash', [path.join(work, 'scripts/build-weread-qt.sh')], {
    env: {...process.env, SDK_PATH: path.join(work, 'sdk'), PATH: `${path.join(work, 'bin')}:${process.env.PATH}`},
    encoding: 'utf8',
  });
  assert.notEqual(result.status, 0, 'an SDK with incompatible QML bytecode must be rejected');
  assert.match(result.stdout + result.stderr, /6\.10\.3/);
  assert(fs.existsSync(retained), 'SDK rejection must preserve the previous build');
  console.log('SDK version guard behavior ok');
} finally {
  fs.rmSync(work, {recursive: true, force: true});
}
