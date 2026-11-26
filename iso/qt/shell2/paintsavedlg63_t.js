// 1.6.3 (the owner, 2026-11-26, FreeBuff's second round): Paint's Save used a small box "File name (relative to C:\)" with
// no folder and no Cancel, and saved to C:\ while Pictures stayed empty. Now it is the Save dialog Notepad has, opening
// in the account's Pictures: the picture is saved there, Cancel leaves it unsaved, and closing an unsaved picture then
// choosing Save then Cancel keeps the window open. node paintsavedlg63_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../..', 'iso/www'), PORT = 8759;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 700) : '')) };
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }
  const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200); fs.createReadStream(f).pipe(s) }).listen(PORT, '127.0.0.1');
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const p = await (await b.newContext({viewport: {width: 1280, height: 800}})).newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto(`http://127.0.0.1:${PORT}/`); await p.waitForFunction(() => typeof paint == 'function' && typeof fileDlg == 'function', null, {timeout: 30000});
  await p.evaluate(() => { try { svj('bd95setup', 1) } catch (e) {} ['oobe', 'login'].forEach(i => { const o = document.getElementById(i); o && o.remove() }); USER = USER || ACCTS[0];
    const h = homePath(); let d = node(['C:']); for (const k of h.slice(1)) { d[k] = d[k] || {}; d = d[k] } d.PICTURES = d.PICTURES || {}; save() });
  const draw = async () => { await p.evaluate(() => paint()); await p.waitForTimeout(400); const c = await p.$('.win:last-of-type canvas') || await p.$('canvas');
    const bb = await c.boundingBox(); await p.mouse.move(bb.x + 20, bb.y + 20); await p.mouse.down(); await p.mouse.move(bb.x + 80, bb.y + 60); await p.mouse.up() };
  await draw();
  await p.click('.win .sv'); await p.waitForSelector('.fdlg', {timeout: 5000});
  const dlg = await p.evaluate(() => ({path: document.querySelector('.fdlg .fdpath').value, name: document.querySelector('.fdlg .fdname, .fdlg input:not([readonly])').value}));
  ok('Save opens the Save dialog, in Pictures, named PICTURE.PNG', /\\PICTURES$/.test(dlg.path) && dlg.name == 'PICTURE.PNG', dlg);
  await p.click('.fdlg .fdx'); await p.waitForTimeout(200);
  ok('Cancel: nothing saved, the dialog gone', await p.evaluate(() => !document.querySelector('.fdlg') && node([...homePath('PICTURES'), 'PICTURE.PNG']) === undefined && node(['C:', 'PICTURE.PNG']) === undefined));
  await p.click('.win .sv'); await p.waitForSelector('.fdlg'); await p.fill('.fdlg .fdname, .fdlg input:not([readonly])', 'MY DRAWING'); await p.click('.fdlg .fdok'); await p.waitForTimeout(400);
  const saved = await p.evaluate(() => { const v = node([...homePath('PICTURES'), 'MY DRAWING.PNG']); return {pics: typeof v == 'string' && v.startsWith('data:image/png'), root: node(['C:', 'MY DRAWING.PNG']) !== undefined} });
  ok('saved in Pictures as MY DRAWING.PNG, not in C:\\', saved.pics && !saved.root, saved);
  await draw(); const n0 = await p.evaluate(() => wins.length);
  await p.evaluate(() => wins.at(-1).close()); await p.waitForTimeout(300);
  const askBtn = await p.$('.rmodal button:has-text("Save")'); if (askBtn) { await askBtn.click(); await p.waitForSelector('.fdlg'); await p.click('.fdlg .fdx'); await p.waitForTimeout(300) }
  ok('close, then Save, then Cancel: the unsaved picture stays open', !!askBtn && await p.evaluate(n => wins.length == n, n0));
  ok('no page errors', !errs.length, errs);
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); await b.close(); srv.close(); process.exit(R.every(Boolean) ? 0 : 1) })().catch(e => { console.log('FAIL ' + e.message); process.exit(1) });
