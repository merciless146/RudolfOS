// Boot menu 1.2.5: Settings › Update and recovery › Boot menu and Restart into Windows (Settings and the Power dialog),
// in the desktop page with a stand-in rudolfd, in Chromium. node boot_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path'), {execSync} = require('child_process');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.mjs': 'text/javascript', '.html': 'text/html'};
// what the stand-in rudolfd says; the tests change it
// what the stand-in rudolfd says; the tests change it
const M = {uefi: true, loader: true, def: '', to: '', windows: [{n: '0003', title: 'Windows Boot Manager'}], calls: []};
const api = (a, q) => {
  M.calls.push(a + (q ? '?' + q : ''));
  if (a == 'bootmenu') return {ok: true, uefi: M.uefi, loader: M.loader, entries: ['rudolf-start.conf', 'rudolf-recovery.conf', 'rudolf-check.conf', 'rudolf-ram.conf', 'rudolf-debug.conf', 'auto-reboot-to-firmware-setup'], selected: 'rudolf-start.conf', default: M.def, timeout: M.to, windows: M.windows};
  if (a == 'bootmenu/set') { const p = new URLSearchParams(q); M.def = p.get('default'); M.to = p.get('timeout'); return {ok: true, message: 'saved'} }
  if (a == 'bootmenu/windows') return {ok: true, message: 'Windows starts at the next restart'};
  return {ok: true};
};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs))) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(8095, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  p.on('pageerror', e => console.log('page error:', e.message, (e.stack || '').split('\n').slice(1, 3).join(' | ')));
  await p.goto('http://127.0.0.1:8095/'); await p.waitForFunction(() => typeof bootSection == 'function' && typeof settings == 'function', null, {timeout: 30000});
  await p.evaluate(() => { try { svj('bd95setup', 1) } catch (e) {} ['oobe', 'login'].forEach(i => { const o = document.getElementById(i); o && o.remove() }); wins.slice().forEach(w => w.close()); USER = USER || {n: 'Admin', r: 'admin'}; USER.r = 'admin'; window.elevate = async () => true; window.__reboots = 0 });
  const q = s => p.evaluate(s);
  await q("settings('upd')"); await p.waitForFunction(() => !!document.querySelector('.boot30 .bmd'), null, {timeout: 15000}).catch(() => 0);
  const opts = await q("[...document.querySelectorAll('.boot30 .bmd option')].map(o=>o.textContent)");
  ok('Settings › Update and recovery shows the Boot menu', opts.length > 0, opts);
  ok('...the default choices: as on the disc and the Rudolf OS entries (not Debug, not the firmware item)', JSON.stringify(opts) == JSON.stringify(['As on the disc (Start Rudolf OS)', 'Recovery mode', 'Check this disc or USB drive', 'Start from memory']), opts);
  ok('...and the wait, 2 to 10 seconds', await q("[...document.querySelectorAll('.boot30 .bmt option')].map(o=>o.value).join()") == ',2,3,4,5,6,8,10');
  await q("(()=>{const b=document.querySelector('.boot30');b.querySelector('.bmd').value='rudolf-ram.conf';b.querySelector('.bmt').value='8';b.querySelector('.bms').click();return 1})()"); await wait(800);
  ok('Save sends both', M.def == 'rudolf-ram.conf' && M.to == '8' && /Saved/.test(await q("document.querySelector('.boot30 .bmm').textContent")), [M.def, M.to]);
  await q("wins.slice().forEach(w=>w.close());settings('upd')"); await p.waitForFunction(() => !!document.querySelector('.boot30 .bmd'), null, {timeout: 15000}).catch(() => 0);
  ok('...and shows them when opened again', await q("document.querySelector('.boot30 .bmd').value+'|'+document.querySelector('.boot30 .bmt').value") == 'rudolf-ram.conf|8');
  // Restart into Windows from Settings: sets the next start, then restarts
  M.calls = [];
  await q("[...document.querySelectorAll('.boot30 button')].find(b=>b.textContent=='Restart into Windows').click()"); await wait(2500);
  ok('Settings: Restart into Windows sets the next start and restarts', M.calls.includes('bootmenu/windows?n=0003') && M.calls.includes('power?action=reboot') && M.calls.indexOf('bootmenu/windows?n=0003') < M.calls.indexOf('power?action=reboot'), M.calls.filter(c => /boot|power/.test(c)));
  // the Power dialog
  M.calls = []; await q("shutdownDlg();1");
  ok('the Power dialog has Restart into Windows', await p.waitForFunction(() => !!document.querySelector('.rmodal .bmwin'), null, {timeout: 8000}).then(() => true, () => false));
  await q("document.querySelector('.rmodal .bmwin').click()"); await wait(2500);
  ok('...which sets the next start and restarts the normal way', M.calls.includes('bootmenu/windows?n=0003') && M.calls.some(c => c == 'power?action=reboot'), M.calls);
  await q("document.querySelectorAll('.rmodal').forEach(o=>o.remove())");
  // no Windows: no button; BIOS: the hint
  M.windows = []; await q("shutdownDlg();1"); await wait(1500);
  ok('without Windows the Power dialog has no such button', !(await q("!!document.querySelector('.rmodal .bmwin')")));
  await q("document.querySelectorAll('.rmodal').forEach(o=>o.remove())");
  M.uefi = false; await q("wins.slice().forEach(w=>w.close());settings('upd')"); await wait(2500);
  ok('BIOS: says to use Boot from hard disk', /Boot from hard disk/.test(await q("(document.querySelector('.boot30')||{}).textContent")));
  await b.close(); srv.close();
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
