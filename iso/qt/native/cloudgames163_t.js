// 1.6.3 Part 8, item 3 ("Xbox gaming and GeForce apps do not respond when clicked"): Xbox Cloud Gaming and GeForce NOW
// (desktop/game51.js) opened from every place they are, on each desktop engine:
//   Chromium mode (the page in Playwright's Chromium): the Start menu's row, its right-click Open, the App Store's Games tab
//     (Play), a pin on the taskbar, a desktop icon (double-click): each opens Oracle at the game's address;
//   Native (rudolf-native): the Start menu's row, a pin, a desktop icon: each opens Oracle's window at the game's address;
//   Lite (the page as rudolf-webkit runs it: RD.engine 'webkit', WebKitGTK without WebRTC): Chromium's window at the game's
//     address (rudolfd's web/open), and a notification that says so plainly.
// Run by same_t.sh with SAME_SCRIPT=cloudgames163_t.js:
//   SAME_SCRIPT=cloudgames163_t.js SAME_PORT=8742 SAME_DSP=:89 sh same_t.sh OUT
//   (node cloudgames163_t.js RUDOLF-NATIVE DIR OUT PORT DISPLAY)
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
const GAMES = {xcloud: {name: 'Xbox Cloud Gaming', url: 'https://www.xbox.com/play', host: /xbox\.com/}, gfn: {name: 'GeForce NOW', url: 'https://play.geforcenow.com/', host: /geforcenow\.com/}};
const MORE = {['bd95pins:' + ACCT]: ['explorer', 'notepad', 'xcloud', 'gfn'], ['bd95deskadd:' + ACCT]: ['xcloud', 'gfn']};
async function signIn(p) {
  await p.goto(BASE + '/'); await p.waitForFunction(() => typeof userLogin == 'function' && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
  await wait(1000); await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
  await p.waitForFunction(() => USER && getComputedStyle(document.getElementById('login')).display == 'none', null, {timeout: 60000}); await wait(3000);
}
/* ---- Chromium mode */
async function chromiumMode(br) {
  ST.state(D, {}, MORE);
  const ctx = await br.newContext({viewport: {width: W, height: H}, timezoneId: 'UTC'}); await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => r.abort());
  const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message)); await signIn(p);
  /* (what Oracle was asked to open: browser() as the page calls it, Oracle's window made as before) */
  await p.evaluate(() => { window.__opened = []; const b = browser; browser = function (u) { __opened.push(String(u || '')); return b.apply(this, arguments) } });
  const closeAll = () => p.evaluate(() => { wins.slice().forEach(w => { try { w.close() } catch (e) {} }); document.querySelectorAll('.ctx').forEach(c => c.remove()); $('#menu').style.display = 'none' });
  const step = async (place, k, act) => {
    await closeAll(); await wait(300); await p.evaluate(() => { __opened.length = 0 }); const e0 = errs.length, n0 = await p.evaluate(() => wins.length);
    try { await act(k) } catch (e) { errs.push('the test: ' + e.message) } await wait(1500);
    const r = await p.evaluate(() => ({opened: __opened.slice(), wins: wins.length}));
    ok('Chromium mode: ' + GAMES[k].name + ' from ' + place + ' opens Oracle at ' + GAMES[k].url, r.opened.includes(GAMES[k].url) && r.wins > n0 && errs.length == e0, {r, errs: errs.slice(e0)});
  };
  for (const k in GAMES) {
    await step('the Start menu', k, async k => { await p.click('#start'); await wait(400); await p.click('#menu li[data-a=' + k + ']') });
    await step('the Start menu\'s right-click Open', k, async k => { await p.click('#start'); await wait(400); await p.click('#menu li[data-a=' + k + ']', {button: 'right'}); await wait(300);
      const q = await p.evaluate(() => { const e = [...document.querySelectorAll('.ctx .cmi')].find(x => x.querySelector('span').textContent == 'Open'); const r = e.getBoundingClientRect(); return [r.left + 30, r.top + r.height / 2] }); await p.mouse.click(q[0], q[1]) });
    await step('the App Store\'s Games tab', k, async k => { await p.evaluate(() => { const W = appStore(); W.showGames() }); await wait(1500);
      const q = await p.evaluate(n => { const c = [...document.querySelectorAll('.gmbi .gmc')].find(x => x.querySelector('b').textContent == n); c.scrollIntoView(); const r = c.querySelector('button').getBoundingClientRect(); return [r.left + r.width / 2, r.top + r.height / 2] }, GAMES[k].name);
      await p.evaluate(() => { __opened.length = 0 }); await p.mouse.click(q[0], q[1]) });
    await step('a pin on the taskbar', k, async k => { const q = await p.evaluate(n => { const e = [...document.querySelectorAll('#pins > *')].find(b => b.title == n); const r = e.getBoundingClientRect(); return [r.left + r.width / 2, r.top + r.height / 2] }, GAMES[k].name); await p.mouse.click(q[0], q[1]) });
    await step('a desktop icon', k, async k => { const q = await p.evaluate(k => { const e = deskIconFor(k); const r = e.getBoundingClientRect(); return [r.left + r.width / 2, r.top + 20] }, k); await p.mouse.dblclick(q[0], q[1]) });
  }
  /* (the same table missed the Library's games of games58.js: each of their rows opens without an error now) */
  const rows = await p.evaluate(() => [...document.querySelectorAll('#menu li[data-a]')].map(l => l.dataset.a).filter(a => typeof A[a] != 'function' && APPS[a] && !(a in {xcloud: 1, gfn: 1})));
  for (const k of rows) { await closeAll(); await wait(200); const e0 = errs.length; await p.evaluate(k => document.querySelector('#menu li[data-a=' + k + ']').click(), k); await wait(800); await p.keyboard.press('Escape');
    ok('Chromium mode: the Start menu\'s ' + k + ' row opens without an error', errs.length == e0, errs.slice(e0)) }
  await ctx.close();
}
/* ---- Lite: the page as rudolf-webkit runs it (WebKitGTK 2.52 without WebRTC: Lite's engine check) */
async function lite(br) {
  ST.state(D, {}, MORE);
  const ctx = await br.newContext({viewport: {width: W, height: H}, timezoneId: 'UTC'}); await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => r.abort());
  const p = await ctx.newPage(); const web = [];
  await p.route(/\/api\/web\/open/, r => { web.push(decodeURIComponent(new URL(r.request().url()).searchParams.get('url') || '')); r.fulfill({status: 200, contentType: 'application/json', body: '{"ok":true}'}) });
  await signIn(p);
  const ready = await p.evaluate(() => { if (typeof RD == 'undefined' || !RD) return false; RD.engine = 'webkit'; window.RTCPeerConnection = undefined; window.__opened = []; window.__said = [];
    const nf = notify; notify = function (t, b) { __said.push(t + ': ' + b); return nf.apply(this, arguments) };
    const b = browser; browser = function (u) { __opened.push(String(u || '')); return b.apply(this, arguments) }; return true });
  ok('Lite: the page with rudolfd (the test\'s stand-in)', ready);
  for (const k in GAMES) {
    await p.evaluate(() => { __opened.length = 0; __said.length = 0 }); web.length = 0;
    await p.click('#start'); await wait(400); await p.click('#menu li[data-a=' + k + ']'); await wait(1500);
    const r = await p.evaluate(() => ({opened: __opened.slice(), said: __said.join(' | ')}));
    r.web = web.slice();
    ok('Lite: ' + GAMES[k].name + ' from the Start menu opens in Chromium\'s window, and says so', r.web.includes(GAMES[k].url) && !r.opened.length && /Chromium/.test(r.said) && r.said.includes(GAMES[k].name), r);
  }
  await ctx.close();
}
/* ---- Native */
let NP = null;
function nlines(k) { try { return fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } }
const nst = k => { const l = nlines(k)[0]; return l ? l.slice(k.length + 1) : null };
const nhit = k => { const l = nlines('hit').find(x => x.startsWith('hit ' + k + ' ')); if (!l) return null; const a = l.split(' ').slice(3).map(Number); return [a[0] + a[2] / 2, a[1] + a[3] / 2] };
const mrow = k => { const l = nlines('mrow').find(x => x.split(' ')[1] == k); if (!l) return null; const a = l.split(' ').slice(2).map(Number); return [a[0] + 40, a[1] + a[3] / 2] };
async function until(f, ms) { const t = Date.now(); while (Date.now() - t < (ms || 20000)) { if (await f()) return true; await wait(200) } return false }
const nclick = (c, b) => { if (c) xdo('mousemove ' + Math.round(c[0]) + ' ' + Math.round(c[1]) + ' click ' + (b || 1)) };
async function menuOpen() { if (nst('menu') != '1') nclick(nhit('start -')); return until(() => nst('menu') == '1' && nlines('mrow').length > 5, 6000) }
async function menuRow(k) { await menuOpen(); const r0 = nlines('mrow')[0].split(' ').slice(2).map(Number);
  for (let i = 0; i < 40 && !mrow(k); i++) { xdo('mousemove ' + (r0[0] + 60) + ' ' + (r0[1] + 100) + ' click 5'); await wait(150) } return mrow(k) }
const oracle = re => nlines('app').some(l => / browser /.test(l.slice(0, 12)) && re.test(l));
async function native() {
  ST.state(D, {}, MORE); try { fs.unlinkSync(path.join(D, 'session')) } catch (e) {}
  const env = Object.assign({}, env0, {HOME: D, RUDOLF_NATIVE_STATE: path.join(D, 'state'), LIBGL_ALWAYS_SOFTWARE: '1', TZ: 'UTC'});
  const lg = fs.openSync(path.join(OUT, 'native.log'), 'a'); NP = cp.spawn(BIN, ['--size=' + W + 'x' + H, '--draw=soft', '--base=' + BASE], {env, stdio: ['ignore', lg, lg], cwd: D});
  await until(() => nst('signin') == '1', 40000); await wait(2000); xdo("type --delay 40 '" + PW.replace(/'/g, "'\\''") + "'"); xdo('key Return');
  await until(() => nst('user') == 'Ann' && nst('signin') == '0', 30000); await wait(3000);
  /* (the places in turn, the two games alternating: Oracle's one window goes to the other game's address each time) */
  const steps = [];
  for (const place of ['the Start menu', 'a pin on the taskbar', 'a desktop icon']) for (const k in GAMES) steps.push([place, k]);
  for (const [place, k] of steps) {
    if (place == 'the Start menu') { const q = await menuRow(k); nclick(q) }
    else if (place == 'a pin on the taskbar') nclick(nhit('pin ' + k));
    else { const l = nlines('dicon').find(x => x.includes(' ' + k + '|')); const a = l ? l.split(' ').map(Number) : null; if (a) xdo('mousemove ' + (a[3] + a[5] / 2) + ' ' + (a[4] + 24) + ' click --repeat 2 --delay 80 1') }
    const g = await until(() => oracle(GAMES[k].host), 20000);
    ok('Native: ' + GAMES[k].name + ' from ' + place + ' opens Oracle at its address', g, {app: nlines('app'), dicon: place == 'a desktop icon' ? nlines('dicon') : undefined});
    if (nst('menu') == '1') xdo('key Escape'); await wait(800) }
  NP.kill();
}
(async () => {
  const br = await chromium.launch();
  try { await chromiumMode(br) } catch (e) { ok('Chromium mode did the steps', 0, e.stack) }
  try { await lite(br) } catch (e) { ok('Lite did the steps', 0, e.stack) }
  await br.close();
  try { await native() } catch (e) { ok('Native did the steps', 0, e.stack); if (NP) NP.kill() }
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
