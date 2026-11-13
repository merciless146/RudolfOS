// 1.5.9 (plan Part 4): Settings › Network › Share this connection (administrators, password each time). A stand-in
// rudolfd.   node hotspot59_t.js [CHROME]
// everything; a call holds off the screen saver, the lock and sleep. A stand-in rudolfd.   node call59_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
let st = {on: false, name: '', band: '', devices: 0, uplink: 'wlan0', wifi: 'wlan0', canAP: true, both: false, saved: false, why: "This computer's Wi-Fi cannot share the Wi-Fi it is connected to: connect by cable, a phone's USB tethering or a modem, then share that."}; const posts = [];
const api = (a, q, m, b) => { const sp = new URLSearchParams(q || '');
  if (a == 'net/hotspot') { if (m == 'POST') { posts.push(sp.get('on') == '0' ? 'off' : 'on ' + b.replace(/\n/g, '|')); if (sp.get('on') == '0') { st.on = false; return {ok: true, message: 'The hotspot is off.'} } const [n, , band] = b.split('\n'); Object.assign(st, {on: true, name: n, band, devices: 1, uplink: 'usb0', saved: true}); return {ok: true, message: 'Sharing this connection as ' + n + '.'} } return Object.assign({ok: true}, st) }
  return {ok: true} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  let body = ''; q.on('data', d => body += d); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/a/')) { const f = path.join(WWW, u); if (fs.existsSync(f)) { s.writeHead(200); return s.end(fs.readFileSync(f)) } }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs, q.method, body))) }
    s.writeHead(404); s.end() }) }).listen(8278, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8278/'); await p.waitForFunction(() => typeof hsRandPw == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x'},{n:'Ben',r:'user',h:'y'}];USER=ACCTS[0];pwCheck=async(a,pw)=>a.n=='Ann'&&pw=='sesame';return 1})()`);
  const open = async () => { await q("(()=>{const o=document.getElementById('nt');o&&o.remove();const d=document.createElement('div');d.id='nt';document.body.appendChild(d);netPanelS(d);return 1})()"); await wait(1200) };
  await open();
  let t = await q("[document.querySelector('#nt .hs59 .hsl')?.textContent||'',document.querySelector('#nt .hst')?.disabled]");
  ok('1. connected by Wi-Fi on a chip that cannot do both: off, why, and no way to start it', /cannot share the Wi-Fi it is connected to/.test(t[0]) && t[1] === true, t);
  st.why = ''; st.uplink = 'usb0'; await open();
  ok('2. with a phone\'s tethering: a name, a new random password and the band offered', await q("document.querySelector('#nt .hsp').value.length==12&&/^Rudolf-/.test(document.querySelector('#nt .hsn').value)&&!document.querySelector('#nt .hst').disabled"));
  await q("(()=>{const r=document.querySelector('#nt .hs59');r.querySelector('.hsn').value='My hotspot';r.querySelector('.hsp').value='secret123';r.querySelector('.hsb').value='5';r.querySelector('.hst').click();return 1})()"); await wait(400);
  ok('3. turning it on asks for the administrator\'s password', /Share this computer.s connection as the Wi-Fi network My hotspot/.test(await q("document.querySelector('.rmodal .rmr')?.textContent||''")) && posts.length == 0);
  await p.type('.rmodal .ep', 'sesame'); await p.keyboard.press('Enter'); await wait(1500);
  ok('4. rudolfd gets the name, password and band (in the body, not the address)', posts.at(-1) == 'on My hotspot|secret123|5', posts);
  t = await q("document.querySelector('#nt .hs59 .hsl')?.textContent||''");
  ok('5. on: its name, the devices connected, the connection shared, the band', /On · My hotspot · 1 device connected · sharing usb0 · 5 GHz/.test(t), t);
  await q("document.querySelector('#nt .hst').click()"); await wait(400); await p.type('.rmodal .ep', 'sesame'); await p.keyboard.press('Enter'); await wait(1200);
  ok('6. off, with the password asked again', posts.at(-1) == 'off', posts);
  await q('USER=ACCTS[1]'); await open();
  ok('7. a standard user cannot turn it on', await q("document.querySelector('#nt .hst').disabled&&/Only an administrator/.test(document.querySelector('#nt .hs59').textContent)"));
  ok('8. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
