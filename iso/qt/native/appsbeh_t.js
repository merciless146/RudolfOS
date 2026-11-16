// 2026-11-16 (the owner's Vivobook, "The rule": Native works as Standard): the programs rudolfd knows, as the owner has them: a Library
// web app (Ember Reach: a package, pkg:rudolf-emberreach), a Flatpak app (Discord: lx:com.discordapp.Discord.desktop) and a Windows
// program in Debian's Wine (Starport GE: lx:deb-starport-ge). On Standard (the reference) they are in the start menu with their own
// icons and open from it (rudolfd's apps/run, debian/launch; the package's page in a window), Packages is found by the start menu's
// search, and Desktop icons… lists every app with its tick shown on the desktop at once. On Native the same: each opens from the start
// menu, a pin and the desktop (a program rudolfd runs with no window of the page's left, as Standard leaves none), with its own picture,
// Packages found by the search, and a tick in Desktop icons… on the desktop within a second or two (it was minutes, or never).
// Run by same_t.sh with SAME_SCRIPT=appsbeh_t.js.   node appsbeh_t.js RUDOLF-NATIVE DIR OUT PORT DISPLAY
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
const ER = 'pkg:rudolf-emberreach', DC = 'lx:com.discordapp.Discord.desktop', SP = 'lx:deb-starport-ge';
const MORE = {['bd95pins:' + ACCT]: ['explorer', 'notepad', DC]};
const calls = () => { try { return fs.readFileSync(path.join(D, 'calls'), 'utf8') } catch (e) { return '' } };
let NP = null;
function nlines(k) { try { return fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } }
const nst = k => { const l = nlines(k)[0]; return l ? l.slice(k.length + 1) : null };
const nhit = k => { const l = nlines('hit').find(x => x.startsWith('hit ' + k + ' ')); if (!l) return null; const a = l.split(' ').slice(3).map(Number); return [a[0] + a[2] / 2, a[1] + a[3] / 2] };
const mrow = k => { const l = nlines('mrow').find(x => x.split(' ')[1] == k); if (!l) return null; const a = l.split(' ').slice(2).map(Number); return [a[0] + 40, a[1] + a[3] / 2] };
async function until(f, ms) { const t = Date.now(); while (Date.now() - t < (ms || 20000)) { if (await f()) return true; await wait(200) } return false }
let TEV = null;
function TEVAL() { if (!TEV) { TEV = path.join(D, 'teval'); try { fs.unlinkSync(TEV) } catch (e) {} cp.execSync('mkfifo ' + TEV) } return TEV }
async function neval(k, js) {
  const o = TEV + '.out', got = () => { try { return fs.readFileSync(o, 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } }, n0 = got().length;
  const fd = fs.openSync(TEV, fs.constants.O_RDWR | fs.constants.O_NONBLOCK); fs.writeSync(fd, k + ' ' + js.replace(/\n/g, ' ') + '\n'); fs.closeSync(fd);
  await until(() => got().length > n0, 8000); const r = got()[n0]; return r == null ? null : r.slice(k.length + 1);
}
const nclick = (c, b) => { if (c) xdo('mousemove ' + Math.round(c[0]) + ' ' + Math.round(c[1]) + ' click ' + (b || 1)) };
const shot = n => { try { cp.execSync('import -window root ' + path.join(OUT, n + '.png'), {env: env0}) } catch (e) {} };
const clientOf = k => nlines('client').find(l => l.includes(' app=' + k + ' '));
const pageOf = k => nlines('app').concat(nlines('page')).some(l => l.split(' ')[1] == k);
const catapp = k => { const l = nlines('catapp').find(x => x.split(' ')[1] == k); return l ? l.split(' ') : null };   /* catapp KEY X PICTURE HAS */
const ran = (re, c0) => re.test(calls().slice(c0));
async function menuOpen() { if (nst('menu') != '1') nclick(nhit('start -')); return until(() => nst('menu') == '1' && nlines('mrow').length > 5, 6000) }
/* a row of the start menu on the screen (the list scrolled to it, as a person would) */
async function menuRow(k) { await menuOpen(); const r0 = nlines('mrow')[0].split(' ').slice(2).map(Number);
  for (let i = 0; i < 40 && !mrow(k); i++) { xdo('mousemove ' + (r0[0] + 60) + ' ' + (r0[1] + 100) + ' click 5'); await wait(150) } return mrow(k) }
/* ---- Standard (the reference, unchanged) */
async function std() {
  const br = await chromium.launch(); const ctx = await br.newContext({viewport: {width: W, height: H}, timezoneId: 'UTC'});
  await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => r.abort());
  const p = await ctx.newPage(); await p.goto(BASE + '/'); await p.waitForFunction(() => typeof userLogin == 'function' && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
  await wait(1000); await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
  await p.waitForFunction(() => USER && getComputedStyle(document.getElementById('login')).display == 'none', null, {timeout: 60000});
  const have = await p.waitForFunction(k => k.every(x => APPS[x]), [ER, DC, SP], {timeout: 30000}).then(() => true, () => false);
  ok('Standard: the Library app, the Linux app and the Windows program are apps of the desktop', have, await p.evaluate(() => Object.keys(APPS).filter(k => /:/.test(k))));
  const rows = await p.evaluate(k => k.map(x => !![...document.querySelectorAll('#menu ul li')].find(l => (l.dataset.v25 || l.dataset.a) == x)), [ER, DC, SP]);
  ok('Standard: ...each in the start menu', rows.every(x => x), rows);
  const own = await p.evaluate(() => [...document.querySelectorAll('#menu ul li.lxv')].map(l => !!l.querySelector('img.lxapp')));
  ok('Standard: ...the Linux and Windows programs with their own icons', own.length == 2 && own.every(x => x), own);
  await p.click('#start'); await wait(300); await p.keyboard.type('Packages'); await wait(300);
  const sr = await p.evaluate(() => [...document.querySelectorAll('#ssres .ssi span')].map(e => e.textContent));
  ok('Standard: the start menu\'s search finds Packages', sr.includes('Packages'), sr);
  await p.keyboard.press('Escape');
  for (const [k, re] of [[DC, /POST apps\/run\?id=com\.discordapp\.Discord&desktop=com\.discordapp\.Discord\.desktop/], [SP, /POST debian\/launch\?id=starport-ge/]]) {
    const c0 = calls().length; await p.evaluate(k => { $('#menu').style.display = 'flex'; [...document.querySelectorAll('#menu ul li')].find(l => l.dataset.v25 == k).click() }, k);
    ok('Standard: the start menu\'s ' + k + ' asks rudolfd to run it', await until(() => ran(re, c0), 8000), calls().slice(c0).split('\n').filter(l => /run|launch/.test(l)));
  }
  await p.evaluate(k => { $('#menu').style.display = 'flex'; [...document.querySelectorAll('#menu ul li')].find(l => l.dataset.v25 == k).click() }, ER);
  const erw = await p.waitForFunction(() => [...document.querySelectorAll('.win iframe')].some(f => /\/pkg\/rudolf-emberreach\//.test(f.src)), null, {timeout: 10000}).then(() => true, () => false);
  ok('Standard: Ember Reach opens its package\'s page in a window', erw);
  await p.evaluate(() => deskIconsDlg()); await wait(300);
  const li = await p.evaluate(() => [...document.querySelectorAll('.dsr')].map(r => r.textContent.trim()));
  ok('Standard: Desktop icons… lists Ember Reach', li.some(t => /Ember Reach$/.test(t)), li);
  const t0 = Date.now(); await p.evaluate(() => [...document.querySelectorAll('.dsr')].find(r => /Ember Reach$/.test(r.textContent.trim())).querySelector('input').click());
  const on = await p.waitForFunction(k => deskHasApp(k), ER, {timeout: 5000}).then(() => true, () => false);
  ok('Standard: ...its tick puts it on the desktop at once', on && Date.now() - t0 < 1500, Date.now() - t0);
  await p.evaluate(() => [...document.querySelectorAll('.dsr')].find(r => /Ember Reach$/.test(r.textContent.trim())).querySelector('input').click());
  await br.close();
}
async function nat() {
  ok('Native: the catalogue has the three programs (Discord and Starport GE opened unseen, Ember Reach in a window)', await until(() => { const a = catapp(ER), b = catapp(DC), c = catapp(SP); return a && b && c && a[2] == '0' && b[2] == '1' && c[2] == '1' }, 40000), nlines('catapp'));
  ok('Native: ...the Linux app and the Windows program with their own pictures, as Standard', await until(() => [DC, SP].every(k => { const c = catapp(k); return c && c[4] == '1' }), 20000), nlines('catapp'));
  const seen = []; for (const k of [ER, DC, SP]) if (await menuRow(k)) seen.push(k); shot('menu');
  ok('Native: the start menu has their rows, with the programs\' pictures', seen.length == 3 && [DC, SP].every(k => (nlines('mric').find(l => l.split(' ')[1] == k) || '').endsWith(' 1')), [nlines('mrow').map(l => l.split(' ')[1]).filter(k => /:/.test(k)), nlines('mric')]);
  for (const [k, re, nm] of [[DC, /POST apps\/run\?id=com\.discordapp\.Discord&desktop=com\.discordapp\.Discord\.desktop/, 'Discord'], [SP, /POST debian\/launch\?id=starport-ge/, 'Starport GE']]) {
    xdo('key Escape'); await wait(300); await menuRow(k); const c0 = calls().length; nclick(mrow(k));
    const r = await until(() => ran(re, c0), 25000); await wait(500);
    ok('Native: the start menu\'s ' + nm + ' asks rudolfd to run it, as Standard', r, calls().slice(c0).split('\n').filter(l => /run|launch|PAGE/.test(l)));
    ok('Native: ...with no window of the page\'s shown for it, and its page gone', !clientOf(k) && await until(() => !pageOf(k), 15000), [clientOf(k), nlines('app')]);
    ok('Native: ...its "Starting…" notification shown, as Standard\'s', await until(() => nlines('toast').some(l => l.includes('|Starting ' + nm)), 6000), nlines('toast'));
  }
  xdo('key Escape'); await wait(300); await menuRow(ER); nclick(mrow(ER));
  ok('Native: the start menu\'s Ember Reach opens its package\'s page in a window', await until(() => clientOf(ER), 30000) && await until(async () => /pkg\/rudolf-emberreach/.test(await neval(ER, `[...document.querySelectorAll('.win iframe')].map(f=>f.src).join(' ')`) || ''), 15000), nlines('client'));
  shot('emberreach');
  await neval(ER, `(()=>{window.__oneAppClose();return 1})()`); await until(() => !clientOf(ER), 10000);
  /* the pin (Discord pinned) */
  { const c0 = calls().length; nclick(nhit('pin ' + DC)); ok('Native: Discord\'s pin runs it', !!nhit('pin ' + DC) && await until(() => ran(/POST apps\/run\?id=com\.discordapp\.Discord/, c0), 25000), nlines('hit').map(l => l.split(' ')[1] + ':' + l.split(' ')[2])) }
  /* Desktop icons… (the desktop's menu): Ember Reach listed; its tick on the desktop at once, and off again */
  nclick([W / 2, H / 2], 3); await until(() => nlines('cmitem').some(l => l.includes('|Desktop icons…')), 5000);
  { const l = nlines('cmitem').find(l => l.includes('|Desktop icons…')); if (l) { const a = l.split(' '); nclick([+a[1], +a[2]]) } }
  ok('Native: Desktop icons… opens', await until(() => clientOf('do-desk'), 30000), nlines('client'));
  const list = async () => { try { return JSON.parse(await neval('do-desk', `JSON.stringify([...document.querySelectorAll('.dsr')].map(r=>r.textContent.trim()))`)) } catch (e) { return [] } };
  ok('Native: ...lists Ember Reach (it was missing until "Restore the standard icons")', await until(async () => (await list()).some(t => /Ember Reach$/.test(t)), 15000), await list());
  for (const want of [1, 0]) {
    const t0 = Date.now(); await neval('do-desk', `(()=>{[...document.querySelectorAll('.dsr')].find(r=>/Ember Reach$/.test(r.textContent.trim())).querySelector('input').click();return 1})()`);
    const g = await until(() => nlines('dicon').some(l => l.includes(' ' + ER + '|')) == !!want, 10000), dt = Date.now() - t0;
    ok('Native: ...its tick ' + (want ? 'puts it on' : 'takes it off') + ' the desktop within 1.5 s (' + dt + ' ms)', g && dt < 1500, {dt, dicon: nlines('dicon').map(l => l.split(' ').slice(7).join(' '))});
    if (want) shot('desk-ember');
  }
  /* Packages, found by the start menu's search as on Standard */
  await neval('do-desk', `(()=>{window.__oneAppClose();return 1})()`); await wait(1000);
  await menuOpen(); xdo('type --delay 30 Packages'); await wait(800); shot('search-packages');
  ok('Native: the start menu\'s search finds Packages, as Standard\'s', nlines('srch').some(l => / Packages$/.test(l) && l.startsWith('srch Apps ')), nlines('srch'));
  { const l = nlines('srch').find(x => / Packages$/.test(x)); if (l) { const a = l.split(' '); nclick([+a[2] + 60, +a[3] + +a[5] / 2]) } }
  ok('Native: ...and a click on it opens Packages', await until(() => clientOf('packages'), 30000), nlines('client'));
  return {};
}
(async () => {
  fs.writeFileSync(path.join(D, 'progs'), ''); fs.writeFileSync(path.join(D, 'deb'), JSON.stringify({installed: true}));
  if (!process.env.APPS_NOSTD) { ST.state(D, {}, MORE); try { await std() } catch (e) { ok('Standard ran: ' + e.message, false) } }
  ST.state(D, {}, MORE); try { fs.unlinkSync(path.join(D, 'session')) } catch (e) {} try { fs.unlinkSync(path.join(D, 'apps.json')) } catch (e) {}
  fs.writeFileSync(path.join(D, 'wm-native'), '');
  const env = Object.assign({}, process.env, {DISPLAY: DSP, HOME: D, RUDOLF_NATIVE_STATE: path.join(D, 'state'), RUDOLF_NATIVE_TEST_EVAL: TEVAL(), LIBGL_ALWAYS_SOFTWARE: '1', TZ: 'UTC'});
  for (const k of ['http_proxy', 'https_proxy', 'HTTP_PROXY', 'HTTPS_PROXY', 'all_proxy', 'ALL_PROXY']) delete env[k];
  const lg = fs.openSync(path.join(OUT, 'native.log'), 'a'); NP = cp.spawn(BIN, ['--size=' + W + 'x' + H, '--draw=soft', '--base=' + BASE], {env, stdio: ['ignore', lg, lg], cwd: D});
  await until(() => nst('signin') == '1', 40000); await wait(2000); xdo("type --delay 40 '" + PW.replace(/'/g, "'\\''") + "'"); xdo('key Return');
  await until(() => nst('user') == 'Ann' && nst('signin') == '0', 30000); await wait(3000);
  try { await nat() } catch (e) { ok('Native ran: ' + e.stack, false) } NP.kill();
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('FAIL ' + e.stack); if (NP) NP.kill(); process.exit(1) });
