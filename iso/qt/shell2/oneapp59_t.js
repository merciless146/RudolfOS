// 1.5.9 (plan Part 12, step 1): the page's one-app mode, index.html#app=NAME, for the native desktop engine. A stand-in
// rudolfd (desktop/session as step 2's rudolfd answers it), a stand-in native shell (window.webkit.messageHandlers.
// rudolfNative, as rudolf-native gives its app windows).   node oneapp59_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 600) : '')) };
let SESSION = null; const CALLS = [];
const srv = http.createServer((q, s) => { const [u0] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/api/')) { const a = u.slice(5); CALLS.push(q.method + ' ' + a);
      if (a == 'events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      if (a == 'desktop/session') { if (!SESSION) { s.writeHead(404, {'content-type': 'application/json'}); return s.end('{"ok":false,"error":"no native session"}') }
        s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(Object.assign({ok: true}, SESSION))) }
      s.writeHead(200, {'content-type': 'application/json'}); return s.end('{"ok":true}') }
    const f = path.join(WWW, u); if (u.startsWith('/a/') && fs.existsSync(f)) { s.writeHead(200); return s.end(fs.readFileSync(f)) }
    s.writeHead(404); s.end() }) }).listen(8295, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
const NATIVE = "window.__rn=[];window.webkit={messageHandlers:{rudolfNative:{postMessage:m=>window.__rn.push(JSON.parse(JSON.stringify(m)))}}};";
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const ctx = await b.newContext({viewport: {width: 1000, height: 700}});
  const errs = []; const watch = p => p.on('pageerror', e => { if (/oneapp|oa[A-Z]|__oneApp|__rnIn/.test(e.stack || '')) errs.push(e.message) });
  // an account, as a desktop sign-in leaves it (its file key made), and a last session with File Explorer open
  const p0 = await ctx.newPage(); watch(p0); await p0.goto('http://127.0.0.1:8295/'); await p0.waitForFunction(() => typeof oaMerge3 == 'function', null, {timeout: 30000});
  const acct = await p0.evaluate(async () => { svj('bd95setup', 1); ['oobe'].forEach(i => { const o = document.getElementById(i); o && o.remove() });
    await FSREADY; const a = await acctNew('Ann', 'pw1', 'admin'); ACCTS = [a]; window.__sys = 1; saveUser(); USER = a; await userLogin(a, 'pw1'); saveUser();
    svj(sessK(), ['explorer']); fs['C:'].SHARED = {}; fsSave(); fsFlush(); await new Promise(r => setTimeout(r, 1500));
    const pk = [...new Uint8Array(await pbkBits('pw1', a.ks, 256))].map(x => x.toString(16).padStart(2, '0')).join('');
    return {id: a.id, pk, fk: !!a.fk} });
  ok('0. (an account with its file key, as a desktop sign-in leaves it)', acct.fk && acct.pk.length == 64, acct);
  await p0.close();
  SESSION = {user: {n: 'Ann', id: acct.id, r: 'admin'}, pk: acct.pk};
  const open = async (hash, native = true) => { const p = await ctx.newPage(); watch(p); if (native) await p.addInitScript(NATIVE);
    CALLS.length = 0; await p.goto('http://127.0.0.1:8295/' + hash); await p.waitForFunction(() => typeof oaMerge3 == 'function', null, {timeout: 30000}); return p };
  const state = p => p.evaluate(() => { const vis = e => !!e && getComputedStyle(e).display != 'none' && (e.offsetParent !== null || getComputedStyle(e).position == 'fixed');
    const m = ONEAPP && ONEAPP.main, r = m && m.el.getBoundingClientRect(), tb = m && m.el.querySelector('.tb');
    return {user: USER && USER.n, signed: !!SESSION, login: vis(document.getElementById('login')), bar: vis(document.getElementById('bar')), icons: [...document.querySelectorAll('#desk .ic')].filter(vis).length,
      wins: wins.length, apps: wins.map(w => w.app), main: !!m, rect: r && [r.left, r.top, r.width, r.height], inner: [innerWidth, innerHeight], tb: tb && getComputedStyle(tb).display, title: document.title,
      home: !!HOME, rn: window.__rn || null, msg: (document.getElementById('oamsg') || {}).textContent || ''} });
  // 1-7: Notepad, signed in from the native session
  let p = await open('#app=notepad'); await p.waitForFunction(() => ONEAPP.main, null, {timeout: 20000}).catch(() => {}); await wait(1500);
  let s = await state(p);
  ok('1. #app=notepad: signed in by itself from the native session (no sign-in screen), the home folder open', s.user == 'Ann' && s.signed && !s.login && s.home, s);
  ok('2. ...only Notepad: no taskbar, no desktop icons, and the last session\'s windows are not reopened', !s.bar && s.icons == 0 && s.wins == 1 && s.apps[0] == 'notepad', s);
  ok('3. ...its window fills the page', s.main && JSON.stringify(s.rect) == JSON.stringify([0, 0, ...s.inner]), [s.rect, s.inner]);
  ok("4. ...in the native shell's frame: the page's own title bar is hidden", s.tb == 'none', s.tb);
  ok("5. ...the window's title is the page's, and the native shell is told (title, ready)", /Notepad/.test(s.title) && s.rn.some(m => m.op == 'title' && /Notepad/.test(m.t)) && s.rn.some(m => m.op == 'ready' && m.app == 'notepad'), s);
  ok('6. ...no heartbeat (the native shell sends it), and the session came from desktop/session', !CALLS.some(c => / heartbeat$/.test(c)) && CALLS.includes('GET desktop/session'), CALLS.filter(c => !/idbm/.test(c)));
  await p.evaluate(() => { ONEAPP.main.setTitle('NOTE.TXT - Notepad') }); await wait(200);
  s = await state(p); ok('7. a new title follows (document.title and the native shell)', s.title == 'NOTE.TXT - Notepad' && s.rn.some(m => m.op == 'title' && m.t == 'NOTE.TXT - Notepad'), s);
  // 8-9: another app goes to the native shell; a window the app opens stays in the page
  await p.evaluate(() => { launch('calc') }); await wait(300); s = await state(p);
  ok('8. another app (launch) goes to the native shell as a window of its own', s.wins == 1 && s.rn.some(m => m.op == 'open' && m.app == 'calc'), s);
  await p.evaluate(() => { win('A dialog', '💬', 300, 200) }); await wait(200); s = await state(p);
  ok("9. ...a window the app opens itself stays in the page (as on the desktop)", s.wins == 2 && s.main, s);
  // 10-11: closing: an app that says no keeps its window; then it closes and the page with it
  await p.evaluate(() => { ONEAPP.main.onclose = () => false; window.__rnIn({op: 'close'}) }); await wait(300); s = await state(p);
  ok("10. the native frame's ✕ (__rnIn close) asks the app first: an app that says no stays open", s.main && s.wins == 2 && !s.rn.some(m => m.op == 'close'), s);
  await p.evaluate(() => { ONEAPP.main.onclose = null; fs['C:'].SHARED['FROMNOTE.TXT'] = 'x'; fsSave(); window.__oneAppClose() }); await wait(1500); s = await state(p);
  ok('11. closing it writes its files and closes the page (close to the native shell)', s.rn.some(m => m.op == 'close') && /closed/.test(s.msg), s);
  await p.close();
  // 12-14: other apps
  for (const [k, n] of [['settings', '12'], ['calc', '13'], ['explorer', '14']]) {
    p = await open('#app=' + k); await p.waitForFunction(() => ONEAPP.main, null, {timeout: 20000}).catch(() => {}); await wait(1000); s = await state(p);
    ok(n + '. #app=' + k + ': that app alone, filling the page', s.main && s.wins == 1 && s.apps[0] == k && JSON.stringify(s.rect) == JSON.stringify([0, 0, ...s.inner]) && !s.bar, s);
    await p.close() }
  // 15: without the native shell (Standard or Lite, a browser): the title bar stays, without minimise and maximise
  p = await open('#app=notepad', false); await p.waitForFunction(() => ONEAPP.main, null, {timeout: 20000}).catch(() => {}); await wait(800);
  const tbs = await p.evaluate(() => { const t = ONEAPP.main.el.querySelector('.tb'), d = s => getComputedStyle(t.querySelector(s)).display; return [getComputedStyle(t).display, d('.mn'), d('.mx'), d('.cl')] });
  ok("15. without the native shell the page's title bar stays, with close only", tbs[0] != 'none' && tbs[1] == 'none' && tbs[2] == 'none' && tbs[3] != 'none', tbs);
  await p.close();
  // 16: without a native session, the page's own sign-in screen first, then the app alone
  SESSION = null; p = await open('#app=calc'); await wait(2500); s = await state(p);
  const before = s; await p.fill('#lu', 'Ann'); await p.click('#lp'); await p.keyboard.type('pw1'); await p.keyboard.press('Enter');
  await p.waitForFunction(() => ONEAPP.main, null, {timeout: 20000}).catch(() => {}); await wait(800); s = await state(p);
  ok("16. without a native session: the page's sign-in screen, then the app alone", before.login && !before.main && s.user == 'Ann' && s.main && s.apps.join() == 'calc' && !s.login, [before, s]);
  await p.close(); SESSION = {user: {n: 'Ann', id: acct.id, r: 'admin'}, pk: acct.pk};
  // 17: an app that does not exist
  p = await open('#app=nosuchapp'); await wait(1500); s = await state(p);
  ok('17. an app that does not exist: said, and the window closes after a few seconds', /no app called/.test(s.msg) && !s.main, s);
  await wait(4500); s = await state(p); ok('18. ...(close sent)', s.rn.some(m => m.op == 'close'), s.rn); await p.close();
  // 19-20: two app pages save C: at once: both changes are kept
  const pa = await open('#app=notepad'), pb = await open('#app=notepad');
  for (const x of [pa, pb]) await x.waitForFunction(() => ONEAPP.main && OAFS.base, null, {timeout: 20000});
  await pa.evaluate(() => { fs['C:'].SHARED['A.TXT'] = 'from A'; fsSave() }); await pb.evaluate(() => { fs['C:'].SHARED['B.TXT'] = 'from B'; fsSave() });
  await wait(2000);
  const seen = await Promise.all([pa, pb].map(x => x.evaluate(() => Object.keys(fs['C:'].SHARED).sort().join())));
  const pc = await open('#app=notepad'); await pc.waitForFunction(() => ONEAPP.main, null, {timeout: 20000});
  const stored = await pc.evaluate(async () => { const v = await kvGet('fs:system'); return Object.keys(v['C:'].SHARED).sort().join() });
  ok('19. two app pages saving C: at once: both files are kept', stored == 'A.TXT,B.TXT,FROMNOTE.TXT', stored);
  ok("20. ...and each page has the other's file", seen.every(x => x == 'A.TXT,B.TXT,FROMNOTE.TXT'), seen);
  await pb.evaluate(() => { delete fs['C:'].SHARED['A.TXT']; fsSave() }); await wait(1500);
  ok('21. a file deleted in one page is gone from the other too', await pa.evaluate(() => !('A.TXT' in fs['C:'].SHARED)) && !(await pc.evaluate(async () => 'A.TXT' in (await kvGet('fs:system'))['C:'].SHARED)));
  ok('22. the merge: changes on both sides kept, a conflict keeps this side', await pa.evaluate(() => JSON.stringify(oaMerge3({a: '1', b: '1', c: {x: '1'}}, {a: '2', b: '1', c: {x: '1', y: '2'}}, {a: '3', b: '3', c: {x: '1', z: '3'}, d: '3'})) == JSON.stringify({a: '2', b: '3', c: {x: '1', y: '2', z: '3'}, d: '3'})));
  ok('23. accounts: an app page other than Settings does not write them', await pa.evaluate(() => { const v = localStorage.getItem('bd95user'); USER.zz = 1; saveUser(); const w = localStorage.getItem('bd95user'); delete USER.zz; return v == w }));
  ok('24. a page opened without #app is the desktop as always (no one-app mode)', await (async () => { const d = await open('', false); const r = await d.evaluate(() => ONEAPP === null && !document.documentElement.classList.contains('oneapp')); await d.close(); return r })());
  ok('25. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
