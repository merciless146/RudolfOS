// 1.4.4 part 3 (net31.js): the taskbar's network icon follows the real network on installed Rudolf OS, with a
// stand-in rudolfd. node net31_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.mjs': 'text/javascript', '.html': 'text/html'};
const SIT = {name: 'sit0', state: 'down', mac: '00:00:00:00', ip: '', wireless: false, rx: 0, tx: 0, virtual: true, default: false, speed: -1};
const NETS = {
  wifi: [{name: 'wlan0', state: 'up', mac: 'aa', ip: '192.168.1.20/24', wireless: true, rx: 1, tx: 1, virtual: false, default: true, ssid: 'Home', rssi: -55}, SIT],
  wired: [{name: 'eth0', state: 'up', mac: 'bb', ip: '10.0.2.15/24', wireless: false, rx: 1, tx: 1, virtual: false, default: true, speed: 1000}, SIT],
  none: [{name: 'wlan0', state: 'down', mac: 'aa', ip: '', wireless: true, rx: 1, tx: 1, virtual: false, default: false}, SIT],
};
const M = {calls: [], ev: [], net: 'wifi', rd: true};
const api = (a, q) => {
  M.calls.push(a + (q ? '?' + q : ''));
  if (a == 'net') return {ok: true, interfaces: NETS[M.net], gateway: M.net == 'none' ? '' : '192.168.1.1', dns: '1.1.1.1', wifiTools: true};
  if (a == 'wifi/status') return {ok: true, raw: 'adapter=wlan0\nwpa_state=' + (M.net == 'wifi' ? 'COMPLETED\nssid=Home' : 'DISCONNECTED')};
  if (a == 'wifi/scan') return {ok: true, raw: 'bssid / frequency / signal level / flags / ssid\n'};
  if (a == 'events') { const e = M.ev.join('\n'); M.ev = []; return {ok: true, events: e} }
  if (a == 'info') return {ok: true, backlight: false, live: 0, boot: 'uefi', kernel: 'test'};
  if (a == 'volume') return {ok: true, volume: 50, muted: false};
  return {ok: true};
};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', async () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); const h = fs.readFileSync(path.join(WWW, 'index.html'), 'utf8');
      return s.end(M.rd ? h.replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>') : h) }
    if (u == '/api/events/wait') for (let i = 0; i < 30 && !M.ev.length; i++) await new Promise(r => setTimeout(r, 100));
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5) == 'events/wait' ? 'events' : u.slice(5), qs))) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(8095, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  const p = await b.newPage(); p.on('pageerror', e => console.log('page error:', e.stack.slice(0, 400)));
  await p.route('https://www.gstatic.com/generate_204', r => r.fulfill({status: 204, body: ''}));
  await p.goto('http://127.0.0.1:8095/'); await p.waitForFunction(() => typeof net31Upd == 'function' && typeof launch == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';window.elevate=async()=>true;return 1})()");
  await p.waitForFunction(() => /Wi-Fi/.test(document.getElementById('net').title), null, {timeout: 10000}).catch(() => {});
  const icon = () => q("[$('#net').title,$('#net').dataset.type,$('#net').dataset.st]");
  let i = await icon();
  ok('1. Wi-Fi with the default route: the Wi-Fi icon, "Wi-Fi — connected (Home)"', /^Wi-Fi — connected \(Home\)/.test(i[0]) && i[1] == 'wifi' && i[2] == 'on', i);
  ok('...never the simulated "Ethernet"', !/Ethernet/.test(i[0]), i);
  await q("$('#net').click()"); await wait(400);
  const pop = await q("document.querySelector('.np31')&&document.querySelector('.np31 .npl').innerText");
  ok('2. the popup lists the real adapters (Wi-Fi · Home), not sit0', /Wi-Fi · Home/.test(pop || '') && !/sit0/.test(pop || ''), pop);
  await q("document.querySelector('.np').remove()");
  M.net = 'none'; M.ev.push('wifi disconnected'); const t0 = Date.now();
  await p.waitForFunction(() => document.getElementById('net').dataset.st == 'off', null, {timeout: 5000}).catch(() => {});
  i = await icon();
  ok('3. a "wifi disconnected" event: "Not connected" within a second', i[0] == 'Not connected' && i[2] == 'off' && Date.now() - t0 < 1500, [i, Date.now() - t0]);
  ok('...and apps see no network (netUp)', await q("netUp()") === false);
  M.net = 'wired'; await q("net31Poll()"); await wait(800);
  i = await icon();
  ok('4. wired: the wired icon and "Ethernet — connected (eth0, 1 Gb/s)"', i[0] == 'Ethernet — connected (eth0, 1 Gb/s)' && i[1] == 'wired', i);
  ok('...and apps see the network (netUp)', await q("netUp()") === true);
  await q("NETST.inet=false;netUpd()");
  ok('5. the internet check still marks "no internet access detected"', /no internet access detected/.test((await icon())[0]) && (await icon())[2] == 'limited', await icon());
  await q("NETST.inet=true;netUpd()");
  await q("settings('net')"); await wait(1500);
  const ns = await q("(()=>{const w=wins.find(w=>/Settings/.test(w.title||''))||null;const c=document.querySelector('.nh');const rows=c?[...c.querySelectorAll('tr')].map(r=>r.innerText):[];const conn=[...document.querySelectorAll('h4')].find(h=>/^Connections$/.test(h.textContent));return {rows,connHidden:!conn||conn.style.display=='none',line:!!document.querySelector('.nsim')}})()");
  ok('6. Network Settings: no virtual interface in Network hardware', ns.rows.length == 1 && !ns.rows.some(r => /sit0/.test(r)), ns.rows);
  ok('...the simulated Connections are hidden, and a line says why', ns.connHidden && ns.line, ns);
  await p.close();
  M.rd = false; const p2 = await b.newPage(); await p2.goto('http://127.0.0.1:8095/'); await p2.waitForFunction(() => typeof launch == 'function', null, {timeout: 30000}); await wait(1500);
  await p2.evaluate("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});USER=USER||ACCTS[0];USER.r='admin';settings('net')})()"); await wait(1500);
  const br = await p2.evaluate("[$('#net').title,!!window.__net31,[...document.querySelectorAll('h4')].some(h=>/^Connections$/.test(h.textContent)&&h.offsetParent)]");
  ok('7. the browser version: the simulated connection as before, with Connections in Network Settings', /Ethernet/.test(br[0]) && !br[1] && br[2], br);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
