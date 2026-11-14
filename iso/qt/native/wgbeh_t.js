// 1.6.0 (plan Part 12, item 2, "The rule": Native works as Standard): the desktop's widgets used the same way on both
// engines, and each step's outcome compared: dragging by the header (and where it is saved), typing in the sticky note
// (keys, Backspace, Enter, Ctrl+A, Ctrl+Z; saved half a second after), the note's menu (its items, in order, greyed as
// the page's), no menu on a widget's header, the ✕, the desktop menu's Add a widget… and its picker (items, order, where
// a new widget goes), a click on the weather opening Weather. Standard: the page in Playwright's Chromium; Native:
// rudolf-native under Xvfb with the same storage (same_rd.py). Run by same_t.sh with SAME_SCRIPT=wgbeh_t.js.
//   node wgbeh_t.js RUDOLF-NATIVE DIR OUT PORT DISPLAY
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path'), cp = require('child_process');
const [BIN, D, OUT, PORT, DSP] = process.argv.slice(2);
const W = 1920, H = 1080, BASE = 'http://127.0.0.1:' + PORT;
const KEYS = JSON.parse(fs.readFileSync(path.join(D, 'keys.json'))), PW = KEYS.Ann.pw;
const ST = require('./same_state.js'), ACCT = ST.ACCT, WX = require('./same_rd_wx.json');
const wait = ms => new Promise(r => setTimeout(r, ms));
const sh = c => cp.execSync(c, {env: Object.assign({}, process.env, {DISPLAY: DSP, LC_ALL: 'C.UTF-8'})}).toString();
const xdo = a => sh('xdotool ' + a);
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 700) : '')) };
fs.mkdirSync(OUT, {recursive: true});
const START = [{t: 'clock', x: 1640, y: 30, txt: ''}, {t: 'note', x: 1370, y: 30, txt: ''}];
// the steps: each engine does them its own way and reports what it sees after each
const OUTS = {std: {}, nat: {}};
// ---- Standard
async function standard() {
  ST.state(D, {}, {['bd95widgets:' + ACCT]: START});
  const br = await chromium.launch(); const ctx = await br.newContext({viewport: {width: W, height: H}, timezoneId: 'UTC'});
  await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => /api\.open-meteo\.com\/v1\/forecast/.test(r.request().url()) ? r.fulfill({status: 200, contentType: 'application/json', body: JSON.stringify(WX)}) : r.abort());
  const p = await ctx.newPage(); await p.goto(BASE + '/');
  await p.waitForFunction(() => typeof userLogin == 'function' && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
  await wait(1000); await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
  await p.waitForFunction(() => USER && getComputedStyle(document.getElementById('login')).display == 'none', null, {timeout: 60000}); await wait(4000);
  const O = OUTS.std, list = () => p.evaluate(() => JSON.parse(localStorage.getItem('bd95widgets:' + uid()) || '[]'));
  const box = i => p.evaluate(i => { const r = document.querySelectorAll('#desk > .wgt')[i].getBoundingClientRect(); return [r.left, r.top, r.width, r.height] }, i);
  const menu = () => p.evaluate(() => { const m = document.querySelector('.ctx'); if (!m) return null; return {title: (m.querySelector('.cmh') || {}).textContent || '', items: [...m.querySelectorAll('.cmi')].map(e => (e.classList.contains('dis') ? '-' : '') + e.querySelector('span').textContent)} });
  // 1. the clock dragged by its header
  let b = await box(0); await p.mouse.move(b[0] + 60, b[1] + 12); await p.mouse.down(); for (let k = 1; k <= 5; k++) { await p.mouse.move(b[0] + 60 - 40 * k, b[1] + 12 + 20 * k); await wait(30) } await p.mouse.up(); await wait(300);
  O.drag = (await list())[0];
  // 2. the note: typed in
  b = await box(1); await p.mouse.click(b[0] + 60, b[1] + 60); await p.keyboard.type('Hello'); await wait(900); O.typed = (await list())[1].txt;
  await p.keyboard.press('Backspace'); await p.keyboard.press('Enter'); await p.keyboard.type('x'); await wait(900); O.edit = (await list())[1].txt;
  await p.keyboard.press('Control+a'); await p.keyboard.type('Z'); await wait(900); O.all = (await list())[1].txt;
  await p.keyboard.press('Control+z'); await wait(900); O.undo = (await list())[1].txt;
  // 3. the note's menu, a header's
  await p.mouse.click(b[0] + 60, b[1] + 60, {button: 'right'}); await wait(300); O.tmenu = await menu(); await p.keyboard.press('Escape'); await p.mouse.click(1000, 700); await wait(200);
  await p.mouse.click(b[0] + 60, b[1] + 12, {button: 'right'}); await wait(300); O.hmenu = await menu(); await p.mouse.click(1000, 700); await wait(200);
  // 4. the ✕ of the clock
  b = await box(0); await p.mouse.click(b[0] + b[2] - 16, b[1] + 12); await wait(400); O.removed = (await list()).map(w => w.t);
  // 5. Add a widget…: the picker, a system monitor, then the weather
  for (const [k, lbl] of [['mon', 'System monitor'], ['weather', 'Weather']]) {
    await p.mouse.click(1000, 600, {button: 'right'}); await wait(300);
    const at = await p.evaluate(() => { const e = [...document.querySelectorAll('.ctx .cmi')].find(x => x.textContent.includes('Add a widget')); const r = e.getBoundingClientRect(); return [r.left + 40, r.top + r.height / 2] });
    await p.mouse.click(at[0], at[1]); await wait(300); if (k == 'mon') O.picker = await menu();
    const it = await p.evaluate(l => { const e = [...document.querySelectorAll('.ctx .cmi')].find(x => x.querySelector('span').textContent == l); const r = e.getBoundingClientRect(); return [r.left + 40, r.top + r.height / 2] }, lbl);
    await p.mouse.click(it[0], it[1]); await wait(600) }
  O.added = (await list()).map(w => [w.t, w.x, w.y]);
  // 6. a click on the weather opens Weather
  await wait(1500); b = await box(2); await p.mouse.click(b[0] + 60, b[1] + 60); await wait(1500);
  O.weather = await p.evaluate(() => wins.some(W => W.app == 'weather'));
  await br.close();
}
// ---- Native
let NP = null;
function nlines(k) { try { return fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } }
const nst = k => { const l = nlines(k)[0]; return l ? l.slice(k.length + 1) : null };
async function until(f, ms) { const t = Date.now(); while (Date.now() - t < (ms || 20000)) { if (await f()) return true; await wait(200) } return false }
async function rdList() { const s = await (await fetch(BASE + '/api/desktop/settings', {headers: {'X-Rudolf-Token': 'tok160same'}})).json(); return s.widgets || [] }
const wbox = i => { const l = nlines('widget').find(x => x.split(' ')[1] == String(i)); if (!l) return null; const a = l.split(' '); return [+a[3], +a[4], +a[5], +a[6]] };
function nmenu() { const m = nlines('cmenu')[0]; if (!m) return null; const t = m.split(' ').slice(5).join(' '); return {title: t == '-' ? '' : t, items: nlines('cmitem').map(l => { const a = l.split(' '); return (a[3] == '1' ? '-' : '') + l.slice(l.indexOf('|') + 1) })} }
async function native() {
  try { fs.unlinkSync(path.join(D, 'widgets.json')) } catch (e) {}
  ST.state(D, {}, {['bd95widgets:' + ACCT]: START});
  const env = Object.assign({}, process.env, {DISPLAY: DSP, HOME: D, RUDOLF_NATIVE_STATE: path.join(D, 'state'), LIBGL_ALWAYS_SOFTWARE: '1', TZ: 'UTC', RUDOLF_WX_BASE: BASE, RUDOLF_WX_GEO: BASE});
  for (const k of ['http_proxy', 'https_proxy', 'HTTP_PROXY', 'HTTPS_PROXY', 'all_proxy', 'ALL_PROXY']) delete env[k];
  try { fs.unlinkSync(path.join(D, 'session')) } catch (e) {}
  const lg = fs.openSync(path.join(OUT, 'native.log'), 'a'); NP = cp.spawn(BIN, ['--size=' + W + 'x' + H, '--draw=soft', '--base=' + BASE], {env, stdio: ['ignore', lg, lg], cwd: D});
  await until(() => nst('signin') == '1', 40000); await wait(2000); xdo("type --delay 40 '" + PW.replace(/'/g, "'\\''") + "'"); xdo('key Return');
  await until(() => nst('user') == 'Ann' && nst('signin') == '0', 30000); await until(() => nlines('widget').length == 2, 20000); await wait(1500);
  const O = OUTS.nat;
  let b = wbox(0); xdo(`mousemove ${b[0] + 60} ${b[1] + 12} mousedown 1`); for (let k = 1; k <= 5; k++) { xdo(`mousemove ${b[0] + 60 - 40 * k} ${b[1] + 12 + 20 * k}`); await wait(30) } xdo('mouseup 1'); await wait(500);
  O.drag = (await rdList())[0];
  b = wbox(1); xdo(`mousemove ${b[0] + 60} ${b[1] + 60} click 1`); await wait(300); xdo('type --delay 30 Hello'); await wait(900); O.typed = (await rdList())[1].txt;
  xdo('key BackSpace Return'); xdo('type x'); await wait(900); O.edit = (await rdList())[1].txt;
  xdo('key ctrl+a'); xdo('type Z'); await wait(900); O.all = (await rdList())[1].txt;
  xdo('key ctrl+z'); await wait(900); O.undo = (await rdList())[1].txt;
  xdo(`mousemove ${b[0] + 60} ${b[1] + 60} click 3`); await until(() => nlines('cmenu').length, 5000); O.tmenu = nmenu(); xdo('key Escape'); await wait(300); xdo('mousemove 1000 700 click 1'); await wait(300);
  xdo(`mousemove ${b[0] + 60} ${b[1] + 12} click 3`); await wait(800); O.hmenu = nmenu(); xdo('mousemove 1000 700 click 1'); await wait(300);
  b = wbox(0); xdo(`mousemove ${b[0] + b[2] - 16} ${b[1] + 12} click 1`); await wait(600); O.removed = (await rdList()).map(w => w.t);
  const cmClick = lbl => { const l = nlines('cmitem').find(x => x.endsWith('|' + lbl)); if (l) { const a = l.split(' '); xdo('mousemove ' + a[1] + ' ' + a[2] + ' click 1') } return !!l };
  for (const [k, lbl] of [['mon', 'System monitor'], ['weather', 'Weather']]) {
    xdo('mousemove 1000 600 click 3'); await until(() => nlines('cmenu').length, 5000); cmClick('Add a widget…'); await until(() => (nlines('cmenu')[0] || '').endsWith('Add a widget'), 5000);
    if (k == 'mon') O.picker = nmenu(); cmClick(lbl); await wait(800) }
  O.added = (await rdList()).map(w => [w.t, w.x, w.y]);
  await until(() => /^2 /.test(nst('weather') || ''), 10000); b = wbox(2); xdo(`mousemove ${b[0] + 60} ${b[1] + 60} click 1`);
  O.weather = await until(() => nlines('app').some(l => l.startsWith('app weather ')), 30000);
  /* (Native only) the app's page takes the shell's list into the page's storage (bd95widgets:ID), as Standard keeps it */
  const want = JSON.stringify(await rdList()), lsv = () => { try { return JSON.parse(fs.readFileSync(path.join(D, 'ls.json'))).data['bd95widgets:' + ACCT] } catch (e) { return null } };
  OUTS.adopt = await until(() => lsv() == want, 15000); OUTS.adoptv = [lsv(), want];
  NP.kill();
}
(async () => {
  try { await standard() } catch (e) { ok('Standard did the steps', 0, e.stack) }
  try { await native() } catch (e) { ok('Native did the steps', 0, e.stack); if (NP) NP.kill() }
  fs.writeFileSync(path.join(OUT, 'wgbeh.json'), JSON.stringify(OUTS, null, 1));
  const s = OUTS.std, n = OUTS.nat, same = (a, b) => JSON.stringify(a) == JSON.stringify(b);
  const L = [['1. the header drags the widget; saved where it was left', 'drag', s.drag && s.drag.x == 1440 && s.drag.y == 130],
    ['2. typing in the note; saved half a second after', 'typed', s.typed == 'Hello'], ['3. ...Backspace, Enter', 'edit', s.edit == 'Hell\nx'],
    ['4. ...Ctrl+A and typing over it', 'all', s.all == 'Z'], ['5. ...Ctrl+Z', 'undo', s.undo == 'Hell\nx'],
    ['6. the note\'s menu: its items in order, greyed without a selection', 'tmenu', s.tmenu && s.tmenu.items.length == 7], ['7. no menu on a widget\'s header', 'hmenu', s.hmenu === null],
    ['8. the ✕ removes it (saved)', 'removed', same(s.removed, ['note'])], ['9. Add a widget…: the picker, its title and items in order', 'picker', s.picker && s.picker.title == 'Add a widget' && s.picker.items.length == 5],
    ['10. ...a new widget where the page puts it (saved)', 'added', s.added && s.added.length == 3], ['11. a click on the weather opens Weather', 'weather', s.weather === true]];
  for (const [t, k, sc] of L) ok(t + ' (Standard: ' + JSON.stringify(s[k]) + ')', sc && same(s[k], n[k]), {std: s[k], nat: n[k]});
  ok('12. (Native) the shell\'s list reaches the page\'s storage through an app\'s page', OUTS.adopt === true, OUTS.adoptv);
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
