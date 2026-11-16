// 1.6.0 (plan Part 4, stage A item 4): data and money on the desktop: Settings › Mobile network's Data this month (the
// count, the bar at 80 % and 100 %, the limit, the month's start day, mobile data off at the limit), and updates on
// mobile data offered with their size, never downloaded by themselves. A stand-in rudolfd.   node data60_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www'); const PORT = 8514;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const M = [{id: 0, maker: 'Quectel', model: 'EG25-G', state: 'connected', failed: '', locked: '', sim: true, simOp: '310260', simName: 'T-Mobile', operator: 'T-Mobile', reg: 'home', signal: '70', tech: 'lte', iface: 'wwan0'}];
let use = {key: 'sim:0123456789abcdef', used: 3456000000, start: '2026-11-01', next: '2026-12-01', limit: 0, startday: 1, dataoff: false, pct: 0, off: false}, calls = [], dl = 0;
const api = (a, qs, m) => { const p = new URLSearchParams(qs);
  if (a == 'mobile/status') return {ok: true, running: true, modems: M, notes: [], route: 'wwan0', cellular: true};
  if (a == 'mobile/settings') { if (m == 'POST') { calls.push(qs); const k = p.get('k'), v = p.get('v'); if (k == 'limit') { use.limit = +v; use.pct = v > 0 ? Math.floor(use.used * 100 / (v * 1e6)) : 0 } else if (k == 'startday') use.startday = +v; else if (k == 'dataoff') use.dataoff = v == '1'; return {ok: true, message: 'saved'} }
    return {ok: true, sim: true, simOp: '310260', simName: 'T-Mobile', auto: {name: 'T-Mobile', apn: 'fast.t-mobile.com', user: '', source: 'database'}, usage: use, autoconnect: true, off: false, roaming: false, apn: '', apnuser: '', apnpass: false, mode: 'auto', modes: ['auto']} }
  if (a == 'mobile/notes') return {ok: true, notes: [], last: 0};
  if (a == 'sys/status') return {ok: true, persistent: true, auto: true, url: 'https://x/stable', current: {version: '1.6.0'}, state: {good: 'a'}, slots: {}, booted: 'a', checked: Date.now() / 1000, available: {serial: 2, version: '1.6.1', notes: '', size: 42000000, urgent: false}};
  if (a == 'sys/download') { dl++; return {ok: true} }
  if (a == 'net/metered') return {ok: true, connected: true, if: 'wwan0', kind: 'usbmodem', metered: true, auto: 'modem', used: 0};
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
  await p.goto(`http://127.0.0.1:${PORT}/`); await p.waitForFunction(() => typeof mobSettings == 'function' && typeof sysuDlLabel == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x'}];USER=ACCTS[0];return 1})()`);
  const open = async () => { await q("(()=>{let d=document.getElementById('mt');if(d)d.remove();d=document.createElement('div');d.id='mt';document.body.appendChild(d);mobPanelS(d);return 1})()"); await wait(1200) };
  const t = () => q("document.getElementById('mt').textContent");
  await open(); let x = await t();
  ok('1. Data this month: the count (3.46 GB) since the month began, until the next; no limit: no bar', /Data this month/.test(x) && /3\.46 GB since 2026-11-01, until 2026-12-01/.test(x) && await q("document.querySelector('#mt .mobbar').style.display=='none'"), x);
  ok("...said that it is counted here and may differ a little from the operator's", /your operator's count can differ a little/.test(x) && /A notice at 80 %/.test(x));
  await q("(()=>{const i=document.querySelector('#mt .moblim');i.value='4000';i.dispatchEvent(new Event('change'));return 1})()"); await wait(500); await open(); x = await t();
  ok('2. a limit of 4000 MB: sent; 86 %, the bar amber', calls.some(c => /k=limit&v=4000/.test(c)) && /3\.46 GB of 4000 MB \(86 %\)/.test(x) && (await q("document.querySelector('#mt .mobbarv').style.width")) == '86%' && /217, 119, 6|d97706/.test(await q("document.querySelector('#mt .mobbarv').style.background")), x);
  await q("(()=>{const s=document.querySelector('#mt .mobsd');s.value='20';s.dispatchEvent(new Event('change'));return 1})()"); await wait(400);
  ok("3. the month's start day (1 to 28): the 20th, sent", calls.some(c => /k=startday&v=20/.test(c)) && (await q("document.querySelectorAll('#mt .mobsd option').length")) == 28);
  await q("document.querySelector('#mt .mobdoff').click()"); await wait(400);
  ok('4. Turn mobile data off at the limit: sent', calls.some(c => /k=dataoff&v=1/.test(c)) && use.dataoff);
  use = {...use, used: 4100000000, pct: 102, off: true}; await open(); x = await t();
  ok('5. over the limit, mobile data off: said (Connect to go on: it may cost), the bar red', /mobile data is off: the limit is reached \(Connect to go on: it may cost\)/.test(x) && /220, 38, 38|dc2626/.test(await q("document.querySelector('#mt .mobbarv').style.background")), x);
  // updates on mobile data
  await q("MTR={ok:true,connected:true,if:'wwan0',kind:'usbmodem',metered:true,auto:'modem'}");
  await q("(()=>{let d=document.getElementById('ut');if(d)d.remove();d=document.createElement('div');d.id='ut';document.body.appendChild(d);updPanel(d);return 1})()"); await wait(1500);
  const btn = await q("document.querySelector('#ut .udl')?.textContent||''");
  ok('6. an update on mobile data: offered with its size ("Download over mobile data: up to 42 MB")', btn == 'Download over mobile data: up to 42 MB', btn);
  ok('...on a metered Wi-Fi: said too; on a plain connection: only the changes', await q("MTR={connected:true,metered:true,auto:'dhcp'};sysuDlLabel({size:42000000})") == 'Download on this metered connection: up to 42 MB' && await q("MTR={connected:true,metered:false};sysuDlLabel({size:42000000})") == 'Download (only the changes)');
  await q("MTR={ok:true,connected:true,if:'wwan0',kind:'usbmodem',metered:true,auto:'modem'};window.__nt=[];const on=notify;notify=function(t,b){__nt.push(t+' | '+b);return on.apply(this,arguments)};SYSU=null;netLive=()=>true");
  await q("(async()=>{try{sysuTick.metered=0}catch(e){}await sysuTick();return 1})()"); await wait(1500);
  const nt = await q("__nt.join('\\n')");
  ok('7. found by itself on mobile data: not downloaded, the notice says why and the size', dl == 0 && /was not downloaded: this connection is metered \(mobile data\)\. Download over mobile data: up to 42 MB/.test(nt), nt);
  ok('8. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
