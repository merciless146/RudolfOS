// 1.6.1 (docs/plan-1.6.1.md Part 9): Settings › Devices › Print queue (print/jobs through rudolfd: rudolf-printq).
// The account's own jobs with their names, state and Cancel; another account's as "Another account's document", with no
// Cancel; a failed one says why; Cancel asks print/cancel and draws the queue again; looked at again while a job
// waits; no job, and no queue, said; the print dialog sends the printer's name with the job and says it is queued.
//   node printq61_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const now = Math.floor(Date.now() / 1000);
const M = {asked: 0, cancel: [], job: null, queue: true, jobs: [
  {id: 7, state: 'printing', mine: true, name: 'Budget <i>2027</i>', printer: 'Office printer', user: 'acct1001', message: '', made: now, done: 0, cancel: true},
  {id: 6, state: 'waiting', mine: false, name: '', printer: 'Office printer', user: '', message: '', made: now, done: 0, cancel: false},
  {id: 5, state: 'failed', mine: true, name: 'Letter', printer: 'Hall printer', user: 'acct1001', message: 'The printer does not answer.', made: now - 60, done: now - 50, cancel: false},
  {id: 4, state: 'sent', mine: true, name: 'Photo', printer: 'USB: HP OfficeJet', user: 'acct1001', message: 'Sent to the printer.', made: now - 90, done: now - 80, cancel: false}]};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0); const b = [];
  q.on('data', d => b.push(d)); q.on('end', async () => {
    const J = (o, c) => { s.writeHead(c || 200, {'content-type': 'application/json'}); s.end(JSON.stringify(o)) };
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/events/wait') { await new Promise(r => setTimeout(r, 800)); s.writeHead(200); return s.end('{"ok":true,"events":""}') }
    if (u == '/api/devices') return J({ok: true, drives: [], usb: [], inputs: []});
    if (u == '/api/print/usb') return J({ok: true, printers: []});
    if (u == '/api/print/jobs') { M.asked++; M.method = q.method; return M.queue ? J({ok: true, printers: [], jobs: M.jobs}) : J({ok: true, queue: false, printers: [], jobs: []}) }
    if (u == '/api/print/cancel') { const id = +new URLSearchParams(qs).get('id'); M.cancel.push(id); const j = M.jobs.find(x => x.id == id); if (j) { j.state = 'canceled'; j.cancel = false } return J({ok: true, message: 'Canceled.'}) }
    if (u == '/api/print/find') return J({ok: true, printers: [{name: 'Office printer', uri: 'ipp://192.168.1.20/ipp/print', model: 'X', location: ''}]});
    if (u == '/api/print/info') return J({ok: true, uri: new URLSearchParams(qs).get('uri'), name: 'Office printer', state: 3, formats: ['application/pdf'], sides: ['one-sided'], colorModes: ['color', 'monochrome'], color: true});
    if (u == '/api/print/job') { M.job = Object.fromEntries(new URLSearchParams(qs)); return J({ok: true, job: 8, queued: true, state: 'waiting', message: 'In the print queue (Settings › Devices).'}) }
    if (u == '/api/print/pdf') { s.writeHead(200, {'content-type': 'application/pdf'}); return s.end(fs.readFileSync(path.join(__dirname, 'printq61_t.pdf'))) }
    if (u.startsWith('/api/')) return J({ok: true});
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8747, '127.0.0.1');
// (a one-page PDF for the print dialog's preview)
fs.writeFileSync(path.join(__dirname, 'printq61_t.pdf'), '%PDF-1.4\n1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj\n2 0 obj << /Type /Pages /Kids [3 0 R] /Count 1 >> endobj\n3 0 obj << /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >> endobj\ntrailer << /Root 1 0 R >>\n%%EOF\n');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  const p = await b.newPage({viewport: {width: 1280, height: 900}}); const errs = []; p.on('pageerror', e => errs.push(String(e.stack || e).slice(0, 300)));
  await p.goto('http://127.0.0.1:8747/'); await p.waitForFunction(() => typeof pqSection == 'function' && typeof settings == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});USER=USER||ACCTS[0];return 1})()");
  const open = async () => { await q("(()=>{wins.slice().forEach(w=>w.close());settings('devs');return 1})()"); await wait(1400) };
  await open();
  const rows = () => q("[...document.querySelectorAll('.pq61 tr')].map(r=>[...r.cells].map(c=>c.textContent))");
  let r = await rows();
  ok('1. Devices › Print queue: every job, newest first, after the USB printers', r.length == 4 && await q("(()=>{const x=document.querySelector('.pq61');return !!x&&!!x.previousElementSibling&&x.previousElementSibling.classList.contains('up60')&&x.querySelector('h4').textContent=='Print queue'})()"), r);
  ok('...the account\'s own job: its name (as text), printer, state, and Cancel', /^Budget <i>2027<\/i>Office printer · Printing/.test(r[0][1]) && r[0][2] == 'Cancel' && !(await q("!!document.querySelector('.pq61 b i')")), r[0]);
  ok('...another account\'s: "Another account\'s document", no Cancel', /^Another account's documentOffice printer · Waiting/.test(r[1][1]) && !r[1][2], r[1]);
  ok('...a failed one says why', /Hall printer · Not printed: The printer does not answer\./.test(r[2][1]), r[2]);
  ok('...a sent one', /USB: HP OfficeJet · Sent to the printer/.test(r[3][1]) && !r[3][2], r[3]);
  ok('...asked with POST, as the other print requests', M.method == 'POST', M.method);
  const n0 = M.asked; await wait(3600);
  ok('2. looked at again while a job waits or prints', M.asked > n0, [n0, M.asked]);
  await q("document.querySelector('.pq61 .pqx').click()"); await wait(900); r = await rows();
  ok('3. Cancel asks print/cancel for that job, and the queue is drawn again', M.cancel.join() == '7' && /Canceled/.test(r[0][1]) && !r[0][2], [M.cancel, r[0]]);
  M.jobs.forEach(j => { if (j.state == 'waiting') j.state = 'sent' }); await wait(3500); const n1 = M.asked; await wait(3500);
  ok('...and no longer once nothing waits or prints', M.asked == n1, [n1, M.asked]);
  M.jobs = []; await open();
  ok('4. no job: said', /Nothing is printing/.test(await q("document.querySelector('.pq61').textContent")));
  M.queue = false; await open();
  ok('...no queue on this computer: said', /not running on this computer/.test(await q("document.querySelector('.pq61').textContent")));
  // the print dialog: the printer's name goes with the job; the notification says it is in the queue
  await q("(()=>{wins.slice().forEach(w=>w.close());prDialog(null,null,{pdf:new Blob([new Uint8Array([37,80,68,70])],{type:'application/pdf'}),title:'Notes'});return 1})()"); await wait(1500);
  await q("(async()=>{const s=document.querySelector('.prdlg .pdst');PR_FINDING=null;await prFind(true);})()"); await wait(800);
  await q("(()=>{const s=document.querySelector('.prdlg .pdst');s.value='ipp://192.168.1.20/ipp/print';s.onchange();return 1})()"); await wait(1200);
  await q("(()=>{window.__nt=[];const o=notify;notify=function(t,b){__nt.push(t+' '+b);return o.apply(this,arguments)};return 1})()");
  await q("document.querySelector('.prdlg .pgo').click()"); await wait(1500);
  ok('5. the print dialog sends the printer\'s name with the job', M.job && M.job.name == 'Office printer' && M.job.uri == 'ipp://192.168.1.20/ipp/print', M.job);
  ok('...and says where the job is', await q("__nt.some(t=>/^Printing Notes → Office printer \\(job 8\\)\\. In the print queue/.test(t))"), await q('__nt'));
  ok('6. no page errors', !errs.length, errs);
  await b.close(); srv.close(); fs.unlinkSync(path.join(__dirname, 'printq61_t.pdf'));
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
