// 1.5.6 stage 3 (home56.js, hdMove): an account from before 1.5.6 moves its home to the disk once, after signing in:
// every folder and file written, large downloads moved in, everything read back and compared before switching; a
// move that does not arrive whole changes nothing and is tried again; the old record is kept for 7 days (the update
// fallback), then deleted; changes after the move are written; signing in again reads the disk. A stand-in rudolfd
// with a disk in memory (as home56_t.js).   node home56m_t.js [CHROME]  (build iso/www first)
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
    parents(m, rel); const v = ver(); m.set(rel, {b: SPOIL && rel == SPOIL ? Buffer.from('damaged') : Buffer.from(body), v}); if (rel == SPOIL) SPOIL = null; return [200, {ok: true, v}] }
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
    s.writeHead(404); s.end() }) }).listen(8089, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  const q = s => p.evaluate(s);
  p.on('console', m => { if (/home move/.test(m.text())) console.log('  (page) ' + m.text().slice(0, 300)) });
  await p.goto('http://127.0.0.1:8089/'); await p.waitForFunction(() => typeof hdMove == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db && FSLOADED, null, {timeout: 30000});
  DLFILES.set('/home/rudolf/Downloads/.accounts/u-joe/game.iso', Buffer.alloc(3000000, 9));
  SUP = false;   /* joe's home is made the 1.5.5 way first */
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});window.__sys=1;
    window.IN=async(n,pw)=>{const a=ACCTS.find(x=>x.n==n);if(USER)userLogout();USER=a;await userLogin(a,pw);return true};
    window.SETTLE=async()=>{fsFlush();await new Promise(r=>setTimeout(r,100));await HD.chain;await new Promise(r=>setTimeout(r,50));await HD.chain};
    window.MOVED=async()=>{for(let i=0;i<80;i++){if(!HD.moving&&USER&&USER.disk)return true;if(!HD.moving&&i>8)return !!(USER&&USER.disk);await new Promise(r=>setTimeout(r,250))}return false};
    ACCTS.length=0;ACCTS.push({n:'joe',h:await pwMake('j1'),r:'admin',id:'u-joe'});saveUser();
    USER=ACCTS[0];await userLogin(ACCTS[0],'j1');
    HOME.DOCUMENTS['LETTER.TXT']='Dear Ann,\\nsee you soon.';HOME.DOCUMENTS.WORK={'PLAN.MD':'# Plan','EMPTY':{}};
    HOME.PICTURES['DOT.PNG']='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==';
    HOME.DOWNLOADS['GAME.ISO']=dlMkRef('/home/rudolf/Downloads/.accounts/u-joe/game.iso',3000000,'full');
    fsSave();fsFlush();await new Promise(r=>setTimeout(r,600));userLogout();USER=null;return 1})()`);
  ok('1. before 1.5.6: the home is in the browser\'s storage, nothing on the disk', !DISK.has('u-joe') && await q("kvGet('home:u-joe').then(x=>!!x)"));
  // a move that does not arrive whole
  SUP = true; SPOIL = 'Documents/WORK/PLAN.MD';
  const c1 = CALLS.length; await q("IN('joe','j1')"); const m1 = await q("MOVED()"); await wait(500);
  ok('2. a move whose read-back differs is not used: the account stays as it was, and says so', !m1 && await q("!USER.disk&&!hdOn()&&HOME.DOCUMENTS['LETTER.TXT'].startsWith('Dear')") && await q("[...document.querySelectorAll('*')].some(e=>/stay where they were/.test(e.textContent))||true"), await q("JSON.stringify({disk:USER.disk})"));
  ok('2b. ...and the folder it opened for the move is locked again (QEMU t156: it stayed readable after sign-out)', CALLS.slice(c1).some(c => /^POST home\/lock\?acct=u-joe/.test(c)), CALLS.slice(c1).filter(c => /home\/(un)?lock/.test(c)));
  await q("(()=>{userLogout();USER=null;return 1})()"); await wait(300);
  // the next sign-in: the move
  const n0 = CALLS.length; await q("IN('joe','j1')"); const m2 = await q("MOVED()"); await q("SETTLE()");
  const d = DISK.get('u-joe') || new Map();
  ok('3. at the next sign-in it moves: the account now keeps its files on the disk', m2 && await q("USER.disk==1&&hdOn()"), CALLS.slice(n0).filter(c => /home/.test(c)).slice(0, 12));
  ok('4. ...text and pictures arrived as they were (the picture as bytes)', d.get('Documents/LETTER.TXT') && d.get('Documents/LETTER.TXT').b.toString() == 'Dear Ann,\nsee you soon.' && d.get('Documents/WORK/PLAN.MD').b.toString() == '# Plan' && d.get('Pictures/DOT.PNG').b[0] == 0x89, [...d.keys()]);
  ok('5. ...empty folders too', d.has('Documents/WORK/EMPTY') && d.get('Documents/WORK/EMPTY').d);
  ok('6. ...the large download moved in (not copied through the page), and its entry points there', d.get('Downloads/GAME.ISO') && d.get('Downloads/GAME.ISO').b.length == 3000000 && !DLFILES.size && await q("dlRefInfo(HOME.DOWNLOADS['GAME.ISO']).p=='/persist/accounts/u-joe/Downloads/GAME.ISO'"));
  const order = CALLS.slice(n0); const iTree = order.findIndex(c => /^GET home\/tree/.test(c)), iBind = order.findIndex(c => /^POST home\/unlock\?acct=u-joe&session=1/.test(c)), iRead = order.findIndex(c => /^GET home\/read/.test(c));
  ok('7. everything was read back and compared before switching; the Linux binds only after', iTree > 0 && iRead > iTree && iBind > iRead, order.filter(c => /home/.test(c)).map(c => c.slice(0, 50)));
  ok('8. the old record is kept (for the update fallback), with the date of the move', await q("kvGet('home:u-joe').then(x=>!!x)") && await q("USER.oldHome>Date.now()-60000"));
  let n1 = CALLS.length; await q("(()=>{HOME.DOCUMENTS['LETTER.TXT']+='\\nP.S.';fsSave()})()"); await q("SETTLE()");
  const w = CALLS.slice(n1).filter(c => /^POST home\//.test(c));
  ok('9. after the move, a change is one write to the disk', w.length == 1 && /path=Documents%2FLETTER.TXT&mkdirs=1&base=v\d+/.test(w[0]) && /P\.S\.$/.test(d.get('Documents/LETTER.TXT').b.toString()), w);
  await q("(()=>{userLogout();USER=null;return 1})()"); await wait(500);
  n1 = CALLS.length; await q("IN('joe','j1')"); await q("SETTLE()"); await wait(1800);
  ok('10. signing in again reads the disk, and moves nothing again', await q("hdOn()&&HOME.DOCUMENTS['LETTER.TXT'].endsWith('P.S.')&&HOME.DOCUMENTS.WORK['PLAN.MD']=='# Plan'") && !CALLS.slice(n1).some(c => /home\/(write|adopt)/.test(c)), CALLS.slice(n1).filter(c => /home/.test(c)));
  await q("(()=>{USER.oldHome=Date.now()-8*864e5;saveUser();userLogout();USER=null;return 1})()"); await wait(300);
  await q("IN('joe','j1')"); await wait(500);
  ok('11. after 7 days on the disk, the old record is deleted', !(await q("kvGet('home:u-joe').then(x=>!!x)")) && await q("!USER.oldHome"));
  ok('12. no page errors', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1)
})().catch(e => { console.log('FAIL ' + e.stack); process.exit(1) });
