// Files 1.2.7: copy folders from a USB drive or memory card into Downloads, in the desktop page with a stand-in
// rudolfd (one drive "PHOTOS", its files in memory), in Chromium. node files_t.js [CHROME]
// The page is iso/www/index.html (build it first: python3 desktop/build19.py iso/www/index.html).
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.mjs': 'text/javascript', '.html': 'text/html'};
// the drive: /media/sdb1/Trip (three folders deep, 40 files), a file over 256 MB and one that cannot be read
const MP = '/media/sdb1', FILES = new Map(), DIRS = new Set([MP]), BIG = new Map(), BAD = new Set(), M = {slow: 0, reads: 0};
const addF = (p, s) => { FILES.set(p, Buffer.from(s)); let d = path.dirname(p); while (d.length > MP.length) { DIRS.add(d); d = path.dirname(d) } };
for (let i = 1; i <= 40; i++) addF(`${MP}/Trip/${['Day 1', 'Day 2', 'Day 2/Beach', 'Day 2/Beach/Sunset'][i % 4]}/note-${i}.txt`, `note number ${i}\n`);
addF(`${MP}/Trip/video.mp4`, 'x'); BIG.set(`${MP}/Trip/video.mp4`, 300 * 1048576);
addF(`${MP}/Trip/locked.txt`, 'secret'); BAD.add(`${MP}/Trip/locked.txt`);
addF(`${MP}/Small/one.txt`, 'one\n'); addF(`${MP}/Small/two.txt`, 'two\n');
const size = p => BIG.get(p) || FILES.get(p).length;
const kids = d => [...new Set([...FILES.keys(), ...DIRS].filter(k => path.dirname(k) == d && k != d))];
const api = (a, q) => { const p = new URLSearchParams(q || '');
  if (a == 'info') return {ok: true, mediaDev: '/dev/sda1', live: false, media: 'usb', boot: 'uefi'};
  if (a == 'disks') return {ok: true, disks: [{name: 'sdb', bytes: 32e9, removable: true, model: 'SanDisk Ultra', partitions: [{name: 'sdb1', bytes: 32e9, fs: 'vfat', label: 'PHOTOS', mount: MP}]}]};
  if (a == 'fs/list') { const d = p.get('path'); if (!DIRS.has(d)) return {__status: 404, ok: false, error: 'no such folder'};
    return {ok: true, items: kids(d).map(k => ({name: path.basename(k), dir: DIRS.has(k), size: DIRS.has(k) ? 0 : size(k), mtime: 1}))} }
  if (a == 'fs/tree') { const d = p.get('path'); if (!DIRS.has(d)) return {__status: 404, ok: false, error: 'no such folder'};
    const items = []; const walk = x => kids(x).forEach(k => { items.push({p: k.slice(d.length + 1), dir: DIRS.has(k), size: DIRS.has(k) ? 0 : size(k), mtime: 1}); if (DIRS.has(k)) walk(k) }); walk(d);
    return {ok: true, items} }
  return {ok: true};
};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => 0); q.on('end', async () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"usb",live:0,recovery:0};</script>')) }
    if (u == '/api/fs/read') { const f = new URLSearchParams(qs).get('path'); M.reads++; if (M.slow) await new Promise(r => setTimeout(r, M.slow));
      if (!FILES.has(f) || BAD.has(f)) { s.writeHead(403, {'content-type': 'application/json'}); return s.end('{"ok":false,"error":"permission denied"}') }
      s.writeHead(200, {'content-type': 'application/octet-stream'}); return s.end(FILES.get(f)) }
    if (u.startsWith('/api/')) { const r = api(u.slice(5), qs); s.writeHead(r.__status || 200, {'content-type': 'application/json'}); return s.end(JSON.stringify(r)) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(8093, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  const p = await b.newPage(); p.on('pageerror', e => console.log('page error:', e.message));
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8093/'); await p.waitForFunction(() => typeof explorer == 'function', null, {timeout: 30000});
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';window.elevate=async()=>true;return 1})()");
  const open = async () => { await q("wins.slice().forEach(w=>w.close());explorer();1"); await wait(1500);
    await q("(()=>{const W=wins.find(w=>w.app=='explorer');const e=[...W.body.querySelectorAll('span')].find(x=>/^PHOTOS \\(/.test(x.textContent));e.click();return 1})()"); await wait(1500) };
  const pick = async (name) => { await q(`(()=>{const W=wins.find(w=>w.app=='explorer');const r=W.body.querySelector('tr[data-k="R:${name}"]');r.click();W.body.querySelector('.rim').click();return 1})()`) };
  const done = () => p.waitForFunction(() => { const W = wins.find(w => w.app == 'explorer'); return W && !W.body.querySelector('.rcx') && /Copied|Left out|Stopped|does not fit|at most/.test(W.body.querySelector('.stat').textContent) }, null, {timeout: 60000}).catch(() => 0);
  const st = () => q("wins.find(w=>w.app=='explorer').body.querySelector('.stat').textContent");
  const dl = () => q("JSON.stringify(node(homePath('DOWNLOADS'))||node(['C:'])||{})").then(JSON.parse);
  await open();
  ok('the drive opens in Files with its folders', await q("!!wins.find(w=>w.app=='explorer').body.querySelector('tr[data-k=\"R:Trip\"]')"));
  // 1. a whole folder
  await pick('Trip'); await done(); let D = await dl(), s = await st();
  const trip = D.TRIP || {};
  const n = (o) => Object.values(o).reduce((a, v) => a + (typeof v == 'object' ? n(v) : 1), 0);
  ok('a folder is copied with its subfolders', trip['DAY 1'] && trip['DAY 2'] && trip['DAY 2'].BEACH && trip['DAY 2'].BEACH.SUNSET, Object.keys(trip));
  ok('...all 40 notes, with their content', n(trip) == 40 && trip['DAY 1']['NOTE-4.TXT'] == 'note number 4\n' && trip['DAY 2'].BEACH.SUNSET['NOTE-3.TXT'] == 'note number 3\n', n(trip));
  ok('...the file over 256 MB and the unreadable one are left out, and said so', !trip['VIDEO.MP4'] && !trip['LOCKED.TXT'] && /Left out 2/.test(s) && /video\.mp4 \(over 256 MB\)/.test(s) && /locked\.txt \(could not be read\)/.test(s), s);
  ok('...the big file was not even read', M.reads == 41, M.reads);   // 40 notes + the unreadable one
  // 2. the same folder again: a name of its own
  await pick('Trip'); await done(); D = await dl();
  ok('copying it again makes "TRIP - COPY", leaving the first', D.TRIP && D['TRIP - COPY'] && n(D['TRIP - COPY']) == 40, Object.keys(D));
  // 3. Cancel halfway
  M.slow = 150; await pick('Trip'); await wait(1600);
  await q("(()=>{const b=wins.find(w=>w.app=='explorer').body.querySelector('.rcx');b&&b.click();return !!b})()"); await done(); M.slow = 0;
  D = await dl(); s = await st(); const part = D['TRIP - COPY 2'] || {};
  ok('Cancel stops the copy and keeps what was copied so far', n(part) > 0 && n(part) < 40 && /Stopped: \d+ of 41 files copied\. Left out 2/.test(s), [n(part), s]);
  // 4. too big for the storage: refused before anything is copied
  await q("navigator.storage.estimate=async()=>({usage:0,quota:5});1"); const before = M.reads;
  await pick('Small'); await done(); s = await st(); D = await dl();
  ok('a folder that does not fit is refused before copying', /does not fit/.test(s) && !D.SMALL && M.reads == before, s);
  await b.close(); srv.close();
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
