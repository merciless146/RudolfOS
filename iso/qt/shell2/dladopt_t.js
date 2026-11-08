// 1.5.5 (dl27.js, rudolfd dl/home): files in Downloads on the disk that no C:\DOWNLOADS entry points at (saved by a
// Linux app, or an Oracle download whose entry was lost) show up in File Explorer's Downloads, after the antivirus;
// not while a download is being taken in, not before they were seen twice, and never one whose entry was deleted
// (waiting to be deleted from the disk). A stand-in rudolfd.   node dladopt_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
const HOME = new Map([['report.pdf', '%PDF-1.4 a report'], ['photo from app.png', 'PNGDATA']]), CALLS = [];
const api = (a, q) => { const p = new URLSearchParams(q || ''); CALLS.push(a + '?' + (q || ''));
  if (a == 'dl/home') return {ok: true, items: [...HOME].map(([name, v]) => ({name, size: v.length, mtime: 1, partial: false})).concat([{name: 'big.iso.crdownload', size: 9, mtime: 1, partial: true}])};
  if (a == 'dl/incoming' || a == 'dl/quarantine') return {ok: true, items: []};
  if (a == 'dl/file') { const n = p.get('name'), to = p.get('to');
    if (to == 'quar') { HOME.delete(n); return {ok: true, name: n, path: '/home/rudolf/Downloads/.quarantine/' + n} }
    if (to == 'del') { HOME.delete(n); return {ok: true} } }
  if (a == 'dl/space') return {ok: true, free: 50e9, total: 100e9};
  if (a == 'events/wait') return {ok: true, events: ''};
  return {ok: true} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":""}') }, 20000);
    if (u == '/api/dl/read') { const p = new URLSearchParams(qs), b = Buffer.from(HOME.get(p.get('name')) || ''); s.writeHead(200, {'content-type': 'application/octet-stream'}); return s.end(b) }
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs))) }
    s.writeHead(404); s.end() }) }).listen(8094, '127.0.0.1');
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8094/'); await p.waitForFunction(() => typeof dlAdopt == 'function' && typeof explorer == 'function', null, {timeout: 30000});
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});USER=USER||ACCTS[0];window.__sys=window.__sys||{};SECC.av.on=true;SECC.av.act='quarantine';clearTimeout(DLN.t);DLN.t=setTimeout(()=>{},1e9);return 1})()");
  const entries = () => q("Object.keys(dlCDir()).sort()");
  // a file whose entry was deleted: waiting to be deleted from the disk, never taken back in
  HOME.set('deleted.txt', 'gone'); await q("(()=>{const k=ld(DLRK,{});k['/home/rudolf/Downloads/deleted.txt']={w:'fs',u:'',miss:0};svj(DLRK,k);return 1})()");
  await q("dlReconcile()");
  ok('1. a file seen once without an entry is not taken in yet', !(await entries()).includes('REPORT.PDF'), await entries());
  await q("(()=>{DLN.ing=1;return dlReconcile()})()");
  ok('2. ...nor while a download is being taken in', !(await entries()).includes('REPORT.PDF'), await entries());
  await q("(()=>{DLN.ing=0;return dlReconcile()})()"); await q("dlReconcile()");
  const e = await entries();
  ok('3. seen again: the files in Downloads are in C:\\DOWNLOADS', e.includes('REPORT.PDF') && e.includes('PHOTO FROM APP.PNG'), e);
  const ref = await q("dlRefInfo(dlCDir()['REPORT.PDF'])");
  ok('4. ...as entries for the file on the disk, checked by the antivirus', ref && ref.p == '/home/rudolf/Downloads/report.pdf' && ref.av == 'full', ref);
  ok('5. a file being downloaded (.crdownload) is not taken in', !e.some(x => /CRDOWNLOAD/.test(x)), e);
  // the deleted one: the reconcile deletes it from the disk (its entry was gone twice), never taken back
  await q("dlReconcile()"); await q("dlReconcile()");
  ok('6. a file whose entry was deleted is not taken back in (it is deleted from the disk instead)', !(await entries()).includes('DELETED.TXT') && CALLS.some(c => /dl\/file\?name=deleted\.txt&from=home&to=del/.test(c)), CALLS.filter(c => /deleted/.test(c)));
  const n1 = (await entries()).length; await q("dlReconcile()"); await q("dlReconcile()");
  ok('7. taken in once (no copies on later passes)', (await entries()).length == n1, await entries());
  // a file with the antivirus test string: quarantined, not listed
  HOME.set('eicar.com.txt', 'X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*');
  await q("dlReconcile()"); await q("dlReconcile()");
  ok('8. a file the antivirus finds is quarantined, not listed', !(await entries()).includes('EICAR.COM.TXT') && CALLS.some(c => /name=eicar\.com\.txt&from=home&to=quar/.test(c)) && await q("QUAR.some(x=>x.n=='EICAR.COM.TXT')"), await entries());
  // File Explorer's Downloads shows them
  await q("explorer(downloadsPath())"); await new Promise(r => setTimeout(r, 800));
  const rows = await q("[...document.querySelectorAll('.win [data-k]')].map(x=>x.textContent).join('|')");
  ok("9. File Explorer's Downloads lists them", /REPORT\.PDF/.test(rows) && /PHOTO FROM APP\.PNG/.test(rows), rows.slice(0, 300));
  ok('10. no page errors', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1)
})().catch(e => { console.log('FAIL ' + e.stack); process.exit(1) });
