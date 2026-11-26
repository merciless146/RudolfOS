// 1.6.3 (the owner, 2026-11-26, from FreeBuff's user test): the Command Prompt's HELP a beginner can act on.
// HELP: a short list in groups; HELP ALL: every command; HELP DIR: one command; an unknown name says so. And what the
// help promises works: CD \ goes to the top (before, it stayed where it was), EDIT of a new file opens Notepad, and the
// lines fit the Terminal's width. node termhelp63_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../..', 'iso/www'), PORT = 8757;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 700) : '')) };
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }
  const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200); fs.createReadStream(f).pipe(s) }).listen(PORT, '127.0.0.1');
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const p = await (await b.newContext({viewport: {width: 1280, height: 800}})).newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto(`http://127.0.0.1:${PORT}/`); await p.waitForFunction(() => typeof prompt == 'function' && typeof termHelp == 'function', null, {timeout: 30000});
  await p.evaluate(() => { try { svj('bd95setup', 1) } catch (e) {} ['oobe', 'login'].forEach(i => { const o = document.getElementById(i); o && o.remove() }); USER = USER || ACCTS[0]; prompt() });
  await p.waitForSelector('.term input', {timeout: 15000}); await p.waitForTimeout(500);
  const run = async c => { await p.evaluate(() => { document.querySelectorAll('.term .o').forEach(o => o.textContent = '') });
    await p.focus('.term input'); await p.fill('.term input', c); await p.press('.term input', 'Enter'); await p.waitForTimeout(250);
    return p.evaluate(() => [...document.querySelectorAll('.term .o')].map(o => o.textContent).join('')) };
  const h = await run('help'); ok('HELP: the short list in groups, with the way on', /FILES AND FOLDERS/.test(h) && /PROGRAMS/.test(h) && /THIS WINDOW/.test(h) && /HELP ALL/.test(h) && !/packages: rpk help/.test(h), h);
  const all = await run('HELP ALL'); ok('HELP ALL: every command', ['DIR', 'CD', 'MD', 'RD', 'TYPE', 'EDIT', 'COPY', 'REN', 'DEL', 'ECHO', 'CLS', 'VER', 'DATE', 'TIME', 'LUA', 'BLOCKLUA', 'EXPLORER', 'RPK', 'EXIT'].every(c => new RegExp('\\n  ' + c + '\\b').test(all)), all);
  const d = await run('help dir'); ok('HELP DIR: one command, with examples', /^help dir\n|DIR \[folder\]/.test(d) && /Example:/.test(d), d);
  const n = await run('HELP NOPE'); ok('an unknown name says so', /No help for "NOPE"\. Type HELP ALL for the list\./.test(n), n);
  const wide = await p.evaluate(() => { const o = document.querySelector('.term'); return {sw: o.scrollWidth, cw: o.clientWidth} });
  ok("the lines fit the Terminal's width", wide.sw <= wide.cw + 1, wide);
  await run('MD HELPTEST'); await run('CD HELPTEST'); const top = await run('CD \\'); const where = await run('CD');
  ok('CD \\ goes to the top, as the help says', /C:\s*$/.test(where.trim().split('\n').pop()), {top, where});
  await run('RD HELPTEST');
  const wn = await p.evaluate(() => wins.length); await run('EDIT HELPNEW.TXT'); await p.waitForTimeout(400);
  const np = await p.evaluate(() => { const w = [...document.querySelectorAll('.win')].find(x => /Notepad/.test(x.textContent)); return w ? w.textContent : '' });
  ok('EDIT of a new file opens it in Notepad by its name, with no error', /HELPNEW\.TXT/.test(np) && !/cannot be opened/.test(np), np.slice(0, 300));
  ok('no page errors', !errs.length, errs);
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); await b.close(); srv.close(); process.exit(R.every(Boolean) ? 0 : 1) })().catch(e => { console.log('FAIL ' + e.message); process.exit(1) });
