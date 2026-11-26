// 1.6.3 (the owner, 2026-11-26, FreeBuff's second round): the ☰ menu's ⏻ Power shut down at once in the browser edition,
// with no choice, and unsaved work was lost: closeAll() closed windows whatever they answered. Now ⏻ Power offers Shut down,
// Restart, Sign out and Cancel; shutting down, restarting and signing out first close the windows as their ✕ does, and a
// window's "Keep open" stops it with the desktop as it was. The system's own Shut down dialog (shutdownDlg) does the same
// before it asks rudolfd to power off. node power63_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../..', 'iso/www'), PORT = 8761;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 700) : '')) };
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }
  const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200); fs.createReadStream(f).pipe(s) }).listen(PORT, '127.0.0.1');
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const p = await (await b.newContext({viewport: {width: 1280, height: 800}})).newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto(`http://127.0.0.1:${PORT}/`); await p.waitForFunction(() => typeof notepad == 'function' && typeof closeAllAsk == 'function', null, {timeout: 30000});
  await p.evaluate(() => { svj('bd95setup', 1); ['oobe', 'login'].forEach(i => { const o = document.getElementById(i); o && o.remove() }); USER = USER || ACCTS[0]; SESSION = SESSION || {} });
  const unsaved = async () => { await p.evaluate(() => { explorer(); notepad() }); await p.waitForTimeout(500); await p.click('.win textarea'); await p.keyboard.type('shopping list'); };
  const state = () => p.evaluate(() => ({wins: wins.length, off: getComputedStyle(document.getElementById('off')).display, signedIn: !!SESSION, modals: [...document.querySelectorAll('.rmodal')].map(m => m.innerText.replace(/\s+/g, ' ').slice(0, 160))}));
  const pick = async t => { await p.click(`.rmodal button:has-text("${t}")`, {timeout: 4000}); await p.waitForTimeout(400) };
  // the browser edition's ⏻ Power: a choice, not an immediate shutdown
  await unsaved(); await p.evaluate(() => A.off()); await p.waitForTimeout(300); let s = await state();
  ok('⏻ Power offers Shut down, Restart, Sign out and Cancel', s.off == 'none' && s.modals.some(m => /Shut down.*Restart.*Sign out.*Cancel/.test(m)), s);
  await pick('Cancel'); s = await state(); ok('Cancel: nothing changes', s.wins == 2 && s.off == 'none' && !s.modals.length, s);
  await p.evaluate(() => A.off()); await p.waitForTimeout(300); await pick('Shut down'); s = await state();
  ok('Shut down with unsaved work: the window asks first', s.off == 'none' && s.modals.some(m => /not saved/.test(m)), s);
  await pick('Keep open'); await p.waitForTimeout(400); s = await state();
  ok('Keep open: no shutdown, the unsaved window still open', s.off == 'none' && s.wins >= 1 && await p.evaluate(() => wins.some(w => w.el.querySelector('textarea') && /shopping/.test(w.el.querySelector('textarea').value))), s);
  // Sign out asks too
  await p.evaluate(() => { A.logoff() }); await p.waitForTimeout(400); s = await state();
  ok('Sign out with unsaved work: the window asks first', s.signedIn && s.modals.some(m => /not saved/.test(m)), s);
  await pick('Keep open'); s = await state(); ok('Keep open: still signed in', s.signedIn && s.wins >= 1, s);
  // Shut down, then Close (don't save): it goes ahead
  await p.evaluate(() => A.off()); await p.waitForTimeout(300); await pick('Shut down'); await pick('Close'); await p.waitForTimeout(400); s = await state();
  ok('Close: the shutdown goes ahead', s.off != 'none' && s.wins == 0, s);
  // the system's Shut down dialog (shutdownDlg): asks before rudolfd is asked to power off
  await p.evaluate(() => { document.getElementById('off').style.display = 'none'; SESSION = {}; window.__rd = []; window.rdPost = u => { __rd.push(u); return Promise.resolve({}) } });
  await unsaved(); await p.evaluate(() => shutdownDlg()); await p.waitForTimeout(300); s = await state();
  ok("the system's Shut down dialog has Sign out", s.modals.some(m => /Sign out/.test(m)), s);
  await pick('Shut down'); s = await state(); ok("the system's Shut down asks about unsaved work first", s.modals.some(m => /not saved/.test(m)), s);
  await pick('Keep open'); s = await state();
  ok('Keep open: rudolfd is not asked to power off, the window stays', await p.evaluate(() => !__rd.length) && s.wins >= 1, {s, rd: await p.evaluate(() => __rd)});
  ok('no page errors', !errs.length, errs);
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); await b.close(); srv.close(); process.exit(R.every(Boolean) ? 0 : 1) })().catch(e => { console.log('FAIL ' + e.message); process.exit(1) });
