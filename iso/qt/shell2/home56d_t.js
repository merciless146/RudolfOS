// 1.5.6 (home56.js, linux26.js, dl27.js): Downloads joins the binds for Linux apps. With rudolfd's downloadsBound,
// the copies lxSync used to keep at the top of the downloads store go (only identical ones), what Linux apps saved
// there moves into the account's Downloads, lxSync copies nothing, and Linux apps get /home/rudolf/<Folder>/... paths
// for files in the account's folder. Without downloadsBound only Downloads is still kept in step.
// A stand-in rudolfd with a disk in memory.   node home56d_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const DISK = new Map(), CALLS = []; let VN = 1, SESSION = '', DLB = true;
const TOP = new Map();   // the top of the downloads store: name -> Buffer
const ver = () => 'v' + (VN++);
const mk = a => { if (!DISK.has(a)) { const m = new Map(); ['Desktop', 'Documents', 'Downloads', 'Music', 'Pictures', 'Videos', '.Trash', '.rudolf'].forEach(x => m.set(x, {d: 1})); DISK.set(a, m) } return DISK.get(a) };
function api(method, a, q, body) { const p = new URLSearchParams(q || ''); CALLS.push(method + ' ' + a + '?' + (q || ''));
  const acct = p.get('acct'), m = acct && DISK.get(acct);
  if (a == 'home') return [200, {ok: true, supported: true, accounts: [...DISK.keys()].map(id => ({id, unlocked: true})), session: SESSION, downloadsBound: DLB && !!SESSION}];
  if (a == 'home/unlock') { mk(acct); if (p.get('session') == '1') SESSION = acct; return [200, {ok: true, message: 'unlocked'}] }
  if (a == 'home/lock') { if (SESSION == acct) SESSION = ''; return [200, {ok: true}] }
  if (a == 'dl/home') return [200, {ok: true, items: p.get('acct') ? [] : [...TOP].map(([name, b]) => ({name, size: b.length, mtime: 1, partial: /\.crdownload$/.test(name)}))}];
  if (a == 'dl/read') { const b = TOP.get(p.get('name')); return b ? [200, b] : [404, {ok: false, error: 'not there'}] }
  if (a == 'dl/file') { if (p.get('from') == 'root' && p.get('to') == 'del') { TOP.delete(p.get('name')); return [200, {ok: true}] } return [400, {ok: false, error: 'to?'}] }
  if (a == 'dl/incoming' || a == 'dl/quarantine') return [200, {ok: true, items: []}];
  if (a.startsWith('fs/')) return [200, {ok: true, items: []}];
  if (a == 'events/wait') return [200, {ok: true, events: ''}];
  if (!a.startsWith('home/')) return [200, {ok: true}];
  const rel = p.get('path') || '';
  if (a == 'home/tree') return [200, {ok: true, items: [...m].map(([k, x]) => x.d ? {p: k, d: 1, s: 0, m: 0, v: 'd'} : {p: k, d: 0, s: x.b.length, m: 0, v: x.v})}];
  if (a == 'home/read') { const x = m.get(rel); return x && !x.d ? [200, x.b] : [404, {ok: false, error: 'not found'}] }
  if (a == 'home/write') { const v = ver(); m.set(rel, {b: Buffer.from(body), v}); return [200, {ok: true, v}] }
  if (a == 'home/adopt') { const f = p.get('from'), t = p.get('to'); const n = f.replace(/^\/home\/rudolf\/Downloads\//, '');
    if (n.includes('/') || !TOP.has(n)) return [404, {ok: false, error: 'the download is not there'}];
    const b = TOP.get(n); TOP.delete(n); const v = ver(); m.set(t, {b, v}); return [200, {ok: true, v}] }
  return [200, {ok: true}] }
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0); const ch = [];
  q.on('data', c => ch.push(c)); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":""}') }, 20000);
    if (u.startsWith('/api/')) { const [st, b] = api(q.method, u.slice(5), qs, Buffer.concat(ch));
      if (Buffer.isBuffer(b)) { s.writeHead(st, {'content-type': 'application/octet-stream'}); return s.end(b) }
      s.writeHead(st, {'content-type': 'application/json'}); return s.end(JSON.stringify(b)) }
    s.writeHead(404); s.end() }) }).listen(8140, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8140/'); await p.waitForFunction(() => typeof hdDlTop == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db && FSLOADED, null, {timeout: 30000});
  // ann's folder on the disk, and the top of the store as 1.5.6's first beta left it (lxSync's copies, a Linux app's file)
  const m = mk('u-ann'); m.set('Downloads/NOTE.TXT', {b: Buffer.from('hello'), v: ver()}); m.set('Downloads/DIFF.TXT', {b: Buffer.from('mine'), v: ver()});
  TOP.set('NOTE.TXT', Buffer.from('hello')); TOP.set('DIFF.TXT', Buffer.from('theirs')); TOP.set('rudolf-os-1.4.1-boot.iso', Buffer.from('ISO')); TOP.set('Unconfirmed 1.crdownload', Buffer.from('part'));
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});window.__sys=1;
    window.IN=async(n,pw)=>{const a=ACCTS.find(x=>x.n==n);if(USER)userLogout();USER=a;await userLogin(a,pw);return true};
    ACCTS.length=0;ACCTS.push({n:'ann',h:await pwMake('a1'),r:'admin',id:'u-ann',disk:1});ACCTS.push({n:'bob',h:await pwMake('b1'),r:'user',id:'u-bob',disk:1});saveUser();return 1})()`);
  await q("IN('ann','a1')"); await wait(800);
  ok('1. signed in on the disk: Downloads is among the binds (HD.dlb)', await q("hdOn()&&HD.dlb===true"), await q("({on:hdOn(),dlb:HD.dlb})"));
  await p.waitForFunction(() => HD.dltop, null, {timeout: 10000}).catch(() => {}); await wait(1500);
  const top = [...TOP.keys()].sort(), dl = [...m.keys()].filter(k => k.startsWith('Downloads/')).sort();
  ok('2. the identical copy at the top of the store goes; the different DIFF.TXT and the Linux app\'s ISO move in; the partial download stays',
    top.join() == 'Unconfirmed 1.crdownload' && dl.join() == 'Downloads/DIFF (2).TXT,Downloads/DIFF.TXT,Downloads/NOTE.TXT,Downloads/rudolf-os-1.4.1-boot.iso' && m.get('Downloads/DIFF.TXT').b.toString() == 'mine', [top, dl]);
  const c = await q("Object.keys(node(downloadsPath())).sort().join()");
  ok('3. ...and File Explorer shows them in Downloads', /DIFF \(2\)\.TXT/.test(c) && /rudolf-os-1\.4\.1-boot\.iso/i.test(c), c);
  // lxSync copies nothing: every shared folder is the account's own
  await q("LX.inst=[{id:'x'}];svj(LXK,[{id:'x'}]);svj('bd95lxowner',uid())"); CALLS.length = 0;
  const n = await q("lxSync()");
  ok('4. lxSync copies nothing while Downloads is bound', n === 0 && !CALLS.some(x => /fs\/(tree|write|read)/.test(x)), CALLS.filter(x => /fs\//.test(x)));
  ok('5. a file in the account\'s folder is given to Linux apps by the name they see', await q("dlDiskPath({p:'/persist/accounts/u-ann/Downloads/A.ISO'})=='/home/rudolf/Downloads/A.ISO'&&dlDiskPath({p:'/persist/accounts/u-ann/Documents/X/b.pdf'})=='/home/rudolf/Documents/X/b.pdf'&&dlDiskPath({p:'/persist/accounts/u-ann/Desktop/c'})=='/persist/accounts/u-ann/Desktop/c'"));
  // without the bind (rudolfd could not bind the store): only Downloads is still kept in step, nothing is taken in
  DLB = false; TOP.set('LATER.TXT', Buffer.from('x'));
  await q("(async()=>{HD.dltop=0;await hdBinds();return 1})()"); await wait(4800);
  ok('6. without downloadsBound: no clean-up of the store\'s top', await q("HD.dlb===false") && TOP.has('LATER.TXT'));
  CALLS.length = 0; await q("lxSync()");
  const trees = CALLS.filter(x => /fs\/tree/.test(x)).map(x => decodeURIComponent(x));
  ok('7. ...and lxSync keeps only Downloads in step (Documents and Pictures are bound)', trees.length == 1 && /\/home\/rudolf\/Downloads/.test(trees[0]), trees);
  // another account than the one Linux apps' Downloads belonged to: left alone
  DLB = true; await q("(async()=>{svj('bd95lxowner','u-bob');HD.dltop=0;HD.dlb=true;return JSON.stringify(await hdDlTop())})()").then(r => ok('8. the store\'s top is taken in only by the account that owned it before', /owner/.test(r) && TOP.has('LATER.TXT'), r));
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
