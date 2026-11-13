// 1.5.9 (plan 7.2): "In a call": 🎙️ in the taskbar while an app records from a microphone, one click mutes it for
// everything; a call holds off the screen saver, the lock and sleep. A stand-in rudolfd.   node call59_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
let users = ['Chromium'], muted = false, echo = {on: false, active: false}; const posts = [];
const api = (a, q, m) => { const sp = new URLSearchParams(q || '');
  if (a == 'mic') { if (m == 'POST') { posts.push(sp.get('muted')); muted = sp.get('muted') == '1' } return {ok: true, users, muted} }
  if (a == 'sound/echo') { if (m == 'POST') { echo = {on: sp.get('on') == '1', active: sp.get('on') == '1'}; posts.push('echo' + sp.get('on')) } return Object.assign({ok: true}, echo) }
  if (a == 'audio') return {ok: true, cards: ' 0 [Generic        ]: HDA-Intel - HD-Audio Generic', pcm: '00-00: ALC : ALC : playback 1 : capture 1'};
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
  await q("(()=>{const d=document.createElement('div');d.id='apt';document.body.appendChild(d);appsPanel(d);return 1})()"); await wait(300);
  const ph = await q("document.querySelector('#apt .ph59')?.textContent||''");
  ok('7. Settings › Apps › Calls to phones: Google Voice, Zoom Phone and Teams Phone, what each needs', /Calls to phones/.test(ph) && /Google Voice/.test(ph) && /Zoom Phone/.test(ph) && /Teams Phone/.test(ph) && /does not sign you up/.test(ph), ph.slice(0, 200));
  // 7.3: a phone number clicked in a tab (the tab protocol's "tel" event)
  await q("(()=>{window.__wo=[];webOpen=u=>{__wo.push(u)};delete S.telApp;dispatchEvent(new MessageEvent('message',{data:{rudolfShell:'tabev',ev:'tel',tid:'t1',num:'+1 (555) 010-2030'},origin:SHELL_ORIGIN,source:null}));telOpen('+1 (555) 010-2030');return 1})()"); await wait(300);
  const tl = await q("(()=>{const o=[...document.querySelectorAll('.rmodal')].pop();return o?o.textContent:''})()");
  ok('8. a phone number clicked: how to call it (Google Voice, Teams, copy), the number cleaned', /Call \+15550102030/.test(tl) && /Google Voice/.test(tl) && /Microsoft Teams/.test(tl) && /Copy the number/.test(tl), tl);
  await q("(()=>{const o=[...document.querySelectorAll('.rmodal')].pop();o.querySelector('.ta').checked=true;[...o.querySelectorAll('.tl button')].find(b=>/Google Voice/.test(b.textContent)).click();return 1})()"); await wait(300);
  ok('9. ...Google Voice opens in Oracle with the number; chosen for next time', await q("__wo.at(-1)=='https://voice.google.com/u/0/calls?a=nc,%2B15550102030'&&S.telApp=='voice'"));
  await q("document.querySelectorAll('.rmodal').forEach(o=>o.remove());telOpen('tel:+44 20 7946 0000')"); await wait(200);
  ok('10. ...the next number goes straight there', await q("!document.querySelector('.rmodal .tl')&&__wo.at(-1)=='https://voice.google.com/u/0/calls?a=nc,%2B442079460000'"));
  // 7.2: Settings › Sound › Calls: the echo canceller, off by default
  await q("(()=>{const d=document.createElement('div');d.id='vpt';document.body.appendChild(d);volPanelS(d,{});return 1})()"); await wait(500);
  const e1 = await q("(()=>{const c=document.querySelector('#vpt .echo59 .ec');return c?[c.checked,c.disabled,document.querySelector('#vpt .echo59').textContent]:null})()");
  ok('11. Settings › Sound › Calls: "Cancel echo for Linux apps", off by default', e1 && !e1[0] && !e1[1] && /Cancel echo for Linux apps/.test(e1[2]) && /processor time/.test(e1[2]), e1);
  await q("(()=>{const c=document.querySelector('#vpt .echo59 .ec');c.checked=true;c.onchange();return 1})()"); await wait(400);
  ok('12. ...switched on: rudolfd is asked, the box stays on', posts.at(-1) == 'echo1' && await q("document.querySelector('#vpt .echo59 .ec').checked"), posts);
  ok('6. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
