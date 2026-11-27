// 1.6.3 (the owner, 2026-11-26, FreeBuff's second round): the desktop's New text document and New folder asked for the
// name in a box titled "Rename" with a Rename button, and a name typed without .TXT left a file no app opened. Now:
// "Name your new file" / "Name your new folder" and Create, .TXT added when left out; Rename itself is unchanged.
// (The native desktop's dialog: native59_t.sh 61-62.) node newfile63_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../..', 'iso/www'), PORT = 8762;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 700) : '')) };
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }
  const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200); fs.createReadStream(f).pipe(s) }).listen(PORT, '127.0.0.1');
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const p = await (await b.newContext({viewport: {width: 1280, height: 800}})).newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto(`http://127.0.0.1:${PORT}/`); await p.waitForFunction(() => typeof deskNew == 'function' && typeof renameDlg == 'function', null, {timeout: 30000});
  await p.evaluate(() => { svj('bd95setup', 1); ['oobe', 'login'].forEach(i => { const o = document.getElementById(i); o && o.remove() }); USER = USER || ACCTS[0]; SESSION = SESSION || {}; let d = node(['C:']); for (const x of homePath('DESKTOP').slice(1)) { d[x] = d[x] || {}; d = d[x] } save() });
  const dlg = () => p.evaluate(() => { const m = document.querySelector('.rmodal'); return m ? {h: m.querySelector('h3').textContent, ok: m.querySelector('.ok').textContent, v: m.querySelector('input').value} : null });
  const name = async t => { await p.fill('.rmodal input', t); await p.press('.rmodal input', 'Enter'); await p.waitForTimeout(300) };
  const desk = () => p.evaluate(() => Object.keys(node(homePath('DESKTOP')) || {}));
  await p.evaluate(() => deskNew(0)); await p.waitForTimeout(300); let d = await dlg();
  ok('New text document: "Name your new file" and Create', d && d.h == 'Name your new file' && d.ok == 'Create' && d.v == 'NEW TEXT DOCUMENT.TXT', d);
  await name('shopping'); let k = await desk();
  ok('a name typed without .TXT gets it: SHOPPING.TXT', k.includes('SHOPPING.TXT') && !k.includes('SHOPPING') && !k.includes('NEW TEXT DOCUMENT.TXT'), k);
  await p.evaluate(() => deskNew(0)); await p.waitForTimeout(300); await name('list.md'); k = await desk();
  ok('a name with its own ending keeps it: LIST.MD', k.includes('LIST.MD'), k);
  await p.evaluate(() => deskNew(1)); await p.waitForTimeout(300); d = await dlg();
  ok('New folder: "Name your new folder" and Create', d && d.h == 'Name your new folder' && d.ok == 'Create', d);
  await name('projects'); k = await desk(); ok('a folder gets no .TXT: PROJECTS', k.includes('PROJECTS'), k);
  await p.evaluate(() => renameDlg(homePath('DESKTOP').concat('SHOPPING.TXT'))); await p.waitForTimeout(300); d = await dlg();
  ok('Rename is unchanged: "Rename", Rename', d && d.h == 'Rename' && d.ok == 'Rename', d);
  await name('groceries'); k = await desk(); ok('...and adds nothing to the name: GROCERIES', k.includes('GROCERIES') && !k.includes('GROCERIES.TXT'), k);
  ok('no page errors', !errs.length, errs);
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); await b.close(); srv.close(); process.exit(R.every(Boolean) ? 0 : 1) })().catch(e => { console.log('FAIL ' + e.message); process.exit(1) });
