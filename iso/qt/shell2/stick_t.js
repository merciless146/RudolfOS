// USB sticks 1.2.5: Make a Rudolf OS USB stick, Back up to a USB stick, and Restore from a backup at the first start,
// in the desktop page with a stand-in rudolfd (drives and their files in memory), in Chromium. node stick_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path'), {execSync} = require('child_process');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.mjs': 'text/javascript', '.html': 'text/html'};
// the stand-in rudolfd: four drives, files kept in memory
const FILES = new Map(), M = {calls: [], mounted: new Set(), corrupt: false};
const DISKS = [
  {name: 'sda', bytes: 16e9, removable: true, model: 'Rudolf stick', partitions: [{name: 'sda1', bytes: 6e8, fs: 'iso9660', label: 'RUDOLF_OS', mount: ''}, {name: 'sda3', bytes: 15e9, fs: 'ext4', label: 'RUDOLFDATA', mount: '/persist'}]},
  {name: 'sdb', bytes: 32e9, removable: true, model: 'SanDisk Ultra', partitions: [{name: 'sdb1', bytes: 32e9, fs: 'vfat', label: 'BACKUPS', mount: ''}]},
  {name: 'sdc', bytes: 512e9, removable: false, model: 'Samsung SSD', partitions: [{name: 'sdc1', bytes: 512e9, fs: 'ntfs', label: 'Windows', mount: ''}]},
  {name: 'sdd', bytes: 1e9, removable: true, model: 'Tiny stick', partitions: []}];
const api = (a, q, body) => {
  M.calls.push(a + (q ? '?' + q : '')); const p = new URLSearchParams(q || '');
  if (a == 'info') return {ok: true, mediaDev: '/dev/sda1', live: false, updateInstalled: true, media: 'usb', boot: 'uefi'};
  if (a == 'disks') return {ok: true, disks: DISKS.map(d => ({...d, partitions: d.partitions.map(x => ({...x, mount: M.mounted.has(x.name) ? '/media/' + x.name : x.mount}))}))};
  if (a == 'install/images') return {ok: true, images: [{path: '/home/rudolf/Downloads/rudolf-os-1.2.6.iso', name: 'rudolf-os-1.2.6.iso', bytes: 6e8}]};
  if (a == 'mkstick') { M.mk = Object.fromEntries(p); return {ok: true, message: 'making the stick'} }
  if (a == 'job') return {ok: true, running: false, status: 0, name: 'mkstick', log: 'Step 1/5: copying\nStep 2/5: checking the copy ...\nThe copy matches\nDone: the Rudolf OS USB stick is ready on /dev/sdb. You can remove it now.\n'};
  if (a == 'mount') { M.mounted.add(p.get('dev')); return {ok: true, message: 'mounted'} }
  if (a == 'umount') { M.mounted.delete(p.get('dev')); return {ok: true, message: 'safe to remove'} }
  if (a == 'fs/mkdir') return {ok: true};
  if (a == 'fs/write') { FILES.set(p.get('path'), body); return {ok: true, message: 'saved'} }
  if (a == 'fs/list') { const d = p.get('path') + '/'; const items = [...FILES.keys()].filter(k => k.startsWith(d) && !k.slice(d.length).includes('/')).map(k => ({name: k.slice(d.length), dir: false, size: FILES.get(k).length, mtime: 1}));
    return items.length ? {ok: true, items} : {__status: 404, ok: false, error: 'no such folder'} }
  return {ok: true};
};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0); const ch = [];
  q.on('data', c => ch.push(c)); q.on('end', () => { const body = Buffer.concat(ch);
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"usb",live:0,recovery:0};</script>')) }
    if (u == '/api/fs/read') { const f = FILES.get(new URLSearchParams(qs).get('path')); if (!f) { s.writeHead(404); return s.end() }
      let b = Buffer.from(f); if (M.corrupt && /\.zip$/.test(new URLSearchParams(qs).get('path'))) { b = Buffer.from(b); b[b.length >> 1] ^= 0xff }
      s.writeHead(200, {'content-type': 'application/octet-stream'}); return s.end(b) }
    if (u.startsWith('/api/')) { const r = api(u.slice(5), qs, body); s.writeHead(r.__status || 200, {'content-type': 'application/json'}); return s.end(JSON.stringify(r)) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(8094, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  let p = await b.newPage(); p.on('pageerror', e => /toUpperCase/.test(e.message) || console.log('page error:', e.message));
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8094/'); await p.waitForFunction(() => typeof stkSection == 'function', null, {timeout: 30000});
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';window.elevate=async()=>true;localStorage.setItem('bd95marker','from-the-backup');return 1})()");
  ok('drive picking', await q("JSON.stringify([stkDisk('/dev/sda1'),stkDisk('/dev/nvme0n1p1'),stkDisk('/dev/mmcblk0p1'),stkDisk('/dev/sr0')])") == '["sda","nvme0n1","mmcblk0","sr0"]');
  await q("settings('stor')"); await p.waitForFunction(() => !!document.querySelector('.stk30 .stkl'), null, {timeout: 15000}).catch(() => 0);
  const rows = await q("[...document.querySelectorAll('.stk30 .stkl .acr')].map(r=>r.firstChild.textContent)");
  ok('Make a stick lists only USB drives of 2 GB or more, not the running one', JSON.stringify(rows) == JSON.stringify(['🔌 sdb — SanDisk Ultra — 32.0 GB']), rows);
  ok('...and offers this Rudolf OS (with its updates) or a downloaded image', await q("[...document.querySelectorAll('.stk30 .stks option')].map(o=>o.textContent).join('|')") == `This Rudolf OS ${'1.2.5'} (with its updates)|rudolf-os-1.2.6.iso (Downloads)`, await q("[...document.querySelectorAll('.stk30 .stks option')].map(o=>o.textContent).join('|')"));
  await q("document.querySelector('.stk30 .stkf').checked=true;document.querySelector('.stk30 .stkl button').click()"); await wait(500);
  ok('the administrator must type the size', await q("!!document.querySelector('.natdlg .stki')&&document.querySelector('.natdlg .g').disabled"));
  await q("(()=>{const i=document.querySelector('.natdlg .stki');i.value='16.0 GB';i.dispatchEvent(new Event('input'));return 1})()");
  ok('...a wrong size keeps the button off', await q("document.querySelector('.natdlg .g').disabled"));
  await q("(()=>{const i=document.querySelector('.natdlg .stki');i.value='32.0 gb';i.dispatchEvent(new Event('input'));document.querySelector('.natdlg .g').click();return 1})()"); await wait(2500);
  ok('...then the stick is made: the drive, this Rudolf OS, with the files', M.mk && M.mk.disk == 'sdb' && M.mk.confirm == 'sdb' && M.mk.image == '' && M.mk.files == '1', M.mk);
  ok('...and the log and the result are shown', await q("/stick is ready/.test(document.querySelector('.stk30 .stkj').textContent)&&/— ready$/.test(document.querySelector('.stk30 .stkl .acr').firstChild.textContent)"));
  // back up to the stick
  const bk = await q("[...document.querySelectorAll('.stk30 .stkb .acr')].map(r=>r.firstChild.textContent)");
  ok('Back up lists the drives to back up to (not Rudolf OS\'s own, not internal disks)', JSON.stringify(bk) == JSON.stringify(['🔌 BACKUPS — SanDisk Ultra — 32.0 GB (vfat)']), bk);
  M.calls = []; await q("document.querySelector('.stk30 .stkb button').click()");
  await p.waitForFunction(() => /Backed up to|could not/.test(document.querySelector('.stk30 .stkbm').textContent), null, {timeout: 20000}).catch(() => 0);
  const msg = await q("document.querySelector('.stk30 .stkbm').textContent"); const names = [...FILES.keys()];
  ok('the backup is written to RUDOLF-BACKUP with its checksum, and read back', /Backed up to BACKUPS: RUDOLF-BACKUP\/rudolf-backup-\d{8}-\d{4}\.zip/.test(msg) && names.length == 2 && names.every(n => n.startsWith('/media/sdb1/RUDOLF-BACKUP/')), [msg, names]);
  const zip = names.find(n => n.endsWith('.zip')), sum = String(FILES.get(zip + '.sha256'));
  ok('...the checksum is the backup\'s SHA-256', sum.split(/\s+/)[0] == require('crypto').createHash('sha256').update(FILES.get(zip)).digest('hex') && sum.includes(path.basename(zip)), sum);
  ok('...and the drive was mounted for it and unmounted after', M.calls.includes('mount?dev=sdb1') && M.calls.includes('umount?dev=sdb1') && !M.mounted.size, M.calls.filter(c => /mount/.test(c)));
  // the first start of a new Rudolf OS: Restore from a backup
  await p.close(); const ctx = await b.newContext(); p = await ctx.newPage(); p.on('pageerror', e => /toUpperCase/.test(e.message) || console.log('page error:', e.message));
  await p.goto('http://127.0.0.1:8094/'); await p.waitForFunction(() => !!document.getElementById('oobe'), null, {timeout: 30000}).catch(() => 0);
  ok('the first-start screen offers Restore from a backup', await q("!!document.querySelector('#oobe .oorest')"));
  M.corrupt = true; await q("document.querySelector('#oobe .oorest').click()");
  await p.waitForFunction(() => document.querySelectorAll('.natdlg .strl .acr').length > 0, null, {timeout: 15000}).catch(() => 0);
  ok('...it finds the backup on the drive', /rudolf-backup-\d{8}-\d{4}\.zip — .* — on BACKUPS/.test(await q("(document.querySelector('.natdlg .strl .acr')||{}).textContent")), await q("document.querySelector('.natdlg').innerText"));
  await q("document.querySelector('.natdlg .strl button').click()"); await p.waitForFunction(() => /cannot be restored/.test(document.querySelector('.natdlg .strm').textContent), null, {timeout: 15000}).catch(() => 0);
  ok('...a damaged backup is refused', /damaged/.test(await q("document.querySelector('.natdlg .strm').textContent")), await q("document.querySelector('.natdlg .strm').textContent"));
  M.corrupt = false; await q("document.querySelector('.natdlg .strl button').click()");
  await p.waitForNavigation({timeout: 20000}).catch(() => 0); await wait(3000);
  ok('...a good one is restored (its settings are back, no first-start screen)', await q("localStorage.getItem('bd95marker')") == 'from-the-backup' && await q("!document.getElementById('oobe')||!!localStorage.getItem('bd95setup')"), await q("localStorage.getItem('bd95marker')"));
  await b.close(); srv.close();
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
