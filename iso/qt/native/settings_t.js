// 1.6.2 (docs/plan-1.6.2.md Part 39): the native Settings (rudolf-settings) against the web one. The same rudolfd stand-in (same_rd.py with
// DIR/tm: settings_rd.json, the answers each page asks for) for the page's Settings in Chromium (Chromium mode, signed in) and the native
// one under Xvfb:
//   - look: each native page's window body screenshot-compared (pxdiff.py, a pixel one pixel away matching, text anti-aliasing aside):
//     at its top and scrolled to its end, under 2 % (SET_LIMIT), light and dark, the blue accent and two others;
//   - Search in Settings: what each native page shows that the search finds (the page's ssHarvest, read from the page) the same; the
//     same results, in the same order, for the same words typed;
//   - the checklist (settings_checklist.txt, made from settings25.js, setsearch59.js, the pages' modules and their tests): each item done
//     on the native one with the pointer and the keyboard (xdotool) and checked by what it shows (its state file), what it asked of
//     rudolfd (the stand-in's calls) and what it saved (the page's storage, ls.json), against what the page saves for the same steps;
//   - the memory at idle with a page open (RssAnon and PSS) against the web one's (Chromium's renderer for the page as an app window,
//     #app=settings, as Part 38 measured on the host).
// node settings_t.js RUDOLF-SETTINGS DIR OUT PORT DISPLAY [CASES]        (run by settings_t.sh)
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path'), cp = require('child_process');
const [BIN, D, OUT, PORT, DSP, ONLY] = process.argv.slice(2);
const LIMIT = +(process.env.SET_LIMIT || 2), TOL = +(process.env.SET_TOL || 32), BASE = 'http://127.0.0.1:' + PORT;
const KEYS = JSON.parse(fs.readFileSync(path.join(D, 'keys.json'))), PW = KEYS.Ann.pw, ST = require('./same_state.js');
const NOW = '2026-11-14T09:41:00Z', NOWS = String(Date.parse(NOW) / 1000);
const wait = ms => new Promise(r => setTimeout(r, ms));
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 900) : '')) };
fs.mkdirSync(OUT, {recursive: true});
const only = k => !ONLY || ONLY.split(',').includes(k);
const PAGES = (process.env.SET_PAGES || 'sys,about').split(',');
const calls = () => { try { return fs.readFileSync(path.join(D, 'calls'), 'utf8') } catch (e) { return '' } };
const env0 = () => { const e = Object.assign({}, process.env, {DISPLAY: DSP, RUI_BASE: BASE, LC_ALL: 'C.UTF-8', NO_AT_BRIDGE: '1', TZ: 'UTC'}); for (const k of ['http_proxy', 'https_proxy', 'HTTP_PROXY', 'HTTPS_PROXY']) delete e[k]; return e };
const sh = c => cp.execSync(c, {env: env0()}).toString();
const px = (a, b, ra, rb, out) => JSON.parse(cp.execSync(`python3 ${path.join(__dirname, 'pxdiff.py')} ${a} ${b} ${ra.join(',')} ${rb.join(',')} ${TOL} ${out} 1`, {env: Object.assign({}, process.env, {PXDIFF_MAP: '1', PXDIFF_AA: '1'})}).toString());
const MODES = {light: {dm: 'light', darkui: true, accent: '#2563eb'}, dark: {dm: 'dark', darkui: true, accent: '#2563eb'}, 'light-green': {dm: 'light', darkui: true, accent: '#16a34a'}, 'dark-pink': {dm: 'dark', darkui: true, accent: '#db2777'}};
const MORE = {};
let BR, GPU = '';
// ---- the page's Settings (Chromium mode)
async function signin(mode) {
  ST.state(D, Object.assign({restore: false}, MODES[mode]), MORE);
  const ctx = await BR.newContext({viewport: {width: 1280, height: 800}, deviceScaleFactor: 1, timezoneId: 'UTC'});
  await ctx.clock.install({time: new Date(NOW)});
  await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => /generate_204/.test(r.request().url()) ? r.fulfill({status: 204, body: ''}) : r.abort());
  const p = await ctx.newPage();
  await p.goto(BASE + '/'); await p.waitForFunction(() => typeof userLogin == 'function' && document.getElementById('login') && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
  await p.clock.runFor(1500); await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
  await p.waitForFunction(() => typeof USER != 'undefined' && USER && getComputedStyle(document.getElementById('login')).display == 'none', null, {timeout: 60000});
  for (let i = 0; i < 12; i++) { await p.clock.runFor(500); await wait(100) }
  return {ctx, p};
}
async function pageShots(mode, pages) {
  const {ctx, p} = await signin(mode); const out = {};
  for (const k of pages) {
    await p.evaluate(k => { document.querySelectorAll('.ntoast,#ntoasts>*').forEach(e => e.remove()); if (window.SW) { SW._ok = 1; SW.close() } settings(k); SW.el.style.left = '120px'; SW.el.style.top = '60px' }, k);
    for (let i = 0; i < 6; i++) { await p.clock.runFor(500); await wait(200) }
    if (!GPU) GPU = await p.evaluate(() => { try { const g = document.createElement('canvas').getContext('webgl'), d = g && g.getExtension('WEBGL_debug_renderer_info'); return d ? g.getParameter(d.UNMASKED_RENDERER_WEBGL) : g ? 'WebGL available' : 'none' } catch (e) { return '' } });
    await p.evaluate(() => document.querySelectorAll('.ntoast,#ntoasts>*').forEach(e => e.remove())); await p.mouse.move(5, 5); await wait(400);
    const b = await p.evaluate(() => { const r = SW.body.getBoundingClientRect(); return [Math.round(r.left), Math.round(r.top), Math.round(r.width), Math.round(r.height)] });
    for (const at of ['top', 'end']) {
      if (at == 'end') { await p.evaluate(() => { const c = SW.body.querySelector('.sc'); c.scrollTop = c.scrollHeight }); await wait(300) }
      const f = path.join(OUT, `${mode}-${k}-${at}.page.png`); await p.screenshot({path: f}); out[k + '-' + at] = {f, b};
    }
    out[k + '-harv'] = await p.evaluate(k => (typeof SSLIVE == 'object' && SSLIVE[k] || []).map(x => x.join('|')), k);
  }
  await ctx.close(); return out;
}
// ---- the native one
let NP = null; const SF = path.join(D, 'setstate');
const nst = () => { try { return fs.readFileSync(SF, 'utf8') } catch (e) { return '' } };
const line = k => (nst().split('\n').find(x => x.startsWith(k + ' ')) || '').slice(k.length + 1);
const lines = k => nst().split('\n').filter(x => x.startsWith(k + ' ')).map(x => x.slice(k.length + 1));
const rect = n => { const l = line('rect ' + n); return l ? l.split(' ').map(Number) : null };
async function until(f, ms) { const t = Date.now(); while (Date.now() - t < (ms || 15000)) { if (await f()) return true; await wait(100) } return false }
function natStart(page, extra) { try { fs.unlinkSync(SF) } catch (e) {} const e = Object.assign(env0(), {RUDOLF_SET_STATE: SF, RUDOLF_SET_NOW: NOWS, RUDOLF_SET_GPU: GPU}, extra || {});
  const q = NP = cp.spawn(BIN, page ? [page] : [], {env: e, stdio: ['ignore', 'pipe', 'pipe']}); q.log = ''; q.stdout.on('data', d => q.log += d); q.stderr.on('data', d => q.log += d) }
function natStop() { if (NP) { NP.kill(); NP = null } }
const xdo = a => sh('xdotool ' + a);
const click = (r, btn) => { xdo(`mousemove ${Math.round(r[0] + r[2] / 2)} ${Math.round(r[1] + r[3] / 2)} click ${btn || 1}`) };
async function natShots(mode, pages) {
  ST.state(D, Object.assign({restore: false}, MODES[mode]), MORE); const out = {};
  for (const k of pages) {
    natStart(k); await until(() => rect('body') && line('page') == k, 20000); await wait(1500); xdo('mousemove 5 5'); await wait(400);
    for (const at of ['top', 'end']) {
      if (at == 'end') { const pg = rect('page'); xdo(`mousemove ${pg[0] + 300} ${Math.min(pg[1] + 200, 600)}`); for (let i = 0; i < 60; i++) xdo('click 5'); xdo('mousemove 5 5'); await wait(800) }
      const f = path.join(OUT, `${mode}-${k}-${at}.nat.png`); sh(`import -window root ${f}`); out[k + '-' + at] = {f, b: rect('body')};
    }
    out[k + '-harv'] = lines('harv').filter(x => x.startsWith(k + '|')).map(x => x.split('|').slice(1, 3).join('|') + (x.endsWith('|1') ? '|1' : ''));
    natStop(); await wait(300);
  }
  return out;
}
// ---- Search in Settings: the same results for the same words (the page's ssMatch over its pages, with System and About harvested as shown)
const QUERIES = ['time zone', 'sound', 'date', 'wifi', 'dark', 'idle', 'about', 'desktop page', 'lang', 'xyzzy'];
async function pageSearch() {
  const {ctx, p} = await signin('light');
  await p.evaluate(() => { settings('sys') }); for (let i = 0; i < 6; i++) { await p.clock.runFor(500); await wait(150) }
  await p.evaluate(() => { SW.show('about', 1) }); for (let i = 0; i < 6; i++) { await p.clock.runFor(500); await wait(150) }
  const r = {}; for (const q of QUERIES) r[q] = await p.evaluate(q => ssMatch(ssAll(SW.SEC), q).map(e => e.k + '|' + e.t + '|' + e.p), q);
  /* (what the page saves for the checklist's steps: Clock analog, 24-hour, the date format, applied) */
  await p.evaluate(() => { SW.show('sys', 1) }); await p.clock.runFor(1000);
  await p.evaluate(() => { const c = SW.body.querySelector('.sc'); const ck = c.querySelector('#ck'); ck.value = 'analog'; ck.dispatchEvent(new Event('change', {bubbles: true}));
    const h = c.querySelector('#h24'); h.checked = true; h.dispatchEvent(new Event('change', {bubbles: true})); const f = c.querySelector('#sdf'); f.value = 'dmy'; f.dispatchEvent(new Event('change', {bubbles: true})); secCommit('sys') });
  await p.clock.runFor(1500); await wait(800);
  const ls = JSON.parse(fs.readFileSync(path.join(D, 'ls.json'))).data; const saved = JSON.parse(ls['bd95set:' + ST.ACCT] || '{}');
  await ctx.close(); return {r, saved};
}
const stored = () => { const d = JSON.parse(fs.readFileSync(path.join(D, 'ls.json'))).data; return {acct: JSON.parse(d['bd95set:' + ST.ACCT] || '{}'), all: JSON.parse(d.bd95set || '{}')} };
// (the window's close box, as a window manager asks: WM_DELETE_WINDOW; there is no window manager under the test's Xvfb)
function wmClose() {
  const w = sh("xdotool search --name '^Settings$' | tail -1").trim(); if (!w) return;
  cp.execFileSync('python3', ['-c', `
import ctypes, sys
X = ctypes.cdll.LoadLibrary('libX11.so.6'); X.XOpenDisplay.restype = ctypes.c_void_p; X.XInternAtom.restype = ctypes.c_ulong
d = X.XOpenDisplay(None); w = int(sys.argv[1])
class E(ctypes.Structure): _fields_ = [('type', ctypes.c_int), ('serial', ctypes.c_ulong), ('send_event', ctypes.c_int), ('display', ctypes.c_void_p), ('window', ctypes.c_ulong), ('message_type', ctypes.c_ulong), ('format', ctypes.c_int), ('l', ctypes.c_long * 5)]
e = E(); e.type = 33; e.window = w; e.message_type = X.XInternAtom(ctypes.c_void_p(d), b'WM_PROTOCOLS', 0); e.format = 32; e.l[0] = X.XInternAtom(ctypes.c_void_p(d), b'WM_DELETE_WINDOW', 0)
buf = (ctypes.c_char * 192)(); ctypes.memmove(buf, ctypes.byref(e), ctypes.sizeof(e))
X.XSendEvent(ctypes.c_void_p(d), ctypes.c_ulong(w), 0, 0, buf); X.XFlush(ctypes.c_void_p(d))`, w], {env: env0()});
}
async function checklist(P) {
  ST.state(D, MODES.light, MORE); const c0 = () => calls().length; let mark = c0(); const since = () => calls().slice(mark);
  const focusWin = () => {};   /* (no window manager under the test's Xvfb: X's focus follows the pointer, which is over the window; set by hand, GTK's keys were left unread) */
  natStart('about'); await until(() => rect('body') && line('page') == 'about', 20000); await wait(1200); focusWin();
  ok('1. it opens at the page asked (rudolf-settings about), the search box above the sidebar of the 22 pages', line('page') == 'about' && rect('search') && nst().split('\n').filter(x => x.startsWith('rect side-')).length == 22, nst().slice(0, 300));
  click(rect('side-sys')); await until(() => line('page') == 'sys', 5000);
  ok('2. a page chosen in the sidebar is shown (System)', line('page') == 'sys', line('page'));
  { const got = []; for (const q of QUERIES) { click(rect('search')); focusWin(); xdo('key ctrl+a BackSpace'); xdo(`type --delay 15 '${q}'`); await until(() => line('q') == q, 4000); await wait(500); got.push([q, lines('r')]) } fs.writeFileSync(path.join(OUT, 'search.json'), JSON.stringify({native: got, page: P.r}, null, 1));
    const bad = got.filter(([q, r]) => JSON.stringify(r) != JSON.stringify(P.r[q]));
    ok(`4. Search in Settings: the same results in the same order as the page's for ${QUERIES.length} searches`, !bad.length, bad.map(([q, r]) => ({q, native: r.slice(0, 6), page: P.r[q].slice(0, 6)}))) }
  { click(rect('search')); xdo('key ctrl+a BackSpace'); xdo("type --delay 15 'time zone'"); await until(() => line('q') == 'time zone', 4000); await wait(400); xdo('key Down Return'); await until(() => line('hit') != '', 5000);
    ok('5. ...Enter opens the result: System, its Time zone row lit and its control focused', line('page') == 'sys' && line('hit') == 'Time zone' && line('focus') == 'tz', [line('page'), line('hit'), line('focus')]) }
  { click(rect('search')); xdo("type --delay 15 'x'"); await until(() => +line('res') >= 0, 3000); xdo('key Escape'); await until(() => line('res') == '-1', 3000); const a = line('res') == '-1'; xdo('key Escape'); await until(() => line('q') == '', 3000);
    ok('6. ...Escape closes the list, then empties the box', a && line('q') == '') }
  { click(rect('side-sys')); await until(() => line('page') == 'sys', 4000); await wait(500); focusWin(); xdo('key Up Up Return'); await until(() => line('page') == 'bt', 4000); ok('3. the keyboard: Up, Up and Enter in the sidebar show Bluetooth (not native yet: it says so)', line('page') == 'bt' && line('native') == '0', [line('page'), line('focus'), line('sidefoc'), nst().split('\n').filter(x => /^act/.test(x)).join(' / '), NP && NP.log.slice(-300)]) }
  click(rect('side-sys')); await until(() => line('page') == 'sys', 4000); await wait(800);
  /* System: the working copy, Apply, Revert, Reset */
  mark = c0();
  { click(rect('ck')); await until(() => line('menu'), 3000); xdo('key Escape'); await wait(300); xdo('key Down'); await until(() => /set ck analog/.test(nst()), 3000);
    click(rect('h24')); await until(() => /set h24 1/.test(nst()), 3000); click(rect('sdf')); await wait(400); xdo('key Escape'); await wait(200); xdo('key Down Down'); await until(() => /set df dmy/.test(nst()), 3000);
    ok('7. Clock Analog, 24-hour, Date format DD/MM/YYYY: unapplied (● Unapplied changes, Apply and Revert on), nothing saved yet', line('dirty') == '1' && /Unapplied/.test(line('foot')) && line('footen') == '1 1' && !/POST lsync/.test(since()), [line('dirty'), line('foot'), line('footen'), since().slice(-200)]) }
  { click(rect('side-acc')); await until(() => rect('dialog'), 3000); ok('8. another page asked for with unapplied changes: the page\'s Unapplied changes (Keep editing, Revert, Apply)', !!rect('dialog') && /ask guard/.test(nst())); xdo('key Escape'); await until(() => !rect('dialog'), 3000) }
  { click(rect('apply')); await until(() => rect('dialog'), 3000); xdo('key Return'); await until(() => /POST lsync/.test(since()), 4000); await wait(400); const s = stored();
    const want = {ck: P.saved.ck, h24: P.saved.h24, df: P.saved.df};
    ok('9. Apply, confirmed: saved as the page saves the same steps (bd95set:ACCT and bd95set: ck, h24, df)', s.acct.ck == want.ck && s.acct.h24 === want.h24 && s.acct.df == want.df && s.all.ck == want.ck && /Settings applied/.test(line('foot')), {native: s.acct, page: want}) }
  { mark = c0(); click(rect('ck')); await wait(300); xdo('key Escape'); await wait(200); xdo('key Up'); await until(() => line('dirty') == '1', 3000); click(rect('revert')); await until(() => rect('dialog'), 3000); xdo('key Return'); await until(() => line('dirty') == '0', 4000);
    ok('10. Revert: back to the saved settings, nothing saved', line('dirty') == '0' && !/POST lsync/.test(since()) && /Changes reverted/.test(line('foot')), [line('dirty'), line('foot')]) }
  { mark = c0(); click(rect('reset')); await until(() => rect('dialog'), 3000); xdo('key Return'); await until(() => /POST lsync/.test(since()), 4000); await wait(400); const s = stored().acct;
    ok('11. Reset to defaults: digital, 12-hour, YYYY/MM/DD, today, this computer\'s time zone, sharing the language: saved', s.ck == 'digital' && s.h24 === false && s.df == 'ymd' && s.doff === 0 && s.tz == 'UTC' && s.shareLang === true, s) }
  { click(rect('sdn')); await wait(300); { const r = rect('sdt'); xdo(`mousemove ${r[0] + 30} ${r[1] + 10} click 1`) } await wait(200); xdo('key Up'); await until(() => /set doff 86400000/.test(nst()), 3000); ok('12. the date (an administrator): one day on moves the desktop\'s date a day (doff)', /set doff 86400000/.test(nst()), nst().split('\n').filter(x => /doff/.test(x)).slice(-3));
    click(rect('sdn')); await until(() => /set doff 0\b/.test(nst()), 3000); ok('13. ...Use current date: back to today', line('dirty') == '0' || /set doff 0/.test(nst())) }
  const scrollTo = async n => { const pg = rect('page'); for (let i = 0; i < 40 && (!rect(n) || rect(n)[1] > 520 || rect(n)[1] < 100); i++) { xdo(`mousemove ${pg[0] + 300} 300 click ${!rect(n) || rect(n)[1] > 520 ? 5 : 4}`); await wait(60) } await wait(500) };
  { mark = c0(); await scrollTo('nts'); click(rect('nts')); await until(() => /POST time\/sync/.test(since()), 4000); ok('14. Set the clock from the internet: rudolfd asked (time/sync)', /POST time\/sync/.test(since())) }
  { mark = c0(); await scrollTo('eng57'); click(rect('eng57')); await wait(300); xdo('key Escape'); await wait(200); xdo('key Up'); await until(() => rect('dialog'), 3000);
    ok('15. Desktop engine: the administrator\'s permission first', /ask engine native/.test(nst()) && !!rect('dialog'));
    xdo('type --delay 30 wrongpw'); xdo('key Return'); await until(() => /POST desktop\/admin/.test(since()), 4000); await wait(500); ok('16. ...a wrong password refused, nothing changed', !/desktop\/engine\?engine/.test(since()));
    xdo(`type --delay 30 '${PW.replace(/'/g, "'\\''")}'`); xdo('key Return'); await until(() => /desktop\/engine\?engine=native/.test(since()), 5000); ok('17. ...the right one: desktop/engine?engine=native', /POST desktop\/engine\?engine=native/.test(since()), since().slice(-300)) }
  { mark = c0(); await scrollTo('ir61'); click(rect('ir61')); await until(() => /POST lsync/.test(since()), 4000); await wait(300); const s = stored().acct;
    ok('18. Idle windows: saved at once (idlerest, idlemin), as rest61.js saves them', s.idlerest === true && s.idlemin === 10, s) }
  { click(rect('ck')); await wait(300); xdo('key Escape'); await wait(200); await scrollTo('ck'); click(rect('ck')); await wait(300); xdo('key Escape'); xdo('key Down'); await until(() => line('dirty') == '1', 3000);
    wmClose(); await until(() => /ask close/.test(nst()), 4000);
    ok('19. closing with unapplied changes asks (Keep editing, Discard, Apply)', /ask close/.test(nst()) && NP && NP.exitCode === null) }
  natStop();
  /* About */
  mark = c0(); natStart('about'); await until(() => rect('body') && line('page') == 'about', 20000); await wait(1000); focusWin();
  ok('20. About: the rows from info and sys/status', /GET info/.test(since()) && /GET sys\/status/.test(since()));
  { click(rect('licences')); await until(() => /open help licences/.test(nst()), 3000); ok('21. Licences: the shell opens Help at its Licences', /act open help licences/.test(nst())) }
  { const cp = rect('licences'); xdo(`mousemove ${cp[0] + 230} ${cp[1] + 10} click 1`); await until(() => fs.existsSync(SF + '.copy'), 3000); const t = fs.existsSync(SF + '.copy') ? fs.readFileSync(SF + '.copy', 'utf8') : '';
    ok('22. Copy details: "Rudolf OS 1.6.2" and each row, as the page copies them', /^Rudolf OS 1\.6\.2\nComputer: ASUSTeK COMPUTER INC\. Vivobook 15\nProcessor: 13th Gen/.test(t) && /\nLanguage: en$/.test(t), t.slice(0, 200)) }
  { await scrollTo('updfile'); mark = c0(); const u = rect('updfile'); xdo(`mousemove ${u[0] + u[2] + 60} ${u[1] + 10} click 1`); await until(() => rect('dialog'), 3000); xdo(`type --delay 30 '${PW.replace(/'/g, "'\\''")}'`); xdo('key Return'); await until(() => /POST rollback/.test(since()), 5000);
    ok('23. Roll back to the previous page: the administrator\'s permission, then rollback', /POST desktop\/admin/.test(since()) && /POST rollback/.test(since()), since().slice(-200)) }
  natStop();
}
// ---- the memory at idle with a page open (System: Display is not native yet), against the web one's
const rss = pid => { const t = fs.readFileSync(`/proc/${pid}/status`, 'utf8'); const g = k => +((t.match(new RegExp('^' + k + ':\\s+(\\d+)', 'm')) || [])[1] || 0) / 1024; let pss = null;
  try { pss = +((fs.readFileSync(`/proc/${pid}/smaps_rollup`, 'utf8').match(/^Pss:\s+(\d+)/m) || [])[1]) / 1024 } catch (e) {} return {anon: g('RssAnon'), pss} };
async function memory() {
  ST.state(D, MODES.light, MORE);
  natStart('sys'); await until(() => rect('body') && line('page') == 'sys', 20000); await wait(12000); const n = rss(NP.pid); natStop();
  const br = await chromium.launch({args: ['--font-render-hinting=none']}); const ctx = await br.newContext({viewport: {width: 860, height: 640}}); const p = await ctx.newPage();
  await p.goto(BASE + '/#app=settings&page=sys'); await wait(25000);
  const pids = cp.execSync(`pgrep -f -- '--type=renderer' || true`).toString().trim().split('\n').filter(Boolean).map(Number);
  let w = null; for (const id of pids) { try { const m = rss(id); if (!w || m.anon > w.anon) w = Object.assign(m, {pid: id}) } catch (e) {} }
  await br.close();
  const f = x => x == null ? 'n/a' : x.toFixed(1); const res = `native ${f(n.anon)} MB RssAnon, ${f(n.pss)} MB PSS; web (Chromium's renderer, #app=settings, System) ${w ? f(w.anon) : 'n/a'} MB RssAnon, ${w ? f(w.pss) : 'n/a'} MB PSS`;
  fs.writeFileSync(path.join(OUT, 'memory.txt'), res + '\n'); console.log('memory: ' + res);
  ok('24. the memory at idle with System open: the native one under the web one\'s', w && n.anon < w.anon, res);
}
const SETD = JSON.parse(fs.readFileSync(path.join(__dirname, 'settings_rd.json')));
(async () => {
  fs.writeFileSync(path.join(D, 'tm'), JSON.stringify(SETD));
  BR = await chromium.launch({ignoreDefaultArgs: ['--hide-scrollbars'], args: ['--font-render-hinting=none']});
  const rows = [];
  for (const mode of Object.keys(MODES)) {
    if (!only(mode) && !(ONLY || '').split(',').some(x => PAGES.includes(x))) continue;
    if (ONLY && !only(mode) && !['light', 'dark'].includes(mode)) continue;
    const pages = PAGES.filter(k => only(mode) || only(k));
    const P = await pageShots(mode, pages), N = await natShots(mode, pages);
    for (const k of pages) for (const at of ['top', 'end']) { const key = k + '-' + at;
      if (!P[key] || !N[key] || !N[key].b) { ok(`${mode} ${key}: drawn by both`, false); continue }
      const d = px(P[key].f, N[key].f, P[key].b, N[key].b, path.join(OUT, `${mode}-${key}`)); rows.push([mode, key, d.pct]);
      ok(`${mode} ${k} (${at}): ${d.pct}% of the window's pixels differ (page ${P[key].b.slice(2).join('x')}, native ${N[key].b.slice(2).join('x')})`, d.pct < LIMIT, {box: d.box, map: d.map}) }
    if (mode == 'light') for (const k of pages) { const a = JSON.stringify(P[k + '-harv']), b = JSON.stringify(N[k + '-harv']);
      ok(`${k}: Search in Settings finds the same rows on the native page as on the web one (${P[k + '-harv'].length})`, a == b, {page: P[k + '-harv'], native: N[k + '-harv']}) }
  }
  if (only('checklist') || only('search')) { const P = await pageSearch(); await checklist(P) }
  if (only('memory')) await memory();
  await BR.close();
  fs.writeFileSync(path.join(OUT, 'summary.tsv'), rows.map(r => r.join('\t')).join('\n') + '\n');
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) && R.length ? 0 : 1);
})().catch(e => { console.error(e); natStop(); process.exit(1) });
