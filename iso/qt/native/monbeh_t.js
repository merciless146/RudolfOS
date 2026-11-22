// 1.6.1 (plan Part 1, "several monitors", and "The rule": Native works and looks as Standard): two monitors (1920x1080 at 0,0, the
// main one, and 1280x800 to its right), the same steps on both engines and each step's outcome compared: the workspace in use, the
// workspace each monitor shows, each window's workspace, whether it is shown, the monitor it is on (by its middle), maximised and
// whether it is inside its monitor; the second monitor's taskbar's windows and lit workspace. Steps: the start (Notepad on the main
// monitor), Calculator from the second taskbar's pin (opens there), maximised there (its size the second monitor's), restored,
// Notepad moved by Ctrl+Alt+Shift+Right to workspace 2 (onto the second monitor), the second taskbar's 3 (shows 3 there), its 1 (the
// main monitor takes 3), its 2 again, Notepad dragged onto the main monitor (into its workspace), the second monitor unplugged
// (its windows kept, none off the screen), plugged in again, and the lock screen (both monitors covered). The pictures of the start:
// the second monitor's taskbar and wallpaper, and the main taskbar, compared (pxdiff.py) as same_t.js does.
// Run by monbeh_t.sh.   node monbeh_t.js RUDOLF-NATIVE DIR OUT PORT DISPLAY
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path'), cp = require('child_process');
const [BIN, D, OUT, PORT, DSP] = process.argv.slice(2);
const SC = +(process.env.MON_SCALE || 100), BH = Math.round(46 * SC / 100), SET = SC != 100 ? {scale: SC} : {};   // (MON_SCALE=125: Display scaling)
const W = 3200, H = 1080, BASE = 'http://127.0.0.1:' + PORT, M = [[0, 0, 1920, 1080], [1920, 0, 1280, 800]];
const KEYS = JSON.parse(fs.readFileSync(path.join(D, 'keys.json'))), PW = KEYS.Ann.pw;
const ST = require('./same_state.js'), ACCT = ST.ACCT;
const wait = ms => new Promise(r => setTimeout(r, ms));
const env = Object.assign({}, process.env, {DISPLAY: DSP, LC_ALL: 'C.UTF-8'});
const xdo = a => cp.execSync('xdotool ' + a, {env}).toString();
const sh = c => cp.execSync(c, {env}).toString();
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 900) : '')) };
fs.mkdirSync(OUT, {recursive: true});
const MON2 = fs.readFileSync(path.join(D, 'monitors'), 'utf8'), MON1 = JSON.stringify(Object.assign(JSON.parse(MON2), {key: 'DUMMY0', monitors: JSON.parse(MON2).monitors.slice(0, 1)}));
const monOf = r => { const cx = r[0] + r[2] / 2, cy = r[1] + 16; return M.findIndex(m => cx >= m[0] && cx < m[0] + m[2] && cy >= m[1] && cy < m[1] + m[3]) };
const inside = (r, k) => { const m = M[Math.max(0, k)]; return r[0] >= m[0] - 2 && r[1] >= m[1] - 2 && r[0] + r[2] <= m[0] + m[2] + 2 && r[1] + r[3] <= m[1] + m[3] + 2 };
// one step's outcome, from {ws, mons:[ws,..], wins:[{app, ws, shown, rect, max}], bar2:[app,..], on2}
function line(o) {
  return ['ws ' + o.ws, 'mons ' + o.mons.join(','), 'bar2 ' + (o.bar2 || []).slice().sort().join(',') + ' on ' + o.on2,
    ...o.wins.slice().sort((a, b) => a.app < b.app ? -1 : 1).map(w => w.app + ' ws' + w.ws + (w.shown ? ' shown mon' + monOf(w.rect) + (inside(w.rect, monOf(w.rect)) ? ' inside' : ' OUTSIDE') + (w.max ? ' max' : '') : ' hidden'))].join(' · ');
}
const STEPS = ['start', 'pin2', 'max', 'unmax', 'move', 'b3', 'b1', 'b2', 'drag', 'unplug', 'plug', 'lock'];
const OUTS = {std: {}, nat: {}}, RECTS = {std: {}, nat: {}};
async function run(e, tag) {
  for (const s of STEPS) {
    if (s == 'pin2') await e.pin2('calc');
    else if (s == 'max') await e.max('calc', 1);
    else if (s == 'unmax') await e.max('calc', 0);
    else if (s == 'move') await e.moveNext('notepad');
    else if (s == 'b3') await e.ws2(3);
    else if (s == 'b1') await e.ws2(1);
    else if (s == 'b2') await e.ws2(2);
    else if (s == 'drag') await e.drag('notepad', -900, 60);
    else if (s == 'unplug') await e.plug(false);
    else if (s == 'plug') await e.plug(true);
    else if (s == 'lock') await e.lock();
    await wait(1500); const o = await e.report(); OUTS[tag][s] = line(o); RECTS[tag][s] = o;
    if (s == 'start') await e.shot(path.join(OUT, 'start.' + tag + '.png'));
    if (s == 'max' || s == 'lock') await e.shot(path.join(OUT, s + '.' + tag + '.png'));
  }
}
async function standard() {
  fs.writeFileSync(path.join(D, 'monitors'), MON2);
  ST.state(D, SET, {['bd95pins:' + ACCT]: ['explorer', 'notepad', 'calc', 'settings']});
  const br = await chromium.launch(); const ctx = await br.newContext({viewport: {width: W, height: H}, timezoneId: 'UTC'});
  await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => r.abort());
  const p = await ctx.newPage(); p.on('pageerror', x => console.log('page error: ' + x.message)); await p.goto(BASE + '/');
  await p.waitForFunction(() => typeof userLogin == 'function' && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
  await wait(1000); await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
  await p.waitForFunction(() => USER && getComputedStyle(document.getElementById('login')).display == 'none' && MON_ON, null, {timeout: 60000}); await wait(2500);
  await p.evaluate(() => launch('notepad')); await wait(1500);
  const at = (sel) => p.evaluate(s => { const e = document.querySelector(s); if (!e) return null; const r = e.getBoundingClientRect(); return [r.left + r.width / 2, r.top + r.height / 2] }, sel);
  const winQ = k => p.evaluate(k => { const W = wins.find(o => o.app == k); if (!W) return null; const r = W.el.querySelector('.tb').getBoundingClientRect(); return [r.left + 70, r.top + r.height / 2] }, k);
  const e = {
    pin2: async k => { const q = await at('#mpins1 [data-k="' + k + '"]'); if (q) await p.mouse.click(q[0], q[1]); await wait(1500) },
    max: async (k, on) => { const q = await winQ(k); if (q) await p.mouse.dblclick(q[0], q[1]); await wait(800) },
    moveNext: async k => { const q = await winQ(k); if (q) await p.mouse.click(q[0], q[1]); await p.keyboard.press('Control+Alt+Shift+ArrowRight'); await wait(600) },
    ws2: async n => { const q = await at('#mwsw1 button:nth-child(' + n + ')'); if (q) await p.mouse.click(q[0], q[1]); await wait(600) },
    drag: async (k, dx, dy) => { const q = await winQ(k); if (!q) return; await p.mouse.move(q[0], q[1]); await p.mouse.down(); await p.mouse.move(q[0] + dx, q[1] + dy, {steps: 12}); await p.mouse.up(); await wait(600) },
    plug: async on => { fs.writeFileSync(path.join(D, 'monitors'), on ? MON2 : MON1); await p.evaluate(() => monFetch()); await wait(800) },
    lock: async () => { await p.evaluate(() => lockScreen()); await wait(1200) },
    shot: f => p.screenshot({path: f}),
    report: () => p.evaluate(() => { const b = document.getElementById('mtasks1'), w2 = document.querySelector('#mwsw1 .on');
      return {ws: WS, mons: MON_ON ? MONS.map(m => m.ws) : [WS], bar2: b ? [...b.children].map(x => (x.__o || {}).app) : [], on2: w2 ? w2.textContent : '-',
        lock: document.documentElement.classList.contains('mlock'),
        wins: wins.map(o => { const r = o.el.getBoundingClientRect(); return {app: o.app, ws: o.ws, shown: o.el.style.display != 'none' && !o.min && r.width > 0, max: o.el.classList.contains('max'), rect: [r.left, r.top, r.width, r.height].map(Math.round)} })} })
  };
  await run(e, 'std'); await br.close();
}
let NP = null;
function nlines(k) { try { return fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } }
const nst = k => { const l = nlines(k)[0]; return l ? l.slice(k.length + 1) : null };
async function until(f, ms) { const t = Date.now(); while (Date.now() - t < (ms || 20000)) { if (await f()) return true; await wait(200) } return false }
async function native() {
  fs.writeFileSync(path.join(D, 'monitors'), MON2);
  ST.state(D, SET, {['bd95pins:' + ACCT]: ['explorer', 'notepad', 'calc', 'settings']}); try { fs.unlinkSync(path.join(D, 'session')) } catch (e) {}
  const ne = Object.assign({}, process.env, {DISPLAY: DSP, HOME: D, RUDOLF_NATIVE_STATE: path.join(D, 'state'), RUDOLF_MON_RUN: path.join(D, 'monitors'), LIBGL_ALWAYS_SOFTWARE: '1', TZ: 'UTC'});
  const lg = fs.openSync(path.join(OUT, 'native.log'), 'a'); NP = cp.spawn(BIN, ['--size=' + W + 'x' + H, '--draw=soft', '--base=' + BASE], {env: ne, stdio: ['ignore', lg, lg], cwd: D});
  await until(() => nst('signin') == '1', 40000); await wait(2000); xdo("type --delay 40 '" + PW.replace(/'/g, "'\\''") + "'"); xdo('key Return');
  await until(() => nst('user') == 'Ann' && nst('signin') == '0', 30000); await wait(2500);
  try { fs.copyFileSync(path.join(D, 'state'), path.join(OUT, 'native.state')) } catch (x) {}
  const clients = () => nlines('client').filter(l => !/ app=- /.test(l) && !/ app=do-/.test(l)).map(l => { const a = l.split(' '); const ws = (nlines('clientws').find(x => x.split(' ')[1] == a[1]) || '').split(' '), cm = (nlines('clientmon').find(x => x.split(' ')[1] == a[1]) || '').split(' ');
    const cw = +a[5], ch = +a[6], th = 35; return {w: a[1], app: (l.match(/ app=(\S+)/) || [])[1], min: +(l.match(/min=(\d)/) || [])[1], max: +(l.match(/max=(\d)/) || [])[1], ws: +ws[2], off: +ws[3], rect: [+cm[3], +cm[4], cw + 2, ch + th + 1]} });
  const mhit = (k, kind, arg) => { const l = nlines('mhit').find(x => x.startsWith('mhit ' + k + ' ' + kind + ' ' + arg + ' ')); if (!l) return null; const a = l.split(' ').slice(4).map(Number); return [a[0] + a[2] / 2, a[1] + a[3] / 2] };
  const click = (q, b) => { if (q) xdo('mousemove ' + q.map(Math.round).join(' ') + ' click ' + (b || 1)) };
  // (the notepad as Standard's: opened from the main taskbar's pin)
  { const l = nlines('hit').find(x => x.startsWith('hit pin notepad ')); if (l) { const a = l.split(' ').slice(3).map(Number); click([a[0] + a[2] / 2, a[1] + a[3] / 2]) } await until(() => clients().length > 0, 20000); await wait(2500) }
  const winQ = k => { const c = clients().find(x => x.app == k); return c ? [c.rect[0] + 70, c.rect[1] + 17] : null };
  const e = {
    pin2: async k => { const n0 = clients().length; click(mhit(1, 'pin', k)); await until(() => clients().length > n0, 20000); await wait(2500) },
    max: async (k, on) => { const q = winQ(k); if (q) xdo('mousemove ' + q.map(Math.round).join(' ') + ' click --repeat 2 --delay 80 1'); await wait(1200) },
    moveNext: async k => { click(winQ(k)); await wait(300); xdo('key ctrl+alt+shift+Right'); await wait(600) },
    ws2: async n => { click(mhit(1, 'ws', String(n))); await wait(600) },
    drag: async (k, dx, dy) => { const q = winQ(k); if (!q) return; xdo('mousemove ' + q.map(Math.round).join(' ') + ' mousedown 1'); for (let i = 1; i <= 12; i++) { xdo('mousemove ' + Math.round(q[0] + dx * i / 12) + ' ' + Math.round(q[1] + dy * i / 12)); await wait(30) } xdo('mouseup 1'); await wait(600) },
    plug: async on => { fs.writeFileSync(path.join(D, 'monitors'), on ? MON2 : MON1);
      if (on) { sh('xrandr --fb 3200x1080 --output DUMMY1 --mode 1280x800 --pos 1920x0'); sh('xrandr >/dev/null') } else { sh('xrandr --output DUMMY1 --off --fb 1920x1080') } await wait(1500) },
    lock: async () => { xdo('mousemove 1000 500'); xdo('key super+l'); await until(() => nst('locked') == '1', 8000); await wait(1200) },
    shot: f => sh('import -window root ' + f),
    report: async () => { const C = clients(), mm = nlines('mmon').map(l => +l.split(' ')[7]), ws = +nst('ws');
      const bar2 = nlines('mhit').filter(x => x.startsWith('mhit 1 win ')).map(x => (C.find(c => c.w.replace(/^0x/, '') == x.split(' ')[3]) || {}).app);
      // (the lit workspace on the second taskbar: the monitor's own)
      return {ws, mons: mm.length ? mm : [ws], bar2, on2: mm.length > 1 ? String(mm[1]) : '-', lock: nst('locked') == '1',
        wins: C.map(c => ({app: c.app, ws: c.ws, shown: !c.min && !c.off, max: !!c.max, rect: c.rect}))} }
  };
  await run(e, 'nat'); NP.kill(); await wait(500);
}
// (Settings › Display › Displays, on Standard: the monitors listed; a workspace chosen and kept (the stand-in rudolfd does what
// screenctl arrange and rudolf-display do); "Make main": the second monitor the main one, at the left)
async function settingsCheck() {
  fs.writeFileSync(path.join(D, 'monitors'), MON2); ST.state(D, {}, {});
  const br = await chromium.launch(); const ctx = await br.newContext({viewport: {width: W, height: H}, timezoneId: 'UTC'});
  await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => r.abort());
  const p = await ctx.newPage(); await p.goto(BASE + '/');
  await p.waitForFunction(() => typeof userLogin == 'function' && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
  await wait(1000); await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
  await p.waitForFunction(() => USER && MON_ON, null, {timeout: 60000}); await wait(1500);
  await p.evaluate(() => settings('disp')); await p.waitForSelector('#dmon1', {timeout: 15000}); await wait(800);
  const rows = await p.evaluate(() => [...document.querySelectorAll('#dmons .dmonn')].map(b => b.textContent).join(','));
  ok('Settings › Display lists both displays', rows == 'DUMMY0,DUMMY1', rows);
  await p.selectOption('#dmon1', '3'); await p.click('#dmonsave'); await wait(2500);
  let f = JSON.parse(fs.readFileSync(path.join(D, 'monitors'))).monitors.map(m => m.name + '=' + m.ws + (m.main ? '*' : '')).join(',');
  ok('the second display set to workspace 3 and kept', f == 'DUMMY0=1*,DUMMY1=3', f);
  ok('shown at once: the second monitor shows 3', await p.evaluate(() => MONS[1].ws == 3));
  await p.click('#dmons .dmonl[data-i="1"]'); await wait(2500);
  f = JSON.parse(fs.readFileSync(path.join(D, 'monitors'))).monitors.map(m => m.name + '=' + m.ws + (m.main ? '*' : '') + '@' + m.x).join(',');
  ok('"Make main": the second display is the main one, at the left', f == 'DUMMY1=3*@0,DUMMY0=1@1280', f);
  await br.close(); fs.writeFileSync(path.join(D, 'monitors'), MON2);
}
function cmp(name, ra, rb) {
  const o = path.join(OUT, 'cmp.' + name.replace(/:/g, '-'));   // (1.6.2: no ':' in a file name: upload-artifact refuses it)
  try { return JSON.parse(cp.execFileSync('python3', [path.join(__dirname, 'pxdiff.py'), path.join(OUT, (name.split(':')[0]) + '.std.png'), path.join(OUT, (name.split(':')[0]) + '.nat.png'), ra.join(','), (rb || ra).join(','), '40', o]).toString()) } catch (x) { return {pct: 100, err: String(x).slice(0, 200)} }
}
(async () => {
  if (process.env.MON_ONLY != 'nat') try { await standard() } catch (x) { console.log('FAIL standard: ' + x.stack) }
  if (process.env.MON_ONLY != 'nat') try { await settingsCheck() } catch (x) { console.log('FAIL settings: ' + x.stack) }
  if (process.env.MON_ONLY != 'std') try { await native() } catch (x) { console.log('FAIL native: ' + x.stack); if (NP) NP.kill() }
  for (const s of STEPS) ok('step ' + s + ': the same on both', OUTS.std[s] && OUTS.std[s] == OUTS.nat[s], {std: OUTS.std[s], nat: OUTS.nat[s]});
  // (what each step should have done, on Standard: the rules themselves)
  const S = OUTS.std;
  ok('calculator opened on the second monitor', /calc ws2 shown mon1 inside/.test(S.pin2 || ''), S.pin2);
  ok('maximised, it fills the second monitor', (() => { const o = RECTS.std.max; const c = o && o.wins.find(w => w.app == 'calc'); return c && c.max && monOf(c.rect) == 1 && Math.abs(c.rect[2] - 1280) < 12 * SC / 100 })(), RECTS.std.max);
  ok('notepad moved to workspace 2 is on the second monitor', /notepad ws2 shown mon1/.test(S.move || ''), S.move);
  ok("the second taskbar's 3 shows workspace 3 there", /mons 1,3/.test(S.b3 || ''), S.b3);
  ok("its 1: the main monitor takes 3", /mons 3,1/.test(S.b1 || ''), S.b1);
  ok('dragged onto the main monitor: into its workspace', /notepad ws3 shown mon0/.test(S.drag || ''), S.drag);
  ok('unplugged: one monitor, nothing outside it', /^ws \d · mons \d · /.test(S.unplug || '') && !/OUTSIDE|mon1/.test(S.unplug || ''), S.unplug);
  ok('plugged in again: two monitors', /mons \d,\d/.test(S.plug || ''), S.plug);
  ok('locked: covered on both engines', RECTS.std.lock && RECTS.std.lock.lock && RECTS.nat.lock && RECTS.nat.lock.lock);
  // (the look: the second monitor's taskbar (the clock left out: a minute may pass between the two), its wallpaper; the main taskbar)
  const b2 = cmp('start:bar2', [1920, 800 - BH, 1280 - 160, BH]), wp2 = cmp('start:wall2', [1920, 0, 1280, 800 - BH - 60]), b1 = cmp('start:bar1', [0, 1080 - BH, 1920 - 160, BH]), lk = cmp('lock:cover2', [1920, 0, 1280, 800]), mx = cmp('max:max2', [1920, 0, 1280 - 120, 800]);
  // (at another scale the engines' text differs by half pixels, the main taskbar's as much: the second one no further apart than it)
  const lim = SC == 100 ? 2 : Math.max(2, b1.pct + 1);
  ok('the second taskbar looks the same (' + b2.pct + ' %)', b2.pct <= lim, b2); ok('the second wallpaper looks the same (' + wp2.pct + ' %)', wp2.pct <= 2, wp2);
  ok('the main taskbar looks the same (' + b1.pct + ' %)', b1.pct <= (SC == 100 ? 2 : 6), b1); ok('the second monitor covered alike when locked (' + lk.pct + ' %)', lk.pct <= 2, lk);
  ok('a window maximised on the second monitor looks the same, its taskbar too (' + mx.pct + ' %)', mx.pct <= lim, mx);
  const f = R.filter(x => !x).length; console.log(f ? 'FAIL ' + f + ' of ' + R.length : 'PASS all ' + R.length); process.exit(f ? 1 : 0);
})();
