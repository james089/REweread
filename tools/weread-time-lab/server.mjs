import { createServer } from 'node:http';
import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import QRCode from 'qrcode';
import { Lab, WebClient, PrivateStore, safeError } from './lab.mjs';
import { pluginPayload, pluginProtocol } from './plugin-protocol.mjs';

export function createApp(lab, port = 8787) {
  const origin = 'http://127.0.0.1:' + port;
  const assets = new Map([['/', ['index.html', 'text/html; charset=utf-8']],
    ['/ui.js', ['ui.js', 'text/javascript; charset=utf-8']], ['/style.css', ['style.css', 'text/css; charset=utf-8']]]);
  return createServer(async (req, res) => {
    const send = (status, value, type = 'application/json') => {
      res.writeHead(status, { 'Content-Type': type, 'Cache-Control': 'no-store', 'X-Content-Type-Options': 'nosniff',
        'Referrer-Policy': 'no-referrer', 'Content-Security-Policy': "default-src 'self'; img-src 'self'; style-src 'self'; script-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'" });
      res.end(type === 'application/json' ? JSON.stringify(value) : value);
    };
    if (req.headers.host !== '127.0.0.1:' + port
      || req.headers['sec-fetch-site'] === 'cross-site'
      || (req.method === 'POST' && req.headers.origin !== origin)) return send(403, { error: 'local_origin_required' });
    try {
      if (req.method === 'GET' && assets.has(req.url)) {
        const [name, type] = assets.get(req.url);
        return send(200, await readFile(new URL(name, import.meta.url)), type);
      }
      if (req.method === 'GET' && req.url === '/api/status') return send(200, lab.status());
      if (req.method === 'GET' && req.url === '/qr.svg') {
        if (!lab.uid) return send(404, { error: 'qr_unavailable' });
        // Render locally. Never send the login URL to an external QR service.
        return send(200, await QRCode.toString('https://weread.qq.com/web/confirm?' + new URLSearchParams({ uid: lab.uid }),
          { type: 'svg', margin: 3, width: 300 }), 'image/svg+xml');
      }
      if (req.method !== 'POST') return send(404, { error: 'not_found' });
      let bytes = 0, raw = '';
      for await (const chunk of req) {
        bytes += chunk.length;
        if (bytes > 4096) return send(413, { error: 'request_too_large' });
        raw += chunk;
      }
      const body = raw ? JSON.parse(raw) : {};
      if (req.url === '/api/login') { await lab.beginLogin(); return send(200, { ok: true }); }
      if (req.url === '/api/shelf') {
        if (!lab.client.loggedIn) throw new Error('login_required');
        if (lab.busy) throw new Error('test_busy');
        return send(200, { books: await lab.client.shelf() });
      }
      if (req.url === '/api/start') { await lab.start(String(body.bookId || ''), Number(body.seconds)); return send(200, { ok: true }); }
      if (req.url === '/api/experiment-enter20-report60') {
        if (lab.busy) throw new Error('test_busy');
        if (await lab.store.read('experiment-enter20-report60.json')) return send(409, {error:'experiment_already_started'});
        await lab.store.write('experiment-enter20-report60.json', {createdAt:Date.now(),bookId:String(body.bookId||''),state:'requested'});
        await lab.start(String(body.bookId||''),20,60);
        return send(200,{ok:true});
      }
      if (req.url === '/api/cancel') { lab.cancel(); return send(200, { ok: true }); }
      return send(404, { error: 'not_found' });
    } catch (error) { send(400, { error: safeError(error) }); }
  });
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  process.umask(0o077);
  try {
    const store = new PrivateStore(); await store.init();
    const client = new WebClient(store); await client.load();
    const lab = new Lab(client, store, {makePayload:pluginPayload,
      isAcknowledged:response=>pluginProtocol({action:'acknowledged',response})}), server = createApp(lab);
    server.on('error', () => { console.error('无法启动本机工具，请检查 8787 端口是否已被占用。'); process.exitCode = 1; });
    server.requestTimeout = 90000;
    server.listen(8787, '127.0.0.1', () => console.log('独立时长测试工具：http://127.0.0.1:8787'));
    for (const signal of ['SIGINT', 'SIGTERM']) process.on(signal, () => {
      lab.cancel(); server.close();
      // No write is resumed on the next launch; durable pending requests stay pending.
      setTimeout(() => process.exit(0), 1000).unref();
    });
  } catch { console.error('无法初始化私有登录存储。'); process.exitCode = 1; }
}
