// 1.5.5 (dl27.js, core25.js, rudolfd dl/*): Downloads for each account. A download goes to the account signed in,
// into its own C:\USERS\<name>\DOWNLOADS, its large file into the account's folder on the disk
// (/home/rudolf/Downloads/.accounts/<id>/); while nobody is signed in it waits; another account does not see it; the
// guest's folder is cleared at sign-out. C:\DOWNLOADS from before 1.5.5 moves to its owner (one account) or stays
// readable by all as C:\SHARED DOWNLOADS (BEFORE 1.5.5) (several), nothing deleted. Files in the account's folder that
// lost their entry come back (#115). A stand-in rudolfd with a disk in memory.
//   node dl155_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const DL = '/home/rudolf/Downloads', DISK = new Map(), CALLS = [];   // full path -> content
const dirOf = (from, acct) => from == 'incoming' ? DL + '/.incoming' : from == 'quarantine' ? DL + '/.quarantine' : from == 'root' ? DL : acct ? DL + '/.accounts/' + acct : DL;
const ls = d => [...DISK].filter(([p]) => p.startsWith(d + '/') && !p.slice(d.length + 1).includes('/')).map(([p, v]) => ({name: p.slice(d.length + 1), size: v.length, mtime: 1, partial: /\.crdownload$/.test(p)}));
const uniq = (d, n) => { let p = d + '/' + n, i = 2; while (DISK.has(p)) { const m = /^(.*?)(\.[^.]*)?$/.exec(n); p = d + '/' + m[1] + ' (' + i++ + ')' + (m[2] || '') } return p };
const api = (a, q) => { const p = new URLSearchParams(q || ''); CALLS.push(a + '?' + (q || ''));
  if (a == 'dl/incoming') return {ok: true, items: ls(DL + '/.incoming')};
  if (a == 'dl/quarantine') return {ok: true, items: ls(DL + '/.quarantine')};
  if (a == 'dl/home') return {ok: true, items: ls(dirOf('home', p.get('acct')))};
  if (a == 'dl/file') { const n = p.get('name'), to = p.get('to'), from = p.get('from') || (to == 'release' ? 'quarantine' : 'incoming'), acct = p.get('acct');
    const src = dirOf(from, acct) + '/' + n; if (!DISK.has(src)) return {ok: false, error: 'the file is not there any more', _code: 404};
    const mv = dst => { DISK.set(dst, DISK.get(src)); DISK.delete(src); return {ok: true, path: dst, name: dst.split('/').pop(), size: DISK.get(dst).length} };
    if (to == 'del') { DISK.delete(src); return {ok: true} }
    if (to == 'keep' || to == 'release' || to == 'acct') return mv(uniq(dirOf('home', acct), n));
    if (to == 'quar') return mv(uniq(DL + '/.quarantine', n));
    if (to == 'rename') return mv(dirOf(from, acct) + '/' + p.get('as')) }
  if (a == 'dl/clear') { const d = DL + '/.accounts/' + p.get('acct') + '/'; [...DISK.keys()].filter(k => k.startsWith(d)).forEach(k => DISK.delete(k)); return {ok: true} }
  if (a == 'dl/space') return {ok: true, free: 50e9, total: 100e9};
  if (a == 'events/wait') return {ok: true, events: ''};
  return {ok: true} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":""}') }, 20000);
    if (u == '/api/dl/read') { const p = new URLSearchParams(qs), b = Buffer.from(DISK.get(dirOf(p.get('from'), p.get('acct')) + '/' + p.get('name')) || ''); s.writeHead(200, {'content-type': 'application/octet-stream'}); return s.end(b) }
    if (u.startsWith('/api/')) { const j = api(u.slice(5), qs); s.writeHead(j._code || 200, {'content-type': 'application/json'}); return s.end(JSON.stringify(j)) }
    s.writeHead(404); s.end() }) }).listen(8089, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd|debian/.test(e.stack || e.message || '')) errs.push(e.message) });
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8089/'); await p.waitForFunction(() => typeof dlReconcile == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  ok('0. at start (nobody signed in) the guest\'s folder is cleared', CALLS.some(c => c == 'dl/clear?acct=guest'), CALLS);
  // before 1.5.5: C:\DOWNLOADS with a small file and a large one at the top of Downloads on the disk; one account
  DISK.set(DL + '/movie.mp4', 'M'.repeat(3000));
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});window.__sys=window.__sys||{};SECC.av.on=true;SECC.av.act='quarantine';
    window.IN=async(n,pw)=>{const a=ACCTS.find(x=>x.n==n);if(!(await pwCheck(a,pw)))return false;if(USER)userLogout();USER=a;await userLogin(a,pw);return true};
    window.OUT=async()=>{fsFlush();await homeChain;A.logoff?A.logoff():userLogout();await homeChain;USER=null};
    ACCTS.length=0;ACCTS.push({n:'boss',h:await pwMake('b1'),r:'admin'});saveUser();
    fs['C:'].DOWNLOADS={'NOTE.TXT':'small note','MOVIE.MP4':dlMkRef('/home/rudolf/Downloads/movie.mp4',3000,'full')};fsSave();
    clearTimeout(DLN.t);DLN.t=setTimeout(()=>{},1e9);return 1})()`);
  await q("IN('boss','b1')"); await wait(500);
  const acct = await q("dlAcct()");
  await q("dlReconcile()");
  const m1 = await q("(()=>{const d=node(downloadsPath());return {path:downloadsPath().join('\\\\'),names:Object.keys(d).sort(),movie:dlRefInfo(d['MOVIE.MP4']),old:isDir(fs['C:'].DOWNLOADS)}})()");
  ok('1. one account: C:\\DOWNLOADS from before moves into its own Downloads', m1.path.toUpperCase() == 'C:\\USERS\\BOSS\\DOWNLOADS' && m1.names.join() == 'MOVIE.MP4,NOTE.TXT' && !m1.old, m1);
  ok('2. ...its large file into the account\'s folder on the disk', m1.movie && m1.movie.p == DL + '/.accounts/' + acct + '/movie.mp4' && DISK.has(DL + '/.accounts/' + acct + '/movie.mp4') && !DISK.has(DL + '/movie.mp4'), [m1.movie, [...DISK.keys()]]);
  // a download while boss is signed in
  DISK.set(DL + '/.incoming/big.zip', 'Z'.repeat(2 * 1048576 + 10));
  await q("(async()=>{clearTimeout(DLN.t);await dlPoll();await DLN.chain;clearTimeout(DLN.t);return 1})()");
  const d1 = await q("(()=>{const v=node([...downloadsPath(),'BIG.ZIP']);return v?dlRefInfo(v):null})()");
  ok('3. a download goes into the signed-in account\'s Downloads, kept in its folder on the disk', d1 && d1.p == DL + '/.accounts/' + acct + '/big.zip', d1);
  // a second account: it does not see boss's downloads
  await q("(async()=>{ACCTS.push({n:'kid',h:await pwMake('k1'),r:'user'});saveUser();return 1})()");
  await q("OUT()"); await q("IN('kid','k1')"); await wait(400);
  const kid = await q("(()=>({acct:dlAcct(),names:Object.keys(node(downloadsPath())||{})}))()");
  ok('4. another account has its own, empty Downloads', kid.names.length == 0 && kid.acct != acct, kid);
  // nobody signed in: a download waits
  await q("OUT()"); DISK.set(DL + '/.incoming/report.pdf', '%PDF-1.4 r');
  await q("(async()=>{clearTimeout(DLN.t);await dlPoll();await DLN.chain;clearTimeout(DLN.t);return 1})()");
  ok('5. nobody signed in: the download waits in .incoming', DISK.has(DL + '/.incoming/report.pdf'));
  await q("IN('kid','k1')"); await wait(300); await q("(async()=>{clearTimeout(DLN.t);await dlPoll();await DLN.chain;clearTimeout(DLN.t);return 1})()");
  ok('6. ...and goes to whoever signs in next', await q("!!node([...downloadsPath(),'REPORT.PDF'])") && !DISK.has(DL + '/.incoming/report.pdf'));
  // boss's references survive kid's clean-up (the disk files of another account are never deleted)
  await q("dlReconcile()"); await q("dlReconcile()"); await q("dlReconcile()");
  ok("7. another account's clean-up leaves boss's files on the disk alone", DISK.has(DL + '/.accounts/' + acct + '/big.zip') && DISK.has(DL + '/.accounts/' + acct + '/movie.mp4'), [...DISK.keys()]);
  // a file in boss's folder whose entry was lost comes back (#115), and not twice
  await q("OUT()"); DISK.set(DL + '/.accounts/' + acct + '/lost.iso', 'L'.repeat(100)); await q("IN('boss','b1')"); await wait(300);
  await q("dlReconcile()"); await q("dlReconcile()"); await q("dlReconcile()");
  const back = await q("(()=>{const d=node(downloadsPath());return Object.keys(d).filter(n=>/LOST/.test(n))})()");
  ok('8. a file in the account\'s folder that lost its entry is listed again, once', back.length == 1, back);
  ok('9. boss\'s downloads are still there after kid\'s session', await q("['MOVIE.MP4','NOTE.TXT','BIG.ZIP'].every(n=>!!node([...downloadsPath(),n]))"));
  // Linux apps' files at the top of Downloads are not taken into an account (they are the Linux apps' folder)
  DISK.set(DL + '/from-linux-app.txt', 'x'); await q("dlReconcile()"); await q("dlReconcile()");
  ok('10. a file a Linux app saved at the top of Downloads is not taken in by an account', !(await q("Object.keys(node(downloadsPath())).some(n=>/FROM-LINUX/.test(n))")) && DISK.has(DL + '/from-linux-app.txt'));
  // the guest: its folder is cleared at sign-out
  await q("OUT()"); DISK.set(DL + '/.accounts/guest/g.bin', 'g'); await q("(async()=>{USER={n:'Guest',r:'guest',id:'guest',h:''};await userLogin(USER,null);return 1})()"); await wait(200);
  CALLS.length = 0; await q("OUT()"); await wait(300);
  ok("11. the guest's large downloads are deleted at sign-out", CALLS.some(c => c == 'dl/clear?acct=guest') && !DISK.has(DL + '/.accounts/guest/g.bin'), CALLS);
  // several accounts before 1.5.5: C:\DOWNLOADS stays readable by all as the shared folder
  DISK.set(DL + '/old-shared.mp4', 'S'.repeat(4000));
  await q("(()=>{fs['C:'].DOWNLOADS={'OLD.TXT':'o','OLD-SHARED.MP4':dlMkRef('/home/rudolf/Downloads/old-shared.mp4',4000,'full')};DLMIG=false;fsSave();return 1})()");
  await q("IN('kid','k1')"); await wait(300); await q("dlReconcile()");
  const sh = await q("(()=>{const d=node(DLSHARED);return d?{names:Object.keys(d).sort(),ref:dlRefInfo(d['OLD-SHARED.MP4']),old:isDir(fs['C:'].DOWNLOADS),mine:Object.keys(node(downloadsPath())).includes('OLD.TXT')}:null})()");
  ok('12. several accounts: the old folder is C:\\SHARED DOWNLOADS (BEFORE 1.5.5), not put in an account', sh && sh.names.join() == 'OLD-SHARED.MP4,OLD.TXT' && !sh.old && !sh.mine, sh);
  ok('13. ...its large file in .accounts/shared on the disk', sh && sh.ref && sh.ref.p == DL + '/.accounts/shared/old-shared.mp4' && DISK.has(DL + '/.accounts/shared/old-shared.mp4'), sh);
  await q("OUT()"); await q("IN('boss','b1')"); await wait(300); await q("dlReconcile()"); await q("dlReconcile()"); await q("dlReconcile()");
  ok('14. ...and every account sees it, its file kept', await q("!!node([...DLSHARED,'OLD-SHARED.MP4'])") && DISK.has(DL + '/.accounts/shared/old-shared.mp4'));
  // deleting a download: gone from the disk after the Trash is emptied
  await q("(()=>{trashPut([[...downloadsPath(),'BIG.ZIP']]);TR.length=0;trSave();return 1})()"); await q("dlReconcile()"); await q("dlReconcile()"); await q("dlReconcile()");
  ok('15. a download deleted for good is deleted from the disk too', !DISK.has(DL + '/.accounts/' + acct + '/big.zip'), [...DISK.keys()]);
  ok('16. no page errors', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1)
})().catch(e => { console.log('FAIL ' + e.stack); process.exit(1) });
