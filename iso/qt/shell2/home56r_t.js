// 1.5.6 stage 4 (home56.js): with the home on the disk, the Trash still gives back large files (they wait in the
// account's .Trash, emptied with the Trash); a backup carries the home; a backup restored on a computer where the
// account's folder is new fills it; an administrator opens, changes and closes another account's home on the disk.
// A stand-in rudolfd with a disk in memory (as home56_t.js).   node home56r_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
// the disk: acct → Map(rel → {d} | {b: Buffer, v}); keys; calls
const DISK = new Map(), KEYS = new Map(), OPEN = new Set(), CALLS = []; let VN = 1, SUP = true, SPOIL = null;
const ver = () => 'v' + (VN++);
const mk = a => { if (!DISK.has(a)) { const m = new Map(); ['Desktop', 'Documents', 'Downloads', 'Music', 'Pictures', 'Videos', '.Trash', '.rudolf'].forEach(x => m.set(x, {d: 1})); DISK.set(a, m) } return DISK.get(a) };
const parents = (m, rel) => { const p = rel.split('/'); for (let i = 1; i < p.length; i++) { const k = p.slice(0, i).join('/'); if (!m.has(k)) m.set(k, {d: 1}) } };
const DLFILES = new Map();
function api(method, a, q, body) { const p = new URLSearchParams(q || ''); CALLS.push(method + ' ' + a + '?' + (q || ''));
  const acct = p.get('acct'), m = acct && DISK.get(acct);
  if (a == 'home') return [200, {ok: true, supported: SUP, accounts: [...DISK.keys()].map(id => ({id, unlocked: OPEN.has(id)})), session: ''}];
  if (a == 'home/unlock') { const k = body.toString(); if (!/^[0-9a-f]{128}$/.test(k)) return [400, {ok: false, error: 'bad key'}];
    if (KEYS.has(acct) && KEYS.get(acct) != k) return [403, {ok: false, error: 'that key does not open this account\'s folder'}]; KEYS.set(acct, k); const made = !DISK.has(acct); mk(acct); OPEN.add(acct); return [200, {ok: true, message: made ? 'made' : 'unlocked'}] }
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
    parents(m, rel); const v = ver(); m.set(rel, {b: SPOIL && rel == SPOIL ? Buffer.from('damaged') : Buffer.from(body), v}); if (rel == SPOIL) SPOIL = null; return [200, {ok: true, v}] }
  if (a == 'home/mkdir') { parents(m, rel + '/x'); m.set(rel, {d: 1}); return [200, {ok: true}] }
  if (a == 'home/list') return [200, {ok: true, items: [...m].filter(([k]) => k.startsWith(rel + '/') && !k.slice(rel.length + 1).includes('/')).map(([k, x]) => ({p: k, d: x.d ? 1 : 0, s: x.d ? 0 : x.b.length, v: x.v || 'd'}))}];
  if (a == 'home/rename') { const f = p.get('from'), t = p.get('to'); if (!m.has(f)) return [404, {ok: false, error: 'not found'}];
    if (m.has(t) && p.get('replace') != '1') return [409, {ok: false, error: 'something with that name is already there'}]; [...m.keys()].forEach(k => { if (k == t || k.startsWith(t + '/')) m.delete(k) });
    parents(m, t); [...m].forEach(([k, x]) => { if (k == f || k.startsWith(f + '/')) { m.delete(k); m.set(t + k.slice(f.length), x) } }); return [200, {ok: true}] }
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
    s.writeHead(404); s.end() }) }).listen(8088, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const ctx = await b.newContext(); let p = await ctx.newPage();
  const errs = []; const watch = pg => pg.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) }); watch(p);
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8088/'); await p.waitForFunction(() => typeof hdTrashSweep == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db && FSLOADED, null, {timeout: 30000});
  const SETUP = `(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});window.__sys=1;
    window.IN=async(n,pw)=>{const a=ACCTS.find(x=>x.n==n);if(USER)userLogout();USER=a;await userLogin(a,pw);return true};
    window.SETTLE=async()=>{fsFlush();await new Promise(r=>setTimeout(r,100));await HD.chain;await new Promise(r=>setTimeout(r,50));await HD.chain};
    window.MOVED=async()=>{for(let i=0;i<80;i++){if(!HD.moving&&USER&&USER.disk&&hdOn())return true;await new Promise(r=>setTimeout(r,250))}return false};return 1})()`;
  await q(SETUP);
  await q("(async()=>{ACCTS.length=0;ACCTS.push({n:'boss',h:await pwMake('b1'),r:'admin',id:'u-boss'});ACCTS.push({n:'kid',h:await pwMake('k1'),r:'user',id:'u-kid'});saveUser();return 1})()");
  // kid's home on the disk; boss (admin) gets kid's key copies at boss's sign-in after kid's first sign-in
  await q("IN('boss','b1')"); await q("SETTLE()"); await q("(()=>{userLogout();USER=null;return 1})()"); await wait(500);   /* (the administrator's key pair, so kid's key gets a copy for it) */
  await q("IN('kid','k1')"); await q("SETTLE()");
  DISK.get('u-kid').set('Videos/HOLIDAY.MP4', {b: Buffer.alloc(2000000, 5), v: ver()});
  await q("hdRefresh()"); await wait(300);
  // the Trash with a large file
  await q("(()=>{trashPut([homePath('VIDEOS').concat('HOLIDAY.MP4')]);return 1})()"); await q("SETTLE()");
  const k = DISK.get('u-kid'), inTrash = [...k.keys()].find(x => /^\.Trash\/[^/]+\/HOLIDAY\.MP4$/.test(x));
  ok('1. a large file put in the Trash waits in the account\'s .Trash on the disk (not deleted)', !!inTrash && !k.has('Videos/HOLIDAY.MP4') && k.get(inTrash).b.length == 2000000, [...k.keys()]);
  ok('2. ...and the Trash\'s entry points there', await q(`dlRefInfo(TR.find(e=>e.n=='HOLIDAY.MP4').v).p=='/persist/accounts/u-kid/${inTrash}'`));
  await q("(()=>{trRestore(TR.findIndex(e=>e.n=='HOLIDAY.MP4'));return 1})()"); await q("SETTLE()");
  ok('3. Restore moves it back (a rename, not a copy)', k.has('Videos/HOLIDAY.MP4') && !k.has(inTrash) && k.get('Videos/HOLIDAY.MP4').b.length == 2000000 && CALLS.some(c => /home\/rename\?acct=u-kid&from=\.Trash/.test(c)), [...k.keys()]);
  await q("(()=>{trashPut([homePath('VIDEOS').concat('HOLIDAY.MP4')]);return 1})()"); await q("SETTLE()");
  await q("(()=>{TR.length=0;trSave();return 1})()"); await wait(3500); await q("HD.chain");
  ok('4. emptying the Trash deletes it from the disk', ![...k.keys()].some(x => /HOLIDAY/.test(x)), [...k.keys()]);
  await q("(()=>{HOME.DOCUMENTS['NOTE.TXT']='hello';fsSave();return 1})()"); await q("SETTLE()");
  await q("(()=>{trashPut([homePath('DOCUMENTS').concat('NOTE.TXT')]);return 1})()"); await q("SETTLE()");
  ok('5. a small file put in the Trash is simply deleted on the disk (the Trash keeps it, as before)', !k.has('Documents/NOTE.TXT') && ![...k.keys()].some(x => /NOTE/.test(x)) && await q("TR.some(e=>e.n=='NOTE.TXT'&&e.v=='hello')"));
  // a backup carries the home
  await q("(async()=>{await backupBlob();return 1})()");
  ok('6. a backup carries the home (its record, encrypted with the file key)', await q("kvGet('home:u-kid').then(x=>!!(x&&x.ct&&x.k=='f'))"));
  // this account on a computer where its folder is new (a restored backup): filled from the record
  await q("(()=>{userLogout();USER=null;return 1})()"); await wait(500);
  DISK.delete('u-kid'); KEYS.delete('u-kid');
  await q("IN('kid','k1')"); const moved = await q("MOVED()");
  ok('7. a restored backup on a computer where the folder is new: its files are moved in', moved && DISK.get('u-kid') && DISK.get('u-kid').has('Documents/WELCOME.TXT'), DISK.get('u-kid') && [...DISK.get('u-kid').keys()]);
  await q("(()=>{HOME.DOCUMENTS['KID.TXT']='mine';fsSave();userLogout();USER=null;return 1})()"); await wait(800);
  // the administrator opens kid's home on the disk
  await q("IN('boss','b1')"); await q("SETTLE()"); await wait(800);
  const other = await q("JSON.stringify({open:!!AC_OTHERS.get('u-kid'),disk:AC_OTHERS.get('u-kid')&&AC_OTHERS.get('u-kid').disk,f:fs['C:'].USERS[acUname(ACCTS[1])]&&fs['C:'].USERS[acUname(ACCTS[1])].DOCUMENTS['KID.TXT']})");
  ok('8. an administrator opens another account\'s home on the disk', /"open":true,"disk":1,"f":"mine"/.test(other), other);
  const n0 = CALLS.length; await q("(()=>{fs['C:'].USERS[acUname(ACCTS[1])].DOCUMENTS['FROM-BOSS.TXT']='hi';fsSave();return 1})()"); await q("SETTLE()"); await wait(300); await q("HD.chain");
  ok('9. ...a change there is written to that account\'s folder', DISK.get('u-kid').get('Documents/FROM-BOSS.TXT') && DISK.get('u-kid').get('Documents/FROM-BOSS.TXT').b.toString() == 'hi', CALLS.slice(n0).filter(c => /home/.test(c)));
  await q("(()=>{userLogout();USER=null;return 1})()"); await wait(800);
  ok('10. ...and it is locked again when the administrator signs out', !OPEN.has('u-kid') && !OPEN.has('u-boss'));
  ok('11. no page errors', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1)
})().catch(e => { console.log('FAIL ' + e.stack); process.exit(1) });
