// 1.6.2 (docs/plan-1.6.2.md Part 40, the owner's Vivobook on 2026-11-23: Starport GE's icon did nothing): the message of a program
// that did not start (rudolf-debian's "lxfail" portal request) and the offer to bring the Windows programs from before 1.5.9 into the
// account (wineold62.js), with a stand-in rudolfd, on each engine: Chromium's and Lite's desktop page (lxPortalPoll), and the native
// desktop's hidden page (#app=do-bg, handed the request by the shell as rudolf-native does: __oaPortal), whose dialog asks the
// shell to show it (op "modal"). Also: a Windows program's message with the offer, Bring them in with its progress, several
// accounts (an administrator's name and password sent), too little room, a program that is not a Windows one, Settings ›
// Apps › Linux, System health, the notice once.     node wineold162_t.js [CHROME]      (WO_PORT, default 8700)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www'), PORT = +process.env.WO_PORT || 8700;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 700) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.html': 'text/html', '.webp': 'image/webp', '.jpg': 'image/jpeg', '.png': 'image/png', '.woff2': 'font/woff2', '.css': 'text/css'};
const FAIL = {kind: 'lxfail', app: 'starport-ge', name: 'Starport GE', status: 2, secs: 0.3, wine: true, dir: '/home/rudolf/.wine/drive_c/users/rudolf/StarportGE', log: '/run/rudolf/debian/starport-ge.log',
  lines: ["/usr/local/bin/starport-ge: 1: cd: can't cd to /home/rudolf/.wine/drive_c/users/rudolf/StarportGE"],
  diag: ['# Wine', 'wine-10.0 (Debian 10.0~rc4)', '# the renderer', 'llvmpipe (LLVM 19.1.7, 256 bits)', '# /dev/dri', 'no /dev/dri'],
  diaglog: '/run/rudolf/debian/starport-ge.diag.txt'};
const WO0 = () => ({ok: true, on: true, offer: true, pending: true, declined: false, several: false, accounts: 1, count: 2, launchers: 1, bytes: 1288490188, free: 40e9, room: true, mode: 'replace', own: false, cut: false, brought: false, running: false, programs: ['StarportGE', 'Notepad2'], job: null});
const M = {calls: [], reqs: [], wo: WO0(), body: null, session: null, engine: null};
const j = (s, o, c) => { s.writeHead(c || 200, {'content-type': 'application/json'}); s.end(JSON.stringify(o)) };
const answer = (a, qs, body, s) => {
  if (a == 'portal/poll') return j(s, {ok: true, requests: M.reqs});
  if (a == 'portal/answer') { const id = new URLSearchParams(qs).get('id'); M.reqs = M.reqs.filter(r => r.id != id); return j(s, {ok: true}) }
  if (a == 'wine/old') { const o = Object.assign({}, M.wo); if (new URLSearchParams(qs).get('dir')) o.inOld = new URLSearchParams(qs).get('dir') == FAIL.dir; return j(s, o) }
  if (a == 'wine/bring') { M.body = body; if (M.wo.room === false) return j(s, {ok: false, error: 'not enough room'}, 507);
    M.wo.running = true; M.wo.pending = false; let k = 0; M.wo.job = {id: 'uAnn', running: true, phase: 'copy', copied: 0, total: 1288490188, files: 0, mode: M.wo.mode, own: '', error: '', alive: true};
    const iv = setInterval(() => { k++; M.wo.job = Object.assign({}, M.wo.job, {copied: Math.min(1288490188, k * 400e6), files: k * 300});
      if (k >= 4) { clearInterval(iv); M.wo = Object.assign(WO0(), {offer: false, pending: false, count: 0, launchers: 0, programs: [], job: Object.assign({}, M.wo.job, {running: false, alive: false, phase: 'done', own: '.wine.own-20261123-101500'})}) } }, 700);
    return j(s, {ok: true, started: true, mode: M.wo.mode}) }
  if (a == 'wine/decline') { M.wo.declined = true; M.wo.pending = false; return j(s, {ok: true}) }
  if (a == 'debian/status') return j(s, {ok: true, installed: 1, version: '13.7', running: 1, wine: 1});
  if (a == 'debian/apps') return j(s, {ok: true, apps: [{id: 'starport-ge', name: 'Starport GE', menu: true, icon: '', mime: '', categories: '', wmclass: ''}]});
  if (a == 'apps/installed') return j(s, {ok: true, apps: []});
  if (a == 'pkg/status') return j(s, {ok: true, installed: [], available: [], sources: []});
  if (a == 'accounts/users') return j(s, {ok: true, on: true, users: [{id: 'u1', uid: 1001, user: 'acct1001', moved: true, signedIn: true}], moveFailed: ''});
  if (a == 'health') return j(s, {ok: true, events: []});
  return j(s, {ok: true, items: [], apps: [], events: []}) };
const LS = "window.__rudolfLsFlush=()=>fetch('/api/lsync',{method:'POST',headers:{'X-Rudolf-Token':'t'},body:'{}'});";
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0); const bs = [];
  q.on('data', d => bs.push(d)); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0' + (M.engine ? ',engine:"' + M.engine + '"' : '') + '};' + LS + '</script>')) }
    if (u.startsWith('/api/')) { const a = u.slice(5); M.calls.push(q.method + ' ' + a + (qs ? '?' + qs : ''));
      if (a == 'events/wait') return setTimeout(() => j(s, {ok: true, events: []}), 20000);
      if (a == 'desktop/session') return M.session ? j(s, M.session) : j(s, {ok: false, error: 'no native session'}, 404);
      return answer(a, qs || '', Buffer.concat(bs).toString(), s) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(PORT, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms)); const NOW = () => Math.floor(Date.now() / 1000);
const URL0 = 'http://127.0.0.1:' + PORT + '/';
const NAT = "window.webkit={messageHandlers:{rudolfNative:{postMessage:m=>{(window.__msgs=window.__msgs||[]).push(JSON.parse(JSON.stringify(m)))}}}};";
const until = async (p, f, ms) => { for (let i = 0; i < (ms || 8000) / 100; i++) { try { if (await p.evaluate(f)) return true } catch (e) {} await wait(100) } return false };
const txt = (p, sel) => p.evaluate(s => { const e = document.querySelector(s); return e ? e.textContent : null }, sel);
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const ctx = await b.newContext({viewport: {width: 1200, height: 800}});
  const errs = []; const watch = p => p.on('pageerror', e => { if (/wo[A-Z]|wineold|lxPortal|oaPortal/.test(e.stack || '')) errs.push(e.message) });
  let p = await ctx.newPage(); watch(p);
  await p.goto(URL0); await p.waitForFunction(() => typeof woFail == 'function' && typeof acctNew == 'function' && typeof FSLOADED != 'undefined' && FSLOADED, null, {timeout: 40000});
  const acct = await p.evaluate(`(async()=>{svj('bd95setup',1);['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    const a=await acctNew('Ann','pw-one','admin');ACCTS=[a];window.__sys=1;saveUser();USER=a;await userLogin(a,'pw-one');fsSave();fsFlush();await new Promise(r=>setTimeout(r,800));
    const hex=u=>[...new Uint8Array(u)].map(x=>x.toString(16).padStart(2,'0')).join('');
    return {id:a.id,pk:hex(await pbkBits('pw-one',a.ks,256)),fk:hex(FKRAW)}})()`);
  ok('0. the account was made and signed in', acct.fk && acct.fk.length == 64, acct);
  // ---- Chromium's desktop page (the default engine), then Lite's (webkit): the same page collects the portal's requests
  for (const [eng, label] of [[null, 'Chromium'], ['webkit', 'Lite']]) {
    M.engine = eng; M.wo = WO0(); M.calls = []; if (eng) { await p.close(); p = await ctx.newPage(); watch(p); await p.goto(URL0); await p.waitForFunction(() => typeof woFail == 'function' && typeof FSLOADED != 'undefined' && FSLOADED, null, {timeout: 40000});
      await p.evaluate("(()=>{['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});USER=USER||ACCTS[0];return 1})()") }
    await p.evaluate("document.querySelectorAll('.rmodal').forEach(o=>o.remove());svj('bd95wo62:'+uid(),1)");   // (the notice, tested below)
    M.reqs = [{id: 'lxfail-' + NOW() + '-' + (eng ? 2 : 1), req: FAIL}];
    const shown = await until(p, "!!document.querySelector('.wo62f')", 8000);
    const f = await txt(p, '.wo62f');
    ok(`1${eng ? 'L' : ''}. ${label}: Starport GE's launcher ending at once is said: its name, how it ended, its log's last line, what it runs with`, shown && /Starport GE did not start/.test(f) && /exit status 2/.test(f) && /can't cd to \/home\/rudolf\/\.wine\/drive_c\/users\/rudolf\/StarportGE/.test(f) && /starport-ge\.log/.test(f) && /What it runs with \(Wine, its 32- and 64-bit GL and Vulkan/.test(f) && /llvmpipe/.test(f) && /no \/dev\/dri/.test(f) && /starport-ge\.diag\.txt/.test(f), f);
    ok(`...${label}: its folder is among the Windows programs from before 1.5.9 (asked of rudolfd with its folder): Bring them in…`, /Its folder \(StarportGE\) is among the Windows programs from before Rudolf OS 1\.5\.9/.test(f) && await p.evaluate("!document.querySelector('.wo62fb').hidden") && M.calls.some(c => c.startsWith('GET wine/old?dir=')), [f, M.calls.filter(c => /wine/.test(c))]);
    ok(`...${label}: the request is answered (taken away, shown once)`, M.calls.some(c => c.startsWith('POST portal/answer?id=lxfail-')) && M.reqs.length == 0 && await p.evaluate("document.querySelectorAll('.wo62f').length") == 1, M.calls.filter(c => /portal\/answer/.test(c)));
    await p.click('.wo62fb'); const off = await until(p, "!!document.querySelector('.wo62')", 4000); const o = await txt(p, '.wo62');
    ok(`2${eng ? 'L' : ''}. ${label}: the offer: the programs, that the copy takes the fresh prefix's place and keeps it, the size and the room, the old one untouched`, off && /StarportGE, Notepad2/.test(o) && /takes its place/.test(o) && /\.wine\.own-/.test(o) && /1\.3 GB/.test(o) && /40\.0 GB free/.test(o) && /stays as it was/.test(o), o);
    await p.click('.wo62go'); const done = await until(p, "/Done\\./.test((document.querySelector('.wo62pt')||{}).textContent||'')", 9000);
    const o2 = await txt(p, '.wo62');
    ok(`...${label}: Bring them in: started (no password: one account), its progress shown, then done, with where her own prefix was kept`, done && M.body == '{}' && /\.wine\.own-20261123-101500/.test(o2), [o2, M.body]);
    ok(`...${label}: said when done, and the Start menu's Debian apps asked for again`, await until(p, "NC.some(n=>/in your account now/.test(n.b||n.body||''))", 4000) && M.calls.filter(c => c.startsWith('GET debian/apps') || c.startsWith('POST debian/apps')).length > 0, await p.evaluate("NC.slice(0,3)"));
    await p.evaluate("document.querySelectorAll('.rmodal').forEach(o=>o.remove())");
  }
  // ---- not a Windows program: the message only
  M.wo = WO0(); M.reqs = [{id: 'lxfail-' + (NOW() - 3600) + '-9', req: Object.assign({}, FAIL, {name: 'Stale'})}, {id: 'lxfail-' + NOW() + '-3', req: {kind: 'lxfail', app: 'foo', name: 'Foo <b>bar</b>', status: 137, secs: 1.2, wine: false, dir: '', log: '/run/rudolf/debian/foo.log', lines: ['Killed']}}];
  await until(p, "!!document.querySelector('.wo62f')", 8000); let f = await txt(p, '.wo62f');
  ok('3. a Debian program stopped at once: said (stopped by signal 9), its name as text (not HTML), no offer', /Foo <b>bar<\/b> did not start/.test(f) && /signal 9/.test(f) && await p.evaluate("document.querySelector('.wo62fb').hidden&&!document.querySelector('.wo62f b')"), f);
  ok('...one left from an hour ago (no desktop was there to say it): taken away, not said now', M.reqs.length == 0 && await p.evaluate("![...document.querySelectorAll('.wo62f h3')].some(h=>/Stale/.test(h.textContent))"), M.reqs);
  await p.evaluate("document.querySelectorAll('.rmodal').forEach(o=>o.remove())");
  // ---- several accounts: an administrator's name and password go with the request; too little room
  M.wo = Object.assign(WO0(), {several: true, accounts: 2, programs: [], mode: 'beside', own: true}); M.body = null;
  await p.evaluate("window.__elev=elevate;elevate=async(r,o)=>{window.__elevAsked=[r,o];return{n:'Ann',p:'pw-ann'}};woOffer('test')"); await until(p, "!!document.querySelector('.wo62')", 4000);
  let o = await txt(p, '.wo62');
  ok('4. several accounts: only how many programs, the copy beside his own, an administrator decides', /2 programs/.test(o) && !/StarportGE/.test(o) && /goes beside it/.test(o) && /an administrator decides/.test(o), o);
  await p.click('.wo62go'); await until(p, "!!window.__elevAsked", 3000); await wait(500);
  ok('...Bring them in asks for an administrator\'s name and password ({cred}) and sends them', JSON.parse(M.body || '{}').n == 'Ann' && JSON.parse(M.body || '{}').p == 'pw-ann' && await p.evaluate("!!(__elevAsked[1]&&__elevAsked[1].cred)"), M.body);
  await until(p, "/Done\\./.test((document.querySelector('.wo62pt')||{}).textContent||'')", 9000); await p.evaluate("elevate=window.__elev;document.querySelectorAll('.rmodal').forEach(o=>o.remove())");
  M.wo = Object.assign(WO0(), {room: false, free: 1e9}); await p.evaluate("woOffer('test')"); await until(p, "!!document.querySelector('.wo62')", 4000); o = await txt(p, '.wo62');
  ok('5. too little room: said, and Bring them in cannot be pressed', /Not enough room: the copy needs 1\.6 GB and 1\.0 GB is free/.test(o) && await p.evaluate("document.querySelector('.wo62go').disabled"), o);
  await p.click('.wo62no'); await wait(400);
  ok('...Don\'t ask again: rudolfd told, the dialog closed', M.calls.some(c => c == 'POST wine/decline') && !(await p.evaluate("!!document.querySelector('.wo62')")), M.calls.slice(-4));
  // ---- Settings › Apps › Linux, System health, the notice once
  M.wo = WO0(); await p.evaluate("const c=document.createElement('div');c.className='wotest';document.body.appendChild(c);debWineCell(c,{wine:1},document.createElement('p'),()=>{});1"); await wait(800);
  let c = await txt(p, '.wotest');
  ok('6. Settings › Apps › Linux, beside Wine: the programs from before are not in the account yet, Bring them in…', /Wine is installed/.test(c) && /StarportGE, Notepad2/.test(c) && /Bring them in…/.test(c), c);
  await p.evaluate("const c=document.createElement('div');c.className='hltest';document.body.appendChild(c);hlUsersRow(c);1"); await wait(800);
  c = await txt(p, '.hltest .wo62h');
  ok('7. System health names it while it is pending', !!c && /Windows programs from before/.test(c) && /Not in your account yet: StarportGE, Notepad2 \(1\.3 GB\)/.test(c), c);
  M.wo.pending = false; M.wo.declined = true; await p.evaluate("document.querySelector('.hltest').remove();const c=document.createElement('div');c.className='hltest';document.body.appendChild(c);hlUsersRow(c);1"); await wait(800);
  ok('...not once declined', !(await p.evaluate("!!document.querySelector('.hltest .wo62h')")));
  M.wo = WO0(); await p.evaluate("try{localStorage.removeItem('bd95wo62:'+uid())}catch(e){};document.querySelectorAll('.rmodal').forEach(o=>o.remove());woNotice()"); await until(p, "!!document.querySelector('.wo62')", 4000);
  const n1 = await p.evaluate("!!document.querySelector('.wo62')"); await p.evaluate("document.querySelectorAll('.rmodal').forEach(o=>o.remove());woNotice()"); await wait(1000);
  ok('8. the notice after sign-in: the offer once, not again for the same account', n1 && !(await p.evaluate("!!document.querySelector('.wo62')")));
  // ---- the native desktop: the shell's hidden page (#app=do-bg), handed the request (__oaPortal), as rudolf-native's pt_tick does
  await p.close(); p = await ctx.newPage(); await p.goto(URL0 + 'favicon.png');
  M.session = {ok: true, id: acct.id, n: 'Ann', r: 'admin', pk: acct.pk, fk: acct.fk}; M.engine = 'native'; M.wo = WO0(); M.reqs = [];
  const pg = await ctx.newPage(); watch(pg); await pg.addInitScript(NAT); await pg.goto(URL0 + '#app=do-bg');
  const rdy = await until(pg, "(window.__msgs||[]).some(m=>m.op=='portal'&&m.ready=='1')&&typeof __oaPortal=='function'", 30000);
  const nid = 'lxfail-' + NOW() + '-4'; M.reqs = [{id: nid, req: FAIL}]; await pg.evaluate(i => __oaPortal(i), nid);
  const nat = await until(pg, "!!document.querySelector('.wo62f')", 8000); f = await txt(pg, '.wo62f');
  ok('9. Native: the hidden page, handed the request, shows the same message with the offer', rdy && nat && /Starport GE did not start/.test(f) && /can't cd to/.test(f) && /Its folder \(StarportGE\)/.test(f), [rdy, f]);
  ok('...and asks the shell to show it above every window (op modal), as its other dialogs', await until(pg, "(window.__msgs||[]).some(m=>m.op=='modal'&&m.on=='1')", 4000), await pg.evaluate("(window.__msgs||[]).map(m=>m.op).slice(-8)"));
  ok('...the request answered', M.calls.some(c => c.startsWith('POST portal/answer?id=' + nid)));
  await pg.click('.wo62fb'); await until(pg, "!!document.querySelector('.wo62')", 4000); await pg.click('.wo62go');
  const nd = await until(pg, "/Done\\./.test((document.querySelector('.wo62pt')||{}).textContent||'')", 9000);
  ok('10. Native: Bring them in, its progress and done, as on Chromium', nd && M.body == '{}', M.body);
  ok('11. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
