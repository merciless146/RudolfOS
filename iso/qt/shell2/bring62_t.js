// 1.6.2 (docs/plan-1.6.2.md Part 27): Bring your files on the page (desktop/bring62.js) with a stand-in rudolfd: the installer's
// step (Settings › Update and recovery › Install here…): what was found, per person and folder with sizes, Windows not shut
// down fully said, BitLocker's recovery key (a wrong one, the right one), a Mac's message; keep (the copy's request, its
// progress, done → the installation goes on), a cancelled copy (nothing erased, the installation does not go on), a drive
// that fills up and the next one continuing with the same folder and selection, erase without keeping, a disk with nothing
// on it going straight on; then Settings' Bring in files from the old computer (the request, the antivirus on what came in)
// and the sign-in's offer. On Standard's page and in Native's app window (index.html#app=settings, the same page alone).
//   node bring62_t.js [CHROME]   (build iso/www first; port 8600, or BRING62_PORT)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 700) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.html': 'text/html', '.webp': 'image/webp', '.jpg': 'image/jpeg', '.woff2': 'font/woff2', '.woff': 'font/woff'};
const PORT = +process.env.BRING62_PORT || 8600, URL0 = 'http://127.0.0.1:' + PORT + '/';
const KEY = '235818-357951-253979-013365-241120-245575-342914-591910';
const SYS = {part: 'nvme0n1p3', fs: 'ntfs', os: 'windows', name: 'Windows', state: 'hibernated', people: [
  {id: 'p1', name: 'Ann <b>', bytes: 3.2e9, folders: [{k: 'Documents', files: 340, bytes: 1.2e9}, {k: 'Pictures', files: 2100, bytes: 2e9}, {k: 'Other', files: 3, bytes: 1e6}]},
  {id: 'p2', name: 'Bob', bytes: 9e8, folders: [{k: 'Videos', files: 2, bytes: 9e8}]}]};
const M = {calls: [], bodies: [], job: null, session: null};
const reset = () => Object.assign(M, {calls: [], bodies: [], job: null, copyEnd: 0, result: {ok: true, disk: 'nvme0n1', found: true, total: 4.1e9, systems: [SYS], locked: [{part: 'nvme0n1p4', kind: 'bitlocker'}, {part: 'nvme0n2p1', kind: 'apfs'}]},
  found: {ok: true, sets: [{dir: '/media/sdc1/Rudolf OS - Bring your files/Old computer 2026-11-22 10.30', made: '2026-11-22T10:31Z', system: 'windows', drive: 1, last: true, people: [{dir: 'Ann _b_', name: 'Ann <b>', files: 2443, bytes: 3.2e9}, {dir: 'Bob', name: 'Bob', files: 2, bytes: 9e8}]}]}});
reset();
const DISKS = {ok: true, disks: [{name: 'nvme0n1', removable: false, model: 'Old laptop disk', bytes: 5.12e11, partitions: [{name: 'nvme0n1p1', fs: 'vfat', label: 'SYSTEM', bytes: 1e8, mount: ''}, {name: 'nvme0n1p3', fs: 'ntfs', label: '', bytes: 5e11, mount: ''}]},
  {name: 'sdb', removable: true, model: 'Rudolf stick', bytes: 3.2e10, partitions: [{name: 'sdb1', fs: 'iso9660', label: 'RUDOLF', bytes: 2e9, mount: '/live/media'}]},
  {name: 'sdc', removable: true, model: 'SanDisk', bytes: 6.4e10, partitions: [{name: 'sdc1', fs: 'exfat', label: 'STICK', bytes: 6.4e10, mount: ''}]},
  {name: 'sdd', removable: true, model: 'Kingston', bytes: 3.2e10, partitions: [{name: 'sdd1', fs: 'vfat', label: 'STICK2', bytes: 3.2e10, mount: ''}]}]};
const LOGS = {
  'bring-scan': ['Looking at what is on nvme0n1…', 'Found: 4.1 GB of personal files.'],
  'bring-copy': ['Counting what to copy…', '2445 files, 4.1 GB, to copy into /media/sdc1/x (60 GB free there).', 'PROGRESS 600 2445 1000000000 4100000000', '  Ann _b_/Pictures/IMG_1.JPG'],
  'bring-import': ['2443 files, 3.2 GB, to bring into your folders.', 'PROGRESS 1200 2443 1600000000 3200000000']};
const END = {'bring-copy': {0: 'Copied and checked: 2445 files (4.1 GB) into /media/sdc1/x.', 28: 'The drive is full: 700 of 2445 files are on it, checked.', 130: 'Cancelled: 600 of 2445 files were copied. Nothing is erased: the disk is as it was.'}};
const job = () => { const j = M.job; if (!j) return {ok: true, name: '', running: false, status: 0, log: ''}; j.n++;
  const L = LOGS[j.name] || [], st = j.name == 'bring-copy' ? (M.cancel ? 130 : M.copyEnd) : 0;
  if (j.n < 3 && !M.cancel) return {ok: true, name: j.name, running: true, status: -1, log: L.join('\n') + '\n'};
  return {ok: true, name: j.name, running: false, status: st, log: L.join('\n') + '\nPROGRESS 2445 2445 4100000000 4100000000\n' + ((END[j.name] || {})[st] || (j.name == 'bring-import' ? 'Brought in: 2 files (12 KB).' : 'done')) + '\n'} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0); const sp = new URLSearchParams(qs || '');
  const bs = []; q.on('data', d => bs.push(d)); q.on('end', () => { const body = Buffer.concat(bs).toString();
    const J = (x, st) => { s.writeHead(st || 200, {'content-type': 'application/json'}); s.end(JSON.stringify(x)) };
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"usb",live:1,recovery:0};</script>')) }
    if (u.startsWith('/api/')) { const a = u.slice(5); M.calls.push(q.method + ' ' + a + (qs ? '?' + qs : ''));
      if (a == 'events/wait') return setTimeout(() => J({ok: true, events: []}), 20000);
      if (a == 'desktop/session') return M.session ? J(M.session) : J({ok: false, error: 'no native session'}, 404);
      if (a == 'disks') return J(DISKS);
      if (/^sys\//.test(a)) return J({ok: false, error: 'not in this test'}, 404);
      if (a == 'info') return J({ok: true, live: true, media: 'usb', boot: 'uefi', recovery: false, bootSecure: 1});
      if (a == 'job') return J(job());
      if (a == 'bring/scan') { M.cancel = 0; M.job = {name: 'bring-scan', n: 0}; return J({ok: true, message: 'looking'}) }
      if (a == 'bring/result') return J(M.result);
      if (a == 'bring/unlock') { M.bodies.push('KEY ' + body); if (body != KEY) return J({ok: false, error: 'That key does not open it.'}, 403);
        M.result = Object.assign({}, M.result, {locked: M.result.locked.filter(x => x.kind != 'bitlocker')}); return J({ok: true, message: 'opened (read-only)'}) }
      if (a == 'bring/copy') { M.cancel = 0; M.bodies.push(body); M.job = {name: 'bring-copy', n: M.slow ? -40 : 0}; return J({ok: true, message: 'copying'}) }
      if (a == 'bring/cancel') { M.cancel = 1; return J({ok: true, message: 'stopping'}) }
      if (a == 'bring/found') return J(M.found);
      if (a == 'bring/import') { M.job = {name: 'bring-import', n: 0}; return J({ok: true, message: 'bringing in'}) }
      if (a == 'home/read') { const p = sp.get('path'); s.writeHead(200, {'content-type': 'application/octet-stream'});
        return s.end(p == '.rudolf/bring-imported.txt' ? 'Documents/letter.txt\nDocuments/From the old computer/x.com.txt\n' : /x\.com/.test(p) ? 'X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*' : 'hello') }
      if (a == 'home') return J({ok: true, supported: false, accounts: []});
      return J({ok: true, message: 'done'}) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(PORT, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const ctx = await b.newContext({viewport: {width: 1200, height: 820}});
  const errs = []; const watch = p => p.on('pageerror', e => errs.push(e.message + ' @ ' + String(e.stack).split('\n').slice(1, 3).join(' ')));
  let p = await ctx.newPage(); watch(p);
  await p.goto(URL0); await p.waitForFunction(() => typeof bringStep == 'function' && typeof acctNew == 'function', null, {timeout: 30000});
  const acct = await p.evaluate(`(async()=>{svj('bd95setup',1);['oobe'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    const a=await acctNew('Ann','pw-one','admin');ACCTS=[a];window.__sys=1;saveUser();USER=a;await userLogin(a,'pw-one');fsSave();fsFlush();await new Promise(r=>setTimeout(r,800));
    const hex=u=>[...new Uint8Array(u)].map(x=>x.toString(16).padStart(2,'0')).join('');
    return {id:a.id,pk:hex(await pbkBits('pw-one',a.ks,256)),fk:hex(FKRAW)}})()`);
  M.session = {ok: true, id: acct.id, n: 'Ann', r: 'admin', pk: acct.pk, fk: acct.fk};
  const D = pg => pg.evaluate(`(()=>{const o=document.querySelector('.brdlg');if(!o)return null;const vis=e=>e&&!e.hidden&&e.offsetParent!==null;
    return {msg:o.querySelector('.brm').textContent,legends:[...o.querySelectorAll('legend')].map(x=>x.textContent),folders:[...o.querySelectorAll('.brf')].map(c=>c.parentNode.textContent.trim()+(c.checked?' [x]':' [ ]')),
      warn:[...o.querySelectorAll('.brw')].map(x=>x.textContent).join('|'),locked:[...o.querySelectorAll('.brlk')].map(x=>x.textContent).join('|'),modes:[...o.querySelectorAll('.brmd')].map(r=>r.parentNode.textContent.trim()+(r.checked?' [x]':'')),
      opts:[...o.querySelectorAll('.brto option')].map(x=>x.textContent),go:o.querySelector('.brgo').textContent,goOff:o.querySelector('.brgo').disabled,pt:o.querySelector('.brpt').textContent,log:o.querySelector('.brl').textContent,
      um:(o.querySelector('.brum')||{}).textContent||'',nt:(o.querySelector('.brnt')||{}).textContent||'',btn:[...o.querySelectorAll('.brr button')].map(x=>x.textContent),bhtml:o.querySelector('legend')?o.querySelector('legend').innerHTML:''}})()`);
  const until = async (pg, f, ms) => { const t = Date.now() + (ms || 8000); for (;;) { const d = await D(pg); if (d && f(d) || Date.now() > t) return d; await wait(200) } };
  const start = pg => pg.evaluate("window.__br=null;bringStep({name:'nvme0n1',model:'Old laptop disk',bytes:5.12e11}).then(v=>window.__br=v);0");
  const brv = async (pg, v, ms) => { const t = Date.now() + (ms || 6000); for (;;) { const x = await pg.evaluate('window.__br'); if (x === v || Date.now() > t) return x === v; await wait(150) } };
  const click = (pg, sel) => pg.evaluate(`document.querySelector(${JSON.stringify(sel)}).click()`);
  async function flow(pg, tag) {
    reset(); await start(pg); let d = await until(pg, d => d.legends.length);
    ok(`${tag} 1. looked at (bring/scan, then its result): the people, each folder with its size, all ticked`, d && M.calls.some(c => /^POST bring\/scan\?disk=nvme0n1$/.test(c)) && d.legends.length == 2 && /^Ann <b> — 3\.0 GB$/.test(d.legends[0]) && d.folders.length == 4 && d.folders.every(x => /\[x\]$/.test(x)) && d.folders.some(x => /^Pictures — 1\.9 GB \(2,100 files\)/.test(x)) && d.folders.some(x => /^Other files and folders/.test(x)), d);
    ok(`${tag} 2. a name from the old disk shown as text (not HTML)`, d && /&lt;b&gt;/.test(d.bhtml), d && d.bhtml);
    ok(`${tag} 3. a hibernated Windows (Fast Startup) said; BitLocker asks for its recovery key (where it usually is); the Mac's message`, d && /hibernated, or uses Fast Startup/.test(d.warn) && /recovery key/.test(d.locked) && /account\.microsoft\.com\/devices\/recoverykey/.test(d.locked) && /macOS on it \(APFS\)\. Rudolf OS cannot read it\. Copy your files on the Mac first/.test(d.locked), d);
    ok(`${tag} 4. keep is the default; erase without keeping says what is lost`, d && d.modes.length == 2 && /^Keep them.*\[x\]$/.test(d.modes[0]) && /Erase them without keeping: 3\.8 GB of personal files on nvme0n1 will be lost for good/.test(d.modes[1]), d && d.modes);
    d = await until(pg, d => d.opts.length && !/Looking/.test(d.opts[0]));
    ok(`${tag} 5. the drives to copy onto: the USB sticks, not the disk installed on, not Rudolf OS's own`, d && d.opts.length == 2 && d.opts.every(x => /STICK/.test(x)) && /4 folder\(s\), 3\.8 GB, are copied onto 🔌? ?STICK|4 folder\(s\), 3\.8 GB, are copied onto STICK/.test(d.nt) && d.go == 'Copy and check' && !d.goOff, d);
    // BitLocker: a wrong key, then the right one (sent as the request's body, never in its address)
    await pg.evaluate("document.querySelector('.brkey').value='111111-222222'"); await click(pg, '.bru'); d = await until(pg, d => /does not open/.test(d.um));
    ok(`${tag} 6. a wrong recovery key: said`, d && /That key does not open it/.test(d.um), d && d.um);
    await pg.evaluate(`document.querySelector('.brkey').value=${JSON.stringify(KEY)}`); await click(pg, '.bru'); d = await until(pg, d => !/recovery key/.test(d.locked) && d.legends.length);
    ok(`${tag} 7. the right key: opened, looked at again (the key in the body only, not kept on the page)`, d && M.bodies.includes('KEY ' + KEY) && !M.calls.some(c => c.includes('235818')) && M.calls.filter(c => /bring\/scan/.test(c)).length == 2 && !(await pg.evaluate("document.body.innerHTML.includes('235818')")), d);
    // untick Bob's videos, copy; progress; done → the installation goes on
    await pg.evaluate("[...document.querySelectorAll('.brf')].find(c=>c.dataset.id=='p2').click()"); d = await until(pg, d => d.opts.length > 1);
    await click(pg, '.brgo'); d = await until(pg, d => /600 of 2,445 files/.test(d.pt));
    const body = M.bodies.filter(x => !/^KEY /.test(x))[0] || '';
    ok(`${tag} 8. the copy asked for: the disk, into "Rudolf OS - Bring your files\\Old computer …" on the stick, the folders ticked (by person and folder)`, /^nvme0n1 continue=0\n\/media\/sdc1\/Rudolf OS - Bring your files\/Old computer \d{4}-\d\d-\d\d \d\d\.\d\d\np1\tDocuments\np1\tPictures\np1\tOther\n$/.test(body) && M.calls.includes('POST mount?dev=sdc1'), body);
    ok(`${tag} 9. its progress, and a Stop button while it runs`, d && /600 of 2,445 files · 953\.67 MB of 3\.8 GB/.test(d.pt) && !/PROGRESS/.test(d.log) && d.btn.includes('Stop'), d);
    d = await until(pg, d => /Copied and checked/.test(d.msg));
    ok(`${tag} 10. done: copied and checked, Continue to install`, d && d.go == 'Continue to install' && /Copied and checked/.test(d.msg) && await pg.evaluate('window.__br') === null, d);
    await click(pg, '.brgo');
    ok(`${tag} 11. ...then the installation goes on (the step answers yes)`, await brv(pg, true) && !(await D(pg)));
    // a cancelled copy: nothing erased, the installation does not go on
    reset(); M.slow = 1; await start(pg); await until(pg, d => d.opts.length > 1); await click(pg, '.brgo'); await until(pg, d => d.btn.includes('Stop'));
    await click(pg, '.brx'); d = await until(pg, d => /Nothing was erased/.test(d.msg)); M.slow = 0;
    ok(`${tag} 12. Stop: bring/cancel, "Nothing was erased", the choice offered again`, d && M.calls.includes('POST bring/cancel') && /Stopped\. Nothing was erased: the disk is as it was/.test(d.msg) && d.go == 'Copy and check', d);
    await click(pg, '.brx');
    ok(`${tag} 13. ...Cancel: the installation does not go on, the old disk closed`, await brv(pg, false) && M.calls.some(c => /^POST bring\/close\?disk=nvme0n1/.test(c)));
    // a drive that fills up, then the next one: the same folder and the same selection
    reset(); M.copyEnd = 28; await start(pg); await until(pg, d => d.opts.length > 1); await click(pg, '.brgo'); d = await until(pg, d => /drive is full/.test(d.msg));
    const b1 = M.bodies[0] || '';
    ok(`${tag} 14. the drive full: said, Safely remove offered, Continue onto this drive`, d && /The drive is full: what is on it is checked/.test(d.msg) && d.btn.some(x => /Safely remove STICK/.test(x)), d);
    d = await until(pg, d => d.go == 'Continue onto this drive' && d.opts.length > 1);
    M.copyEnd = 0; await pg.evaluate("const s=document.querySelector('.brto');s.value=[...s.options].findIndex(o=>/STICK2/.test(o.textContent));s.dispatchEvent(new Event('change'))");
    await pg.evaluate("document.querySelector('.brgo').disabled=false"); await click(pg, '.brgo'); d = await until(pg, d => /Copied and checked/.test(d.msg));
    const b2 = M.bodies[1] || '';
    ok(`${tag} 15. ...the next drive continues: continue=1, the same folder name and selection, onto the other stick`, /^nvme0n1 continue=1\n\/media\/sdd1\//.test(b2) && b1.split('\n').slice(1).map(x => x.replace(/^\/media\/sd.1/, '')).join('\n') == b2.split('\n').slice(1).map(x => x.replace(/^\/media\/sd.1/, '')).join('\n'), [b1, b2]);
    await click(pg, '.brgo'); await wait(300);
    // erase without keeping
    reset(); await start(pg); await until(pg, d => d.legends.length);
    await pg.evaluate("document.querySelectorAll('.brmd')[1].click()"); d = await D(pg);
    ok(`${tag} 16. erase without keeping chosen: the button says so`, d && d.go == 'Erase without keeping', d && d.go);
    await click(pg, '.brgo');
    ok(`${tag} 17. ...bring/decide?what=erase, then the installation goes on`, await brv(pg, true) && M.calls.some(c => /^POST bring\/decide\?disk=nvme0n1&what=erase$/.test(c)));
    // nothing on the disk: straight on
    reset(); M.result = {ok: true, disk: 'nvme0n1', found: false, total: 0, systems: [], locked: []}; await start(pg);
    ok(`${tag} 18. a disk with no other system: the installation goes straight on`, await brv(pg, true, 10000) && !(await D(pg)));
    // Bring in files from the old computer (Settings › Update and recovery), the account keeping its files on the disk
    reset(); await pg.evaluate("USER.disk=true;HD.acct=uid();settings('upd')"); await pg.waitForSelector('.brin .brlook', {timeout: 10000}).catch(() => {});
    const has = await pg.evaluate("!!document.querySelector('.brin .brlook')");
    await click(pg, '.brin .brlook'); await pg.waitForSelector('.brin .brset', {timeout: 8000}).catch(() => {});
    const sets = await pg.evaluate("[...document.querySelectorAll('.brin .brset')].map(f=>f.textContent)");
    ok(`${tag} 19. Settings: Bring in files from the old computer, the copies on the drives (who, how much; the account's own name first)`, has && sets.length == 1 && /Ann <b>'s files — 3\.0 GB \(2,443 files\)/.test(sets[0]) && /Bob's files/.test(sets[0]), sets);
    await pg.evaluate("[...document.querySelectorAll('.brin .brpp')][0].click()"); await click(pg, '.brin .brgoin');
    let m = ''; for (let i = 0; i < 40 && !/delete it when/.test(m); i++) { await wait(300); m = await pg.evaluate("document.querySelector('.brin .brim').textContent") }
    ok(`${tag} 20. ...bring/import for the account, the copy and the person (into its folders, done by the helper)`, M.calls.some(c => c == `POST bring/import?acct=${encodeURIComponent(acct.id)}&dir=${encodeURIComponent('/media/sdc1/Rudolf OS - Bring your files/Old computer 2026-11-22 10.30')}&person=Ann%20_b_`), M.calls.filter(c => /import/.test(c)));
    ok(`${tag} 21. ...the antivirus checks what came in: the EICAR file deleted and said, the rest kept`, /blocked 1/.test(m) && M.calls.some(c => /^POST home\/delete\?.*x\.com\.txt/.test(c)) && !M.calls.some(c => /^POST home\/delete\?.*letter/.test(c)) && /Documents, Pictures in Pictures/.test(m), [m, M.calls.filter(c => /home\//.test(c))]);
    ok(`${tag} 22. ...and remembered: "brought in already"`, /brought in already/.test(await pg.evaluate("(document.querySelector('.brin .brlook').click(),new Promise(r=>setTimeout(()=>r(document.querySelector('.brin .brsets').textContent),900)))")));
  }
  // Standard's page
  await flow(p, 'Standard');
  // the Settings button itself: the step comes after the administrator's prompt and before "Erase?"
  reset(); M.result = {ok: true, disk: 'nvme0n1', found: false, total: 0, systems: [], locked: []};
  await p.evaluate("elevate=async()=>true;document.querySelectorAll('.brdlg').forEach(x=>x.remove());settings('upd')"); await wait(1500);
  await p.evaluate("[...document.querySelectorAll('.dsk .acr')].find(r=>/nvme0n1/.test(r.textContent)).lastChild.click()"); await wait(3000);
  const erase = await p.evaluate("[...document.querySelectorAll('h3,b,div')].some(e=>e.offsetParent&&/^Erase nvme0n1\\?$/.test(e.textContent.trim()))");
  ok('Settings 23. Install here…: looked at first (bring/scan), then "Erase nvme0n1?", nothing installed yet', M.calls.some(c => /bring\/scan/.test(c)) && erase && !M.calls.some(c => /^POST install/.test(c)), M.calls.slice(-6));
  // the offer at sign-in (Standard's page; not in an app window of the native desktop)
  reset(); await p.evaluate("BR.offered.clear();localStorage.removeItem(brDoneK());window.__nt=[];const n0=notify;notify=function(t,m,o){window.__nt.push(t+' | '+m);return n0.apply(this,arguments)};brOffer()"); await wait(1200);
  const nt = await p.evaluate('window.__nt');
  ok('Standard 24. the offer: a notification (Ann, Bob), opening Settings › Update and recovery', nt.some(x => /^Bring in your files from the old computer\? \| A copy of Windows's files is on a drive \(Ann <b>, Bob\)/.test(x)), nt);
  await p.close();
  // Native's app window: the same page alone (index.html#app=settings, the session's account)
  p = await ctx.newPage(); watch(p); await p.goto(URL0 + '#app=settings'); await p.waitForFunction(() => typeof bringStep == 'function' && typeof ONEAPP != 'undefined' && ONEAPP && USER, null, {timeout: 30000});
  await p.waitForSelector('.win.oamain', {timeout: 20000});
  await flow(p, 'Native');
  reset(); await p.evaluate("BR.offered.clear();localStorage.removeItem(brDoneK());window.__nt=[];const n0=notify;notify=function(t,m,o){window.__nt.push(t);return n0.apply(this,arguments)};brOffer()"); await wait(1200);
  ok('Native 24. no offer from an app window (the service page makes it, once)', !(await p.evaluate('window.__nt')).length);
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
