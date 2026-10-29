import { mkdir, lstat, readFile, writeFile, rename, realpath } from 'node:fs/promises';
import { join, resolve } from 'node:path';
import { homedir } from 'node:os';
import { randomUUID } from 'node:crypto';
import { performance } from 'node:perf_hooks';
import { UA, initialState, parseReader, readerPath, fingerprint, receiptSummary, unwrapResponse } from './protocol.mjs';
import { pluginPayload, pluginProtocol } from './plugin-protocol.mjs';

export const defaultRoot = join(homedir(), 'Library', 'Application Support', 'REweread-TimeLab');
export class PrivateStore {
  constructor(root = defaultRoot) { this.root = resolve(root); }
  async init() {
    await mkdir(this.root, { recursive: true, mode: 0o700 });
    const stat = await lstat(this.root);
    if (stat.isSymbolicLink() || !stat.isDirectory() || (stat.mode & 0o077)
      || await realpath(this.root) !== this.root) throw new Error('private_storage_required');
  }
  async read(name) {
    const path = join(this.root, name);
    try {
      const stat = await lstat(path);
      if (stat.isSymbolicLink() || !stat.isFile() || (stat.mode & 0o077) || stat.size > 8 * 1024 * 1024) throw new Error('private_storage_required');
      return JSON.parse(await readFile(path, 'utf8'));
    } catch (error) { if (error.code === 'ENOENT') return null; throw new Error('private_storage_required'); }
  }
  async write(name, data) {
    const path = join(this.root, name), temp = path + '.' + randomUUID() + '.pending';
    await writeFile(temp, JSON.stringify(data), { flag: 'wx', mode: 0o600 });
    await rename(temp, path);
  }
}
export class WebClient {
  constructor(store, transport = fetch) { this.store = store; this.transport = transport; this.cookies = {}; }
  async load() { this.cookies = (await this.store.read('session.json'))?.cookies || {}; }
  async save() { await this.store.write('session.json', { version: 1, cookies: this.cookies }); }
  get loggedIn() { return Boolean(this.cookies.wr_vid && this.cookies.wr_skey); }
  async request(path, { body, timeout = 20000, html = false, anonymous = false, referer = '/' } = {}) {
    // All authentication stays at this one fixed HTTPS origin. No redirect forwarding.
    if (!path.startsWith('/') || path.startsWith('//') || !referer.startsWith('/') || referer.startsWith('//')) throw new Error('invalid_path');
    const headers = { 'User-Agent': UA, Accept: html ? 'text/html' : 'application/json',
      Origin: 'https://weread.qq.com', Referer: 'https://weread.qq.com' + referer, 'Cache-Control': 'no-cache' };
    if (!anonymous) headers.Cookie = Object.entries(this.cookies).map(([k, v]) => k + '=' + v).join('; ');
    if (body !== undefined) headers['Content-Type'] = 'application/json';
    let response;
    try {
      response = await this.transport('https://weread.qq.com' + path, { method: body === undefined ? 'GET' : 'POST',
        headers, body: body === undefined ? undefined : JSON.stringify(body), redirect: 'error', signal: AbortSignal.timeout(timeout) });
    } catch { throw new Error('network_failed'); }
    for (const line of response.headers.getSetCookie()) {
      const pair = line.split(';')[0], separator = pair.indexOf('=');
      if (separator > 0) {
        const key = pair.slice(0, separator), value = pair.slice(separator + 1);
        if (/^[A-Za-z0-9_-]+$/.test(key) && !/[\r\n;]/.test(value)) {
          if (!value || /(?:^|;)\s*max-age=0(?:;|$)/i.test(line)) delete this.cookies[key];
          else this.cookies[key] = value;
        }
      }
    }
    await this.save();
    if (!response.ok) throw new Error([401, 403].includes(response.status) ? 'login_required' : 'remote_request_failed');
    const text = await response.text();
    if (text.length > 16 * 1024 * 1024) throw new Error('response_too_large');
    if (html) return text;
    try { return unwrapResponse(JSON.parse(text)); }
    catch { throw new Error('invalid_response'); }
  }
  async reader(id) {
    const reader = parseReader(await this.request(readerPath(id), { html: true, referer: readerPath(id) }));
    if (!reader.token) {
      const config = await this.request('/web/getConfig');
      if (typeof config.token !== 'string' || !config.token) throw new Error('reader_state_missing');
      reader.token = config.token;
    }
    return reader;
  }
  async shelf() {
    const state = initialState(await this.request('/web/shelf', { html: true })), found = new Map();
    const walk = (node, depth = 0) => {
      if (!node || typeof node !== 'object' || depth > 20) return;
      if (/^[A-Za-z0-9_-]{1,128}$/.test(String(node.bookId || '')) && typeof node.title === 'string')
        found.set(String(node.bookId), { id: String(node.bookId), title: node.title });
      for (const value of Object.values(node)) walk(value, depth + 1);
    };
    walk(state);
    if (!found.size) throw new Error('shelf_unavailable');
    return [...found.values()];
  }
}
const errorCodes = new Set(['private_storage_required', 'network_failed', 'login_required', 'remote_request_failed',
  'response_too_large', 'invalid_response', 'reader_state_missing', 'saved_progress_missing', 'shelf_unavailable',
  'otp_required', 'login_expired', 'entry_rejected', 'duration_rejected', 'position_changed', 'canceled',
  'computer_suspended', 'reader_state_missing', 'invalid_duration', 'test_busy', 'entry_unknown', 'duration_unknown', 'entry_unconfirmed', 'duration_unconfirmed']);
export const safeError = error => errorCodes.has(error?.message) ? error.message : 'operation_failed';

export class Lab {
  constructor(client, store, { now = () => performance.now(), wait = ms => new Promise(r => setTimeout(r, ms)), makePayload = pluginPayload,
    isAcknowledged = response => pluginProtocol({action:'acknowledged',response}) } = {}) {
    this.client = client; this.store = store; this.now = now; this.wait = wait;
    this.makePayload=makePayload; this.isAcknowledged=isAcknowledged;
    this.login = { state: 'idle' }; this.uid = ''; this.run = null; this.cancelRequested = false;
  }
  status() {
    const { cookies } = this.client;
    // Deliberate allowlist: never return reader HTML, login identifiers or headers.
    const run = this.run && Object.fromEntries(['id', 'state', 'seconds', 'reportedSeconds', 'elapsed', 'before', 'after', 'increase',
      'entryAcknowledged', 'durationAcknowledged', 'entryReceipt', 'durationReceipt', 'error', 'checks', 'positionUnchanged'].filter(k => this.run[k] !== undefined).map(k => [k, this.run[k]]));
    return { loggedIn: Boolean(cookies.wr_vid && cookies.wr_skey), login: this.login.state, loginError: this.login.error, run };
  }
  async beginLogin() {
    if (this.login.state === 'waiting' || this.busy) throw new Error('test_busy');
    this.login = { state: 'requesting' };
    try {
      const value = await this.client.request('/api/auth/getLoginUid', { anonymous: true });
      if (typeof value.uid !== 'string' || !value.uid) throw new Error('invalid_response');
      this.uid = value.uid; this.login = { state: 'waiting' };
      void this.pollLogin();
    } catch (e) { this.login = { state: 'error', error: safeError(e) }; throw e; }
  }
  async pollLogin() {
    const uid = this.uid, deadline = this.now() + 180000;
    try {
      while (this.now() < deadline && this.uid === uid) {
        const result = await this.client.request('/api/auth/getLoginInfo?' + new URLSearchParams({ uid, otp: '' }), { timeout: 65000 });
        const vid = result.webLoginVid || result.vid || result.userVid || this.client.cookies.wr_vid;
        if (result.succeed === true && vid && typeof result.accessToken === 'string' && result.accessToken) {
          if (/[\r\n;]/.test(String(vid) + result.accessToken)) throw new Error('invalid_response');
          this.client.cookies.wr_vid = String(vid); this.client.cookies.wr_skey = result.accessToken;
          await this.client.save();
          // Same renewal endpoint as the plugin; a renewal failure does not discard QR credentials.
          try { await this.client.request('/web/login/renewal', { body: { rq: '%2Fweb%2Fbook%2Fread', ql: false } }); } catch { /* saved QR session remains */ }
          this.login = { state: 'done' }; this.uid = ''; return;
        }
        if (result.logicCode === 'NEED_OTP') throw new Error('otp_required');
        if (result.logicCode && result.logicCode !== 'LOGIN_TIMEOUT') throw new Error('login_expired');
        await this.wait(1000);
      }
      throw new Error('login_expired');
    } catch (e) { this.uid = ''; this.login = { state: 'error', error: safeError(e) }; }
  }
  get busy() { return this.running === true; }
  async persist() { await this.store.write('run-' + this.run.id + '.json', this.run); }
  async start(id, seconds, reportedSeconds = seconds) {
    if (this.busy || this.login.state === 'waiting') throw new Error('test_busy');
    if (!this.client.loggedIn) throw new Error('login_required');
    if (!/^[A-Za-z0-9_-]{1,128}$/.test(id) || !Number.isInteger(seconds) || seconds < 10 || seconds > 3600
      || !Number.isInteger(reportedSeconds) || reportedSeconds < 1 || reportedSeconds > 3600) throw new Error('invalid_duration');
    this.cancelRequested = false; this.running = true;
    this.run = { id: randomUUID(), bookId: id, seconds, reportedSeconds, state: 'preparing', elapsed: 0, checks: [] };
    try { await this.persist(); } catch (e) { this.running = false; throw e; }
    void this.execute(id, seconds);
  }
  cancel() { this.cancelRequested = true; }
  async execute(id, seconds) {
    try {
      const before = await this.client.reader(id);
      this.run.before = before.record.readingTime;
      const original = fingerprint(before.record);
      if (this.cancelRequested) throw new Error('canceled');
      const entry = await this.makePayload(id, before, null);
      this.run.state = 'entry_pending'; await this.persist();
      let receipt;
      try { receipt = await this.client.request('/web/book/read', { body: entry, referer: readerPath(id) }); }
      catch { throw new Error('entry_unknown'); }
      this.run.entryAcknowledged = await this.isAcknowledged(receipt);
      this.run.entryReceipt = receiptSummary(receipt); await this.persist();
      if (!this.run.entryAcknowledged) throw new Error(this.run.entryReceipt.state === 'rejected' ? 'entry_rejected' : 'entry_unconfirmed');
      const started = this.now(); let lastTick = started;
      this.run.state = 'waiting'; await this.persist();
      while (this.now() - started < seconds * 1000) {
        if (this.cancelRequested) throw new Error('canceled');
        await this.wait(Math.min(500, seconds * 1000 - (this.now() - started)));
        const current = this.now();
        if (current - lastTick > 3000) throw new Error('computer_suspended');
        lastTick = current; this.run.elapsed = Math.floor((current - started) / 1000);
      }
      if (this.cancelRequested) throw new Error('canceled');
      this.run.state = 'checking_position';
      const fresh = await this.client.reader(id);
      if (fingerprint(fresh.record) !== original) throw new Error('position_changed');
      const report = await this.makePayload(id, fresh, this.run.reportedSeconds);
      this.run.state = 'duration_pending'; await this.persist();
      if (this.cancelRequested) throw new Error('canceled');
      try { receipt = await this.client.request('/web/book/read', { body: report, referer: readerPath(id) }); }
      catch { throw new Error('duration_unknown'); }
      this.run.durationAcknowledged = await this.isAcknowledged(receipt); this.run.durationReceipt = receiptSummary(receipt); await this.persist();
      if (!this.run.durationAcknowledged) throw new Error(this.run.durationReceipt.state === 'rejected' ? 'duration_rejected' : 'duration_unconfirmed');
      this.run.state = 'verifying'; await this.persist();
      // Reads only, never replay a write merely because aggregate time has not changed.
      for (const delay of [0, 15000, 30000]) {
        if (delay) await this.wait(delay);
        const after = await this.client.reader(id);
        this.run.after = after.record.readingTime; this.run.increase = this.run.after - this.run.before;
        this.run.positionUnchanged = fingerprint(after.record) === original;
        this.run.checks.push({ elapsed: Math.floor((this.now() - started) / 1000), total: this.run.after });
        await this.persist();
      }
      this.run.state = 'done'; await this.persist();
    } catch (error) {
      this.run.state = 'stopped'; this.run.error = safeError(error);
      try { await this.persist(); } catch { this.run.error = 'private_storage_required'; }
    } finally { this.running = false; }
  }
}
