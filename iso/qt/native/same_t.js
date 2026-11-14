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
const only = ONLY ? ONLY.split(',') : null;
// ---- Standard: the page in Chromium
let BR;
async function stdCase(c) {
  state(c.set, c.more);
  const ctx = await BR.newContext({viewport: {width: W, height: H}, deviceScaleFactor: 1, timezoneId: 'UTC'});
  await ctx.clock.install({time: new Date('2026-11-14T09:41:00Z')});
  /* (nothing from outside: the internet's check answers, as on a connected computer, the rest is refused) */
  await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => /generate_204/.test(r.request().url()) ? r.fulfill({status: 204, body: ''}) : r.abort());
  const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto(BASE + '/'); await p.waitForFunction(() => typeof userLogin == 'function' && document.getElementById('login') && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
  await p.clock.runFor(1500); await wait(500);
  const rects = {};
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
  }
  rects.screen = [0, 0, W, H]; rects.wallpaper = [0, 0, W, H];
  rects.bar = await rectOf('#bar'); rects.start = await rectOf('#start'); rects.pins = await rectOf('#pins'); rects.tray = await rectOf('#tray'); rects.clock = await rectOf('#tray .clk, #clock, #tray');
  rects.menu = await rectOf('#menu'); rects.signbox = (await rectOf('#lockov .lkbox')) || (await rectOf('#login .lb'));
  rects.icons = await p.evaluate(() => { const l = [...document.querySelectorAll('#desk > .ic')].filter(e => e.offsetParent).map(e => e.getBoundingClientRect()); if (!l.length) return null;
    const x = Math.min(...l.map(r => r.left)), y = Math.min(...l.map(r => r.top)); return [Math.round(x), Math.round(y), Math.round(Math.max(...l.map(r => r.right)) - x), Math.round(Math.max(...l.map(r => r.bottom)) - y)] });
  rects.title = await p.evaluate(() => { const w = [...document.querySelectorAll('.win')].filter(e => e.offsetParent).pop(); const t = w && w.querySelector('.tb'); if (!t) return null; const r = w.getBoundingClientRect(), q = t.getBoundingClientRect(); return [Math.round(r.left), Math.round(r.top), Math.round(r.width), Math.round(q.bottom - r.top)] });
  rects.desc = await p.evaluate(() => ({bar: document.getElementById('bar').className, body: document.body.className}));
  const f = path.join(OUT, c.n + '.std.png'); await p.screenshot({path: f});
  await ctx.close(); return {f, rects, errs};
}
// ---- Native: rudolf-native under Xvfb, the same storage
let NP = null, NLOG = path.join(OUT, 'native.log');
function nst(k) { try { const l = fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').find(x => x.startsWith(k + ' ')); return l ? l.slice(k.length + 1) : null } catch (e) { return null } }
function nhit(k) { try { const l = fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').find(x => x.startsWith('hit ' + k + ' ')); if (!l) return null; const a = l.split(' ').slice(3).map(Number); return [a[0] + a[2] / 2, a[1] + a[3] / 2] } catch (e) { return null } }
async function until(f, ms) { const t = Date.now(); while (Date.now() - t < (ms || 20000)) { if (await f()) return true; await wait(200) } return false }
function natStart() {
  const env = Object.assign({}, process.env, {DISPLAY: DSP, HOME: D, RUDOLF_NATIVE_STATE: path.join(D, 'state'), LIBGL_ALWAYS_SOFTWARE: '1', TZ: 'UTC',
    LD_PRELOAD: process.env.SAME_FAKETIME || '', FAKETIME: '2026-11-14 09:41:00', FAKETIME_DONT_FAKE_MONOTONIC: '1', FAKETIME_NO_CACHE: '1'});
  if (!env.LD_PRELOAD) delete env.LD_PRELOAD;
  const lg = fs.openSync(NLOG, 'a'); NP = cp.spawn(BIN, ['--size=' + W + 'x' + H, '--draw=soft', '--base=' + BASE], {env, stdio: ['ignore', lg, lg], cwd: D});
}
const xdo = a => sh('xdotool ' + a);
async function natSettle(c) {
  const n0 = (calls().match(/GET desktop\/settings/g) || []).length;
  await until(() => (calls().match(/GET desktop\/settings/g) || []).length > n0 + 1, 15000); await wait(900);
}
async function natCase(c, first) {
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
  const f = path.join(OUT, c.n + '.nat.png'); sh('import -window root ' + f);
  // (back as it was for the next case: the menu closed, the app window closed, unlocked)
  if (c.act == 'menu') { xdo('key Escape'); await wait(400) }
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
    try { n = await natCase(c) } catch (e) { ok(c.n + ': Native drew it', 0, e.message); continue }
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
