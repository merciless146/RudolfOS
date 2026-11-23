// 1.6.2 (docs/plan-1.6.2.md Part 38, step 1): what an app's window of the native desktop really runs, and what it costs. The built page
// (iso/www) served by the stand-in rudolfd (iso/qt/host/fixtures/native_rd.py, signed in as Ann, Task Manager's answers from
// fixtures/tm62.json), in Chromium, as a native window's page (window.webkit.messageHandlers.rudolfNative stood in, so the page is in
// its native one-app mode, lean61.js too):
//   cover  Chromium's precise coverage (the Profiler) of a Task Manager window through every tab and action, and of a Settings window
//          panel by panel: the functions run, by module (desktop/build19.py's mods; "base" is base_v24.html's own code), written as JSON
//   mem    the renderer's private memory (and PSS) of one window, alone in its browser, 20 s after it opened and after collecting
//   node lean162m.js cover OUT.json [CHROME]
//   node lean162m.js mem URLHASH [ID] [CHROME]        e.g.  mem app=taskmgr      mem app=taskmgr 0123456789abcdef (www/a/ID.html, as /?oa=ID)
// Ports 8670-8679 (LEAN_PORT, default 8670).
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path'), cp = require('child_process'), os = require('os');
const HERE = __dirname, WWW = path.join(HERE, '../../www'), FX = path.join(HERE, '../host/fixtures');
const [MODE, A1, A2, A3] = process.argv.slice(2);
const PORT = +(process.env.LEAN_PORT || 8670), BASE = 'http://127.0.0.1:' + PORT;
const wait = ms => new Promise(r => setTimeout(r, ms));
const T = fs.mkdtempSync(path.join(os.tmpdir(), 'lean162.')), D = path.join(T, 'd'); fs.mkdirSync(D);
cp.execFileSync('node', [path.join(FX, 'deskls.js'), path.join(D, 'ls.json'), path.join(D, 'keys.json')]);
const KEYS = JSON.parse(fs.readFileSync(path.join(D, 'keys.json')));
fs.writeFileSync(path.join(D, 'settings.json'), JSON.stringify({s: {bs: 'solid', b1: '#112233', tb: 'bottom', accent: '#2563eb', h24: true}, pins: ['notepad', 'calc'], notes: 2, wallpaper: false}));
fs.copyFileSync(path.join(FX, 'tm62.json'), path.join(D, 'tm.json'));
fs.writeFileSync(path.join(D, 'stats-on'), '1');
// the native shell's message handler, stood in: what the page posts is kept (window.__posts)
fs.writeFileSync(path.join(D, 'inject.js'), "window.webkit={messageHandlers:{rudolfNative:{postMessage(m){(window.__posts=window.__posts||[]).push(m)}}}};");
// (LEAN_COUNT=1) what the page makes that holds memory outside its own objects: counted by kind (window.__made)
if (process.env.LEAN_COUNT) fs.appendFileSync(path.join(D, 'inject.js'), `(()=>{const M=window.__made={};const n=k=>{M[k]=(M[k]||0)+1};
 for(const C of ['AudioContext','OfflineAudioContext','Worker','SharedWorker','OffscreenCanvas','Image','WebSocket','EventSource','BroadcastChannel','MutationObserver','ResizeObserver','IntersectionObserver']){const O=window[C];if(!O)continue;window[C]=new Proxy(O,{construct(t,a,nt){n(C);return Reflect.construct(t,a,nt)}})}
 for(const C of ['DateTimeFormat','NumberFormat','Collator','PluralRules','RelativeTimeFormat','Segmenter','DisplayNames','ListFormat']){const O=Intl[C];if(!O)continue;Intl[C]=new Proxy(O,{construct(t,a,nt){n('Intl.'+C);return Reflect.construct(t,a,nt)}})}
 const gc=HTMLCanvasElement.prototype.getContext;HTMLCanvasElement.prototype.getContext=function(k){n('canvas '+k+' '+this.width+'x'+this.height);return gc.apply(this,arguments)};
 const tl=Date.prototype.toLocaleString,tld=Date.prototype.toLocaleDateString,tlt=Date.prototype.toLocaleTimeString;Date.prototype.toLocaleString=function(){n('toLocaleString');return tl.apply(this,arguments)};Date.prototype.toLocaleDateString=function(){n('toLocaleDateString');return tld.apply(this,arguments)};Date.prototype.toLocaleTimeString=function(){n('toLocaleTimeString');return tlt.apply(this,arguments)};
 for(const k of ['compile','instantiate','instantiateStreaming']){const f=WebAssembly[k];WebAssembly[k]=function(){n('wasm '+k);return f.apply(this,arguments)}}
 const ib=window.createImageBitmap;if(ib)window.createImageBitmap=function(){n('createImageBitmap');return ib.apply(this,arguments)};
 const io=indexedDB.open.bind(indexedDB);indexedDB.open=function(a){n('idb '+a);return io.apply(null,arguments)};
 const si=window.setInterval;window.setInterval=function(f,t){n('setInterval');return si.apply(this,arguments)};})();`);
const rd = cp.spawn('python3', [path.join(FX, 'native_rd.py'), String(PORT), D], {stdio: 'ignore'});
const done = c => { try { rd.kill() } catch (e) {} fs.rmSync(T, {recursive: true, force: true}); process.exit(c) };
async function login() {
  for (let i = 0; i < 50; i++) { try { const r = await fetch(BASE + '/api/desktop/login', {method: 'POST', headers: {'X-Rudolf-Token': 'tok59native'}, body: JSON.stringify({n: 'Ann', p: KEYS.Ann.pw})}); if (r.ok) return; } catch (e) {} await wait(200) }
  throw new Error('the stand-in rudolfd did not answer');
}
const CH = MODE == 'cover' ? A2 : A3;   // (memraw: the same as mem)
async function open(hash, page) {
  const b = await chromium.launch({executablePath: CH || undefined, args: ['--js-flags=--expose-gc']});
  const ctx = await b.newContext({viewport: {width: 1280, height: 800}}); const p = await ctx.newPage();
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  return {b, ctx, p, errs, go: () => p.goto(BASE + '/' + (page ? '?oa=' + page : '') + '#' + hash + '&w=1&sw=1280&sh=800')};
}
// the functions run, by where they are (the page's script: each module starts at its __svcMod("NAME") marker)
function byModule(cov, src) {
  const marks = []; const re = /__svcMod\("([^"]+)"\);/g; let m; while ((m = re.exec(src))) marks.push([m.index, m[1]]);
  const where = off => { let lo = 0, hi = marks.length - 1, r = 'base'; while (lo <= hi) { const mid = (lo + hi) >> 1; if (marks[mid][0] <= off) { r = marks[mid][1]; lo = mid + 1 } else hi = mid - 1 } return r == 'base' ? 'base' : r };
  const out = {};
  for (const f of cov) for (const r of f.functions) { if (!r.ranges[0].count || !r.functionName) continue; const o = r.ranges[0].startOffset, k = where(o);
    (out[k] = out[k] || {fns: new Set(), bytes: 0}); if (!out[k].fns.has(r.functionName)) { out[k].fns.add(r.functionName); out[k].bytes += r.ranges[0].endOffset - o } }
  const res = {}; for (const k of Object.keys(out).sort()) res[k] = {n: out[k].fns.size, bytes: out[k].bytes, fns: [...out[k].fns].sort()}; return res;
}
async function cover(OUT) {
  const R = {when: new Date().toISOString(), chrome: '', taskmgr: {}, settings: {}};
  const html = await (await fetch(BASE + '/')).text();
  const sizes = {}; { const re = /__svcMod\("([^"]+)"\);/g; let m, last = null, lastI = 0; while ((m = re.exec(html))) { if (last) sizes[last] = m.index - lastI; last = m[1]; lastI = m.index } }
  R.moduleBytes = sizes;
  // Task Manager: every tab, sorting, the search box, a process picked, End process (cancelled at the administrator's prompt), the
  // right-click menu, Refresh, Clear cache, Copy system report, Report a problem (opened, closed), Startup's switch
  {
    const o = await open('app=taskmgr'); R.chrome = o.b.version(); const cdp = await o.ctx.newCDPSession(o.p);
    await cdp.send('Profiler.enable'); await cdp.send('Profiler.startPreciseCoverage', {callCount: true, detailed: false});
    await o.go(); await o.p.waitForFunction(() => window.__tmW && __tmW.body && __tmW.body.querySelector('.tmtabs'), null, {timeout: 60000}); await wait(3000);
    const take = async () => (await cdp.send('Profiler.takePreciseCoverage')).result.filter(s => s.url.startsWith(BASE));
    const all = []; const step = async (n, js, ms) => { if (js) { try { await o.p.evaluate(js) } catch (e) { R.taskmgr['!' + n] = e.message } } await wait(ms || 2500); const c = await take(); all.push(...c); R.taskmgr[n] = byModule(c, html) };
    await step('start', null, 0);
    for (const t of ['apps', 'procs', 'perf', 'system', 'hw', 'start'])
      await step('tab ' + t, `__tmW.body.querySelector(".tmtabs button[data-t='${t}']").click()`, 3500);
    await step('actions', `(async()=>{const q=s=>__tmW.body.querySelector(s);q(".tmtabs button[data-t='procs']").click();await new Promise(r=>setTimeout(r,2500));
      q('th[data-k=name]').click();q('th[data-k=pid]').click();const f=q('.tmf');f.value='web';f.dispatchEvent(new Event('input'));
      const r=q('tr[data-pid="520"]');r&&r.click();r&&r.dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,clientX:200,clientY:200}));
      document.dispatchEvent(new KeyboardEvent('keydown',{key:'Escape',bubbles:true}));q('.tme').click();await new Promise(r=>setTimeout(r,1500));
      const c=[...document.querySelectorAll('button')].find(b=>/^Cancel$/.test(b.textContent.trim())&&b.offsetParent);c&&c.click();
      q(".tmtabs button[data-t='perf']").click();await new Promise(r=>setTimeout(r,2500));const cc=q('.tmccb');cc&&cc.click();
      q(".tmtabs button[data-t='system']").click();await new Promise(r=>setTimeout(r,2500));const rf=q('.tmrf');rf&&rf.click();
      q(".tmtabs button[data-t='start']").click();const rs=q('.rs');rs&&rs.click();rs&&rs.click();
      q('.tmcopy').click();await new Promise(r=>setTimeout(r,2000));q('.tmrep').click();await new Promise(r=>setTimeout(r,2000));
      document.querySelectorAll('.win').forEach(w=>{if(w!==__tmW.el&&/report/i.test(w.textContent)){const x=w.querySelector('.x,.cls,[data-c=x]');x&&x.click()}})})()`, 4000);
    R.taskmgr.all = byModule(all, html); R.taskmgr.errors = o.errs; await o.b.close();
  }
  // Settings: each panel shown (SW.show), its own functions (what the panel ran after the window was up)
  {
    const o = await open('app=settings'); const cdp = await o.ctx.newCDPSession(o.p);
    await cdp.send('Profiler.enable'); await cdp.send('Profiler.startPreciseCoverage', {callCount: true, detailed: false});
    await o.go(); await o.p.waitForFunction(() => typeof SW == 'object' && SW && SW.SEC, null, {timeout: 60000}); await wait(3000);
    const take = async () => (await cdp.send('Profiler.takePreciseCoverage')).result.filter(s => s.url.startsWith(BASE));
    const all = []; let c = await take(); all.push(...c); R.settings.start = byModule(c, html);
    const keys = await o.p.evaluate('Object.keys(SW.SEC)');
    for (const k of keys) { try { await o.p.evaluate(`SW.show('${k}',1)`) } catch (e) { R.settings['!' + k] = e.message } await wait(2500); c = await take(); all.push(...c); R.settings['panel ' + k] = byModule(c, html) }
    R.settings.all = byModule(all, html); R.settings.errors = o.errs; await o.b.close();
  }
  fs.writeFileSync(OUT, JSON.stringify(R, null, 1)); console.log('written', OUT);
}
// a process's private memory and PSS (smaps_rollup), MB
const smaps = pid => { try { const t = fs.readFileSync('/proc/' + pid + '/smaps_rollup', 'utf8'), g = k => +((t.match(new RegExp('^' + k + ':\\s+(\\d+)', 'm')) || [0, 0])[1]); return {priv: (g('Private_Clean') + g('Private_Dirty')) / 1024, pss: g('Pss') / 1024} } catch (e) { return null } };
const renderers = bpid => { const out = []; for (const d of fs.readdirSync('/proc')) { if (!/^\d+$/.test(d)) continue; try { const c = fs.readFileSync('/proc/' + d + '/cmdline', 'utf8'); if (c.includes('--type=renderer') && !c.includes('--extension-process')) { const st = fs.readFileSync('/proc/' + d + '/stat', 'utf8'); out.push(+d) } } catch (e) {} } return out };
async function mem(hash, page) {
  const before = new Set(renderers()); const o = await open(hash, page); await o.go();
  await wait(20000); const cdp = await o.ctx.newCDPSession(o.p); await cdp.send('HeapProfiler.collectGarbage'); await wait(3000);
  const mine = renderers().filter(p => !before.has(p)); const m = mine.map(smaps).filter(Boolean);
  const heap = await o.p.evaluate(() => performance.memory ? performance.memory.usedJSHeapSize / 1048576 : null);
  const st = await o.p.evaluate(() => ({read: window.__leanRead || [], up: !!(typeof OA == 'object' && OA.main && OA.main.el), nodes: document.getElementsByTagName('*').length}));
  if (process.env.LEAN_DUMP) await dump(o);
  const r = {hash, page: page || 'index.html', renderer: m[0] && {priv: +m[0].priv.toFixed(1), pss: +m[0].pss.toFixed(1)}, heap: heap && +heap.toFixed(1), n: m.length, up: st.up, nodes: st.nodes, read: st.read, errors: o.errs.slice(0, 5)};
  console.log(JSON.stringify(r)); await o.b.close(); return r;
}
// the same with Chromium started by hand: no automation (Playwright's network domain keeps every response's body, the page's 3.7 MB
// among them); its DevTools port asked only to collect garbage and to read the page's state. This is the reading the plan gives.
async function memraw(hash, page) {
  const exe = CH || chromium.executablePath(), P2 = PORT + 5, U = fs.mkdtempSync(path.join(T, 'u'));
  const before = new Set(renderers());
  const c = cp.spawn(exe, ['--headless=new', '--no-sandbox', '--no-first-run', '--no-default-browser-check', '--user-data-dir=' + U, '--remote-debugging-port=' + P2, '--window-size=1280,800',
    '--disable-background-networking', '--disable-component-update', '--js-flags=--expose-gc', /^\//.test(hash) ? BASE + hash : BASE + '/' + (page ? '?oa=' + page : '') + '#' + hash + '&w=1&sw=1280&sh=800'], {stdio: 'ignore'});
  await wait(20000);
  const tg = (await (await fetch('http://127.0.0.1:' + P2 + '/json/list')).json()).find(t => t.type == 'page');
  const ws = new WebSocket(tg.webSocketDebuggerUrl); await new Promise(r => ws.onopen = r); let id = 0; const pend = {};
  ws.onmessage = e => { const m = JSON.parse(e.data); if (pend[m.id]) { pend[m.id](m.result); delete pend[m.id] } };
  const send = (method, params) => new Promise(r => { pend[++id] = r; ws.send(JSON.stringify({id, method, params: params || {}})) });
  await send('HeapProfiler.collectGarbage'); await wait(3000);
  const ev = await send('Runtime.evaluate', {returnByValue: true, expression: "JSON.stringify({read:window.__leanRead||[],up:!!(typeof OA=='object'&&OA.main&&OA.main.el),title:document.title,heap:performance.memory?+(performance.memory.usedJSHeapSize/1048576).toFixed(1):null,nodes:document.getElementsByTagName('*').length,css:[...document.styleSheets].reduce((a,x)=>{try{return a+[...x.cssRules].reduce((b,r)=>b+r.cssText.length,0)}catch(e){return a}},0),styles:document.styleSheets.length,made:window.__made})"});
  const mine = renderers().filter(p => !before.has(p)); const m = mine.map(smaps).filter(Boolean).sort((a, b) => b.priv - a.priv);
  if (process.env.LEAN_DUMP) { const bv = await (await fetch('http://127.0.0.1:' + P2 + '/json/version')).json(); const bw = new WebSocket(bv.webSocketDebuggerUrl); await new Promise(r => bw.onopen = r);
    const ev = []; let bid = 0; const bp = {}; let fin; const done2 = new Promise(r => fin = r);
    bw.onmessage = e => { const x = JSON.parse(e.data); if (x.method == 'Tracing.dataCollected') ev.push(...x.params.value); else if (x.method == 'Tracing.tracingComplete') fin(); else if (bp[x.id]) { bp[x.id](x.result); delete bp[x.id] } };
    const bs = (method, params) => new Promise(r => { bp[++bid] = r; bw.send(JSON.stringify({id: bid, method, params: params || {}})) });
    await bs('Tracing.start', {traceConfig: {includedCategories: ['disabled-by-default-memory-infra'], memoryDumpConfig: {triggers: []}}, transferMode: 'ReportEvents'});
    await bs('Tracing.requestMemoryDump', {levelOfDetail: 'detailed'}); await bs('Tracing.end'); await done2; bw.close();
    for (const e of ev) if (e.ph == 'v' && e.args && e.args.dumps && e.args.dumps.allocators && e.args.dumps.allocators.blink_gc) { const A = e.args.dumps.allocators, top = {};
      for (const k in A) { if (k.split('/').length > (+process.env.LEAN_DUMP || 2)) continue; const sz = A[k].attrs && A[k].attrs.size; if (sz) top[k] = +(parseInt(sz.value, 16) / 1048576).toFixed(1) }
      console.log('dump', JSON.stringify(Object.fromEntries(Object.entries(top).filter(([k, v]) => v >= 0.3).sort((a, b) => b[1] - a[1])))) } }
  ws.close(); c.kill(); await wait(500);
  const r = Object.assign({hash, page: page || "index.html", renderer: m[0] && {priv: +m[0].priv.toFixed(1), pss: +m[0].pss.toFixed(1)}, n: m.length}, ev && ev.result && ev.result.value ? JSON.parse(ev.result.value) : {eval: ev});
  console.log(JSON.stringify(r)); return r;
}
// (LEAN_DUMP=1) Chromium's own account of the renderer's memory (memory-infra): by allocator, MB
async function dump(o) {
  const cdp = await o.b.newBrowserCDPSession(); const ev = [];
  cdp.on('Tracing.dataCollected', e => ev.push(...e.value));
  await cdp.send('Tracing.start', {traceConfig: {includedCategories: ['disabled-by-default-memory-infra'], memoryDumpConfig: {triggers: []}}, transferMode: 'ReportEvents'});
  await cdp.send('Tracing.requestMemoryDump', {levelOfDetail: 'detailed'}).catch(e => console.log(e.message));
  const fin = new Promise(r => cdp.once('Tracing.tracingComplete', r)); await cdp.send('Tracing.end'); await fin;
  for (const e of ev) if (e.ph == 'v' && e.args && e.args.dumps && e.args.dumps.allocators) { const A = e.args.dumps.allocators, top = {};
    for (const k in A) { if (k.split('/').length > (+process.env.LEAN_DUMP || 2)) continue; const sz = A[k].attrs && A[k].attrs.size; if (sz) top[k] = +(parseInt(sz.value, 16) / 1048576).toFixed(1) }
    const pm = e.args.dumps.process_totals; console.log('dump pid', e.pid, JSON.stringify(Object.fromEntries(Object.entries(top).filter(([k, v]) => v >= 0.5).sort((a, b) => b[1] - a[1])))) }
}
(async () => {
  try { await login();
    if (MODE == 'cover') await cover(A1);
    else if (MODE == 'mem') await mem(A1, A2);
    else if (MODE == 'memraw') await memraw(A1, A2);
    else { console.log('node lean162m.js cover OUT.json | mem HASH [PAGE]'); done(2) }
    done(0) } catch (e) { console.log('FAIL', e.stack || e); done(1) }
})();
