// 1.5.9 (plan 7.2): "In a call": 🎙️ in the taskbar while an app records from a microphone, one click mutes it for
// everything; a call holds off the screen saver, the lock and sleep. A stand-in rudolfd.   node call59_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
let users = ['Chromium'], muted = false; const posts = [];
const api = (a, q, m) => { const sp = new URLSearchParams(q || '');
  if (a == 'mic') { if (m == 'POST') { posts.push(sp.get('muted')); muted = sp.get('muted') == '1' } return {ok: true, users, muted} }
  if (a == 'media') return {ok: true, playing: 0, mic: users.length};
  return {ok: true} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/a/')) { const f = path.join(WWW, u); if (fs.existsSync(f)) { s.writeHead(200); return s.end(fs.readFileSync(f)) } }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs, q.method))) }
    s.writeHead(404); s.end() }) }).listen(8276, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8276/'); await p.waitForFunction(() => typeof call59Poll == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x'}];USER=ACCTS[0];return 1})()`);
  await q('call59Poll()'); await wait(300);
  const t = await q("TRAYX.call?[TRAYX.call.textContent,TRAYX.call.title]:null");
  ok('1. an app records from the microphone: 🎙️ in the taskbar with its name', t && t[0] == '🎙️' && /In a call: Chromium uses the microphone/.test(t[1]), t);
  ok('2. a call holds off the screen saver, the lock and sleep (as a video does)', await q('mediaOther()'));
  await q('TRAYX.call.click()'); await wait(400);
  const t2 = await q("TRAYX.call?[TRAYX.call.textContent,TRAYX.call.title]:null");
  ok('3. one click mutes the microphone for everything (rudolfd), shown as muted', posts.at(-1) == '1' && t2 && t2[0] == '🔇' && /Microphone muted/.test(t2[1]), {posts, t2});
  await q('TRAYX.call.click()'); await wait(400);
  ok('4. ...and back on', posts.at(-1) == '0' && await q("TRAYX.call.textContent=='🎙️'"), posts);
  users = []; await q('call59Poll()'); await wait(300);
  ok('5. the call ended: the indicator goes, and the screen saver may start again', !(await q('!!TRAYX.call')) && !(await q('mediaOther()')));
  ok('6. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
