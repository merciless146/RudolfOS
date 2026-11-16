// 1.6.0 (plan Part 6, stage A item 6): the Messages app: a new message copied from the modem into the account's folder
// (.rudolf/messages.json through rudolfd's home/write) and deleted from the modem, its conversation with the unread
// count; a long Unicode message: the count of parts while typing, sent (the number and the text to rudolfd), Sent, then
// Delivered from the delivery report; a new conversation by number; kept across reopening; the text in notices only with
// an administrator's leave; a new message's notice on the lock screen (its sender only). A stand-in rudolfd.
//   node messages60_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www'); const PORT = 8516;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const M = [{id: 0, maker: 'Quectel', model: 'EG25-G', state: 'connected', failed: '', locked: '', sim: true}];
let SMS = [], FILE = null, calls = [], notes = [], next = 5, preview = false;
const rx = (id, num, text, ts) => ({id, sms: {content: {number: num, text}, properties: {state: 'received', 'pdu-type': 'deliver', timestamp: ts}}});
const api = (a, qs, m, body) => { const p = new URLSearchParams(qs);
  if (a == 'mobile/status') return {ok: true, running: true, modems: M, notes: []};
  if (a == 'mobile/sms') return {ok: true, messages: SMS};
  if (a == 'mobile/sms/send') { const b = JSON.parse(body); calls.push({a, b}); const id = next++; SMS.push({id, sms: {content: {number: b.number, text: b.text}, properties: {state: 'sent', 'pdu-type': 'submit', 'delivery-state': '--'}}}); return {ok: true, id} }
  if (a == 'mobile/sms/delete') { calls.push({a, qs}); SMS = SMS.filter(x => x.id != +p.get('sms')); return {ok: true, message: 'deleted'} }
  if (a == 'mobile/sms/preview') { if (m == 'POST') { calls.push({a, qs, body}); preview = p.get('on') == '1' } return {ok: true, on: preview} }
  if (a == 'mobile/notes') { const since = +p.get('since') || 0; return {ok: true, notes: notes.filter(n => n.n > since), last: notes.length ? notes[notes.length - 1].n : 0} }
  if (a == 'home/read') return FILE == null ? [404, {ok: false, error: 'not found'}] : ['raw', FILE];
  if (a == 'home/write') { calls.push({a, qs}); FILE = body; return {ok: true} }
  return {ok: true} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0); let body = '';
  q.on('data', d => body += d); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/a/')) { const f = path.join(WWW, u); if (fs.existsSync(f)) { s.writeHead(200); return s.end(fs.readFileSync(f)) } }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      let r = api(u.slice(5), qs || '', q.method, body), code = 200; if (Array.isArray(r)) { if (r[0] == 'raw') { s.writeHead(200, {'content-type': 'application/octet-stream'}); return s.end(r[1]) } [code, r] = r }
      s.writeHead(code, {'content-type': 'application/json'}); return s.end(JSON.stringify(r)) }
    s.writeHead(404); s.end() }) }).listen(PORT, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto(`http://127.0.0.1:${PORT}/`); await p.waitForFunction(() => typeof messagesApp == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x',id:'uAnn',disk:1},{n:'Kid',r:'user',h:'y'}];USER=ACCTS[0];HD.acct=uid();if(!HOME)HOME={};
    pwCheck=async(a,pw)=>a.n=='Ann'&&pw=='secret';window.__nt=[];const on=notify;notify=function(t,b){__nt.push(t+' | '+b);return on.apply(this,arguments)};return hdOn()})()`);
  await q("messagesApp()"); await wait(1500);
  let x = await q("MSG.W.body.textContent");
  ok('1. Messages opens: no messages yet, saying where they come from', /No messages yet/.test(x), x);
  SMS.push(rx(1, '+1 (555) 987-6543', 'Your code is 123456', '2026-11-16T01:00:00+00:00')); await q("msgSync()"); await wait(800);
  x = await q("MSG.W.body.querySelector('.msgcl').textContent");
  ok('2. a new message: copied in, its conversation (the number as one) with the unread count', /\+15559876543 \(1\)/.test(x) && /Your code is 123456/.test(x), x);
  ok("...kept in the account's folder (.rudolf/messages.json, home/write) and deleted from the modem", calls.some(c => c.a == 'home/write' && /path=\.rudolf%2Fmessages\.json/.test(c.qs)) && /Your code is 123456/.test(FILE || '') && calls.some(c => c.a == 'mobile/sms/delete' && /sms=1/.test(c.qs)) && SMS.length == 0, calls);
  await q("document.querySelector('.msgci').click()"); await wait(300);
  ok('...opened: the bubble, and no longer unread', (await q("[...MSG.W.body.querySelectorAll('.msgi .msgx')].map(e=>e.textContent).join()")) == 'Your code is 123456' && !/\(1\)/.test(await q("MSG.W.body.querySelector('.msgcl').textContent")));
  const long = 'Привет! ' + 'Это длинное сообщение. '.repeat(6);
  await p.fill('.msgt', long); await q("document.querySelector('.msgt').dispatchEvent(new Event('input'))");
  x = await q("MSG.W.body.querySelector('.msgn').textContent");
  ok('3. a long message in another alphabet: the characters and the parts it takes (Unicode: 67 a part)', x == [...long].length + ' characters · ' + Math.ceil([...long].length / 67) + ' parts (Unicode)', x);
  await q("document.querySelector('.msgs').click()"); await wait(1200);
  const c = calls.find(c => c.a == 'mobile/sms/send') || {};
  ok('...sent: the number and the whole text to rudolfd; the bubble says Sent', c.b && c.b.number == '+15559876543' && c.b.text == long && /Sent$/.test(await q("[...MSG.W.body.querySelectorAll('.msgo .msgi2')].pop().textContent")), c);
  SMS[0].sms.properties['delivery-state'] = 'completed-received'; await q("msgSync()"); await wait(800);
  ok("4. the delivery report: Delivered; the sent message deleted from the modem", /Delivered$/.test(await q("[...MSG.W.body.querySelectorAll('.msgo .msgi2')].pop().textContent")) && SMS.length == 0);
  await p.fill('.msgto', '+1 555 000 1111'); await q("document.querySelector('.msgnew').click()"); await wait(300);
  ok('5. a new conversation by number', (await q("MSG.W.body.querySelector('.msgh').textContent")) == '+15550001111');
  await p.fill('.msgt', 'Hi'); await q("document.querySelector('.msgs').click()"); await wait(800);
  ok('...a short message: one part (GSM-7)', await q("msgParts('Hi').parts==1&&msgParts('a'.repeat(161)).parts==2&&msgParts('é'.repeat(160)).parts==1"));
  await q("MSG.W.close();MSG.db=null"); await wait(300); await q("messagesApp()"); await wait(1200);
  x = await q("MSG.W.body.querySelector('.msgcl').textContent");
  ok("6. reopened: read back from the account's folder, the newest conversation first", /^\+15550001111/.test(x) && /\+15559876543/.test(x), x);
  await q("document.querySelector('.msgprev').click()"); await wait(300);
  const why = await q("document.querySelector('.rmodal .rmr')?.textContent||''"); await p.type('.rmodal .ep', 'secret'); await q("document.querySelector('.rmodal .ey').click()"); await wait(800);
  const pc = calls.find(c => c.a == 'mobile/sms/preview') || {};
  ok("7. the text in notices: an administrator's leave asked (saying the lock screen too), sent with it", /lock screen/.test(why) && /on=1/.test(pc.qs || '') && JSON.parse(pc.body || '{}').n == 'Ann' && preview, [why, pc]);
  await q("LOCKED=true;__nt=[]"); notes.push({n: 1, t: '💬 New message from +15559876543', b: 'Open Messages to read it.'}, {n: 2, t: '📶 Using mobile data', b: 'x'}); await wait(6500);
  x = await q("__nt.join('\\n')");
  ok("8. locked: a new message's notice (its sender, not its text); the other notices wait", /💬 New message from \+15559876543 \| Open Messages to read it\./.test(x) && !/Using mobile data/.test(x), x);
  await q("LOCKED=false");
  ok('9. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
