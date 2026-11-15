// 1.6.0 (plan Part 12, item 5, "The rule": Native works as Standard): the window keys used the same way on both engines and each
// step's outcome compared: the window in front, the windows open (minimised ones marked), the maximised one, the start menu.
// Steps: Alt+Tab (the window before), Alt+Tab+Tab (back again), Alt+Tab then Escape (nothing), Super+Up (maximise), Super+Down twice
// (restore, minimise), the account's own key for "Show the desktop" (Ctrl+Alt+K instead of Super+D, set in Settings › Keyboard,
// bd95kb:ID), Super+D (no longer that), Super+Tab and a click on a window (it comes back to the front), Super+Tab then Escape, Alt+F4
// (the window in front closed), Super+E (its key set to "": still the default, as the page's kbOf), Super+I twice (one Settings), Super alone (the start menu) and again,
// Ctrl+Esc, F1 (Help).
// Run by same_t.sh with SAME_SCRIPT=keybeh_t.js.   node keybeh_t.js RUDOLF-NATIVE DIR OUT PORT DISPLAY
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path'), cp = require('child_process');
const [BIN, D, OUT, PORT, DSP] = process.argv.slice(2);
const W = 1920, H = 1080, BASE = 'http://127.0.0.1:' + PORT;
const KEYS = JSON.parse(fs.readFileSync(path.join(D, 'keys.json'))), PW = KEYS.Ann.pw;
const ST = require('./same_state.js'), ACCT = ST.ACCT;
const wait = ms => new Promise(r => setTimeout(r, ms));
const xdo = a => cp.execSync('xdotool ' + a, {env: Object.assign({}, process.env, {DISPLAY: DSP, LC_ALL: 'C.UTF-8'})}).toString();
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 700) : '')) };
fs.mkdirSync(OUT, {recursive: true});
const MORE = {['bd95pins:' + ACCT]: ['explorer', 'notepad', 'calc', 'settings'], ['bd95kb:' + ACCT]: {desktop: 'Ctrl+Alt+K', files: ''}}, OUTS = {std: {}, nat: {}};
const STEPS = ['start', 'calc', 'alttab', 'alttab2', 'altesc', 'max', 'restore', 'min', 'ownkey', 'superd', 'ovpick', 'ovesc', 'close', 'supere', 'settings', 'settings2', 'super', 'super2', 'ctrlesc', 'f1'];
async function run(e) {
  const O = {};
  for (const s of STEPS) {
    if (s == 'calc') await e.pin('calc');
    else if (s == 'alttab') await e.altTab(1, 0);
    else if (s == 'alttab2') await e.altTab(2, 0);
    else if (s == 'altesc') await e.altTab(1, 1);
    else if (s == 'max') await e.keys('super+Up');
    else if (s == 'restore' || s == 'min') await e.keys('super+Down');
    else if (s == 'ownkey') await e.keys('ctrl+alt+k');
    else if (s == 'superd') { await e.pin('calc'); await e.keys('super+d') }
    else if (s == 'ovpick') await e.overview('notepad');
    else if (s == 'ovesc') await e.overview(null);
    else if (s == 'close') await e.keys('alt+F4');
    else if (s == 'supere') await e.keys('super+e');
    else if (s == 'settings' || s == 'settings2') await e.keys('super+i', 3000);
    else if (s == 'super' || s == 'super2') await e.keys('super');
    else if (s == 'ctrlesc') { await e.keys('ctrl+Escape'); O['ctrlesc:open'] = await e.report(); await e.keys('ctrl+Escape') }
    else if (s == 'f1') await e.keys('F1', 3000);
    await wait(1200); O[s] = await e.report();
  }
  return O;
}
const pwKey = k => k.split('+').map(x => ({super: 'Meta', ctrl: 'Control', alt: 'Alt', shift: 'Shift', Up: 'ArrowUp', Down: 'ArrowDown', d: 'd', e: 'e', i: 'i', k: 'k'}[x] || x)).join('+');
async function standard() {
  ST.state(D, {}, MORE);
  const br = await chromium.launch(); const ctx = await br.newContext({viewport: {width: W, height: H}, timezoneId: 'UTC'});
  await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => r.abort());
  const p = await ctx.newPage(); await p.goto(BASE + '/');
  await p.waitForFunction(() => typeof userLogin == 'function' && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
  await wait(1000); await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
  await p.waitForFunction(() => USER && getComputedStyle(document.getElementById('login')).display == 'none', null, {timeout: 60000}); await wait(3000);
  await p.evaluate(() => launch('notepad')); await wait(1500);
  const e = {
    pin: k => p.evaluate(k => { const b = [...document.querySelectorAll('#pins > *')].find(x => x.title == APPS[k].name); b.click() }, k),
    keys: async (k, ms) => { await p.keyboard.press(pwKey(k)); await wait(ms || 0) },
    altTab: async (n, esc) => { await p.keyboard.down('Alt'); for (let i = 0; i < n; i++) { await p.keyboard.press('Tab'); await wait(150) } if (esc) await p.keyboard.press('Escape'); await wait(300); await p.keyboard.up('Alt') },
    overview: async k => { await p.keyboard.press('Meta+Tab'); await wait(800);
      if (!k) { await p.keyboard.press('Escape'); return }
      const q = await p.evaluate(k => { const W = wins.find(o => o.app == k); const r = W.el.getBoundingClientRect(); return [r.left + r.width / 2, r.top + r.height / 2] }, k); await p.mouse.click(q[0], q[1]) },
    report: () => p.evaluate(() => { const top = MRU.find(o => wins.includes(o) && !o.min && o.ws == WS);
      return ['front ' + (top ? top.app : '-'), 'open ' + wins.map(o => (o.min ? '-' : '') + o.app).sort().join(','), 'max ' + wins.filter(o => o.el.classList.contains('max')).map(o => o.app).join(','),
        'menu ' + ($('#menu').style.display == 'flex' ? 1 : 0)].join(' · ') })
  };
  OUTS.std = await run(e); await br.close();
}
let NP = null;
function nlines(k) { try { return fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } }
const nst = k => { const l = nlines(k)[0]; return l ? l.slice(k.length + 1) : null };
const nhit = k => { const l = nlines('hit').find(x => x.startsWith('hit ' + k + ' ')); if (!l) return null; const a = l.split(' ').slice(3).map(Number); return [a[0] + a[2] / 2, a[1] + a[3] / 2] };
async function until(f, ms) { const t = Date.now(); while (Date.now() - t < (ms || 20000)) { if (await f()) return true; await wait(200) } return false }
async function native() {
  ST.state(D, {}, MORE); try { fs.unlinkSync(path.join(D, 'session')) } catch (e) {}
  const env = Object.assign({}, process.env, {DISPLAY: DSP, HOME: D, RUDOLF_NATIVE_STATE: path.join(D, 'state'), LIBGL_ALWAYS_SOFTWARE: '1', TZ: 'UTC'});
  const lg = fs.openSync(path.join(OUT, 'native.log'), 'a'); NP = cp.spawn(BIN, ['--size=' + W + 'x' + H, '--draw=soft', '--base=' + BASE], {env, stdio: ['ignore', lg, lg], cwd: D});
  await until(() => nst('signin') == '1', 40000); await wait(2000); xdo("type --delay 40 '" + PW.replace(/'/g, "'\\''") + "'"); xdo('key Return');
  await until(() => nst('user') == 'Ann' && nst('signin') == '0', 30000); await wait(2500);
  const clients = () => nlines('client').filter(l => !/ app=- /.test(l) && !/ app=do-/.test(l)).map(l => { const a = l.split(' ');
    return {w: a[1], app: (l.match(/ app=(\S+)/) || [])[1], min: +(l.match(/min=(\d)/) || [])[1], max: +(l.match(/ max=(\d)/) || [])[1], focus: +(l.match(/ focus=(\d)/) || [])[1]} });
  const click = (q, b) => { if (q) xdo('mousemove ' + q.map(Math.round).join(' ') + ' click ' + (b || 1)) };
  const e = {
    pin: async k => { const n0 = clients().length; click(nhit('pin ' + k)); await until(() => clients().length > n0, 20000); await wait(2500) },
    keys: async (k, ms) => { xdo('key ' + k); await wait(ms || 0) },
    altTab: async (n, esc) => { xdo('keydown alt'); for (let i = 0; i < n; i++) { xdo('key Tab'); await wait(150) } if (esc) xdo('key Escape'); await wait(300); xdo('keyup alt') },
    overview: async k => { xdo('key super+Tab'); await until(() => +nst('overview') > 0, 5000); await wait(500);
      if (!k) { xdo('key Escape'); return }
      const c = clients().find(x => x.app == k), l = c && nlines('ovwin').find(x => x.split(' ')[1] == c.w); if (l) { const a = l.split(' ').slice(2).map(Number); click([a[0] + a[2] / 2, a[1] + a[3] / 2]) } },
    report: async () => { const C = clients(), f = C.find(c => c.focus && !c.min);
      return ['front ' + (f ? f.app : '-'), 'open ' + C.map(c => (c.min ? '-' : '') + c.app).sort().join(','), 'max ' + C.filter(c => c.max).map(c => c.app).join(','), 'menu ' + (nst('menu') == '1' ? 1 : 0)].join(' · ') }
  };
  { click(nhit('pin notepad')); await until(() => clients().length > 0, 30000); await wait(2500) }
  OUTS.nat = await run(e); NP.kill();
}
(async () => {
  try { await standard() } catch (e) { ok('Standard did the steps', 0, e.stack) }
  try { await native() } catch (e) { ok('Native did the steps', 0, e.stack); if (NP) NP.kill() }
  fs.writeFileSync(path.join(OUT, 'keybeh.json'), JSON.stringify(OUTS, null, 1));
  let i = 1; for (const s of STEPS.concat(['ctrlesc:open'])) ok((i++) + '. ' + s + ': ' + OUTS.std[s], OUTS.std[s] !== undefined && OUTS.std[s] == OUTS.nat[s], {std: OUTS.std[s], nat: OUTS.nat[s]});
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
