// 1.6.0 (plan Part 12, item 6, "The rule": Native works as Standard): the screen saver and its lock on both engines, each step's
// outcome compared (the saver running, the screen locked). An hour without input (rudolfd's idle, as the test sets it) with sound
// playing, a microphone in use, an app keeping the screen on (rudolfd's media): no saver; none of them: the saver; input again
// after 4 s: the saver ends and the screen locks ("lock when the screen saver ends"); unlocked and idle again at once: no saver
// for a minute (UNLOCKED_AT). Each check is idleCheck: the page's 15 s timer, called at once on Standard; the shell's own 15 s tick
// on Native.
// Run by same_t.sh with SAME_SCRIPT=savbeh_t.js.   node savbeh_t.js RUDOLF-NATIVE DIR OUT PORT DISPLAY
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path'), cp = require('child_process');
const [BIN, D, OUT, PORT, DSP] = process.argv.slice(2);
const W = 1920, H = 1080, BASE = 'http://127.0.0.1:' + PORT;
const KEYS = JSON.parse(fs.readFileSync(path.join(D, 'keys.json'))), PW = KEYS.Ann.pw;
const ST = require('./same_state.js');
const wait = ms => new Promise(r => setTimeout(r, ms));
const xdo = a => cp.execSync('xdotool ' + a, {env: Object.assign({}, process.env, {DISPLAY: DSP, LC_ALL: 'C.UTF-8'})}).toString();
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 700) : '')) };
fs.mkdirSync(OUT, {recursive: true});
const MORE = {bd95sys: {sson: true, ssmin: 1, ssstyle: 'clock', lockss: true, lockmin: 0, suspmin: 0}}, OUTS = {std: {}, nat: {}};
const idle = ms => { if (ms == null) { try { fs.unlinkSync(path.join(D, 'idle')) } catch (e) {} } else fs.writeFileSync(path.join(D, 'idle'), String(ms)) };
const media = o => fs.writeFileSync(path.join(D, 'media'), JSON.stringify(o || {}));
const STEPS = ['playing', 'mic', 'held', 'idle', 'back', 'unlocked', 'fresh'];
async function run(e) {
  const O = {};
  for (const s of STEPS) {
    if (s == 'playing') { media({playing: 1}); idle(3600000); await e.check() }
    else if (s == 'mic') { media({mic: 1}); await e.check() }
    else if (s == 'held') { media({idleHeld: 1}); await e.check() }
    else if (s == 'idle') { media({}); await e.check(); await wait(2500) }
    else if (s == 'back') { await wait(2000); idle(0); await e.until(async () => (await e.report()).startsWith('saver 0 · locked 1'), 15000); await wait(1000) }
    else if (s == 'unlocked') { idle(null); await e.unlock(); await wait(1500) }
    else if (s == 'fresh') { idle(3600000); await e.check() }
    O[s] = await e.report();
  }
  idle(null); media({});
  return O;
}
async function until(f, ms) { const t = Date.now(); while (Date.now() - t < (ms || 20000)) { if (await f()) return true; await wait(200) } return false }
async function standard() {
  ST.state(D, {}, MORE); idle(null); media({});
  const br = await chromium.launch(); const ctx = await br.newContext({viewport: {width: W, height: H}, timezoneId: 'UTC'});
  await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => r.abort());
  const p = await ctx.newPage(); await p.goto(BASE + '/');
  await p.waitForFunction(() => typeof userLogin == 'function' && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
  await wait(1000); await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
  await p.waitForFunction(() => USER && getComputedStyle(document.getElementById('login')).display == 'none', null, {timeout: 60000}); await wait(3000);
  const e = {
    until,
    check: async () => { await p.evaluate(() => idleCheck()); await wait(1200) },
    unlock: async () => { await p.waitForSelector('#lockov .lkp', {timeout: 10000}); await p.focus('#lockov .lkp'); await p.keyboard.type(PW); await p.keyboard.press('Enter'); await until(() => p.evaluate(() => !LOCKED), 10000) },
    report: () => p.evaluate(() => 'saver ' + (SAVER && document.getElementById('saver') ? 1 : 0) + ' · locked ' + (LOCKED ? 1 : 0))
  };
  OUTS.std = await run(e); await br.close();
}
let NP = null;
function nlines(k) { try { return fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } }
const nst = k => { const l = nlines(k)[0]; return l ? l.slice(k.length + 1) : null };
async function native() {
  ST.state(D, {}, MORE); idle(null); media({}); try { fs.unlinkSync(path.join(D, 'session')) } catch (e) {}
  const env = Object.assign({}, process.env, {DISPLAY: DSP, HOME: D, RUDOLF_NATIVE_STATE: path.join(D, 'state'), LIBGL_ALWAYS_SOFTWARE: '1', TZ: 'UTC'});
  const lg = fs.openSync(path.join(OUT, 'native.log'), 'a'); NP = cp.spawn(BIN, ['--size=' + W + 'x' + H, '--draw=soft', '--base=' + BASE], {env, stdio: ['ignore', lg, lg], cwd: D});
  await until(() => nst('signin') == '1', 40000); await wait(2000); xdo("type --delay 40 '" + PW.replace(/'/g, "'\\''") + "'"); xdo('key Return');
  await until(() => nst('user') == 'Ann' && nst('signin') == '0', 30000); await wait(2500);
  const saverUp = () => (nst('saver') || '').startsWith('1') && /^client .* full=1 .* app=do-saver /m.test(fs.readFileSync(path.join(D, 'state'), 'utf8'));
  const e = {
    until,
    /* (the shell's next look: its 15 s tick; with the saver asked for, until its window is up) */
    check: async () => { const n0 = +nst('idlechecks'); await until(() => +nst('idlechecks') > n0, 20000); await wait(300); if ((nst('saver') || '').startsWith('1')) await until(saverUp, 30000) },
    unlock: async () => { await until(() => nst('locked') == '1' && nst('signin') == '1', 10000); await wait(800); xdo("type --delay 40 '" + PW.replace(/'/g, "'\\''") + "'"); xdo('key Return'); await until(() => nst('locked') == '0', 10000) },
    report: async () => 'saver ' + (saverUp() ? 1 : 0) + ' · locked ' + (nst('locked') == '1' ? 1 : 0)
  };
  OUTS.nat = await run(e); NP.kill();
}
(async () => {
  try { await standard() } catch (e) { ok('Standard did the steps', 0, e.stack) }
  try { await native() } catch (e) { ok('Native did the steps', 0, e.stack); if (NP) NP.kill() }
  fs.writeFileSync(path.join(OUT, 'savbeh.json'), JSON.stringify(OUTS, null, 1));
  let i = 1; for (const s of STEPS) ok((i++) + '. ' + s + ': ' + OUTS.std[s], OUTS.std[s] !== undefined && OUTS.std[s] == OUTS.nat[s], {std: OUTS.std[s], nat: OUTS.nat[s]});
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
