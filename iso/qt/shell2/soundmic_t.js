// 1.5.4 (settings25.js, Sound): with the AMD audio processor's microphones (a card that records only), Settings ›
// Sound offers only cards that play as outputs, and lists the microphones in words. The Vivobook's cards as they
// should be: HD-Audio (speakers, headphones, headset microphone), HD-Audio HDMI, acp6x (the digital microphones).
// The desktop page with a stand-in rudolfd, in Chromium.   node soundmic_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
const CARDS = ' 0 [Generic        ]: HDA-Intel - HD-Audio Generic\n                      HD-Audio Generic at 0xd05c8000 irq 76\n 1 [Generic_1      ]: HDA-Intel - HD-Audio Generic\n                      HD-Audio Generic at 0xd05c0000 irq 77\n 2 [acp6x          ]: acp6x - acp6x\n                      ASUSTeK COMPUTER INC.-Vivobook Go E1404FA_E1404FA\n';
const PCM = '00-03: HDMI 0 : HDMI 0 : playback 1\n01-00: ALC256 Analog : ALC256 Analog : playback 1 : capture 1\n02-00: acp6x-dmic-capture DMIC-0 : : capture 1\n';
let mode = 'new';
const api = a => { if (a == 'audio') return mode == 'old' ? {ok: true, cards: CARDS} : {ok: true, cards: CARDS, pcm: PCM}; if (a == 'events/wait') return {ok: true, events: []}; return {ok: true} };
const srv = http.createServer((q, s) => { const [u0] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5)))) }
    s.writeHead(404); s.end() }) }).listen(8095, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8095/'); await p.waitForFunction(() => typeof volPanelS == 'function', null, {timeout: 30000});
  const show = () => p.evaluate(() => { let d = document.getElementById('sdt'); if (d) d.remove(); d = document.createElement('div'); d.id = 'sdt'; document.body.appendChild(d); volPanelS(d, null) });
  const get = () => p.evaluate(() => { const d = document.getElementById('sdt'); return {opts: [...d.querySelectorAll('select.card option')].map(o => o.value + ':' + o.textContent), mics: (d.querySelector('.mics') || {}).textContent} });
  await show(); await wait(1200); let g = await get();
  ok('1. outputs: only the cards that play (the HDA card and HDMI), not the microphone card', g.opts.length == 2 && g.opts[0].startsWith('0:') && g.opts[1].startsWith('1:'), g);
  ok('2. microphones: the AMD digital microphones in words', /Built-in digital microphones \(AMD audio processor\)/.test(g.mics), g);
  ok('3. ...and the headset microphone of the HDA card, not HDMI', /Generic_1 — HDA-Intel - HD-Audio Generic/.test(g.mics) && (g.mics.match(/·/g) || []).length == 1, g);
  mode = 'old'; await show(); await wait(1200); g = await get();
  ok('4. an older rudolfd (no pcm): every card offered, as before', g.opts.length == 3, g);
  ok('5. no page errors', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1)
})().catch(e => { console.log('FAIL ' + e.stack); process.exit(1) });
