// 1.5.9 (plan 7.4): a video call in the Standard engine (Chromium, as Oracle's tabs use it), with Chromium's stand-in
// camera and microphone: a page calls itself (two RTCPeerConnections), and the video and sound arrive. And with Lite,
// whose WebKitGTK has no WebRTC, the call sites open in Chromium's window.   node webrtc59_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 400) : '')) };
const PAGE = fs.readFileSync(path.join(__dirname, '../webkit/webrtc59_ci.py'), 'utf8').match(/PAGE = r'''([\s\S]*?)'''/)[1];
const RES = {};
const srv = http.createServer((q, s) => { let b = ''; q.on('data', d => b += d); q.on('end', () => {
  if (q.url == '/result') { const d = JSON.parse(b); RES[d.k] = d.v; s.writeHead(204); return s.end() }
  if (q.url == '/desk') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0,engine:"webkit"};</script>')) }
  if (q.url.startsWith('/a/')) { const f = path.join(WWW, q.url); if (fs.existsSync(f)) { s.writeHead(200); return s.end(fs.readFileSync(f)) } }
  if (q.url.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end('{"ok":true}') }
  s.writeHead(200, {'content-type': 'text/html'}); s.end(PAGE) }) }).listen(8279, '127.0.0.1');
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--use-fake-device-for-media-stream', '--use-fake-ui-for-media-stream']});
  const p = await b.newPage(); await p.goto('http://127.0.0.1:8279/');
  const t0 = Date.now(); while (!RES.done && Date.now() - t0 < 45000) await new Promise(r => setTimeout(r, 300));
  ok('1. Chromium has WebRTC; the camera and microphone give tracks', RES.rtc == 'function' && /audio:live/.test(RES.gum) && /video:live/.test(RES.gum), RES);
  ok('2. the call connects', ['connected', 'completed'].includes(RES.ice), RES);
  ok('3. video frames and sound arrive', !/^0x/.test(RES.video || '0x0') && RES.audio > 0, RES);
  // Lite: the desktop (WebKit without WebRTC) sends call sites to Chromium's window
  const d = await b.newPage(); await d.addInitScript(() => { delete window.RTCPeerConnection; window.RTCPeerConnection = undefined });
  await d.goto('http://127.0.0.1:8279/desk'); await d.waitForFunction(() => typeof rtChromium == 'function', null, {timeout: 30000});
  const r = await d.evaluate(() => { SHELL = SHELL || {}; SHELL_CAPS = ['tabs']; RD.engine = 'webkit'; return ['https://meet.google.com/abc-defg-hij', 'https://app.zoom.us/wc/join/1', 'https://teams.live.com/meet/1', 'https://example.com/'].map(rtChromium) });
  ok('4. Lite without WebRTC: Meet, Zoom\'s web client and Teams open in Chromium\'s window; other sites stay', JSON.stringify(r) == '[true,true,true,false]', r);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
