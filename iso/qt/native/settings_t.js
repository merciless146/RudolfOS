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
const PAGES = (process.env.SET_PAGES || 'sys,kbd,mouse,upd,about,vol,mm,acc,disp,bt,remote,mob,net,web,notif,devs').split(',');
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
function natStart(page, extra) { try { fs.unlinkSync(SF) } catch (e) {} const e = Object.assign(env0(), {RUDOLF_SET_STATE: SF, RUDOLF_SET_NOW: NOWS, RUDOLF_SET_GPU: GPU, RUDOLF_SET_WWW: path.join(__dirname, '../../www')}, extra || {});
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
    /* (a view in the middle of a long page: the native one scrolled a few steps, the page then at the same place: pageMid) */
    { const bd = rect('body'); xdo(`mousemove ${bd[0] + 500} ${bd[1] + 300}`); for (let i = 0; i < 60; i++) xdo('click 4'); for (let i = 0; i < 6; i++) xdo('click 5'); xdo('mousemove 5 5'); await wait(800);
      const f = path.join(OUT, `${mode}-${k}-mid.nat.png`); sh(`import -window root ${f}`); out[k + '-mid'] = {f, b: rect('body'), y: +line('scroll')} }
    /* (1.6.3: a long page, Display's ~3,500 px, seen at more places on its way down: 5 steps of the wheel each) */
    for (const at of MIDS[k] || []) { const bd = rect('body'); xdo(`mousemove ${bd[0] + 500} ${bd[1] + 300}`); for (let i = 0; i < 5; i++) xdo('click 5'); xdo('mousemove 5 5'); await wait(800);
      const f = path.join(OUT, `${mode}-${k}-${at}.nat.png`); sh(`import -window root ${f}`); out[k + '-' + at] = {f, b: rect('body'), y: +line('scroll')} }
    try { fs.copyFileSync(SF, path.join(OUT, `${mode}-${k}.state`)) } catch (e) {}   /* (what the native page showed: its parts' places, for comparing) */
    out[k + '-harv'] = lines('harv').filter(x => x.startsWith(k + '|')).map(x => x.split('|').slice(1, 3).join('|') + (x.endsWith('|1') ? '|1' : ''));
    natStop(); await wait(300);
  }
  return out;
}
// ---- Search in Settings: the same results for the same words (the page's ssMatch over its pages, with System and About harvested as shown)
const MIDS = {disp: ['mid2', 'mid3', 'mid4', 'mid5', 'mid6', 'mid7'], mob: ['mid2', 'mid3'], net: ['mid2', 'mid3', 'mid4', 'mid5'], web: ['mid2', 'mid3'], devs: ['mid2']};
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
  { click(rect('side-sys')); await until(() => line('page') == 'sys', 4000); await wait(500); focusWin(); xdo('key Up Up Return'); await until(() => line('page') == 'bt', 4000); ok('3. the keyboard: Up, Up and Enter in the sidebar show Bluetooth (native since 1.6.3 PR 6)', line('page') == 'bt' && line('native') == '1', [line('page'), line('focus'), line('sidefoc'), nst().split('\n').filter(x => /^act/.test(x)).join(' / '), NP && NP.log.slice(-300)]) }
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
  const scrollTo = async n => { const bd = rect('body'); for (let i = 0; i < 40; i++) { const r = rect(n); if (r && r[1] >= bd[1] + 60 && r[1] + r[3] <= bd[1] + 550) break;
    xdo(`mousemove ${bd[0] + 500} ${bd[1] + 300} click ${!r || r[1] + r[3] > bd[1] + 550 ? 5 : 4}`); await wait(450) } await wait(300) };
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
// ---- the checklist of Update and recovery (settings25.js updPanel, sysupd25.js, sysext60.js, startup60.js, bring62.js, boot30.js)
async function checkUpd() {
  ST.state(D, MODES.light, MORE); const c0 = () => calls().length; let mark = c0(); const since = () => calls().slice(mark);
  /* (the keys go where the pointer is: there is no window manager under the test's Xvfb) */
  const pw = () => { const d = rect('dialog'); if (d) xdo(`mousemove ${d[0] + 5} ${d[1] + 5}`); xdo(`type --delay 30 '${PW.replace(/'/g, "'\\''")}'`); xdo('key Return') };
  /* (a wheel step at a time, the state read again after each: its places follow the scrolling) */
  const scrollTo = async n => { const bd = rect('body'); for (let i = 0; i < 40; i++) { const r = rect(n); if (r && r[1] >= bd[1] + 60 && r[1] + r[3] <= bd[1] + 550) break;
    xdo(`mousemove ${bd[0] + 500} ${bd[1] + 300} click ${!r || r[1] + r[3] > bd[1] + 550 ? 5 : 4}`); await wait(450) } await wait(300) };
  const after = async (n, dx) => { await scrollTo(n); const r = rect(n); xdo(`mousemove ${Math.round(r[0] + r[2] + (dx || 0))} ${Math.round(r[1] + r[3] / 2)} click 1`) };
  natStart('upd'); await until(() => rect('body') && line('page') == 'upd', 20000); await wait(1500);
  ok('U1. Update and recovery: what rudolfd says (sys/status, sysext/status, info, disks, check, bootmenu)', ['GET sys/status', 'GET sysext/status', 'GET info', 'GET disks', 'GET check', 'GET bootmenu'].every(x => since().includes(x)), since().slice(0, 400));
  { mark = c0(); await wait(800); await scrollTo('bmd');
    /* (the menu closed with Escape over it, then the drop-down, focused, takes the arrow keys as the page's select does) */
    const pick = async (n, i, of) => { click(rect(n)); await until(() => line('menu'), 3000); await wait(300); const m = line('menu').split(' ').map(Number), rh = (m[3] - 10) / of;
      xdo(`mousemove ${m[0] + 40} ${Math.round(m[1] + 5 + (i + .5) * rh)}`); await wait(200); xdo('click 1'); await until(() => !line('menu'), 3000); await wait(300) };   /* (the row chosen with the pointer) */
    await pick('bmd', 1, 5); await pick('bmt', 1, 8);
    await scrollTo('bms'); click(rect('bms')); await until(() => rect('dialog'), 3000); pw(); await until(() => /bootmenu\/set/.test(since()), 5000);
    ok('U13. Boot menu: Recovery mode, 2 seconds, Save: the administrator\'s permission, bootmenu/set', /POST bootmenu\/set\?default=rudolf-recovery\.conf&timeout=2/.test(since()), [since().slice(-300), nst().split('\n').filter(x => /^rect (bm|dialog)|^menu|^sel|^scroll|^act select/.test(x))]) }
  { const bd = rect('body'); xdo(`mousemove ${bd[0] + 500} ${bd[1] + 300}`); for (let i = 0; i < 80; i++) xdo('click 4'); await wait(600) }
  { mark = c0(); click(rect('wnl')); await wait(300); const r = rect('wnl'); xdo(`mousemove ${r[0] + 245} ${r[1] + 7} click 1`); await until(() => /open help new/.test(nst()), 3000); ok("U2. What's new: the shell opens Help at What's new", /act open help new/.test(nst())) }
  { mark = c0(); click(rect('ucheck')); await until(() => /job done/.test(nst()), 6000); ok('U3. Check now: sys/check, then rudolfd\'s job followed to its end', /POST sys\/check/.test(since()) && /GET job/.test(since()) && /job done 0/.test(nst()), since().slice(-300)) }
  { mark = c0(); await scrollTo('uauto'); click(rect('uauto')); await until(() => rect('dialog'), 3000); pw(); await until(() => /sys\/set\?k=auto/.test(since()), 5000);
    ok('U4. Download security updates automatically, off: the administrator\'s permission, then sys/set?k=auto&v=0', /POST desktop\/admin/.test(since()) && /POST sys\/set\?k=auto&v=0/.test(since()), since().slice(-300)) }
  { mark = c0(); await wait(800); await scrollTo('ubeta'); click(rect('ubeta')); await until(() => rect('dialog'), 3000); pw(); await until(() => /sys\/beta\?on=1/.test(since()), 5000);
    ok('U5. Get beta versions: the administrator\'s name and password, then sys/beta?on=1 (they go to rudolfd with it)', /POST sys\/beta\?on=1/.test(since()), since().slice(-300)) }
  { mark = c0(); await wait(1500); await after('ucheck', 50); await until(() => rect('dialog'), 3000); pw(); await until(() => /sys\/rollback/.test(since()), 5000);
    ok('U6. Roll back system…: the administrator\'s permission, then sys/rollback', /POST sys\/rollback/.test(since()), since().slice(-300)) }
  { mark = c0(); await wait(800); await scrollTo('uadv'); click(rect('uadv')); await until(() => rect('uurl'), 3000); click(rect('uchk')); await until(() => /sys\/urlcheck/.test(since()), 4000);
    ok('U7. Update address: the field with the address, Check this address (sys/urlcheck)', !!rect('uurl') && /POST sys\/urlcheck\?url=https%3A%2F%2Fmerciless146/.test(since()), since().slice(-200)) }
  { mark = c0(); await scrollTo('sxremove0'); click(rect('sxremove0')); await until(() => rect('dialog'), 3000); xdo('key Return'); await until(() => rect('dialog') && /ask sysext remove/.test(nst()), 3000); pw();
    await until(() => /POST sysext\/remove/.test(since()), 5000); await until(() => /job done/.test(nst().split('\n').slice(-3).join('\n')), 4000);
    ok('U8. System extensions › Remove: asked, the administrator\'s name and password, sysext/remove?name=…, the job followed', /POST sysext\/remove\?name=rudolf-ext-hplip/.test(since()), since().slice(-300)) }
  { mark = c0(); await wait(2000); await scrollTo('sxi'); await wait(500); click(rect('sxi')); await until(() => line('menu'), 3000); const m = line('menu').split(' ').map(Number); xdo(`mousemove ${m[0] + 40} ${m[1] + m[3] - 14} click 1`); await until(() => rect('dialog'), 3000); pw();
    await until(() => /sysext\/install/.test(since()), 5000); ok('U9. Install…: the extensions not installed yet, one chosen, sysext/install?name=…', /POST sysext\/install\?name=rudolf-ext-rtl8821/.test(since()), [since().slice(-300), nst().split('\n').filter(x => /^act|^rect (sxi|dialog)|^menu/.test(x)).slice(-8)]) }
  { mark = c0(); await wait(1500); await scrollTo('install0'); click(rect('install0')); await until(() => rect('dialog'), 3000); pw(); await until(() => /ask Erase nvme0n1/.test(nst()), 4000); await wait(300); xdo('key Return'); await until(() => /POST install\?/.test(since()), 4000);
    ok('U10. Install here…: the administrator\'s permission, Erase nvme0n1?, then install?disk=nvme0n1&confirm=nvme0n1', /POST install\?disk=nvme0n1&confirm=nvme0n1/.test(since()), since().slice(-300)) }
  { await until(() => /ask Rudolf OS is installed/.test(nst()), 4000); const d = rect('dialog'); if (d) { xdo(`mousemove ${d[0] + 5} ${d[1] + 5}`); xdo('key Escape') } }   /* (Later: the installation's own question after it) */
  { mark = c0(); await wait(1500); await scrollTo('factory'); click(rect('factory')); await until(() => rect('dialog'), 3000); pw(); await until(() => /ask Factory reset/.test(nst()), 4000); await wait(300); xdo('key Return'); await until(() => /factory-reset/.test(since()), 4000);
    ok('U11. Factory reset…: the administrator\'s permission, asked again, then factory-reset', /POST factory-reset/.test(since()), [since().slice(-200), nst().split('\n').filter(x => /^act|^rect (factory|dialog)/.test(x)).slice(-6)]) }
  { mark = c0(); await wait(800); await scrollTo('supck'); click(rect('supck')); await until(() => rect('dialog'), 3000); pw(); await until(() => /startup-check/.test(since()), 5000);
    ok('U12. Check this drive\'s start-up…: the administrator\'s name and password, sys/startup-check', /POST sys\/startup-check/.test(since())) }
  /* Bring in files (bring62.js brSection, brImport, brScan): Ann's files on the disk, a copy found on a drive, two files brought in, one of them EICAR's */
  { const L = JSON.parse(fs.readFileSync(path.join(D, 'ls.json'))), u = JSON.parse(L.data.bd95user); u.accts.forEach(a => { if (a.id == 'uAnnA1b2') a.disk = 1 }); L.data.bd95user = JSON.stringify(u); L.gen++;
    fs.writeFileSync(path.join(D, 'ls.json'), JSON.stringify(L)); const H = path.join(D, 'home'); fs.mkdirSync(path.join(H, '.rudolf'), {recursive: true}); fs.mkdirSync(path.join(H, 'Documents'), {recursive: true});
    fs.writeFileSync(path.join(H, 'Documents/notes.txt'), 'hello from the old computer\n'); fs.writeFileSync(path.join(H, 'Documents/eicar.txt'), 'X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*');
    fs.writeFileSync(path.join(H, '.rudolf/bring-imported.txt'), 'Documents/notes.txt\nDocuments/eicar.txt\n');
    const T = JSON.parse(JSON.stringify(SETD)); T['bring/found'] = {ok: true, sets: [{dir: '/media/sdb1/Rudolf OS - Bring your files/Old computer 2026-11-20 10.00', made: '2026-11-20T10:00:00Z', system: 'windows', drive: 1, last: true, people: [{name: 'Ann', dir: 'Ann', bytes: 2100, files: 2}]}]};
    T.job = {ok: true, running: false, status: 0, log: 'PROGRESS 2 2 2100 2100\nBrought in 2 files.\n'}; fs.writeFileSync(path.join(D, 'tm'), JSON.stringify(T));
    mark = c0(); await wait(800); await scrollTo('brlook'); click(rect('brlook')); await until(() => rect('brin0'), 4000); await scrollTo('brin0'); click(rect('brin0'));
    await until(() => /Your files from the old computer are here/.test(nst()), 8000); const c = since(), l = JSON.parse(fs.readFileSync(path.join(D, 'ls.json'))).data;
    ok('U14. Bring in files: Look on the drives (bring/found), Bring them in (bring/import), the job, the antivirus over what came in: EICAR\'s file deleted (home/delete) and written to the security log, the set marked brought in',
      /POST bring\/import\?acct=uAnnA1b2&dir=%2Fmedia%2Fsdb1%2FRudolf%20OS%20-%20Bring%20your%20files%2FOld%20computer%202026-11-20%2010\.00&person=Ann/.test(c) && /POST home\/delete\?acct=uAnnA1b2&path=Documents%2Feicar\.txt/.test(c) && !/delete\?.*notes/.test(c)
      && fs.existsSync(path.join(H, 'Documents/notes.txt')) && !fs.existsSync(path.join(H, 'Documents/eicar.txt')) && /Blocked EICAR\.TXT.*EICAR-Test-File/.test(l.bd95seclog || '') && /Old computer 2026-11-20 10\.00\|2026-11-20T10:00:00Z\|Ann/.test(l['bd95bring:uAnnA1b2'] || ''),
      [c.slice(-600), l.bd95seclog, l['bd95bring:uAnnA1b2'], nst().split('\n').filter(x => /^act/.test(x)).slice(-6)]) }
  /* Before an installation (bringStep): Windows found on the disk, Ann's Documents kept on a USB drive, then the installation goes on */
  { const T = JSON.parse(JSON.stringify(SETD)); T['bring/result'] = {ok: true, found: true, total: 1048576, systems: [{name: 'Windows 11', part: 'nvme0n1p3', state: 'hibernated', people: [{id: 'p1', name: 'Ann', bytes: 1048576, folders: [{k: 'Documents', bytes: 1048576, files: 3}]}]}], locked: []};
    T.disks.disks.push({name: 'sdb', model: 'USB DISK', bytes: 32e9, removable: true, partitions: [{name: 'sdb1', fs: 'exfat', label: 'KEEP', mount: '', bytes: 32e9}]}); T.job = {ok: true, running: false, status: 0, log: 'PROGRESS 3 3 1048576 1048576\nCopied 3 files.\n'};
    fs.writeFileSync(path.join(D, 'tm'), JSON.stringify(T)); mark = c0(); await wait(800);
    await scrollTo('install0'); click(rect('install0')); await until(() => rect('dialog'), 3000); pw(); await until(() => rect('bsgo') && /bringstep .* has another system/.test(nst()), 6000); await wait(500);
    sh(`import -window root ${path.join(OUT, 'bringstep.png')}`); const g = rect('bsgo'); const keep = !!rect('bskeep') && !!rect('bsf0') && !!rect('bsto'); xdo(`mousemove ${g[0] + 10} ${g[1] + 8} click 1`); await until(() => /Copied and checked/.test(nst()), 6000); await wait(400);
    const g2 = rect('bsgo'); xdo(`mousemove ${g2[0] + 10} ${g2[1] + 8} click 1`); await until(() => /ask Erase nvme0n1/.test(nst()), 4000); await wait(300); xdo('key Return'); await until(() => /POST install\?/.test(since()), 4000);
    const c = since(); ok('U15. Install here… on a disk with Windows (bringStep): the disk looked at (bring/scan, bring/result), Keep them, the USB drive mounted (mount?dev=sdb1), bring/copy, its job, Continue to install, then Erase nvme0n1? and install',
      keep && /POST bring\/scan\?disk=nvme0n1/.test(c) && /GET bring\/result\?disk=nvme0n1/.test(c) && /POST mount\?dev=sdb1/.test(c) && /POST bring\/copy/.test(c) && /POST install\?disk=nvme0n1&confirm=nvme0n1/.test(c) && !/bring\/decide/.test(c),
      [c.slice(-500), keep, nst().split('\n').filter(x => /^act|^rect bs/.test(x)).slice(-10)]);
    await until(() => /ask Rudolf OS is installed/.test(nst()), 4000); const d = rect('dialog'); if (d) { xdo(`mousemove ${d[0] + 5} ${d[1] + 5}`); xdo('key Escape') } fs.writeFileSync(path.join(D, 'tm'), JSON.stringify(SETD)) }
  natStop();
}
// ---- the checklist of Keyboard (settings25.js kbdPanel) and Mouse and touchpad (devices26.js mousePanel, settings31.js): what each saves
async function checkKbdMouse() {
  ST.state(D, MODES.light, MORE); const c0 = () => calls().length; let mark = c0(); const since = () => calls().slice(mark);
  const LS = () => JSON.parse(fs.readFileSync(path.join(D, 'ls.json'))).data, SET = () => JSON.parse(LS()['bd95set:uAnnA1b2'] || '{}');
  const scrollTo = async n => { const bd = rect('body'); for (let i = 0; i < 60; i++) { const r = rect(n); if (r && r[1] >= bd[1] + 60 && r[1] + r[3] <= bd[1] + 550) break;
    xdo(`mousemove ${bd[0] + 500} ${bd[1] + 300} click ${!r || r[1] + r[3] > bd[1] + 550 ? 5 : 4}`); await wait(450) } await wait(300) };
  const pick = async (n, i, of) => { click(rect(n)); await until(() => line('menu'), 3000); await wait(300); const m = line('menu').split(' ').map(Number), rh = (m[3] - 10) / of;
    xdo(`mousemove ${m[0] + 40} ${Math.round(m[1] + 5 + (i + .5) * rh)}`); await wait(200); xdo('click 1'); await until(() => !line('menu'), 3000); await wait(300) };
  const over = () => { const b = rect('body'); xdo(`mousemove ${b[0] + 400} ${b[1] + 20}`) };
  natStart('kbd'); await until(() => rect('body') && line('page') == 'kbd', 20000); await wait(1500);
  { await pick('kbla', 2, 16); click(rect('kbadd')); await until(() => /"de"/.test(LS()['bd95kbl:uAnnA1b2'] || ''), 4000);
    ok('K1. Add a layout: Deutsch chosen, Add: the account\'s layouts ["us","de"] (bd95kbl:ACCT)', LS()['bd95kbl:uAnnA1b2'] == '["us","de"]', LS()['bd95kbl:uAnnA1b2']) }
  { await until(() => rect('kbup1'), 3000); await wait(500); /* (the page built again after Add: its buttons placed) */ click(rect('kbup1')); await until(() => LS()['bd95kbl:uAnnA1b2'] == '["de","us"]', 4000); const a = LS()['bd95kbl:uAnnA1b2'];
    await until(() => rect('kbrm1'), 3000); await wait(300); click(rect('kbrm0')); await until(() => LS()['bd95kbl:uAnnA1b2'] == '["us"]', 4000);
    ok('K2. ▲ moves Deutsch first (["de","us"]), Remove takes it away (["us"])', a == '["de","us"]' && LS()['bd95kbl:uAnnA1b2'] == '["us"]', [a, LS()['bd95kbl:uAnnA1b2']]) }
  { await wait(500); await pick('kbime', 1, 4); await until(() => SET().ime == 'ja', 4000); await scrollTo('kbch'); click(rect('kbch')); await until(() => SET().cliphist === false, 4000);
    ok('K3. Input method Japanese, Clipboard history off: saved at once (ime "ja", cliphist false in bd95set:ACCT)', SET().ime == 'ja' && SET().cliphist === false, SET()) }
  { await scrollTo('kbc0'); click(rect('kbc0')); await until(() => /kbcap 1 files/.test(nst()), 3000); over(); xdo('key ctrl+alt+k'); await until(() => /Ctrl\+Alt\+K/.test(LS()['bd95kb:uAnnA1b2'] || ''), 4000);
    const m1 = LS()['bd95kb:uAnnA1b2']; await until(() => rect('kbd0'), 3000); await wait(300); click(rect('kbd0')); await until(() => LS()['bd95kb:uAnnA1b2'] == '{}', 4000);
    ok('K4. Change, then Ctrl+Alt+K: Open File Explorer\'s keys (bd95kb:ACCT {"files":"Ctrl+Alt+K"}; the shell asked for every key meanwhile: kbcap 1, then 0); Default: {}', m1 == '{"files":"Ctrl+Alt+K"}' && /act kbcap 0/.test(nst()) && LS()['bd95kb:uAnnA1b2'] == '{}', [m1, LS()['bd95kb:uAnnA1b2']]) }
  { await wait(400); await scrollTo('kbc1'); click(rect('kbc1')); await until(() => /kbcap 1 desktop/.test(nst()), 3000); over(); xdo('key ctrl+Escape'); await until(() => /Ctrl\+Escape/.test(LS()['bd95kb:uAnnA1b2'] || ''), 4000);
    const m = LS()['bd95kb:uAnnA1b2']; await until(() => /notify Shortcut moved/.test(nst()), 3000);
    ok('K5. Ctrl+Escape, Open the Start menu\'s, for Show the desktop: it moves ({"desktop":"Ctrl+Escape","start":""}) and "Shortcut moved" is said', m == '{"desktop":"Ctrl+Escape","start":""}' && /notify Shortcut moved/.test(nst()), m) }
  { await wait(400); await scrollTo('kbc2'); click(rect('kbc2')); await until(() => /kbcap 1 run/.test(nst()), 3000); over(); xdo('key e'); await wait(400); const refused = /kbcap refused E/.test(nst()); xdo('key Escape'); await wait(800);
    ok('K6. Change, E alone: "Use Ctrl, Alt or Super"; Esc cancels and saves nothing (the page\'s own Esc fixed in 1.6.2 to do the same)', refused && !/"run"/.test(LS()['bd95kb:uAnnA1b2']) && /act kbcap 0/.test(nst().split('\n').slice(-6).join('\n')), [LS()['bd95kb:uAnnA1b2'], nst().split('\n').filter(x => /^act kb/.test(x)).slice(-4)]) }
  { await scrollTo('kbr'); click(rect('kbr')); await until(() => LS()['bd95kb:uAnnA1b2'] == '{}', 4000); ok('K7. Reset all shortcuts: {}', LS()['bd95kb:uAnnA1b2'] == '{}') }
  natStop();
  natStart('mouse'); await until(() => rect('body') && line('page') == 'mouse', 20000); await wait(1500);
  { mark = c0(); const r = rect('ptsp'); xdo(`mousemove ${Math.round(r[0] + r[2] * (8 + 204 * (1.5 - 0.3) / 2.7) / 220)} ${Math.round(r[1] + r[3] / 2)} click 1`); await until(() => /POST pointer/.test(since()), 4000); await wait(300);
    const st = SET(); ok('M1. Pointer speed 1.5: saved (ptr.speed 1.5, msens 6) and sent (pointer?speed=1.5&accel=1&tap=1&natural=0&typing=1)', st.ptr && st.ptr.speed == 1.5 && st.msens == 6 && /POST pointer\?speed=1\.5&accel=1&tap=1&natural=0&typing=1/.test(since()), [st, since().slice(-200)]) }
  { mark = c0(); click(rect('pttap')); await until(() => /tap=0/.test(since()), 4000); ok('M2. Tap to click off: pointer?…&tap=0, ptr.tap false', /POST pointer\?speed=1\.5&accel=1&tap=0/.test(since()) && SET().ptr.tap === false, since().slice(-200)) }
  { await scrollTo('mpb'); await pick('mpb', 1, 2); await until(() => SET().mhand == 'left', 4000); ok('M3. Primary button: Right button (left-handed): mhand "left"', SET().mhand == 'left') }
  { await scrollTo('mpd'); click(rect('mpd')); await wait(300); xdo('key Right'); await until(() => SET().mdbl == 600, 4000); const d = rect('dbltest'); xdo(`mousemove ${d[0] + 20} ${d[1] + 10} click --repeat 2 --delay 80 1`); await until(() => /dbltest 1/.test(nst()), 3000);
    ok('M4. Double-click speed: a click at its middle (550), then →: mdbl 600; the folder opens on a double-click', SET().mdbl == 600 && /dbltest 1/.test(nst()), [SET().mdbl, nst().split('\n').filter(x => /^act (persistS mdbl|dbltest)/.test(x))]) }
  { await scrollTo('mpo'); click(rect('mpo')); await until(() => SET().msingle === true, 4000); ok('M5. Open files and folders with a single click: msingle true', SET().msingle === true) }
  { mark = c0(); await scrollTo('ptdef'); click(rect('ptdef')); await until(() => SET().mhand == 'right' && SET().msingle === false, 4000); const st = SET();
    ok('M6. Restore defaults: ptr {speed 1, accel 1, tap, no natural, typing}, msens 5, mhand right, mdbl 450, mscroll 5, msingle false; sent to rudolfd', JSON.stringify(st.ptr) == '{"speed":1,"accel":1,"tap":true,"natural":false,"typing":true}' && st.msens == 5 && st.mdbl == 450 && st.mscroll == 5 && /POST pointer\?speed=1&accel=1&tap=1&natural=0&typing=1/.test(since()), st) }
  natStop();
}

// ---- 1.6.3 Part 3: Volume Controls, Accessibility, Multimedia Settings (their checklists: V, A, MM), each step checked by what the native one
// shows, asks of rudolfd and saves, as the page's code does it (base_v24.html volPanelS/secReset, osd31.js, sndrs60.js, settings25.js,
// mictest59.js, call59.js; accPanelS and its modules, tts25.js; mmPanelS)
async function checkVolAccMm() {
  ST.state(D, MODES.light, MORE); const c0 = () => calls().length; let mark = c0(); const since = () => calls().slice(mark);
  const LS = () => JSON.parse(fs.readFileSync(path.join(D, 'ls.json'))).data, SET = () => JSON.parse(LS()['bd95set:uAnnA1b2'] || '{}');
  const scrollTo = async n => { const bd = rect('body'); for (let i = 0; i < 80; i++) { const r = rect(n); if (r && r[1] >= bd[1] + 60 && r[1] + r[3] <= bd[1] + 520) break;
    xdo(`mousemove ${bd[0] + 500} ${bd[1] + 300} click ${!r || r[1] + r[3] > bd[1] + 520 ? 5 : 4}`); await wait(350) } await wait(300) };
  const pick = async (n, i, of) => { click(rect(n)); await until(() => line('menu'), 3000); await wait(300); const m = line('menu').split(' ').map(Number), rh = (m[3] - 10) / of;
    xdo(`mousemove ${m[0] + 40} ${Math.round(m[1] + 5 + (i + .5) * rh)}`); await wait(200); xdo('click 1'); await until(() => !line('menu'), 3000); await wait(300) };
  const confirm = async b => { click(rect(b)); await until(() => rect('dialog'), 3000); xdo('key Return'); await wait(900) };
  const notes = re => nst().split('\n').filter(x => re.test(x));
  natStart('vol', {RUDOLF_SET_NOSOUND: '1'}); await until(() => rect('body') && line('page') == 'vol', 20000); await wait(1500);
  { await scrollTo('sndv-startup'); { const r = rect('sndv-startup'); xdo(`mousemove ${Math.round(r[0] + 8 + .7 * (r[2] - 16))} ${Math.round(r[1] + r[3] / 2)} click 1`) } await wait(300); xdo('key Left'); xdo('key Left'); await until(() => line('dirty') == '1', 3000); await wait(400);
    const sf = notes(/^act sfx startup/); ok('V1. Startup chime\'s slider, two steps left: 68%, unapplied; let go, the sound tried at the working copy\'s volume (the shell: sfx startup 0.4760 0)', line('dirty') == '1' && sf.length && /sfx startup 0\.4760 0$/.test(sf[sf.length - 1]), sf) }
  { await pick('sndp-error', 2, 6); await until(() => /snd error p 2/.test(nst()), 3000); click(rect('sndt-click'));  await wait(400);
    ok('V2. Errors: Triple beep (p 2), heard at once even if off (forced); ▶ on Button clicks plays it (sfx click 0.2800 0)', /act sfx error 0\.4900 2/.test(nst()) && /act sfx click 0\.2800 0/.test(nst()), notes(/^act sfx/)) }
  { mark = c0(); await scrollTo('sysmute'); const r = rect('sysmute'); xdo(`mousemove ${r[0] + 10} ${r[1] + r[3] / 2} click 1`); await until(() => /volume\?muted=1/.test(since()), 3000);
    click(rect('sysvol')); xdo('key Right'); await until(() => /volume\?set=/.test(since()), 3000);
    ok('V3. System volume: Mute (volume?muted=1), then the slider (volume?set=N), at once, as osd31.js', /POST volume\?muted=1/.test(since()) && /POST volume\?set=\d+/.test(since()), since().slice(-300)) }
  { await scrollTo('rsall'); await pick('rsall', 3, 6); await until(() => /snd scheme Retro/.test(nst()), 3000);
    ok('V4. Use one style everywhere: Retro: "Retro for 18 of 18 categories. Press Apply to keep it." and the startup chime tried', /snd scheme Retro 18/.test(nst()) && /act sfx startup .* 3$/.test(notes(/^act sfx startup/).slice(-1)[0] || ''), notes(/scheme|sfx startup/)) }
  { await confirm('apply'); await until(() => SET().snd && SET().snd.startup, 4000); const s = SET().snd;
    ok('V5. Apply: snd saved as the page saves it (startup 68 % Retro boot, error Triple beep… p 2 by Retro: the scheme\'s Triple beep, click Typewriter)', s && s.startup.vol == 68 && s.startup.p == 3 && s.error.p == 2 && s.click.p == 2 && s.master.vol == 70 && s.logout.p == 1, s) }
  { await confirm('reset'); await until(() => SET().snd && SET().snd.startup.vol == 70, 4000); const s = SET().snd;
    ok('V6. Reset to defaults: every category on, its own volume, Classic (p 0), the master volume kept; saved at once', s.startup.vol == 70 && s.startup.p == 0 && s.click.vol == 40 && s.error.p == 0 && s.master.on === true && line('dirty') == '0', s) }
  { mark = c0(); await scrollTo('card'); await pick('card', 1, 2); await until(() => /POST audio\?card=1/.test(since()), 3000);
    await scrollTo('mtsel'); await pick('mtsel', 1, 2); await until(() => /audio\/mic\?name=/.test(since()), 3000);
    await scrollTo('echo'); click(rect('echo')); await until(() => /sound\/echo\?on=1/.test(since()), 3000);
    ok('V7. Sound card 1 (audio?card=1), the headset microphone (audio/mic?name=…HiFi__Headset__source), echo cancelling on (sound/echo?on=1)', /POST audio\?card=1/.test(since()) && /POST audio\/mic\?name=alsa_input\.pci-0000_04_00\.6\.HiFi__Headset__source/.test(since()) && /POST sound\/echo\?on=1/.test(since()) && /Chosen: Headset Microphone \(for every app\)/.test(nst()), since()) }
  { await scrollTo('mix-system'); click(rect('mix-system')); xdo('key Home'); await until(() => /"system":0/.test(LS()['bd95mix:uAnnA1b2'] || ''), 3000);
    ok('V8. App volume: system at 0 %: bd95mix:ACCT {"system":0}, at once (mixSet)', LS()['bd95mix:uAnnA1b2'] == '{"system":0}', LS()['bd95mix:uAnnA1b2']) }
  natStop();
  natStart('acc', {RUDOLF_SET_NOSOUND: '1'}); await until(() => rect('body') && line('page') == 'acc', 20000); await wait(1500);
  { await pick('csize', 3, 6); await scrollTo('rmotion'); click(rect('rmotion')); await scrollTo('slowk'); await pick('slowk', 2, 5); await until(() => line('dirty') == '1', 3000);
    await confirm('apply'); await until(() => SET().slowk == 500, 4000); const s = SET();
    ok('A1. Cursor size Large, Reduce motion, Slow keys 500 ms, Apply: csize 48, rmotion true, slowk 500 (bd95set:ACCT), "Settings applied."', s.csize === 48 && s.rmotion === true && s.slowk === 500 && /Settings applied/.test(line('foot')), s) }
  { mark = c0(); await scrollTo('atp'); click(rect('atp')); await until(() => /say\?/.test(since()), 3000); await scrollTo('tts-ja'); click(rect('tts-ja')); await until(() => (since().match(/say\?/g) || []).length > 1, 3000);
    const u8 = decodeURIComponent(since()); ok('A2. Speak a sample (say?lang=en-us&text=Hello!…), Japanese\'s ▶ (say?lang=ja, its sample in kana)', /POST say\?lang=en-us&text=Hello!/.test(u8) && /POST say\?lang=ja&text=こんにちは。これはおんせいよみあげのこえです。/.test(u8), u8.slice(-400)) }
  { await confirm('reset'); await until(() => SET().slowk === 0, 4000); const s = SET();
    ok('A3. Reset to defaults: DEFS.acc kept at once (csize "system", rmotion false, slowk 0, lang "en"), "Defaults restored."', s.csize == 'system' && s.rmotion === false && s.slowk === 0 && s.lang == 'en' && /Defaults restored/.test(line('foot')), s) }
  { const r = rect('mvgo'); await scrollTo('mvgo'); const m = rect('mvgo'); xdo(`mousemove ${m[0] + 160} ${m[1] + m[3] - 6} click 1`); await until(() => line('page') == 'mouse', 3000);
    ok('A4. The link "Mouse and touchpad" opens that page', line('page') == 'mouse', nst().split('\n').filter(x => /^page|open mouse/.test(x))) }
  natStop();
  natStart('mm', {RUDOLF_SET_NOSOUND: '1', RUDOLF_SET_RUFFLE: 'ok'}); await until(() => rect('body') && line('page') == 'mm', 20000); await wait(1500);
  { click(rect('inst-ruffle')); await until(() => line('dirty') == '1', 3000); await confirm('apply'); await until(() => /"ruffle":\{"inst":true/.test(LS().bd95mm || ''), 4000);
    const m = JSON.parse(LS().bd95mm || '{}'), lg = JSON.parse(LS().bd95seclog || '{}');
    ok('MM1. Ruffle Installed (and so enabled), Apply: bd95mm {plug:{ruffle:{inst:true,en:true}…}, set:{…}}, the security log "Ann installed plugin ruffle"', m.plug && m.plug.ruffle.inst && m.plug.ruffle.en && !m.plug.gb.inst && m.set.rufQ == 'high' && lg.sys && lg.sys.some(e => e.m == 'Ann installed plugin ruffle'), [m, lg.sys]) }
  { await scrollTo('rufUrl'); click(rect('rufUrl')); xdo("type --delay 20 'http://example.com'"); await until(() => line('dirty') == '1', 3000); await confirm('apply');
    ok('MM2. A runtime address that is not https://: refused ("must start with https://"), nothing saved', /mmm The runtime address must start with https:\/\//.test(nst()) && JSON.parse(LS().bd95mm).set.rufUrl == '', notes(/mmm/)) }
  { await confirm('revert'); await scrollTo('mmchk'); click(rect('mmchk')); await until(() => /mmm Ruffle downloaded and ready/.test(nst()), 4000);
    ok('MM3. Revert ("Changes reverted."), then Check Ruffle download: "Ruffle downloaded and ready."', /mmm Ruffle downloaded and ready/.test(nst()) && /footer Changes reverted/.test(nst()), notes(/mmm|footer/)) }
  { await scrollTo('gbPal'); await pick('gbPal', 3, 4); await until(() => line('dirty') == '1', 3000); await confirm('reset'); await wait(300);
    ok('MM4. Palette Amber, then Reset to defaults: the preferences back, unapplied until Apply ("Preferences reset. Apply to keep them."), nothing saved', /footer Preferences reset/.test(nst()) && line('dirty') == '0' && JSON.parse(LS().bd95mm).set.gbPal == 'green', notes(/footer|mm gbPal/)) }
  { mark = c0(); await scrollTo('mmf1'); click(rect('mmf1')); click(rect('mmp1')); await wait(400);
    ok('MM5. INSTALLERS folder and Emu: asked of the native shell (open explorer C:\\PROGRAMS\\INSTALLERS, open gameboy)', /act open explorer C:\\PROGRAMS\\INSTALLERS/.test(nst()) && /act open gameboy/.test(nst()), notes(/act open/)) }
  natStop();
}

// ---- 1.6.3 Part 3, PR 5: Display Properties' checklist (D): what the native one saves and asks, as dispPanelS and its modules do
async function checkDisp() {
  ST.state(D, MODES.light, MORE); const c0 = () => calls().length; let mark = c0(); const since = () => calls().slice(mark);
  const LS = () => JSON.parse(fs.readFileSync(path.join(D, 'ls.json'))).data, SET = () => JSON.parse(LS()['bd95set:uAnnA1b2'] || '{}'), SYS = () => JSON.parse(LS().bd95sys || '{}');
  const scrollTo = async n => { const bd = rect('body'); for (let i = 0; i < 120; i++) { const r = rect(n); if (r && r[1] >= bd[1] + 60 && r[1] + r[3] <= bd[1] + 520) break;
    xdo(`mousemove ${bd[0] + 500} ${bd[1] + 300} click ${!r || r[1] + r[3] > bd[1] + 520 ? 5 : 4}`); await wait(250) } await wait(300) };
  const pick = async (n, i, of) => { click(rect(n)); await until(() => line('menu'), 3000); await wait(300); const m = line('menu').split(' ').map(Number), rh = (m[3] - 10) / of;
    xdo(`mousemove ${m[0] + 40} ${Math.round(m[1] + 5 + (i + .5) * rh)}`); await wait(200); xdo('click 1'); await until(() => !line('menu'), 3000); await wait(300) };
  const confirm = async b => { click(rect(b)); await until(() => rect('dialog'), 3000); xdo('key Return'); await wait(900) };
  cp.execFileSync('python3', ['-c', 'import sys\nfrom PIL import Image\nImage.new("RGB", (3000, 1500), (200, 80, 40)).save(sys.argv[1])', path.join(D, 'wall.png')]);
  natStart('disp', {RUDOLF_SET_NOSOUND: '1', RUDOLF_SET_PICK: path.join(D, 'wall.png')}); await until(() => rect('body') && line('page') == 'disp', 20000); await wait(1500);
  { click(rect('dyn-stag')); await until(() => line('dirty') == '1', 3000); await scrollTo('acsw'); const a = rect('acsw'); xdo(`mousemove ${Math.round(a[0] + 5 * 24.7 + 10)} ${Math.round(a[1] + 10)} click 1`); await wait(400);
    await confirm('apply'); await until(() => SET().dyn == 'stag', 4000); const st = SET();
    ok('D1. Moonlit Stag chosen and Green from the swatches, Apply: dyn "stag", accent "#16a34a" (bd95set:ACCT), as the page saves them', st.dyn == 'stag' && st.accent == '#16a34a', st) }
  { await scrollTo('lkck'); await pick('lkck', 1, 3); await until(() => (SYS().lk || {}).clock == 'small', 3000);
    ok('D2. Lock and sign-in screen\'s Clock: Small, kept at once for everyone (bd95sys.lk.clock), the rest of bd95sys kept', (SYS().lk || {}).clock == 'small' && SYS().lk.dim === 30, SYS()) }
  { await scrollTo('ssmin'); await pick('ssmin', 0, 10); await until(() => SYS().ssmin === 1, 3000);
    ok('D3. Screen saver\'s Start after: 1 minute, kept at once (bd95sys.ssmin 1)', SYS().ssmin === 1, SYS()) }
  { await scrollTo('dtab'); await pick('dtab', 1, 3); await until(() => SET().tablet == 'on', 3000);
    ok('D4. Tablet mode On: kept at once (persistS: tablet "on"), not left to Apply', SET().tablet == 'on' && line('dirty') == '0', [SET().tablet, line('dirty')]) }
  { mark = c0(); await scrollTo('dres'); await pick('dres', 2, 5); await scrollTo('dsap'); click(rect('dsap')); await until(() => rect('dialog'), 3000); xdo('key Return'); await until(() => /POST screen\?/.test(since()), 4000);
    ok('D5. Resolution 1366×768, Apply, Change (confirmed): screen?res=1366x768&fps=60', /POST screen\?res=1366x768&fps=60/.test(since()), since()) }
  { mark = c0();
    await scrollTo('wpc'); click(rect('wpc')); await until(() => /POST idbm\/put/.test(since()), 6000); await wait(500); await confirm('apply'); await until(() => SET().wpimg, 4000); const st = SET();
    ok('D7. Picture, From this computer…: kept as the page keeps it (idbm/put of rudolfos/kv "wp:ID"), then Apply: wpimg ID, wpname "wall.png", dyn ""; bd95wpids:ACCT', /POST idbm\/put\?f=[0-9a-f]{16}/.test(since()) && st.wpimg && st.wpname == 'wall.png' && st.dyn === '' && (LS()['bd95wpids:uAnnA1b2'] || '').includes(st.wpimg), [st.wpimg, st.wpname, st.dyn, nst().split('\n').filter(x => /wpm|idbm|play|POST/.test(x)), since().slice(-300)]) }
  { mark = c0(); await scrollTo('dgp'); await pick('dgp', 1, 3); await until(() => /POST display\?mode=full/.test(since()), 4000);
    ok('D6. Graphics card: Use it for games and the desktop (display?mode=full)', /POST display\?mode=full/.test(since()), since()) }
  natStop();
}
// ---- Bluetooth (bt51.js btPanel; the stand-in's bt/* keep the status in DIR/bt.json): each device's buttons, Search, found, the pairing question, Off
async function checkBt() {
  ST.state(D, MODES.light, MORE); for (const f of ['bt.json', 'btask']) try { fs.unlinkSync(path.join(D, f)) } catch (e) {}
  const c0 = () => calls().length; let mark = c0(); const since = () => calls().slice(mark); const acts = () => lines('act').join('\n');
  const press = async (n, re) => { mark = c0(); await until(() => rect(n), 4000); click(rect(n)); await until(() => re.test(since()), 6000); await wait(900) };
  natStart('bt'); await until(() => rect('body') && line('page') == 'bt', 20000); await wait(1500);
  await press('btm1.0', /POST bt\/connect\?mac=AA:BB:CC:DD:EE:02/); await until(() => /btmsg Headphones "Pro" is connected\./.test(acts()), 3000);
  ok('B1. Headphones "Pro": Connect (bt/connect?mac=), "… is connected.", its row then offers Disconnect', /POST bt\/connect\?mac=AA:BB:CC:DD:EE:02/.test(since()) && /btmsg Headphones "Pro" is connected\./.test(acts()) && /bt adapter 2 2/.test(acts()), since());
  await press('btm0.1', /POST bt\/remove\?mac=AA:BB:CC:DD:EE:01/); await until(() => /bt adapter 1 2/.test(lines('act').pop() || ''), 3000);
  ok('B2. Game controller: Forget (bt/remove?mac=), "… is forgotten. Pair it again to use it.", one device left paired', /btmsg Game controller is forgotten\. Pair it again to use it\./.test(acts()) && /bt adapter 1 2/.test(acts()), since());
  await press('btn0.0', /POST bt\/pair\?mac=AA:BB:CC:DD:EE:04/); await until(() => /btmsg Keyboard K3 is paired and connected\./.test(acts()), 3000);
  ok('B3. Keyboard K3 (the nearest first): Pair (bt/pair?mac=), "Pairing with …" then "… is paired and connected."', /btmsg Pairing with Keyboard K3… keep it in pairing mode/.test(acts()) && /btmsg Keyboard K3 is paired and connected\./.test(acts()), since());
  await press('btf', /POST bt\/scan\?s=15/);
  ok('B4. Search for devices: bt/scan?s=15', /POST bt\/scan\?s=15/.test(since()), since());
  await press('btv', /POST bt\/visible/);
  ok('B5. Let this computer be found: bt/visible', /POST bt\/visible/.test(since()), since());
  mark = c0(); fs.writeFileSync(path.join(D, 'btask'), JSON.stringify({t: 1764063600, kind: 'confirm', name: 'Phone', code: '482913'}));
  await until(() => rect('dialog'), 5000); const asked = /ask bt confirm 482913/.test(acts()); xdo('key Return'); await until(() => /POST bt\/answer/.test(since()), 4000);
  ok('B6. A phone\'s pairing question (bt/ask) asked with its code; Yes, pair: bt/answer?ok=1', asked && /POST bt\/answer\?ok=1/.test(since()), [asked, since()]);
  await wait(500); await press('bto', /POST bt\/power\?on=0/);
  ok('B7. Bluetooth Off: bt/power?on=0', /POST bt\/power\?on=0/.test(since()), since());
  natStop(); for (const f of ['bt.json', 'btask']) try { fs.unlinkSync(path.join(D, f)) } catch (e) {}
}
// ---- Remote access (remote50.js remotePanel; the stand-in keeps the state in DIR/remote.json, rudolf-remote's accounts in DIR/remote-accounts)
async function checkRemote() {
  ST.state(D, MODES.light, MORE); for (const f of ['remote.json', 'remote-accounts', 'health']) try { fs.unlinkSync(path.join(D, f)) } catch (e) {}
  const c0 = () => calls().length; let mark = c0(); const since = () => calls().slice(mark); const rdf = f => { try { return fs.readFileSync(path.join(D, f), 'utf8') } catch (e) { return '' } };
  const LS = () => JSON.parse(fs.readFileSync(path.join(D, 'ls.json'))).data;
  const pw = () => { const d = rect('dialog'); if (d) xdo(`mousemove ${d[0] + 5} ${d[1] + 5}`); xdo(`type --delay 30 '${PW.replace(/'/g, "'\\''")}'`); xdo('key Return') };
  const bottom = async () => { const bd = rect('body'); xdo(`mousemove ${bd[0] + 500} ${bd[1] + 300}`); for (let i = 0; i < 30; i++) xdo('click 5'); xdo('mousemove 5 5'); await wait(600) };
  natStart('remote'); await until(() => rect('body') && line('page') == 'remote', 20000); await wait(1500);
  { await bottom(); mark = c0(); click(rect('rmu1')); await until(() => rect('dialog'), 3000); pw(); await until(() => /POST health/.test(since()), 5000); await wait(500);
    const u = JSON.parse(LS().bd95user || '{}'), bob = (u.accts || []).find(a => a.n == 'Bob') || {}, ra = rdf('remote-accounts');
    ok('R1. Bob\'s unattended access, with the administrator\'s permission: bd95user\'s Bob.ru true (the rest kept), rudolf-remote\'s accounts "Bob\\tHASH\\tuser\\t1", a note in System health', bob.ru === true && bob.h && /^Bob\t[^\t]+\tuser\t1$/m.test(ra) && /^Ann\t[^\t]+\tadmin\t0$/m.test(ra) && /unattended access turned on for an account/.test(rdf('health')), {bob: Object.keys(bob), ra: ra.replace(/\t[^\t]{20,}\t/g, '\tHASH\t'), h: rdf('health')}) }
  { mark = c0(); await until(() => rect('rme'), 3000); click(rect('rme')); await until(() => /POST remote\/end/.test(since()), 3000); await until(() => !rect('rme'), 4000);
    ok('R2. End the session: remote/end, then "Nobody."', /POST remote\/end/.test(since()) && !rect('rme'), since()) }
  { mark = c0(); const bd = rect('body'); xdo(`mousemove ${bd[0] + 500} ${bd[1] + 300}`); for (let i = 0; i < 30; i++) xdo('click 4'); await wait(600); click(rect('rmo')); await until(() => rect('dialog'), 3000); pw(); await until(() => /POST remote\?on=0/.test(since()), 5000); await wait(600);
    const log = (JSON.parse(LS().bd95seclog || '{}').sys || []).map(e => e.m);
    ok('R3. Remote access off, with the administrator\'s permission: remote?on=0, "Remote access off" in the security log', /POST remote\?on=0/.test(since()) && log.includes('Remote access off') && /remote off/.test(lines('act').join('\n')), [since(), log.slice(-2)]) }
  natStop(); for (const f of ['remote.json', 'remote-accounts', 'health']) try { fs.unlinkSync(path.join(D, f)) } catch (e) {}
}
// ---- Mobile network (mobile60.js mobPanelS; the stand-in's modems: 0 connected with its SIM's settings, 1 locked with its PIN)
async function checkMob() {
  ST.state(D, MODES.light, MORE); try { fs.unlinkSync(path.join(D, 'mobpin')) } catch (e) {}
  const c0 = () => calls().length; let mark = c0(); const since = () => calls().slice(mark); const acts = () => lines('act').join('\n');
  const pw = () => { const d = rect('dialog'); if (d) xdo(`mousemove ${d[0] + 5} ${d[1] + 5}`); xdo(`type --delay 30 '${PW.replace(/'/g, "'\\''")}'`); xdo('key Return') };
  const scrollTo = async n => { const bd = rect('body'); for (let i = 0; i < 60; i++) { const r = rect(n); if (r && r[1] >= bd[1] + 60 && r[1] + r[3] <= bd[1] + 520) break;
    xdo(`mousemove ${bd[0] + 500} ${bd[1] + 300} click ${!r || r[1] + r[3] > bd[1] + 520 ? 5 : 4}`); await wait(250) } await wait(300) };
  const pick = async (n, i, of) => { click(rect(n)); await until(() => line('menu'), 3000); await wait(300); const m = line('menu').split(' ').map(Number), rh = (m[3] - 10) / of;
    xdo(`mousemove ${m[0] + 40} ${Math.round(m[1] + 5 + (i + .5) * rh)}`); await wait(200); xdo('click 1'); await until(() => !line('menu'), 3000); await wait(300) };
  const set = re => until(() => re.test(since()), 4000);
  natStart('mob'); await until(() => rect('body') && line('page') == 'mob', 20000); await wait(1500);
  { mark = c0(); await scrollTo('mobroam0'); click(rect('mobroam0')); await set(/POST mobile\/settings\?id=0&k=roaming&v=1/);
    ok('M1. Use mobile data when roaming: mobile/settings?id=0&k=roaming&v=1', /POST mobile\/settings\?id=0&k=roaming&v=1/.test(since()), since()) }
  { mark = c0(); await scrollTo('moblim0'); click(rect('moblim0')); xdo('key ctrl+a'); xdo('type 2000'); xdo('key Return'); await set(/k=limit&v=2000/);
    /* (28 days: the menu is not all on the screen; the select's own Down key, as Chromium's, picks the next) */
    await scrollTo('mobsd0'); click(rect('mobsd0')); await until(() => line('menu'), 3000); xdo('key Escape'); await until(() => !line('menu'), 3000); xdo('key Down'); await set(/k=startday&v=2/);
    ok('M2. Monthly limit 2000 (Enter) and the month starting on day 2: k=limit&v=2000, k=startday&v=2', /POST mobile\/settings\?id=0&k=limit&v=2000/.test(since()) && /k=startday&v=2/.test(since()), since()) }
  { mark = c0(); await scrollTo('mobmode0'); await pick('mobmode0', 2, 4); await set(/k=mode&v=4g/);
    ok('M3. Network type 4G (LTE) only: k=mode&v=4g', /POST mobile\/settings\?id=0&k=mode&v=4g/.test(since()), since()) }
  { mark = c0(); await scrollTo('mobcust0'); click(rect('mobcust0')); await until(() => rect('mobapn0'), 3000); click(rect('mobapn0')); xdo('type Inet.Example'); await scrollTo('mobsave0'); click(rect('mobsave0')); await set(/k=apnuser/);
    ok('M4. Typed in: the APN Inet.Example (its capitals kept), Save: k=apn&v=Inet.Example, then k=apnuser', /POST mobile\/settings\?id=0&k=apn&v=Inet\.Example/.test(since()) && /k=apnuser&v=/.test(since()), since()) }
  { mark = c0(); await scrollTo('mobauto0'); click(rect('mobauto0')); await set(/k=apn&v=(&|$)/m);
    ok('M5. Automatic again: k=apn&v= (empty)', /POST mobile\/settings\?id=0&k=apn&v=( |$)/m.test(since()), since()) }
  { mark = c0(); await scrollTo('mobpin1'); click(rect('mobpin1')); xdo('type 123'); click(rect('mobpinb1')); await until(() => /mobmsg A PIN is 4 to 8 digits\./.test(acts()), 3000);
    const short = /mobmsg A PIN is 4 to 8 digits\./.test(acts()) && !/mobile\/pin/.test(since());
    click(rect('mobpin1')); xdo('key ctrl+a'); xdo('type 1234'); click(rect('mobpinb1')); await until(() => rect('dialog'), 3000); pw(); await set(/POST mobile\/pin\?id=1/); await wait(500);
    let b = {}; try { b = JSON.parse(fs.readFileSync(path.join(D, 'mobpin'), 'utf8')) } catch (e) {}
    ok('M6. The SIM\'s PIN: 123 refused ("A PIN is 4 to 8 digits."), 1234 with the administrator\'s name and password: mobile/pin?id=1 {n, p, pin}', short && /POST mobile\/pin\?id=1/.test(since()) && b.n == 'Ann' && b.p == PW && b.pin == '1234', [short, since(), Object.keys(b)]) }
  { mark = c0(); await scrollTo('mobcb0'); click(rect('mobcb0')); await set(/POST mobile\/connect\?id=0&on=0/);
    ok('M7. Disconnect: mobile/connect?id=0&on=0', /POST mobile\/connect\?id=0&on=0/.test(since()), since()) }
  natStop(); try { fs.unlinkSync(path.join(D, 'mobpin')) } catch (e) {} try { fs.unlinkSync(path.join(D, 'mobile')) } catch (e) {}
}
// ---- Network Settings (settings25.js, wifi26.js, settings31.js, mobile58.js, hotspot59.js, vpn62.js; the stand-in keeps each body in DIR/body-NAME)
async function checkNet() {
  ST.state(D, MODES.light, MORE); const B = n => { try { return fs.readFileSync(path.join(D, 'body-' + n), 'utf8') } catch (e) { return null } };
  for (const f of fs.readdirSync(D)) if (/^body-|^wifijob$/.test(f)) fs.unlinkSync(path.join(D, f));
  const c0 = () => calls().length; let mark = c0(); const since = () => calls().slice(mark); const acts = () => lines('act').join('\n'); const dbg = () => lines('act').slice(-12);
  const LS = () => JSON.parse(fs.readFileSync(path.join(D, 'ls.json'))).data;
  const pw = () => { const d = rect('dialog'); if (d) xdo(`mousemove ${d[0] + 5} ${d[1] + 5}`); xdo(`type --delay 30 '${PW.replace(/'/g, "'\\''")}'`); xdo('key Return') };
  const scrollTo = async n => { const bd = rect('body'); for (let i = 0; i < 90; i++) { const r = rect(n); if (r && r[1] >= bd[1] + 60 && r[1] + r[3] <= bd[1] + 500) break;
    xdo(`mousemove ${bd[0] + 500} ${bd[1] + 300} click ${!r || r[1] + r[3] > bd[1] + 500 ? 5 : 4}`); await wait(200) } await wait(300) };
  const pick = async (n, i, of) => { click(rect(n)); await until(() => line('menu'), 3000); await wait(300); const m = line('menu').split(' ').map(Number), rh = (m[3] - 10) / of;
    xdo(`mousemove ${m[0] + 40} ${Math.round(m[1] + 5 + (i + .5) * rh)}`); await wait(200); xdo('click 1'); await until(() => !line('menu'), 3000); await wait(300) };
  let FOC = []; const typeIn = async (n, t) => { await scrollTo(n); click(rect(n)); await wait(300); FOC.push(n + ':' + line('focus') + ':' + JSON.stringify(rect(n))); xdo('key ctrl+a'); xdo(`type --delay 20 '${t}'`); await wait(200) };
  natStart('net'); await until(() => rect('body') && line('page') == 'net', 20000); await wait(2000);
  { mark = c0(); await scrollTo('wfc1'); click(rect('wfc1')); await until(() => B('wifi_connect') != null, 4000); await until(() => /POST \/api\/job|GET job/.test(since()) && !/wifijob/.test(''), 3000); await wait(1500);
    ok('N1. Cafe Guest (open, saved): Connect: wifi/connect "Cafe Guest\\n\\nsaved", then rudolfd\'s job followed', B('wifi_connect') == 'Cafe Guest\n\nsaved' && /GET job/.test(since()), [B('wifi_connect'), since().slice(-300)]) }
  { mark = c0(); await scrollTo('wfc2'); click(rect('wfc2')); await until(() => rect('wfp'), 3000); click(rect('wfp')); xdo('type abc'); FOC.push('wfgo:' + JSON.stringify(rect('wfgo')) + JSON.stringify(rect('wfp'))); click(rect('wfgo')); await until(() => /wfe A Wi-Fi password has 8 to 63 characters\./.test(acts()), 3000);
    const short = /wfe A Wi-Fi password has 8 to 63 characters\./.test(acts()); click(rect('wfp')); xdo('key ctrl+a'); xdo('type longpassword1'); xdo('key Return'); await until(() => B('wifi_connect') && B('wifi_connect').startsWith('Neighbour'), 4000); await wait(1500);
    ok('N2. Neighbour 5G (WPA2/WPA3, not saved): its password asked; "abc" refused (8 to 63 characters); then wifi/connect "Neighbour 5G\\nlongpassword1\\npsk"', short && B('wifi_connect') == 'Neighbour 5G\nlongpassword1\npsk', [[short, B('wifi_connect')], FOC]) }
  { await scrollTo('wfmt0'); click(rect('wfmt0')); await until(() => (LS().bd95metered || '').includes('Home'), 3000);
    ok('N3. Saved network Home: Metered: bd95metered ["Home"], as appSetMetered keeps it', LS().bd95metered == '["Home"]', [LS().bd95metered, dbg()]) }
  { await scrollTo('wff1'); click(rect('wff1')); await wait(400); click(rect('wff1')); await until(() => B('wifi_forget') != null, 3000);
    ok('N4. Cafe Guest: Forget, tapped twice ("Tap again to forget"): wifi/forget "Cafe Guest"', B('wifi_forget') == 'Cafe Guest' && /forget sure Cafe Guest/.test(acts()), [[B('wifi_forget')], dbg()]) }
  { mark = c0(); await typeIn('nad', '192.168.1.50/24'); await typeIn('ngw', '192.168.1.1'); await typeIn('ndn', '1.1.1.1'); await scrollTo('nst'); click(rect('nst')); await until(() => /net\/static/.test(since()), 3000);
    ok('N5. Manual address 192.168.1.50/24, gateway, DNS, Apply: net/static?if=wlan0&addr=192.168.1.50%2F24&gw=192.168.1.1&dns=1.1.1.1', /POST net\/static\?if=wlan0&addr=192\.168\.1\.50%2F24&gw=192\.168\.1\.1&dns=1\.1\.1\.1/.test(since()), [since(), dbg()]) }
  { mark = c0(); await typeIn('npx', 'proxy.example:3128'); click(rect('npa')); await until(() => /POST proxy/.test(since()), 3000);
    ok('N6. Proxy proxy.example:3128, Save proxy: proxy?server=proxy.example%3A3128', /POST proxy\?server=proxy\.example%3A3128/.test(since()), [since(), FOC, rect('body')]) }
  { await scrollTo('prov'); await pick('prov', 1, 3); await scrollTo('n'); await pick('n', 2, 3); click(rect('apply')); await until(() => rect('dialog'), 3000); xdo('key Return'); await until(() => /"prov":"ddg"/.test(LS().bd95net || ''), 3000);
    const nc = JSON.parse(LS().bd95net || '{}'), log = (JSON.parse(LS().bd95seclog || '{}').sys || []).map(e => e.m);
    ok('N7. Web search DuckDuckGo, 20 results, Apply (the footer): bd95net.search {prov ddg, n 20}, the rest as it was; "Ann changed network settings" in the security log', nc.search && nc.search.prov == 'ddg' && nc.search.n === 20 && nc.dl && nc.dl.max === 4096 && Array.isArray(nc.conns) && log.includes('Ann changed network settings'), [[nc, log.slice(-2)], dbg()]) }
  { mark = c0(); await scrollTo('mtrsel'); await pick('mtrsel', 1, 3); await until(() => /net\/metered\?/.test(since()), 3000);
    ok('N8. This network: Metered: net/metered?if=wlan0&v=on', /POST net\/metered\?if=wlan0&v=on/.test(since()), [since(), dbg()]) }
  { mark = c0(); await scrollTo('hst'); click(rect('hst')); await until(() => rect('dialog'), 3000); pw(); await until(() => B('net_hotspot') != null, 4000);
    ok('N9. Share this connection, Turn on, with the administrator\'s permission: net/hotspot "Rudolf-Home\\n\\n2.4" (the saved password kept)', B('net_hotspot') == 'Rudolf-Home\n\n2.4', B('net_hotspot')) }
  { mark = c0(); await scrollTo('vpnb0'); click(rect('vpnb0')); await until(() => /vpn\/on/.test(since()), 3000);
    await scrollTo('vpnk0'); const k = rect('vpnk0'); xdo(`mousemove ${k[0] + 10} ${k[1] + 9} click 1`); await until(() => rect('dialog'), 3000); pw(); await until(() => B('vpn_set') != null, 4000); let b = {}; try { b = JSON.parse(B('vpn_set')) || {} } catch (e) {}
    ok('N10. VPN Home router: Connect (anyone may): vpn/on?id=v1; its kill switch with the administrator\'s name and password: vpn/set {n, p, id v1, kill true}', /POST vpn\/on\?id=v1/.test(since()) && b.n == 'Ann' && b.p == PW && b.id == 'v1' && b.kill === true, [since(), Object.keys(b), JSON.stringify(rect('vpnk0')), dbg()]) }
  natStop(); for (const f of fs.readdirSync(D)) if (/^body-|^wifijob$/.test(f)) fs.unlinkSync(path.join(D, f));
}
// ---- Oracle (signin26.js signinPanel, settings31.js, orasleep63.js)
async function checkWeb() {
  ST.state(D, MODES.light, MORE); const c0 = () => calls().length; let mark = c0(); const since = () => calls().slice(mark); const acts = () => lines('act').join('\n');
  const LS = () => JSON.parse(fs.readFileSync(path.join(D, 'ls.json'))).data, SET = () => JSON.parse(LS()['bd95set:uAnnA1b2'] || '{}');
  const scrollTo = async n => { const bd = rect('body'); for (let i = 0; i < 60; i++) { const r = rect(n); if (r && r[1] >= bd[1] + 60 && r[1] + r[3] <= bd[1] + 480) break;
    xdo(`mousemove ${bd[0] + 500} ${bd[1] + 300} click ${!r || r[1] + r[3] > bd[1] + 480 ? 5 : 4}`); await wait(200) } await wait(300) };
  const pick = async (n, i, of) => { click(rect(n)); await until(() => line('menu'), 3000); await wait(300); const m = line('menu').split(' ').map(Number), rh = (m[3] - 10) / of;
    xdo(`mousemove ${m[0] + 40} ${Math.round(m[1] + 5 + (i + .5) * rh)}`); await wait(200); xdo('click 1'); await until(() => !line('menu'), 3000); await wait(300) };
  const typeIn = async (n, t) => { await scrollTo(n); click(rect(n)); await wait(200); xdo('key ctrl+a'); xdo(`type --delay 20 '${t}'`); await wait(200) };
  natStart('web'); await until(() => rect('body') && line('page') == 'web', 20000); await wait(1500);
  { mark = c0(); click(rect('sishell')); await until(() => /POST shell\?on=1/.test(since()), 3000); await wait(300);
    ok('O1. Open websites as real browser tabs (unticked, as the page shows it while the shell has not said): ticked, shell?on=1', /POST shell\?on=1/.test(since()), since()) }
  { click(rect('siask')); await until(() => SET().siAsk === false, 3000);
    ok('O2. When a page asks for a password: Ask first off, kept at once (siAsk false)', SET().siAsk === false, SET().siAsk) }
  { await typeIn('sin', 'not a site'); xdo('key Return'); await until(() => /sim bad/.test(acts()), 3000); const bad = /sim bad/.test(acts());
    await typeIn('sin', 'https://Mail.Example.co.uk/login'); xdo('key Return'); await until(() => (SET().webSites || []).length, 3000);
    ok('O3. Your sign-in sites: "not a site" refused; https://Mail.Example.co.uk/login added as example.co.uk (siDomain)', bad && JSON.stringify(SET().webSites) == '["example.co.uk"]', [bad, SET().webSites]) }
  { await until(() => rect('sirm0'), 3000); click(rect('sirm0')); await until(() => (SET().webSites || ['x']).length == 0, 3000);
    ok('O4. ...its Remove: webSites []', JSON.stringify(SET().webSites) == '[]', SET().webSites) }
  { mark = c0(); await scrollTo('oprov'); await pick('oprov', 2, 3); await typeIn('ourl', 'http://search.example/?q={q}'); await scrollTo('apply'); click(rect('apply')); await until(() => rect('dialog'), 3000); xdo('key Return');
    await until(() => /nsm The custom search address/.test(acts()), 3000); const bad = /nsm The custom search address must start with https:\/\/ and contain \{q\}\./.test(acts());
    await typeIn('ourl', 'https://search.example/?q={q}'); click(rect('apply')); await until(() => rect('dialog'), 3000); xdo('key Return'); await until(() => /"prov":"custom"/.test(LS().bd95net || ''), 3000);
    const nc = JSON.parse(LS().bd95net || '{}');
    ok('O5. Web search: Custom with http:// refused at Apply; https://search.example/?q={q} applied: bd95net.search {prov custom, url}', bad && nc.search && nc.search.prov == 'custom' && nc.search.url == 'https://search.example/?q={q}', [bad, nc.search]) }
  { await scrollTo('ofreesel'); await pick('ofreesel', 2, 5); await until(() => SET().oraFree === 60, 3000); const log = (JSON.parse(LS().bd95seclog || '{}').sys || []).map(e => e.m);
    ok('O6. Memory: Free tabs after 1 hour, kept at once (oraFree 60), "Ann set Oracle to free tabs not looked at for 1 hour" in the security log', SET().oraFree === 60 && log.includes('Ann set Oracle to free tabs not looked at for 1 hour'), [SET().oraFree, log.slice(-2)]) }
  natStop();
}
// ---- Notifications (settings25.js notifPanel)
async function checkNotif() {
  ST.state(D, MODES.light, MORE); const LS = () => JSON.parse(fs.readFileSync(path.join(D, 'ls.json'))).data, SET = () => JSON.parse(LS()['bd95set:uAnnA1b2'] || '{}');
  const pick = async (n, i, of) => { click(rect(n)); await until(() => line('menu'), 3000); await wait(300); const m = line('menu').split(' ').map(Number), rh = (m[3] - 10) / of;
    xdo(`mousemove ${m[0] + 40} ${Math.round(m[1] + 5 + (i + .5) * rh)}`); await wait(200); xdo('click 1'); await until(() => !line('menu'), 3000); await wait(300) };
  natStart('notif'); await until(() => rect('body') && line('page') == 'notif', 20000); await wait(1200);
  click(rect('ndn')); click(rect('nsd')); await until(() => SET().dnd === true && SET().nsnd === false, 3000); await pick('ntm', 3, 4); await until(() => SET().ntime === 12, 3000);
  ok('T1. Do not disturb on, Sounds off, pop-ups for 12 seconds: kept at once (dnd true, nsnd false, ntime 12), as persistS keeps them', SET().dnd === true && SET().nsnd === false && SET().ntime === 12, [SET().dnd, SET().nsnd, SET().ntime]);
  { click(rect('nt')); await until(() => /notify This is a test \| Notifications look like this\./.test(lines('act').join('\n')), 3000);
    ok('T2. Send a test notification: "This is a test", "Notifications look like this." (the desktop\'s notifications, as every native app\'s)', /notify This is a test \| Notifications look like this\./.test(lines('act').join('\n')), lines('act').slice(-3)) }
  { click(rect('nc')); await until(() => LS()['bd95notif:uAnnA1b2'] == '[]', 3000);
    ok('T3. Clear history: bd95notif:ACCT [] (svj)', LS()['bd95notif:uAnnA1b2'] == '[]', LS()['bd95notif:uAnnA1b2']) }
  natStop();
}
// ---- the memory at idle with a page open (System: Display is not native yet), against the web one's
const rss = pid => { const t = fs.readFileSync(`/proc/${pid}/status`, 'utf8'); const g = k => +((t.match(new RegExp('^' + k + ':\\s+(\\d+)', 'm')) || [])[1] || 0) / 1024; let pss = null;
  try { pss = +((fs.readFileSync(`/proc/${pid}/smaps_rollup`, 'utf8').match(/^Pss:\s+(\d+)/m) || [])[1]) / 1024 } catch (e) {} return {anon: g('RssAnon'), pss} };
async function memory() {
  ST.state(D, MODES.light, MORE);
  natStart('sys'); await until(() => rect('body') && line('page') == 'sys', 20000); await wait(12000); const n = rss(NP.pid); natStop();
  natStart('disp'); await until(() => rect('body') && line('page') == 'disp', 20000); await wait(12000); const nd = rss(NP.pid); natStop();
  const br = await chromium.launch({args: ['--font-render-hinting=none']}); const ctx = await br.newContext({viewport: {width: 860, height: 640}}); const p = await ctx.newPage();
  await p.goto(BASE + '/#app=settings&page=sys'); await wait(25000);
  const pids = cp.execSync(`pgrep -f -- '--type=renderer' || true`).toString().trim().split('\n').filter(Boolean).map(Number);
  let w = null; for (const id of pids) { try { const m = rss(id); if (!w || m.anon > w.anon) w = Object.assign(m, {pid: id}) } catch (e) {} }
  await br.close();
  const f = x => x == null ? 'n/a' : x.toFixed(1); const res = `native ${f(n.anon)} MB RssAnon, ${f(n.pss)} MB PSS (with Display open: ${f(nd.anon)} MB RssAnon, ${f(nd.pss)} MB PSS); web (Chromium's renderer, #app=settings, System) ${w ? f(w.anon) : 'n/a'} MB RssAnon, ${w ? f(w.pss) : 'n/a'} MB PSS`;
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
    { const {ctx, p} = await signin(mode); for (const k of pages) { if (!N[k + '-mid']) continue;
        await p.evaluate(k => { document.querySelectorAll('.ntoast,#ntoasts>*').forEach(e => e.remove()); if (window.SW) { SW._ok = 1; SW.close() } settings(k); SW.el.style.left = '120px'; SW.el.style.top = '60px' }, k);
        for (let i = 0; i < 6; i++) { await p.clock.runFor(500); await wait(200) } await p.mouse.move(5, 5);
        for (const at of ['mid'].concat(MIDS[k] || [])) { if (!N[k + '-' + at]) continue;
          await p.evaluate(y => { SW.body.querySelector('.sc').scrollTop = y }, N[k + '-' + at].y); await wait(400);
          const b = await p.evaluate(() => { const r = SW.body.getBoundingClientRect(); return [Math.round(r.left), Math.round(r.top), Math.round(r.width), Math.round(r.height)] });
          const f = path.join(OUT, `${mode}-${k}-${at}.page.png`); await p.screenshot({path: f}); P[k + '-' + at] = {f, b} } } await ctx.close() }
    for (const k of pages) for (const at of ['top', 'mid'].concat(MIDS[k] || [], ['end'])) { const key = k + '-' + at;
      if (!P[key] || !N[key] || !N[key].b) { ok(`${mode} ${key}: drawn by both`, false); continue }
      const d = px(P[key].f, N[key].f, P[key].b, N[key].b, path.join(OUT, `${mode}-${key}`)); rows.push([mode, key, d.pct]);
      ok(`${mode} ${k} (${at}): ${d.pct}% of the window's pixels differ (page ${P[key].b.slice(2).join('x')}, native ${N[key].b.slice(2).join('x')})`, d.pct < LIMIT, {box: d.box, map: d.map}) }
    if (mode == 'light') for (const k of pages) { const a = JSON.stringify(P[k + '-harv']), b = JSON.stringify(N[k + '-harv']);
      ok(`${k}: Search in Settings finds the same rows on the native page as on the web one (${P[k + '-harv'].length})`, a == b, {page: P[k + '-harv'], native: N[k + '-harv']}) }
  }
  if (only('checklist') || only('search')) { const P = await pageSearch(); await checklist(P) }
  if (only('checklist') || only('upd-checklist')) await checkUpd();
  if (only('checklist') || only('km-checklist')) await checkKbdMouse();
  if (only('checklist') || only('vam-checklist')) await checkVolAccMm();
  if (only('checklist') || only('disp-checklist')) await checkDisp();
  if (only('checklist') || only('bt-checklist')) await checkBt();
  if (only('checklist') || only('remote-checklist')) await checkRemote();
  if (only('checklist') || only('mob-checklist')) await checkMob();
  if (only('checklist') || only('net-checklist')) await checkNet();
  if (only('checklist') || only('web-checklist')) await checkWeb();
  if (only('checklist') || only('notif-checklist')) await checkNotif();
  if (only('memory')) await memory();
  await BR.close();
  fs.writeFileSync(path.join(OUT, 'summary.tsv'), rows.map(r => r.join('\t')).join('\n') + '\n');
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) && R.length ? 0 : 1);
})().catch(e => { console.error(e); natStop(); process.exit(1) });
