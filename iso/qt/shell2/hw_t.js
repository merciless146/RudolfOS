// Check this computer 1.2.5 (Task Manager › Hardware): every step with a stand-in rudolfd, Chromium's fake microphone
// (a beep) and its software WebGL. node hw_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path'), {execSync} = require('child_process');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.mjs': 'text/javascript', '.html': 'text/html'};
const M = {calls: [], usbAt: 0, speedFail: false};
const api = (a, q) => {
  M.calls.push(a + (q ? '?' + q : ''));
  if (a == 'system') return {ok: true, renderer: null, display: 'gpu-nomodeset', audio: ' 0 [PCH            ]: HDA-Intel - HDA Intel PCH\n                      HDA Intel PCH at 0xf7f10000 irq 32\n 1 [NVidia         ]: HDA-Intel - HDA NVidia\n',
    net: 'eth0 up 100 1000 2000 wired r8169\nwlan0 up - 10 20 wifi mt7921e\n', services: {sound: true}, machine: 'Test', firmware: 'Test', kernel: {boot: 'uefi', messages: ''}, cpu: {}, memory: {total: 8e9}, gpu: '', storage: '', input: ''};
  if (a == 'wifi/scan') return {ok: true, raw: 'bssid / frequency / signal level / flags / ssid\naa:bb:cc:dd:ee:01\t2437\t-40\t[WPA2-PSK-CCMP][ESS]\tOne\naa:bb:cc:dd:ee:02\t5180\t-60\t[WPA2-PSK-CCMP][ESS]\tTwo\naa:bb:cc:dd:ee:03\t2412\t-70\t[ESS]\tThree'};
  if (a == 'speedtest') return M.speedFail ? {__status: 502, ok: false, error: 'the update server could not be reached'} : {ok: true, bytes: 10693240, ms: 2000};
  if (a == 'disks') { return {ok: true, disks: [{name: 'sda', bytes: 5e11, removable: false, model: 'SSD', partitions: []}].concat(M.usbAt && Date.now() > M.usbAt ? [{name: 'sdz', bytes: 16e9, removable: true, model: 'Kingston DataTraveler', partitions: []}] : [])} }
  if (a == 'stats') return {ok: true, battery: [], cpu: 1, memTotal: 8e9, memAvailable: 4e9, load: 0};
  return {ok: true};
};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs))) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(8093, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--use-fake-ui-for-media-stream', '--use-fake-device-for-media-stream', '--autoplay-policy=no-user-gesture-required']});
  const p = await b.newPage(); p.on('pageerror', e => /toUpperCase/.test(e.message) || console.log('page error:', e.message));
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8093/'); await p.waitForFunction(() => typeof hwCheck == 'function' && typeof taskmgr26 == 'function', null, {timeout: 30000});
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';window.elevate=async()=>true;return 1})()");
  await q("launch('taskmgr')"); await wait(1500); await q("[...document.querySelectorAll('.tmtabs button')].find(b=>b.dataset.t=='hw').click()");
  ok('Task Manager › Hardware has Check this computer', await p.waitForFunction(() => !!document.querySelector('.hwcgo'), null, {timeout: 10000}).then(() => true, () => false));
  await q("document.querySelector('.hwcgo').click()"); await wait(500);
  const step = () => q("[document.querySelector('.hwc .hwcn').textContent,document.querySelector('.hwc h3').textContent,document.querySelector('.hwc .hwcb').innerText,[...document.querySelectorAll('.hwc .hwcbt button')].map(b=>b.textContent).join('|')]");
  const click = l => q(`[...document.querySelectorAll('.hwc .hwcbt button')].find(b=>b.textContent==${JSON.stringify(l)}).click()`);
  const until = (l, t = 15000) => p.waitForFunction(l => [...document.querySelectorAll('.hwc .hwcbt button')].some(b => b.textContent == l), l, {timeout: t}).then(() => true, () => false);
  let s = await step();
  ok('step 1: sound, with the cards and the question', s[0] == 'Step 1 of 6' && s[1] == 'Sound' && /2 sound cards/.test(s[2]) && /Yes, I heard both/.test(s[3]), s);
  await click('Yes, I heard both'); await wait(300); s = await step();
  ok('step 2: microphone (Chromium\'s fake microphone beeps)', s[1] == 'Microphone', s);
  await until('Next', 10000); s = await step();
  ok('...hears it', /Heard you/.test(s[2]), s[2]);
  await click('Next'); await until('Next', 20000); s = await step();
  ok('step 3: network: wired speed with the cable hint, Wi-Fi networks, the download speed', s[1] == 'Network' && /Wired eth0: connected, 100 Mb\/s/.test(s[2]) && /slower than this chip/.test(s[2]) && /Wi-Fi: 3 networks in range/.test(s[2]) && /Download: 43 Mb\/s/.test(s[2]), s[2]);
  await click('Next'); await wait(300); s = await step();
  ok('step 4: USB, waiting for a stick', s[1] == 'USB' && /Plug a USB stick/.test(s[2]), s);
  M.usbAt = Date.now() + 3000;   // the stick is plugged in three seconds later
  await until('Next', 20000); s = await step();
  ok('...finds the one plugged in', /Found Kingston DataTraveler \(14\.9 GB\)/.test(s[2]), s[2]);
  await click('Next'); await until('Next', 20000); s = await step();
  ok('step 5: graphics: what draws, and frames per second', s[1] == 'Graphics' && /drawn by the processor/.test(s[2]) && /Frames per second: \d+/.test(s[2]) && /Graphics card has the choices/.test(s[2]), s[2]);
  await click('Next'); await wait(800); s = await step();
  ok('step 6: sleep, optional on a computer without a battery (Skip first)', s[1] == 'Sleep' && /Optional/.test(s[2]) && /Skip\|Sleep for 10 seconds/.test(s[3]), s);
  await click('Skip'); await wait(500); s = await step();
  ok('the results: works, problem, skipped', s[1] == 'Results' && /Sound\s+✓ works/.test(s[2]) && /Microphone\s+✓ works/.test(s[2]) && /Network\s+✓ works/.test(s[2]) && /USB\s+✓ works/.test(s[2]) && /Graphics\s+✗ problem\s+processor drawing/.test(s[2]) && /Sleep\s+– skipped/.test(s[2]), s[2]);
  ok('...with Report a problem when something failed', /Report a problem/.test(s[3]), s[3]);
  const rep = await q("sysReport('').then(r=>r.text)");
  ok('the system report has the check, without names', /Hardware check: sound pass, microphone pass \(-?\d+ dB\), network pass \(wired 100 Mb\/s, slow, wifi 3 networks, 43 Mb\/s down\), usb pass \(14\.9 GB\), graphics fail \(processor drawing, \d+ fps\), sleep skip/.test(rep) && !/One|Two|Three|Kingston/.test(rep.split('Hardware check')[1].split('\n')[0]), (rep.match(/Hardware check.*/) || [''])[0]);
  await click('Done');
  // a sleep test and a network that fails
  M.speedFail = true; M.usbAt = 0; await q("hwCheck();1"); await wait(300);
  await click('Yes, I heard both'); await until('Next', 10000); await click('Next'); await until('Next', 20000); s = await step();
  ok('a download that fails is a network problem', /Something is not working/.test(s[2]) && /Download: the update server could not be reached/.test(s[2]), s[2]);
  await click('Next'); await wait(500); M.usbAt = Date.now() + 2000; await until('Next', 20000); await click('Next'); await until('Next', 20000); await click('Next'); await wait(800);
  M.calls = []; await click('Sleep for 10 seconds'); await until('Next', 30000); s = await step();
  ok('Sleep for 10 seconds asks rudolfd to sleep with a 10 s wake alarm', M.calls.includes('power?action=suspend&wake=10'), M.calls.filter(c => /power/.test(c)));
  await b.close(); srv.close();
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
