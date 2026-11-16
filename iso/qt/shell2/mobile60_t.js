// 1.6.0 (plan Part 2, stage A item 2): Settings › Mobile network: the SIM's PIN (an administrator's name and password
// sent for rudolfd to check; the tries left; a wrong PIN said), the PUK with a new PIN (its warning; the two new PINs
// compared), a SIM blocked for good; the APN found by itself or typed in exactly (case kept), roaming off by default,
// the network type; no modem; quick settings' Airplane mode. A stand-in rudolfd.   node mobile60_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www'); const PORT = 8512;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const base = {id: 0, maker: 'Quectel', model: 'EG25-G', firmware: '', driver: 'qmi_wwan', state: 'locked', failed: '', locked: 'sim-pin', retries: {pin: 3, puk: 10}, sim: true, simOp: '310260', simName: 'T-Mobile', operator: '', reg: 'idle', signal: '0', tech: ''};
let M = [{...base}], calls = [], air = false, set = {apn: '', apnuser: '', apnpass: false, roaming: false, mode: 'auto', modes: ['auto', '4g', '3g']};
const api = (a, qs, m, body) => { const p = new URLSearchParams(qs); let b = {}; try { b = JSON.parse(body || '{}') } catch (e) {}
  if (a == 'mobile/status') return {ok: true, running: M.length > 0, modems: M, notes: []};
  if (a == 'mobile/airplane') { if (m == 'POST') { calls.push({a, qs}); air = p.get('on') == '1' } return {ok: true, on: air} }
  if (a == 'mobile/pin' || a == 'mobile/puk') { calls.push({a, qs, b}); if (b.n != 'Ann' || b.p != 'secret') return [403, {ok: false, error: 'Wrong password.'}];
    const x = M[0]; if (a == 'mobile/pin') { if (b.pin != '1234') { x.retries.pin--; return [400, {ok: false, error: 'Wrong PIN. ' + x.retries.pin + ' tries left.'}] } x.locked = ''; x.state = 'registered'; x.operator = 'T-Mobile'; x.reg = 'home'; return {ok: true, message: 'PIN accepted: the SIM is unlocked.'} }
    if (b.puk != '12345678') { x.retries.puk--; return [400, {ok: false, error: 'Wrong PUK. ' + x.retries.puk + ' tries left before the SIM is blocked for good.'}] }
    x.locked = ''; x.state = 'registered'; return {ok: true, message: 'PUK accepted: the SIM is unlocked, with the new PIN.'} }
  if (a == 'mobile/settings') { if (m == 'POST') { calls.push({a, qs}); const k = p.get('k'), v = p.get('v'); if (k == 'apnpass') set.apnpass = !!v; else if (k == 'roaming') set.roaming = v == '1'; else set[k] = v; return {ok: true, message: 'saved'} }
    return {ok: true, sim: true, simOp: '310260', simName: 'T-Mobile', auto: {name: 'T-Mobile', apn: 'fast.t-mobile.com', user: '', source: 'database'}, ...set} }
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
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 1000}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto(`http://127.0.0.1:${PORT}/`); await p.waitForFunction(() => typeof mobPanelS == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x'},{n:'Kid',r:'user',h:'y'}];USER=ACCTS[1];pwCheck=async(a,pw)=>a.n=='Ann'&&pw=='secret';
    window.__nt=[];const on=notify;notify=function(t,b){__nt.push(t+' | '+b);return on.apply(this,arguments)};return 1})()`);
  const open = async () => { await q("(()=>{let d=document.getElementById('mt');if(d)d.remove();d=document.createElement('div');d.id='mt';document.body.appendChild(d);mobPanelS(d);return 1})()"); await wait(1200) };
  const t = () => q("document.getElementById('mt').textContent");
  const allow = async pw => { await wait(300); await p.type('.rmodal .ep', pw); await q("document.querySelector('.rmodal .ey').click()"); await wait(1200) };
  await open(); let x = await t();
  ok('1. Settings › Mobile network: the modem and its SIM locked with its PIN, the tries left, an administrator needed', /Quectel EG25-G/.test(x) && /locked with its PIN/.test(x) && /3 tries left/.test(x) && /administrator's password/.test(x) && await q("!!document.querySelector('#mt .mobpinb')"), x);
  await p.type('#mt .mobpin', '12'); await q("document.querySelector('#mt .mobpinb').click()"); await wait(300);
  ok('2. a PIN that is not 4 to 8 digits: said, nothing asked', (await q("document.querySelector('#mt .mobmsg').textContent")) == 'A PIN is 4 to 8 digits.' && !(await q("!!document.querySelector('.rmodal')")) && calls.length == 0);
  await p.fill('#mt .mobpin', ''); await p.type('#mt .mobpin', '1111'); await q("document.querySelector('#mt .mobpinb').click()");
  const why = (await wait(300), await q("document.querySelector('.rmodal .rmr')?.textContent||''")); await allow('secret');
  const c0 = calls[0] || {};
  ok("3. ...a wrong one: the administrator's prompt says what for; rudolfd asked with the PIN and the administrator; its answer shown", /Unlock the SIM card with its PIN/.test(why) && c0.a == 'mobile/pin' && /id=0/.test(c0.qs) && c0.b.pin == '1111' && c0.b.n == 'Ann' && (await q("document.querySelector('#mt .mobmsg').textContent")) == 'Wrong PIN. 2 tries left.', [why, calls]);
  await wait(1500); await p.fill('#mt .mobpin', ''); await p.type('#mt .mobpin', '1234'); await q("document.querySelector('#mt .mobpinb').click()"); await allow('secret'); await wait(1500); x = await t();
  ok('4. the right PIN: told, and the page shows the SIM unlocked with its operator, and its settings', (await q("__nt.join('\\n')")).includes('PIN accepted') && /SIM card: T-Mobile \(310260\)/.test(x) && /The operator's settings \(APN\)/.test(x), x);
  ok("5. the APN found by itself: T-Mobile's, from the operators' database, chosen", /Automatic: fast\.t-mobile\.com \(T-Mobile, from the operators' database\)/.test(x) && await q("document.querySelector('#mt .mobauto').checked&&document.querySelector('#mt .mobcf').style.display=='none'"), x);
  ok('6. roaming off by default, with what it may cost; the network types this modem has', await q("!document.querySelector('#mt .mobroam').checked") && /can cost much more/.test(x) && (await q("[...document.querySelectorAll('#mt .mobmode option')].map(o=>o.value).join()")) == 'auto,4g,3g', x);
  await q("document.querySelector('#mt .mobcust').click()"); await wait(200);
  ok('7. typing it in: the fields shown, with the warning about capitals', await q("document.querySelector('#mt .mobcf').style.display==''") && /some networks refuse it in other capitals/.test(await t()));
  calls = []; await p.type('#mt .mobapn', 'bad apn'); await q("document.querySelector('#mt .mobsave').click()"); await wait(400);
  ok('...an APN with a space: refused here, nothing sent', calls.length == 0 && (await q("__nt.join('\\n')")).includes('letters, digits, dots and hyphens'));
  await p.fill('#mt .mobapn', 'Wholesale'); await p.type('#mt .mobpass', 'pw'); await q("document.querySelector('#mt .mobsave').click()"); await wait(800);
  const sv = calls.filter(c => c.a == 'mobile/settings').map(c => c.qs);
  ok('8. saved exactly as typed (Wholesale, its capital kept), with the user name and the password', sv.some(s => /k=apn&v=Wholesale$/.test(s)) && sv.some(s => /k=apnuser/.test(s)) && sv.some(s => /k=apnpass&v=pw/.test(s)) && set.apn == 'Wholesale', sv);
  await q("(()=>{const c=document.querySelector('#mt .mobroam');c.click();return 1})()"); await wait(500);
  ok('9. roaming turned on: sent', calls.some(c => /k=roaming&v=1/.test(c.qs)) && set.roaming, calls);
  await q("(()=>{const s=document.querySelector('#mt .mobmode');s.value='4g';s.dispatchEvent(new Event('change'));return 1})()"); await wait(500);
  ok('10. the network type: 4G only, sent', calls.some(c => /k=mode&v=4g/.test(c.qs)) && set.mode == '4g', calls);
  await open();
  ok('...the page opened again shows what was kept: typed in, roaming on, 4G', await q("document.querySelector('#mt .mobcust').checked&&document.querySelector('#mt .mobapn').value=='Wholesale'&&document.querySelector('#mt .mobroam').checked&&document.querySelector('#mt .mobmode').value=='4g'&&document.querySelector('#mt .mobpass').placeholder.includes('kept')"));
  // the PUK
  M[0] = {...base, locked: 'sim-puk', retries: {pin: 0, puk: 10}}; calls = []; await open(); x = await t();
  ok('11. three wrong PINs: the PUK and a new PIN asked, with the warning that ten wrong PUKs block the SIM for good', /blocked/.test(x) && /PUK \(8 digits/.test(x) && /ten wrong PUKs block the SIM for good/.test(x) && /10 tries left/.test(x) && await q("!!document.querySelector('#mt .mobpukb')"), x);
  await p.type('#mt .mobpuk', '12345678'); await p.type('#mt .mobnp1', '4321'); await p.type('#mt .mobnp2', '4322'); await q("document.querySelector('#mt .mobpukb').click()"); await wait(300);
  ok('...the two new PINs not the same: said, nothing sent', (await q("document.querySelector('#mt .mobmsg').textContent")) == 'The two new PINs are not the same.' && calls.length == 0);
  await p.fill('#mt .mobnp2', '4321'); await q("document.querySelector('#mt .mobpukb').click()"); await allow('secret');
  const c1 = calls[0] || {};
  ok('12. the PUK and the new PIN sent with the administrator; unblocked', c1.a == 'mobile/puk' && c1.b.puk == '12345678' && c1.b.pin == '4321' && (await q("__nt.join('\\n')")).includes('PUK accepted'), calls);
  M[0] = {...base, locked: '', state: 'failed', failed: 'sim-error'}; await open(); x = await t();
  ok('13. a SIM blocked for good: said, and what to do (a new SIM from the operator); no settings', /blocked for good/.test(x) && /ask them for a new SIM/.test(x) && !/APN/.test(x), x);
  M[0] = {...base, sim: false, locked: '', state: 'failed', failed: 'sim-missing'}; await open(); x = await t();
  ok('14. no SIM: said', /No SIM card/.test(x) && !/APN/.test(x), x);
  M = []; await open(); x = await t();
  ok('15. no modem: said, with the other ways to mobile data', /No cellular modem is connected/.test(x) && /hotspot or a phone/.test(x), x);
  // airplane mode in quick settings
  calls = []; await q("qsToggle()"); await wait(800);
  const tiles = await q("[...document.querySelectorAll('#qsp .qst span')].map(s=>s.textContent)");
  ok('16. quick settings: Airplane mode, the ninth switch, off', tiles.length == 9 && tiles[8] == 'Airplane mode' && await q("!document.querySelector('#qsp .qair').classList.contains('on')"), tiles);
  await q("document.querySelector('#qsp .qair').click()"); await wait(800);
  ok('...turned on: rudolfd asked, the switch on', calls.some(c => c.a == 'mobile/airplane' && /on=1/.test(c.qs)) && air && await q("document.querySelector('#qsp .qair').classList.contains('on')"), calls);
  await q("qsClose()"); M = [{...base, locked: '', state: 'disabled'}]; await open(); x = await t();
  ok('17. Settings › Mobile network says airplane mode is on (the modem off), and where to turn it off', /Airplane mode is on/.test(x) && /quick settings/.test(x), x);
  await q("qsToggle()"); await wait(800);
  ok('...quick settings opened again: the switch read back on', await q("document.querySelector('#qsp .qair').classList.contains('on')"));
  await q("document.querySelector('#qsp .qair').click()"); await wait(800);
  ok('...turned off', calls.some(c => c.a == 'mobile/airplane' && /on=0/.test(c.qs)) && !air && await q("!document.querySelector('#qsp .qair').classList.contains('on')"), calls);
  ok('18. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
