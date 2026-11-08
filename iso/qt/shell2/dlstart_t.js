// 1.5.5 (dl27.js): the downloads' checks start by themselves after start-up, with nothing downloaded in this session:
// on a computer with one account, C:\DOWNLOADS from before 1.5.5 moves into the account's own Downloads (with its
// large files) shortly after signing in, as it should have on the Vivobook. A stand-in rudolfd; the page is loaded
// as on a restart (the accounts loaded by the page itself, window.__sys set after dl27.js ran).
//   node dlstart_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const DL = '/home/rudolf/Downloads', CALLS = [];
const api = (a, q) => { const p = new URLSearchParams(q || ''); CALLS.push(a + '?' + (q || ''));
  if (a == 'dl/incoming' || a == 'dl/quarantine' || a == 'dl/home') return {ok: true, items: []};
  if (a == 'dl/file' && p.get('to') == 'acct') { const n = p.get('name'); return {ok: true, path: DL + '/.accounts/' + p.get('acct') + '/' + n, name: n, size: 9} }
  if (a == 'dl/space') return {ok: true, free: 50e9, total: 100e9};
  if (a == 'events/wait') return {ok: true, events: ''};
  return {ok: true} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":""}') }, 20000);
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs))) }
    s.writeHead(404); s.end() }) }).listen(8092, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const ctx = await b.newContext(); let p = await ctx.newPage();
  const errs = []; const watch = pg => pg.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  watch(p);
  await p.goto('http://127.0.0.1:8092/'); await p.waitForFunction(() => typeof dlMigrate == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  // before 1.5.5: one account, C:\DOWNLOADS with a small file and a large one on the disk; saved, then a restart
  await p.evaluate(`(async()=>{try{svj('bd95setup',1)}catch(e){}ACCTS.length=0;ACCTS.push({n:'john',h:await pwMake('j1'),r:'admin'});saveUser();
    fs['C:'].DOWNLOADS={'NOTES.TXT':'hello','STARPORTGE_INSTALL.EXE':dlMkRef('${DL}/StarportGE_install.exe',60018940,'full')};fsSave();await new Promise(r=>setTimeout(r,800));fsFlush&&fsFlush();return 1})()`);
  await wait(1500); await p.close(); p = await ctx.newPage(); watch(p);
  await p.goto('http://127.0.0.1:8092/'); await p.waitForFunction(() => typeof dlMigrate == 'function' && typeof pwCheck == 'function' && ACCTS.length && FSLOADED && fs['C:'] && isDir(fs['C:'].DOWNLOADS), null, {timeout: 30000});
  const before = await p.evaluate("JSON.stringify(Object.keys(fs['C:'].DOWNLOADS||{}))");
  ok('1. after the restart, C:\\DOWNLOADS from before is there (nothing downloaded in this session)', /NOTES\.TXT/.test(before), before);
  // sign in as the sign-in screen does; no download, no call to dlPoll from here
  await p.evaluate("(async()=>{['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});const a=ACCTS.find(x=>x.n=='john');if(USER)userLogout();USER=a;await userLogin(a,'j1');return 1})()");
  const end = Date.now() + 20000; let moved = false;
  while (Date.now() < end && !(moved = await p.evaluate("!isDir(fs['C:'].DOWNLOADS)&&!!node(downloadsPath())&&'NOTES.TXT' in node(downloadsPath())"))) await wait(500);
  ok('2. the checks started by themselves and moved it into the account\'s Downloads', moved, await p.evaluate("JSON.stringify({C:Object.keys(fs['C:']),D:node(downloadsPath())&&Object.keys(node(downloadsPath()))})"));
  const ref = await p.evaluate("dlRefInfo(node(downloadsPath())['STARPORTGE_INSTALL.EXE'])");
  ok('3. ...its large file moved into the account\'s folder on the disk', ref && /\/\.accounts\/[a-z0-9-]+\/StarportGE_install\.exe$/.test(ref.p) && CALLS.some(c => /dl\/file\?name=StarportGE_install\.exe&from=root&to=acct&acct=/.test(c)), [ref, CALLS.filter(c => /to=acct/.test(c))]);
  ok('4. the incoming folder is being watched (dl/incoming asked for)', CALLS.some(c => c.startsWith('dl/incoming')));
  ok('5. no page errors', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1)
})().catch(e => { console.log('FAIL ' + e.stack); process.exit(1) });
