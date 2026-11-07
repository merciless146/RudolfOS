// 1.5.3 (devices26.js): Settings › Devices shows each USB device's speed and its port's ("USB 2.0, 480 Mbit/s, in a
// USB 3 port (10 Gbit/s)"), and a "Not supported yet" list of the devices with no driver, or that every device has
// one. The desktop page with a stand-in rudolfd, in Chromium.   node devunsup_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
let un = [{bus: 'usb', id: '148f:5370', kind: 'Wi-Fi adapter', name: 'Ralink 802.11 n WLAN'}, {bus: 'pci', id: '1022:15e2', kind: 'Sound processor', name: 'AMD Sound processor'}];
const DEV = {ok: true, usb: [{t: 'usb', kind: 'Wi-Fi adapter', name: 'Ralink 802.11 n WLAN', id: '148f:5370', driver: 0, speed: 480, port: 480},
  {t: 'usb', kind: 'USB drive', name: 'SanDisk Cruzer Blade', id: '0781:5567', driver: 1, speed: 480, port: 10000},
  {t: 'usb', kind: 'Mouse', name: 'Logitech Receiver', id: '046d:c52b', driver: 1, speed: 12, port: 0}], drives: [], inputs: []};
const api = a => { if (a == 'devices') return DEV; if (a == 'devices/unsupported') return un === null ? null : {ok: true, devices: un}; if (a == 'events/wait') return {ok: true, events: []}; return {ok: true} };
const srv = http.createServer((q, s) => { const [u0] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      const j = api(u.slice(5)); if (!j) { s.writeHead(500, {'content-type': 'application/json'}); return s.end('{"ok":false,"error":"could not list the devices"}') }
      s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(j)) }
    s.writeHead(404); s.end() }) }).listen(8097, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8097/'); await p.waitForFunction(() => typeof devPanel == 'function', null, {timeout: 30000});
  const show = () => p.evaluate(() => { let d = document.getElementById('dvt'); if (d) d.remove(); d = document.createElement('div'); d.id = 'dvt'; document.body.appendChild(d); devPanel(d) });
  const text = () => p.evaluate(() => document.getElementById('dvt').innerText);
  await show(); await wait(1200); let t = await text();
  ok('1. the dongle: USB 2.0 in a USB 2.0 port, no driver', /802\.11 n WLAN[\s\S]*USB 2\.0, 480 Mbit\/s, in a USB 2\.0 port[\s\S]*no driver in Rudolf OS/.test(t), t);
  ok('2. the drive: USB 2.0 in a USB 3 port (10 Gbit/s)', /Cruzer Blade[\s\S]*USB 2\.0, 480 Mbit\/s, in a USB 3 port \(10 Gbit\/s\)/.test(t), t);
  ok('3. the mouse: USB 1.1, port not known: no port text', /USB 1\.1, 12 Mbit\/s(?!, in)/.test(t) && !/12 Mbit\/s, in/.test(t), t);
  ok('4. "Not supported yet" lists the two devices with no driver', /Not supported yet[\s\S]*Ralink 802\.11 n WLAN[\s\S]*Wi-Fi adapter · USB 148f:5370 · no driver yet[\s\S]*AMD Sound processor[\s\S]*built in \(PCI\) 1022:15e2/.test(t), t);
  un = []; await show(); await wait(1200); t = await text();
  ok('5. nothing missing: it says every device has a driver', /Not supported yet[\s\S]*Every device in this computer has a driver\./.test(t), t);
  un = null; await show(); await wait(1200); t = await text();
  ok('6. an older rudolfd without the list: no section, the rest still shows', !/Not supported yet/.test(t) && /Cruzer Blade/.test(t), t);
  ok('7. no page errors', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1)
})().catch(e => { console.log('FAIL ' + e.stack); process.exit(1) });
