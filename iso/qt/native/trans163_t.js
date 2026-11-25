// 1.6.3 Part 8, item 2 ("Transitions on Native are rough/blocky"): the window transitions (LitePoint, litepoint60.js and
// litepoint.h) recorded frame by frame (ffmpeg x11grab, 60 frames a second) on Chromium mode (Playwright's Chromium on the
// screen, kiosk) and on Native, Notepad opened from its pin, minimized and restored with it, and compared:
//  N1. at rest, Native's frame has its rounded corners (its X shape: GDK set none on a frame, a window it did not make);
//  N2. opening, the window is never shown whole in one frame (it showed whole at once, the light drawn around it);
//  N3. minimizing and restoring with its taskbar button: never whole in one frame either (it vanished whole, and came back
//      whole for a frame or two before the transition);
//  N4. each transition's frames: at least 10 in its 240 ms, no gap over 50 ms between two (the shell's own count, lpstat),
//      and no more than 3 frames in a row without a change while it moves (the recording);
//  N5. Native's opening changes in at least 60 % as many frames as Chromium mode's.
// Under Xvfb (no graphics card: the soft drawing). The numbers go to OUT/trans163.json.   Run by same_t.sh:
//   SAME_SCRIPT=trans163_t.js SAME_PORT=8748 SAME_DSP=:94 sh same_t.sh OUT
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path'), cp = require('child_process');
const [BIN, D, OUT, PORT, DSP] = process.argv.slice(2);
const W = 1920, H = 1080, BASE = 'http://127.0.0.1:' + PORT;
const KEYS = JSON.parse(fs.readFileSync(path.join(D, 'keys.json'))), PW = KEYS.Ann.pw;
const ST = require('./same_state.js'), ACCT = ST.ACCT;
const wait = ms => new Promise(r => setTimeout(r, ms));
const env0 = Object.assign({}, process.env, {DISPLAY: DSP, LC_ALL: 'C.UTF-8'});
const xdo = a => cp.execSync('xdotool ' + a, {env: env0}).toString();
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 700) : '')) };
fs.mkdirSync(OUT, {recursive: true});
const PINS = ['explorer', 'notepad', 'calc'], M = {std: {}, nat: {}};
function rec(name, secs) { const f = path.join(OUT, name + '.gray'); const p = cp.spawn('ffmpeg', ['-loglevel', 'error', '-y', '-f', 'x11grab', '-framerate', '60', '-video_size', W + 'x' + H, '-i', DSP, '-t', String(secs), '-c:v', 'rawvideo', '-pix_fmt', 'gray', '-f', 'rawvideo', f], {env: env0, stdio: ['ignore', 'inherit', 'inherit']});
  return {f, done: new Promise(r => { p.on('exit', r); p.on('error', r) })} }
function an(f, box) { try { return JSON.parse(cp.execFileSync('python3', [path.join(__dirname, 'trans163_an.py'), f, String(W), String(H), ...box.map(v => String(Math.round(v)))]).toString()) } catch (e) { return {err: String(e.message).slice(0, 200)} } finally { try { fs.unlinkSync(f) } catch (e) {} } }
/* ---- Chromium mode, on the screen */
async function standard() {
  ST.state(D, {}, {['bd95pins:' + ACCT]: PINS});
  const br = await chromium.launch({headless: false, args: ['--kiosk', '--window-position=0,0', '--window-size=' + W + ',' + H], env: env0});
  const ctx = await br.newContext({viewport: null, timezoneId: 'UTC'}); await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => r.abort());
  const p = await ctx.newPage(); await p.goto(BASE + '/');
  await p.waitForFunction(() => typeof userLogin == 'function' && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
  await wait(1000); await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
  await p.waitForFunction(() => USER && getComputedStyle(document.getElementById('login')).display == 'none', null, {timeout: 60000}); await wait(4000);
  const pin = await p.evaluate(() => { const e = [...document.querySelectorAll('#pins > *')].find(b => b.title == APPS.notepad.name); const r = e.getBoundingClientRect(); return [r.left + r.width / 2, r.top + r.height / 2] });
  const box = () => p.evaluate(() => { const W = wins.find(w => w.app == 'notepad'); if (!W) return null; const r = W.el.getBoundingClientRect(); return [r.left, r.top, r.width, r.height] });
  for (const [k, secs] of [['open', 1.6], ['min', 1.2], ['res', 1.2]]) {
    const b0 = await box(); const r = rec('std-' + k, secs); await wait(350); await p.mouse.click(pin[0], pin[1]); await r.done; await wait(600);
    let b = await box(); if (!b) { await wait(1500); b = await box() } if (!b || b[2] < 1) b = b0; M.std[k] = b ? an(r.f, b) : {err: 'no window'} }
  await br.close();
}
/* ---- Native */
let NP = null;
function nlines(k) { try { return fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } }
const nst = k => { const l = nlines(k)[0]; return l ? l.slice(k.length + 1) : null };
const nhit = k => { const l = nlines('hit').find(x => x.startsWith('hit ' + k + ' ')); if (!l) return null; const a = l.split(' ').slice(3).map(Number); return [a[0] + a[2] / 2, a[1] + a[3] / 2] };
async function until(f, ms) { const t = Date.now(); while (Date.now() - t < (ms || 20000)) { if (await f()) return true; await wait(100) } return false }
const client = () => { const l = nlines('client').find(x => / app=notepad /.test(x)); if (!l) return null; const a = l.split(' '); return {w: a[1], frame: a[2], x: +a[3], y: +a[4], cw: +a[5], ch: +a[6]} };
const shape = fr => { try { const o = cp.execSync('xwininfo -shape -id 0x' + fr, {env: env0}).toString(); return /No window shape defined/.test(o) ? 'none' : ((o.match(/Window shape extents:\s*(\S+)/) || [])[1] || '?') } catch (e) { return 'err' } };
async function native() {
  ST.state(D, {}, {['bd95pins:' + ACCT]: PINS}); try { fs.unlinkSync(path.join(D, 'session')) } catch (e) {}
  const env = Object.assign({}, env0, {HOME: D, RUDOLF_NATIVE_STATE: path.join(D, 'state'), LIBGL_ALWAYS_SOFTWARE: '1', TZ: 'UTC'});
  const lg = fs.openSync(path.join(OUT, 'native.log'), 'a'); NP = cp.spawn(BIN, ['--size=' + W + 'x' + H, '--draw=soft', '--base=' + BASE], {env, stdio: ['ignore', lg, lg], cwd: D});
  await until(() => nst('signin') == '1', 40000); await wait(2000); xdo("type --delay 40 '" + PW.replace(/'/g, "'\\''") + "'"); xdo('key Return');
  await until(() => nst('user') == 'Ann' && nst('signin') == '0', 30000); await wait(4000);
  const ph = nhit('pin notepad'), click = () => xdo('mousemove ' + ph.map(Math.round).join(' ') + ' click 1');
  for (const [k, secs] of [['open', 3], ['min', 1.2], ['res', 1.2]]) {
    const r = rec('nat-' + k, secs); await wait(350); click(); if (k == 'open') await until(() => client(), 30000); await r.done; await wait(800);
    const c = client(); M.nat[k] = c ? an(r.f, [c.x, c.y, c.cw + 2, c.ch + 36]) : {err: 'no window'} }
  const c = client(); M.nat.rest = c ? shape(c.frame) : 'no window';
  M.nat.lpstat = nlines('lpstat').map(l => l.split(' ').slice(1));
  NP.kill();
}
(async () => {
  try { await standard() } catch (e) { ok('Chromium mode did the steps', 0, e.stack) }
  try { await native() } catch (e) { ok('Native did the steps', 0, e.stack); if (NP) NP.kill() }
  fs.writeFileSync(path.join(OUT, 'trans163.json'), JSON.stringify(M, null, 1));
  const s = M.std, n = M.nat, sum = o => o ? (o.err ? o.err : `${o.changed} frames changing in ${o.ms} ms, biggest ${o.biggest}, ${o.big_frames} big, still ${o.still_run}`) : '-';
  for (const k of ['open', 'min', 'res']) console.log(`  ${k}: Chromium ${sum(s[k])} | Native ${sum(n[k])}`);
  ok('N1. at rest, Native\'s frame has its rounded corners (an X shape: ' + n.rest + ')', n.rest && n.rest != 'none' && n.rest != 'err' && n.rest != 'no window', n.rest);
  ok('N2. opening, the window is never shown whole in one frame (biggest change ' + (n.open || {}).biggest + ' of its box)', n.open && n.open.biggest < .6, n.open);
  ok('N3. minimizing and restoring with its taskbar button: never whole in one frame either (biggest ' + (n.min || {}).biggest + ', ' + (n.res || {}).biggest + ')', n.min && n.res && n.min.biggest < .6 && n.res.biggest < .6, {min: n.min, res: n.res});
  const st = (n.lpstat || []).filter(a => /^(c2c|min|min<)$/.test(a[0]));
  ok('N4. each transition: 10 frames or more, no gap over 50 ms (' + st.map(a => a.join(' ')).join(', ') + '), no more than 3 still frames in a row while it moves',
    st.length >= 3 && st.every(a => +a[1] >= 10 && +a[3] <= 50) && ['open', 'min', 'res'].every(k => n[k] && n[k].still_run <= 3), {st, still: ['open', 'min', 'res'].map(k => n[k] && n[k].still_run)});
  ok('N5. Native\'s opening changes in at least 60 % as many frames as Chromium mode\'s', n.open && s.open && n.open.changed >= .6 * s.open.changed, {nat: n.open, std: s.open});
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
