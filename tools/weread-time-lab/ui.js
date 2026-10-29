const $ = id => document.getElementById(id);
const errors = { network_failed:'连接微信读书失败。', login_required:'登录已失效，请重新扫码。', shelf_unavailable:'当前网页书架无法解析，请手动填写 bookId。', reader_state_missing:'无法解析网页状态，未继续提交。', saved_progress_missing:'未取得可靠的云端保存位置，未继续提交。', otp_required:'账号要求额外手机验证，当前工具暂不支持。', login_expired:'二维码已过期，请重新生成。', entry_rejected:'进入阅读被拒绝，未上传时长。', entry_unknown:'进入阅读请求结果不明，已停止，不会自动重发。', duration_rejected:'时长请求被拒绝。', duration_unknown:'时长请求结果不明，不会自动重发。', position_changed:'云端阅读位置发生变化，已停止，不覆盖位置。', canceled:'已停止；之前已经发出的请求无法撤回。', computer_suspended:'检测到电脑休眠或明显停顿，已停止。', invalid_duration:'请选书并选择有效时长。', test_busy:'当前有操作正在进行，请稍候。', private_storage_required:'本机私有存储不可用。' };
const phases = { preparing:'读取云端基线…', entry_pending:'发送进入阅读…', waiting:'实际等待中', checking_position:'核对云端位置…', duration_pending:'提交一次实际时长…', verifying:'回读云端总时长…', done:'本轮测试完成', stopped:'本轮测试已停止' };
Object.assign(errors, { entry_unconfirmed:'进入阅读没有返回可确认的成功回执；未上传时长，也未自动重发。', duration_unconfirmed:'时长请求没有返回可确认的成功回执，未自动重发。' });
async function api(path, data = {}) {
  const response = await fetch(path, { method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(data) });
  const result = await response.json();
  if (!response.ok) throw new Error(errors[result.error] || '操作失败；已隐藏原始响应以保护登录信息。');
  return result;
}
function action(id, fn) { $(id).onclick = async () => { $('error').textContent=''; $(id).disabled=true; try { await fn(); } catch(e) { $('error').textContent=e.message; } finally { $(id).disabled=false; } }; }
action('login', async () => { await api('/api/login'); $('qr').src='/qr.svg'; $('qr').hidden=false; });
action('shelf', async () => {
  const { books } = await api('/api/shelf'); $('books').replaceChildren(new Option('请选择一本书', ''));
  for (const book of books) $('books').add(new Option(book.title, book.id));
});
$('books').onchange = () => { $('bookId').value=$('books').value; };
action('start', async () => {
  if (!$('consent').checked) throw new Error('请先确认本轮测试会写入账号的云端阅读记录。');
  await api('/api/start', { bookId:$('bookId').value.trim(), seconds:Number($('seconds').value) });
});
action('cancel', () => api('/api/cancel'));
async function refresh() {
  try {
    const state = await (await fetch('/api/status')).json();
    $('loginStatus').textContent=state.loggedIn?'已独立登录，本机凭据已保存。':state.login==='waiting'?'请用微信扫码并确认登录。':state.login==='error'?(errors[state.loginError] || '登录失败，请重试。'):'尚未登录。';
    $('login').hidden=state.loggedIn; $('login').disabled=state.login==='waiting';
    $('testPanel').hidden=!state.loggedIn;
    if (state.login==='waiting') {
      if (!$('qr').getAttribute('src')) $('qr').src='/qr.svg';
      $('qr').hidden=false;
    } else $('qr').hidden=true;
    const run=state.run;
    if (run) {
      const busy=!['done','stopped'].includes(run.state);
      $('start').disabled=busy; $('shelf').disabled=busy;
      $('cancel').disabled=!['preparing','entry_pending','waiting','checking_position'].includes(run.state);
      $('phase').textContent=(phases[run.state]||run.state)+(run.state==='waiting'?` · ${run.elapsed} / ${run.seconds} 秒`:'')
        +(run.reportedSeconds!==run.seconds?` · 实验：等待 ${run.seconds} 秒，提交 ${run.reportedSeconds} 秒`:'');
      $('progress').max=run.seconds; $('progress').value=run.elapsed;
      const receiptLabel=(accepted,receipt)=>accepted===undefined?'—':accepted?'成功':receipt?.state==='rejected'?'拒绝':'未确认';
      $('entryAck').textContent=receiptLabel(run.entryAcknowledged,run.entryReceipt);
      $('durationAck').textContent=receiptLabel(run.durationAcknowledged,run.durationReceipt);
      $('increase').textContent=run.increase===undefined?'—':`${run.increase} 秒`;
      $('result').textContent=run.error?(errors[run.error]||'操作失败，未自动重发。'):run.state==='done'?`测试前 ${run.before} 秒，最后回读 ${run.after} 秒。位置${run.positionUnchanged?'保持一致':'已变化，结果可能受其他客户端影响'}。此数值是观察窗口内的差额，并非服务端入账凭证。`:'回执成功与实际入账分开显示。网络结果不明时不自动重发。';
      const diagnostic=run.durationReceipt||run.entryReceipt;
      if (diagnostic?.errCode!==undefined) $('result').textContent+=` 服务端错误码：${diagnostic.errCode}。`;
      $('checks').replaceChildren(...(run.checks||[]).map(item=>{const li=document.createElement('li');li.textContent=`入场后 ${item.elapsed} 秒回读：累计 ${item.total} 秒`;return li;}));
    }
  } catch { $('error').textContent='本机服务已停止或暂时不可用。'; }
  finally { setTimeout(refresh,1000); }
}
refresh();
