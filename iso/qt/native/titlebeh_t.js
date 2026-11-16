// 1.6.0 (the owner's photo of 2026-11-15, "The rule": a window's title bar works on Native as on Standard): the same steps done
// with the pointer on a window's title bar and edges on both engines, and each step's outcome compared: the window's place and
// size (its page's part: the frame's edge and title bar not counted), maximised, minimised, which window is in use, which are open.
// Steps: Notepad opened; its title bar dragged; a double-click on it (maximised) and again (back); its bottom right corner dragged
// (resized); its left edge dragged; _ (minimised) and its taskbar button (back); □ (maximised) and again (back); a right-click on
// the title bar (the window's menu: its rows) and Always on top from it; Calculator opened, then a middle-click on its title bar
// (sent behind: Notepad in use); ✕ (closed).
// Run by same_t.sh with SAME_SCRIPT=titlebeh_t.js.   node titlebeh_t.js RUDOLF-NATIVE DIR OUT PORT DISPLAY
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
const STEPS = ['start', 'drag', 'dbl', 'dbl2', 'corner', 'left', 'min', 'back', 'max', 'max2', 'menu', 'ontop', 'two', 'middle', 'close'];
/* (each step's pointer places, relative to the window's top left corner: the title bar's middle, its buttons, its edges) */
async function run(e) {
  const O = {};
  for (const s of STEPS) {
    const g = await e.geom('notepad');
    if (s == 'drag') await e.drag([g.x + 200, g.y + 17], [g.x + 350, g.y + 117]);
    else if (s == 'dbl' || s == 'dbl2') await e.dbl([g.x + 200, g.y + 17]);
    else if (s == 'corner') await e.drag([g.x + g.fw - 3, g.y + g.fh - 3], [g.x + g.fw - 3 + 80, g.y + g.fh - 3 + 60]);
    else if (s == 'left') await e.drag([g.x + 2, g.y + 200], [g.x + 2 - 40, g.y + 200]);
    else if (s == 'min') await e.click([g.x + g.fw - 1 - 8 - 24 - 27 - 27 + 12, g.y + 1 + 7 + 11]);
    else if (s == 'back') await e.task('notepad');
    else if (s == 'max' || s == 'max2') await e.click([g.x + g.fw - 1 - 8 - 24 - 27 + 12, g.y + 1 + 7 + 11]);
    else if (s == 'menu') { O['menu:rows'] = await e.menu([g.x + 200, g.y + 17]); }
    else if (s == 'ontop') await e.menuRow([g.x + 200, g.y + 17], 'Always on top');
    else if (s == 'two') await e.pin('calc');
    else if (s == 'middle') { const c = await e.geom('calc'); await e.click([c.x + 120, c.y + 17], 2) }
    else if (s == 'close') await e.click([g.x + g.fw - 1 - 8 - 12, g.y + 1 + 7 + 11]);
    await wait(1200); O[s] = await e.report();
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
  const e = {
    geom: k => p.evaluate(k => { const w = wins.find(o => o.app == k); if (!w) return {x: 0, y: 0, fw: 0, fh: 0}; const r = w.el.getBoundingClientRect(); return {x: r.left, y: r.top, fw: r.width, fh: r.height} }, k),
    drag: async (a, b) => { await p.mouse.move(a[0], a[1]); await p.mouse.down(); for (let i = 1; i <= 10; i++) await p.mouse.move(a[0] + (b[0] - a[0]) * i / 10, a[1] + (b[1] - a[1]) * i / 10); await p.mouse.up() },
    dbl: q => p.mouse.dblclick(q[0], q[1]),
    click: (q, b) => p.mouse.click(q[0], q[1], {button: b == 2 ? 'middle' : b == 3 ? 'right' : 'left'}),
    task: k => p.evaluate(k => { const b = [...document.querySelectorAll('#tasks button')].find(b => { const W = winOf(b); return W && W.app == k }); b.click() }, k),
    pin: async k => { await p.evaluate(k => { const b = [...document.querySelectorAll('#pins > *')].find(x => x.title == APPS[k].name); b.click() }, k); await wait(1500) },
    menu: async q => { await p.mouse.click(q[0], q[1], {button: 'right'}); await wait(400); const r = await p.evaluate(() => [...document.querySelectorAll('.ctx .cmi span')].map(x => x.textContent).join('|')); await p.keyboard.press('Escape'); await wait(300); return r },
    menuRow: async (q, row) => { await p.mouse.click(q[0], q[1], {button: 'right'}); await wait(400);
      const it = await p.evaluate(l => { const e = [...document.querySelectorAll('.ctx .cmi')].find(x => x.querySelector('span').textContent == l); if (!e) return null; const r = e.getBoundingClientRect(); return [r.left + 40, r.top + r.height / 2] }, row);
      if (it) await p.mouse.click(it[0], it[1]); else await p.keyboard.press('Escape') },
    report: () => p.evaluate(() => wins.filter(o => o.el.isConnected).map(o => { const r = o.el.getBoundingClientRect(), t = o.el.querySelector('.tb').getBoundingClientRect(), b = parseFloat(getComputedStyle(o.el).borderTopWidth) || 0;
      return o.app + ' ' + (o.min ? 'min' : [Math.round(r.left), Math.round(r.top), Math.round(r.width - 2 * b), Math.round(r.height - 2 * b - t.height)].join(',')) + (o.el.classList.contains('max') ? ' max' : '') + (o.el.classList.contains('act') && !o.min ? ' act' : '') + (o.el.classList.contains('ontop') ? ' ontop' : '') }).sort().join(' · '))
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
  for (const k of ['http_proxy', 'https_proxy', 'HTTP_PROXY', 'HTTPS_PROXY', 'all_proxy', 'ALL_PROXY']) delete env[k];
  const lg = fs.openSync(path.join(OUT, 'native.log'), 'a'); NP = cp.spawn(BIN, ['--size=' + W + 'x' + H, '--draw=soft', '--base=' + BASE], {env, stdio: ['ignore', lg, lg], cwd: D});
  await until(() => nst('signin') == '1', 40000); await wait(2000); xdo("type --delay 40 '" + PW.replace(/'/g, "'\\''") + "'"); xdo('key Return');
  await until(() => nst('user') == 'Ann' && nst('signin') == '0', 30000); await wait(2500);
  const clients = () => nlines('client').filter(l => !/ app=- /.test(l) && !/ app=do-/.test(l)).map(l => { const a = l.split(' ');
    return {w: a[1], app: (l.match(/ app=(\S+)/) || [])[1], x: +a[3], y: +a[4], cw: +a[5], ch: +a[6], min: +(l.match(/min=(\d)/) || [])[1], max: +(l.match(/max=(\d)/) || [])[1], focus: +(l.match(/focus=(\d)/) || [])[1]} });
  const mv = q => xdo('mousemove ' + q.map(Math.round).join(' '));
  const e = {
    geom: async k => { const c = clients().find(x => x.app == k); return c ? {x: c.x, y: c.y, fw: c.cw + 2, fh: c.ch + 36} : {x: 0, y: 0, fw: 0, fh: 0} },
    drag: async (a, b) => { mv(a); xdo('mousedown 1'); for (let i = 1; i <= 10; i++) { mv([a[0] + (b[0] - a[0]) * i / 10, a[1] + (b[1] - a[1]) * i / 10]); await wait(30) } xdo('mouseup 1') },
    dbl: async q => { mv(q); xdo('click --repeat 2 --delay 120 1') },
    click: async (q, b) => { mv(q); xdo('click ' + (b || 1)) },
    task: async k => { const c = clients().find(x => x.app == k); const q = c && nhit('win ' + c.w); if (q) { mv(q); xdo('click 1') } },
    pin: async k => { const n0 = clients().length; mv(nhit('pin ' + k)); xdo('click 1'); await until(() => clients().length > n0, 20000); await wait(2500) },
    menu: async q => { mv(q); xdo('click 3'); await until(() => nlines('cmitem').length, 8000); await wait(200); const r = nlines('cmitem').map(l => l.slice(l.lastIndexOf('|') + 1)).join('|'); xdo('key Escape'); await wait(400); return r },
    menuRow: async (q, row) => { mv(q); xdo('click 3'); await until(() => nlines('cmitem').length, 8000); await wait(200); const l = nlines('cmitem').find(x => x.endsWith('|' + row)); if (l) { const b = l.split(' '); xdo('mousemove ' + b[1] + ' ' + b[2] + ' click 1') } else xdo('key Escape') },
    report: async () => { const top = nlines('clienttop').map(l => l.split(' ')[1]);
      return clients().map(c => c.app + ' ' + (c.min ? 'min' : [c.x, c.y, c.cw, c.ch].join(',')) + (c.max ? ' max' : '') + (c.focus && !c.min ? ' act' : '') + (top.includes(c.w) ? ' ontop' : '')).sort().join(' · ') }
  };
  { mv(nhit('pin notepad')); xdo('click 1'); await until(() => clients().length > 0, 30000); await wait(2500) }
  OUTS.nat = await run(e); NP.kill();
}
(async () => {
  try { await standard() } catch (e) { ok('Standard did the steps', 0, e.stack) }
  try { await native() } catch (e) { ok('Native did the steps', 0, e.stack); if (NP) NP.kill() }
  fs.writeFileSync(path.join(OUT, 'titlebeh.json'), JSON.stringify(OUTS, null, 1));
  let i = 1; for (const s of STEPS.concat(['menu:rows'])) ok((i++) + '. ' + s + ': ' + OUTS.std[s], OUTS.std[s] !== undefined && OUTS.std[s] == OUTS.nat[s], {std: OUTS.std[s], nat: OUTS.nat[s]});
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
