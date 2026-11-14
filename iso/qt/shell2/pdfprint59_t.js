// 1.5.9: the PDF Viewer prints: 🖨️ Print and Ctrl+P open Rudolf OS's print dialog with the PDF itself (its own paper),
// which goes to an IPP printer as it is (or is saved as a copy). A stand-in rudolfd with one printer.   node pdfprint59_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www'), J = {'content-type': 'application/json'};
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 400) : '')) };
const jobs = [], htmlPdf = [];
const srv = http.createServer((q, s) => { const [u, qs] = q.url.split('?'); const body = []; q.on('data', c => body.push(c)); q.on('end', () => {
  if (u == '/') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
  if (u.startsWith('/lib/') || u.startsWith('/a/')) { const f = path.join(WWW, u); if (fs.existsSync(f)) { s.writeHead(200, {'content-type': f.endsWith('.mjs') || f.endsWith('.js') ? 'text/javascript' : 'application/octet-stream'}); return s.end(fs.readFileSync(f)) } }
  if (u == '/api/print/find') { s.writeHead(200, J); return s.end(JSON.stringify({ok: true, printers: [{name: 'Office Laser', uri: 'ipp://10.0.0.9/ipp/print'}]})) }
  if (u == '/api/print/info') { s.writeHead(200, J); return s.end(JSON.stringify({ok: true, name: 'Office Laser', model: 'Laser 9', state: 'idle', formats: ['application/pdf'], color: true, sides: ['one-sided', 'two-sided-long-edge'], media: ['na_letter_8.5x11in', 'iso_a4_210x297mm']})) }
  if (u == '/api/print/job') { jobs.push({q: Object.fromEntries(new URLSearchParams(qs)), body: Buffer.concat(body)}); s.writeHead(200, J); return s.end('{"ok":true,"job":42}') }
  if (u == '/api/print/pdf') { htmlPdf.push(1); s.writeHead(500, J); return s.end('{"ok":false,"error":"not used"}') }
  if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200, J); s.end('{"ok":true,"events":[]}') }, 20000); s.writeHead(200, J); return s.end('{"ok":true}') }
  s.writeHead(404); s.end() }) }).listen(8292, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => { if (/pdf|pr[A-Z]|print/i.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8292/'); await p.waitForFunction(() => typeof pdfPrint == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x'}];USER=ACCTS[0];
    const bl=pdfFromText('Letter','Hello printer\\nSecond line');const u=await new Promise(r=>{const f=new FileReader();f.onload=()=>r(f.result);f.readAsDataURL(bl)});node(['C:'])['HELLO.PDF']=u;return 1})()`);
  await q("pdfViewer(['C:','HELLO.PDF'])"); await wait(2500);
  const W = "wins.find(w=>/^PDF Viewer/.test(w.el.querySelector('.t').textContent))";
  ok('1. the PDF Viewer has 🖨️ Print', await q(`!!${W}.body.querySelector('.prt')`));
  await q(`${W}.body.querySelector('.prt').click()`); await wait(2500);
  const d = await q("(()=>{const o=document.querySelector('.prdlg');return o?{pap:o.querySelector('.ppap').textContent,prev:o.querySelector('.prprev').textContent,dst:[...o.querySelectorAll('.pdst option')].map(x=>x.textContent)}:null})()");
  ok('2. Print: the print dialog with the PDF itself (no page made from HTML), its paper the PDF\'s own (A4)', d && /^A4/.test(d.pap) && /the PDF's own/.test(d.pap) && /1 page/.test(d.prev) && htmlPdf.length == 0, d);
  ok('3. ...the printers on the network are offered, and Save as PDF', d && d.dst.includes('Save as PDF') && d.dst.some(x => /Office Laser/.test(x)), d && d.dst);
  await q("(()=>{const o=document.querySelector('.prdlg');const s=o.querySelector('.pdst');s.value='ipp://10.0.0.9/ipp/print';s.onchange();return 1})()"); await wait(800);
  await q("document.querySelector('.prdlg .pcop').value=2;document.querySelector('.prdlg .pgo').click()"); await wait(1200);
  const j = jobs[0];
  ok('4. Print: the PDF goes to the printer as it is (application/pdf), 2 copies, A4 paper', j && j.q.format == 'application/pdf' && j.q.copies == '2' && j.q.media == 'iso_a4_210x297mm' && j.body.slice(0, 5).toString() == '%PDF-' && j.q.title == 'HELLO', j && j.q);
  ok('5. ...the dialog closes and says it is printing', await q("!document.querySelector('.prdlg')"));
  // Ctrl+P in the viewer
  await q(`(()=>{const w=${W};w.focus&&w.focus();return 1})()`); await p.keyboard.press('Control+p'); await wait(1500);
  ok('6. Ctrl+P opens it too', await q("!!document.querySelector('.prdlg')"));
  await q("document.querySelector('.prdlg .pcan').click()");
  // nothing open
  await q("pdfViewer()"); await wait(800);
  await q(`wins.filter(w=>/^PDF Viewer$/.test(w.el.querySelector('.t').textContent)).pop().body.querySelector('.prt').click()`); await wait(400);
  ok('7. with no PDF open: says so, no dialog', !(await q("!!document.querySelector('.prdlg')")) && /Open a PDF/.test(await q(`wins.filter(w=>/^PDF Viewer$/.test(w.el.querySelector('.t').textContent)).pop().body.querySelector('.stat').textContent`)));
  ok('8. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
