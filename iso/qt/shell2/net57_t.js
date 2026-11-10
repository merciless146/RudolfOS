// 1.5.7 (plan stage 8): the network at start-up. netLive/netWait and the "rudolf-online" event from rudolfd's network
// state; the update check waits for the network instead of an hour; the Library's list is not kept empty after an
// offline try; buttons that need the internet wait for it; Oracle's "No internet connection" page opens the page
// once online.   node net57_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 600) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm', '.json': 'application/json'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8168, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await (await b.newContext()).newPage();
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.addInitScript(() => {
    window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0};
    window.M = {up: false, calls: [], lib: 0};
    const f0 = window.fetch;
    window.fetch = (u, o) => { u = String(u); const q = new URL(u, location.href); const a = q.pathname.replace('/api/', ''); const J = (x, st) => Promise.resolve(new Response(JSON.stringify(Object.assign({ok: !st}, x)), {status: st || 200, headers: {'content-type': 'application/json'}}));
      if (/catalog\.json/.test(u)) { M.lib++; return M.up ? J({apps: [{name: 'rudolf-emu', title: 'Emu'}]}) : Promise.reject(new TypeError('Failed to fetch')) }
      if (u.includes('/api/')) M.calls.push(((o && o.method) || 'GET') + ' ' + a);
      if (a == 'net') return J({wifiTools: true, interfaces: [{name: 'wlan0', wireless: true, state: 'up', ip: M.up ? '192.168.1.20' : '', default: M.up}]});
      if (a == 'sys/status') return J({persistent: true, auto: true, url: 'https://example/', checked: 0, slots: {}, state: {}});
      if (a == 'job') return J({running: false, status: 0, log: ''});
      if (a == 'sys/check') return J({});
      if (u.includes('/api/')) return J({items: [], apps: [], windows: []});
      return f0(u, o) } });
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8168/'); await p.waitForFunction(() => typeof netWait == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS.length=0;ACCTS.push({n:'ann',h:await pwMake('a1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'a1');window.__elevTrust=Date.now();return 1})()`);
  await q("net31Poll()"); await wait(300);
  ok('1. offline at start-up: netLive() is false (rudolfd: Wi-Fi up, no address)', (await q("netLive()")) === false);
  // 2. the update check
  await q("(()=>{M.calls.length=0;window.__ev=0;addEventListener('rudolf-online',()=>__ev++);window.__w=netWait().then(()=>'online');sysuTick();return 1})()"); await wait(800);
  ok('2. the update check offline: no check, it waits for the network (not an hour)', !(await q("M.calls.some(c=>/sys\\/check/.test(c))")) && await q("sysuTick.wait===1"), await q("M.calls"));
  // 3. the Library's list offline is not kept
  const c1 = await q("(async()=>{LIBCAT=null;const c=await libCatalog();return Object.keys(c).length})()");
  ok('3. the Library\'s list asked for offline: empty, and not kept', c1 === 0 && await q("LIBCAT===null"));
  // 4. a button that needs the internet waits
  await q("(()=>{window.__gate=[];window.__g=netGate(t=>__gate.push(t)).then(()=>'go');return 1})()"); await wait(200);
  ok('4. a button pressed offline says it waits for the internet', /Waiting for the internet/.test(await q("__gate[0]||''")) && (await q("Promise.race([__g,new Promise(r=>setTimeout(()=>r('waiting'),300))])")) == 'waiting');
  // 5. Oracle's offline page
  const pg = await q("(()=>{window.__net=1;const s=NETST.state;NETST.state='off';const d=secPage('https://example.com/news',(t,h,x)=>({t,h,x:x||''}),(u,t)=>t,s=>String(s));NETST.state=s;return d})()");
  ok('5. Oracle offline: "No internet connection", and it opens the page by itself once online', pg && pg.t == 'No internet connection' && /opens by itself once this computer is online/.test(pg.h) && /rudolf-online/.test(pg.x) && /go\(U\)/.test(pg.x) && /https:\/\/example.com\/news/.test(pg.x), pg);
  await q("(()=>{const f=document.createElement('iframe');f.id='ofl';f.srcdoc=\"<script>addEventListener('message',function(e){parent.__got=e.data})<\\/script>\";document.body.appendChild(f);return 1})()"); await wait(300);
  // 6. online: the event, the waits, the frames
  await q("(()=>{M.up=true;return net31Poll()})()"); await wait(500);
  ok('6. online: "rudolf-online" fires once, netWait and the button go on', (await q("__ev")) == 1 && (await q("__w")) == 'online' && (await q("__g")) == 'go' && /^$/.test(await q("__gate[1]")), [await q("__ev"), await q("__gate")]);
  ok('7. ...Oracle\'s pages are told ("rudolf-online" to every frame)', (await q("window.__got")) == 'rudolf-online');
  ok('8. ...the Library\'s list is had now', (await q("libCatalog().then(c=>Object.keys(c).join())")) == 'rudolf-emu');
  await wait(21500);
  ok('9. ...and the update check runs about 20 s after (not in an hour)', await q("M.calls.some(c=>/POST sys\\/check/.test(c))"), await q("M.calls.filter(c=>/sys/.test(c))"));
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
