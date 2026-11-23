// 1.6.2 (docs/plan-1.6.2.md Part 31, item 4): Settings › Update before the first update after the system's images are
// packed another way (xz to zstd): the button says the size that is downloaded ("Download (about 530 MB)") on every
// connection, and the card says why, once; an ordinary update still says "Download (only the changes)". The built page
// (iso/www/index.html) with a stand-in rudolfd on 127.0.0.1:8622.   node updsize162_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www'); const PORT = 8622;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
let AV = {serial: 2, version: '1.6.2', notes: '', size: 545000000, urgent: false, whole: 530000000, wholeLayers: 'base,chromium,desktop,modules', packed: 'zstd, 256 KB blocks'};
const api = a => {
  if (a == 'sys/status') return {ok: true, persistent: true, auto: true, url: 'https://x/stable', current: {version: '1.6.2b'}, state: {good: 'a'}, slots: {}, booted: 'a', checked: Date.now() / 1000, available: AV};
  return {ok: true} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/a/')) { const f = path.join(WWW, u); if (fs.existsSync(f)) { s.writeHead(200); return s.end(fs.readFileSync(f)) } }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs || '', q.method))) }
    s.writeHead(404); s.end() }) }).listen(PORT, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 1000}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto(`http://127.0.0.1:${PORT}/`); await p.waitForFunction(() => typeof updPanel == 'function' && typeof sysuDlLabel == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x'}];USER=ACCTS[0];MTR={connected:true,metered:false};return 1})()`);
  const open = async () => { await q("(()=>{let d=document.getElementById('ut');if(d)d.remove();d=document.createElement('div');d.id='ut';document.body.appendChild(d);updPanel(d);return 1})()"); await wait(1500) };
  await open();
  const btn = await q("document.querySelector('#ut .udl')?.textContent||''"), txt = await q("document.getElementById('ut').textContent");
  ok('1. the first zstd update: the button says the size downloaded whole (about 530 MB)', btn == 'Download (about 530 MB)', btn);
  ok("2. ...and the card why, once: the images packed in a new way (zstd, 256 KB blocks), downloaded whole this once, later only the changes",
    /This update packs the system's images in a new way \(zstd, 256 KB blocks\), so it downloads them whole this once: about 530 MB\. Later updates download only the changes again\./.test(txt) && txt.split('packs the system').length == 2, txt);
  ok('3. on mobile data and on a metered Wi-Fi: the same size ("about", not "up to" the whole release)',
    await q("MTR={connected:true,metered:true,auto:'modem'};sysuDlLabel(SYSU.available)") == 'Download over mobile data: about 530 MB' &&
    await q("MTR={connected:true,metered:true,auto:'dhcp'};sysuDlLabel(SYSU.available)") == 'Download on this metered connection: about 530 MB');
  AV = {serial: 3, version: '1.6.2.1', notes: '', size: 545000000, urgent: false, whole: 0, wholeLayers: '', packed: ''};
  await q("MTR={connected:true,metered:false}"); await open();
  const btn2 = await q("document.querySelector('#ut .udl')?.textContent||''"), txt2 = await q("document.getElementById('ut').textContent");
  ok('4. the update after it: "Download (only the changes)", no note', btn2 == 'Download (only the changes)' && !/packs the system/.test(txt2), btn2);
  ok('5. ...on mobile data: "up to" the layers\' size, as before', await q("MTR={connected:true,metered:true,auto:'modem'};sysuDlLabel(SYSU.available)") == 'Download over mobile data: up to 545 MB');
  AV = {serial: 2, version: '1.6.2', notes: '', size: 545000000, urgent: false};   // (an older rudolf-update: no "whole")
  await q("MTR={connected:true,metered:false}"); await open();
  ok('6. a status without "whole" (rudolf-update before this): as before', await q("document.querySelector('#ut .udl')?.textContent||''") == 'Download (only the changes)');
  ok('7. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
