// 1.6.0 (plan Part 5, stage A item 5): signal and status on the desktop: the taskbar's network icon with mobile data's
// bars (from the modem's signal), its operator and type, the network pop-up's row (in use, or in reserve behind Wi-Fi),
// an IPv6-only connection (the CLAT's default route) still shown as mobile data, Settings › Mobile network's signal
// details (RSRP, RSRQ, SINR with words), System health's "Mobile network" events. A stand-in rudolfd.
//   node signal60_t.js [CHROME]   (the native desktop draws the same icon: rudolf-native.c ic_net, same_t.sh)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www'); const PORT = 8515;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
let NET, SIG = {rate: 5, lte: {rsrp: -87.4, rsrq: -9, sinr: 17, rssi: -61}, nr5g: {rsrp: null, rsrq: null, sinr: null}, umts: {rscp: null, ecio: null, rssi: null}};
const cell = (o) => ({name: 'wwan0', state: 'unknown', mac: '', ip: '10.64.12.34/30', wireless: false, virtual: false, default: true, cellular: true, carrier: 'T-Mobile', tech: 'LTE', signal: 74, ...o});
const wifi = (o) => ({name: 'wlan0', state: 'up', ip: '192.168.1.5/24', wireless: true, virtual: false, default: false, ssid: 'Home', rssi: -55, ...o});
NET = {ok: true, interfaces: [cell()]};
const M = [{id: 0, maker: 'Quectel', model: 'EG25-G', state: 'connected', failed: '', locked: '', sim: true, simOp: '310260', simName: 'T-Mobile', operator: 'T-Mobile', reg: 'home', signal: '74', tech: 'lte', iface: 'wwan0'}];
const api = (a, qs, m) => {
  if (a == 'net') return NET;
  if (a == 'mobile/status') return {ok: true, running: true, modems: M, notes: [], route: 'wwan0', cellular: true};
  if (a == 'mobile/signal') return {ok: true, ...SIG};
  if (a == 'mobile/settings') return {ok: true, sim: true, simOp: '310260', simName: 'T-Mobile', auto: null, usage: {used: 0, limit: 0, startday: 1, pct: 0}, autoconnect: true, off: false, roaming: false, apn: '', apnuser: '', apnpass: false, mode: 'auto', modes: ['auto']};
  if (a == 'mobile/notes') return {ok: true, notes: [], last: 0};
  return {ok: true} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/a/')) { const f = path.join(WWW, u); if (fs.existsSync(f)) { s.writeHead(200); return s.end(fs.readFileSync(f)) } }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs || '', q.method))) }
    s.writeHead(404); s.end() }) }).listen(PORT, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 1000}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto(`http://127.0.0.1:${PORT}/`); await p.waitForFunction(() => typeof mobSignal == 'function' && typeof net31Poll == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x'}];USER=ACCTS[0];return 1})()`);
  const icon = async () => { await q("net31Poll().then(()=>{NETST.inet=true;netUpd()})"); await wait(400); return q("(()=>{const B=document.getElementById('net');const r=[...B.querySelectorAll('rect')];return {type:B.dataset.type,st:B.dataset.st,title:B.title,rects:r.length,on:r.filter(x=>x.getAttribute('fill')!='#4b5563').length}})()") };
  let i = await icon();
  ok('1. mobile data in use: the network icon shows four bars, three lit (signal 74 %), and says the operator and type', i.type == 'cell' && i.rects == 4 && i.on == 3 && /^Mobile data — connected.*\(T-Mobile · LTE\) · signal 74 %/.test(i.title), i);
  NET = {ok: true, interfaces: [cell({signal: 90, tech: '5G'})]}; i = await icon();
  ok('...a strong 5G signal: four bars, 5G', i.on == 4 && /T-Mobile · 5G/.test(i.title), i);
  NET = {ok: true, interfaces: [cell({signal: 20})]}; i = await icon(); ok('...a weak one: one bar', i.on == 1, i);
  NET = {ok: true, interfaces: [cell({ip: '', default: false}), {name: 'clat', state: 'unknown', ip: '192.0.0.1/32', virtual: true, default: true}]}; i = await icon();
  ok('2. IPv6-only (the CLAT carries the default route): still mobile data, connected', i.type == 'cell' && i.st == 'on' && /^Mobile data — connected \(/.test(i.title), i);
  NET = {ok: true, interfaces: [wifi({default: true}), cell({default: false})]}; i = await icon();
  ok('3. Wi-Fi in use: the Wi-Fi icon', i.type == 'wifi', i);
  await q("(()=>{const B=document.getElementById('net');B.click();return 1})()"); await wait(800);
  const rows = await q("[...document.querySelectorAll('.np31 .npr')].map(r=>r.textContent)");
  ok('...the network pop-up: mobile data with its operator and type, in reserve', rows.some(r => /📶Mobile data · T-Mobile · LTEIn reserve/.test(r)) && rows.some(r => /Wi-Fi · HomeConnected/.test(r)), rows);
  await q("(()=>{const o=document.querySelector('.np');o&&o.remove();return 1})()");
  await q("(()=>{let d=document.getElementById('mt');if(d)d.remove();d=document.createElement('div');d.id='mt';document.body.appendChild(d);mobPanelS(d);return 1})()"); await wait(1800);
  let x = await q("document.querySelector('#mt .mobsigl')?.textContent||''");
  ok("4. Settings › Mobile network: the signal's details with words (RSRP good, RSRQ excellent, SINR good, RSSI)", x == 'LTE: RSRP -87.4 dBm (good) · RSRQ -9 dB (excellent) · SINR 17 dB (good) · RSSI -61 dBm', x);
  SIG = {rate: 5, lte: {rsrp: -112, rsrq: -17, sinr: 2, rssi: -80}, nr5g: {rsrp: -110, rsrq: -14, sinr: 4}, umts: {}}; await wait(5600);
  x = await q("document.querySelector('#mt .mobsigl')?.textContent||''");
  ok('...every few seconds: weaker (poor, fair), with 5G', /RSRP -112 dBm \(poor\)/.test(x) && /RSRQ -17 dB \(fair\)/.test(x) && /SINR 2 dB \(fair\)/.test(x) && /5G: RSRP -110 dBm/.test(x), x);
  ok("5. System health names the modem's events: Mobile network", await q("HL_KIND.modem&&HL_KIND.modem[1]=='Mobile network'"));
  ok('6. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
