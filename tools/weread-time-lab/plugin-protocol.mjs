import { spawn } from 'node:child_process';
import { createHash } from 'node:crypto';
import { createInterface } from 'node:readline';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('../../', import.meta.url));
export function pluginProtocol(request) {
  return new Promise((resolveResult, reject) => {
    const process = spawn('lua', [fileURLToPath(new URL('plugin-protocol.lua', import.meta.url)),
      resolve(root, 'downloads/time-lab-runtime/protocol.lua')], { cwd:root, stdio:['pipe','pipe','pipe'] });
    let result, failed = false;
    const fail = () => { failed=true; reject(new Error('plugin_protocol_failed')); };
    const timeout = setTimeout(() => { process.kill(); fail(); }, 10000);
    process.stdin.on('error', fail); process.on('error', fail);
    // Never forward child stderr: an exception could include private arguments.
    process.stderr.resume();
    createInterface({input:process.stdout}).on('line', line => {
      try {
        const value=JSON.parse(line);
        if (['md5','sha256'].includes(value.hash) && typeof value.text==='string') {
          process.stdin.write(JSON.stringify({digest:createHash(value.hash).update(value.text).digest('hex')})+'\n');
        } else if (Object.hasOwn(value,'result')) result=value.result;
        else { process.kill(); fail(); }
      } catch { process.kill(); fail(); }
    });
    process.on('close', code => { clearTimeout(timeout); if (!failed) code===0 && result!==undefined ? resolveResult(result) : fail(); });
    process.stdin.write(JSON.stringify(request)+'\n');
  });
}
export const pluginPayload = (bookId,reader,seconds) => pluginProtocol({action:'payload',opts:{
  book_id:bookId,chapter_uid:reader.record.chapterUid,chapter_idx:reader.record.chapterIdx,
  chapter_offset:reader.record.chapterOffset,progress:reader.record.progress,summary:reader.record.summary,
  psvts:reader.ps,pclts:reader.pc,token:reader.token,...(seconds===null?{}:{elapsed_seconds:seconds}),
}});
