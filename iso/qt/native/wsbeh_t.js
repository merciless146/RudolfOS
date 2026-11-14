// 1.6.0 (plan Part 12, item 4, "The rule": Native works as Standard): the workspaces used the same way on both engines and
// each step's outcome compared: the workspace shown, the windows shown, the taskbar's windows, the workspaces with windows
// (#wsw .has), the window menu's "Move to workspace" rows. Steps: a window in workspace 1, workspace 2's button, a window
// opened there, Ctrl+Alt+Left, a window moved to workspace 3 by its menu, Ctrl+Alt+Right, Ctrl+Alt+Shift+Right (the
// window with it), workspace 1's button, the pin of an app open in another workspace (a new window here, as the page's).
// Run by same_t.sh with SAME_SCRIPT=wsbeh_t.js.   node wsbeh_t.js RUDOLF-NATIVE DIR OUT PORT DISPLAY
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
const STEPS = ['start', 'go2', 'open', 'left', 'menu', 'move3', 'right', 'shift', 'go1', 'pin'];
async function run(e) {
  const O = {};
  for (const s of STEPS) {
    if (s == 'go2') await e.wsClick(2);
    else if (s == 'open') await e.pin('calc');
    else if (s == 'left') await e.keys('ctrl+alt+Left');
    else if (s == 'menu') O['menu:rows'] = await e.wsRows('notepad');
    else if (s == 'move3') await e.menuRow('notepad', 'Move to workspace 3');
    else if (s == 'right') await e.keys('ctrl+alt+Right');
    else if (s == 'shift') await e.keys('ctrl+alt+shift+Right');
    else if (s == 'go1') await e.wsClick(1);
    else if (s == 'pin') await e.pin('notepad');
    await wait(1500); O[s] = await e.report();
  }
  return O;
}
async function standard() {
  ST.state(D, {}, {['bd95pins:' + ACCT]: PINS});
  const br = await chromium.launch(); const ctx = await br.newContext({viewport: {width: W, height: H}, timezoneId: 'UTC'});
  await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => r.abort());
  const p = await ctx.newPage(); await p.goto(BASE + '/');
  await p.waitForFunction(() => typeof userLogin == 'function' && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
  await wait(1000); await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
  await p.waitForFunction(() => USER && getComputedStyle(document.getElementById('login')).display == 'none', null, {timeout: 60000}); await wait(3000);
  await p.evaluate(() => launch('notepad')); await wait(1500);
  const click = async (sel, b) => { const q = await p.evaluate(s => { const e = typeof s == 'string' ? document.querySelector(s) : null; if (!e) return null; const r = e.getBoundingClientRect(); return [r.left + r.width / 2, r.top + r.height / 2] }, sel); if (q) await p.mouse.click(q[0], q[1], {button: b || 'left'}); await wait(400) };
  const winBtn = k => p.evaluate(k => { const e = [...document.querySelectorAll('#tasks button')].find(b => { const W = winOf(b); return W && W.app == k && b.style.display != 'none' }); if (!e) return null; const r = e.getBoundingClientRect(); return [r.left + r.width / 2, r.top + r.height / 2] }, k);
  const e = {
    wsClick: n => click('#wsw button:nth-child(' + n + ')'),
    pin: k => p.evaluate(k => { const b = [...document.querySelectorAll('#pins > *')].find(x => x.title == APPS[k].name); b.click() }, k),
    keys: async k => { await p.mouse.click(1500, 500); await p.evaluate(() => { const W = MRU.find(o => o.ws == WS && !o.min); W && W.focus() }); await p.keyboard.press(k.replace('ctrl', 'Control').replace('alt', 'Alt').replace('shift', 'Shift').replace('Left', 'ArrowLeft').replace('Right', 'ArrowRight')) },
    wsRows: async k => { const q = await winBtn(k); await p.mouse.click(q[0], q[1], {button: 'right'}); await wait(400); const r = await p.evaluate(() => [...document.querySelectorAll('.ctx .cmi span')].map(x => x.textContent).filter(x => /^Move to workspace/.test(x)).join('|')); await p.keyboard.press('Escape'); return r },
    menuRow: async (k, row) => { const q = await winBtn(k); await p.mouse.click(q[0], q[1], {button: 'right'}); await wait(400);
      const it = await p.evaluate(l => { const e = [...document.querySelectorAll('.ctx .cmi')].find(x => x.querySelector('span').textContent == l); if (!e) return null; const r = e.getBoundingClientRect(); return [r.left + 40, r.top + r.height / 2] }, row);
      if (it) await p.mouse.click(it[0], it[1]); else await p.keyboard.press('Escape') },
    report: () => p.evaluate(() => ['ws ' + WS, 'shown ' + wins.filter(o => o.el.style.display != 'none' && !o.min).map(o => o.app).sort().join(','),
      'bar ' + wins.filter(o => o.tb && o.tb.style.display != 'none').map(o => o.app).sort().join(','), 'has ' + [1, 2, 3].filter(i => wins.some(o => o.ws == i)).join(',')].join(' · '))
  };
  OUTS.std = await run(e); await br.close();
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
  const clients = () => nlines('client').filter(l => !/ app=- /.test(l) && !/ app=do-/.test(l)).map(l => { const a = l.split(' '); const ws = (nlines('clientws').find(x => x.split(' ')[1] == a[1]) || '').split(' ');
    return {w: a[1], app: (l.match(/ app=(\S+)/) || [])[1], min: +(l.match(/min=(\d)/) || [])[1], ws: +ws[2], off: +ws[3]} });
  const click = (q, b) => { if (q) xdo('mousemove ' + q.map(Math.round).join(' ') + ' click ' + (b || 1)) };
  const nApps = () => clients().length;
  const e = {
    wsClick: async n => { click(nhit('ws ' + n)); await wait(800) },
    pin: async k => { const n0 = nApps(); click(nhit('pin ' + k)); await until(() => nApps() > n0, 20000); await wait(2500) },
    keys: async k => { xdo('mousemove 1500 500'); xdo('key ' + k); await wait(800) },
    winQ: k => { const c = clients().find(x => x.app == k && !x.off); return c ? nhit('win ' + c.w) : null },
    wsRows: async k => { click(e.winQ(k), 3); await until(() => nlines('cmitem').length, 8000); await wait(200); const r = nlines('cmitem').map(l => l.slice(l.lastIndexOf('|') + 1)).filter(x => /^Move to workspace/.test(x)).join('|'); xdo('key Escape'); await wait(400); return r },
    menuRow: async (k, row) => { click(e.winQ(k), 3); await until(() => nlines('cmitem').length, 8000); await wait(200); const l = nlines('cmitem').find(x => x.endsWith('|' + row)); if (l) { const b = l.split(' '); xdo('mousemove ' + b[1] + ' ' + b[2] + ' click 1') } else xdo('key Escape') },
    report: async () => { const C = clients(), ws = +nst('ws'); const wins = nlines('hit').filter(x => x.startsWith('hit win ')).map(x => x.split(' ')[2]);
      return ['ws ' + ws, 'shown ' + C.filter(c => !c.off && !c.min).map(c => c.app).sort().join(','), 'bar ' + C.filter(c => wins.includes(c.w)).map(c => c.app).sort().join(','),
        'has ' + [1, 2, 3].filter(i => C.some(c => c.ws == i)).join(',')].join(' · ') }
  };
  { click(nhit('pin notepad')); await until(() => nApps() > 0, 30000); await wait(2500) }
  OUTS.nat = await run(e); NP.kill();
}
(async () => {
  try { await standard() } catch (e) { ok('Standard did the steps', 0, e.stack) }
  try { await native() } catch (e) { ok('Native did the steps', 0, e.stack); if (NP) NP.kill() }
  fs.writeFileSync(path.join(OUT, 'wsbeh.json'), JSON.stringify(OUTS, null, 1));
  let i = 1; for (const s of STEPS.concat(['menu:rows'])) ok((i++) + '. ' + s + ': ' + OUTS.std[s], OUTS.std[s] !== undefined && OUTS.std[s] == OUTS.nat[s], {std: OUTS.std[s], nat: OUTS.nat[s]});
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
