// 2026-11-16 (the owner's Vivobook, "The rule": Native works as Standard): the desktop's sounds (base_v24.html sfx, Volume Controls). The
// same actions on both engines, each followed by the sounds it made: on Standard the page's (window.__sfx), on Native the shell's
// (its "sounds" count: the shell plays every sound now, its own buttons' and the pages' once each). The start button, a start menu
// row (a window opening), the window's ✕, quick settings and a tile, a notification, a button inside an app's window, a taskbar pin,
// a menu's row; each must make exactly the sounds Standard makes, none twice. Run by same_t.sh with SAME_SCRIPT=sfxbeh_t.js.
//   node sfxbeh_t.js RUDOLF-NATIVE DIR OUT PORT DISPLAY
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path'), cp = require('child_process');
const [BIN, D, OUT, PORT, DSP] = process.argv.slice(2);
const W = 1920, H = 1080, BASE = 'http://127.0.0.1:' + PORT;
const KEYS = JSON.parse(fs.readFileSync(path.join(D, 'keys.json'))), PW = KEYS.Ann.pw;
const ST = require('./same_state.js'), ACCT = ST.ACCT;
const wait = ms => new Promise(r => setTimeout(r, ms));
const env0 = Object.assign({}, process.env, {DISPLAY: DSP, LC_ALL: 'C.UTF-8'});
const xdo = a => cp.execSync('xdotool ' + a, {env: env0}).toString();
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 900) : '')) };
fs.mkdirSync(OUT, {recursive: true});
const MORE = {['bd95pins:' + ACCT]: ['explorer', 'notepad', 'calc']};
let NP = null;
function nlines(k) { try { return fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } }
const nst = k => { const l = nlines(k)[0]; return l ? l.slice(k.length + 1) : null };
const nhit = k => { const l = nlines('hit').find(x => x.startsWith('hit ' + k + ' ')); if (!l) return null; const a = l.split(' ').slice(3).map(Number); return [a[0] + a[2] / 2, a[1] + a[3] / 2] };
const mrow = k => { const l = nlines('mrow').find(x => x.split(' ')[1] == k); if (!l) return null; const a = l.split(' ').slice(2).map(Number); return [a[0] + 40, a[1] + a[3] / 2] };
const flybtn = k => { const l = nlines('flybtn').find(x => x.split(' ')[1] == k); if (!l) return null; const a = l.split(' '); return [+a[2], +a[3]] };
async function until(f, ms) { const t = Date.now(); while (Date.now() - t < (ms || 20000)) { if (await f()) return true; await wait(200) } return false }
let TEV = null;
function TEVAL() { if (!TEV) { TEV = path.join(D, 'teval'); try { fs.unlinkSync(TEV) } catch (e) {} cp.execSync('mkfifo ' + TEV) } return TEV }
async function neval(k, js) {
  const o = TEV + '.out', got = () => { try { return fs.readFileSync(o, 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } }, n0 = got().length;
  const fd = fs.openSync(TEV, fs.constants.O_RDWR | fs.constants.O_NONBLOCK); fs.writeSync(fd, k + ' ' + js.replace(/\n/g, ' ') + '\n'); fs.closeSync(fd);
  await until(() => got().length > n0, 8000); const r = got()[n0]; return r == null ? null : r.slice(k.length + 1);
}
const nclick = (c, b) => { if (c) xdo('mousemove ' + Math.round(c[0]) + ' ' + Math.round(c[1]) + ' click ' + (b || 1)) };
const clientOf = k => nlines('client').find(l => l.includes(' app=' + k + ' '));
/* the shell's sounds so far: {click: N, open: N, ...} */
const snd = () => { const o = {}; for (const kv of (nst('sounds') || '').split(' ')) { const [k, v] = kv.split(':'); if (k) o[k] = +v } return o };
const delta = (a, b) => { const o = {}; for (const k in b) if (b[k] - (a[k] || 0)) o[k] = b[k] - (a[k] || 0); return o };
const same = (a, b) => JSON.stringify(Object.keys(a).sort().map(k => [k, a[k]])) == JSON.stringify(Object.keys(b).sort().map(k => [k, b[k]]));
const count = l => { const o = {}; for (const [k] of l) o[k] = (o[k] || 0) + 1; return o };
const STEPS = ['start button', 'start menu row: Notepad', 'the window\'s ✕', 'quick settings', 'a tile (Dark mode)', 'a notification from an app', 'a button in an app\'s window', 'a pinned app', 'the desktop\'s menu: a row'];
const STD = {};
async function std() {
  const br = await chromium.launch(); const ctx = await br.newContext({viewport: {width: W, height: H}, timezoneId: 'UTC'});
  await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => r.abort());
  const p = await ctx.newPage(); await p.goto(BASE + '/'); await p.waitForFunction(() => typeof userLogin == 'function' && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
  await wait(1000); await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
  await p.waitForFunction(() => USER && getComputedStyle(document.getElementById('login')).display == 'none', null, {timeout: 60000}); await wait(3000);
  const step = async (n, f) => { const a = await p.evaluate(() => window.__sfx.length); await f(); await wait(700); STD[n] = count(await p.evaluate(a => window.__sfx.slice(a), a)) };
  await step(STEPS[0], () => p.click('#start'));
  await step(STEPS[1], () => p.click('#menu li[data-a=notepad]'));
  await step(STEPS[2], () => p.click('.win .tb .cl'));
  await step(STEPS[3], () => p.click('#qsb'));
  await step(STEPS[4], () => p.evaluate(() => [...document.querySelectorAll('#qsp .qst')].find(b => b.textContent.includes('Dark mode'))).then(() => p.click('#qsp .qst:first-child')));
  await p.mouse.click(W / 2, 300); await wait(300); await p.evaluate(() => { S.dm = 'light'; themeApply() });
  await step(STEPS[5], () => p.evaluate(() => notify('Hello', 'From an app')));
  await p.evaluate(() => launch('calc')); await wait(800);
  await step(STEPS[6], () => p.evaluate(() => { const b = [...document.querySelectorAll('.win button')].find(x => x.textContent.trim() == '7'); const r = b.getBoundingClientRect(); return [r.left + r.width / 2, r.top + r.height / 2] }).then(c => p.mouse.click(c[0], c[1])));
  await step(STEPS[7], () => p.click('#pins .pin:nth-child(1)'));
  await p.mouse.click(1500, 500, {button: 'right'}); await wait(300);
  await step(STEPS[8], () => p.evaluate(() => { const e = [...document.querySelectorAll('.ctx .cmi')].find(x => /Arrange icons by name/.test(x.textContent)); const r = e.getBoundingClientRect(); return [r.left + 30, r.top + r.height / 2] }).then(c => p.mouse.click(c[0], c[1])));
  console.log('Standard:', JSON.stringify(STD));
  ok('Standard: the start button and a menu\'s row click; a window opening and closing; a notification', STD[STEPS[0]].click == 1 && STD[STEPS[1]].open == 1 && STD[STEPS[2]].close == 1 && STD[STEPS[5]].notify == 1, STD);
  await br.close();
}
async function nat() {
  const N = {};
  const step = async (n, f, settle) => { const a = snd(); await f(); await wait(settle || 3000); N[n] = delta(a, snd()) };
  await step(STEPS[0], () => nclick(nhit('start -')), 800);
  await step(STEPS[1], async () => { await until(() => mrow('notepad'), 4000); nclick(mrow('notepad')); await until(() => clientOf('notepad'), 30000) }, 4000);
  await step(STEPS[2], async () => { const l = clientOf('notepad').split(' '); const fx = parseInt(l[3]), fy = parseInt(l[4]), cw = parseInt(l[5]);   /* (the frame's ✕: the title bar's last button) */
    nclick([fx + cw - 14, fy + 17]); await until(() => !clientOf('notepad'), 15000) }, 2500);
  await step(STEPS[3], async () => { nclick(nhit('qs -')); await until(() => (nst('fly') || '').startsWith('qs'), 5000) }, 800);
  await step(STEPS[4], () => nclick(flybtn('qs:0')), 7000);   /* (the tile's page has done its part by then: no second click) */
  xdo('key Escape'); await wait(300); nclick([W / 2, 300]); await wait(500);
  nclick(nhit('qs -')); await until(() => (nst('fly') || '').startsWith('qs'), 5000); nclick(flybtn('qs:0')); await wait(6000); xdo('key Escape'); nclick([W / 2, 300]); await wait(800);
  nclick(nhit('pin calc')); await until(() => clientOf('calc'), 30000); await wait(2500);
  await step(STEPS[5], () => neval('calc', `(()=>{notify('Hello','From an app');return 1})()`), 2500);
  await step(STEPS[6], async () => { const r = JSON.parse(await neval('calc', `JSON.stringify((()=>{const b=[...document.querySelectorAll('.win button')].find(x=>x.textContent.trim()=='7');const r=b.getBoundingClientRect();return [r.left+r.width/2,r.top+r.height/2]})())`));
    const l = clientOf('calc').split(' '); nclick([parseInt(l[3]) + 1 + r[0], parseInt(l[4]) + 35 + r[1]]) }, 1500);
  await step(STEPS[7], async () => { nclick(nhit('pin explorer')); await until(() => clientOf('explorer'), 30000) }, 4000);
  nclick([1500, 500], 3); await until(() => nlines('cmitem').some(l => l.endsWith('|Arrange icons by name')), 5000);
  await step(STEPS[8], () => { const l = nlines('cmitem').find(l => l.endsWith('|Arrange icons by name')).split(' '); nclick([+l[1], +l[2]]) }, 1500);
  console.log('Native:', JSON.stringify(N));
  for (const s of STEPS) ok('Native: ' + s + ': the sounds Standard makes, once each (' + JSON.stringify(STD[s] || {}) + ')', N[s] && same(N[s], STD[s] || {}), {native: N[s], standard: STD[s]});
}
(async () => {
  ST.state(D, {dm: 'light'}, MORE); try { await std() } catch (e) { ok('Standard ran: ' + e.message, false) }
  ST.state(D, {dm: 'light'}, MORE); try { fs.unlinkSync(path.join(D, 'session')) } catch (e) {}
  fs.writeFileSync(path.join(D, 'wm-native'), '');
  const env = Object.assign({}, process.env, {DISPLAY: DSP, HOME: D, RUDOLF_NATIVE_STATE: path.join(D, 'state'), RUDOLF_NATIVE_TEST_EVAL: TEVAL(), LIBGL_ALWAYS_SOFTWARE: '1', TZ: 'UTC'});
  for (const k of ['http_proxy', 'https_proxy', 'HTTP_PROXY', 'HTTPS_PROXY', 'all_proxy', 'ALL_PROXY']) delete env[k];
  const lg = fs.openSync(path.join(OUT, 'native.log'), 'a'); NP = cp.spawn(BIN, ['--size=' + W + 'x' + H, '--draw=soft', '--base=' + BASE], {env, stdio: ['ignore', lg, lg], cwd: D});
  await until(() => nst('signin') == '1', 40000); await wait(2000); xdo("type --delay 40 '" + PW.replace(/'/g, "'\\''") + "'"); xdo('key Return');
  await until(() => nst('user') == 'Ann' && nst('signin') == '0', 30000); await wait(6000);
  try { await nat() } catch (e) { ok('Native ran: ' + e.stack, false) } NP.kill();
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('FAIL ' + e.stack); if (NP) NP.kill(); process.exit(1) });
