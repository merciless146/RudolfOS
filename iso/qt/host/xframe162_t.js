// 1.6.2 (docs/plan-1.6.2.md Part 29): Rudolf's title bar on Standard's windows from outside the page; run by xframe162_t.sh (which
// starts Xvfb with two monitors, rudolf-display --input and a private /run). Here: a stand-in rudolfd (apps/windows, show, desktop,
// close, kill and web/strut as rudolfd.c answers them, from and to rudolf-display's own files), the desktop page in Chromium as the
// desktop's window, the stand-in apps (xwmclient) and real ones; input through xdotool (the X server's own pointer, so each press
// lands on whichever window is there, the page's or the app's).
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path'), cp = require('child_process');
const T = process.env.XF_T, XC = process.env.XF_XC, OUT = process.env.XF_OUT, WWW = path.join(__dirname, '../../www'), PORT = 8602;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 700) : '')) };
const wait = ms => new Promise(r => setTimeout(r, ms));
const sh = c => { try { return cp.execSync(c, {stdio: ['ignore', 'pipe', 'ignore']}).toString() } catch (e) { return (e.stdout || '').toString() } };
const rf = f => { try { return fs.readFileSync(f, 'utf8') } catch (e) { return '' } };
const J = {'content-type': 'application/json'}, MT = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm', '.woff2': 'font/woff2', '.png': 'image/png', '.svg': 'image/svg+xml'};
const CALLS = [];
// ---- the stand-in rudolfd: what rudolfd.c's api_apps and api_web do with these requests
const showPlace = (q, x) => { const v = []; for (const k of ['x', 'y', 'w', 'h']) { const s = q.get(k); if (s == null || !s.length || s.length > 6 || !/^-?\d+$/.test(s)) return null; const n = +s; if (n < (k == 'x' || k == 'y' ? -16384 : 40) || n > 16384) return null; v.push(n) } return `win ${x} ${v.join(' ')}\n` };
const srv = http.createServer((rq, s) => { const [u0, qs] = rq.url.split('?'); const u = decodeURIComponent(u0), q = new URLSearchParams(qs || ''); rq.on('data', () => {}); rq.on('end', () => {
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(rf(path.join(WWW, 'index.html')).replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};window.__rudolfLsFlush=()=>Promise.resolve();</script>')) }
  if (u.startsWith('/api/')) { const a = u.slice(5); CALLS.push(a + (qs ? '?' + qs : ''));
    if (a == 'events/wait') return setTimeout(() => { s.writeHead(200, J); s.end('{"ok":true,"events":[]}') }, 20000);
    if (a == 'apps/windows') { const m = rf('/run/rudolf/wm').replace(/\n.*/s, ''), sm = /^win ([0-9a-f]+)/.exec(m), shown = sm ? parseInt(sm[1], 16) : 0;
      const w = rf('/run/rudolf/windows').split('\n').filter(Boolean).map(l => { const f = l.split('\t'); return f.length < 5 ? null : {win: f[0], web: f[1] == '1', shown: parseInt(f[0], 16) == shown, fullscreen: f[4] == '1', class: f[2], instance: f[3], title: f.slice(5).join('\t')} }).filter(Boolean);
      s.writeHead(200, J); return s.end(JSON.stringify({ok: true, windows: w, mode: m || 'desktop'})) }
    if (/^apps\/(show|close|kill)$/.test(a)) { const what = a.slice(5), x = parseInt(q.get('win') || '', 16); if (!x || !/^[0-9a-fA-F]+$/.test(q.get('win') || '')) { s.writeHead(400, J); return s.end('{"ok":false,"error":"bad window"}') }
      let b = `${what == 'show' ? 'win' : what} ${x.toString(16)}\n`; if (what == 'show') b = showPlace(q, x.toString(16)) || b;
      fs.writeFileSync(what == 'show' ? '/run/rudolf/wm' : '/run/rudolf/wmcmd', b); s.writeHead(200, J); return s.end('{"ok":true}') }
    if (a == 'apps/desktop') { fs.writeFileSync('/run/rudolf/wm', 'desktop'); s.writeHead(200, J); return s.end('{"ok":true}') }
    if (a == 'web/strut') { const side = q.get('side') == 'top' ? 'top' : 'bottom'; let n = +q.get('size') || 0; if (n < 0 || n > 200) n = 0; const bh = +q.get('bar') || 0;
      fs.writeFileSync('/run/rudolf/strut', bh > 0 && bh <= 120 ? `${side} ${n} bar ${bh}\n` : `${side} ${n}\n`); fs.writeFileSync('/run/rudolf/wm', rf('/run/rudolf/wm') || 'desktop\n'); s.writeHead(200, J); return s.end('{"ok":true}') }
    s.writeHead(200, J); return s.end('{"ok":true}') }
  const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': MT[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(PORT, '127.0.0.1');
// ---- the X side
const kids = [];
const app = (args, env) => { const lg = path.join(T, 'app' + kids.length + '.log'), o = fs.openSync(lg, 'w'); const k = cp.spawn(XC, args, {stdio: ['ignore', o, o], env: Object.assign({}, process.env, env || {})}); kids.push(k); k.lg = lg; return k };
const idOf = async (k, key) => { for (let i = 0; i < 80; i++) { const m = new RegExp('^' + (key || 'id') + ' 0x([0-9a-f]+)', 'm').exec(rf(k.lg)); if (m) return m[1]; await wait(100) } return '' };
const xgeo = id => { const o = sh(`xwininfo -id 0x${id}`); const g = k => +((new RegExp(k + ':\\s+(-?\\d+)')).exec(o) || [])[1]; return o ? [g('Absolute upper-left X'), g('Absolute upper-left Y'), g('Width'), g('Height')] : null };
const viewable = id => /Map State: IsViewable/.test(sh(`xwininfo -id 0x${id}`));
const stack = () => sh(`${XC} stack`).trim().split('\n').map(x => x.replace(/^0x/, ''));
const above = (a, b) => { const s = stack(); return s.indexOf(a) > s.indexOf(b) && s.indexOf(b) >= 0 };
const wmNow = () => rf('/run/rudolf/wm').trim();
const xd = c => sh('xdotool ' + c);
const shot = () => { sh(`import -window root ${T}/s.png`); };
const px = (x, y) => sh(`convert ${T}/s.png -format "%[fx:int(255*p{${x},${y}}.r)] %[fx:int(255*p{${x},${y}}.g)] %[fx:int(255*p{${x},${y}}.b)]" info:`).trim().split(' ').map(Number);
const near = (a, b, t) => a && b && a.length == b.length && a.every((v, i) => Math.abs(v - b[i]) <= (t == null ? 1 : t));
const keep = n => { try { fs.copyFileSync(T + '/s.png', path.join(OUT, n + '.png')) } catch (e) {} };
const drag = async (x, y, dx, dy, steps) => { steps = steps || 6; xd(`mousemove ${x} ${y} mousedown 1`); await wait(150); for (let k = 1; k <= steps; k++) { xd(`mousemove ${Math.round(x + dx * k / steps)} ${Math.round(y + dy * k / steps)}`); await wait(60) } await wait(100); xd('mouseup 1'); await wait(700) };
(async () => {
  const ctx = await chromium.launchPersistentContext(path.join(T, 'prof'), {headless: false, viewport: null, executablePath: process.env.XF_CHROME || undefined,
    args: ['--class=RudolfDesktop', '--kiosk', '--test-type', '--disable-infobars', '--no-first-run', '--force-device-scale-factor=1', '--window-position=0,0', '--window-size=1280,800', '--disable-gpu'], env: Object.assign({}, process.env)});
  const p = ctx.pages()[0] || await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto('http://127.0.0.1:' + PORT + '/'); await p.waitForFunction(() => typeof xfSync == 'function' && typeof lxWinPoll == 'function', null, {timeout: 60000});
  const q = (f, a) => p.evaluate(f, a);
  /* (the page must fill the screen from its top-left corner, as the session's kiosk window does: no bar of Chromium's above it,
     such as the one for --no-sandbox that Chrome's stable builds show) */
  const top0 = await q(() => [screenX, screenY, outerHeight - innerHeight, innerWidth, innerHeight]);
  if (top0[2] > 0 || top0[0] || top0[1]) console.log('(note: the page is not at the screen\'s corner: ' + JSON.stringify(top0) + ')');
  await q(() => { try { svj('bd95setup', 1) } catch (e) {} ['oobe', 'login'].forEach(i => { const o = document.getElementById(i); o && o.remove() }); ACCTS = [{n: 'Ann', r: 'admin', h: 'x', id: 'ann'}]; USER = ACCTS[0]; S.accent = '#2563eb'; S.dm = 'light'; themeApply(); applySet(); try { userUI() } catch (e) {} webStrut.last = null; webStrut() });
  await wait(1500);
  // the desktop's window: the whole X screen
  const desk = sh(`xdotool search --class RudolfDesktop`).trim().split('\n').map(x => (+x).toString(16)).find(x => { const g = xgeo(x); return g && g[2] >= 1200 }) || '';
  // ---- 1. the window manager announced
  const chk = (/window id # (0x[0-9a-f]+)/.exec(sh('xprop -root _NET_SUPPORTING_WM_CHECK')) || [])[1], nm = chk ? sh(`xprop -id ${chk} _NET_WM_NAME`) : '', sup = sh('xprop -root _NET_SUPPORTED');
  ok('1. rudolf-display says it is the window manager (_NET_SUPPORTING_WM_CHECK, named rudolf-display; _NET_SUPPORTED: _NET_WM_STATE, FULLSCREEN), as rudolf-native does', !!chk && /"rudolf-display"/.test(nm) && /_NET_WM_STATE_FULLSCREEN/.test(sup) && /_NET_WM_STATE\b/.test(sup), [chk, nm, sup]);
  const bar = await q(() => xfBarPx());
  ok('2. the page gives rudolf-display the strip for its title bar (web/strut\'s bar: Rudolf\'s title bar\'s height)', bar >= 24 && bar <= 60 && rf('/run/rudolf/strut').trim() == `bottom 48 bar ${bar}` || (bar >= 24 && new RegExp(` bar ${bar}$`).test(rf('/run/rudolf/strut').trim())), [bar, rf('/run/rudolf/strut')]);
  const tbH = await q(() => { const s = rf => 0; const b = document.getElementById('bar'); return Math.round(innerHeight - b.getBoundingClientRect().top) });
  // ---- 3. an app asking for a frame
  const A = app(['app']); const ID = await idOf(A);
  let g0 = null; for (let i = 0; i < 30 && !g0; i++) { const m = /^geom (.*)$/m.exec(rf(A.lg)); if (m) g0 = m[1].split(' ').map(Number); else await wait(50) }
  const st = rf('/run/rudolf/strut').trim().split(' '), sb = +st[1];
  ok('3. a new window, before the page frames it: fitted on the main monitor below the strip and above the taskbar', near(g0, [0, bar, 1280, 800 - sb - bar], 0), [g0, bar, sb]);
  await p.waitForSelector(`.xfr[data-xwin="${ID}"]`, {timeout: 6000}).catch(() => {});
  const fr = () => q(id => { const el = document.querySelector(`.xfr[data-xwin="${id}"]`); if (!el) return null; const F = el && XF.map.get(id), b = el.querySelector('.body').getBoundingClientRect(), r = el.getBoundingClientRect(), tb = el.querySelector('.tb').getBoundingClientRect();
    const bt = s => { const e = el.querySelector('.tb ' + s); if (!e) return null; const x = e.getBoundingClientRect(); return [Math.round(x.left + x.width / 2), Math.round(x.top + x.height / 2)] };
    return {title: el.querySelector('.t').textContent, max: el.classList.contains('max'), act: el.classList.contains('act'), shown: getComputedStyle(el).display != 'none' && getComputedStyle(el).visibility != 'hidden', min: !!(F && F.W.min),
      inside: [Math.round(b.left), Math.round(b.top), Math.round(b.width), Math.round(b.height)], el: [r.left, r.top, r.width, r.height].map(Math.round), tb: [tb.left, tb.top, tb.width, tb.height].map(Math.round),
      mn: bt('.mn'), mx: bt('.mx'), cl: bt('.cl'), ws: F && F.W.ws, tbbg: getComputedStyle(el.querySelector('.tb')).backgroundColor, btn: !!document.querySelector(`#tasks .lxtb[data-win="${id}"]`)} }, ID);
  const follow = async (id, ms) => { let f, g; for (let i = 0; i < (ms || 3000) / 100; i++) { f = await fr(); g = xgeo(id); if (f && near(g, f.inside, 1)) return [true, f, g]; await wait(100) } return [false, f, g] };
  let [inF, f, g] = await follow(ID, 4000);
  ok('4. the page frames it: a window of the page\'s own (Rudolf\'s title bar with the app\'s title, _ □ ✕), maximized as Standard fitted it, its taskbar button the app\'s', f && f.title == 'Starport stand-in' && f.max && f.act && f.mn && f.mx && f.cl && f.btn, f);
  ok('5. ...the app\'s window is placed in the frame\'s inside, shown above the desktop', inF && above(ID, desk) && viewable(ID) && /^win [0-9a-f]+ -?\d+ \d+ \d+ \d+$/.test(wmNow()), [f && f.inside, g, wmNow(), stack()]);
  { const e = f && [f.inside[0] - f.el[0], f.el[0] + f.el[2] - f.inside[0] - f.inside[2], f.inside[1] - f.el[1], f.el[1] + f.el[3] - f.inside[1] - f.inside[3]];
    ok('5b. as Native frames the same window: the frame around the app 1 px at the sides and bottom, 35 px above (Rudolf\'s title bar), Native\'s _NET_FRAME_EXTENTS 1 1 35 1 (csd161_t.sh 4)', e && e.join(' ') == '1 1 35 1', e) }
  await wait(1500); shot(); keep('5-framed');   /* (after the window's opening transition) */
  const tbc = f && f.tbbg.match(/\d+/g).slice(0, 3).map(Number), tp = f && px(f.tb[0] + 60, f.tb[1] + 3), ip = f && px(f.inside[0] + 40, f.inside[1] + 40);
  ok('6. on the screen: Rudolf\'s title bar (the accent colour) above the app, the app\'s own pixels in the frame', near(tp, tbc, 6) && near(ip, [0, 0, 0], 2), [tp, tbc, ip]);
  const kb = sh(`${XC} getfocus`).trim().replace(/^0x/, '');
  ok('7. ...and it has the keyboard', kb == ID, [kb, ID]);
  // ---- 8. restore, then move by its title bar
  xd(`mousemove ${f.mx[0]} ${f.mx[1]} click 1`); await wait(900); [inF, f, g] = await follow(ID);
  ok('8. □ restores the frame (a window\'s size); the app follows into its inside', f && !f.max && inF && f.el[2] < 1200, [f && f.el, g]);
  const f0 = f; await drag(f.tb[0] + 200, f.tb[1] + 12, 160, 90); [inF, f, g] = await follow(ID);
  ok('9. dragging Rudolf\'s title bar moves the frame, and the app with it', f && f.el[0] - f0.el[0] == 160 && f.el[1] - f0.el[1] == 90 && inF, [f0 && f0.el, f && f.el, g]);
  // the app keeps up while the bar is dragged (rudolf-display reads the place every 15 ms then): its place within a step of the frame's
  { const fa = f; xd(`mousemove ${fa.tb[0] + 200} ${fa.tb[1] + 12} mousedown 1`); await wait(150); let worst = 0;
    for (let k = 1; k <= 10; k++) { xd(`mousemove ${fa.tb[0] + 200 + k * 12} ${fa.tb[1] + 12}`); await wait(120); const gg = xgeo(ID), ff = await fr(); worst = Math.max(worst, Math.abs(gg[0] - ff.inside[0])) }
    xd('mouseup 1'); await wait(600); [inF, f, g] = await follow(ID);
    ok('10. ...and keeps up during the drag (at most one 12 px step behind, 120 ms apart)', worst <= 12 && inF, worst) }
  // ---- 11. resizing from each edge and corner
  const rs = id => q(id => [...document.querySelectorAll(`.xfr[data-xwin="${id}"] > .rsz`)].map(h => { const r = h.getBoundingClientRect(); const k = h.className.replace(/.*rsz-/, '');
    /* (pressed in the handle's outer part, beyond the frame's edge: the app's window is right inside the frame's 1 px edge) */
    return [k, Math.round(k.includes('e') ? r.right - 2 : k.includes('w') ? r.left + 1 : r.left + r.width / 2), Math.round(k.includes('s') ? r.bottom - 2 : k.includes('n') ? r.top + 1 : r.top + r.height / 2)] }), id);
  const D = {n: [0, -30], s: [0, 30], e: [30, 0], w: [-30, 0], ne: [30, -30], nw: [-30, -30], se: [30, 30], sw: [-30, 30]}; const bad = [];
  for (const [k] of await rs(ID)) { const h = (await rs(ID)).find(x => x[0] == k), b0 = await fr(); await drag(h[1], h[2], D[k][0], D[k][1], 4); const [o2, b1, g1] = await follow(ID);
    const dw = b1.el[2] - b0.el[2], dh = b1.el[3] - b0.el[3], dx = b1.el[0] - b0.el[0], dy = b1.el[1] - b0.el[1];
    const want = {w: k.includes('e') ? 30 : k.includes('w') ? 30 : 0, h: k.includes('s') ? 30 : k.includes('n') ? 30 : 0, x: k.includes('w') ? -30 : 0, y: k.includes('n') ? -30 : 0};
    /* (the page's own sizing adds the 2 px of the window's border to its new width: as for every window of Rudolf's) */
    if (!(o2 && Math.abs(dw - want.w) <= 2 && Math.abs(dh - want.h) <= 2 && dx == want.x && dy == want.y)) bad.push([k, dw, dh, dx, dy, o2, g1]) }
  ok('11. sizing from each of the 8 edges and corners (pressed where only the page has pixels: the 3 px beyond the frame\'s edge): the frame, and the app with it', bad.length == 0 && (await rs(ID)).length == 8, bad);
  // ---- 12. the title bar's styles and colours are the page's
  await q(() => { S.twstyle = 'candy'; S.twacm = 'comp'; twApply() }); await wait(300);
  const cs = await q(id => { const t = document.querySelector(`.xfr[data-xwin="${id}"] .tb`), w = win('Probe', '📝', 300, 200); w.el.classList.remove('act'); document.querySelector(`.xfr[data-xwin="${id}"]`).classList.add('act'); const a = getComputedStyle(t), mx = getComputedStyle(t.querySelector('.mx'));
    const r = [a.backgroundImage, mx.borderRadius, a.color]; w.focus(); const b = getComputedStyle(w.el.querySelector('.tb')), bm = getComputedStyle(w.el.querySelector('.tb .mx')); const r2 = [b.backgroundImage, bm.borderRadius, b.color]; w.close(); return [r, r2] }, ID);
  ok('12. the title bar\'s style and colour (Display Properties › Title bars: Candy, Complementary here) drawn on the frame as on Rudolf\'s own windows', cs[0][0] == cs[1][0] && cs[0][1] == '999px' && cs[0][1] == cs[1][1] && cs[0][2] == cs[1][2], cs);
  await q(() => { S.twstyle = 'standard'; S.twacm = 'normal'; twApply() }); await wait(300);
  // the Probe window closed: the frame active again (the page focused it), the app shown
  await q(id => XF.map.get(id).W.focus(), ID); [inF, f, g] = await follow(ID);
  // ---- 13. maximize again
  xd(`mousemove ${f.mx[0]} ${f.mx[1]} click 1`); await wait(900); [inF, f, g] = await follow(ID);
  ok('13. □ again maximizes it: the app fills the maximized frame\'s inside', f && f.max && inF, [f && f.el, g]);
  // ---- 14. minimize, and back from its taskbar button
  xd(`mousemove ${f.mn[0]} ${f.mn[1]} click 1`); await wait(1000); f = await fr();
  ok('14. _ minimizes it: the frame goes, the app goes behind the desktop page (rudolf-display: desktop)', f && f.min && !f.shown && wmNow() == 'desktop' && above(desk, ID), [f, wmNow(), stack()]);
  const lb = await q(id => { const b = document.querySelector(`#tasks .lxtb[data-win="${id}"]`).getBoundingClientRect(); return [Math.round(b.left + b.width / 2), Math.round(b.top + b.height / 2)] }, ID);
  xd(`mousemove ${lb[0]} ${lb[1]} click 1`); await wait(1000); [inF, f, g] = await follow(ID);
  ok('15. its taskbar button brings it back: the frame, the app in it, in front', f && !f.min && f.shown && inF && above(ID, desk), [f, g, wmNow()]);
  xd(`mousemove ${lb[0]} ${lb[1]} click 1`); await wait(1000); f = await fr();
  ok('16. ...and pressed again minimizes it, as a window\'s taskbar button does', f && f.min && wmNow() == 'desktop', [f, wmNow()]);
  xd(`mousemove ${lb[0]} ${lb[1]} click 1`); await wait(1000); [inF, f, g] = await follow(ID);
  // ---- 17. restore it smaller, then the desktop clicked: the app behind the page, the frame kept with the app's name
  xd(`mousemove ${f.mx[0]} ${f.mx[1]} click 1`); await wait(900); [inF, f, g] = await follow(ID);
  xd('mousemove 1250 120 click 1'); await wait(1200); f = await fr(); shot(); keep('17-behind');
  const php = f && px(f.inside[0] + 10, f.inside[1] + 10);
  ok('17. the desktop clicked: the app behind the page; its frame keeps its place, its inside the app\'s icon and name (light grey)', f && f.shown && !f.act && wmNow() == 'desktop' && above(desk, ID) && near(php, [243, 244, 246], 3), [f, wmNow(), php]);
  xd(`mousemove ${f.inside[0] + 60} ${f.inside[1] + 60} click 1`); await wait(1000); [inF, f, g] = await follow(ID);
  ok('18. a click on the frame brings the app back in it', f && f.act && inF && above(ID, desk) && sh(`${XC} getfocus`).trim().replace(/^0x/, '') == ID, [f, g, wmNow()]);
  await q(() => { window.__np = notepad() }); await wait(1000); f = await fr();
  ok('19. another of Rudolf\'s windows opened in front: the app goes behind the page', f && !f.act && wmNow() == 'desktop' && above(desk, ID), [f, wmNow()]);
  await q(() => __np.close()); await wait(1200); [inF, f, g] = await follow(ID);
  ok('20. ...and closed: the page\'s last window, the frame, is in front again, with its app', f && f.act && inF && above(ID, desk), [f, wmNow()]);
  // ---- 21. a menu of the frame's over the app: the app waits behind the page while it is open
  xd(`mousemove ${f.tb[0] + 150} ${f.tb[1] + 12} click 3`); await wait(900);
  const menu = await q(() => !!document.querySelector('.ctx')), wm21 = wmNow(); xd('key Escape'); await wait(300); await q(() => { try { cmenuClose() } catch (e) {} }); await wait(1000); [inF, f, g] = await follow(ID);
  ok('21. the frame\'s menu (right-click on its title bar), over the app: the app behind the page while it is open, back after', menu && wm21 == 'desktop' && inF && above(ID, desk), [menu, wm21, wmNow()]);
  // ---- 22. the workspaces
  await q(id => wsMove(XF.map.get(id).W, 2), ID); await wait(1000); f = await fr();
  ok('22. moved to workspace 2: the frame and the app gone from workspace 1', f && !f.shown && wmNow() == 'desktop' && above(desk, ID), [f, wmNow()]);
  await q(() => wsGo(2)); await wait(1200); [inF, f, g] = await follow(ID);
  ok('23. ...workspace 2: the frame and the app in it', f && f.shown && inF && above(ID, desk), [f, g, wmNow()]);
  await q(id => { wsMove(XF.map.get(id).W, 1); wsGo(1); XF.map.get(id).W.focus() }, ID); await wait(1200); [inF, f, g] = await follow(ID);
  // ---- 24. dragged partly past the screen's right edge: the app with its frame, partly off the screen as the frame is
  await drag(f.tb[0] + 100, f.tb[1] + 12, 1280 - f.tb[0] - 300, 0, 8); [inF, f, g] = await follow(ID);
  ok('24. dragged partly past the screen\'s right edge: the app goes with its frame, partly off the screen too', f && f.el[0] + f.el[2] > 1280 && inF && g[0] + g[2] > 1280, [f && f.el, g]);
  await drag(f.tb[0] + 100, f.tb[1] + 12, 200 - f.el[0], 0, 8); [inF, f, g] = await follow(ID);
  // ---- 25. close
  if (process.env.XF_DEBUG) console.log('before close', JSON.stringify(f), wmNow());
  xd(`mousemove ${f.cl[0]} ${f.cl[1]} click 1`); for (let i = 0; i < 40 && !/^delete/m.test(rf(A.lg)); i++) await wait(100); await wait(2500); f = await fr();
  ok('25. ✕ asks the app to close (WM_DELETE_WINDOW, as its own close does); it closes, and its frame and taskbar button go', /^delete/m.test(rf(A.lg)) && !f && !(await q(id => !!document.querySelector(`#tasks .lxtb[data-win="${id}"]`), ID)), [rf(A.lg).slice(-200), f]);
  // ---- 26. an app drawing its own title bar: framed too (both bars, as on Native for an app not on the list)
  const C = app(['csd', '500', '360']); const CID = await idOf(C); await p.waitForSelector(`.xfr[data-xwin="${CID}"]`, {timeout: 6000}).catch(() => {});
  const fc = async () => { const o = await q(id => { const el = document.querySelector(`.xfr[data-xwin="${id}"]`); if (!el) return null; const b = el.querySelector('.body').getBoundingClientRect(); return [Math.round(b.left), Math.round(b.top), Math.round(b.width), Math.round(b.height)] }, CID); return o };
  let ci, cg; for (let i = 0; i < 40; i++) { ci = await fc(); cg = xgeo(CID); if (ci && near(cg, ci, 1)) break; await wait(100) }
  shot(); keep('26-own-bar'); const cp1 = ci && px(ci[0] + 100, ci[1] + 4);
  ok('26. an app drawing its own title bar: Rudolf\'s frame around it too, its own bar below Rudolf\'s (both, as on Native for an app off the list)', ci && near(cg, ci, 1) && near(cp1, [31, 41, 55], 3), [ci, cg, cp1]);
  // ---- 27. full screen asked for: the whole screen, no frame; back: in its frame
  C.kill(); await wait(1500);
  // (on a window asking for a frame: the stand-in drawing its own stays the screen's size when it leaves full screen, so the
  // game rule of 1.5.2, a borderless window the screen's size, takes it for a game again, as before this change)
  const V = app(['app']); const VID = await idOf(V); let fv; for (let i = 0; i < 50; i++) { fv = await follow(VID, 200); if (fv[0]) break }
  sh(`${XC} full 0x${VID} 1`); await wait(2500); const gfs = xgeo(VID), ffs = await q(id => { const el = document.querySelector(`.xfr[data-xwin="${id}"]`); return el && getComputedStyle(el).visibility }, VID);
  ok('27. full screen (a video, a game): the whole screen, taskbar too, and no frame', near(gfs, [0, 0, 1280, 800], 0) && ffs == 'hidden' && above(VID, desk), [gfs, ffs]);
  sh(`${XC} full 0x${VID} 0`); await wait(2500); fv = await follow(VID, 3000);
  ok('28. ...full screen ended: in its frame again', fv[0], fv.slice(1));
  V.kill(); await wait(1500);
  // ---- 29. a game (Wine's borderless window the screen's size): full screen, no frame
  const G = app(['borderless', '1280', '800']); const GID = await idOf(G); await wait(3000);
  const gg = xgeo(GID), gf = await q(id => { const el = document.querySelector(`.xfr[data-xwin="${id}"]`); return el ? getComputedStyle(el).visibility : 'none' }, GID);
  ok('29. a game (a borderless window the screen\'s size): kept full screen, no frame seen', near(gg, [0, 0, 1280, 800], 0) && gf != 'visible' && above(GID, desk), [gg, gf]);
  G.kill(); await wait(2000);
  // ---- 30. a dialog: centred above its window, where the frame put it
  const I = app(['inno']); const IID = await idOf(I), DID = await idOf(I, 'dlg'); await p.waitForSelector(`.xfr[data-xwin="${IID}"]`, {timeout: 6000}).catch(() => {});
  await wait(1500); await q(id => { const W = XF.map.get(id).W; if (W.el.classList.contains('max')) W.el.querySelector('.mx').click(); W.focus(); setTimeout(() => { W.el.style.left = '40px'; W.el.style.top = '60px'; W.el.style.width = '700px'; W.el.style.height = '520px' }, 800) }, IID); await wait(1500);
  const og = xgeo(IID), dg = xgeo(DID), dcx = dg && dg[0] + dg[2] / 2, ocx = og && og[0] + og[2] / 2, dcy = dg && dg[1] + dg[3] / 2, ocy = og && og[1] + og[3] / 2;
  ok('30. a dialog (WM_TRANSIENT_FOR): centred above its window in its frame (not the screen\'s middle), and in front of it', og && dg && Math.abs(dcx - ocx) <= 2 && Math.abs(dcy - ocy) <= 2 && Math.abs(dcx - 640) > 20 && above(DID, IID), [og, dg]);
  I.kill(); await wait(1500);
  // ---- 31. never over the lock screen
  const K = app(['app']); const KID = await idOf(K); for (let i = 0; i < 40; i++) { const r = await follow(KID, 200); if (r[0]) break }
  await q(() => lockScreen()); await wait(4000); const lk = wmNow(), lka = above(desk, KID);
  ok('31. the screen locked while an app was shown: the lock screen in front, and the app not brought back over it', lk == 'desktop' && lka, [lk, stack()]);
  await q(() => { const o = document.getElementById('lockov'); o && o.remove(); LOCKED = false }); await q(id => XF.map.get(id).W.focus(), KID); await wait(1200); K.kill(); await wait(1500);
  // ---- real apps, when the host has them
  const real = async (nm, cmd, args, env) => { const before = new Set(rf('/run/rudolf/windows').split('\n').map(l => l.split('\t')[0])); const k = cp.spawn(cmd, args, {stdio: 'ignore', env: Object.assign({}, process.env, {NO_AT_BRIDGE: '1', GDK_BACKEND: 'x11', GTK_A11Y: 'none'}, env || {})}); kids.push(k);
    let id = ''; for (let i = 0; i < 150 && !id; i++) { await wait(100); id = rf('/run/rudolf/windows').split('\n').map(l => l.split('\t')[0]).find(x => x && !before.has(x)) || '' }
    let fi, gi; for (let i = 0; i < 60 && id; i++) { fi = await q(id => { const el = document.querySelector(`.xfr[data-xwin="${id}"]`); if (!el) return null; const b = el.querySelector('.body').getBoundingClientRect(); return [Math.round(b.left), Math.round(b.top), Math.round(b.width), Math.round(b.height)] }, id); gi = xgeo(id); if (fi && near(gi, fi, 1)) break; await wait(100) }
    return {k, id, fi, gi} };
  const mwm = id => { const o = sh(`xprop -id 0x${id} _MOTIF_WM_HINTS`); const m = /= (.*)$/m.exec(o); return m ? m[1].split(',').map(x => +x.trim()) : null };
  for (const [n, c, a, e] of [['32', 'gtk3-widget-factory', [], null], ['33', 'gtk4-widget-factory', [], null]]) {
    if (!sh(`command -v ${c}`).trim()) { console.log(`SKIP ${n}. ${c} is not installed`); continue }
    const r = await real(c, c, a, e); shot(); keep(n + '-' + c);
    ok(`${n}. ${c} (GTK ${n == '32' ? sh('pkg-config --modversion gtk+-3.0').trim() : sh('pkg-config --modversion gtk4').trim()}): framed by the page, its window in the frame's inside (its header bar below Rudolf's title bar)`, r.id && r.fi && near(r.gi, r.fi, 1), r);
    r.k.kill(); await wait(2000) }
  const CHR = process.env.XF_CHROME || chromium.executablePath(), cprof = path.join(T, 'cprof'), eprof = path.join(T, 'eprof'); fs.mkdirSync(cprof + '/Default', {recursive: true}); fs.mkdirSync(eprof + '/Default', {recursive: true});
  fs.writeFileSync(eprof + '/Default/Preferences', '{"browser":{"custom_chrome_frame":true}}');
  const cargs = pr => ['--user-data-dir=' + pr, '--no-first-run', '--no-default-browser-check', '--disable-gpu', '--no-sandbox', '--window-size=700,500', 'data:text/html,<title>Chromium window</title><body>hello'];
  { const r = await real('chromium', CHR, cargs(cprof)); const h = r.id && mwm(r.id); shot(); keep('34-chromium');
    ok(`34. Chromium (${sh(CHR + ' --version').trim().split(' ').pop()}), as the encrypted Oracle and the Tor windows are: its own frame off (it asks the window manager for one: _MOTIF_WM_HINTS decorations not 0), Rudolf's frame from the page`, r.id && r.fi && near(r.gi, r.fi, 1) && !(h && (h[0] & 2) && h[2] == 0), [r, h]); r.k.kill(); await wait(2500) }
  { const r = await real('electron', CHR, cargs(eprof)); const h = r.id && mwm(r.id); shot(); keep('35-electron-style');
    ok('35. an Electron-style app (Chromium with its own frame, as Discord\'s frame: false): it keeps its own bar (decorations 0) and gets Rudolf\'s frame too (both, as on Native)', r.id && r.fi && near(r.gi, r.fi, 1) && h && (h[0] & 2) && h[2] == 0, [r, h]); r.k.kill(); await wait(2500) }
  ok('36. no page errors', errs.length == 0, errs);
  const c = CALLS.filter(x => /^apps\/(show|desktop)/.test(x)).length;
  console.log(`(the page told rudolf-display ${c} times; ${CALLS.filter(x => x.startsWith('apps/show?') && /&x=/.test(x)).length} with a place)`);
  kids.forEach(k => { try { k.kill() } catch (e) {} }); await ctx.close(); srv.close();
  console.log(`${R.filter(Boolean).length}/${R.length} passed`); process.exit(R.every(Boolean) ? 0 : 1)
})().catch(e => { console.log('FAIL ' + (e && e.stack || e)); process.exit(1) });
