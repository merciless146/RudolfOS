// 1.5.9: Settings › Network › Wi-Fi › "Radios of this network" (each radio with its band, channel, signal and state, and
// a Use switch), and "⚡ Why does the Wi-Fi drop?": Blitzen gets the Wi-Fi's log, the kernel's Wi-Fi messages and the
// radios, with the Wi-Fi settings that exist. A stand-in rudolfd.   node wifiradios59_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const until = Math.floor(Date.now() / 1000) + 3600;
let daOff = false; const posts = [];
const radios = () => `bands=2.4 5 \nnet=0\nssid=SpectrumSetup-3E\nradio=bc:96:e5:88:1f:d9\t5220\t-53\tinuse\nradio=bc:96:e5:88:1f:da\t2412\t-60\t${daOff ? 'off' : 'failed:' + until}\n`;
const KERN = '[   87.028611] wlan0: disconnect from AP bc:96:e5:88:1f:d9 for new auth to bc:96:e5:88:1f:da\n[   87.849511] wlan0: authentication with bc:96:e5:88:1f:da timed out\n[  132.709428] wlan0: associated';
const api = (a, q, m) => { const sp = new URLSearchParams(q || '');
  if (a == 'wifi/status') return {ok: true, raw: 'adapter=wlan0\nwpa_state=COMPLETED\nssid=SpectrumSetup-3E\nbssid=bc:96:e5:88:1f:d9\nip_address=192.168.1.11\nRSSI=-53\nwlan=wlan0\tf8:54:f6:2f:6a:1f\tpci\trtw88_8821ce\trtw88_8821ce\n'};
  if (a == 'wifi/scan') return {ok: true, raw: 'bssid / frequency / signal level / flags / ssid\nbc:96:e5:88:1f:d9\t5220\t-53\t[WPA2-PSK-CCMP]\tSpectrumSetup-3E\n'};
  if (a == 'wifi/radios') return {ok: true, raw: radios()};
  if (a == 'wifi/radio' && m == 'POST') { posts.push(sp.get('bssid') + ' ' + sp.get('on')); daOff = sp.get('on') == 'off'; return {ok: true, message: 'Radio turned ' + sp.get('on')} }
  if (a == 'ai/facts') return {ok: true, version: '1.5.9', uptime: 300, wifi: 'Fri Nov 13 07:51 wifi: network 0: left out (did not answer): bc:96:e5:88:1f:da', wifiKernel: KERN, wifiNow: radios()};
  if (a == 'ai/status') return {ok: true, installed: true, loaded: false};
  return {ok: true} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/blitzen-docs.json') { s.writeHead(200); return s.end(fs.readFileSync(path.join(WWW, 'blitzen-docs.json'))) }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs, q.method))) }
    s.writeHead(404); s.end() }) }).listen(8272, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8272/'); await p.waitForFunction(() => typeof wifiPanel == 'function' && typeof bzWhy == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});const d=document.createElement('div');d.id='wft';document.body.appendChild(d);wifiPanel(d,'');document.querySelector('#wft .wfrad').open=true;return 1})()`); await wait(1500);
  const rows = await q("[...document.querySelectorAll('#wft .wfradr')].map(r=>[r.textContent.replace(/\\s+/g,' ').trim(),r.querySelector('input').checked])");
  ok('1. the adapter\'s bands', /This computer's Wi-Fi: 2\.4 GHz, 5 GHz/.test(await q("document.querySelector('#wft .wfradb').textContent")));
  ok('2. both radios: 2.4 GHz channel 1 and 5 GHz channel 44, the one in use marked', rows.length == 2 && /2\.4 GHz · channel 1/.test(rows[0][0]) && /5 GHz · channel 44 · In use/.test(rows[1][0]) && rows[1][1], rows);
  ok('3. the 2.4 GHz radio that did not answer: "left out until …", switched off', /Didn't answer: left out until/.test(rows[0][0]) && !rows[0][1], rows);
  await q("(()=>{const c=document.querySelectorAll('#wft .wfradr input')[0];c.checked=true;c.dispatchEvent(new Event('change'));return 1})()"); await wait(800);
  ok('4. switching it on asks rudolfd (wifi/radio … on)', posts.at(-1) == 'bc:96:e5:88:1f:da on', posts);
  await q("(()=>{const c=document.querySelectorAll('#wft .wfradr input')[0];c.checked=false;c.dispatchEvent(new Event('change'));return 1})()"); await wait(1200);
  ok('5. and off again: it shows "Turned off"', posts.at(-1) == 'bc:96:e5:88:1f:da off' && /Turned off/.test(await q("document.querySelectorAll('#wft .wfradr')[0].textContent")), posts);
  ok('6. only one radio left on: a warning', /Only one radio of this network is left on/.test(await q("document.querySelector('#wft .wfradb').textContent")));
  // Blitzen's diagnosis: what it would read, and the knowledge it gets
  ok('7. "Why does the Wi-Fi drop?" is there (Blitzen available)', await q("!document.querySelector('#wft .wfwhy').hidden"));
  await q("(()=>{window.__ask=null;blzAskFrom=(a,qq,ctx,o)=>{window.__ask={a,qq,ctx,o}};document.querySelector('#wft .wfwhy').click();return 1})()"); await wait(1500);
  const parts = await q("[...document.querySelectorAll('.bzrev label, .bzrev b, .bzrev summary')].map(x=>x.textContent).join(' | ')");
  ok('8. the review shows the Wi-Fi\'s log, the kernel\'s Wi-Fi messages and the Wi-Fi now', /Wi-Fi log/.test(parts) && /Wi-Fi \(kernel\)/.test(parts) && /The Wi-Fi now/.test(parts), parts);
  await q("(()=>{const b=[...document.querySelectorAll('.bzrev button')].find(x=>/Ask|Continue|Go|OK/i.test(x.textContent));b&&b.click();return 1})()"); await wait(800);
  const ask = await q('__ask');
  ok('9. Blitzen is asked which Wi-Fi settings to change, with the kernel lines (the radio that timed out) and the radios', ask && /Wi-Fi keep dropping/.test(ask.qq) && /authentication with bc:96:e5:88:1f:da timed out/.test(ask.ctx) && /radio=bc:96:e5:88:1f:d9\t5220\t-53\tinuse/.test(ask.ctx) && ask.o.pack == 'wifi', ask && {qq: ask.qq, pack: ask.o.pack, ctx: ask.ctx.slice(0, 300)});
  ok('10. its knowledge: how to read the lines, and only the settings that exist ("Radios of this network", Adapter, Country, Forget)', await q("/authentication with X timed out/.test(BLZ_PACKS.wifi)&&/Radios of this network/.test(BLZ_PACKS.wifi)&&/recommend only these/.test(BLZ_PACKS.wifi)&&/Never tell the person to run commands/.test(BLZ_PACKS.wifi)"));
  ok('12. a Wi-Fi problem asked in Blitzen\'s chat goes the same way (logs first); a how-to question does not', await q("bzWifiQ('why does my wifi keep dropping?')&&bzWifiQ('The internet keeps cutting out')&&bzWifiQ('my connection is slow and unstable')&&!bzWifiQ('how do I join a wifi network?')&&!bzWifiQ('write a python script')"));
  ok('11. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  const k = R.filter(Boolean).length; console.log(`${k}/${R.length} passed`); process.exit(k == R.length ? 0 : 1);
})().catch(e => { console.log('ERROR', e.stack); process.exit(1) });
