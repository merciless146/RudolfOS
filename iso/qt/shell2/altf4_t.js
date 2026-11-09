// 1.5.6: Alt+F4 with the desktop's window in front arrives as Alt+F20 (rudolf-display: Chromium closed the desktop's
// whole window on Alt+F4, and the Vivobook went back to the sign-in screen). The desktop reads it as Alt+F4 again:
// the window in front closes, and only that one. Ctrl+F20 is still Ctrl+W.
//   node altf4_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8146, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd|debian/.test(e.stack || e.message || '')) errs.push(e.message) });
  const q = s => p.evaluate(s);
  // F20 as X delivers it (Playwright has no F20): a trusted key through the DevTools protocol. mods: 1 Alt, 2 Ctrl
  const cdp = await p.context().newCDPSession(p);
  const f20 = async mods => { for (const type of ['rawKeyDown', 'keyUp']) await cdp.send('Input.dispatchKeyEvent', {type, key: 'F20', code: 'F20', windowsVirtualKeyCode: 131, nativeVirtualKeyCode: 131, modifiers: mods}) };
  await p.goto('http://127.0.0.1:8146/'); await p.waitForFunction(() => typeof notepad == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    ACCTS.length=0;ACCTS.push({n:'boss',h:await pwMake('b1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'b1');return 1})()`);
  await q("wins.slice().forEach(w=>w.close());notepad();notepad();1"); await wait(500);
  const n0 = await q("wins.length"), top = await q("(()=>{const w=actWin();return w&&wins.indexOf(w)})()");
  await f20(1); await wait(500);
  const s1 = await q("({n:wins.length})");
  ok('1. Alt+F20 (Alt+F4 from rudolf-display) closes the window in front, and only that one', n0 == 2 && top >= 0 && s1.n == 1, {n0, top, s1});
  await f20(1); await wait(500);
  ok('2. again: the other window closes; the desktop stays', await q("wins.length") == 0 && await q("!!document.getElementById('desk')"));
  await f20(1); await wait(300);
  ok('3. with no window open nothing happens (and nothing breaks)', await q("wins.length") == 0);
  // Ctrl+F20 is still Ctrl+W for the desktop's apps
  await q("window.__cw=0;addEventListener('keydown',e=>{if(e.ctrlKey&&e.key=='w'&&e.code=='KeyW')__cw++});1");
  await f20(2); await wait(200);
  ok('4. Ctrl+F20 still arrives as Ctrl+W', await q("__cw") == 1, await q("__cw"));
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
