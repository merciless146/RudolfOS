// 1.5.9 (plan Part 12, step 1): the page's one-app mode (index.html#app=NAME), as the native desktop engine shows each
// app: signed in from rudolfd's native session (desktop/session: the account and its keys, no password), the app alone
// filling the page, its title as the page's, closed cleanly; without a session the sign-in screen first. In Chromium,
// with a stand-in rudolfd. (iso/qt/webkit/oneapp59_ci.py checks the same in WebKitGTK.)   node oneapp59_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.html': 'text/html', '.webp': 'image/webp', '.jpg': 'image/jpeg', '.woff2': 'font/woff2', '.woff': 'font/woff'};
const M = {calls: [], session: null};
// the lsync copy's flush, as rudolfd's page has it (the stand-in records it)
const LS = "window.__rudolfLsFlush=()=>fetch('/api/lsync',{method:'POST',headers:{'X-Rudolf-Token':'t'},body:'{}'});";
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  const bs = []; q.on('data', d => bs.push(d)); q.on('end', () => { if (u == '/api/desktop/apps' || u == '/api/desktop/wallpaper') M[u.slice(13)] = {q: qs, b: Buffer.concat(bs)};
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0' + (M.engine ? ',engine:"' + M.engine + '"' : '') + '};' + LS + '</script>')) }
    if (u.startsWith('/api/')) { M.calls.push(q.method + ' ' + u.slice(5));
      if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      if (u == '/api/devlink' && M.devlink) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(M.devlink)) }
      if (u == '/api/desktop/session') { if (!M.session) { s.writeHead(404, {'content-type': 'application/json'}); return s.end('{"ok":false,"error":"no native session"}') }
        s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(M.session)) }
      s.writeHead(200, {'content-type': 'application/json'}); return s.end('{"ok":true}') }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(8295, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
const URL0 = 'http://127.0.0.1:8295/';
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const ctx = await b.newContext({viewport: {width: 1100, height: 760}});
  const errs = []; const watch = p => p.on('pageerror', e => { if (/\boa[A-Z]|oneapp|ONEAPP|userLogin/.test(e.stack || '')) errs.push(e.message) });
  // an account made and signed in as the page does, with a file in its home folder
  let p = await ctx.newPage(); watch(p);
  await p.goto(URL0); await p.waitForFunction(() => typeof oaOpen == 'function' && typeof acctNew == 'function', null, {timeout: 30000});
  const acct = await p.evaluate(`(async()=>{svj('bd95setup',1);['oobe'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    const a=await acctNew('Ann','pw-one','admin');ACCTS=[a];window.__sys=1;saveUser();USER=a;await userLogin(a,'pw-one');
    HOME.DOCUMENTS['NOTE.TXT']='written before';fsSave();fsFlush();await new Promise(r=>setTimeout(r,800));
    const hex=u=>[...new Uint8Array(u)].map(x=>x.toString(16).padStart(2,'0')).join('');
    return {id:a.id,pk:hex(await pbkBits('pw-one',a.ks,256)),fk:hex(FKRAW)}})()`);
  await p.close();
  ok('0. the account was made and signed in once (its file key exists)', acct.fk && acct.fk.length == 64, acct);
  M.session = {ok: true, id: acct.id, n: 'Ann', r: 'admin', pk: acct.pk, fk: acct.fk};
  const open = async (hash, init) => { const pg = await ctx.newPage(); watch(pg); if (init) await pg.addInitScript(init);
    await pg.goto(URL0 + hash); await pg.waitForFunction(() => typeof oaOpen == 'function', null, {timeout: 30000}); return pg };
  // each app alone
  const APPS = ['notepad', 'calc', 'settings', 'explorer', 'paint', 'taskmgr'];
  for (const k of APPS) {
    p = await open('#app=' + k);
    let shown = true; try { await p.waitForSelector('.win.oamain', {timeout: 20000}) } catch (e) { shown = false }
    const g = await p.evaluate(`(()=>{const w=document.querySelector('.win.oamain'),vis=e=>!!e&&getComputedStyle(e).display!='none'&&e.getClientRects().length>0;
      const r=w&&w.getBoundingClientRect(),tb=w&&w.querySelector('.tb');
      return {user:USER&&USER.n,login:vis(document.getElementById('login')),bar:vis(document.getElementById('bar')),icons:[...document.querySelectorAll('#desk>.ic')].filter(vis).length,
        r:r&&[r.left,r.top,r.width,r.height],tb:tb&&vis(tb),title:document.title,wt:tb&&tb.querySelector('.t').textContent.trim(),wins:wins.length,cls:document.documentElement.classList.contains('oneapp'),
        home:HOME&&HOME.DOCUMENTS&&HOME.DOCUMENTS['NOTE.TXT'],app:OA.main&&OA.main.app}})()`);
    ok(`${k} 1. opens alone: signed in as the session's account, no sign-in screen`, shown && g.user == 'Ann' && !g.login && g.cls, g);
    ok(`${k} 2. ...filling the page, without its own title bar, the taskbar or the icons`, shown && g.r && g.r[0] == 0 && g.r[1] == 0 && g.r[2] == 1100 && g.r[3] == 760 && !g.tb && !g.bar && g.icons == 0, g);
    ok(`${k} 3. ...the page's title is the window's`, shown && g.title == g.wt && g.title.length > 0 && g.app == k, g);
    if (k == 'notepad') ok(`${k} 4. the home folder opened with the session's keys (a file written before is read)`, g.home == 'written before', g.home);
    const n0 = M.calls.filter(c => c == 'POST lsync').length;
    await p.evaluate('window.__oneAppClose()'); await wait(1200);
    const c = await p.evaluate(`({closed:!!document.getElementById('oaclosed'),main:!!document.querySelector('.win.oamain'),title:document.title})`);
    ok(`${k} 5. closed: its settings written first (the lsync copy), then "closed" (a browser tab a script cannot close)`, c.closed && !c.main && M.calls.filter(x => x == 'POST lsync').length > n0, [c, n0]);
    await p.close();
  }
  // what the page gives the native shell (step 2): the start menu's list and the drawn backgrounds, after sign-in
  p = await open('#app=calc'); await p.waitForSelector('.win.oamain', {timeout: 20000});
  const jpg = await p.evaluate(`(async()=>{const c=document.createElement('canvas');c.width=c.height=8;const u=c.toDataURL('image/jpeg',0.9);await kvSet(wpKey('wtest'),u);S.wpimg='wtest';applySet();return u.length})()`);
  await wait(1500);
  ok('16. a picture of your own: a copy for the shell (desktop/wallpaper, the JPEG, for this account)', M.wallpaper && M.wallpaper.q == 'acct=' + acct.id + '&id=wtest' && M.wallpaper.b[0] == 0xff && M.wallpaper.b[1] == 0xd8, M.wallpaper && [M.wallpaper.q, M.wallpaper.b.length, jpg]);
  await wait(1500);
  const cat = M.apps && JSON.parse(M.apps.b.toString());
  ok('15. after sign-in the page gives rudolfd the start menu\'s list and the backgrounds\' pictures (desktop/apps)', cat && cat.apps.some(a => a.k == 'notepad' && a.name == 'Notepad' && a.icon) && cat.bg.alpine && /^a\//.test(cat.bg.alpine.d) && cat.acct == acct.id, cat && {n: cat.apps.length, bg: Object.keys(cat.bg)});
  await p.close();
  // Settings with a change not applied: closing asks, as its ✕ does; the window stays until answered
  p = await open('#app=settings'); await p.waitForSelector('.win.oamain', {timeout: 20000});
  await p.evaluate("SW.show('disp',1)"); await wait(800); await p.selectOption('#dtws', 'retro'); await wait(300);
  await p.evaluate('window.__oneAppClose()'); await wait(800);
  const asked = await p.evaluate(`({main:!!document.querySelector('.win.oamain'),closed:!!document.getElementById('oaclosed'),dlg:[...document.querySelectorAll('button')].filter(b=>b.offsetParent).map(b=>b.textContent).join('|')})`);
  ok('6. Settings with a change not applied: closing asks first; the app stays', asked.main && !asked.closed && /Keep editing/.test(asked.dlg), asked);
  await p.evaluate("[...document.querySelectorAll('button')].find(b=>b.offsetParent&&b.textContent=='Discard').click()"); await wait(1200);
  ok('6b. ...Discard: then it closes', await p.evaluate("!!document.getElementById('oaclosed')&&!document.querySelector('.win.oamain')"));
  await p.close();
  // with the native shell: its messages (title, another app in a window of its own, close)
  const MS = "window.__msgs=[];window.webkit={messageHandlers:{rudolfNative:{postMessage:m=>window.__msgs.push(JSON.parse(JSON.stringify(m)))}}};";
  p = await open('#app=calc', MS); await p.waitForSelector('.win.oamain', {timeout: 20000}); await wait(300);
  let m = await p.evaluate('window.__msgs');
  ok('7. the native shell hears the title', m.some(x => x.op == 'title' && x.t == 'Calculator'), m);
  await p.evaluate("launch('notepad')"); await wait(400);
  m = await p.evaluate('window.__msgs'); const w2 = await p.evaluate('wins.length');
  ok('8. another app asked for: a window of its own (a message to the shell), not in this page', m.some(x => x.op == 'open' && x.app == 'notepad') && w2 == 1, [m, w2]);
  await p.evaluate('window.__oneAppClose()'); await wait(1200); m = await p.evaluate('window.__msgs');
  ok('9. ...and that the app closed', m.some(x => x.op == 'close'), m);
  await p.close();
  // 1.5.9 step 7: the development link's banner and question are the native shell's, not every app window's; and the
  // Native desktop's app windows are WebKit (html.webkit: the page draws dropdowns itself)
  M.devlink = {ok: true, available: true, on: true, repo: 'o/r', secondsLeft: 600, status: {state: 'on'}, pending: {id: 'c1', kind: 'run', what: 'ls'}}; M.engine = 'native';
  p = await open('#app=settings', MS); await p.waitForSelector('.win.oamain', {timeout: 20000}); await wait(5000);
  const dl = await p.evaluate(`({banner:!!document.getElementById('devlinkBanner'),ask:!!document.getElementById('devlinkAsk'),wk:document.documentElement.classList.contains('webkit'),
    top:document.querySelector('.win.oamain').getBoundingClientRect().top})`);
  ok('17. with the native shell: no development link banner or question in the app window (the shell shows them once)', !dl.banner && !dl.ask && dl.top == 0, dl);
  ok('18. ...and the page knows it is WebKit (html.webkit: dropdowns drawn by the page)', dl.wk, dl);
  await p.close();
  p = await open('#app=calc'); await p.waitForSelector('.win.oamain', {timeout: 20000}); await wait(5000);
  ok('19. a one-app page in a browser (no native shell): the banner as before', await p.evaluate("!!document.getElementById('devlinkBanner')"));
  await p.close(); M.devlink = null; M.engine = '';
  // no native session (the page opened by hand): the sign-in screen first, then the app alone
  M.session = null;
  p = await open('#app=calc'); await wait(1500);
  const lg = await p.evaluate(`(()=>{const L=document.getElementById('login');return {login:!!L&&getComputedStyle(L).display!='none',main:!!document.querySelector('.win.oamain')}})()`);
  ok('10. without a native session: the sign-in screen, no app yet', lg.login && !lg.main, lg);
  await p.fill('#lu', 'Ann'); await p.click('#lp'); await p.keyboard.type('pw-one'); await p.keyboard.press('Enter');
  let after = true; try { await p.waitForSelector('.win.oamain', {timeout: 20000}) } catch (e) { after = false }
  ok('11. ...signed in: Calculator alone', after && await p.evaluate("USER&&USER.n=='Ann'&&document.title=='Calculator'&&getComputedStyle(document.getElementById('bar')).display=='none'"));
  await p.close();
  // an app that does not exist
  M.session = {ok: true, id: acct.id, n: 'Ann', r: 'admin', pk: acct.pk, fk: acct.fk};
  p = await open('#app=nosuchapp'); await wait(2500);
  ok('12. an app that does not exist: said so', await p.evaluate("!!document.getElementById('oaclosed')&&/no app/.test(document.getElementById('oaclosed').textContent)"));
  await p.close();
  // the plain desktop is unchanged
  p = await open(''); await wait(2000);
  ok('13. without #app= the desktop is as before (sign-in screen, taskbar)', await p.evaluate("!document.documentElement.classList.contains('oneapp')&&getComputedStyle(document.getElementById('login')).display!='none'"));
  await p.close();
  ok('14. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
