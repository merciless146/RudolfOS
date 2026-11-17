// 1.6.0 (docs/plan-1.6.0.md Part 12, item 12): the timer gate (svc60.js) and the native desktop's service page (#app=do-svc), in
// Chromium with a stand-in rudolfd: Standard's page is untouched (the browser's own timers, every module's running, the heartbeat); an
// app's window on the native desktop runs only what it needs (the theme's and icons' ticks, not the alarms', the heartbeat, the
// updates'); one on Standard or opened by hand in a browser is untouched; the service page runs them once signed in (not what the shell does itself),
// is "visible" to the modules, rings an alarm once and a Clock timer made in the Clock's window (closed since) once, through the
// shell's toasts and sounds; a window it would make is the app's own window (op open); clearInterval on a held timer works.
//   node svc60_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 700) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.html': 'text/html', '.webp': 'image/webp', '.jpg': 'image/jpeg', '.woff2': 'font/woff2', '.woff': 'font/woff'};
const M = {calls: [], session: null, engine: ''};
const PORT = +process.env.SVC60_PORT || 8601;
const srv = http.createServer((q, s) => { const [u0] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0' + (M.engine ? ',engine:"' + M.engine + '"' : '') + '};</script>')) }
    if (u.startsWith('/api/')) { M.calls.push({t: Date.now(), c: q.method + ' ' + u.slice(5), ref: q.headers['x-page'] || ''});
      if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      if (u == '/api/desktop/session') { if (!M.session) { s.writeHead(404, {'content-type': 'application/json'}); return s.end('{"ok":false,"error":"no native session"}') }
        s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(M.session)) }
      s.writeHead(200, {'content-type': 'application/json'}); return s.end('{"ok":true}') }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(PORT, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
const URL0 = 'http://127.0.0.1:' + PORT + '/';
// the native shell's message handler, standing in: what each page posts is kept in window.__posted
const NAT = "window.__posted=[];window.webkit={messageHandlers:{rudolfNative:{postMessage:m=>{window.__posted.push(m)}}}};";
const runs = (l, m) => l.filter(h => h.mod == m && h.run).length;
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const ctx = await b.newContext({viewport: {width: 1100, height: 760}});
  const errs = []; const watch = p => p.on('pageerror', e => { if (/svc|__svc|SVC|oaTmr|__tmr/.test((e.stack || '') + e.message)) errs.push(e.message) });
  // ---- Standard: nothing changes
  let p = await ctx.newPage(); watch(p); await p.goto(URL0); await p.waitForFunction(() => typeof oaOpen == 'function' && typeof acctNew == 'function', null, {timeout: 30000});
  const std = await p.evaluate(`({nat:String(window.setTimeout).includes('[native code]')&&String(window.setInterval).includes('[native code]')&&String(window.clearInterval).includes('[native code]'),on:SVC.on,held:__svcList().length,tmr:typeof window.__tmrIn})`);
  ok('1. Standard (no #app=): setTimeout and setInterval are the browser\'s own, nothing held, no shared timers', std.nat && !std.on && std.held == 0 && std.tmr == 'undefined', std);
  const acct = await p.evaluate(`(async()=>{svj('bd95setup',1);['oobe'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    const a=await acctNew('Ann','pw-one','admin');ACCTS=[a];window.__sys=1;saveUser();USER=a;await userLogin(a,'pw-one');
    const hex=u=>[...new Uint8Array(u)].map(x=>x.toString(16).padStart(2,'0')).join('');
    return {id:a.id,pk:hex(await pbkBits('pw-one',a.ks,256)),fk:hex(FKRAW)}})()`);
  M.session = {ok: true, id: acct.id, n: 'Ann', r: 'admin', pk: acct.pk, fk: acct.fk};
  const t0 = Date.now(); await wait(9000);
  ok('2. ...its heartbeat goes to rudolfd as before (every 8 s)', M.calls.some(c => c.t > t0 && c.c == 'POST heartbeat'), M.calls.slice(-5));
  await p.close();
  const open = async (hash, init) => { const pg = await ctx.newPage(); watch(pg); if (init) await pg.addInitScript(init);
    await pg.goto(URL0 + hash); await pg.waitForFunction(() => typeof oaOpen == 'function', null, {timeout: 30000}); return pg };
  // ---- an app's page on Standard (a Tor window) or opened by hand in a browser (no native desktop): untouched
  p = await open('#app=notepad'); await p.waitForSelector('.win.oamain', {timeout: 20000});
  let l = await p.evaluate('__svcList()'), g = await p.evaluate(`({nat:String(window.setInterval).includes('[native code]'),on:SVC.on})`);
  ok('3. an app\'s page on Standard or in a browser by hand (no native desktop): the browser\'s own timers, nothing held, as before', l.length == 0 && g.nat && !g.on, [l.length, g]);
  await p.close();
  // ---- the native desktop: an app's window
  M.engine = 'native';
  p = await open('#app=notepad', NAT); await p.waitForSelector('.win.oamain', {timeout: 20000}); await wait(500);
  l = await p.evaluate('__svcList()');
  ok('4. a native app\'s window (Notepad): the theme\'s and the icons\' minute ticks and its start run', runs(l, 'a11y25.js') >= 3 && l.some(h => h.run && /dynResolve\(\)\.mode/.test(h.f)), l.filter(h => h.run));
  ok('5. ...not the alarms\' and reminders\', the heartbeat, the session\'s save, the lag meter, the updates\', the home folder... of others',
    runs(l, 'apps25b.js') == 0 && runs(l, 'core25.js') == 0 && runs(l, 'shell25.js') == 0 && runs(l, 'term26.js') == 0 && runs(l, 'sysupd25.js') == 0 && runs(l, 'linux26.js') == 0 && runs(l, 'call59.js') == 0 && runs(l, 'tabs26.js') == 0, l.filter(h => h.run));
  const nat = await p.evaluate(`(()=>{const h=SVC.held.find(x=>x.mod=='call59.js'&&x.t=='i');clearInterval(CALL59.iv);return {gone:h&&h.gone,same:h&&h.id==CALL59.iv,hid:document.hidden}})()`);
  ok('6. ...a held timer\'s own clearInterval works (the page keeps its number)', nat.gone && nat.same, nat);
  const t1 = Date.now(); await wait(9000);
  ok('7. ...no heartbeat from it (the shell has its own)', !M.calls.some(c => c.t > t1 && c.c == 'POST heartbeat'), M.calls.filter(c => c.t > t1).map(c => c.c));
  await p.close();
  p = await open('#app=browser', NAT); await wait(2500); l = await p.evaluate('__svcList()');
  ok('8. Oracle\'s window: its own (its real tabs, dark pages, the bookmarks bar) run there', runs(l, 'tabs26.js') == 1 && runs(l, 'web46.js') == 1 && runs(l, 'apps25b.js') == 0, l.filter(h => h.run).map(h => h.mod));
  await p.close();
  // ---- the service page
  const hm = d => String(d.getHours()).padStart(2, '0') + ':' + String(d.getMinutes()).padStart(2, '0');
  const sv = await open('#app=do-svc&since=' + (Date.now() - 1000), NAT);
  await sv.waitForFunction(() => window.__posted.some(m => m.op == 'svc' && m.ready == '1'), null, {timeout: 30000}).catch(() => {});
  l = await sv.evaluate('__svcList()');
  const s1 = await sv.evaluate(`({ready:__posted.some(m=>m.op=='svc'&&m.ready=='1'),user:USER&&USER.n,hidden:document.hidden,vis:document.visibilityState,wins:wins.length,title:document.title})`);
  ok('9. the service page signs in, says it is ready, has no window, is "visible" to the modules', s1.ready && s1.user == 'Ann' && s1.hidden === false && s1.vis == 'visible' && s1.wins == 0, s1);
  ok('10. ...runs the desktop\'s background work (the alarms\' and reminders\', the session\'s save, the updates\', Linux apps\')', runs(l, 'apps25b.js') == 2 && runs(l, 'shell25.js') >= 2 && runs(l, 'term26.js') == 2 && runs(l, 'sysupd25.js') == 2 && runs(l, 'linux26.js') >= 3, l.filter(h => !h.run).map(h => h.mod + ' ' + h.f.slice(0, 30)));
  ok('11. ...not what the shell does itself: the heartbeat, the microphone\'s, the cellular notices, the development link', runs(l, 'call59.js') == 0 && runs(l, 'mobile60.js') == 0 && runs(l, 'devlink40.js') == 0 && !l.some(h => h.run && /heartbeat/.test(h.f)), l.filter(h => h.run).map(h => h.mod));
  // an alarm this minute: rung once, as the shell's toast, with the shell's sound
  await sv.evaluate(`svj(ALK(),[{t:'${hm(new Date())}',label:'Wake',days:[],on:true}])`);
  await sv.waitForFunction(() => window.__posted.some(m => m.op == 'toast' && /Wake/.test(m.t)), null, {timeout: 8000}).catch(() => {}); await wait(3000);
  let po = await sv.evaluate('__posted');
  ok('12. an alarm: one toast through the shell, with its sound (op sfx), nothing drawn in the page', po.filter(m => m.op == 'toast' && m.t == '⏰ Wake').length == 1 && po.some(m => m.op == 'sfx' && m.k == 'notify'), po.filter(m => m.op != 'size'));
  // a reminder's click opens Calendar in a window of its own; a window the page would make is that app's own
  await sv.evaluate(`launch('calendar')`); await sv.evaluate(`try{calcApp&&calcApp()}catch(e){}`); await wait(300);
  po = await sv.evaluate(`({p:__posted.filter(m=>m.op=='open'),w:wins.length,el:document.querySelectorAll('.win').length})`);
  ok('13. ...an app it opens (a reminder\'s click) is a window of its own (op open), never one inside it', po.p.some(m => m.app == 'calendar') && po.w == 0 && po.el == 0, po);
  // a Clock timer: made in the Clock's window, which closes; the service page rings it once
  const ck = await open('#app=clock', NAT); await ck.waitForSelector('.tmtabs [data-t=timer]', {timeout: 20000});
  await ck.evaluate(`(()=>{document.querySelector('.tmtabs [data-t=timer]').click()})()`); await wait(300);
  const nT = await ck.evaluate(`(()=>{const q=s=>document.querySelector('.clb '+s);q('.tm').value=0;q('.ts').value=3;q('.tl').value='Tea';q('.ta').click();return TIMERS.length})()`);
  const ckl = await ck.evaluate('__svcList()');
  ok('14. the Clock\'s window: a timer started there (its own alarms\' tick is not running in it)', nT == 1 && runs(ckl, 'apps25b.js') == 0, [nT, ckl.filter(h => h.mod == 'apps25b.js')]);
  await ck.evaluate('window.__oneAppClose()'); await ck.close();
  await sv.waitForFunction(() => window.__posted.some(m => m.op == 'toast' && m.t == '⏰ Tea'), null, {timeout: 10000}).catch(() => {}); await wait(2500);
  po = await sv.evaluate(`({n:__posted.filter(m=>m.op=='toast'&&m.t=='⏰ Tea').length,d:TIMERS.map(t=>t.done)})`);
  ok('15. ...its window closed: the service page rings it once', po.n == 1 && po.d.join() == '1', po);
  const ck2 = await open('#app=clock', NAT); await ck2.waitForSelector('.tmtabs [data-t=timer]', {timeout: 20000});
  await ck2.evaluate(`document.querySelector('.tmtabs [data-t=timer]').click()`); await wait(800);
  const sh = await ck2.evaluate(`[...document.querySelectorAll('.clb .acr')].map(r=>r.textContent)`);
  ok('16. ...the Clock\'s window opened again shows it Done (the timers are the desktop\'s)', sh.length == 1 && /Done/.test(sh[0]) && /Tea/.test(sh[0]), sh);
  await ck2.close();
  // a timer from before this sign-in is gone, as with Standard's page
  await sv.close();
  const sv2 = await open('#app=do-svc&since=' + (Date.now() + 1000), NAT); await sv2.waitForFunction(() => window.__posted.some(m => m.op == 'svc'), null, {timeout: 30000}).catch(() => {});
  const left = await sv2.evaluate(`(()=>{try{return JSON.parse(localStorage.getItem('bd95timers:'+uid())).l.length}catch(e){return -1}})()`);
  ok('17. a new sign-in\'s service page drops the Clock\'s timers made before it (Standard\'s page loses them at sign-out)', left == 0, left);
  await sv2.close();
  ok('18. no page errors from the gate or the timers', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('FAIL', e); process.exit(1) });
