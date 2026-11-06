// 1.5.2 (emu26.js): Game Boy, Color and Advance games in Emu's game library (/home/rudolf/Games/GB): "Add games" lists
// them, the library shows them under Game Boy / GBA with readable names, and playing one hands the file to the Game
// Boy player. The desktop page with a stand-in rudolfd, in Chromium. Made-up files only, no games.
//   node emugb_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
const GB = '/home/rudolf/Games/GB', GAME = 'MADE UP_ YELLOW - SPECIAL EDITION.gbc', BYTES = Buffer.from('made-up game boy bytes');
const DIRS = {[GB]: [{name: GAME, dir: false, size: BYTES.length, mtime: 1}, {name: 'notes.txt', dir: false, size: 3, mtime: 1}],
  '/home/rudolf/Downloads': [{name: 'A.ZIP', dir: false, size: 9, mtime: 1}, {name: 'b.gba', dir: false, size: 9, mtime: 1}, {name: 'c.gb', dir: false, size: 9, mtime: 1}, {name: 'readme.txt', dir: false, size: 9, mtime: 1}]};
const api = (a, q) => { const sp = new URLSearchParams(q || '');
  if (a == 'fs/list') return {ok: true, items: DIRS[sp.get('path')] || []};
  if (a == 'games/info') return {ok: true, used: 1000, free: 5e10, persistent: true};
  if (a == 'disks') return {ok: true, disks: []};
  if (a == 'pkg/status') return {ok: true, installed: [], available: [], sources: []};
  if (a == 'events/wait') return {ok: true, events: []};
  return {ok: true} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/fs/read') { if (new URLSearchParams(qs).get('path') == GB + '/' + GAME) { s.writeHead(200); return s.end(BYTES) } s.writeHead(404); return s.end() }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs))) }
    s.writeHead(404); s.end() }) }).listen(8098, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8098/'); await p.waitForFunction(() => typeof emuLibrary == 'function' && typeof fs != 'undefined' && fs['C:'], null, {timeout: 30000});
  await p.evaluate(() => { try { svj('bd95setup', 1); svj('bd95emutab', 'gb') } catch (e) {} ['oobe', 'login'].forEach(i => { const o = document.getElementById(i); o && o.remove() }); USER = USER || ACCTS[0]; wins.slice().forEach(w => w.close());
    window.__gb = []; gbPlayer = () => ({loadAny: (u, n) => { window.__gb.push({n, s: new TextDecoder().decode(u)}); return true }}) });
  await p.evaluate(() => emuLibrary()); await wait(1500);
  const lib = await p.evaluate(() => [...wins.at(-1).body.querySelectorAll('.eltile')].map(t => t.innerText.replace(/\s+/g, ' ')));
  ok('1. the Game Boy / GBA tab shows the game added to the library', lib.length == 1, lib);
  ok('2. ...with a readable name (underscores and double spaces tidied) and "Game Boy"', /MADE UP YELLOW - SPECIAL EDITION.*Game Boy/.test(lib[0] || ''), lib);
  await p.evaluate(() => wins.at(-1).body.querySelector('.eltile').click()); await wait(800);
  const gb = await p.evaluate(() => window.__gb);
  ok('3. playing it hands the file from the library to the Game Boy player', gb.length == 1 && gb[0].n.endsWith('.gbc') && gb[0].s == 'made-up game boy bytes', gb);
  const ctx = await p.evaluate(() => { const t = wins.at(-1).body.querySelector('.eltile'); t.dispatchEvent(new MouseEvent('contextmenu', {bubbles: true, clientX: 50, clientY: 50})); const m = document.querySelector('.ctx'); const r = m ? m.innerText : 'no menu'; cmenuClose(); return r });
  ok('4. its menu can delete it from the library (it is a real file now)', /Delete from the library/.test(ctx), ctx);
  await p.evaluate(() => emuAddGames()); await wait(1200);
  const add = await p.evaluate(() => ({rows: [...wins.at(-1).body.querySelectorAll('.earow')].map(r => r.innerText.trim()), msg: wins.at(-1).body.querySelector('.msg').innerText}));
  ok('5. "Add games" lists Game Boy (.gb), Advance (.gba) and ZIP files, not text files', add.rows.length == 3 && add.rows.some(r => /b\.gba/.test(r)) && add.rows.some(r => /c\.gb/.test(r)) && !add.rows.some(r => /readme/.test(r)), add.rows);
  ok('6. no page errors', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1)
})().catch(e => { console.log('FAIL ' + e.stack); process.exit(1) });
