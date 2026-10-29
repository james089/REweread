import { createHash, randomInt } from 'node:crypto';

export const UA = 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/135.0.0.0 Safari/537.36';
const digest = (algorithm, value) => createHash(algorithm).update(String(value)).digest('hex');
export const fingerprint = record => digest('sha256', JSON.stringify(
  ['chapterUid', 'chapterIdx', 'chapterOffset', 'progress', 'summary'].map(k => record[k] ?? (k === 'summary' ? '' : 0))));

// Wire-format implementation for the Web reader, not a bundled upstream client.
export function encodeId(value) {
  const text = String(value), hash = digest('md5', text), numeric = /^\d+$/.test(text);
  const segments = numeric ? (text.match(/.{1,9}/g) || []).map(s => Number(s).toString(16))
    : [Array.from(Buffer.from(text), n => n.toString(16)).join('')];
  let encoded = hash.slice(0, 3) + (numeric ? '3' : '4') + '2' + hash.slice(-2)
    + segments.map(s => s.length.toString(16).padStart(2, '0') + s).join('g');
  if (encoded.length < 20) encoded += hash.slice(0, 20 - encoded.length);
  return encoded + digest('md5', encoded).slice(0, 3);
}
export const readerPath = id => '/web/reader/' + encodeId(id);
export function sign(payload) {
  const query = Object.keys(payload).filter(k => k !== 's').sort()
    .map(k => k + '=' + encodeURIComponent(payload[k])).join('&');
  let odd = 0x15051505, even = odd;
  for (let i = query.length - 1; i > 0; i -= 2) {
    odd = (odd ^ (query.charCodeAt(i) << ((query.length - i) % 30))) & 0x7fffffff;
    even = (even ^ (query.charCodeAt(i - 1) << (i % 30))) & 0x7fffffff;
  }
  return (odd + even).toString(16);
}
function appId() {
  let hash = 0;
  for (const character of UA) hash = (hash * 131 + character.charCodeAt(0)) & 0x7fffffff;
  return 'wb' + UA.split(' ').slice(0, 12).map(s => s.length % 10).join('') + 'h' + hash;
}
export function payload(id, reader, seconds, clock = Date.now(), nonce = randomInt(1000)) {
  const record = reader.record;
  if (!reader.ps || !record) throw new Error('reader_state_missing');
  // SSR starts pclts at 0. The browser's created hook replaces it with the
  // encoded client timestamp before either enter or duration is submitted.
  const clientTimestamp = typeof reader.pc === 'string' && reader.pc ? reader.pc : encodeId(Math.floor(clock / 1000));
  const body = { appId: appId(), b: encodeId(id), c: encodeId(record.chapterUid),
    ci: record.chapterIdx, co: record.chapterOffset, sm: record.summary ?? '',
    pr: record.progress, ct: Math.floor(clock / 1000), ps: reader.ps, pc: clientTimestamp };
  if (seconds !== null) {
    if (!Number.isSafeInteger(seconds) || seconds < 1 || seconds > 3600 || !reader.token) throw new Error('invalid_duration');
    Object.assign(body, { rt: seconds, ts: clock, rn: nonce,
      sg: digest('sha256', String(clock) + nonce + reader.token) });
  }
  body.s = sign(body);
  return body;
}
export function acknowledged(response) {
  if (!response || response.success === false || response.success === 0) return false;
  if (response.errCode !== undefined && Number(response.errCode) !== 0) return false;
  return response.succ !== undefined ? response.succ === 1 || response.succ === true
    : response.errCode !== undefined && Number(response.errCode) === 0;
}
export function receiptSummary(response) {
  const result = {};
  for (const key of ['errCode', 'succ', 'success']) {
    let value = response?.[key];
    if (key === 'errCode' && typeof value === 'string' && /^-?\d{1,8}$/.test(value)) value = Number(value);
    if (typeof value === 'boolean' || (typeof value === 'number' && Number.isFinite(value))) result[key] = value;
  }
  result.state = acknowledged(response) ? 'acknowledged'
    : result.success === false || result.success === 0 || result.succ === false || result.succ === 0
      || (result.errCode !== undefined && result.errCode !== 0) ? 'rejected' : 'unconfirmed';
  return result;
}
export function unwrapResponse(json) {
  if (!json || typeof json !== 'object' || !json.data || typeof json.data !== 'object') return json;
  const result = { ...json.data };
  for (const key of ['errCode', 'succ', 'success']) {
    if (json[key] === undefined) continue;
    // An explicit rejection at either envelope level always wins.
    if (key === 'errCode' && result.errCode !== undefined && Number(result.errCode) !== 0) continue;
    if ((key === 'succ' || key === 'success') && (result[key] === 0 || result[key] === false)) continue;
    result[key] = json[key];
  }
  return result;
}
export function initialState(html) {
  if (typeof html !== 'string' || html.length > 16 * 1024 * 1024) throw new Error('reader_state_missing');
  const marker = html.indexOf('window.__INITIAL_STATE__');
  if (marker < 0) throw new Error('reader_state_missing');
  const assignment = html.indexOf('=', marker), start = html.indexOf('{', assignment);
  let depth = 0, quoted = false, escaped = false;
  for (let i = start; i >= 0 && i < html.length; i++) {
    const c = html[i];
    if (quoted) { if (escaped) escaped = false; else if (c === '\\') escaped = true; else if (c === '"') quoted = false; }
    else if (c === '"') quoted = true;
    else if (c === '{') depth++;
    else if (c === '}' && --depth === 0) return JSON.parse(html.slice(start, i + 1));
  }
  throw new Error('reader_state_missing');
}
export function parseReader(html) {
  const reader = initialState(html).reader, record = reader?.progress?.book;
  if (!record || !['chapterUid', 'chapterIdx', 'chapterOffset', 'progress', 'readingTime']
    .every(k => Number.isFinite(record[k]) && record[k] >= 0)
    || !Number.isSafeInteger(record.readingTime) || !reader.psvts
    || reader.pclts === undefined || reader.pclts === null) throw new Error('saved_progress_missing');
  return { record, ps: reader.psvts, pc: reader.pclts, token: reader.token,
    title: String(reader.bookInfo?.title || '') };
}
