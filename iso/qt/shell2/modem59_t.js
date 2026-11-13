// 1.5.9 (plan Part 1): Settings › Network › Cellular modems (read only). A stand-in rudolfd.   node modem59_t.js [CHROME]
// everything; a call holds off the screen saver, the lock and sleep. A stand-in rudolfd.   node call59_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
let modems = [{id: 0, maker: 'Quectel', model: 'EG25', firmware: 'EG25GGBR07A08M2G', driver: 'qmi_wwan,option', state: 'registered', failed: '--', locked: '--', sim: true, operator: 'T-Mobile', signal: '74', tech: 'lte', imei: '861234…'}];
const api = (a) => { if (a == 'mobile/status') return {ok: true, running: modems.length > 0, modems}; return {ok: true} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/a/')) { const f = path.join(WWW, u); if (fs.existsSync(f)) { s.writeHead(200); return s.end(fs.readFileSync(f)) } }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs, q.method))) }
    s.writeHead(404); s.end() }) }).listen(8277, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8277/'); await p.waitForFunction(() => typeof mdmLine == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x'}];USER=ACCTS[0];const d=document.createElement('div');d.id='nt';document.body.appendChild(d);netPanelS(d);return 1})()`); await wait(1500);
  const t = await q("document.querySelector('#nt .mdm59')?.textContent||''");
  ok('1. Settings › Network: the modem, its operator, network type, signal, firmware and drivers', /Cellular modems/.test(t) && /Quectel EG25/.test(t) && /On the network · T-Mobile · LTE · signal 74%/.test(t) && /firmware EG25GGBR07A08M2G/.test(t) && /qmi_wwan,option/.test(t), t);
  ok('2. ...and that connecting with a SIM comes in 1.6.0', /comes in Rudolf OS 1\.6\.0/.test(t));
  ok('3. no SIM, and a locked SIM, said so', await q("mdmLine({state:'failed',failed:'sim-missing',sim:false}).includes('no SIM card')&&mdmLine({state:'locked',sim:true,locked:'sim-pin'}).includes('needs its PIN (sim-pin)')"));
  modems = []; await q("(()=>{const d=document.getElementById('nt');d.innerHTML='';netPanelS(d);return 1})()"); await wait(1200);
  ok('4. no modem: nothing shown', !(await q("document.querySelector('#nt .mdm59')?.textContent||''")));
  ok('5. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
