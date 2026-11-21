// 1.6.1 (the owner, 2026-11-20): the desktop's file picker (picker26.js) reads at most what this computer's memory allows
// (rfMaxFor: a sixth of the memory, at most 40% of what is free, never over 2 GB; 512 MB without rudolfd's numbers).
// rudolfd stood in.   node pickermem61_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www'); const MB = 1048576, GB = 1024 * MB;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 400) : '')) };
let MEM = {ok: true}; const READ = [];
const DL = [{name: 'six.bin', dir: false, size: 600 * MB, mtime: 1}, {name: 'four.bin', dir: false, size: 400 * MB, mtime: 1}];
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/fs/read') { READ.push(new URLSearchParams(qs).get('path')); s.writeHead(200); return s.end('x') }
    if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":""}') }, 20000);
    if (u.startsWith('/api/')) { const a = u.slice(5); s.writeHead(200, {'content-type': 'application/json'});
      return s.end(JSON.stringify(a == 'memory' ? MEM : a == 'fs/list' ? {ok: true, items: DL} : {ok: true})) }
    s.writeHead(404); s.end() }) }).listen(8614, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto('http://127.0.0.1:8614/'); await p.waitForFunction(() => typeof rfMaxFor == 'function' && typeof rfPick == 'function', null, {timeout: 30000});
  await p.evaluate(() => { try { svj('bd95setup', 1) } catch (e) {} ['oobe', 'login'].forEach(i => { const o = document.getElementById(i); o && o.remove() }) });
  const f = (t, a) => p.evaluate(([t, a]) => rfMaxFor(t, a) / 1048576, [t, a]);
  ok('1. the Vivobook (3.3 GB, 1.5 GB free): about 512 MB, as before', Math.abs(await f(3.3 * GB, 1.5 * GB) - 520) < 10, await f(3.3 * GB, 1.5 * GB));
  ok('2. 8 GB (5 GB free): about 1.2 GB', Math.abs(await f(8 * GB, 5 * GB) - 1260) < 15, await f(8 * GB, 5 * GB));
  ok('3. 16 GB or more: 2 GB, never more', await f(16 * GB, 12 * GB) == 2048 && await f(64 * GB, 60 * GB) == 2048);
  ok('4. little free now (400 MB): 160 MB, so a large file cannot end the desktop', await f(3.3 * GB, 400 * MB) == 160, await f(3.3 * GB, 400 * MB));
  ok('5. almost nothing free: never below 128 MB; a tiny computer (1 GB): 256 MB at most', await f(3.3 * GB, 50 * MB) == 128 && await f(1 * GB, 900 * MB) == 256);
  ok('6. no numbers from rudolfd: 512 MB, as before', await f(0, 0) == 512 && await f(NaN, NaN) == 512);
  const pick = async n => { await p.evaluate(() => { const i = document.createElement('input'); i.type = 'file'; rfPick(i) }); await p.waitForSelector('.rfpick .pkr');
    await p.locator('.rfpick .pkr', {hasText: n}).dblclick(); await wait(400); const m = await p.evaluate(() => (document.querySelector('.rfpick .rfmsg') || {}).textContent || ''); await p.evaluate(() => document.querySelectorAll('.rfpick').forEach(x => x.remove())); return m };
  MEM = {ok: true, total: 3.3 * GB, available: 1.5 * GB};
  const m7 = await pick('six.bin');
  ok('7. the picker on the Vivobook: a 600 MB file is refused with the limit and why', /larger than 5(19|20)\.0 MB, the most this computer's memory lets an app take in at once/.test(m7) && !READ.length, m7);
  MEM = {ok: true, total: 16 * GB, available: 12 * GB}; READ.length = 0;
  await pick('six.bin');
  ok('8. on a 16 GB computer the same file is read', READ.some(x => /six\.bin$/.test(x)), READ);
  MEM = {ok: true, total: 3.3 * GB, available: 600 * MB}; READ.length = 0;
  const m9 = await pick('four.bin');
  ok('9. the Vivobook with little free memory now: a 400 MB file waits (240 MB now)', /larger than 240\.0 MB/.test(m9) && !READ.length, m9);
  ok('no page errors', !errs.length, errs);
  await b.close(); srv.close();
  const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
