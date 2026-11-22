// 1.6.2 (docs/plan-1.6.2.md Part 29, its left part): an app's own maximize and minimize buttons and a drag of its own title bar on
// Chromium, run by xfask162_t.sh (Xvfb, rudolf-display --input, a private /run). A stand-in rudolfd as rudolfd.c answers (apps/windows
// with the asks, show with the place and max, desktop, close, web/strut), the desktop page in Chromium, and the stand-in app that
// draws its own bar (fixtures/xwmclient.c csd: _NET_WM_MOVERESIZE on a press in its bar, or with the edge in XWM_DIR); input through
// xdotool. Each request done as csd161_t.sh does it on Native; the results written to $XA_OUT/chromium.txt in the form the Native
// half writes (the app window's own geometry: what changed), for xfask162_t.sh to compare.
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path'), cp = require('child_process');
const T = process.env.XA_T, XC = process.env.XA_XC, OUT = process.env.XA_OUT, WWW = path.join(__dirname, '../../www'), PORT = 8604;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 700) : '')) };
const wait = ms => new Promise(r => setTimeout(r, ms));
const sh = c => { try { return cp.execSync(c, {stdio: ['ignore', 'pipe', 'ignore']}).toString() } catch (e) { return (e.stdout || '').toString() } };
const rf = f => { try { return fs.readFileSync(f, 'utf8') } catch (e) { return '' } };
const J = {'content-type': 'application/json'}, MT = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm', '.woff2': 'font/woff2', '.png': 'image/png', '.svg': 'image/svg+xml'};
// ---- the stand-in rudolfd: what rudolfd.c's api_apps (apps_show_place, apps_asks) and api_web do with these requests
const showPlace = (q, x) => { const v = []; for (const k of ['x', 'y', 'w', 'h']) { const s = q.get(k); if (s == null || !s.length || s.length > 6 || !/^-?\d+$/.test(s)) return null; const n = +s; if (n < (k == 'x' || k == 'y' ? -16384 : 40) || n > 16384) return null; v.push(n) } return `win ${x} ${v.join(' ')}${q.get('max') == '1' ? ' max' : ''}\n` };
const asks = () => rf('/run/rudolf/wmask').split('\n').map(l => { let m;
  if ((m = /^(\d+) at ([0-9a-f]+) (-?\d+) (-?\d+) (\d+) (\d+) ([01])$/.exec(l))) return {n: +m[1], k: 'at', win: m[2], x: +m[3], y: +m[4], w: +m[5], h: +m[6], end: m[7] == '1'};
  if ((m = /^(\d+) max ([0-9a-f]+) ([012])$/.exec(l))) return {n: +m[1], k: 'max', win: m[2], act: +m[3]};
  if ((m = /^(\d+) min ([0-9a-f]+)$/.exec(l))) return {n: +m[1], k: 'min', win: m[2]}; return null }).filter(Boolean);
const srv = http.createServer((rq, s) => { const [u0, qs] = rq.url.split('?'); const u = decodeURIComponent(u0), q = new URLSearchParams(qs || ''); rq.on('data', () => {}); rq.on('end', () => {
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(rf(path.join(WWW, 'index.html')).replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};window.__rudolfLsFlush=()=>Promise.resolve();</script>')) }
  if (u.startsWith('/api/')) { const a = u.slice(5);
    if (a == 'events/wait') return setTimeout(() => { s.writeHead(200, J); s.end('{"ok":true,"events":[]}') }, 20000);
    if (a == 'apps/windows') { const m = rf('/run/rudolf/wm').replace(/\n.*/s, ''), sm = /^win ([0-9a-f]+)/.exec(m), shown = sm ? parseInt(sm[1], 16) : 0;
      const w = rf('/run/rudolf/windows').split('\n').filter(Boolean).map(l => { const f = l.split('\t'); return f.length < 5 ? null : {win: f[0], web: f[1] == '1', shown: parseInt(f[0], 16) == shown, fullscreen: f[4] == '1', class: f[2], instance: f[3], title: f.slice(5).join('\t')} }).filter(Boolean);
      s.writeHead(200, J); return s.end(JSON.stringify({ok: true, windows: w, mode: m || 'desktop', asks: asks()})) }
    if (/^apps\/(show|close|kill)$/.test(a)) { const what = a.slice(5), x = parseInt(q.get('win') || '', 16); if (!x || !/^[0-9a-fA-F]+$/.test(q.get('win') || '')) { s.writeHead(400, J); return s.end('{"ok":false,"error":"bad window"}') }
      let b = `${what == 'show' ? 'win' : what} ${x.toString(16)}\n`; if (what == 'show') b = showPlace(q, x.toString(16)) || b;
      fs.writeFileSync(what == 'show' ? '/run/rudolf/wm' : '/run/rudolf/wmcmd', b); s.writeHead(200, J); return s.end('{"ok":true}') }
    if (a == 'apps/desktop') { fs.writeFileSync('/run/rudolf/wm', 'desktop'); s.writeHead(200, J); return s.end('{"ok":true}') }
    if (a == 'web/strut') { const side = q.get('side') == 'top' ? 'top' : 'bottom'; let n = +q.get('size') || 0; if (n < 0 || n > 200) n = 0; const bh = +q.get('bar') || 0, fr = +q.get('frame') || 0;
      fs.writeFileSync('/run/rudolf/strut', bh > 0 && bh <= 120 ? (fr > 0 && fr <= 160 ? `${side} ${n} frame ${fr} bar ${bh}\n` : `${side} ${n} bar ${bh}\n`) : `${side} ${n}\n`); fs.writeFileSync('/run/rudolf/wm', rf('/run/rudolf/wm') || 'desktop\n'); s.writeHead(200, J); return s.end('{"ok":true}') }
    s.writeHead(200, J); return s.end('{"ok":true}') }
  const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': MT[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(PORT, '127.0.0.1');
const kids = [];
const app = (args, env) => { const lg = path.join(T, 'app' + kids.length + '.log'), o = fs.openSync(lg, 'w'); const k = cp.spawn(XC, args, {stdio: ['ignore', o, o], env: Object.assign({}, process.env, env || {})}); kids.push(k); k.lg = lg; return k };
const idOf = async (k, key) => { for (let i = 0; i < 80; i++) { const m = new RegExp('^' + (key || 'id') + ' 0x([0-9a-f]+)', 'm').exec(rf(k.lg)); if (m) return m[1]; await wait(100) } return '' };
const xgeo = id => { const o = sh(`xwininfo -id 0x${id}`); const g = k => +((new RegExp(k + ':\\s+(-?\\d+)')).exec(o) || [])[1]; return o ? [g('Absolute upper-left X'), g('Absolute upper-left Y'), g('Width'), g('Height')] : null };
const wmNow = () => rf('/run/rudolf/wm').trim();
const xd = c => sh('xdotool ' + c);
const near = (a, b, t) => a && b && a.length == b.length && a.every((v, i) => Math.abs(v - b[i]) <= (t == null ? 1 : t));
const stack = () => sh(`${XC} stack`).trim().split('\n').map(x => x.replace(/^0x/, ''));
const above = (a, b) => { const s = stack(); return s.indexOf(a) > s.indexOf(b) && s.indexOf(b) >= 0 };
const DIRF = path.join(T, 'dir'), setDir = d => fs.writeFileSync(DIRF, String(d));
// a drag with the pointer from X Y by DX DY in five steps, as csd161_t.sh's pdrag (the app asks _NET_WM_MOVERESIZE on the press)
const pdrag = async (x, y, dx, dy, dir) => { setDir(dir); xd(`mousemove ${x} ${y} mousedown 1`); await wait(300); for (let k = 1; k <= 5; k++) { xd(`mousemove ${Math.round(x + dx * k / 5)} ${Math.round(y + dy * k / 5)}`); await wait(80) } xd('mouseup 1'); await wait(400) };
const LINES = []; const rec = (k, v) => LINES.push(k + ' ' + (Array.isArray(v) ? v.join(' ') : v));
const dlt = (a, b) => a && b ? [b[0] - a[0], b[1] - a[1], b[2] - a[2], b[3] - a[3]] : ['?'];
(async () => {
  const ctx = await chromium.launchPersistentContext(path.join(T, 'prof'), {headless: false, viewport: null, executablePath: process.env.XA_CHROME || undefined,
    /* (no --enable-automation: Chrome's stable builds show their "controlled by automated test software" bar for it, 56 px
       above the page, so the page no longer starts at the screen's corner and every press through xdotool misses) */
    ignoreDefaultArgs: ['--enable-automation'],
    args: ['--class=RudolfDesktop', '--kiosk', '--test-type', '--disable-infobars', '--no-first-run', '--force-device-scale-factor=1', '--window-position=0,0', '--window-size=1280,800', '--disable-gpu'], env: Object.assign({}, process.env)});
  const p = ctx.pages()[0] || await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto('http://127.0.0.1:' + PORT + '/'); await p.waitForFunction(() => typeof xfAsks == 'function' && typeof lxWinPoll == 'function', null, {timeout: 60000});
  const q = (f, a) => p.evaluate(f, a);
  await q(() => { try { svj('bd95setup', 1) } catch (e) {} ['oobe', 'login'].forEach(i => { const o = document.getElementById(i); o && o.remove() }); ACCTS = [{n: 'Ann', r: 'admin', h: 'x', id: 'ann'}]; USER = ACCTS[0]; S.accent = '#2563eb'; S.dm = 'light'; themeApply(); applySet(); try { userUI() } catch (e) {} webStrut.last = null; webStrut() });
  await wait(1500);
  const desk = sh(`xdotool search --class RudolfDesktop`).trim().split('\n').map(x => (+x).toString(16)).find(x => { const g = xgeo(x); return g && g[2] >= 1200 }) || '';
  const SUP = ' ' + sh(`${XC} supported`).split('\n').join(' ') + ' ';
  ok('A1. rudolf-display says it does what an app\'s own bar and buttons ask (_NET_SUPPORTED: _NET_WM_MOVERESIZE, MAXIMIZED_VERT and _HORZ, HIDDEN), as rudolf-native does (GTK moves a window itself without it)', ['_NET_WM_MOVERESIZE', '_NET_WM_STATE_MAXIMIZED_VERT', '_NET_WM_STATE_MAXIMIZED_HORZ', '_NET_WM_STATE_HIDDEN'].every(a => SUP.includes(' ' + a + ' ')), SUP);
  // ---- the stand-in drawing its own bar (Electron's frame: false, a GTK header bar), framed by the page
  setDir(8); const C = app(['csd', '500', '360'], {XWM_DIR: DIRF}); const ID = await idOf(C);
  const fr = () => q(id => { const el = document.querySelector(`.xfr[data-xwin="${id}"]`); if (!el) return null; const F = XF.map.get(id), b = el.querySelector('.body').getBoundingClientRect(), r = el.getBoundingClientRect();
    return {max: el.classList.contains('max'), act: el.classList.contains('act'), min: !!(F && F.W.min), inside: [Math.round(b.left), Math.round(b.top), Math.round(b.width), Math.round(b.height)], el: [r.left, r.top, r.width, r.height].map(Math.round),
      mx: (() => { const x = el.querySelector('.tb .mx').getBoundingClientRect(); return [Math.round(x.left + x.width / 2), Math.round(x.top + x.height / 2)] })()} }, ID);
  const follow = async (ms) => { let f, g; for (let i = 0; i < (ms || 3000) / 100; i++) { f = await fr(); g = xgeo(ID); if (f && near(g, f.inside, 1)) return [true, f, g]; await wait(100) } return [false, f, g] };
  await p.waitForSelector(`.xfr[data-xwin="${ID}"]`, {timeout: 6000}).catch(() => {});
  let [inF, f, g] = await follow(4000);
  // a restored frame of the window's own size, away from the screen's edges (as on Native, where it opens at its own size)
  await q(id => { const W = XF.map.get(id).W; W.focus(); if (W.el.classList.contains('max')) W.el.querySelector('.mx').click() }, ID); await wait(900);
  await q(id => { const W = XF.map.get(id).W; W.el.style.left = '200px'; W.el.style.top = '120px'; W.el.style.width = '502px'; W.el.style.height = '396px'; W.focus() }, ID); await wait(900); [inF, f, g] = await follow();
  const T0 = f && f.inside[1] - f.el[1];
  ok('A2. the app drawing its own bar: framed by the page, its window in the frame\'s inside (35 px of Rudolf\'s bar above it, as Native\'s framed window)', inF && !f.max && T0 == 35 && near(g.slice(2), [500, 360], 2), [f, g]);
  // ---- moving by its own bar
  let G0 = g; await pdrag(g[0] + 100, g[1] + 15, 150, 90, 8); [inF, f, g] = await follow();
  ok('A3. dragging its own bar moves it (_NET_WM_MOVERESIZE, move): the pointer\'s way, Rudolf\'s frame following', inF && near(dlt(G0, g), [150, 90, 0, 0], 0), [G0, g, f]); rec('move', dlt(G0, g));
  const G1 = g; xd(`mousemove ${g[0] + 300} ${g[1] + 250}`); await wait(600); g = xgeo(ID);
  ok('A4. ...the drag ends with the button: the pointer moving afterwards leaves it', near(g, G1, 0), [G1, g]);
  await pdrag(g[0] + 250, g[1] + 15, 0, -400, 8); [inF, f, g] = await follow();
  ok('A5. ...as Rudolf\'s own title bar: its frame not above the desktop\'s top (web/strut\'s frame: its top edge above the app)', inF && f.el[1] == 0 && / frame 35 bar /.test(rf('/run/rudolf/strut')), [f, g, rf('/run/rudolf/strut')]); rec('top', [g[1] - 35]);
  await pdrag(g[0] + 250, g[1] + 15, -150, 140, 8); [inF, f, g] = await follow();
  // ---- resizing from each edge and corner (the app asks with the edge: 0 top-left, clockwise to 7 left)
  const bad = []; for (let d = 0; d < 8; d++) { const a = (await follow())[2]; await pdrag(a[0] + 250, a[1] + 200, 40, 30, d); const r = await follow(); rec('rs' + d, dlt(a, r[2])); if (!r[0]) bad.push([d, r]) }
  ok('A6. a resize from each edge and corner (_NET_WM_MOVERESIZE 0-7): the frame follows (the numbers compared with Native\'s below)', bad.length == 0, bad);
  // ---- the keyboard's move and size
  [inF, f, g] = await follow(); G0 = g; setDir(10); xd(`mousemove ${g[0] + 250} ${g[1] + 200} click 1`); await wait(400); xd('key Right Right Right Down Down'); await wait(300); xd('key Return'); await wait(500); [inF, f, g] = await follow();
  ok('A7. the keyboard\'s move (_NET_WM_MOVERESIZE 10): the arrows 10 px a step, Enter ends it; the frame follows', inF && near(dlt(G0, g), [30, 20, 0, 0], 0), [G0, g]); rec('kmove', dlt(G0, g));
  G0 = g; setDir(9); xd(`mousemove ${g[0] + 250} ${g[1] + 200} click 1`); await wait(400); xd('key Right Right Down'); await wait(300); xd('key Return'); await wait(500); [inF, f, g] = await follow();
  ok('A8. the keyboard\'s size (9): from the bottom-right corner, as a drag of it', inF && near(dlt(G0, g), [0, 0, 22, 12], 0), [G0, g]); rec('ksize', dlt(G0, g));
  G0 = g; setDir(10); xd(`mousemove ${g[0] + 250} ${g[1] + 200} click 1`); await wait(400); xd('key Left Left Up'); await wait(300); xd('key Escape'); await wait(500); [inF, f, g] = await follow();
  ok('A9. ...Escape puts it back where it was', inF && near(g, G0, 0), [G0, g]); rec('kesc', dlt(G0, g));
  xd('key a'); await wait(300); setDir(8);
  ok('A10. ...and the keyboard is the app\'s again (the grab gone)', sh(`${XC} getfocus`).trim() == '0x' + ID, sh(`${XC} getfocus`));
  // ---- maximize and restore from the app, minimize
  G0 = g; sh(`${XC} max 0x${ID} 1`); await wait(1200); [inF, f, g] = await follow();
  const nst = () => (/^state (.*)$/m.exec(sh(`${XC} props 0x${ID}`)) || [])[1];
  ok('A11. maximize from the app (_NET_WM_STATE_MAXIMIZED_VERT and _HORZ): Rudolf\'s maximize of its frame, the app in it', inF && f.max, [f, g]); rec('maxgeo', g);
  ok('A12. ...and its _NET_WM_STATE says so (the app shows its restore button)', nst() == 'MAXIMIZED_VERT,MAXIMIZED_HORZ' && /^state MAXIMIZED_VERT,MAXIMIZED_HORZ/m.test(rf(C.lg)), nst()); rec('maxst', nst());
  const GM = g; await pdrag(400, g[1] + 15, 100, 100, 8); [inF, f, g] = await follow();
  ok('A13. ...maximized, its bar does not move it (as Rudolf\'s own title bar)', inF && near(g, GM, 0) && f.max, [GM, g]); rec('maxdrag', dlt(GM, g));
  sh(`${XC} max 0x${ID} 2`); await wait(1200); [inF, f, g] = await follow();
  ok('A14. restore (toggled): back to its size and place, the state cleared', inF && !f.max && near(g, G0, 0) && nst() == '-', [G0, g, nst()]); rec('restore', dlt(G0, g)); rec('resst', nst());
  sh(`${XC} iconify 0x${ID}`); await wait(1200); f = await fr();
  ok('A15. minimize from the app (WM_CHANGE_STATE iconic): its frame minimized, the app behind the desktop page', f && f.min && wmNow() == 'desktop' && above(desk, ID), [f, wmNow()]); rec('iconify', f && f.min ? 'min' : '-');
  const lb = await q(id => { const b = document.querySelector(`#tasks .lxtb[data-win="${id}"]`).getBoundingClientRect(); return [Math.round(b.left + b.width / 2), Math.round(b.top + b.height / 2)] }, ID);
  xd(`mousemove ${lb[0]} ${lb[1]} click 1`); await wait(1200); [inF, f, g] = await follow();
  ok('A16. ...its taskbar button brings it back, the same size and place', inF && !f.min && near(g, G0, 0) && above(ID, desk), [G0, g]); rec('back', dlt(G0, g));
  sh(`${XC} hide 0x${ID}`); await wait(1200); f = await fr();
  ok('A17. minimize asked with _NET_WM_STATE_HIDDEN: minimized as well', f && f.min && wmNow() == 'desktop', [f, wmNow()]); rec('hide', f && f.min ? 'min' : '-');
  xd(`mousemove ${lb[0]} ${lb[1]} click 1`); await wait(1200); [inF, f, g] = await follow();
  // ---- Rudolf's own bar on the same frame, as before; then a stale place from the page never moves it back
  const G2 = g, fa = f; xd(`mousemove ${fa.el[0] + 120} ${fa.el[1] + 12} mousedown 1`); await wait(150); for (let k = 1; k <= 6; k++) { xd(`mousemove ${fa.el[0] + 120 + k * 10} ${fa.el[1] + 12 + k * 5}`); await wait(60) } xd('mouseup 1'); await wait(700); [inF, f, g] = await follow();
  ok('A18. Rudolf\'s own title bar on it still moves it (the page\'s drag), the app following', inF && near(dlt(G2, g), [60, 30, 0, 0], 0), [G2, g]);
  // the asks: only after the page was loaded; numbers increasing; what rudolfd passes on
  const as = asks(); ok('A19. rudolf-display\'s asks (/run/rudolf/wmask): increasing numbers, at most 16, the kinds used (at, max, min)', as.length > 0 && as.length <= 16 && as.every((a, i) => !i || a.n > as[i - 1].n) && ['at', 'max', 'min'].every(k => as.some(a => a.k == k)), rf('/run/rudolf/wmask'));
  // ---- a real app's own header bar, when the host has GTK 3's widget factory (it moves its window itself without _NET_WM_MOVERESIZE)
  if (sh('command -v gtk3-widget-factory').trim()) { C.kill(); await wait(1500);
    const before = new Set(rf('/run/rudolf/windows').split('\n').map(l => l.split('\t')[0])); const k = cp.spawn('gtk3-widget-factory', [], {stdio: 'ignore', env: Object.assign({}, process.env, {NO_AT_BRIDGE: '1', GDK_BACKEND: 'x11', GTK_A11Y: 'none'})}); kids.push(k);
    let gid = ''; for (let i = 0; i < 150 && !gid; i++) { await wait(100); gid = rf('/run/rudolf/windows').split('\n').map(l => l.split('\t')[0]).find(x => x && !before.has(x)) || '' }
    const gfr = () => q(id => { const el = document.querySelector(`.xfr[data-xwin="${id}"]`); if (!el) return null; const b = el.querySelector('.body').getBoundingClientRect(); return {max: el.classList.contains('max'), inside: [Math.round(b.left), Math.round(b.top), Math.round(b.width), Math.round(b.height)]} }, gid);
    const gfol = async () => { let f1, g1; for (let i = 0; i < 40; i++) { f1 = await gfr(); g1 = xgeo(gid); if (f1 && near(g1, f1.inside, 1)) return [true, f1, g1]; await wait(100) } return [false, f1, g1] };
    await p.waitForSelector(`.xfr[data-xwin="${gid}"]`, {timeout: 8000}).catch(() => {});
    await q(id => { const W = XF.map.get(id).W; W.focus(); if (W.el.classList.contains('max')) W.el.querySelector('.mx').click() }, gid); await wait(1200);
    await q(id => { const W = XF.map.get(id).W; W.el.style.left = '100px'; W.el.style.top = '100px'; W.focus() }, gid); await wait(1200);
    let [o1, f1, g1] = await gfol(); const a0 = g1;
    /* (its header bar's empty part, left of the page switcher: GTK asks the window manager to move it) */
    xd(`mousemove ${a0[0] + Math.round(a0[2] * 0.12)} ${a0[1] + 22} mousedown 1`); await wait(300); for (let s = 1; s <= 6; s++) { xd(`mousemove ${a0[0] + Math.round(a0[2] * 0.12) + s * 20} ${a0[1] + 22 + s * 10}`); await wait(80) } xd('mouseup 1'); await wait(900);
    [o1, f1, g1] = await gfol();
    ok(`A21. GTK ${sh('pkg-config --modversion gtk+-3.0').trim() || '3'}'s widget factory: dragging its own header bar moves it (GTK asks _NET_WM_MOVERESIZE now that rudolf-display says it does it), Rudolf's frame following`, o1 && a0 && Math.abs(g1[0] - a0[0] - 120) <= 2 && Math.abs(g1[1] - a0[1] - 60) <= 2, [a0, g1, f1]);
    xd(`mousemove ${g1[0] + Math.round(g1[2] * 0.12)} ${g1[1] + 22}`); await wait(300);
    xd('mousedown 1 mouseup 1 mousedown 1 mouseup 1'); await wait(1500); [o1, f1, g1] = await gfol();
    ok('A22. ...a double-click on its header bar maximizes its frame (GTK asks _NET_WM_STATE), as Rudolf\'s □', o1 && f1.max, [f1, g1, sh(`xprop -id 0x${gid} _NET_WM_STATE`)]);
    k.kill(); await wait(1000) } else console.log('SKIP A21-A22: gtk3-widget-factory is not installed');
  ok('A20. no page errors', errs.length == 0, errs);
  fs.writeFileSync(path.join(OUT, 'chromium.txt'), LINES.join('\n') + '\n');
  kids.forEach(k => { try { k.kill() } catch (e) {} }); await ctx.close(); srv.close();
  console.log(`${R.filter(Boolean).length}/${R.length} passed`); process.exit(R.every(Boolean) ? 0 : 1)
})().catch(e => { console.log('FAIL ' + (e && e.stack || e)); process.exit(1) });
