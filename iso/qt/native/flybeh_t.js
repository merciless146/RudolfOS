// 1.6.0 (plan Part 12, item 3, second part, "The rule": Native works as Standard): the tray's pop-ups used the same way
// on both engines and each step's outcome compared: the calendar under the clock (opened and closed by the clock, the
// months with ▶ and ◀, closed by a click elsewhere, Open Calendar, a day double-clicked), the volume's (a click on the
// slider sets the volume, Mute, closed by its button, Desktop sounds…), and one pop-up giving way to the other.
// Run by same_t.sh with SAME_SCRIPT=flybeh_t.js.   node flybeh_t.js RUDOLF-NATIVE DIR OUT PORT DISPLAY
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path'), cp = require('child_process');
const [BIN, D, OUT, PORT, DSP] = process.argv.slice(2);
const W = 1920, H = 1080, BASE = 'http://127.0.0.1:' + PORT;
const KEYS = JSON.parse(fs.readFileSync(path.join(D, 'keys.json'))), PW = KEYS.Ann.pw;
const ST = require('./same_state.js'), ACCT = ST.ACCT;
const wait = ms => new Promise(r => setTimeout(r, ms));
const xdo = a => cp.execSync('xdotool ' + a, {env: Object.assign({}, process.env, {DISPLAY: DSP, LC_ALL: 'C.UTF-8'})}).toString();
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 700) : '')) };
fs.mkdirSync(OUT, {recursive: true});
const MORE = {['bd95cal:' + ACCT]: [{d: '2026-11-20', t: '10:00', title: 'Dentist'}]};
const vol = async () => { const v = await (await fetch(BASE + '/api/volume', {headers: {'X-Rudolf-Token': 'tok160same'}})).json(); return v.volume + (v.muted ? ' muted' : '') };
const volSet = async q => fetch(BASE + '/api/volume?' + q, {method: 'POST', headers: {'X-Rudolf-Token': 'tok160same'}});
// the steps: [name, what is done]; after each, what is open (cal MONTH, vol TEXT or -), the apps' windows, rudolfd's volume
const STEPS = ['open', 'next', 'prev2', 'clock-again', 'next2', 'clock-time', 'open2', 'outside', 'open3', 'opencal', 'vol', 'slider', 'mute', 'vol-again', 'vol2', 'to-clock', 'outside2', 'vol3', 'sounds', 'open4', 'dbl20'];
async function run(e) {
  const O = {}; await volSet('set=70'); await volSet('muted=0');
  for (const s of STEPS) {
    if (s == 'opencal') await e.click(await e.at('open'));
    else if (/^open|^clock-again|^to-clock/.test(s)) await e.click(await e.at('clock'));
    else if (/^vol/.test(s)) await e.click(await e.at('vol'));
    else if (s == 'next' || s == 'next2') await e.click(await e.at('next'));
    else if (s == 'clock-time') await e.click(await e.at('clocktime'));
    else if (s == 'prev2') { await e.click(await e.at('prev')); await e.click(await e.at('prev')) }
    else if (/^outside/.test(s)) await e.click([700, 400]);
    else if (s == 'slider') { const r = await e.at('range'); await e.click([r[2] + 8 + (154 - 16) * .25, r[1]]) }
    else if (s == 'mute') await e.click(await e.at('mute'));
    else if (s == 'sounds') await e.click(await e.at('sounds'));
    else if (s == 'dbl20') await e.dbl(await e.at('2026-11-20'));
    await wait(1500); O[s] = [await e.open(), await e.apps(), await vol()].join(' · ');
    if (s == 'opencal' || s == 'sounds' || s == 'dbl20') await e.closeApps();
  }
  return O;
}
function std(p) {
  return {
    at: k => p.evaluate(k => { const q = s => { const e = document.querySelector(s); if (!e) return null; const r = e.getBoundingClientRect(); return [r.left + r.width / 2, r.top + r.height / 2, r.left] };
      if (k == 'clock') return q('#tray'); if (k == 'clocktime') { const r = document.querySelector('#tray').getBoundingClientRect(); return [r.left + 20, r.top + r.height / 2] } if (k == 'vol') return q('#vol'); if (k == 'next') return q('#calp .cn'); if (k == 'prev') return q('#calp .cp'); if (k == 'open') return q('#calp .copen');
      if (k == 'range') return q('.svp input[type=range]'); if (k == 'mute') return q('.svp input[type=checkbox]'); if (k == 'sounds') return q('.svp a'); return q('#calp td[data-d="' + k + '"]') }, k),
    click: async q => { await p.mouse.click(q[0], q[1]); await wait(300) },
    dbl: async q => { await p.mouse.dblclick(q[0], q[1]); await wait(300) },
    open: () => p.evaluate(() => { const c = document.querySelector('#calp'), v = document.querySelector('.svp'); return c ? 'cal ' + c.querySelector('.fh b').textContent : v ? 'vol ' + v.querySelector('.svc').textContent : '-' }),
    apps: () => p.evaluate(() => wins.map(W => W.app).sort().join(',') || 'none'),
    closeApps: () => p.evaluate(() => wins.slice().forEach(W => W.close()))
  };
}
let NP = null;
function nlines(k) { try { return fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } }
const nst = k => { const l = nlines(k)[0]; return l ? l.slice(k.length + 1) : null };
const nhit = k => { const l = nlines('hit').find(x => x.startsWith('hit ' + k + ' ')); if (!l) return null; const a = l.split(' ').slice(3).map(Number); return [a[0] + a[2] / 2, a[1] + a[3] / 2] };
async function until(f, ms) { const t = Date.now(); while (Date.now() - t < (ms || 20000)) { if (await f()) return true; await wait(200) } return false }
const MON = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
function nat() {
  return {
    at: async k => { if (k == 'clock' || k == 'vol') return nhit(k + ' -'); if (k == 'clocktime') { const l = nlines('hit').find(x => x.startsWith('hit clock ')).split(' ').slice(3).map(Number); return [l[0] + 20, l[1] + l[3] / 2] } const l = nlines('flybtn').find(x => x.split(' ')[1] == k); if (!l) return null; const a = l.split(' ');
      if (k == 'range') { const f = nst('fly').split(' '); return [+a[2], +a[3], +a[2] - 77] } return [+a[2], +a[3]] },
    click: async q => { if (q) xdo('mousemove ' + Math.round(q[0]) + ' ' + Math.round(q[1]) + ' click 1'); await wait(400) },
    dbl: async q => { if (q) xdo('mousemove ' + Math.round(q[0]) + ' ' + Math.round(q[1]) + ' click --repeat 2 --delay 80 1'); await wait(400) },
    open: async () => { const f = (nst('fly') || '-').split(' '); if (f[0] == 'cal') { const [y, m] = f[5].split('-'); return 'cal ' + MON[+m - 1] + ' ' + y } if (f[0] == 'vol') return 'vol ' + (f[7] == '1' ? 'Muted' : f[6] + '%'); return '-' },
    apps: async () => { await until(() => true, 10); const l = nlines('client').map(x => (x.match(/ app=(\S+)/) || [])[1]).filter(k => k && k != '-' && !/^do-/.test(k)); return l.sort().join(',') || 'none' },
    closeApps: async () => { for (const l of nlines('client')) { const a = l.split(' '); if (!/ app=(-|do-)/.test(l)) xdo('mousemove ' + (+a[3] + +a[5] + 2 - 9 - 12) + ' ' + (+a[4] + 7 + 11) + ' click 1') } await until(() => !nlines('client').some(l => !/ app=(-|do-)/.test(l)), 15000); await wait(800) }
  };
}
(async () => {
  const OUTS = {};
  // Standard
  ST.state(D, {}, MORE);
  { const br = await chromium.launch(); const ctx = await br.newContext({viewport: {width: W, height: H}, timezoneId: 'UTC'}); await ctx.clock.install({time: new Date('2026-11-14T09:41:00Z')});
    await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => r.abort());
    const p = await ctx.newPage(); await p.goto(BASE + '/'); await p.waitForFunction(() => typeof userLogin == 'function' && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
    await wait(1000); await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
    await p.waitForFunction(() => USER && getComputedStyle(document.getElementById('login')).display == 'none', null, {timeout: 60000}); await wait(3000);
    OUTS.std = await run(std(p)); await br.close() }
  // Native
  ST.state(D, {}, MORE); try { fs.unlinkSync(path.join(D, 'session')) } catch (e) {}
  const env = Object.assign({}, process.env, {DISPLAY: DSP, HOME: D, RUDOLF_NATIVE_STATE: path.join(D, 'state'), LIBGL_ALWAYS_SOFTWARE: '1', TZ: 'UTC',
    LD_PRELOAD: process.env.SAME_FAKETIME || '', FAKETIME: '@2026-11-14 09:41:00', FAKETIME_DONT_FAKE_MONOTONIC: '1', FAKETIME_NO_CACHE: '1'}); if (!env.LD_PRELOAD) delete env.LD_PRELOAD;
  const lg = fs.openSync(path.join(OUT, 'native.log'), 'a'); NP = cp.spawn(BIN, ['--size=' + W + 'x' + H, '--draw=soft', '--base=' + BASE], {env, stdio: ['ignore', lg, lg], cwd: D});
  await until(() => nst('signin') == '1', 40000); await wait(2000); xdo("type --delay 40 '" + PW.replace(/'/g, "'\\''") + "'"); xdo('key Return');
  await until(() => nst('user') == 'Ann' && nst('signin') == '0', 30000); await wait(3000);
  OUTS.nat = await run(nat()); NP.kill();
  for (const s of STEPS) ok(s + ': ' + OUTS.std[s], OUTS.std[s] == OUTS.nat[s], {std: OUTS.std[s], nat: OUTS.nat[s]});
  // the page's own outcomes, whatever both do
  ok('the calendar: November, then December, then October', /^cal November 2026/.test(OUTS.std.open) && /^cal December 2026/.test(OUTS.std.next) && /^cal October 2026/.test(OUTS.std.prev2), OUTS.std);
  ok('the slider at a quarter: 25 %', /^vol 25% · .* · 25$/.test(OUTS.std.slider), OUTS.std.slider);
  fs.writeFileSync(path.join(OUT, 'flybeh.json'), JSON.stringify(OUTS, null, 1));
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('FAIL ' + e.stack); if (NP) NP.kill(); process.exit(1) });
