// 1.6.0 (plan Part 12, "The rule"): Native must look exactly as Standard. The same state (the page's storage, served by
// same_rd.py) drawn by Standard (the desktop page in Chromium) and by Native (rudolf-native under Xvfb), at the same
// screen size; each part cut out of both screenshots where Standard's page has it, and compared (pxdiff.py): a part
// fails over 2 % of its pixels differing by more than a small tolerance. Both pictures and the difference picture are
// kept in OUT for the PR.   node same_t.js RUDOLF-NATIVE DIR OUT PORT DISPLAY [ONLY-CASES,...]
// (Run by same_t.sh, which starts same_rd.py and Xvfb.)
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path'), cp = require('child_process');
const [BIN, D, OUT, PORT, DSP, ONLY] = process.argv.slice(2);
const W = 1920, H = 1080, LIMIT = +(process.env.SAME_LIMIT || 2), TOL = +(process.env.SAME_TOL || 32);
const BASE = 'http://127.0.0.1:' + PORT;
const KEYS = JSON.parse(fs.readFileSync(path.join(D, 'keys.json'))), PW = KEYS.Ann.pw;
const wait = ms => new Promise(r => setTimeout(r, ms));
const sh = (c, o) => cp.execSync(c, Object.assign({env: Object.assign({}, process.env, {DISPLAY: DSP, LC_ALL: 'C.UTF-8'})}, o || {})).toString();
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 600) : '')) };
fs.mkdirSync(OUT, {recursive: true});
// ---- the state (same_state.js)
const ST = require('./same_state.js'), ACCT = ST.ACCT; const state = (set, more) => ST.state(D, set, more);
const calls = () => { try { return fs.readFileSync(path.join(D, 'calls'), 'utf8') } catch (e) { return '' } };
// ---- the cases: the settings, what is done before the screenshot, and the parts compared
const STY = ['standard', 'wire', 'mono', 'candy', 'retro'];
const CASES = [];
CASES.push({n: 'signin', set: {}, signin: 1, parts: ['screen', 'signbox']});
CASES.push({n: 'desk', set: {}, parts: ['wallpaper', 'bar', 'start', 'pins', 'tray', 'clock', 'icons']});
for (const st of STY) if (st != 'standard') CASES.push({n: 'bar-' + st, set: {tbstyle: st}, parts: ['bar']});
for (const m of ['normal', 'negative', 'comp']) CASES.push({n: 'bar-accent-' + m, set: {tbac: true, tbacm: m}, parts: ['bar']});
for (const st of STY) CASES.push({n: 'btn-' + st, set: {tbbstyle: st}, parts: ['bar']});
CASES.push({n: 'btn-accent', set: {tbbac: true, tbbacm: 'normal'}, parts: ['bar']});
CASES.push({n: 'bar-top', set: {tb: 'top'}, parts: ['bar', 'icons']});
CASES.push({n: 'clock-24', set: {h24: true, df: 'ymd'}, parts: ['clock']});
CASES.push({n: 'wall-gradient', set: {bs: 'gradient', b1: '#0f766e', b2: '#1e1b4b'}, parts: ['wallpaper']});
CASES.push({n: 'menu', set: {}, act: 'menu', parts: ['menu']});
CASES.push({n: 'hover-pin', set: {}, act: 'hover', parts: ['bar', 'pins']});
CASES.push({n: 'hover-candy', set: {tbstyle: 'candy'}, act: 'hover', parts: ['bar']});
for (const st of STY) CASES.push({n: 'title-' + st, set: {twstyle: st}, act: 'notepad', parts: st == 'standard' ? ['title', 'bar'] : ['title']});
CASES.push({n: 'title-negative', set: {twacm: 'negative'}, act: 'notepad', parts: ['title']});
CASES.push({n: 'lock', set: {}, act: 'lock', parts: ['screen', 'signbox']});
/* (item 1's second part) files in the Desktop folder, the desktop's and a file's menus, Rename and Open with */
CASES.push({n: 'desk-files', set: {}, files: 1, parts: ['icons']});
CASES.push({n: 'desk-menu', set: {}, files: 1, act: 'deskmenu', parts: ['ctx']});
CASES.push({n: 'file-menu', set: {}, files: 1, act: 'filemenu', parts: ['ctx']});
CASES.push({n: 'file-rename', set: {}, files: 1, act: 'rename', parts: ['dlg']});
CASES.push({n: 'file-openwith', set: {}, files: 1, act: 'openwith', parts: ['dlg']});
/* (item 2) the widgets: each of the five, in its place; the weather's place and answer the same for both (Open-Meteo answered
   by the test: Standard's page through the route, Native's through RUDOLF_WX_BASE) */
const WG = [{t: 'clock', x: 1640, y: 30, txt: ''}, {t: 'cal', x: 1640, y: 200, txt: ''}, {t: 'note', x: 1370, y: 30, txt: 'Buy milk\nCall Bob at 5'}, {t: 'mon', x: 1370, y: 250, txt: ''}, {t: 'weather', x: 1100, y: 30, txt: ''}];
const WLOC = {lat: 52.52, lon: 13.41, name: 'Berlin, Land Berlin, Germany', cc: 'DE'};
const WX = require('./same_rd_wx.json');   /* (what same_rd.py answers Native with) */
CASES.push({n: 'widgets', set: {}, more: {['bd95widgets:' + ACCT]: WG, bd95wloc: WLOC}, wg: 5, parts: ['wg0', 'wg1', 'wg2', 'wg3', 'wg4']});
CASES.push({n: 'widgets-ask', set: {}, more: {['bd95widgets:' + ACCT]: [{t: 'weather', x: 1100, y: 30, txt: ''}, {t: 'note', x: 1370, y: 30, txt: ''}]}, wg: 2, parts: ['wg0', 'wg1']});
CASES.push({n: 'widgets-top', set: {tb: 'top', h24: true}, more: {['bd95widgets:' + ACCT]: WG.slice(0, 2), bd95wloc: WLOC}, wg: 2, parts: ['wg0', 'wg1']});
CASES.push({n: 'widgets-over', set: {bs: 'gradient', b1: '#0f766e', b2: '#1e1b4b'}, more: {['bd95widgets:' + ACCT]: [{t: 'clock', x: 30, y: 60, txt: ''}, {t: 'mon', x: 60, y: 200, txt: ''}]}, wg: 2, parts: ['wg0', 'wg1']});   /* (over the icons: what is behind, blurred) */
CASES.push({n: 'widget-picker', set: {}, act: 'wgpick', parts: ['ctx']});
/* (item 3) the taskbar's right-click menus: the empty bar, the start button, a pinned app (Oracle's with its own row), a window's
   button (read from the window's page), the sound, the network, the clock */
const PINS2 = {['bd95pins:' + ACCT]: ['explorer', 'notepad', 'calc', 'settings', 'browser']};
for (const r of ['bar', 'start', 'pin:notepad', 'pin:browser', 'vol', 'net', 'clock']) CASES.push({n: 'tbm-' + r.replace(':', '-'), set: {}, more: PINS2, rc: r, parts: ['ctx']});
CASES.push({n: 'tbm-win', set: {}, more: PINS2, act: 'notepad', rc: 'win:notepad', parts: ['ctx']});
CASES.push({n: 'tbm-bar-win', set: {}, more: PINS2, act: 'notepad', rc: 'bar', parts: ['ctx']});
const only = ONLY ? ONLY.split(',') : null;
// ---- Standard: the page in Chromium
let BR;
async function stdCase(c) {
  state(c.set, c.more);
  const ctx = await BR.newContext({viewport: {width: W, height: H}, deviceScaleFactor: 1, timezoneId: 'UTC'});
  await ctx.clock.install({time: new Date('2026-11-14T09:41:00Z')});
  /* (nothing from outside: the internet's check answers, as on a connected computer, the rest is refused) */
  await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => /generate_204/.test(r.request().url()) ? r.fulfill({status: 204, body: ''}) :
    /api\.open-meteo\.com\/v1\/forecast/.test(r.request().url()) ? r.fulfill({status: 200, contentType: 'application/json', body: JSON.stringify(WX)}) : r.abort());
  const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto(BASE + '/'); await p.waitForFunction(() => typeof userLogin == 'function' && document.getElementById('login') && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
  await p.clock.runFor(1500); await wait(500);
  const rects = {}; let pt = null;
  const rectOf = async sel => p.evaluate(s => { const e = typeof s == 'string' ? document.querySelector(s) : null; if (!e) return null; const r = e.getBoundingClientRect(); return r.width ? [Math.round(r.left), Math.round(r.top), Math.round(r.width), Math.round(r.height)] : null }, sel);
  if (!c.signin) {
    await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
    await p.waitForFunction(() => typeof USER != 'undefined' && USER && getComputedStyle(document.getElementById('login')).display == 'none', null, {timeout: 60000});
    for (let i = 0; i < 12; i++) { await p.clock.runFor(500); await wait(150) }
    await p.mouse.move(W / 2, H / 3);
    if (c.act == 'menu') { await p.click('#start'); await p.clock.runFor(600); await wait(300) }
    if (c.act == 'hover') { const r = await rectOf('#pins .pin:nth-child(2)'); await p.mouse.move(r[0] + r[2] / 2, r[1] + r[3] / 2); await p.clock.runFor(400); await wait(200) }
    if (c.act == 'notepad') { await p.evaluate(() => launch('notepad')); for (let i = 0; i < 4; i++) { await p.clock.runFor(400); await wait(150) } await p.mouse.move(W / 2, H - 200) }
    if (c.act == 'lock') { await p.evaluate(() => typeof lockScreen == 'function' ? lockScreen() : showLogin()); for (let i = 0; i < 4; i++) { await p.clock.runFor(400); await wait(150) } }
    if (c.files) { await p.evaluate(() => { const d = deskFiles(); d['REPORT.TXT'] = 'hello'; d['PROJECTS'] = {}; fsSave(); EXPL.forEach(f => f()) }); for (let i = 0; i < 4; i++) { await p.clock.runFor(400); await wait(150) } }
    const icAt = n => p.evaluate(n => { const e = [...document.querySelectorAll('#desk > .ic')].find(x => (x.dataset.fn || x.querySelector('span').textContent) == n); const r = e.getBoundingClientRect(); return [Math.round(r.left + r.width / 2), Math.round(r.top + 20)] }, n);
    if (c.act == 'deskmenu') { pt = [1500, 500]; await p.mouse.click(pt[0], pt[1], {button: 'right'}); await p.clock.runFor(300); await wait(200) }
    if (c.act == 'filemenu') { pt = await icAt('REPORT.TXT'); await p.mouse.click(pt[0], pt[1], {button: 'right'}); await p.clock.runFor(300); await wait(200) }
    if (c.act == 'rename') { await p.evaluate(() => renameDlg(homePath('DESKTOP').concat('REPORT.TXT'))); await p.clock.runFor(300); await wait(200) }
    if (c.act == 'openwith') { await p.evaluate(() => openWith(homePath('DESKTOP').concat('REPORT.TXT'))); await p.clock.runFor(300); await wait(200) }
    if (c.rc) { const at = await p.evaluate(([r, H]) => { const [k, a] = r.split(':'); let e = null;
        if (k == 'bar') return [1000, H - 21];
        if (k == 'start') e = document.getElementById('start'); if (k == 'vol' || k == 'net') e = document.getElementById(k); if (k == 'clock') e = document.getElementById('tray');
        if (k == 'pin') e = [...document.querySelectorAll('#pins > *')].find(b => b.title == APPS[a].name);
        if (k == 'win') e = [...document.querySelectorAll('#tasks button')].find(b => { const W = winOf(b); return W && W.app == a });
        const q = e.getBoundingClientRect(); return [Math.round(q.left + q.width / 2), Math.round(q.top + q.height / 2)] }, [c.rc, H]);
      pt = at; await p.mouse.click(at[0], at[1], {button: 'right'}); await p.clock.runFor(300); await wait(200); await p.mouse.move(300, 300); await p.clock.runFor(200); await wait(100) }
    if (c.act == 'wgpick') { await p.mouse.click(1500, 500, {button: 'right'}); await p.clock.runFor(300); await wait(200);
      const r = await p.evaluate(() => { const e = [...document.querySelectorAll('.ctx .cmi')].find(x => x.textContent.includes('Add a widget')); const q = e.getBoundingClientRect(); return [q.left + 50, q.top + q.height / 2] });
      await p.mouse.click(r[0], r[1]); await p.clock.runFor(300); await wait(200) }
    if (/menu$|wgpick/.test(c.act || '')) await p.mouse.move(W / 2, H - 200);
  }
  rects.screen = [0, 0, W, H]; rects.wallpaper = [0, 0, W, H];
  rects.bar = await rectOf('#bar'); rects.start = await rectOf('#start'); rects.pins = await rectOf('#pins'); rects.tray = await rectOf('#tray'); rects.clock = await rectOf('#tray .clk, #clock, #tray');
  rects.menu = await rectOf('#menu'); rects.signbox = (await rectOf('#lockov .lkbox')) || (await rectOf('#login .lb'));
  rects.icons = await p.evaluate(() => { const l = [...document.querySelectorAll('#desk > .ic')].filter(e => e.offsetParent).map(e => e.getBoundingClientRect()); if (!l.length) return null;
    const x = Math.min(...l.map(r => r.left)), y = Math.min(...l.map(r => r.top)); return [Math.round(x), Math.round(y), Math.round(Math.max(...l.map(r => r.right)) - x), Math.round(Math.max(...l.map(r => r.bottom)) - y)] });
  rects.title = await p.evaluate(() => { const w = [...document.querySelectorAll('.win')].filter(e => e.offsetParent).pop(); const t = w && w.querySelector('.tb'); if (!t) return null; const r = w.getBoundingClientRect(), q = t.getBoundingClientRect(); return [Math.round(r.left), Math.round(r.top), Math.round(r.width), Math.round(q.bottom - r.top)] });
  { const r = await rectOf('.ctx'); rects.ctx = r ? [r[0] - 4, r[1] - 4, r[2] + 8, r[3] + 8] : null; rects.dlg = await rectOf('.rmodal .rmbox') }   /* (a menu with its focus ring) */
  for (let i = 0; i < (c.wg || 0); i++) rects['wg' + i] = await p.evaluate(i => { const e = document.querySelectorAll('#desk > .wgt')[i]; if (!e) return null; const r = e.getBoundingClientRect(); return [Math.round(r.left) - 20, Math.round(r.top) - 10, Math.round(r.width) + 40, Math.round(r.height) + 40] }, i);   /* (with its shadow) */
  rects.desc = await p.evaluate(() => ({bar: document.getElementById('bar').className, body: document.body.className}));
  const f = path.join(OUT, c.n + '.std.png'); await p.screenshot({path: f});
  await ctx.close(); return {f, rects, errs, pt};
}
// ---- Native: rudolf-native under Xvfb, the same storage
let NP = null, NLOG = path.join(OUT, 'native.log');
function nst(k) { try { const l = fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').find(x => x.startsWith(k + ' ')); return l ? l.slice(k.length + 1) : null } catch (e) { return null } }
function nhit(k) { try { const l = fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').find(x => x.startsWith('hit ' + k + ' ')); if (!l) return null; const a = l.split(' ').slice(3).map(Number); return [a[0] + a[2] / 2, a[1] + a[3] / 2] } catch (e) { return null } }
async function until(f, ms) { const t = Date.now(); while (Date.now() - t < (ms || 20000)) { if (await f()) return true; await wait(200) } return false }
function natStart() {
  const env = Object.assign({}, process.env, {DISPLAY: DSP, HOME: D, RUDOLF_NATIVE_STATE: path.join(D, 'state'), LIBGL_ALWAYS_SOFTWARE: '1', TZ: 'UTC', RUDOLF_WX_BASE: BASE, RUDOLF_WX_GEO: BASE,
    LD_PRELOAD: process.env.SAME_FAKETIME || '', FAKETIME: '2026-11-14 09:41:00', FAKETIME_DONT_FAKE_MONOTONIC: '1', FAKETIME_NO_CACHE: '1'});
  if (!env.LD_PRELOAD) delete env.LD_PRELOAD;
  for (const k of ['http_proxy', 'https_proxy', 'HTTP_PROXY', 'HTTPS_PROXY', 'all_proxy', 'ALL_PROXY']) delete env[k];   /* (Open-Meteo is the test's own) */
  const lg = fs.openSync(NLOG, 'a'); NP = cp.spawn(BIN, ['--size=' + W + 'x' + H, '--draw=soft', '--base=' + BASE], {env, stdio: ['ignore', lg, lg], cwd: D});
}
const xdo = a => sh('xdotool ' + a);
async function natSettle(c) {
  const n0 = (calls().match(/GET desktop\/settings/g) || []).length;
  await until(() => (calls().match(/GET desktop\/settings/g) || []).length > n0 + 1, 15000); await wait(900);
}
/* the files of the desktop-files cases, made as anyone would: the desktop's menu, New text document and New folder, Rename */
let NATFILES = 0;
async function natFiles() {
  if (NATFILES) return; NATFILES = 1;
  const has = n => { try { return fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').some(x => x.startsWith('dicon ') && x.endsWith('|' + n)) } catch (e) { return false } };
  for (const [item, name] of [['New text document', 'REPORT.TXT'], ['New folder', 'PROJECTS']]) {
    xdo('mousemove 1500 500 click 3'); await until(() => nst('cmenu') != null, 5000);
    const l = fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').find(x => x.startsWith('cmitem ') && x.endsWith('|' + item)); if (!l) continue; const a = l.split(' ');
    xdo('mousemove ' + a[1] + ' ' + a[2] + ' click 1'); await until(() => (nst('dialog') || '').startsWith('rename'), 30000); await wait(400);
    xdo('type --delay 30 ' + name); xdo('key Return'); await until(() => (nst('dialog') || '').startsWith('-'), 30000); await until(() => has(name), 15000); await wait(800);
  }
  xdo('mousemove ' + W / 2 + ' ' + Math.round(H / 3)); await wait(300);
}
async function natCase(c, s) {
  state(c.set, c.more);
  if (c.signin) {
    try { fs.unlinkSync(path.join(D, 'session')) } catch (e) {}
    if (NP) { NP.kill(); NP = null; await wait(500) }
    natStart(); await until(() => nst('signin') == '1', 40000); await wait(1500);
  } else if (!NP || nst('user') == '-' || nst('user') == null) {
    if (!NP) { natStart(); await until(() => nst('signin') == '1', 40000) }
    await wait(2000); xdo("type --delay 40 '" + PW.replace(/'/g, "'\\''") + "'"); xdo('key Return');
    await until(() => nst('user') == 'Ann' && nst('signin') == '0', 30000); await wait(2500);
  } else await natSettle(c);
  // (what is open from before goes)
  xdo('mousemove ' + W / 2 + ' ' + Math.round(H / 3)); await wait(300);
  if (c.act == 'menu') { const h = nhit('start'); if (h) xdo('mousemove ' + h.join(' ') + ' click 1'); await until(() => nst('menu') == '1', 5000); xdo('mousemove ' + W / 2 + ' ' + Math.round(H / 3)); await wait(800) }
  if (c.act == 'hover') { const h = nhit('pin notepad'); if (h) xdo('mousemove ' + h.map(Math.round).join(' ')); await wait(800) }
  let cl = null;
  if (c.act == 'notepad') { const h = nhit('pin notepad'); if (h) xdo('mousemove ' + h.join(' ') + ' click 1');
    await until(() => /^client .* app=notepad/m.test(fs.readFileSync(path.join(D, 'state'), 'utf8')), 30000); await wait(3000); xdo('mousemove ' + W / 2 + ' ' + (H - 200)); await wait(500);
    const l = fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').find(x => /^client .* app=notepad/.test(x)); if (l) { const a = l.split(' '); cl = [+a[3], +a[4], +a[5], +a[6]] } }
  if (c.act == 'lock') { xdo('key super+l'); await until(() => nst('locked') == '1', 8000); await wait(1200) }
  if (c.files) await natFiles();
  const nlines = k => { try { return fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } };
  const cmClick = lbl => { const l = nlines('cmitem').find(x => x.endsWith('|' + lbl)); if (l) { const a = l.split(' '); xdo('mousemove ' + a[1] + ' ' + a[2] + ' click 1') } return !!l };
  const fileAt = () => { const l = nlines('dicon').find(x => x.endsWith('|REPORT.TXT')); if (!l) return null; const a = l.split(' '); return [+a[3] + 42, +a[4] + 20] };
  if (c.rc) { const [k, a] = c.rc.split(':'); let q = k == 'bar' ? [1000, H - 21] : k == 'pin' ? nhit('pin ' + a) : k == 'win' ? null : nhit(k + ' -');
    if (k == 'win') { const cl = fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').find(x => new RegExp('^client .* app=' + a + ' ').test(x)); if (cl) q = nhit('win ' + cl.split(' ')[1]) }
    if (q && s.pt && Math.abs(q[0] - s.pt[0]) < 6 && Math.abs(q[1] - s.pt[1]) < 6) q = s.pt;   /* (the same pixel as Standard's click, when the part is there) */
    if (q) xdo('mousemove ' + q.map(Math.round).join(' ') + ' click 3'); await until(() => nlines('cmenu').length, 8000); xdo('mousemove 300 300'); await wait(600) }
  if (c.act == 'wgpick') { xdo('mousemove 1500 500 click 3'); await until(() => nlines('cmenu').length, 5000); cmClick('Add a widget…'); await until(() => (nlines('cmenu')[0] || '').endsWith('Add a widget'), 5000); xdo('mousemove ' + W / 2 + ' ' + (H - 200)); await wait(500) }
  if (c.act == 'deskmenu') { xdo('mousemove 1500 500 click 3'); await until(() => nlines('cmenu').length, 5000); xdo('mousemove ' + W / 2 + ' ' + (H - 200)); await wait(500) }
  if (c.act == 'filemenu') { const q = s.pt || fileAt(); if (q) xdo('mousemove ' + q.join(' ') + ' click 3'); await until(() => nlines('cmenu').length, 5000); xdo('mousemove ' + W / 2 + ' ' + (H - 200)); await wait(500) }
  if (c.act == 'rename' || c.act == 'openwith') { const q = fileAt(); if (q) xdo('mousemove ' + q.join(' ') + ' click 3'); await until(() => nlines('cmenu').length, 5000);
    cmClick(c.act == 'rename' ? 'Rename' : 'Open with…'); await until(() => (nst('dialog') || '').startsWith(c.act == 'rename' ? 'rename' : 'openwith'), 20000); xdo('mousemove ' + W / 2 + ' ' + (H - 100)); await wait(700) }
  const f = path.join(OUT, c.n + '.nat.png'); sh('import -window root ' + f);
  // (back as it was for the next case: the menu closed, the app window closed, unlocked)
  if (c.act == 'menu' || c.rc || /menu$|rename|openwith|wgpick/.test(c.act || '')) { xdo('key Escape'); await wait(400) }
  if (c.act == 'notepad') {   /* (its ✕: 9 px from the frame's right edge, 7 px down, 24 by 22) */
    const l = fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').find(x => /^client .* app=notepad/.test(x));
    if (l) { const a = l.split(' '); xdo('mousemove ' + (+a[3] + +a[5] + 2 - 9 - 12) + ' ' + (+a[4] + 7 + 11) + ' click 1') }
    await until(() => !/^client .* app=notepad/m.test(fs.readFileSync(path.join(D, 'state'), 'utf8')), 10000); await wait(800) }
  if (c.act == 'lock') { xdo("type --delay 40 '" + PW.replace(/'/g, "'\\''") + "'"); xdo('key Return'); await until(() => nst('locked') == '0', 8000); await wait(800) }
  return {f, client: cl};
}
// ---- the comparison
function cmp(name, a, b, ra, rb) {
  const o = path.join(OUT, name);
  try { const r = JSON.parse(cp.execFileSync('python3', [path.join(__dirname, 'pxdiff.py'), a, b, ra.join(','), (rb || ra).join(','), String(TOL), o]).toString()); return r }
  catch (e) { return {pct: 100, err: String(e.message).slice(0, 300)} }
}
(async () => {
  BR = await chromium.launch();
  const rows = [];
  for (const c of CASES) {
    if (only && !only.includes(c.n)) continue;
    let s, n;
    try { s = await stdCase(c) } catch (e) { ok(c.n + ': Standard drew it', 0, e.message); continue }
    try { n = await natCase(c, s) } catch (e) { ok(c.n + ': Native drew it', 0, e.message); continue }
    for (const part of c.parts) {
      let ra = s.rects[part], rb = null;
      if (part == 'title' && n.client && ra) rb = [n.client[0], n.client[1], ra[2], ra[3]];   /* (each engine's own window; the frame's border and title bar above the app) */
      if (!ra) { ok(c.n + ' ' + part + ': Standard has it', 0, s.rects); continue }
      const r = cmp(c.n + '.' + part, s.f, n.f, ra, rb);
      rows.push([c.n, part, r.pct]);
      ok(c.n + ' ' + part + ': ' + (r.pct != null ? r.pct.toFixed(2) : '?') + ' % of the pixels differ (at most ' + LIMIT + ' %)', r.pct <= LIMIT, r);
    }
  }
  fs.writeFileSync(path.join(OUT, 'summary.tsv'), rows.map(r => r.join('\t')).join('\n') + '\n');
  if (NP) NP.kill(); await BR.close();
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('FAIL ' + e.stack); if (NP) NP.kill(); process.exit(1) });
