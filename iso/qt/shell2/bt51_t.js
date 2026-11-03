// 1.5.1 (bt51.js): Settings › Bluetooth and its notifications, against a stand-in for rudolfd's bt API (its status is
// what /usr/lib/rudolf/bluetooth status prints). node bt51_t.js [STATUS.json] [CHROME] (default: fixtures/bt51_status.json, which the bluetooth script printed with fixtures/fake-bluetoothctl standing in for BlueZ)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 300) : '')) };
const ST = JSON.parse(fs.readFileSync(process.argv[2] || path.join(__dirname, 'fixtures/bt51_status.json'), 'utf8')), POSTS = [];
const dev = m => ST.devices.find(d => d.mac == m);
const api = (q, s, body) => {
  const u = new URL(q.url, 'http://x'), p = u.pathname.slice(5); let j = q.method == 'POST' ? {ok: true} : {ok: false, error: 'not in the stand-in'}, code = 200;
  if (q.method == 'POST') POSTS.push(p + u.search);
  const m = u.searchParams.get('mac');
  if (p == 'bt') j = ST;
  else if (p == 'bt/power') { ST.powered = u.searchParams.get('on') == '1'; j = ST }
  else if (p == 'bt/scan') j = ST;
  else if (p == 'bt/pair') { if (m == 'AA:BB:CC:DD:EE:03') { Object.assign(dev(m), {paired: true, connected: true, name: 'Keyboard K380', icon: 'input-keyboard'}); j = {ok: true, status: ST} } else { code = 409; j = {ok: false, error: 'Failed to pair: org.bluez.Error.AuthenticationFailed'} } }
  else if (p == 'bt/connect') { dev(m).connected = true; j = {ok: true, status: ST} }
  else if (p == 'bt/remove') { ST.devices = ST.devices.filter(d => d.mac != m); j = {ok: true, status: ST} }
  else if (p == 'events/wait') return setTimeout(() => { s.writeHead(200, {'content-type': 'application/json'}); s.end('{"ok":true,"events":[]}') }, 20000);
  s.writeHead(code, {'content-type': 'application/json'}); s.end(JSON.stringify(j));
};
const srv = http.createServer((q, s) => { let b = ''; q.on('data', d => b += d); q.on('end', () => {
  if (q.url.startsWith('/api/')) return api(q, s, b);
  s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', "<head><script>window.RUDOLFD={token:'t',base:'',boot:'uefi',media:'disk',live:0,recovery:0}</script>")) }) }).listen(8099, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[3] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8099/'); await p.waitForFunction(() => typeof btPanel == 'function' && typeof fs != 'undefined' && fs['C:'], null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});USER=USER||ACCTS[0];return 1})()");
  await q("settings('bt')"); await wait(800);
  const t = await q("(()=>{const c=SW.body.querySelector('.sc');return{h:c.querySelector('h3').textContent,on:c.querySelector('.bto').checked,mine:c.querySelector('.btm').innerText,near:c.querySelector('.btn').innerText}})()");
  ok('1. Settings has Bluetooth, on, with the paired devices and their state', t.h == 'Bluetooth' && t.on && /Xbox Wireless Controller/.test(t.mine) && /controller · connected · battery 12%/.test(t.mine) && /WH-1000XM4 "Pro"/.test(t.mine) && /headphones or speaker · not connected/.test(t.mine), t);
  ok('2. a device found nearby without a name is shown as Unnamed device, with Pair', /Unnamed device/.test(t.near) && /Pair/.test(t.near), t.near);
  await q("[...SW.body.querySelectorAll('.btn button')].find(b=>b.textContent=='Pair').click()"); await wait(800);
  const t2 = await q("[SW.body.querySelector('.btm').innerText,SW.body.querySelector('.btmsg').textContent]");
  ok('3. Pair pairs it: it moves to My devices, connected, and a message says so', POSTS.includes('bt/pair?mac=AA:BB:CC:DD:EE:03') && /Keyboard K380/.test(t2[0]) && /keyboard · connected/.test(t2[0]) && /paired and connected/.test(t2[1]), t2);
  ST.devices.push({mac: 'AA:BB:CC:DD:EE:04', name: 'Speaker', icon: 'audio-card', paired: false, connected: false, audio: true, battery: null, rssi: -50});
  await q("SW.show('bt',1)"); await wait(600);
  await q("[...SW.body.querySelectorAll('.btn button')].find(b=>b.textContent=='Pair').click()"); await wait(800);
  ok('4. a pairing that fails says what to do', /Make sure it is on, near the computer, and \(to pair\) in pairing mode/.test(await q("SW.body.querySelector('.btmsg').textContent")));
  await q("[...SW.body.querySelectorAll('.btm .acr')].find(r=>/WH-1000/.test(r.textContent)).querySelector('button').click()"); await wait(700);
  ok('5. Connect connects a paired device', POSTS.includes('bt/connect?mac=AA:BB:CC:DD:EE:02') && /WH-1000XM4[\s\S]*connected/.test(await q("SW.body.querySelector('.btm').innerText")));
  await q("[...SW.body.querySelectorAll('.btm .acr')].find(r=>/Keyboard/.test(r.textContent)).querySelectorAll('button')[1].click()"); await wait(700);
  ok('6. Forget forgets it', POSTS.includes('bt/remove?mac=AA:BB:CC:DD:EE:03') && !/Keyboard/.test(await q("SW.body.querySelector('.btm').innerText")));
  dev('AA:BB:CC:DD:EE:02').connected = false;
  await q("(async()=>{BT_SEEN=null;await btWatch();return 1})()");   // the first look: no notifications
  await q("NC.length=0;1");
  dev('AA:BB:CC:DD:EE:01').connected = false; dev('AA:BB:CC:DD:EE:02').connected = true;
  await q("btWatch()"); await wait(400);
  const n = await q("NC.map(n=>n.t+' | '+n.b)");
  ok('7. notifications: a controller went away, headphones connected (sound goes there)', n.some(x => /Xbox Wireless Controller disconnected/.test(x)) && n.some(x => /WH-1000XM4 "Pro" connected \| Sound now plays through it/.test(x)), n);
  dev('AA:BB:CC:DD:EE:01').connected = true; await q("NC.length=0;btWatch()"); await wait(400);
  const n2 = await q("NC.map(n=>n.t)");
  ok('8. ...and a low battery once it connects again (12%)', n2.some(x => /battery low/.test(x)), n2);
  await q("SW.show('bt',1)"); await wait(400);
  await q("(()=>{const c=SW.body.querySelector('.bto');c.checked=false;c.dispatchEvent(new Event('change'))})()"); await wait(600);
  ok('9. the switch turns it off', POSTS.includes('bt/power?on=0') && await q("SW.body.querySelector('.bts').textContent") == 'Off');
  ST.adapter = null; await q("SW.show('bt',1)"); await wait(500);
  ok('10. no adapter: it says so (and that a USB adapter works)', /no Bluetooth adapter/.test(await q("SW.body.querySelector('.bts').textContent")));
  ok('11. no page errors', !errs.length, errs);
  console.log(`${R.filter(x => x).length}/${R.length} passed`); await b.close(); srv.close(); process.exit(R.every(x => x) ? 0 : 1);
})();
