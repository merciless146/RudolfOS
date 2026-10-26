// New boot files 1.2.8 (item 17), in the desktop page with a stand-in rudolfd, in Chromium: Settings › Security ›
// Secure Boot › Get new boot files (the drive Rudolf OS runs from) and Install here › Replace the boot files only
// (another drive), and a boot image kept out of Make a stick. node bootf_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.mjs': 'text/javascript', '.html': 'text/html'};
const H = 'ab'.repeat(32), BOOT = '/home/rudolf/Downloads/rudolf-os-1.2.8-boot.iso', FULL = '/home/rudolf/Downloads/rudolf-os-1.2.8.iso';
const M = {calls: [], pending: false, result: ''};
// the running drive (an NVMe drive installed from an old disc) and a second installed drive
const DISKS = [
  {name: 'nvme0n1', bytes: 128e9, removable: false, model: 'Laptop SSD', partitions: [{name: 'nvme0n1p1', bytes: 4.7e8, fs: 'iso9660', label: 'RUDOLF_OS', mount: '/live/media'}, {name: 'nvme0n1p3', bytes: 1.2e11, fs: 'ext4', label: 'RUDOLFDATA', mount: '/persist'}]},
  {name: 'sdb', bytes: 64e9, removable: false, model: 'Old laptop disk', partitions: [{name: 'sdb1', bytes: 4.7e8, fs: 'iso9660', label: 'RUDOLF_OS', mount: ''}, {name: 'sdb3', bytes: 6e10, fs: 'ext4', label: 'RUDOLFDATA', mount: ''}]},
  {name: 'sdc', bytes: 512e9, removable: false, model: 'Samsung SSD', partitions: [{name: 'sdc1', bytes: 512e9, fs: 'ntfs', label: 'Windows', mount: ''}]}];
const api = (a, q) => {
  M.calls.push(a + (q ? '?' + q : '')); const p = new URLSearchParams(q || '');
  if (a == 'info') return {ok: true, live: false, updateInstalled: true, media: 'disk', boot: 'uefi', bootSecure: 0, bootFiles: {pending: M.pending, result: M.result}};
  if (a == 'secureboot/status') return {ok: true, efi: true, on: false, firmware: true, enrolled: false, pending: false, certificate: true, lockdown: '', unverified: ''};
  if (a == 'disks') return {ok: true, disks: DISKS};
  if (a == 'check') return {ok: true};
  if (a == 'install/images') return {ok: true, images: [{path: FULL, name: 'rudolf-os-1.2.8.iso', bytes: 547356672, parts: 18, boot: false}, {path: BOOT, name: 'rudolf-os-1.2.8-boot.iso', bytes: 28e6, boot: true}]};
  if (a == 'bootfiles') { M.bf = Object.fromEntries(p); M.pending = true; return {ok: true, message: 'checking the image'} }
  if (a == 'install') { M.inst = Object.fromEntries(p); return {ok: true, message: 'writing the boot files'} }
  if (a == 'job') return {ok: true, running: false, status: 0, name: 'bootfiles', log: 'Image: Rudolf OS 1.2.8, boot files only (27 MiB).\nReady: the new boot files (Rudolf OS 1.2.8) are written the next time the computer starts\n'};
  return {ok: true};
};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/api/')) { const r = api(u.slice(5), qs); s.writeHead(r.__status || 200, {'content-type': 'application/json'}); return s.end(JSON.stringify(r)) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(8095, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  const p = await b.newPage(); p.on('pageerror', e => /toUpperCase/.test(e.message) || console.log('page error:', e.message));
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8095/'); await p.waitForFunction(() => typeof bootFilesDlg == 'function' && typeof sbSection == 'function', null, {timeout: 30000});
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';window.elevate=async()=>true;window.__restart=0;window.sbRestart=()=>__restart++;
    const D=node(downloadsPath());D['RUDOLF-OS-1.2.8-BOOT.ISO.SHA256']='data:application/octet-stream;base64,'+btoa('${H}  rudolf-os-1.2.8-boot.iso\\n');return 1})()`);
  const sbText = async () => { await q("(()=>{wins.slice().forEach(w=>w.close());settings('sec');return 1})()"); await p.waitForFunction(() => /Off\./.test(document.querySelector('.sb28')?.innerText || ''), null, {timeout: 10000}).catch(() => 0); await wait(300); return q("document.querySelector('.sb28').innerText") };
  let t = await sbText();
  ok('Secure Boot on an old drive offers Get new boot files (keeping files) before a stick', /Get new boot files replaces them and keeps your files/.test(t) && /Get new boot files…\s*Make a USB stick…/.test(t), t);
  await q("[...document.querySelectorAll('.sb28 button')].find(b=>b.textContent=='Get new boot files…').click()"); await p.waitForSelector('.bfdlg .bfs', {timeout: 5000}).catch(() => 0); await wait(300);
  let d = await q("[document.querySelector('.bfdlg').innerText,[...document.querySelectorAll('.bfdlg .bfs option')].map(o=>o.value)]");
  ok('the dialog lists the boot image first, says what it is, and finds its .sha256', d[1][0] == BOOT && d[1][1] == FULL && /rudolf-os-1\.2\.8-boot\.iso/.test(d[0]) && /A boot image: the system keeps coming from the installed update/.test(d[0]) && /Its \.sha256 is in Downloads/.test(d[0]), d);
  await q(`(()=>{const s=document.querySelector('.bfdlg .bfs');s.value=${JSON.stringify(FULL)};s.onchange();return 1})()`);
  ok('...a whole disc image is used only if it fits', /used only if it fits in front of your files/.test(await q("document.querySelector('.bfdlg .bfm').textContent")));
  await q(`(()=>{const s=document.querySelector('.bfdlg .bfs');s.value=${JSON.stringify(BOOT)};s.onchange();[...document.querySelectorAll('.bfdlg button')].find(b=>b.textContent=='Prepare the new boot files').click();return 1})()`);
  await p.waitForFunction(() => /written the next time the computer starts/.test(document.querySelector('.bfdlg')?.innerText || ''), null, {timeout: 10000}).catch(() => 0);
  d = await q("document.querySelector('.bfdlg').innerText");
  ok('Prepare asks rudolfd to stage the image with its SHA-256, shows the log, then what happens at the next start', M.bf && M.bf.image == BOOT && M.bf.sha == H && /Ready: the new boot files/.test(d) && /Do not switch it off|do not switch it off/.test(d) && /Restart now/.test(d), [M.bf, d.slice(0, 300)]);
  await q("[...document.querySelectorAll('.bfdlg button')].find(b=>b.textContent=='Restart now').click()"); await wait(300);
  ok('...Restart now restarts', await q('__restart') == 1 && !(await q("!!document.querySelector('.bfdlg')")));
  t = await sbText();
  ok('while they wait, Secure Boot says they are written at the next start', /New boot files are ready: they are written the next time the computer starts/.test(t) && /Restart now/.test(t), t);
  // Install here, from a stick, for another installed drive
  await q("(()=>{wins.slice().forEach(w=>w.close());settings('upd');return 1})()"); await p.waitForFunction(() => document.querySelectorAll('.dsk .acr').length >= 3, null, {timeout: 10000}).catch(() => 0);
  const rows = await q("[...document.querySelectorAll('.dsk .acr')].map(r=>[r.firstChild.textContent,[...r.querySelectorAll('button')].map(b=>b.textContent).join('|')])");
  ok('Install here offers Replace the boot files only for an installed drive, not for the running one or another disk', rows.some(r => /sdb/.test(r[0]) && r[1] == 'Replace the boot files only…|Install here…') && rows.some(r => /nvme0n1/.test(r[0]) && r[1] == 'Install here…') && rows.some(r => /sdc/.test(r[0]) && r[1] == 'Install here…'), rows);
  const opts = await q("[...document.querySelectorAll('.isrc option')].map(o=>o.textContent)");
  ok('...and labels the boot image in its image list', opts.some(o => /rudolf-os-1\.2\.8-boot\.iso — boot files only/.test(o)), opts);
  await q(`(()=>{const s=document.querySelector('.isrc');s.value=${JSON.stringify(BOOT)};[...document.querySelectorAll('.dsk .acr')].find(r=>/sdb/.test(r.firstChild.textContent)).querySelector('.bfo').click();return 1})()`); await wait(500);
  const dl = await q("[...document.querySelectorAll('.rmbox,.sdlg,[role=alertdialog]')].map(x=>x.innerText).join(' / ')");
  await q("[...document.querySelectorAll('button')].find(b=>b.textContent=='Replace the boot files').click()"); await wait(1500);
  ok('...which asks first (files kept), then asks rudolfd for bootonly=1 with the image and its SHA-256', /Its files, settings and installed system are kept/.test(dl) && M.inst && M.inst.disk == 'sdb' && M.inst.confirm == 'sdb' && M.inst.bootonly == '1' && M.inst.image == BOOT && M.inst.sha == H, [dl.slice(0, 200), M.inst]);
  // Make a stick never offers a boot image
  ok('Make a USB stick leaves boot images out (they have no system)', await q("(async()=>{const im=(await rd('install/images')).images.filter(x=>!x.boot);return im.length==1&&im[0].name=='rudolf-os-1.2.8.iso'})()") && /filter\(x=>!x\.boot\)/.test(await q("stkSection.toString()")));
  await b.close(); srv.close();
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
