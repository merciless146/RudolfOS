// 1.5.9: Settings › Network › Wi-Fi › "This computer's Wi-Fi radios": a drop-down menu between "Radios of this network"
// and "⚡ Why does the Wi-Fi drop?", with a check by each radio of this computer that is on and a blank space by one
// that is off. Only an administrator changes it, typing their password every time; a standard user sees it and cannot.
// A router's radio on a band that is off shows so and cannot be turned on. A stand-in rudolfd.   node wifibands59_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
let off5 = false; const posts = [];
const api = (a, q, m) => { const sp = new URLSearchParams(q || '');
  if (a == 'wifi/status') return {ok: true, raw: 'adapter=wlan0\nwpa_state=COMPLETED\nssid=SpectrumSetup-3E\nbssid=bc:96:e5:88:1f:d9\nip_address=192.168.1.11\nRSSI=-53\nwlan=wlan0\tf8:54:f6:2f:6a:1f\tpci\trtw88_8821ce\trtw88_8821ce\n'};
  if (a == 'wifi/scan') return {ok: true, raw: 'bssid / frequency / signal level / flags / ssid\n'};
  if (a == 'wifi/bands') return {ok: true, raw: `adapter=wlan0\tf8:54:f6:2f:6a:1f\tpci\trtw88_8821ce\nband=2.4\ton\nband=5\t${off5 ? 'off' : 'on'}\nfreq=${off5 ? 2412 : 5220}\n`};
  if (a == 'wifi/radios') return {ok: true, raw: `bands=2.4 5 \nbandsoff=${off5 ? '5 ' : ''}\nnet=0\nssid=SpectrumSetup-3E\nradio=bc:96:e5:88:1f:d9\t5220\t-53\t${off5 ? 'bandoff' : 'inuse'}\nradio=bc:96:e5:88:1f:da\t2412\t-60\t${off5 ? 'inuse' : 'on'}\n`};
  if (a == 'wifi/band' && m == 'POST') { posts.push(sp.get('band') + ' ' + sp.get('on')); off5 = sp.get('band') == '5' && sp.get('on') == 'off'; return {ok: true, message: `This computer's ${sp.get('band')} GHz radio turned ${sp.get('on')}`} }
  if (a == 'ai/status') return {ok: true, installed: true, loaded: false};
  return {ok: true} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/a/')) { const f = path.join(WWW, u); if (fs.existsSync(f)) { s.writeHead(200); return s.end(fs.readFileSync(f)) } }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs, q.method))) }
    s.writeHead(404); s.end() }) }).listen(8273, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8273/'); await p.waitForFunction(() => typeof wifiPanel == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  const open = async () => { await q(`(()=>{const o=document.getElementById('wft');o&&o.remove();const d=document.createElement('div');d.id='wft';document.body.appendChild(d);wifiPanel(d,'');document.querySelector('#wft .wfrad').open=true;return 1})()`); await wait(1200) };
  const menu = async () => { await q("document.querySelector('#wft .wfbdb').click()"); await wait(500);
    return q("[...document.querySelectorAll('#wft .wfbdi')].map(i=>({check:i.querySelector('.wfbdc').textContent,text:i.textContent.replace(/\\s+/g,' ').trim(),dis:i.disabled,aria:i.getAttribute('aria-checked')}))") };
  // a standard user first
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x'},{n:'Ben',r:'user',h:'y'}];USER=ACCTS[1];pwCheck=async(a,pw)=>a.n=='Ann'&&pw=='sesame';return 1})()`);
  await open();
  const order = await q("(()=>{const w=document.querySelector('#wft'),k=[...w.querySelectorAll('.wfrad,.wfbd,.wfwhy')].map(e=>e.className.split(' ')[0]);return k})()");
  ok('1. the menu sits between "Radios of this network" and "Why does the Wi-Fi drop?"', JSON.stringify(order) == '["wfrad","wfbd","wfwhy"]', order);
  let it = await menu();
  ok('2. the computer\'s radios, each with a check on the left when on', it.length == 2 && it[0].check == '✓' && /^✓\s*2\.4 GHz radio/.test(it[0].text) && it[1].check == '✓' && /5 GHz radio\s*in use/.test(it[1].text) && it.every(x => x.aria == 'true'), it);
  if (process.env.SHOT) await p.screenshot({path: process.env.SHOT, clip: {x: 0, y: 0, width: 800, height: 700}});
  ok('3. a standard user cannot change it, and is told only an administrator can', it.every(x => x.dis) && /Only an administrator can change this/.test(await q("document.querySelector('#wft .wfbdn').textContent")), it);
  // an administrator: the password each time, even just after another approval
  await q("USER=ACCTS[0];window.__elevTrust=Date.now()"); await open(); it = await menu();
  ok('4. an administrator can choose', it.every(x => !x.dis), it);
  await q("document.querySelectorAll('#wft .wfbdi')[1].click()"); await wait(400);
  const dlg = await q("(()=>{const o=document.querySelector('.rmodal .rmr');return o?o.textContent:''})()");
  ok('5. turning the 5 GHz radio off asks for the administrator\'s password (even right after another approval)', /Turn off this computer's 5 GHz Wi-Fi radio/.test(dlg) && posts.length == 0, dlg);
  await p.type('.rmodal .ep', 'wrong'); await p.keyboard.press('Enter'); await wait(400);
  ok('6. a wrong password changes nothing', posts.length == 0 && /Wrong password/.test(await q("document.querySelector('.rmodal .rmmsg').textContent")), posts);
  await p.type('.rmodal .ep', 'sesame'); await p.keyboard.press('Enter'); await wait(1500);
  ok('7. the right one: rudolfd turns the 5 GHz radio off', posts.at(-1) == '5 off', posts);
  it = await menu();
  ok('8. the 5 GHz radio now has a blank space where the check goes; the 2.4 GHz one keeps its check', it[1].check == '' && it[1].aria == 'false' && /^5 GHz radio/.test(it[1].text) && it[0].check == '✓', it);
  const note = await q("document.querySelector('#wft .wfbdn').textContent");
  ok('9. it says an administrator turned it off and Rudolf OS will not turn it back on', /5 GHz turned off by an administrator; Rudolf OS does not turn it back on by itself/.test(note), note);
  await q("document.body.click()"); await wait(1200);
  const rr = await q("[...document.querySelectorAll('#wft .wfradr')].map(r=>[r.textContent.replace(/\\s+/g,' ').trim(),r.querySelector('input').checked,r.querySelector('input').disabled])");
  ok('10. "Radios of this network": the router\'s 5 GHz radio shows this computer\'s radio is off, and cannot be turned on there', rr.some(r => /This computer's 5 GHz radio is off/.test(r[0]) && !r[1] && r[2]) && /turned off by an administrator/.test(await q("document.querySelector('#wft .wfradb').textContent")), rr);
  ok('11. the menu closes on a click outside', await q("document.querySelector('#wft .wfbdm').hidden"));
  it = await menu();
  await q("document.querySelectorAll('#wft .wfbdi')[0].click()"); await wait(400);
  ok('12. the last radio on cannot be turned off (no password asked, nothing sent)', !(await q("!!document.querySelector('.rmodal .ep')")) && posts.length == 1, posts);
  await q("window.__elevTrust=Date.now()"); it = await menu(); await q("document.querySelectorAll('#wft .wfbdi')[1].click()"); await wait(400);
  ok('13. turning it on asks for the password again too', /Turn on this computer's 5 GHz Wi-Fi radio/.test(await q("(()=>{const o=document.querySelector('.rmodal .rmr');return o?o.textContent:''})()")));
  await p.type('.rmodal .ep', 'sesame'); await p.keyboard.press('Enter'); await wait(1200);
  it = await menu();
  ok('14. on again: both checked', posts.at(-1) == '5 on' && it.every(x => x.check == '✓'), {posts, it});
  ok('15. Blitzen keeps an administrator\'s choice: a finding, and no "turn off" for a radio on a band that is off', await q(`(()=>{const a=bzWifiFindings({uptime:600,wifiNow:'bands=2.4 5 \\nbandsoff=5 \\nssid=Home\\nradio=aa:aa:aa:aa:aa:01\\t5180\\t-50\\tbandoff\\nradio=aa:aa:aa:aa:aa:02\\t2437\\t-60\\tinuse\\n',wifiKernel:'[ 5.0] wlan0: authentication with aa:aa:aa:aa:aa:01 timed out'});return /5 GHz radio is turned off by an administrator/.test(a.findings)&&!/Turn off the 5 GHz/.test(a.recs)&&/This computer's Wi-Fi radios/.test(BLZ_PACKS.wifi)})()`));
  ok('16. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
