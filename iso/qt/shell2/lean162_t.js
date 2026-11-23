// 1.6.2 (docs/plan-1.6.2.md Part 38, step 2): Task Manager's small page (desktop/lean62.py, a/ID.html) against the whole page's Task
// Manager. Both as the native desktop's window (#app=taskmgr, the native shell's message handler stood in), signed in through the stand-in
// rudolfd (iso/qt/host/fixtures/native_rd.py; Task Manager's answers from fixtures/tm62.json), the same clock and random numbers; every
// tab, and every action: sorting, the search box, a process picked, its right-click menu, End process (the administrator's prompt, then
// Cancel), End process tree, Clear cache, Refresh, Startup's switch and Remove, Copy system report, Report a problem (its dialog),
// Ctrl+Alt+Del's urgent window (&urgent=1). What the window shows at each step, what the page told the native shell (its messages) and
// what it asked rudolfd must be the same; the small page must never read the rest of the page (window.__leanRead), never give the
// start menu's catalogue, show no sign-in screen, and have no error. Then the memory: the renderer's private memory of each, alone in its
// browser (lean162m.js memraw: no automation attached).
//   node lean162_t.js [CHROME]        (ports 8670-8679: LEAN_PORT)
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path'), cp = require('child_process'), os = require('os');
const HERE = __dirname, WWW = path.join(HERE, '../../www'), FX = path.join(HERE, '../host/fixtures');
const CH = process.argv[2], PORT = +(process.env.LEAN_PORT || 8671), BASE = 'http://127.0.0.1:' + PORT;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 1500) : '')) };
const wait = ms => new Promise(r => setTimeout(r, ms));
const IDX = fs.readFileSync(path.join(WWW, 'index.html'), 'utf8'), TMID = (IDX.match(/__LEAN_TM='([0-9a-f]{16})'/) || [])[1];
const T = fs.mkdtempSync(path.join(os.tmpdir(), 'lean162t.')), D = path.join(T, 'd'); fs.mkdirSync(D);
cp.execFileSync('node', [path.join(FX, 'deskls.js'), path.join(D, 'ls.json'), path.join(D, 'keys.json')]);
const KEYS = JSON.parse(fs.readFileSync(path.join(D, 'keys.json')));
fs.writeFileSync(path.join(D, 'settings.json'), JSON.stringify({s: {bs: 'solid', b1: '#112233', tb: 'bottom', accent: '#2563eb', h24: true}, pins: ['notepad', 'calc'], notes: 2, wallpaper: false}));
fs.copyFileSync(path.join(FX, 'tm62.json'), path.join(D, 'tm.json')); fs.writeFileSync(path.join(D, 'stats-on'), '1');
// the native shell's handler stood in (its messages kept), the errors kept, and the same clock and random numbers each time
fs.writeFileSync(path.join(D, 'inject.js'), `window.webkit={messageHandlers:{rudolfNative:{postMessage(m){(window.__posts=window.__posts||[]).push(m);if(m&&m.op=='set')(window.__setAt=window.__setAt||[]).push(String(new Error().stack).split('\\n').slice(2,7).map(x=>x.trim().split(' ')[1]).join('<'))}}}};
window.__errs=[];addEventListener('error',e=>__errs.push(String(e.message)));
(()=>{let x=12345;Math.random=()=>{x=(x*1103515245+12345)%2147483648;return x/2147483648};
 const D0=Date,T0=D0.UTC(2026,10,21,15,0,0),p0=performance.now(),now=()=>T0+Math.floor((performance.now()-p0)/60000)*60000;
 class FD extends D0{constructor(...a){a.length?super(...a):super(now())}static now(){return now()}}window.Date=FD})();`);
const rd = cp.spawn('python3', [path.join(FX, 'native_rd.py'), String(PORT), D], {stdio: 'ignore'});
const done = c => { try { rd.kill() } catch (e) {} fs.rmSync(T, {recursive: true, force: true}); process.exit(c) };
const calls = () => { try { return fs.readFileSync(path.join(D, 'calls'), 'utf8').split('\n').filter(Boolean) } catch (e) { return [] } };
// what a step shows, without what changes by itself (times, sizes measured from the browser)
const norm = h => String(h).replace(/\b\d{1,2}:\d{2}(:\d{2})?( ?[AP]M)?\b/g, 'T').replace(/\d+(\.\d+)?px/g, 'Npx').replace(/ style="[^"]*(width|height)[^"]*"/g, ' style=S');
async function session(oa, extra) {
  const b = await chromium.launch({executablePath: CH || undefined}); const ctx = await b.newContext({viewport: {width: 1280, height: 800}});
  await ctx.grantPermissions(['clipboard-read', 'clipboard-write'], {origin: BASE}).catch(() => {});
  const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message));
  fs.writeFileSync(path.join(D, 'calls'), '');
  await p.goto(BASE + '/' + (oa ? '?oa=' + oa : '') + '#app=taskmgr&w=7&sw=1280&sh=800' + (extra || ''));
  await p.waitForFunction(() => window.__tmW && __tmW.body && __tmW.body.querySelector('.tmtabs') && __tmW.body.querySelector('.tmb').textContent.length > 20, null, {timeout: 60000});
  await wait(2500);
  const S = {steps: {}, login: await p.evaluate(() => { const l = document.getElementById('login'); return !!(l && l.offsetParent) })};
  const snap = async n => { await wait(600); S.steps[n] = norm(await p.evaluate(() => {
    const dlg = [...document.querySelectorAll('.win,.cmenu,.ctx,.rmodal,.dlg')].filter(e => e.offsetParent && e !== __tmW.el).map(e => e.innerText).join('\n---\n');
    return __tmW.el.innerHTML + '\n===\n' + dlg })) };
  const ev = js => p.evaluate(js);
  await snap('start');
  for (const t of ['apps', 'procs', 'perf', 'system', 'hw', 'start']) { await ev(`__tmW.body.querySelector(".tmtabs button[data-t='${t}']").click()`); await wait(t == 'system' || t == 'hw' ? 1500 : 800); await snap('tab ' + t) }
  await ev(`__tmW.body.querySelector(".tmtabs button[data-t='procs']").click()`); await wait(1200);
  await ev(`__tmW.body.querySelector('th[data-k=name]').click()`); await snap('sort by name');
  await ev(`__tmW.body.querySelector('th[data-k=pid]').click()`); await snap('sort by PID');
  await ev(`(()=>{const f=__tmW.body.querySelector('.tmf');f.value='web';f.dispatchEvent(new Event('input'))})()`); await snap('find web');
  await ev(`__tmW.body.querySelector('tr[data-pid="520"]').click()`); await snap('picked 520');
  await ev(`__tmW.body.querySelector('tr[data-pid="520"]').dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,clientX:300,clientY:300}))`); await snap('its menu');
  await p.keyboard.press('Escape'); await wait(300);
  await ev(`__tmW.body.querySelector('.tme').click()`); await wait(800); await snap('End process: asked');
  await ev(`(()=>{const c=[...document.querySelectorAll('button')].find(b=>/^Cancel$/.test(b.textContent.trim())&&b.offsetParent);c&&c.click()})()`); await snap('End process: cancelled');
  await ev(`__tmW.body.querySelector('.tmt2').click()`); await wait(800); await snap('End process tree: asked');
  await ev(`(()=>{const c=[...document.querySelectorAll('button')].find(b=>/^Cancel$/.test(b.textContent.trim())&&b.offsetParent);c&&c.click()})()`); await snap('End process tree: cancelled');
  await ev(`__tmW.body.querySelector(".tmtabs button[data-t='perf']").click()`); await wait(1200);
  await ev(`__tmW.body.querySelector('.tmccb').click()`); await wait(800); await snap('Clear cache');
  await ev(`__tmW.body.querySelector(".tmtabs button[data-t='system']").click()`); await wait(1500);
  await ev(`__tmW.body.querySelector('.tmrf').click()`); await wait(1500); await snap('Refresh');
  await ev(`__tmW.body.querySelector(".tmtabs button[data-t='start']").click()`); await wait(500);
  await ev(`__tmW.body.querySelector('.rs').click()`); await snap('Startup: off'); await ev(`__tmW.body.querySelector('.rs').click()`); await snap('Startup: on');
  await ev(`__tmW.body.querySelector('.tmcopy').click()`); await wait(1500); await snap('Copy system report');
  S.clip = await p.evaluate(() => navigator.clipboard.readText().catch(() => '')).then(t => norm(t).replace(/\b\d+ (s|min|h)\b/g, 'N'));
  await ev(`__tmW.body.querySelector('.tmrep').click()`); await wait(1500); await snap('Report a problem');
  S.posts = await ev(`(window.__posts||[]).map(m=>{const o=Object.assign({},m);delete o.w;return JSON.stringify(o).replace(/\\b\\d{10,}\\b/g,'N').replace(/"id":"N\\.\\d+"/g,'"id":"N"')})`);
  S.read = await ev('window.__leanRead||[]'); S.setAt = await ev('window.__setAt||[]'); S.stored = await ev("localStorage.getItem('bd95set:uAnnA1b2')"); S.small = await ev('window.__LEAN||""'); S.errs = errs.concat(await ev('window.__errs'));
  S.calls = [...new Set(calls().map(c => c.replace(/\?.*$/, '')))].sort();
  await b.close(); return S;
}
(async () => {
  try {
    for (let i = 0; i < 50; i++) { try { if ((await fetch(BASE + '/api/desktop/login', {method: 'POST', headers: {'X-Rudolf-Token': 'tok59native'}, body: JSON.stringify({n: 'Ann', p: KEYS.Ann.pw})})).ok) break } catch (e) {} await wait(200) }
    ok('0. the build made Task Manager\'s small page (index.html names it, a/ID.html is there and says what it is)', TMID && fs.existsSync(path.join(WWW, 'a', TMID + '.html')) && fs.readFileSync(path.join(WWW, 'a', TMID + '.html'), 'utf8').includes("<script>window.__OAPAGE=1;window.__LEAN=\"taskmgr\""), TMID);
    const F = await session(null, '&full=1'), L = await session(TMID);   /* (&full=1: the whole page stays the whole page) */
    ok('1. the small page is the small page, the whole page the whole page', L.small == 'taskmgr' && F.small == '', [L.small, F.small]);
    const diff = Object.keys(F.steps).filter(k => F.steps[k] !== L.steps[k]);
    ok('2. every tab and action shows the same (' + Object.keys(F.steps).length + ' steps)', !diff.length && Object.keys(L.steps).length == Object.keys(F.steps).length,
      diff.slice(0, 2).map(k => { const a = F.steps[k], b = L.steps[k]; let i = 0; while (i < a.length && a[i] == b[i]) i++; return k + ': whole …' + a.slice(Math.max(0, i - 120), i + 200) + '… small …' + (b || '').slice(Math.max(0, i - 120), i + 200) + '…' }).join(' || '));
    ok('3. the system report copied is the same', F.clip && F.clip == L.clip, [F.clip.slice(0, 200), L.clip.slice(0, 200)]);
    const cat = L.posts.filter(m => /"op":"cat"/.test(m)), only = (A, B) => { const c = {}; B.forEach(x => c[x] = (c[x] || 0) + 1); return A.filter(x => !(c[x]-- > 0)) };
    const Fp = F.posts.filter(m => !/"op":"cat"/.test(m));
    ok('4. what it told the native shell is the same (and never the start menu\'s catalogue)', !only(Fp, L.posts).length && !only(L.posts, Fp).length && !cat.length, {whole_only: only(Fp, L.posts), small_only: only(L.posts, Fp)});
    // (the whole page's sign-in also does the whole desktop's work, which the shell's hidden page does: the small page asks none of it)
    const extra = F.calls.filter(c => !L.calls.includes(c));
    console.log('asked by the whole page only: ' + extra.join(', '));
    if (process.env.LEAN_WHY) console.log('set from: whole', JSON.stringify(F.setAt), 'small', JSON.stringify(L.setAt));
    { const a = JSON.parse(F.stored || '{}'), b = JSON.parse(L.stored || '{}'), ks = [...new Set(Object.keys(a).concat(Object.keys(b)))].filter(k => JSON.stringify(a[k]) !== JSON.stringify(b[k]));
      ok('4b. the account\'s settings as the page left them are the same', !ks.length, ks.map(k => k + ': ' + JSON.stringify(a[k]) + ' / ' + JSON.stringify(b[k]))) }
    ok('5. what it asked rudolfd, the whole page asked too', L.calls.every(c => F.calls.includes(c) || c == 'PAGE oa'), L.calls.filter(c => !F.calls.includes(c)));
    ok('6. the small page never read the rest of the page; no sign-in screen; no error in either', !L.read.length && !L.login && !F.login && !L.errs.length && !F.errs.length, {read: L.read, login: [F.login, L.login], errs: [F.errs, L.errs]});
    // Ctrl+Alt+Del's window: the same in both
    const U1 = await session(null, '&urgent=1&full=1'), U2 = await session(TMID, '&urgent=1');
    { const only = (A, B) => { const c = {}; B.forEach(x => c[x] = (c[x] || 0) + 1); return A.filter(x => !(c[x]-- > 0)) }, U1p = U1.posts.filter(m => !/"op":"cat"/.test(m));
      ok('7. Ctrl+Alt+Del\'s Task Manager (&urgent=1): the same, and what it told the shell', U1.steps.start == U2.steps.start && !only(U1p, U2.posts).length && !only(U2.posts, U1p).length && !U2.read.length, {whole_only: only(U1p, U2.posts), small_only: only(U2.posts, U1p), read: U2.read}) }
    // the memory: each alone in its own browser, no automation (lean162m.js memraw), twice each
    const mem = (...a) => { const o = cp.execFileSync('node', [path.join(HERE, 'lean162m.js'), 'memraw', ...a].concat(CH ? [CH] : []), {env: Object.assign({}, process.env, {LEAN_PORT: String(PORT + 2)})}).toString(); return JSON.parse(o.split('\n').find(l => l.startsWith('{'))) };
    const W = [mem('app=taskmgr&full=1', ''), mem('app=taskmgr&full=1', '')], S = [mem('app=taskmgr', TMID), mem('app=taskmgr', TMID)];
    const avg = A => +(A.reduce((a, m) => a + m.renderer.priv, 0) / A.length).toFixed(1);
    console.log('memory (the renderer\'s private memory, MB): the whole page ' + W.map(m => m.renderer.priv).join(', ') + ' (' + avg(W) + '); the small page ' + S.map(m => m.renderer.priv).join(', ') + ' (' + avg(S) + ')');
    ok('8. the small page uses less memory than the whole page (' + avg(W) + ' -> ' + avg(S) + ' MB)', avg(S) < avg(W) - 5 && S.every(m => m.up === true && !m.read.length), {W, S});
  } catch (e) { ok('ran', false, e.stack) }
  const n = R.filter(x => x).length; console.log(n + '/' + R.length + ' passed'); done(n == R.length ? 0 : 1);
})();
