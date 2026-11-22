// 1.6.2 (docs/plan-1.6.2.md Part 25b): Linux apps' progress on their taskbar buttons on Standard (lxprog62.js). rudolfd's
// portal/progress (the signed-in account's apps' desktop file IDs, StartupWMClass, a percentage) fills the Linux app's button
// with Rudolf's own fill (tbprog61.js: the same place, colour and setting), only while its window is not the one shown; done:
// to 100 %, then gone; never the creeping fill; the app's windows found by class, instance, the ID's last part or StartupWMClass;
// another app's, an unknown app's and malformed entries ignored; nothing it is given becomes HTML; Rudolf's own fills unchanged.
// A stand-in rudolfd (ports 8600+). With LXP_API (lxprog162_t.sh: fixtures/lxprogapi.c's command line, the real portal and a
// stand-in app on a private bus), portal/progress is rudolfd's own answer, and the measured fill goes to LXP_OUT (Standard's
// against Native's).   node lxprog62_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path'), cp = require('child_process');
const WWW = path.join(__dirname, '../../../iso/www'), J = {'content-type': 'application/json'};
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 500) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm', '.woff2': 'font/woff2'};
const REAL = process.env.LXP_API ? process.env.LXP_API.split('\t') : null, PORT = +(process.env.LXP_PORT || 8600);
const M = {wins: [], mode: 'desktop', prog: {ok: true, apps: []}, asked: 0};
const LS = "window.__rudolfLsFlush=()=>Promise.resolve();";
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'), u = decodeURIComponent(u0); q.on('data', () => {}); q.on('end', () => {
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};' + LS + '</script>')) }
  if (u.startsWith('/api/')) {
    if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200, J); s.end('{"ok":true,"events":[]}') }, 20000);
    if (u == '/api/apps/windows') { s.writeHead(200, J); return s.end(JSON.stringify({ok: true, windows: M.wins.map(w => ({...w, shown: M.mode == 'win ' + w.win})), mode: M.mode})) }
    if (u == '/api/apps/show') { M.mode = 'win ' + new URLSearchParams(qs).get('win'); s.writeHead(200, J); return s.end('{"ok":true}') }
    if (u == '/api/apps/desktop') { M.mode = 'desktop'; s.writeHead(200, J); return s.end('{"ok":true}') }
    if (u == '/api/portal/progress') { M.asked++; s.writeHead(200, J);
      if (REAL) { let o = ''; try { o = cp.execFileSync(REAL[0], REAL.slice(1), {timeout: 10000}).toString() } catch (e) { o = '{"ok":false}' } M.last = o.trim(); return s.end(o) }
      return s.end(typeof M.prog == 'string' ? M.prog : JSON.stringify(M.prog)) }
    if (u == '/api/portal/poll') { s.writeHead(200, J); return s.end('{"ok":true,"requests":[]}') }
    s.writeHead(200, J); return s.end('{"ok":true}') }
  const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(PORT, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const ctx = await b.newContext({viewport: {width: 1280, height: 860}});
  const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto('http://127.0.0.1:' + PORT + '/'); await p.waitForFunction(() => typeof winProgress == 'function' && typeof lxpPoll == 'function' && typeof lxTaskbar == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x'}];USER=ACCTS[0];S.accent='#2563eb';S.dm='dark';S.tbpcol='green';themeApply();applySet();
    window.TW=win('Untitled - Notepad','📝',360,240);TW.app='notepad';return 1})()`);
  // the fill on a button (a Linux app's by its window, or a Rudolf window's), against the button
  const fillOf = sel => q(`(()=>{const b=${sel};if(!b)return 'nobutton';const f=b.querySelector(':scope > .tbpf');if(!f)return null;const r=b.getBoundingClientRect(),x=f.getBoundingClientRect(),cs=getComputedStyle(f),bs=getComputedStyle(b),bw=parseFloat(bs.borderLeftWidth)||0,bt=parseFloat(bs.borderTopWidth)||0;
    return {frac:x.width/(r.width-2*bw-6),dl:x.left-r.left-bw,dt:x.top-r.top-bt,db:r.bottom-bt-x.bottom,bg:cs.backgroundColor,op:+cs.opacity,z:cs.zIndex,iso:bs.isolation,num:f.classList.contains('tbpnum'),txt:b.textContent,html:b.innerHTML}})()`);
  const lx = win => fillOf(`lxpBtn(${JSON.stringify(win)})`);
  M.wins = [{win: '1e00001', web: false, fullscreen: false, class: 'Fetch', instance: 'fetch', title: 'Fetch - downloads'},
            {win: '1e00002', web: false, fullscreen: false, class: 'Google-chrome', instance: 'google-chrome', title: 'Chrome'},
            {win: '1e00003', web: false, fullscreen: false, class: 'Other', instance: 'other', title: 'Another app'}];
  if (REAL) {
    // ---- the real portal's note through rudolfd's own portal/progress (lxprog162_t.sh's stand-in app: org.example.Fetch at 42 %)
    await p.waitForFunction(() => { const b = lxpBtn('1e00001'); return b && b.querySelector(':scope > .tbpf') }, null, {timeout: 15000}).catch(() => {});
    await wait(800); const f = await lx('1e00001');
    ok('R1. the real listener (a stand-in app\'s LauncherEntry signal, the account\'s portal, rudolfd\'s portal/progress): its button fills to 42 %', f && Math.abs(f.frac - .42) < .01 && f.num, [f, M.last]);
    ok('R2. ...with Rudolf\'s own fill (3 px inside, behind the title, green, half see-through)', f && Math.abs(f.dl - 3) < .6 && Math.abs(f.dt - 3) < .6 && f.z == '-1' && /0\.0862745 0\.639216 0\.290196 \/ 0\.5|22, 163, 74, 0\.5/.test(f.bg), f);
    ok('R3. another app\'s window: nothing', (await lx('1e00003')) == null);
    if (process.env.LXP_OUT) { const bx = await q(`(()=>{const b=lxpBtn('1e00001'),f=b.querySelector('.tbpf'),r=b.getBoundingClientRect(),x=f.getBoundingClientRect();return {bw:r.width,fw:x.width,bg:getComputedStyle(f).backgroundColor}})()`);
      fs.writeFileSync(process.env.LXP_OUT, JSON.stringify({frac: f && f.frac, ...bx})) }
    if (process.env.LXP_STEP) { fs.writeFileSync(process.env.LXP_STEP, 'measured\n'); for (let i = 0; i < 100 && fs.readFileSync(process.env.LXP_STEP, 'utf8') != 'done\n'; i++) await wait(200);
      await wait(1500); ok('R4. the app hides its progress (progress-visible false): the fill completes and goes', (await lx('1e00001')) == null) }
    ok('R5. no page errors', errs.length == 0, errs);
    await b.close(); srv.close(); const n = R.filter(Boolean).length; console.log(n + '/' + R.length + ' passed'); process.exit(n == R.length ? 0 : 1);
  }
  // 1. nothing to say: no fill; asked only while a Linux window is there
  await wait(2500);
  ok('1. Linux apps\' windows on the taskbar, nothing in progress: no fill', (await lx('1e00001')) == null && (await lx('1e00002')) == null && M.asked > 0, [await lx('1e00001'), M.asked]);
  // 2. an app at 42 %, its window not shown (the desktop in front): its button fills
  M.prog = {ok: true, apps: [{id: 'org.example.Fetch', wmclass: '', v: 42}]}; await wait(2200); let f = await lx('1e00001');
  ok('2. an app\'s progress (org.example.Fetch, 42 %): its window\'s button (class Fetch: the ID\'s last part) fills to 42 %', f && Math.abs(f.frac - .42) < .01, f);
  ok('3. ...Rudolf\'s own fill: 3 px inside the edge, behind the picture and the title, a percentage (not the creeping fill)', f && Math.abs(f.dl - 3) < .6 && Math.abs(f.dt - 3) < .6 && Math.abs(f.db - 3) < .6 && f.z == '-1' && f.iso == 'isolate' && f.num && /Fetch/.test(f.txt), f);
  ok('4. ...green by default, half see-through, as Rudolf\'s', f && /0\.0862745 0\.639216 0\.290196 \/ 0\.5|22, 163, 74, 0\.5/.test(f.bg), f && f.bg);
  ok('5. the other apps\' buttons: nothing', (await lx('1e00002')) == null && (await lx('1e00003')) == null);
  M.prog = {ok: true, apps: [{id: 'org.example.Fetch', wmclass: '', v: 80}]}; await wait(1800); f = await lx('1e00001');
  ok('6. the width follows the app (80 %)', f && Math.abs(f.frac - .8) < .01, f);
  // 7. shown: hidden; back to the desktop: where it is now
  await q("lxpBtn('1e00001').click()"); await wait(2200);
  ok('7. its window shown (as a restored window): no fill', (await lx('1e00001')) == null && await q("LX.mode=='win 1e00001'"), await lx('1e00001'));
  M.prog = {ok: true, apps: [{id: 'org.example.Fetch', wmclass: '', v: 60}]}; await q("lxpBtn('1e00001').click()"); await wait(2200); f = await lx('1e00001');
  ok('8. back to the desktop (minimized again): where the work is now (60 %)', f && Math.abs(f.frac - .6) < .01, f);
  // 9. done: to 100 %, fades, gone
  M.prog = {ok: true, apps: []}; await wait(1150); f = await lx('1e00001');
  ok('9. the app done (its progress hidden): the fill completes (to 100 %)', f && f.frac > .9 && /width: calc\(100% - 6px\)/.test(f.html), f);
  await wait(1500); ok('10. ...then fades and is gone', (await lx('1e00001')) == null);
  // 11. StartupWMClass: Chrome's (com.google.Chrome, class Google-chrome)
  M.prog = {ok: true, apps: [{id: 'com.google.Chrome', wmclass: 'google-chrome', v: 25}]}; await wait(2200); f = await lx('1e00002');
  ok('11. an app found by its StartupWMClass (com.google.Chrome: google-chrome) fills its window\'s button (25 %)', f && Math.abs(f.frac - .25) < .01 && (await lx('1e00001')) == null, f);
  // 11b. a window's own progress (the XApp hint, as Firefox's on X11): that window, before its app's
  M.prog = {ok: true, apps: [{id: 'org.example.Fetch', wmclass: '', v: 20}], windows: [{win: '1e00001', v: 64}, {win: '1e00003', v: 0}, {win: 'zz<b>', v: 5}, {win: '1e00002', v: '9'}]}; await wait(2200);
  f = await lx('1e00001'); const f3b = await lx('1e00003');
  ok('11b. a window\'s own progress (_NET_WM_XAPP_PROGRESS: rudolfd\'s "windows") comes first (64 %, not its app\'s 20 %); 0, other IDs, not numbers: ignored', f && Math.abs(f.frac - .64) < .01 && f3b == null && (await lx('1e00002')) == null, [f, f3b]);
  M.prog = {ok: true, apps: [{id: 'com.google.Chrome', wmclass: 'google-chrome', v: 25}]}; await wait(2200);
  // 12. the colour setting: the same as Rudolf's
  await q("S.tbpcol='blue';tbpApply()"); await wait(300); f = await lx('1e00002');
  ok('12. the Progress colour (Blue) applies to it as to Rudolf\'s own', f && /0\.145098 0\.388235 0\.921569|37, 99, 235/.test(f.bg), f && f.bg);
  await q("S.tbpcol='green';tbpApply()");
  // 13. unknown, malformed, HTML
  M.prog = {ok: true, apps: [{id: 'org.nosuch.App', wmclass: '', v: 50}, {id: '<img src=x onerror=window.PWN=1>', wmclass: '', v: 50}, {id: 'fetch', wmclass: '<b>', v: 50},
    {id: 'other', wmclass: '', v: 'busy'}, {id: 'other', wmclass: '', v: 101}, {id: 'other', wmclass: '', v: 4.5}, {id: 'other', v: -1}, {id: ['other'], v: 5}, null, 7]}; await wait(2600);
  const f1 = await lx('1e00001'), f3 = await lx('1e00003');
  ok('13. an unknown app, an ID with other characters, a value that is not a whole percentage 0-100: ignored (a bad StartupWMClass left out)', f1 && Math.abs(f1.frac - .5) < .01 && f3 == null && await q('!window.PWN'), [f1, f3]);
  ok('14. ...nothing it was given became HTML (the button holds its picture, its title and the fill only)', await q(`(()=>{const b=lxpBtn('1e00001');return [...b.childNodes].every(n=>n.nodeType==1&&['lxti','lxtt','tbpf'].some(c=>n.classList.contains(c)))&&!document.querySelector('#tasks img[src=x]')})()`), (f1 || {}).html);
  M.prog = '{"ok":true,"apps":'; await wait(1800); ok('15. an answer that is not JSON: nothing changes, no error', (await lx('1e00001')) != null && errs.length == 0, errs);
  M.prog = {ok: true, apps: []}; await wait(2500);
  // 16. a "busy" from an app never creeps: the page never gives Linux apps 'busy'
  ok('16. never the creeping fill for an app (only whole percentages are taken)', await q("[...LXP.w.values()].every(W=>{const st=TBP.m.get(W);return !st||Object.values(st.src).every(v=>v!='busy')})"));
  // 17. the window closed while in progress: forgotten
  M.prog = {ok: true, apps: [{id: 'org.example.Fetch', wmclass: '', v: 30}]}; await wait(2200); const had = !!(await lx('1e00001'));
  M.wins = M.wins.filter(w => w.win != '1e00001'); await wait(2500);
  ok('17. its window closed while at work: its button gone, nothing kept', had && await q("lxpBtn('1e00001')==null&&!LXP.w.has('1e00001')"));
  // 18. Rudolf's own fills as before (Part 25): a minimized Notepad at 40 %
  await q('TW.el.querySelector(".mn").click();winProgress(TW,40)'); await wait(800); f = await fillOf('TW.tb');
  ok('18. Rudolf\'s own windows\' fill as before (a minimized Notepad at 40 %)', f && Math.abs(f.frac - .4) < .01, f);
  // 19. the native desktop's pages ask nothing (the shell draws Linux apps' fills there)
  const e1 = errs.length; await p.close(); const p2 = await ctx.newPage(); p2.on('pageerror', e => errs.push(e.message));
  await p2.goto('http://127.0.0.1:' + PORT + '/#app=notepad'); await p2.waitForFunction(() => typeof lxpPoll == 'function' && typeof ONEAPP != 'undefined' && !!ONEAPP, null, {timeout: 30000});
  await p2.evaluate("LX.wins=[{win:'1e00009',web:false,class:'Fetch',instance:'fetch'}]"); const a0 = M.asked; await wait(3000);
  ok('19. a page of one app (the native desktop\'s windows) asks nothing: the shell draws Linux apps\' fills there', M.asked == a0, M.asked - a0);
  errs.splice(e1);
  ok('20. no page errors', errs.length == 0, errs);
  await b.close(); srv.close(); const n = R.filter(Boolean).length; console.log(n + '/' + R.length + ' passed'); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.log('FAIL ' + (e && e.stack || e)); process.exit(1) });
