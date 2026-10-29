import { test } from 'node:test';
import assert from 'node:assert/strict';
import { mkdtemp, rm, stat, symlink, realpath } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { createServer, request } from 'node:http';
import { Lab, PrivateStore, WebClient } from './lab.mjs';
import { createApp } from './server.mjs';
import { initialState, parseReader, payload, acknowledged, sign, encodeId, unwrapResponse, receiptSummary } from './protocol.mjs';

const reader = () => ({ record: { chapterUid:3, chapterIdx:2, chapterOffset:55, progress:12,
  readingTime:100, summary:'测试文本😀' }, ps:'synthetic-ps', pc:'synthetic-pc', token:'synthetic-token' });
function fixture({ rejectEntry = false, uncertain = false, conflict = false, cancel = false, suspend = false } = {}) {
  let clock = 0, reads = 0, writes = 0;
  const records = [], calls = [];
  const client = { loggedIn:true, cookies:{ wr_vid:'synthetic', wr_skey:'synthetic' },
    async reader() { reads++; const r=reader(); if (conflict && reads>1) r.record.chapterOffset++;
      if (writes===2) r.record.readingTime=130; return r; },
    async request(path, options) { assert.match(options.referer,/^\/web\/reader\//); calls.push({ path, body:options.body }); writes++;
      if (uncertain && writes===2) throw new Error('sensitive raw response');
      return rejectEntry?{succ:0}:{succ:1}; } };
  let lab;
  lab = new Lab(client, { async write(name, value) { records.push(structuredClone(value)); } }, {
    makePayload:payload, isAcknowledged:acknowledged,
    now:()=>clock, wait: async ms => { clock+=suspend?5000:ms; if(cancel) lab.cancel(); } });
  return { lab, calls, records, client, clock:()=>clock };
}
async function settled(lab) { for(let i=0;i<150 && lab.busy;i++) await new Promise(r=>setImmediate(r)); assert.equal(lab.busy,false); }

test('explicit start is required; actual wait, one entry, one duration, three readbacks', async () => {
  const f=fixture(); assert.equal(f.calls.length,0);
  await f.lab.start('12345',30); await settled(f.lab);
  assert.equal(f.calls.length,2); assert.equal(f.calls[0].body.rt,undefined); assert.equal(f.calls[1].body.rt,30);
  assert.equal(f.lab.run.state,'done'); assert.equal(f.lab.run.increase,30); assert.equal(f.lab.run.checks.length,3);
  assert.equal(f.lab.run.positionUnchanged,true); assert.ok(f.clock()>=75000);
  assert.ok(f.records.some(r=>r.state==='entry_pending')); assert.ok(f.records.some(r=>r.state==='duration_pending'));
  assert.equal(JSON.stringify(f.lab.status()).includes('synthetic'),false);
});
test('entry rejection, cancellation, sleep or position conflict never submit duration', async () => {
  for(const options of [{rejectEntry:true},{cancel:true},{suspend:true},{conflict:true}]) {
    const f=fixture(options); await f.lab.start('12345',30); await settled(f.lab);
    assert.equal(f.calls.length,1); assert.equal(f.lab.run.state,'stopped');
    assert.equal(f.calls[0].body.rt,undefined);
  }
});
test('ambiguous duration persists pending and is never automatically retried', async () => {
  const f=fixture({uncertain:true}); await f.lab.start('12345',30); await settled(f.lab);
  assert.equal(f.calls.length,2); assert.equal(f.lab.run.error,'duration_unknown');
  assert.ok(f.records.some(r=>r.state==='duration_pending'));
  assert.equal(JSON.stringify(f.lab.status()).includes('sensitive'),false);
});
test('busy and invalid requests cannot produce concurrent writes', async () => {
  const f=fixture(); await assert.rejects(f.lab.start('bad/id',30)); await assert.rejects(f.lab.start('12345',3601));
  await f.lab.start('12345',30); await assert.rejects(f.lab.start('12345',30), /test_busy/); await settled(f.lab);
  assert.equal(f.calls.length,2);
});
test('independent QR login persists session without disclosing UID or token and never starts reading', async () => {
  const paths=[], saved=[];
  const client={cookies:{},get loggedIn(){return Boolean(this.cookies.wr_skey);},
    async save(){saved.push(structuredClone(this.cookies));},
    async request(path){paths.push(path);
      if(path==='/api/auth/getLoginUid')return {uid:'synthetic-private-uid'};
      if(path.startsWith('/api/auth/getLoginInfo?'))return {succeed:true,webLoginVid:'synthetic-vid',accessToken:'synthetic-private-token'};
      return {succ:1};
    }};
  const lab=new Lab(client,{async write(){} }); await lab.beginLogin();
  for(let i=0;i<20 && lab.login.state==='waiting';i++)await new Promise(r=>setImmediate(r));
  assert.equal(lab.login.state,'done'); assert.equal(client.loggedIn,true); assert.ok(saved.length);
  assert.equal(JSON.stringify(lab.status()).includes('synthetic'),false);
  assert.equal(paths.some(path=>path==='/web/book/read'),false); assert.equal(lab.uid,'');
});
test('saved position is parsed without executing script or choosing currentChapter', () => {
  const state={ reader:{ ...reader(), progress:{book:reader().record}, psvts:'ps', pclts:'pc', currentChapter:{chapterUid:999} } };
  const html='window.__INITIAL_STATE__ = '+JSON.stringify(state)+'; throw Error("must not execute");';
  assert.equal(parseReader(html).record.chapterUid,3);
  assert.deepEqual(initialState('window.__INITIAL_STATE__={"text":"}\\\"{"};'),{text:'}"{'});
  assert.throws(()=>parseReader('window.__INITIAL_STATE__ = {"reader":{}};'));
  assert.equal(payload('12345',reader(),null,100000).rt,undefined);
  const body=payload('12345',reader(),30,100000,5); assert.equal(body.s,sign(body));
  assert.equal(body.co,55); assert.equal(body.sm,'测试文本😀'); assert.equal(body.rt,30);
  const zeroPcHtml='window.__INITIAL_STATE__='+JSON.stringify({reader:{ progress:{book:reader().record}, psvts:'ps', pclts:0, token:'synthetic' }})+';';
  assert.equal(parseReader(zeroPcHtml).pc,0);
  assert.equal(payload('12345',parseReader(zeroPcHtml),30,100000,5).pc,encodeId(100));
  assert.equal(payload('12345',parseReader(zeroPcHtml),null,100000,5).pc,encodeId(100));
  assert.equal(payload('12345',reader(),null,100000,5).pc,'synthetic-pc');
  assert.equal(acknowledged({succ:1,errCode:9}),false); assert.equal(acknowledged({succ:1}),true);
});
test('response envelopes retain business rejection; missing flags are not described as rejection', () => {
  assert.equal(acknowledged(unwrapResponse({succ:1,data:{}})),true);
  assert.equal(acknowledged(unwrapResponse({errCode:-2010,data:{succ:1}})),false);
  assert.equal(acknowledged(unwrapResponse({errCode:0,data:{errCode:-2010}})),false);
  assert.equal(receiptSummary({}).state,'unconfirmed');
  assert.equal(receiptSummary({errCode:'-2010'}).state,'rejected');
  assert.deepEqual(receiptSummary({succ:0,errCode:-2010,message:'private-response',token:'private-token'}),
    {errCode:-2010,succ:0,state:'rejected'});
});
test('private store uses 0700/0600 and rejects symlinked account files', async t => {
  const root=await realpath(await mkdtemp(join(tmpdir(),'weread-lab-test-')));
  t.after(()=>rm(root,{recursive:true,force:true}));
  const store=new PrivateStore(root); await store.init(); await store.write('session.json',{cookies:{}});
  assert.equal((await stat(root)).mode&0o777,0o700); assert.equal((await stat(join(root,'session.json'))).mode&0o777,0o600);
  await symlink(join(root,'session.json'),join(root,'linked.json')); await assert.rejects(store.read('linked.json'));
});
test('HTTP client uses fixed origin, merges cookies privately, sanitizes network failures', async () => {
  const saved=[]; const store={async read(){return null;},async write(name,value){saved.push(structuredClone(value));}};
  const client=new WebClient(store,async(url,options)=>{
    assert.equal(new URL(url).origin,'https://weread.qq.com'); assert.equal(options.redirect,'error');
    return new Response('{"data":{"succeed":false}}',{headers:{'set-cookie':'wr_test=synthetic; Path=/; Secure'}});
  });
  await client.request('/api/auth/getLoginUid',{anonymous:true}); assert.equal(client.cookies.wr_test,'synthetic');
  await assert.rejects(client.request('//untrusted.example'));
  client.transport=async()=>{throw new Error('raw-secret');}; await assert.rejects(client.request('/'),/^Error: network_failed$/);
  assert.ok(saved.length);
});
test('loopback server blocks hostile Host and cross-origin writes; status never contains credentials', async t => {
  const probe=createServer(); await new Promise(r=>probe.listen(0,'127.0.0.1',r));
  const port=probe.address().port; await new Promise(r=>probe.close(r));
  const f=fixture(); const server=createApp(f.lab,port); await new Promise(r=>server.listen(port,'127.0.0.1',r));
  t.after(()=>new Promise(r=>server.close(r)));
  const url='http://127.0.0.1:'+port;
  const status=await (await fetch(url+'/api/status')).text(); assert.equal(status.includes('synthetic'),false);
  assert.equal((await fetch(url+'/api/cancel',{method:'POST',headers:{Origin:'https://untrusted.example'}})).status,403);
  const hostileHostStatus = await new Promise((resolve, reject) => {
    request(url+'/api/status', { headers:{ Host:'untrusted.example' } }, response => {
      response.resume(); resolve(response.statusCode);
    }).on('error', reject).end();
  });
  assert.equal(hostileHostStatus,403);
});
