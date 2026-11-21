// 1.6.1 (the owner, 2026-11-20): a ZIP larger than 1.5 MB kept on the disk (dlzip61.js, dl27.js, files25.js; rudolfd dl/zip,
// dl/unzip, dl/unzipped): opening it lists what is inside, with Extract here; Open with offers Archive Manager; Extract here (the
// window's, File Explorer's) shows a progress window with Cancel, then a folder named after the ZIP beside it, its sub-folders as
// in the ZIP, each file a reference to its file in the account's folder, checked by the antivirus (quarantined, or blocked and
// deleted, said so). dl27.js dlScope leaves references in sub-folders of Downloads where they are (not moved to the top, not
// renamed on the disk), still deletes files nothing refers to (a nested file deleted and the Trash emptied), still renames the
// top's; dlAdopt takes nothing in while dl/home says "unzip", and a file nobody refers to afterwards. A stand-in rudolfd with a
// disk in memory; made-up names only.   node dlzip61_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 700) : '')) };
const DL = '/home/rudolf/Downloads', DISK = new Map(), CALLS = [], EICAR = 'X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*';
const dirOf = (from, acct) => from == 'incoming' ? DL + '/.incoming' : from == 'quarantine' ? DL + '/.quarantine' : from == 'root' ? DL : acct ? DL + '/.accounts/' + acct : DL;
const ls = d => [...DISK].filter(([p]) => p.startsWith(d + '/') && !p.slice(d.length + 1).includes('/')).map(([p, v]) => ({name: p.slice(d.length + 1), size: v.length, mtime: 1, partial: false}));
const uniq = (d, n) => { let p = d + '/' + n, i = 2; while (DISK.has(p)) { const m = /^(.*?)(\.[^.]*)?$/.exec(n); p = d + '/' + m[1] + ' (' + i++ + ')' + (m[2] || '') } return p };
// what the made-up ZIP holds (path inside, content); unpacked flat into the account's folder as dlzip.sh does
const ZIP = [['Made Up Game/readme.txt', 'Read me first'], ['Made Up Game/data/level 1.dat', 'L'.repeat(3000)], ['Made Up Game/data/deep/x.bin', 'B'.repeat(5000)], ['readme.txt', 'the other readme']];
const ZDIRS = ['Made Up Game', 'Made Up Game/data', 'Made Up Game/data/deep', 'Made Up Game/empty'];
const zdirs = () => J.zip == ZIP ? ZDIRS : [...new Set(J.zip.map(x => x[0].split('/').slice(0, -1).join('/')).filter(Boolean))];
const J = {job: null, pending: '', result: null, zip: ZIP, stopReq: 0, slow: 4};
function unpack(acct, upto) { const files = [], d = dirOf('home', acct); for (const [p, c] of J.zip.slice(0, upto)) { const to = uniq(d, p.split('/').pop()); DISK.set(to, c); files.push({path: p, disk: to.split('/').pop(), size: c.length}) } return files }
const api = (a, q, post) => { const p = new URLSearchParams(q || ''); CALLS.push((post ? 'POST ' : '') + a + '?' + (q || ''));
  if (a == 'dl/incoming' || a == 'dl/quarantine') return {ok: true, items: []};
  if (a == 'dl/home') { const r = {ok: true, items: ls(dirOf('home', p.get('acct')))}; if (J.pending && J.pending == p.get('acct')) r.unzip = true; return r }
  if (a == 'dl/zip') { if (!DISK.has(dirOf('home', p.get('acct')) + '/' + p.get('name'))) return {ok: false, error: 'the file is not there any more', _code: 404};
    const items = [{name: 'Made Up Game/', size: 0, dir: true}, ...J.zip.map(([n, c]) => ({name: n, size: c.length, dir: false})), {name: 'Made Up Game/empty/', size: 0, dir: true}];
    return {ok: true, items, more: false, total: items.length, bytes: J.zip.reduce((s, x) => s + x[1].length, 0)} }
  if (a == 'dl/unzip') { if (!post) return {ok: false, error: 'use POST', _code: 400};
    if (p.get('stop') == '1') { J.stopReq = 1; return {ok: true, message: 'stopping'} }
    if (J.job) return {ok: false, error: 'another task is still running, try again in a moment', _code: 409};
    J.job = {n: 0, acct: p.get('acct'), name: p.get('name')}; J.pending = p.get('acct'); J.result = null; J.stopReq = 0; return {ok: true, message: 'started'} }
  if (a == 'dl/unzipped') { if (post) { J.result = null; J.pending = ''; return {ok: true} } const r = J.result || {dirs: [], files: [], end: ''}; return Object.assign({ok: true, running: !!J.job, acct: J.pending}, r) }
  if (a == 'job') { const j = J.job; if (!j) return {ok: true, name: J.last || '', running: false, status: 0, log: ''}; j.n++; const N = J.zip.length;
    if (j.n == 1 && j.name) j.files = unpack(j.acct, N);   // (the files are in the account's folder while the job runs, as dlzip.sh moves them)
    const head = `total ${N} ${J.zip.reduce((s, x) => s + x[1].length, 0)}\n`;
    if (J.stopReq) { const files = j.files.slice(0, 1); j.files.slice(1).forEach(f => DISK.delete(dirOf('home', j.acct) + '/' + f.disk)); J.result = {dirs: ZDIRS.slice(0, 1), files, end: 'stopped 1 ' + files[0].size}; J.job = null; J.last = 'dl-unzip'; return {ok: true, name: 'dl-unzip', running: false, status: 0, log: head + 'stopped 1'} }
    if (j.n < J.slow) return {ok: true, name: 'dl-unzip', running: true, status: -1, log: head + `progress 1 ${N} Made Up Game/readme.txt\nprogress 2 ${N} Made Up Game/data/level 1.dat\n`};
    const files = j.files; J.result = {dirs: zdirs(), files, end: `done ${N} ${files.reduce((s, f) => s + f.size, 0)}`}; J.job = null; J.last = 'dl-unzip';
    return {ok: true, name: 'dl-unzip', running: false, status: 0, log: head + files.map(f => `file ${f.path}\t${f.disk}\t${f.size}`).join('\n') + '\n' + J.result.end + '\n'} }
  if (a == 'dl/file') { const n = p.get('name'), to = p.get('to'), from = p.get('from') || (to == 'release' ? 'quarantine' : 'incoming'), acct = p.get('acct');
    const src = dirOf(from, acct) + '/' + n; if (!DISK.has(src)) return {ok: false, error: 'the file is not there any more', _code: 404};
    const mv = dst => { DISK.set(dst, DISK.get(src)); DISK.delete(src); return {ok: true, path: dst, name: dst.split('/').pop(), size: DISK.get(dst).length} };
    if (to == 'del') { DISK.delete(src); return {ok: true} }
    if (to == 'keep' || to == 'release' || to == 'acct') return mv(uniq(dirOf('home', acct), n));
    if (to == 'quar') return mv(uniq(DL + '/.quarantine', n));
    if (to == 'copy') { const dst = uniq(dirOf('home', acct), p.get('as') || n); DISK.set(dst, DISK.get(src)); return {ok: true, path: dst, name: dst.split('/').pop(), size: DISK.get(dst).length} }
    if (to == 'rename') { const dst = dirOf(from, acct) + '/' + p.get('as'); if (DISK.has(dst)) return {ok: false, error: 'a file with that name is already there', _code: 409}; return mv(dst) } }
  if (a == 'dl/link') return {ok: true, url: '/dlf/x'};
  if (a == 'dl/space') return {ok: true, free: 50e9, total: 100e9};
  if (a == 'dl/clear') return {ok: true};
  return {ok: true} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":""}') }, 20000);
    if (u == '/api/dl/read') { CALLS.push('dl/read?' + qs); const p = new URLSearchParams(qs), b = Buffer.from(DISK.get(dirOf(p.get('from'), p.get('acct')) + '/' + p.get('name')) || ''); s.writeHead(200, {'content-type': 'application/octet-stream'}); return s.end(b) }
    if (u.startsWith('/api/')) { const j = api(u.slice(5), qs, q.method == 'POST'); s.writeHead(j._code || 200, {'content-type': 'application/json'}); return s.end(JSON.stringify(j)) }
    s.writeHead(404); s.end() }) }).listen(8600, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd|debian/.test(e.stack || e.message || '')) errs.push(e.message) });
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8600/'); await p.waitForFunction(() => typeof dlZipWin == 'function' && typeof dlReconcile == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});window.__sys=window.__sys||{};SECC.av.on=true;SECC.av.act='quarantine';
    ACCTS.length=0;ACCTS.push({n:'ann',h:await pwMake('a1'),r:'admin'});saveUser();const a=ACCTS[0];USER=a;await userLogin(a,'a1');
    clearTimeout(DLN.t);DLN.t=setTimeout(()=>{},1e9);window.NT=[];const _n=notify;notify=function(t,b,o){NT.push(t+': '+b);return _n.apply(this,arguments)};return 1})()`); await wait(500);
  const acct = await q('dlAcct()'), AD = DL + '/.accounts/' + acct + '/';
  DISK.set(AD + 'game.zip', 'PK' + 'z'.repeat(5000)); DISK.set(AD + 'readme.txt', 'already here');
  await q(`(()=>{const d=node(downloadsPath());d['GAME.ZIP']=dlMkRef('${AD}game.zip',5002,'full');dlRefsAdd('${AD}game.zip');d['README.TXT']=dlMkRef('${AD}readme.txt',12,'full');dlRefsAdd('${AD}readme.txt');fsSave();return 1})()`);
  const ZP = "[...downloadsPath(),'GAME.ZIP']";
  // 1. opening it: the list
  await q(`openPath(${ZP})`); await wait(800);
  let w = await q("(()=>{const W=wins.find(x=>/^Archive Manager - GAME.ZIP/.test(x.title||x.el.querySelector('.tt,.title')?.textContent||''))||wins.at(-1);const b=W.body;return {rows:[...b.querySelectorAll('tbody tr')].map(r=>r.textContent),stat:b.querySelector('.dlzs').textContent,ex:!b.querySelector('.dlzx').disabled}})()");
  ok('1. opening a large ZIP kept on the disk: its contents listed (names, sizes, folders, nested)', w.rows.includes('📁 Made Up Game/') && w.rows.includes('📄 Made Up Game/data/level 1.dat2.9 KB') && w.rows.includes('📁 Made Up Game/empty/') && /^4 file\(s\), 7\.8 KB unpacked, in GAME\.ZIP$/.test(w.stat) && w.ex, w);
  ok('2. ...read by rudolfd (dl/zip with the name on the disk and the account), not loaded into the page', CALLS.includes(`dl/zip?name=game.zip&acct=${acct}`) && !CALLS.some(c => /^dl\/read\?name=game\.zip/.test(c)), CALLS.slice(-5));
  const ow = await q(`openersFor(${ZP}).map(o=>o[1])`);
  ok('3. Open with: Archive Manager offered for it', ow.includes('Archive Manager') && ow.includes('Built-in viewer'), ow);
  await q(`(()=>{wins.slice().forEach(W=>{try{W.close()}catch(e){}});const o=openersFor(${ZP}).find(x=>x[1]=='Archive Manager');o[4](${ZP});return 1})()`); await wait(600);
  ok('4. ...which opens the same list', await q("!!document.querySelector('.dlzl tbody tr')"));
  // 5. Extract here in that window: the progress window, then the folder
  await q("document.querySelector('.dlzx').click()"); await wait(1400);
  const pw = await q("(()=>{const n=document.querySelector('.dlzn'),s=document.querySelector('.dlzb'),c=document.querySelector('.dlzc');return {n:n&&n.textContent,s:s&&s.textContent,c:c&&!c.disabled}})()");
  ok('5. Extract here: a progress window (the file, n of N, bytes, Cancel)', /^Unpacking level 1\.dat$/.test(pw.n) && /^2 of 4 files · about 3\.9 KB of 7\.8 KB$/.test(pw.s) && pw.c && CALLS.includes(`POST dl/unzip?name=game.zip&acct=${acct}`), pw);
  // (as Oracle's page on the native desktop, which does not know this page's DLN.ing: twice, as dlAdopt takes a file seen twice)
  const ad1 = await q("(async()=>{const s=DLN.ing;DLN.ing=0;const a=await dlAdopt({}),b=await dlAdopt({});DLN.ing=s;return [a,b]})()");
  ok('6. ...meanwhile dlAdopt takes in none of its files, also in another page (dl/home says unzip)', DISK.has(AD + 'level 1.dat') && ad1[0] === false && ad1[1] === false && !(await q("Object.keys(node(downloadsPath())).some(k=>/LEVEL/.test(k))")), ad1);
  await wait(3500);
  let f = await q(`(()=>{const D=node(downloadsPath()),G=D.GAME;const info=v=>dlRefInfo(v);return G?{top:Object.keys(D).sort(),g:Object.keys(G).sort(),m:Object.keys(G['MADE UP GAME']||{}).sort(),data:Object.keys((G['MADE UP GAME']||{}).DATA||{}).sort(),deep:info(((G['MADE UP GAME']||{}).DATA||{}).DEEP['X.BIN']),lvl:info(G['MADE UP GAME'].DATA['LEVEL 1.DAT']),rd:info(G['README.TXT']),rd2:info(G['MADE UP GAME']['README.TXT']),empty:isDir(G['MADE UP GAME'].EMPTY),keys:Object.keys(ld(DLRKA(),{}))}:{top:Object.keys(D)}})()`);
  ok('7. a folder named after the ZIP beside it, its sub-folders as in the ZIP (the empty one too)', f.g && f.top.includes('GAME') && f.g.join() == 'MADE UP GAME,README.TXT' && f.m.join() == 'DATA,EMPTY,README.TXT' && f.data.join() == 'DEEP,LEVEL 1.DAT' && f.empty, f);
  ok('8. each file a reference to its own file in the account\'s folder (flat, names that clash made free), its size', f.lvl && f.lvl.p == AD + 'level 1.dat' && f.lvl.s == 3000 && f.lvl.av == 'full' && f.deep.p == AD + 'x.bin' && f.rd2.p == AD + 'readme (2).txt' && f.rd.p == AD + 'readme (3).txt' && DISK.get(AD + 'readme.txt') == 'already here', f);
  ok('9. ...tracked (dlRefsAdd), the result taken (dl/unzipped), the progress window done', f.keys && f.keys.includes(AD + 'level 1.dat') && f.keys.includes(AD + 'x.bin') && CALLS.includes('POST dl/unzipped?') && J.pending == '', f.keys);
  const nt = await q('NT');
  ok('10. a notification says where they are', nt.some(x => /^Extracted: 4 file\(s\) from GAME\.ZIP are in C:\\USERS\\ANN\\DOWNLOADS\\GAME\.$/i.test(x)), nt);
  // 11-13. dlScope: nested references stay, are not renamed on the disk, files keep
  const before = CALLS.length; await q("dlReconcile()"); await wait(300); await q("dlReconcile()"); await wait(300);
  const after = CALLS.slice(before);
  f = await q(`(()=>{const D=node(downloadsPath());return {top:Object.keys(D).sort(),lvl:dlRefInfo(D.GAME['MADE UP GAME'].DATA['LEVEL 1.DAT'])}})()`);
  ok('11. dlScope leaves references in sub-folders of Downloads where they are (not moved to the top)', f.top.join() == 'GAME,GAME.ZIP,README.TXT' && f.lvl && f.lvl.p == AD + 'level 1.dat', f);
  ok('12. ...not renamed on the disk, nothing deleted, nothing taken in again (dlAdopt knows them)', !after.some(c => /to=rename|to=del/.test(c)) && DISK.has(AD + 'level 1.dat') && DISK.has(AD + 'x.bin') && !(await q("Object.keys(node(downloadsPath())).some(k=>/LEVEL|X\\.BIN/.test(k))")), after.filter(c => /dl\/file/.test(c)));
  // 13. a nested file deleted, the Trash emptied: its file deleted from the disk after two looks, the others kept
  await q(`(()=>{trashPut([[...downloadsPath(),'GAME','MADE UP GAME','DATA','LEVEL 1.DAT']]);return 1})()`); await q("dlReconcile()"); await wait(200);
  ok('13. a nested file in the Trash: still kept on the disk', DISK.has(AD + 'level 1.dat'));
  await q("(()=>{TR.length=0;trSave();return 1})()"); await q("dlReconcile()"); await wait(200); await q("dlReconcile()"); await wait(300);
  ok('14. ...the Trash emptied: its file deleted from the disk; the other nested ones kept', !DISK.has(AD + 'level 1.dat') && DISK.has(AD + 'x.bin') && DISK.has(AD + 'readme (2).txt'), [...DISK.keys()]);
  // 15. the top's references are still renamed on the disk; one put elsewhere still goes back to Downloads
  await q(`(()=>{const D=node(downloadsPath());D['NOTES.TXT']=D['README.TXT'];delete D['README.TXT'];const H=node(homePath('DOCUMENTS'));H['MOVED.BIN']=D.GAME['MADE UP GAME'].DATA.DEEP['X.BIN'];delete D.GAME['MADE UP GAME'].DATA.DEEP['X.BIN'];fsSave();return 1})()`);
  await q("dlReconcile()"); await wait(400);
  f = await q(`(()=>{const D=node(downloadsPath());return {top:Object.keys(D).sort(),n:dlRefInfo(D['NOTES.TXT']),m:dlRefInfo(D['MOVED.BIN']),doc:Object.keys(node(homePath('DOCUMENTS')))}})()`);
  ok('15. at Downloads\' top a renamed reference is still renamed on the disk; one moved out of Downloads still comes back to its top', f.n && f.n.p == AD + 'NOTES.TXT' && DISK.has(AD + 'NOTES.TXT') && f.m && f.m.p == AD + 'MOVED.BIN' && DISK.has(AD + 'MOVED.BIN') && !f.doc.includes('MOVED.BIN'), f);
  // 16. a file nobody refers to (after the extraction) is still taken in by dlAdopt after two looks
  DISK.set(AD + 'lost.iso', 'I'.repeat(100)); await q("dlReconcile()"); await wait(200); await q("dlReconcile()"); await wait(400);
  ok('16. dlAdopt still takes in a file nobody refers to', await q("!!node(downloadsPath())['LOST.ISO']"), await q("Object.keys(node(downloadsPath()))"));
  // 16b. a copy in a sub-folder that names the same file as another (made before 1.6.1): made a file of its own, kept in its folder
  await q(`(()=>{const M=node(downloadsPath()).GAME['MADE UP GAME'];M['COPY.TXT']=M['README.TXT'];fsSave();return 1})()`); await q("dlReconcile()"); await wait(400);
  f = await q(`(()=>{const M=node(downloadsPath()).GAME['MADE UP GAME'];return {c:dlRefInfo(M['COPY.TXT']),r:dlRefInfo(M['README.TXT']),top:Object.keys(node(downloadsPath())).sort()}})()`);
  ok('16b. a copy in a sub-folder naming the same file: copied on the disk, kept in its sub-folder (dlDupFix)', f.c && f.r && f.c.p != f.r.p && DISK.has(f.c.p) && DISK.has(f.r.p) && !f.top.includes('COPY.TXT'), f);
  // 17-18. File Explorer's Extract here, and the antivirus: one quarantined, one blocked and deleted
  J.zip = [['docs/a.txt', 'aaaa'], ['docs/test.com', EICAR], ['b.txt', 'bb']]; J.slow = 2;
  await q(`(()=>{wins.slice().forEach(W=>{try{W.close()}catch(e){}});explorer(downloadsPath());return 1})()`); await wait(800);
  const menu = await q(`(()=>{const k=pkey(${ZP}),row=[...document.querySelectorAll('[data-k]')].find(r=>r.dataset.k==k);if(!row)return 'no row';row.dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,clientX:300,clientY:300}));const it=[...document.querySelectorAll('.cmi')].find(x=>/^Extract here/.test(x.textContent));if(!it)return [...document.querySelectorAll('.cmi')].map(x=>x.textContent);it.click();return 'ok'})()`);
  await wait(3500);
  f = await q(`(()=>{const D=node(downloadsPath()),G=D['GAME (2)'];return {g:G&&Object.keys(G).sort(),docs:G&&Object.keys(G.DOCS||{}).sort(),q:QUAR.map(x=>x.n+'|'+x.th.join()+'|'+dlRefInfo(x.data).p),log:(SLOG.av||[]).map(x=>x.m).slice(-2)}})()`);
  ok('17. File Explorer\'s Extract here: the same extraction (GAME (2) beside the ZIP)', menu == 'ok' && f.g && f.g.join() == 'B.TXT,DOCS' && f.docs.join() == 'A.TXT', {menu, f});
  ok('18. the antivirus on each file: the EICAR test file quarantined (not in the folder), said so', f.q.some(x => /^TEST\.COM\|EICAR-Test-File\|\/home\/rudolf\/Downloads\/\.quarantine\/test\.com$/.test(x)) && !DISK.has(AD + 'test.com') && DISK.has(DL + '/.quarantine/test.com') && f.log.some(x => /Quarantined TEST\.COM \(extracted from GAME\.ZIP\)/.test(x)), f);
  J.zip = [['evil.com', EICAR], ['ok.txt', 'ok']]; await q("SECC.av.act='block'"); await q(`dlZipExtract(${ZP})`); await wait(3500);
  f = await q(`(()=>{const G=node(downloadsPath())['GAME (3)'];return {g:G&&Object.keys(G).sort()}})()`);
  ok('19. ...set to block: deleted from the disk, not in the folder', f.g && f.g.join() == 'OK.TXT' && !DISK.has(AD + 'evil.com'), [f, [...DISK.keys()]]);
  // 20. Cancel
  J.zip = ZIP; J.slow = 30; await q(`(dlZipExtract(${ZP}),1)`); await wait(1200); await q("[...document.querySelectorAll('.dlzc')].at(-1).click()"); await wait(2500);
  f = await q(`(()=>{const G=node(downloadsPath())['GAME (4)'];return {g:G&&JSON.stringify(G),n:[...document.querySelectorAll('.dlzn')].at(-1).textContent}})()`);
  ok('20. Cancel stops it (dl/unzip?stop=1); what was extracted is kept in the folder, said so', CALLS.includes('POST dl/unzip?stop=1') && f.g && /README\.TXT/.test(f.g), f);
  // 21. busy: another task running
  J.job = {n: -1e9, acct, name: 'x'}; const busy = await q(`(async()=>{await dlZipExtract(${ZP});return 1})()`); J.job = null; J.pending = '';
  const nt2 = await q('NT');
  ok('21. another task running: said so, nothing started', /busy with another task/.test(nt2.at(-1)) && !(await q("!!node(downloadsPath())['GAME (5)']")), nt2.slice(-2));
  ok('21b. the stopped one said so', nt2.some(x => /^Extraction stopped: Stopped: 1 file\(s\) from GAME\.ZIP are in .*GAME \(4\)\.$/.test(x)), nt2.slice(-3));
  ok('22. no page error', !errs.length, errs);
  await b.close(); srv.close(); const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
