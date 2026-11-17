// 1.6.0 (the owner's Vivobook, 2026-11-17): File Explorer's Drives list. Only the disk Rudolf OS runs from is left out (a
// partition of it in use outside /media), as devplug does; another Rudolf OS stick shows its RUDOLFDATA, read-only and
// named so, never its start-up parts (ISO 9660, RUDOLF EFI); the system's own device-mapper devices (dm-N: its checked
// layers, the encrypted folder) are never drives. Stand-in rudolfd; node drives60_t.js [CHROME]  (build the page first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.mjs': 'text/javascript', '.html': 'text/html'};
// the Vivobook's disks as rudolfd listed them on 2026-11-17 (the dm-N ones as an older rudolfd still lists them)
const DISKS = [
  {name: 'nvme0n1', bytes: 128e9, removable: false, model: 'KINGSTON', partitions: [{name: 'nvme0n1p2', bytes: 28e6, fs: 'vfat', label: 'RUDOLF EFI', mount: ''}, {name: 'nvme0n1p3', bytes: 125e9, fs: 'ext4', label: 'RUDOLFDATA', mount: '/persist'}]},
  {name: 'sda', bytes: 64e9, removable: true, model: 'USB DISK 3.0', partitions: [{name: 'sda1', bytes: 40e6, fs: 'iso9660', label: 'RUDOLF_BOOT_1_6_0', mount: ''}, {name: 'sda2', bytes: 28e6, fs: 'vfat', label: 'RUDOLF EFI', mount: ''}, {name: 'sda3', bytes: 61e9, fs: 'ext4', label: 'RUDOLFDATA', mount: '/media/sda3'}]},
  {name: 'sdb', bytes: 32e9, removable: true, model: 'SanDisk Ultra', partitions: [{name: 'sdb1', bytes: 32e9, fs: 'vfat', label: 'PHOTOS', mount: ''}]},
  {name: 'dm-0', bytes: 6e6, removable: false, model: '', partitions: [{name: 'dm-0', bytes: 6e6, fs: 'squashfs', label: '', mount: ''}]},
  {name: 'dm-1', bytes: 141e6, removable: false, model: '', partitions: [{name: 'dm-1', bytes: 141e6, fs: 'squashfs', label: '', mount: ''}]}];
const api = a => a == 'info' ? {ok: true, live: false, media: 'disk', boot: 'uefi'} : a == 'disks' ? {ok: true, disks: DISKS} : {ok: true};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => 0); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs))) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(8701, '127.0.0.1');
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  const p = await b.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message));
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8701/'); await p.waitForFunction(() => typeof explorer == 'function' && FSLOADED && fs['C:'] && fs['C:'].USERS, null, {timeout: 30000});
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';return 1})()");
  await q("wins.slice().forEach(w=>w.close());explorer();1"); await p.waitForTimeout(1500);
  const side = await q("(()=>{const W=wins.find(w=>w.app=='explorer');return [...W.body.querySelectorAll('span')].map(x=>x.textContent).filter(t=>/\\((\\d|[\\d.]+ [KMGT]?B)/.test(t)||/^dm-|^sd|^nvme|RUDOLF|PHOTOS|Rudolf OS data/.test(t))})()");
  ok('1. another Rudolf OS stick shows: its RUDOLFDATA, named "Rudolf OS data (another Rudolf OS, read-only)"', side.some(t => /^Rudolf OS data \(another Rudolf OS, read-only\) \(/.test(t)), side);
  ok('2. ...never its start-up parts (ISO 9660, RUDOLF EFI)', !side.some(t => /RUDOLF_BOOT|RUDOLF EFI|^sda1|^sda2/.test(t)), side);
  ok('3. an ordinary USB stick shows', side.some(t => /^PHOTOS \(/.test(t)), side);
  ok('4. the disk Rudolf OS runs from does not (its RUDOLFDATA in use at /persist)', !side.some(t => /nvme|^RUDOLFDATA/.test(t)), side);
  ok('5. the system\'s device-mapper devices (dm-N) are never drives', !side.some(t => /^dm-/.test(t)), side);
  ok('6. rudolfd leaves dm-N out of its list too', /case \$n in loop\*\|ram\*\|zram\*\|dm-\*\) continue;;/.test(fs.readFileSync(path.join(ROOT, 'iso/sys/rudolfd.c'), 'utf8')));
  const dp = fs.readFileSync(path.join(ROOT, 'iso/sys/root/usr/lib/rudolf/devplug'), 'utf8'), m = dp.slice(dp.indexOf('\nmount)'), dp.indexOf('\neject)'));
  ok('7. devplug mount (a click there): never the system disk, never start-up parts, RUDOLFDATA read-only and named so', /system_disk \$\(disk_of \$p\) && \{/.test(m) && /\[ "\$k" = boot \] && \{/.test(m) && /mount_part \$p ro/.test(m) && /another Rudolf OS, read-only/.test(m), m);
  ok('no page errors', !errs.length, errs);
  await b.close(); srv.close(); const n = R.filter(Boolean).length; console.log(n + '/' + R.length + ' passed'); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
