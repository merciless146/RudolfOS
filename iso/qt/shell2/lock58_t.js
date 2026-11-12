// 1.5.8 (lock58.js): Display Properties › Lock and sign-in screen (background: the desktop's, a picture, a colour;
// blur, darkening, clock, position, the account's name), on the lock screen and the sign-in page; and the
// shutting-down screen, with the tone played before the power goes. The desktop page with a stand-in rudolfd.
//   node lock58_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.mjs': 'text/javascript', '.html': 'text/html'};
const M = {calls: [], idle: 0, errors: [], mode: 'desktop', playing: 0};
const api = (a, q) => {
  M.calls.push(a);
  if (a == 'idle') return {ok: true, idle: M.idle};
  if (a == 'power' && /action=/.test(q || '')) { M.power = {at: Date.now(), q}; return {ok: true} }
  if (a == 'media') return {ok: true, playing: M.playing};
  if (a == 'apps/windows') return {ok: true, windows: [], mode: M.mode};
  if (a == 'apps/desktop') { M.mode = 'desktop'; return {ok: true} }
  if (a == 'apps/show') { M.mode = 'win ' + new URLSearchParams(q).get('win'); return {ok: true} }
  if (a == 'info') return {ok: true, backlight: false, live: 0, boot: 'uefi', kernel: 'test'};
  if (a == 'events') return {ok: true, events: ''};
  if (a == 'volume') return {ok: true, volume: 50, muted: false};
  return {ok: true};
};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', async () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/events/wait') await new Promise(r => setTimeout(r, 1500));
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5) == 'events/wait' ? 'events' : u.slice(5), qs))) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(8098, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const ctx = await b.newContext();
  // the old saved value: "Off" (0 minutes) from User Settings before 1.4.4
  await ctx.addInitScript(() => { if (!sessionStorage.getItem('t')) { sessionStorage.setItem('t', 1); localStorage.setItem('bd95sys', JSON.stringify({guest: true, ssmin: 0, lockss: true, lockmin: 0, suspmin: 0})) } });
  const p = await ctx.newPage(); p.on('pageerror', e => { M.errors.push(e.message); console.log('page error:', e.stack.slice(0, 300)) });
  await p.goto('http://127.0.0.1:8098/'); await p.waitForFunction(() => typeof ssRun == 'function' && typeof launch == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}(()=>{const o=document.getElementById('oobe');o&&o.remove();const l=document.getElementById('login');if(l)l.style.display='none'})();wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';window.elevate=async()=>true;window.__locks=0;const L=lockScreen;return 1})()");
  ok('0. the desktop is up, signed in', await q("!!USER"));
  // Display Properties › Lock and sign-in screen
  await q("launch('settings')"); await wait(600);
  await q("(()=>{const W=wins.find(w=>w.app=='settings');const b=[...W.body.querySelectorAll('*')].find(e=>e.children.length==0&&/^Display Properties$/.test(e.textContent.trim()));b&&b.click()})()"); await wait(600);
  ok('1. Display Properties has a "Lock and sign-in screen" section', await q("!!document.querySelector('.lk58')"));
  ok('1b. ...with the background settings: right after the colours, before the fonts, its rows in the same grid', await q("(()=>{const d=document.querySelector('.lk58'),b=document.querySelector('#b1').parentElement,f=document.querySelector('#ff');return d.previousElementSibling===b&&!!(d.compareDocumentPosition(f)&Node.DOCUMENT_POSITION_FOLLOWING)&&getComputedStyle(d).display=='contents'})()"));
  await q("(()=>{const d=document.querySelector('.lk58');const s=(id,v)=>{const e=d.querySelector(id);e.value=v;e.dispatchEvent(new Event('change'))};s('#lkbg','color');const c=d.querySelector('#lkcol');c.value='#aa0000';c.dispatchEvent(new Event('change'));const r=d.querySelector('#lkdim');r.value=0;r.dispatchEvent(new Event('input'));s('#lkck','small');s('#lkpos','bottom');const u=d.querySelector('#lkuser');u.checked=false;u.dispatchEvent(new Event('change'))})()");
  ok('2. ...its choices are kept for the computer', !!(await q("SYSC.lk")));
  const k = JSON.parse(await q("JSON.stringify(SYSC.lk)"));
  ok('...colour, no darkening, small clock, bottom left, no account name', k.bg == 'color' && k.color == '#aa0000' && k.dim == 0 && k.clock == 'small' && k.pos == 'bottom' && k.user === false, k);
  await q("wins.slice().forEach(w=>w.close())");
  await q("lockScreen()"); await wait(300);
  const lk = await q("(()=>{const o=document.getElementById('lockov');const b=o.querySelector('.lkbgl:last-of-type');const bg=[...o.querySelectorAll('.lkbgl')].map(x=>getComputedStyle(x).backgroundColor);return {bg,ck:getComputedStyle(o.querySelector('.lkclock')).fontSize,av:getComputedStyle(o.querySelector('.lkav')).display,jc:o.style.justifyContent,ai:o.style.alignItems}})()");
  ok('3. the lock screen: the colour behind it', lk.bg.includes('rgb(170, 0, 0)'), lk);
  ok('...the small clock, no picture or name, at the bottom left', lk.ck == '36px' && lk.av == 'none' && lk.jc == 'flex-end' && lk.ai == 'flex-start', lk);
  await q("document.getElementById('lockov').remove();LOCKED=false");
  // a picture of your own
  await q("(()=>{const c=document.createElement('canvas');c.width=40;c.height=20;const x=c.getContext('2d');x.fillStyle='#00aa00';x.fillRect(0,0,40,20);svj('bd95lkpic',c.toDataURL('image/jpeg',.9));SYSC.lk=Object.assign({},SYSC.lk,{bg:'picture',blur:6,clock:'off',user:true});sysSave()})()");
  await q("lockScreen()"); await wait(300);
  const pk = await q("(()=>{const o=document.getElementById('lockov');const l=[...o.querySelectorAll('.lkbgl')];return {img:l.map(x=>getComputedStyle(x).backgroundImage).join(' ').slice(0,60),blur:l.map(x=>getComputedStyle(x).filter).join(' '),ck:getComputedStyle(o.querySelector('.lkclock')).display,av:getComputedStyle(o.querySelector('.lkav')).display}})()");
  ok('4. a picture of your own, blurred; no clock; the name back', /data:image\/jpeg/.test(pk.img) && /blur\(6px\)/.test(pk.blur) && pk.ck == 'none' && pk.av != 'none', pk);
  await q("document.getElementById('lockov').remove();LOCKED=false");
  // the desktop's background, remembered for the sign-in page
  await q("SYSC.lk=Object.assign({},SYSC.lk,{bg:'desktop',blur:0,clock:'large'});sysSave();document.body.style.backgroundImage='linear-gradient(rgb(1, 2, 3), rgb(4, 5, 6))'");
  await q("lockScreen()"); await wait(300);
  const dk = await q("[...document.getElementById('lockov').querySelectorAll('.lkbgl')].map(x=>getComputedStyle(x).backgroundImage).join(' ')");
  ok('5. the desktop\'s background behind the lock screen', /rgb\(1, 2, 3\)/.test(dk), dk.slice(0, 100));
  await q("document.getElementById('lockov').remove();LOCKED=false");
  await q("showLogin()"); await wait(1200);
  const lg = await q("(()=>{const L=document.getElementById('login');return {bg:[...L.querySelectorAll('.lkbgl')].map(x=>getComputedStyle(x).backgroundImage).join(' '),ck:(L.querySelector('.lkck .lkclock')||{}).textContent||''}})()");
  ok('6. the sign-in page: the remembered desktop background, and a clock', /rgb\(1, 2, 3\)/.test(lg.bg) && /\d/.test(lg.ck), lg);
  // the shutting-down screen
  await q("USER=USER||ACCTS[0];document.getElementById('login').style.display='none';S.snd.master.on=true;S.snd.shutdown={on:true,vol:100}");
  const t0 = Date.now(); await q("shutdownDlg()"); await wait(200);
  await q("document.querySelector('.rmodal button[data-a=poweroff]').click()"); await wait(250);
  const sc = await q("(()=>{const o=document.getElementById('shutov');return o?{t:o.querySelector('.shutt').textContent,z:+getComputedStyle(o).zIndex,img:!!o.querySelector('img')}:null})()");
  ok('7. Shut down: a full-screen "Shutting down…" at once, over everything', sc && sc.t == 'Shutting down…' && sc.z > 1000007 && sc.img, sc);
  for (let i = 0; i < 40 && !M.power; i++) await wait(100);
  const len = await q("sfxLen('shutdown')");
  ok('8. ...the power is asked for only after the tone has played', M.power && M.power.at - t0 >= Math.min(3000, len * 1000) && /action=poweroff/.test(M.power.q || ''), {after: M.power && M.power.at - t0, len});
  ok('9. no page errors', M.errors.length == 0, M.errors);
  await b.close(); srv.close();
  const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})();
