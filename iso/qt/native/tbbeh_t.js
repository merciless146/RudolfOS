// 1.6.0 (plan Part 12, item 3, "The rule": Native works as Standard): the taskbar's right-click menus used the same way on
// both engines and each step's outcome compared: a window's menu (Maximize, Restore, Minimize, Always on top's tick), the
// taskbar's (Restore all windows, Close all windows), a pinned app's (Unpin from taskbar, Oracle's New private tab), the
// start button's (Command Prompt). Run by same_t.sh with SAME_SCRIPT=tbbeh_t.js.   node tbbeh_t.js RUDOLF-NATIVE DIR OUT PORT DISPLAY
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path'), cp = require('child_process');
const [BIN, D, OUT, PORT, DSP] = process.argv.slice(2);
const W = 1920, H = 1080, BASE = 'http://127.0.0.1:' + PORT;
const KEYS = JSON.parse(fs.readFileSync(path.join(D, 'keys.json'))), PW = KEYS.Ann.pw;
const ST = require('./same_state.js'), ACCT = ST.ACCT;
const wait = ms => new Promise(r => setTimeout(r, ms));
const xdo = a => cp.execSync('xdotool ' + a, {env: Object.assign({}, process.env, {DISPLAY: DSP, LC_ALL: 'C.UTF-8'})}).toString();
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 700) : '')) };
fs.mkdirSync(OUT, {recursive: true});
const PINS = ['explorer', 'notepad', 'calc', 'settings', 'browser'], OUTS = {std: {}, nat: {}};
// the steps, as [what, menu (bar, start, pin:K, win:K), the row's words]; after each the engine reports its windows and pins
const STEPS = [['max', 'win:notepad', 'Maximize'], ['restore', 'win:notepad', 'Restore'], ['min', 'win:notepad', 'Minimize'], ['restoreall', 'bar', 'Restore all windows'],
  ['top', 'win:notepad', 'Always on top'], ['tick', 'win:notepad', ''], ['unpin', 'pin:calc', 'Unpin from taskbar'], ['private', 'pin:browser', 'New private tab'], ['prompt', 'start', 'Command Prompt'], ['closeall', 'bar', 'Close all windows']];
async function standard() {
  ST.state(D, {}, {['bd95pins:' + ACCT]: PINS});
  const br = await chromium.launch(); const ctx = await br.newContext({viewport: {width: W, height: H}, timezoneId: 'UTC'});
  await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => r.abort());
  const p = await ctx.newPage(); await p.goto(BASE + '/');
  await p.waitForFunction(() => typeof userLogin == 'function' && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
  await wait(1000); await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
  await p.waitForFunction(() => USER && getComputedStyle(document.getElementById('login')).display == 'none', null, {timeout: 60000}); await wait(3000);
  await p.evaluate(() => launch('notepad')); await wait(1500);
  const at = r => p.evaluate(([r, H]) => { const [k, a] = r.split(':'); let e = null; if (k == 'bar') return [1000, H - 21]; if (k == 'start') e = document.getElementById('start');
    if (k == 'pin') e = [...document.querySelectorAll('#pins > *')].find(b => b.title == APPS[a].name); if (k == 'win') e = [...document.querySelectorAll('#tasks button')].find(b => { const W = winOf(b); return W && W.app == a });
    const q = e.getBoundingClientRect(); return [q.left + q.width / 2, q.top + q.height / 2] }, [r, H]);
  const report = () => p.evaluate(() => ({wins: wins.map(W => [W.app, W.min ? 1 : 0, W.el.classList.contains('max') ? 1 : 0, W.el.classList.contains('opriv') ? 1 : 0]).sort((a, b) => (a[0] + a[3]).localeCompare(b[0] + b[3])), pins: pinList()}));
  for (const [n, m, row] of STEPS) {
    const q = await at(m); await p.mouse.click(q[0], q[1], {button: 'right'}); await wait(400);
    OUTS.std[n + ':menu'] = await p.evaluate(() => [...document.querySelectorAll('.ctx .cmi')].map(e => (e.classList.contains('dis') ? '-' : '') + e.querySelector('i').textContent + e.querySelector('span').textContent).join('|'));
    const it = await p.evaluate(l => { const e = [...document.querySelectorAll('.ctx .cmi')].find(x => x.querySelector('span').textContent == l); if (!e) return null; const r = e.getBoundingClientRect(); return [r.left + 40, r.top + r.height / 2] }, row);
    if (it) await p.mouse.click(it[0], it[1]); else await p.keyboard.press('Escape'); await wait(2500); OUTS.std[n] = await report() }
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
  const pins = async () => { const s = await (await fetch(BASE + '/api/desktop/settings', {headers: {'X-Rudolf-Token': 'tok160same'}})).json(); return s.pins };
  const report = async () => ({wins: nlines('client').filter(l => !/ app=- /.test(l) && !/ app=do-/.test(l)).map(l => { const a = l.split(' '), k = (l.match(/ app=(\S+)/) || [])[1];
    return [k, +(l.match(/min=(\d)/) || [])[1], +(l.match(/max=(\d)/) || [])[1], / app=browser .*Private/.test(l) ? 1 : 0] }).sort((a, b) => (a[0] + a[3]).localeCompare(b[0] + b[3])), pins: await pins()});
  for (const [n, m, row] of STEPS) {
    const [k, a] = m.split(':'); let q = k == 'bar' ? [1000, H - 21] : k == 'start' ? nhit('start -') : k == 'pin' ? nhit('pin ' + a) : null;
    if (k == 'win') { const cl = nlines('client').find(x => new RegExp(' app=' + a + ' ').test(x)); if (cl) q = nhit('win ' + cl.split(' ')[1]) }
    xdo('mousemove ' + q.map(Math.round).join(' ') + ' click 3'); await until(() => nlines('cmenu').length, 8000); await wait(200);
    const ics = nlines('cmicon').map(l => l.split(' ')); OUTS.nat[n + ':menu'] = nlines('cmitem').map((l, j) => { const b = l.split(' '), ic = ics.find(x => x[1] == String(j)); return (b[3] == '1' ? '-' : '') + (ic ? ic.slice(2).join(' ') : '') + l.slice(l.lastIndexOf('|') + 1) }).join('|');
    const l = row && nlines('cmitem').find(x => x.endsWith('|' + row)); if (l) { const b = l.split(' '); xdo('mousemove ' + b[1] + ' ' + b[2] + ' click 1') } else xdo('key Escape');
    await wait(4000); OUTS.nat[n] = await report() }
  NP.kill();
}
(async () => {
  try { await standard() } catch (e) { ok('Standard did the steps', 0, e.stack) }
  try { await native() } catch (e) { ok('Native did the steps', 0, e.stack); if (NP) NP.kill() }
  fs.writeFileSync(path.join(OUT, 'tbbeh.json'), JSON.stringify(OUTS, null, 1));
  /* (the menus' rows compared without the pictures: the page's ✓ for Always on top is a picture of its own row) */
  const strip = m => String(m || '').split('|').map(x => x.replace(/^(-?)[^A-Za-z]*/, '$1')).join('|');
  let i = 1; for (const [n] of STEPS) {
    ok((i++) + '. ' + n + ': the menu\'s rows and what it did (Standard: ' + JSON.stringify(OUTS.std[n]) + ')',
      OUTS.std[n] && (n == 'tick' ? /✓Always on top/.test(OUTS.std['tick:menu']) && /✓Always on top/.test(OUTS.nat['tick:menu']) : strip(OUTS.std[n + ':menu']) == strip(OUTS.nat[n + ':menu'])) && JSON.stringify(OUTS.std[n]) == JSON.stringify(OUTS.nat[n]),
      {std: [OUTS.std[n + ':menu'], OUTS.std[n]], nat: [OUTS.nat[n + ':menu'], OUTS.nat[n]]}) }
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
