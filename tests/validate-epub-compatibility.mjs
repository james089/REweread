import fs from 'node:fs';
import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
const manager=fs.readFileSync('apps/weread-move/lib/download_manager.lua','utf8');
assert.equal((manager.match(/EpubCompatibility.prepare\(job.bodies, job.assets\)/g)||[]).length,2,'full/partial and opening packagers must normalize cached and fresh inputs');
assert.equal((manager.match(/job.selected, epub_bodies/g)||[]).length,2);
assert(manager.includes('"opening", epub_assets') && manager.includes('"partial", epub_assets'));
// Run behavior tests where Lua is installed; CI without Lua keeps the contract
// check, with Lua behavior also required separately before device deployment.
let ran=false;
for (const lua of ['luajit','lua']) {
 const r=spawnSync(lua,['tests/epub_compatibility_smoke.lua'],{encoding:'utf8'});
 if(r.error?.code==='ENOENT')continue;
 assert.equal(r.status,0,r.stderr||r.stdout);process.stdout.write(r.stdout);ran=true;break;
}
console.log(`EPUB packaging integration ok (Lua behavior ${ran?'passed':'requires runtime'})`);
