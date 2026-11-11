// 1.5.8 (plan Part 6): mobile data on the desktop. A phone's USB tethering or a modem plugged in is announced; on a
// metered connection the daily update check finds an update but does not download it (and says so once), Linux
// apps and Debian are not updated in the background; Settings › Network › Metered connection shows the connection,
// why it is metered and this month's data, and sets it per network. rudolfd stood in.
// node mobile58_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const M = {met: {ok: true, connected: true, if: 'wwan0', kind: 'usbmodem', key: 'if:02:00:00:00:00:02', name: 'MiFi 7730L', metered: true, manual: false, auto: 'modem', used: 2500000}, posts: [], events: ''};
const api = (a, q, method) => {
  if (method == 'POST') M.posts.push(a + (q ? '?' + q : ''));
  if (a == 'net/metered') {
    if (method == 'POST') { const p = new URLSearchParams(q); const v = p.get('v'); M.met.manual = v != 'auto'; M.met.metered = v == 'on' || (v == 'auto'); return {ok: true} }
    return M.met;
  }
  if (a == 'sys/status') return {ok: true, persistent: true, auto: true, url: 'https://x', checked: 0, canUpdate: true, available: {version: '1.5.9'}, slots: {}, state: {}};
  if (a == 'job') return {ok: true, running: false, log: 'done\n'};
  if (a == 'net') return {ok: true, interfaces: [], gateway: '', dns: '', wifiTools: true};
  if (a == 'events') { const e = M.events; M.events = ''; return {ok: true, events: e} }
  if (a == 'info') return {ok: true, backlight: false, live: 0, boot: 'uefi', kernel: 'test'};
  return {ok: true};
};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', async () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/events/wait') await new Promise(r => setTimeout(r, 800));
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5) == 'events/wait' ? 'events' : u.slice(5), qs, q.method))) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8181, '127.0.0.1');
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const p = await (await b.newContext({viewport: {width: 1280, height: 900}})).newPage(); const errs = []; p.on('pageerror', e => errs.push(e.stack.slice(0, 300)));
  await p.goto('http://127.0.0.1:8181/'); await p.waitForFunction(() => typeof netMetered == 'function' && typeof sysuTick == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';window.elevate=async()=>true;window.__n=[];const o=notify;notify=function(t,m,op){__n.push([t,m]);return o.apply(this,arguments)};return 1})()");
  await q("mtrPoll()");
  ok('1. the modem in use is metered', await q("netMetered()"));
  // the update check on a metered connection
  M.posts = []; await q("window.netLive=()=>true;sysuTick.metered=0;sysuJob=async w=>{await rdPost('sys/'+w)};SYSU={available:{version:'1.5.9'}};sysuTick()"); await p.waitForTimeout(800);
  ok('2. metered: the update is checked for, not downloaded', M.posts.includes('sys/check') && !M.posts.includes('sys/download'), M.posts);
  ok('...and a notification says why, once', (await q("__n.filter(n=>/metered/.test(n[1])).length")) == 1, await q('__n'));
  M.met.metered = false; M.met.auto = ''; await q("mtrPoll()"); M.posts = [];
  await q("sysuTick()"); await p.waitForTimeout(800);
  ok('3. not metered: it is downloaded', M.posts.includes('sys/download'), M.posts);
  // background updates of Linux apps and Debian wait
  M.met.metered = true; await q("mtrPoll()");
  const bg = await q("(async()=>{let called=0;const o=lxApi;lxApi=async(...a)=>{called++;return o(...a)};localStorage.removeItem('bd95lxupd');await lxAutoUpdate();lxApi=o;return called})()");
  ok('4. metered: Linux apps are not updated in the background', bg == 0, bg);
  // a phone's tethering plugged in
  await q("__n=[]"); M.events = 'dev {"a":"add","t":"net","if":"usb0","kind":"phone","name":"Pixel 7","metered":true}\n';
  await q("devEvents('dev {\"a\":\"add\",\"t\":\"net\",\"if\":\"usb0\",\"kind\":\"phone\",\"name\":\"Pixel 7\",\"metered\":true}')"); await p.waitForTimeout(300);
  const n = await q("__n.find(n=>/Phone \\(USB tethering\\) connected/.test(n[0]))");
  ok('5. a phone\'s tethering plugged in: "📱 Phone (USB tethering) connected", metered', n && /Pixel 7/.test(n[1]) && /metered/.test(n[1]), await q('__n'));
  // Settings › Network
  M.met = {ok: true, connected: true, if: 'wlan0', kind: 'wifi', key: 'wifi:AndroidAP', name: '', metered: true, manual: false, auto: 'dhcp', used: 123000000};
  await q("settings('net')"); await p.waitForSelector('.mtr58 .mtrnow', {timeout: 5000}); await p.waitForTimeout(800);
  const t = await q("document.querySelector('.mtr58 .mtrnow').textContent");
  ok('6. Settings › Network: the Wi-Fi network, metered because it says so, 123 MB this month', /AndroidAP/.test(t) && /the network says it is metered/.test(t) && /123\.0 MB used this month/.test(t), t);
  ok('...the choice shows Automatic', await q("document.querySelector('#mtrsel').value") == 'auto');
  M.posts = []; await p.selectOption('#mtrsel', 'off'); await p.waitForTimeout(800);
  ok('7. "Not metered" for this network: sent to rudolfd for wlan0', M.posts.includes('net/metered?if=wlan0&v=off'), M.posts);
  const t2 = await q("document.querySelector('.mtr58 .mtrnow').textContent");
  ok('...and shown (set by you)', /not metered \(set by you/.test(t2), t2);
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); const k = R.filter(Boolean).length; console.log(`${k}/${R.length} passed`); process.exit(k == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
