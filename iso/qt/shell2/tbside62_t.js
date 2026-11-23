// 1.6.2 (desktop/tbside62.js): with the taskbar at the left or the right, Quick settings, the notifications and the calendar open
// beside their buttons (not in the screen's bottom-right corner), a right-click menu stays off the taskbar, and the desktop's icons
// stay beside the taskbar as it grows. On the desktop page with a stand-in rudolfd (Native's side: rudolf-native.c fl_place_over and
// cm_open, built by the native checks).
//   node tbside62_t.js [CHROME]   (build iso/www first; port 8613, or TBSIDE_PORT)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path'), os = require('os'), cp = require('child_process');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 700) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.webp': 'image/webp', '.jpg': 'image/jpeg', '.png': 'image/png', '.woff2': 'font/woff2', '.json': 'application/json', '.wasm': 'application/wasm'};
const PORT = +process.env.TBSIDE_PORT || 8613, URL0 = 'http://127.0.0.1:' + PORT + '/';
const srv = http.createServer((q, s) => { const [u0] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    const J = (x, st) => { s.writeHead(st || 200, {'content-type': 'application/json'}); s.end(JSON.stringify(x)) };
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }
    if (u.startsWith('/api/')) { const a = u.slice(5); if (a == 'events/wait') return setTimeout(() => J({ok: true, events: []}), 20000);
      if (/^sys\//.test(a)) return J({ok: false, error: 'not in this test'}, 404); return J({ok: true, items: [], apps: [], windows: [], events: []}) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(PORT, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.addInitScript(() => { window.RUDOLFD = {token: 't', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0} });
  await p.goto(URL0); await p.waitForFunction(() => typeof tbSidePlace == 'function' && typeof acctNew == 'function', null, {timeout: 30000});
  await p.evaluate(() => { try { svj('bd95setup', 1) } catch (e) {} ['oobe', 'login'].forEach(i => { const o = document.getElementById(i); o && o.remove() }); ACCTS = [{n: 'Ann', r: 'admin', h: 'x', id: 'ann'}]; USER = ACCTS[0]; try { userUI() } catch (e) {} });
  await wait(1200);
  const R_ = (sel) => p.evaluate(s => { const e = document.querySelector(s); if (!e) return null; const r = e.getBoundingClientRect(); return [r.left, r.top, r.right, r.bottom].map(Math.round) }, sel);
  for (const side of ['left', 'right']) {
    await p.evaluate(s => { S.tb = s; applySet() }, side); await wait(600); await p.mouse.click(640, 300); await p.keyboard.press('Escape');
    const bar = await R_('#bar'); const bad = [];
    for (const [btn, pop] of [['#qsb', '#qsp'], ['#ncb', '#ncp'], ['#tray', '#calp']]) {
      await p.click(btn); await wait(500); const r = await R_(pop), bt = await R_(btn);
      if (!r) { bad.push([pop, 'not open']); continue }
      const beside = side == 'left' ? r[0] >= bar[2] && r[0] - bt[2] <= 12 : r[2] <= bar[0] && bt[0] - r[2] <= 12;
      const onScreen = r[1] >= 0 && r[3] <= 800 && r[0] >= 0 && r[2] <= 1280, level = Math.abs(r[1] - bt[1]) < 2 || r[3] >= 790 || r[1] <= 6;
      if (!(beside && onScreen && level)) bad.push([pop, r, bt, bar]);
      await p.keyboard.press('Escape'); await p.mouse.click(640, 300); await wait(250); if (await R_(pop)) { await p.click(btn); await wait(200) } }
    ok(`1${side == 'left' ? 'a' : 'b'}. taskbar at the ${side}: Quick settings, the notifications and the calendar open beside their buttons, level with them, on the screen, off the taskbar`, bad.length == 0, bad);
    await p.mouse.click(side == 'left' ? bar[0] + 10 : bar[2] - 10, 500, {button: 'right'}); await wait(400); const m = await R_('.ctx');
    ok(`2${side == 'left' ? 'a' : 'b'}. ...the taskbar's right-click menu off the taskbar, beside it`, m && (side == 'left' ? m[0] >= bar[2] : m[2] <= bar[0]), [m, bar]);
    await p.keyboard.press('Escape'); await p.mouse.click(640, 300);
    /* the taskbar grows (a wider part in it): the desktop moves at once, not at the clock's next minute */
    const g = await p.evaluate(async () => { const e = document.createElement('div'); e.id = 'tbside-t'; e.style.cssText = 'width:260px;height:4px;flex:0 0 auto'; document.getElementById('bar').appendChild(e);
      await new Promise(r => requestAnimationFrame(() => requestAnimationFrame(r))); await new Promise(r => setTimeout(r, 100));
      const b = document.getElementById('bar').getBoundingClientRect(), d = document.getElementById('desk').getBoundingClientRect(); e.remove(); return [Math.round(b.left), Math.round(b.right), Math.round(d.left), Math.round(d.right)] });
    ok(`3${side == 'left' ? 'a' : 'b'}. ...the taskbar grown: the desktop (its icons) beside it at once`, side == 'left' ? g[2] >= g[1] : g[3] <= g[0], g) }
  /* at the bottom, as before: the corner */
  await p.evaluate(() => { S.tb = 'bottom'; applySet() }); await wait(500); await p.click('#qsb'); await wait(400); const q = await R_('#qsp');
  ok('4. taskbar at the bottom: Quick settings where it always was (the bottom-right corner)', q && 1280 - q[2] <= 10 && q[3] <= 800 - 40, q);
  ok('5. no page errors', errs.length == 0, errs);
  const f = R.filter(x => !x).length; console.log((R.length - f) + '/' + R.length + ' passed'); await b.close(); srv.close(); process.exit(f ? 1 : 0)
})().catch(e => { console.log('FAIL ' + e.stack); process.exit(1) });
