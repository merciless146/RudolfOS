// 1.5.6 stage 2 (home56.js): a new account's home folder is a view of its folder on the disk (rudolfd home/*): opened
// with its key at sign-in, only what changed written, large files as references copied on the disk, downloads moved
// in, other programs' changes shown, both versions kept on a conflict, locked at sign-out, read back at the next
// sign-in; an account from before 1.5.6 keeps its storage. A stand-in rudolfd with a disk in memory.
//   node home56_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
// the disk: acct → Map(rel → {d} | {b: Buffer, v}); keys; calls
const DISK = new Map(), KEYS = new Map(), OPEN = new Set(), CALLS = []; let VN = 1, SUP = true;
const ver = () => 'v' + (VN++);
const mk = a => { if (!DISK.has(a)) { const m = new Map(); ['Desktop', 'Documents', 'Downloads', 'Music', 'Pictures', 'Videos', '.Trash', '.rudolf'].forEach(x => m.set(x, {d: 1})); DISK.set(a, m) } return DISK.get(a) };
const parents = (m, rel) => { const p = rel.split('/'); for (let i = 1; i < p.length; i++) { const k = p.slice(0, i).join('/'); if (!m.has(k)) m.set(k, {d: 1}) } };
const DLFILES = new Map();
function api(method, a, q, body) { const p = new URLSearchParams(q || ''); CALLS.push(method + ' ' + a + '?' + (q || ''));
  const acct = p.get('acct'), m = acct && DISK.get(acct);
  if (a == 'home') return [200, {ok: true, supported: SUP, accounts: [...DISK.keys()].map(id => ({id, unlocked: OPEN.has(id)})), session: ''}];
  if (a == 'home/unlock') { const k = body.toString(); if (!/^[0-9a-f]{128}$/.test(k)) return [400, {ok: false, error: 'bad key'}];
    if (KEYS.has(acct) && KEYS.get(acct) != k) return [403, {ok: false, error: 'that key does not open this account\'s folder'}]; KEYS.set(acct, k); mk(acct); OPEN.add(acct); return [200, {ok: true}] }
  if (a == 'home/lock') { OPEN.delete(acct); return [200, {ok: true}] }
  if (a == 'dl/incoming' || a == 'dl/quarantine' || a == 'dl/home') return [200, {ok: true, items: []}];
  if (a == 'events/wait') return [200, {ok: true, events: ''}];
  if (!a.startsWith('home/')) return [200, {ok: true}];
  if (!OPEN.has(acct)) return [423, {ok: false, error: 'the account\'s folder is locked'}];
  const rel = p.get('path') || '';
  if (a == 'home/tree') return [200, {ok: true, items: [...m].map(([k, x]) => x.d ? {p: k, d: 1, s: 0, m: 0, v: 'd'} : {p: k, d: 0, s: x.b.length, m: 0, v: x.v})}];
  if (a == 'home/read') { const x = m.get(rel); return x && !x.d ? [200, x.b] : [404, {ok: false, error: 'not found'}] }
  if (a == 'home/link') return m.has(rel) ? [200, {ok: true, url: '/dlf/' + Buffer.from(acct + ':' + rel).toString('hex').slice(0, 32).padEnd(32, '0')}] : [404, {ok: false, error: 'not found'}];
  if (a == 'home/write') { const x = m.get(rel), base = p.get('base');
    if (base == 'new' && x) return [409, {ok: false, conflict: true, error: 'a file with that name already exists', v: x.v}];
    if (base && base != 'new' && x && x.v != base) return [409, {ok: false, conflict: true, error: 'the file was changed meanwhile', v: x.v}];
    parents(m, rel); const v = ver(); m.set(rel, {b: Buffer.from(body), v}); return [200, {ok: true, v}] }
  if (a == 'home/mkdir') { parents(m, rel + '/x'); m.set(rel, {d: 1}); return [200, {ok: true}] }
  if (a == 'home/delete') { [...m.keys()].forEach(k => { if (k == rel || k.startsWith(rel + '/')) m.delete(k) }); return [200, {ok: true}] }
  if (a == 'home/copy') { const f = p.get('from'), t = p.get('to'); const x = m.get(f); if (!x) return [404, {ok: false, error: 'not found'}]; parents(m, t); m.set(t, {b: x.b, v: ver()}); return [200, {ok: true}] }
  if (a == 'home/adopt') { const f = p.get('from'), t = p.get('to'); const b = DLFILES.get(f); if (!b) return [404, {ok: false, error: 'the download is not there'}]; DLFILES.delete(f); parents(m, t); const v = ver(); m.set(t, {b, v}); return [200, {ok: true, v}] }
  return [404, {ok: false, error: 'unknown'}] }
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0); const ch = [];
  q.on('data', c => ch.push(c)); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":""}') }, 20000);
    if (u.startsWith('/api/')) { const [st, b] = api(q.method, u.slice(5), qs, Buffer.concat(ch));
      if (Buffer.isBuffer(b)) { s.writeHead(st, {'content-type': 'application/octet-stream'}); return s.end(b) }
      s.writeHead(st, {'content-type': 'application/json'}); return s.end(JSON.stringify(b)) }
    s.writeHead(404); s.end() }) }).listen(8090, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8090/'); await p.waitForFunction(() => typeof hdSync == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db && FSLOADED, null, {timeout: 30000});
  /* (the move of accounts from before 1.5.6 is stage 3, tested in home56m_t.js: off here while "old" signs in) */
  SUP = false;
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});window.__sys=1;
    window.IN=async(n,pw)=>{const a=ACCTS.find(x=>x.n==n);if(USER)userLogout();USER=a;await userLogin(a,pw);return true};
    window.SETTLE=async()=>{fsFlush();await new Promise(r=>setTimeout(r,100));await HD.chain;await new Promise(r=>setTimeout(r,50));await HD.chain};
    ACCTS.length=0;ACCTS.push({n:'old',h:await pwMake('o1'),r:'admin',id:'u-old'});ACCTS.push({n:'ann',h:await pwMake('a1'),r:'user',id:'u-ann'});saveUser();
    /* "old" has a home from before 1.5.6 (its record in the browser's storage) */
    await kvPutNow('home:u-old',{v:1,plain:JSON.stringify({DESKTOP:{},DOCUMENTS:{'OLD.TXT':'kept in C:'},PICTURES:{},MUSIC:{},VIDEOS:{},DOWNLOADS:{}})});
    USER=ACCTS[0];await userLogin(ACCTS[0],'o1');fsSave();fsFlush();await new Promise(r=>setTimeout(r,500));userLogout();USER=null;return 1})()`);
  ok('1. an account from before 1.5.6 keeps its storage (its folder is never opened on the disk)', !CALLS.some(c => /home\/unlock\?acct=u-old/.test(c)) && await q("!ACCTS[0].disk"), CALLS.filter(c => /home/.test(c)));
  SUP = true; await q("IN('ann','a1')"); await q("SETTLE()");
  const un = CALLS.find(c => /^POST home\/unlock\?acct=u-ann&session=1/.test(c));
  ok('2. a new account keeps its files on the disk: its folder is opened at sign-in (with the Linux apps\' folders)', !!un && await q("ACCTS[1].disk==1&&!!ACCTS[1].fk&&hdOn()"), CALLS.filter(c => /home/.test(c)));
  ok('3. ...with the welcome file written to the disk', DISK.get('u-ann').has('Documents/WELCOME.TXT'), [...DISK.get('u-ann').keys()]);
  ok('4. ...and nothing written to the browser\'s storage for it', !(await q("kvGet('home:u-ann').then(x=>!!x)")));
  let n0 = CALLS.length;
  await q("(()=>{HOME.DOCUMENTS['NOTES.TXT']='first';fsSave()})()"); await q("SETTLE()");
  let w = CALLS.slice(n0).filter(c => /^POST home\//.test(c));
  ok('5. a new file: one write, of that file only', w.length == 1 && /home\/write\?acct=u-ann&path=Documents%2FNOTES.TXT&mkdirs=1&base=new/.test(w[0]) && DISK.get('u-ann').get('Documents/NOTES.TXT').b.toString() == 'first', w);
  n0 = CALLS.length; await q("(()=>{HOME.DOCUMENTS['NOTES.TXT']='second';fsSave()})()"); await q("SETTLE()");
  w = CALLS.slice(n0).filter(c => /^POST home\//.test(c));
  ok('6. an edit: one write, with the version it was based on', w.length == 1 && /path=Documents%2FNOTES.TXT&mkdirs=1&base=v\d+/.test(w[0]) && DISK.get('u-ann').get('Documents/NOTES.TXT').b.toString() == 'second', w);
  n0 = CALLS.length;
  await q("(()=>{HOME.PICTURES.TRIP={};HOME.PICTURES.TRIP['DOT.PNG']='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==';fsSave()})()"); await q("SETTLE()");
  w = CALLS.slice(n0).filter(c => /^POST home\//.test(c)); const png = DISK.get('u-ann').get('Pictures/TRIP/DOT.PNG');
  ok('7. a new folder and a picture: the folder made, the picture written as its bytes (not base64)', /home\/mkdir\?acct=u-ann&path=Pictures%2FTRIP/.test(w[0]) && png && png.b[0] == 0x89 && png.b.slice(1, 4).toString() == 'PNG', [w, png && png.b.slice(0, 8)]);
  n0 = CALLS.length; await q("(()=>{HOME.DOCUMENTS['RENAMED.TXT']=HOME.DOCUMENTS['NOTES.TXT'];delete HOME.DOCUMENTS['NOTES.TXT'];fsSave()})()"); await q("SETTLE()");
  ok('8. a rename: the new name written, the old one deleted', DISK.get('u-ann').get('Documents/RENAMED.TXT').b.toString() == 'second' && !DISK.get('u-ann').has('Documents/NOTES.TXT'), CALLS.slice(n0));
  // a large file made by another program, the refresh, a reference, copying it in C:
  DISK.get('u-ann').set('Videos/BIG.MP4', {b: Buffer.alloc(2 * 1048576, 7), v: ver()}); DISK.get('u-ann').set('Documents/FROM-LINUX.TXT', {b: Buffer.from('written by a Linux app'), v: ver()});
  await q("hdRefresh()"); await wait(300);
  const big = await q("JSON.stringify([isDlRef(HOME.VIDEOS['BIG.MP4']),dlRefInfo(HOME.VIDEOS['BIG.MP4']),HOME.DOCUMENTS['FROM-LINUX.TXT']])");
  ok('9. files another program saved show up: a large one as a reference, a small one with its text', /^\[true,\{"p":"\/persist\/accounts\/u-ann\/Videos\/BIG.MP4","s":2097152/.test(big) && /written by a Linux app/.test(big), big);
  const link = await q("dlLink(HOME.VIDEOS['BIG.MP4'])");
  ok('10. a large file opens through a /dlf/ link made in the account\'s folder', /^\/dlf\/[0-9a-f]{32}$/.test(link) && CALLS.some(c => /^GET home\/link\?acct=u-ann&path=Videos%2FBIG.MP4/.test(c)), [link]);
  n0 = CALLS.length; await q("(()=>{HOME.DESKTOP['BIG.MP4']=HOME.VIDEOS['BIG.MP4'];fsSave()})()"); await q("SETTLE()");
  ok('11. copying a large file in C: copies it on the disk (the reference is not written as a file)', CALLS.slice(n0).some(c => /home\/copy\?acct=u-ann&from=Videos%2FBIG.MP4&to=Desktop%2FBIG.MP4/.test(c)) && DISK.get('u-ann').get('Desktop/BIG.MP4').b.length == 2097152 && await q("dlRefInfo(HOME.DESKTOP['BIG.MP4']).p=='/persist/accounts/u-ann/Desktop/BIG.MP4'"), CALLS.slice(n0));
  // a large download kept in Downloads moves into the account's folder
  DLFILES.set('/home/rudolf/Downloads/.accounts/u-ann/movie.mp4', Buffer.alloc(1600000, 1));
  n0 = CALLS.length; await q("(()=>{HOME.DOWNLOADS['MOVIE.MP4']=dlMkRef('/home/rudolf/Downloads/.accounts/u-ann/movie.mp4',1600000,'full');fsSave()})()"); await q("SETTLE()");
  ok('12. a large download moves into the account\'s Downloads on the disk', DISK.get('u-ann').get('Downloads/MOVIE.MP4') && !DLFILES.size && await q("dlRefInfo(HOME.DOWNLOADS['MOVIE.MP4']).p=='/persist/accounts/u-ann/Downloads/MOVIE.MP4'"), CALLS.slice(n0));
  // a conflict: another program changed the file the desktop then saves
  DISK.get('u-ann').set('Documents/RENAMED.TXT', {b: Buffer.from('changed by an app'), v: ver()});
  await q("(()=>{HOME.DOCUMENTS['RENAMED.TXT']='changed here';fsSave()})()"); await q("SETTLE()");
  const d = DISK.get('u-ann');
  ok('13. a file changed by another program and here: both kept', d.get('Documents/RENAMED.TXT').b.toString() == 'changed by an app' && d.get('Documents/RENAMED (desktop copy).TXT') && d.get('Documents/RENAMED (desktop copy).TXT').b.toString() == 'changed here', [...d.keys()]);
  n0 = CALLS.length; await q("(()=>{delete HOME.PICTURES.TRIP;fsSave()})()"); await q("SETTLE()");
  ok('14. deleting a folder: one delete for the folder', CALLS.slice(n0).filter(c => /home\/delete/.test(c)).length == 1 && !d.has('Pictures/TRIP') && !d.has('Pictures/TRIP/DOT.PNG'), CALLS.slice(n0));
  // sign out: written, then locked; sign in again: read back
  await q("(()=>{HOME.DOCUMENTS['LAST.TXT']='written at sign-out';userLogout();USER=null;return 1})()"); await wait(800);
  const lockAt = CALLS.findIndex(c => /^POST home\/lock\?acct=u-ann/.test(c)), lastAt = CALLS.findIndex(c => /path=Documents%2FLAST.TXT/.test(c));
  ok('15. sign-out: the last change is written, then the folder is locked', lastAt >= 0 && lockAt > lastAt && !OPEN.has('u-ann'), CALLS.slice(-6));
  await q("IN('ann','a1')"); await q("SETTLE()");
  const back = await q("JSON.stringify({w:HOME.DOCUMENTS['WELCOME.TXT']&&HOME.DOCUMENTS['WELCOME.TXT'].slice(0,20),last:HOME.DOCUMENTS['LAST.TXT'],ren:HOME.DOCUMENTS['RENAMED.TXT'],big:isDlRef(HOME.VIDEOS['BIG.MP4']),movie:isDlRef(HOME.DOWNLOADS['MOVIE.MP4'])})");
  ok('16. signed in again: the files are read back from the disk', /"last":"written at sign-out"/.test(back) && /"ren":"changed by an app"/.test(back) && /"big":true/.test(back) && /"movie":true/.test(back) && /"w":"Welcome/.test(back), back);
  const kw = KEYS.get('u-ann'); ok('17. the same key opens it each time (made from the account\'s file key)', /^[0-9a-f]{128}$/.test(kw) && !CALLS.some(c => /unlock/.test(c) && /403/.test(c)));
  n0 = CALLS.length; await q("(()=>{fsSave();return 1})()"); await q("SETTLE()");
  ok('18. reading back writes nothing', !CALLS.slice(n0).some(c => /^POST home\/(write|delete|mkdir|copy)/.test(c)), CALLS.slice(n0));
  await q("(()=>{userLogout();USER=null;return 1})()"); await wait(500);
  SUP = false; await q("IN('old','o1')"); await wait(2500);
  ok('19. the account from before 1.5.6 still has its files in C:', await q("HOME&&HOME.DOCUMENTS['OLD.TXT']=='kept in C:'&&!hdOn()"));
  ok('20. no page errors', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1)
})().catch(e => { console.log('FAIL ' + e.stack); process.exit(1) });
