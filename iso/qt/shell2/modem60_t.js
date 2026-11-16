// 1.6.0 (plan Part 1, stage A item 1): Settings › Network › Cellular modems: the FCC unlock of a laptop's module (saying
// what it does; an administrator's name and password sent for rudolfd to check), turned off again; the modules Linux
// supports only partly, named (also with no modem); the SIM's PIN and PUK said apart. A stand-in rudolfd.
//   node modem60_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www'); const PORT = 8511;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
const XMM = {id: '8086:7360', name: 'Intel XMM7360 (Fibocom L850-GL)', text: 'Built into some ThinkPads and other laptops. Linux has no driver for it as it is set up there (PCIe): ModemManager cannot see it.'};
let fcc = 'off', calls = [];
let modems = () => [{id: 0, maker: 'Quectel', model: 'EM05-G', firmware: 'EM05GFAR07A07M1G', driver: 'qmi_wwan,option', state: 'disabled', failed: '', locked: '', sim: true, operator: '', signal: '0', tech: '', imei: '861234…', usb: '2c7c:030a', fcc, fccId: '2c7c:030a'}];
let notes = [XMM];
const api = (a, qs, m, body) => {
  if (a == 'mobile/status') { const ms = modems(); return {ok: true, running: ms.length > 0, modems: ms, notes} }
  if (a == 'mobile/fcc') { calls.push({m, qs, body}); const p = new URLSearchParams(qs); let b = {}; try { b = JSON.parse(body) } catch (e) {}
    if (m != 'POST' || b.n != 'Ann' || b.p != 'secret') return [403, {ok: false, error: 'an administrator\'s name and password are needed'}];
    fcc = p.get('on') == '1' ? 'on' : 'off'; return {ok: true, message: fcc == 'on' ? "The cellular module's radio is turned on (FCC unlock for 2c7c:030a)" : 'The FCC unlock is off for 2c7c:030a: the radio stays off from the next start'} }
  return {ok: true} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0); let body = '';
  q.on('data', d => body += d); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/a/')) { const f = path.join(WWW, u); if (fs.existsSync(f)) { s.writeHead(200); return s.end(fs.readFileSync(f)) } }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      let r = api(u.slice(5), qs || '', q.method, body), code = 200; if (Array.isArray(r)) [code, r] = r;
      s.writeHead(code, {'content-type': 'application/json'}); return s.end(JSON.stringify(r)) }
    s.writeHead(404); s.end() }) }).listen(PORT, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto(`http://127.0.0.1:${PORT}/`); await p.waitForFunction(() => typeof mdmLine == 'function' && typeof mdmFcc == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x'},{n:'Kid',r:'user',h:'y'}];USER=ACCTS[1];pwCheck=async(a,pw)=>a.n=='Ann'&&pw=='secret';
    window.__nt=[];const on=notify;notify=function(t,b){__nt.push(t+' | '+b);return on.apply(this,arguments)};const d=document.createElement('div');d.id='nt';document.body.appendChild(d);netPanelS(d);return 1})()`); await wait(1500);
  const t = () => q("document.querySelector('#nt .mdm59')?.textContent||''");
  let x = await t();
  ok('1. the modem whose radio is locked: the FCC unlock offered, saying what it does and for which module', /Quectel EM05-G/.test(x) && /FCC lock/.test(x) && /2c7c:030a/.test(x) && /each time it starts/.test(x) && /administrator/.test(x) && await q("!!document.querySelector('#nt .mdmfccon')"), x);
  ok('2. ...nothing says connecting comes later any more', !/comes in Rudolf OS 1\.6\.0/.test(x), x);
  await q("document.querySelector('#nt .mdmfccon').click()"); await wait(300);
  const why = await q("document.querySelector('.rmodal .rmr')?.textContent||''");
  ok("3. turning it on asks for an administrator (even signed in as a standard user), saying what will happen", /Turn on this laptop's cellular module \(2c7c:030a\)/.test(why) && /ModemManager's unlock/.test(why) && await q("[...document.querySelectorAll('.rmodal .ea option')].map(o=>o.value).join()") == 'Ann', why);
  await p.type('.rmodal .ep', 'secret'); await q("document.querySelector('.rmodal .ey').click()"); await wait(1500);
  const c = calls[calls.length - 1] || {};
  ok("4. ...rudolfd is asked (POST mobile/fcc, the module's id, on) with the administrator's name and password to check", c.m == 'POST' && /id=2c7c%3A030a|id=2c7c:030a/.test(c.qs) && /on=1/.test(c.qs) && JSON.parse(c.body || '{}').n == 'Ann', calls);
  x = await t();
  ok('5. ...the answer told, and the module shown with the unlock on, and a way to turn it off', (await q("__nt.join('\\n')")).includes("radio is turned on") && /unlocked each time the module starts/.test(x) && await q("!!document.querySelector('#nt .mdmfccoff')"), x);
  await q("document.querySelector('#nt .mdmfccoff').click()"); await wait(300); await p.type('.rmodal .ep', 'wrong'); await q("document.querySelector('.rmodal .ey').click()"); await wait(400);
  ok('6. a wrong password: the prompt says so and nothing is asked of rudolfd', (await q("document.querySelector('.rmodal .rmmsg')?.textContent||''")) == 'Wrong password.' && calls.length == 1);
  await q("document.querySelector('.rmodal .en').click()"); await wait(200);
  await q("document.querySelector('#nt .mdmfccoff').click()"); await wait(300); await p.type('.rmodal .ep', 'secret'); await q("document.querySelector('.rmodal .ey').click()"); await wait(1500);
  ok('7. turned off with the administrator: the offer is back', calls.length == 2 && /on=0/.test(calls[1].qs) && await q("!!document.querySelector('#nt .mdmfccon')"), calls);
  x = await t();
  ok('8. the modules Linux supports only partly: named, with what is known', /Cellular modules Linux supports only partly/.test(x) && /Intel XMM7360 \(Fibocom L850-GL\) \(8086:7360\)/.test(x) && /no driver/.test(x), x);
  modems = () => []; await q("(()=>{const d=document.getElementById('nt');d.innerHTML='';netPanelS(d);return 1})()"); await wait(1200); x = await t();
  ok('9. ...also when ModemManager sees no modem (the module that fails silently otherwise)', !/Cellular modems/.test(x) && /Intel XMM7360/.test(x), x);
  notes = []; await q("(()=>{const d=document.getElementById('nt');d.innerHTML='';netPanelS(d);return 1})()"); await wait(1200);
  ok('10. no modem and no such module: nothing shown', !(await t()));
  ok('11. a locked SIM: the PIN or the PUK said apart, no signal shown while locked or when there is none', await q("mdmLine({state:'locked',sim:true,locked:'sim-puk',signal:'0'})") == 'SIM locked · the SIM needs its PUK (sim-puk)' && (await q("mdmLine({state:'locked',sim:true,locked:'sim-pin'})")).includes('needs its PIN (sim-pin)') && await q("mdmLine({state:'disabled',sim:true,signal:'0'})") == 'Off');
  ok('12. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
