// 1.6.3 Part 8, item 6 ("Desktop right-click menu does not disappear if clicked outside of while open on Native. Will only close
// after an option is selected."): a right-click menu (the desktop's, the taskbar's, a window's) opened, then closed by a press
// elsewhere (the desktop, the taskbar, a window's title bar, inside a window), Esc, Tab, Alt+Tab (switching windows) or another
// right-click (which opens the next menu, at its own place), on Chromium mode's page and on Native; each step's outcome (the menu
// gone, or the one new menu) the same on both. (Alt+Tab: Native's switcher closes the menu, the owner's words for item 6; the page
// under Playwright keeps its menu over its own switcher with one window, and is only reported.)   Run by same_t.sh with SAME_SCRIPT=cmclose163_t.js:
//   SAME_SCRIPT=cmclose163_t.js SAME_PORT=8740 SAME_DSP=:87 sh same_t.sh OUT
//   (node cmclose163_t.js RUDOLF-NATIVE DIR OUT PORT DISPLAY)
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path'), cp = require('child_process');
const [BIN, D, OUT, PORT, DSP] = process.argv.slice(2);
const W = 1920, H = 1080, BASE = 'http://127.0.0.1:' + PORT;
const KEYS = JSON.parse(fs.readFileSync(path.join(D, 'keys.json'))), PW = KEYS.Ann.pw;
const ST = require('./same_state.js'), ACCT = ST.ACCT;
const wait = ms => new Promise(r => setTimeout(r, ms));
const xdo = a => cp.execSync('xdotool ' + a, {env: Object.assign({}, process.env, {DISPLAY: DSP, LC_ALL: 'C.UTF-8'})}).toString();
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 700) : '')) };
fs.mkdirSync(OUT, {recursive: true});
const PINS = ['explorer', 'notepad', 'calc', 'settings'], OUTS = {std: {}, nat: {}};
// the steps: [what, the menu (desk: the desktop's at 1200,300; bar: the taskbar's; win: Notepad's taskbar button's), how it is left
// (click:WHERE, key:KEY, right:WHERE; WHERE: desk (300,500), bar (the taskbar's empty part), title (Notepad's title bar), inside
// (inside Notepad's window))]; after each the engine says whether a menu is open and where
const STEPS = [['desk-click', 'desk', 'click:desk'], ['desk-esc', 'desk', 'key:Escape'], ['desk-bar', 'desk', 'click:bar'], ['desk-title', 'desk', 'click:title'],
  ['desk-inside', 'desk', 'click:inside'], ['desk-tab', 'desk', 'key:Tab'], ['desk-alttab', 'desk', 'key:alt+Tab'], ['desk-right', 'desk', 'right:desk'],
  ['bar-desk', 'bar', 'click:desk'], ['bar-esc', 'bar', 'key:Escape'], ['win-title', 'win', 'click:title'], ['win-desk', 'win', 'click:desk']];
const DESK = [1200, 300], ELSE = [300, 500], BAR = [1000, H - 21];
async function standard() {
  ST.state(D, {}, {['bd95pins:' + ACCT]: PINS});
  const br = await chromium.launch(); const ctx = await br.newContext({viewport: {width: W, height: H}, timezoneId: 'UTC'});
  await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => r.abort());
  const p = await ctx.newPage(); await p.goto(BASE + '/');
  await p.waitForFunction(() => typeof userLogin == 'function' && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
  await wait(1000); await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
  await p.waitForFunction(() => USER && getComputedStyle(document.getElementById('login')).display == 'none', null, {timeout: 60000}); await wait(3000);
  await p.evaluate(() => { launch('notepad') }); await wait(1500);
  await p.evaluate(() => { const W = wins.find(w => w.app == 'notepad'); W.el.style.left = '700px'; W.el.style.top = '520px' }); await wait(300);
  const win = () => p.evaluate(() => { const W = wins.find(w => w.app == 'notepad'), r = W.el.getBoundingClientRect(); return [r.left, r.top, r.width, r.height] });
  const at = async w => { if (w == 'desk') return ELSE; if (w == 'bar') return BAR; const r = await win(); return w == 'title' ? [r[0] + r[2] / 2, r[1] + 14] : [r[0] + r[2] / 2, r[1] + r[3] / 2] };
  const menu = () => p.evaluate(() => { const m = document.querySelector('.ctx'); if (!m) return 'none'; const r = m.getBoundingClientRect(); return 'open ' + Math.round(r.left) + ',' + Math.round(r.top) });
  for (const [n, m, how] of STEPS) {
    let q = m == 'desk' ? DESK : m == 'bar' ? BAR : await p.evaluate(() => { const e = [...document.querySelectorAll('#tasks button')].find(b => { const W = winOf(b); return W && W.app == 'notepad' }); const r = e.getBoundingClientRect(); return [r.left + r.width / 2, r.top + r.height / 2] });
    await p.mouse.click(q[0], q[1], {button: 'right'}); await wait(500); OUTS.std[n + ':opened'] = (await menu()) != 'none';
    const [k, a] = how.split(':');
    if (k == 'key') await p.keyboard.press(a == 'alt+Tab' ? 'Alt+Tab' : a); else { const w = await at(a); await p.mouse.click(w[0], w[1], {button: k == 'right' ? 'right' : 'left'}) }
    await wait(600); OUTS.std[n] = await menu(); if (OUTS.std[n] != 'none') { await p.keyboard.press('Escape'); await wait(300) } }
  await br.close();
}
let NP = null;
function nlines(k) { try { return fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } }
const nst = k => { const l = nlines(k)[0]; return l ? l.slice(k.length + 1) : null };
const nhit = k => { const l = nlines('hit').find(x => x.startsWith('hit ' + k + ' ')); if (!l) return null; const a = l.split(' ').slice(3).map(Number); return [a[0] + a[2] / 2, a[1] + a[3] / 2] };
async function until(f, ms) { const t = Date.now(); while (Date.now() - t < (ms || 20000)) { if (await f()) return true; await wait(200) } return false }
async function native() {
  ST.state(D, {}, {['bd95pins:' + ACCT]: PINS}); try { fs.unlinkSync(path.join(D, 'session')) } catch (e) {}
  const env = Object.assign({}, process.env, {DISPLAY: DSP, HOME: D, RUDOLF_NATIVE_STATE: path.join(D, 'state'), LIBGL_ALWAYS_SOFTWARE: '1', TZ: 'UTC'});
  const lg = fs.openSync(path.join(OUT, 'native.log'), 'a'); NP = cp.spawn(BIN, ['--size=' + W + 'x' + H, '--draw=soft', '--base=' + BASE], {env, stdio: ['ignore', lg, lg], cwd: D});
  await until(() => nst('signin') == '1', 40000); await wait(2000); xdo("type --delay 40 '" + PW.replace(/'/g, "'\\''") + "'"); xdo('key Return');
  await until(() => nst('user') == 'Ann' && nst('signin') == '0', 30000); await wait(2500);
  const ph = nhit('pin notepad'); xdo('mousemove ' + ph.map(Math.round).join(' ') + ' click 1'); await until(() => nlines('client').some(l => / app=notepad /.test(l)), 30000); await wait(2500);
  const cl = () => nlines('client').find(x => / app=notepad /.test(x)).split(' ');
  xdo("windowmove " + parseInt(cl()[1], 16) + ' 700 520'); await wait(800);
  const win = () => { const b = cl(); return [+b[3], +b[4], +b[5], +b[6]] };
  const at = w => { if (w == 'desk') return ELSE; if (w == 'bar') return BAR; const r = win(); return w == 'title' ? [r[0] + r[2] / 2, r[1] + 14] : [r[0] + r[2] / 2, r[1] + 40 + (r[3] - 40) / 2] };
  const menu = () => { const l = nlines('cmenu')[0]; if (!l) return 'none'; const a = l.split(' '); return 'open ' + a[1] + ',' + a[2] };
  const mv = q => 'mousemove ' + q.map(Math.round).join(' ');
  for (const [n, m, how] of STEPS) {
    let q = m == 'desk' ? DESK : m == 'bar' ? BAR : nhit('win ' + cl()[1]);
    xdo(mv(q) + ' click 3'); await until(() => nlines('cmenu').length, 8000); await wait(400); OUTS.nat[n + ':opened'] = menu() != 'none';
    const [k, a] = how.split(':');
    if (k == 'key') xdo('key ' + a); else xdo(mv(at(a)) + ' click ' + (k == 'right' ? 3 : 1));
    await wait(900); OUTS.nat[n] = menu(); if (OUTS.nat[n] != 'none') { xdo('key Escape'); await wait(400); if (menu() != 'none') { xdo('mousemove 1 1 click 1'); await wait(400) } } }
  NP.kill();
}
(async () => {
  try { await standard() } catch (e) { ok('Chromium mode did the steps', 0, e.stack) }
  try { await native() } catch (e) { ok('Native did the steps', 0, e.stack); if (NP) NP.kill() }
  fs.writeFileSync(path.join(OUT, 'cmclose163.json'), JSON.stringify(OUTS, null, 1));
  // (the outcome: 'none', or a menu open at a place; a new menu's place compared within a few pixels: the two engines' fonts)
  const same = (a, b) => { if (a == 'none' || b == 'none') return a == b; const x = a.split(' ')[1].split(',').map(Number), y = b.split(' ')[1].split(',').map(Number); return Math.abs(x[0] - y[0]) <= 6 && Math.abs(x[1] - y[1]) <= 6 };
  let i = 1; for (const [n, , how] of STEPS) {
    const want = how.startsWith('right:') ? 'a new menu at the press' : 'closed';
    ok((i++) + '. ' + n + ': ' + want + ' (Chromium: ' + OUTS.std[n] + ', Native: ' + OUTS.nat[n] + ')',
      OUTS.std[n + ':opened'] && OUTS.nat[n + ':opened'] && (n == 'desk-alttab' ? OUTS.nat[n] == 'none' : (how.startsWith('right:') ? OUTS.std[n] != 'none' : OUTS.std[n] == 'none') && same(OUTS.std[n], OUTS.nat[n])), {std: OUTS.std, nat: OUTS.nat}) }
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
