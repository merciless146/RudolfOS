// 2026-11-15 (docs/plan-1.6.0.md Part 8): Settings › Devices › Printers plugged in by USB (GET print/usb): each one's
// name (as text) and kind: IPP over USB ready or needing the USB printers extension (a button to System extensions),
// PDF, PostScript ready or not, needs a driver; none plugged in; Refresh and a USB device plugged in read it again; the
// print dialog lists them as "USB: name" (from print/find) and says when one needs a driver.
//   node usbprint60_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const M = {asked: 0, printers: [
  {name: 'HP OfficeJet Pro 9010', uri: 'ipp://127.0.0.1:60000/ipp/print', model: 'OfficeJet Pro 9010', usb: 'ipp', kind: 'IPP over USB', ready: true},
  {name: 'Brother <b>HL</b>', uri: 'usb://lp0', model: 'HL-L5100DN', usb: 'postscript', kind: 'PostScript', ready: false},
  {name: 'HP LaserJet 1020', uri: 'usb://lp1', model: 'HP LaserJet 1020', usb: 'driver', kind: 'needs a driver', ready: false},
  {name: 'Canon G3010', uri: 'usb://ipp/1-4', model: 'G3010', usb: 'ipp', kind: 'IPP over USB (needs the USB printers extension)', ready: false}]};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', async () => {
    const J = (o, c) => { s.writeHead(c || 200, {'content-type': 'application/json'}); s.end(JSON.stringify(o)) };
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/events/wait') { await new Promise(r => setTimeout(r, 800)); s.writeHead(200); return s.end('{"ok":true,"events":""}') }
    if (u == '/api/devices') return J({ok: true, drives: [], usb: [], inputs: []});
    if (u == '/api/print/usb') { M.asked++; return J({ok: true, printers: M.printers}) }
    if (u == '/api/print/find') return J({ok: true, printers: M.printers.map(p => ({name: 'USB: ' + p.name, uri: p.uri, model: p.model, location: 'USB', usb: p.usb, kind: p.kind}))});
    if (u == '/api/print/info') { const uri = new URLSearchParams(qs).get('uri'); if (uri == 'usb://lp1') return J({ok: true, uri, name: 'HP LaserJet 1020', model: 'HP LaserJet 1020', location: 'USB', state: 3, message: 'It needs a driver Rudolf OS does not have.', formats: []}); return J({ok: true, uri, name: 'x', state: 3, formats: ['application/pdf']}) }
    if (u == '/api/sys/status') return J({ok: true, persistent: true, booted: 'a', slots: {}, current: {version: '1.6.0'}, state: {good: 'a'}, url: 'https://x/stable', auto: true, checked: 0});
    if (u.startsWith('/api/')) return J({ok: true});
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8746, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  const p = await b.newPage({viewport: {width: 1280, height: 900}}); const errs = []; p.on('pageerror', e => errs.push(String(e.stack || e).slice(0, 300)));
  await p.goto('http://127.0.0.1:8746/'); await p.waitForFunction(() => typeof upSection == 'function' && typeof settings == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});USER=USER||ACCTS[0];return 1})()");
  const open = async () => { await q("(()=>{wins.slice().forEach(w=>w.close());settings('devs');return 1})()"); await wait(1200) };
  await open();
  const rows = await q("[...document.querySelectorAll('.up60 tr')].map(r=>[...r.cells].map(c=>c.textContent))");
  ok('1. Devices › Printers plugged in by USB: each one with its kind', rows.length == 4 && rows[0][1] == 'HP OfficeJet Pro 9010IPP over USB · ready to print · OfficeJet Pro 9010', rows);
  ok('...after the devices\' list', await q("(()=>{const x=document.querySelector('.up60');return !!x&&x.previousElementSibling&&x.previousElementSibling.classList.contains('dvx')})()"));
  ok('...a name shown as text', /Brother <b>HL<\/b>/.test(rows[1][1]) && !(await q("!!document.querySelector('.up60 b b')")), rows[1]);
  ok('...PostScript without the extension: says so, with System extensions…', /Reads PostScript · needs the USB printers extension/.test(rows[1][1]) && rows[1][2] == 'System extensions…', rows[1]);
  ok('...one that needs a driver: says it cannot print yet', /Needs a driver Rudolf OS does not have .*cannot print yet/.test(rows[2][1]) && !rows[2][2], rows[2]);
  ok('...IPP over USB without the extension: says so, with System extensions…', /IPP over USB · needs the USB printers extension/.test(rows[3][1]) && rows[3][2] == 'System extensions…', rows[3]);
  await q("document.querySelectorAll('.up60 .upx')[0].click()"); await wait(900);
  ok('2. System extensions… opens Update and recovery', await q("!!document.querySelector('.sx60')||[...document.querySelectorAll('h3')].some(h=>h.textContent=='Update and recovery')"));
  await open(); const n0 = M.asked; await q("document.querySelector('.dvr').click()"); await wait(700);
  ok('3. Refresh reads them again', M.asked > n0, [n0, M.asked]);
  const n1 = M.asked; await q("dispatchEvent(new Event('rudolf-usb'))"); await wait(2200);
  ok('...and so does a USB device plugged in', M.asked > n1, [n1, M.asked]);
  M.printers = []; await open();
  ok('4. none plugged in: said', /No USB printer is plugged in/.test(await q("document.querySelector('.up60').textContent")));
  M.printers = [{name: 'HP LaserJet 1020', uri: 'usb://lp1', model: 'HP LaserJet 1020', usb: 'driver', kind: 'needs a driver', ready: false}];
  // the print dialog (WordPro's Print, through printDialog's own list): "USB: name", and needs a driver said
  const has = await q("typeof prFind=='function'");
  if (has) { await q("PR_FINDING=null;PR_FOUND=null;prFind(true)"); await wait(600);
    ok('5. the print dialog\'s list has them as "USB: name"', await q("(PR_FOUND||[]).some(p=>p.name=='USB: HP LaserJet 1020'&&p.uri=='usb://lp1')"), await q('PR_FOUND'));
    const plan = await q("(async()=>{const i=await prJSON('print/info?uri='+encodeURIComponent('usb://lp1'));return prPlan(i,true)})()");
    ok('...one that needs a driver: nothing the dialog can send (it says the printer needs a driver)', plan === null, plan) }
  else ok('5. the print dialog is in the page', false);
  ok('6. no page errors', !errs.length, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
