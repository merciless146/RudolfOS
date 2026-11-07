// 1.5.3 (wifi26.js): the Wi-Fi adapter choice in Settings › Network. With two adapters (the built-in one and a USB
// dongle) it shows both and passes the choice to rudolfd (wifi/adapter); with one it does not show; when the chosen
// USB adapter is unplugged it says so. The desktop page with a stand-in rudolfd, in Chromium.
//   node wifiad_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
const W0 = 'wlan=wlan0\tf8:54:f6:2f:6a:1f\tpci\trtw88_8821ce\trtw88_8821ce', W1 = 'wlan=wlan1\t00:0f:13:52:70:01\tusb\trt2800usb\tRalink 802.11 n WLAN';
let mode = 'two', cur = 'wlan0'; const posts = [];
const status = () => { const l = ['adapter=' + (mode == 'one' || mode == 'missing' ? 'wlan0' : cur), 'wpa_state=COMPLETED', 'ssid=Home', 'ip_address=192.168.1.5', W0];
  if (mode == 'two') l.push(W1); if (mode == 'missing') l.push('chosen=00:0f:13:52:70:01', 'chosen_missing=1'); if (mode == 'two' && cur == 'wlan1') l.push('chosen=00:0f:13:52:70:01'); return l.join('\n') + '\n' };
const api = (a, q, m) => { const sp = new URLSearchParams(q || '');
  if (a == 'wifi/status') return {ok: true, raw: status()};
  if (a == 'wifi/scan') return {ok: true, raw: 'bssid / frequency / signal level / flags / ssid\naa:bb:cc:dd:ee:ff\t5180\t-50\t[WPA2-PSK-CCMP]\tHome\n'};
  if (a == 'wifi/adapter' && m == 'POST') { posts.push(sp.get('if')); cur = sp.get('if'); return {ok: true, message: 'Using Ralink 802.11 n WLAN for Wi-Fi'} }
  if (a == 'events/wait') return {ok: true, events: []};
  return {ok: true} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs, q.method))) }
    s.writeHead(404); s.end() }) }).listen(8099, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8099/'); await p.waitForFunction(() => typeof wifiPanel == 'function', null, {timeout: 30000});
  const panel = () => p.evaluate(() => { let d = document.getElementById('wft'); if (d) d.remove(); d = document.createElement('div'); d.id = 'wft'; document.body.appendChild(d); wifiPanel(d, '') });
  const ad = () => p.evaluate(() => { const a = document.querySelector('#wft .wfad'), s = a && a.querySelector('select');
    return {text: a ? a.innerText : '', opts: s ? [...s.options].map(o => [o.value, o.textContent, o.selected]) : null, disabled: s ? s.disabled : null} });
  await panel(); await wait(1200); let a = await ad();
  ok('1. two adapters: the choice shows both, in words', a.opts && a.opts.length == 2 && a.opts[0][1] == 'Built-in (rtw88_8821ce)' && a.opts[1][1] == 'USB: Ralink 802.11 n WLAN', a);
  ok('2. ...with the one in use selected (the built-in one)', a.opts && a.opts[0][2] && !a.opts[1][2], a);
  await p.evaluate(() => { const s = document.querySelector('#wft .wfad select'); s.value = 'wlan1'; s.dispatchEvent(new Event('change')) }); await wait(1500);
  ok('3. choosing the dongle asks rudolfd for it (wifi/adapter?if=wlan1)', posts.length == 1 && posts[0] == 'wlan1', posts);
  a = await ad(); ok('4. ...and then shows it as the one in use', a.opts && a.opts[1][2], a);
  mode = 'one'; await panel(); await wait(1200); a = await ad();
  ok('5. one adapter: no choice', !a.opts && !a.text.trim(), a);
  mode = 'missing'; await panel(); await wait(1200); a = await ad();
  ok('6. the chosen dongle unplugged: it says the built-in Wi-Fi is used', /not plugged in, so the built-in Wi-Fi is used/.test(a.text) && a.disabled === true, a);
  ok('7. no page errors', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1)
})().catch(e => { console.log('FAIL ' + e.stack); process.exit(1) });
