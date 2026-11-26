// 1.6.0 (plan Part 12, item 3, second part, "The rule": Native works as Standard): quick settings (☰) and the
// notifications' centre (🔔) used the same way on both engines and each step's outcome compared: the switches (Do not
// disturb, Dark mode on and off) and what the page keeps of them, a notification dismissed, Do not disturb in the
// centre, Clear all, one panel giving way to the other, Settings from quick settings; (1.6.0 Part 2) Airplane mode on and
// off (rudolfd's mobile/airplane, as same_rd.py keeps it); (Part 3) Mobile data off and on, with a connected modem.
// (1.6.3, the owner) the centre's Mute: on and off, kept as the page keeps it, and no pop-up while on, an error's included.
// Run by same_t.sh with SAME_SCRIPT=qsbeh_t.js.   node qsbeh_t.js RUDOLF-NATIVE DIR OUT PORT DISPLAY
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path'), cp = require('child_process');
const [BIN, D, OUT, PORT, DSP] = process.argv.slice(2);
const W = 1920, H = 1080, BASE = 'http://127.0.0.1:' + PORT;
const KEYS = JSON.parse(fs.readFileSync(path.join(D, 'keys.json'))), PW = KEYS.Ann.pw;
const ST = require('./same_state.js'), ACCT = ST.ACCT;
const wait = ms => new Promise(r => setTimeout(r, ms));
const xdo = a => cp.execSync('xdotool ' + a, {env: Object.assign({}, process.env, {DISPLAY: DSP, LC_ALL: 'C.UTF-8'})}).toString();
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 700) : '')) };
fs.mkdirSync(OUT, {recursive: true});
const MORE = {['bd95notif:' + ACCT]: [{id: 1, t: 'Download finished', b: 'report.pdf is in Downloads', k: 'info', app: '', at: Date.parse('2026-11-14T09:40:00Z')},
  {id: 2, t: 'Update failed', b: 'Could not reach the update server', k: 'error', app: '', at: Date.parse('2026-11-14T08:40:00Z')}]};
/* what the page keeps (rudolfd's copy of its storage): Do not disturb, the theme, how many notifications */
/* (dm: the page's default, auto, until a page saved its settings) */
const kept = async () => { const d = JSON.parse(fs.readFileSync(path.join(D, 'ls.json'), 'utf8')).data, st = JSON.parse(d['bd95set:' + ACCT] || '{}'), nt = JSON.parse(d['bd95notif:' + ACCT] || '[]');
  let air = '0', mob = '-'; try { air = fs.readFileSync(path.join(D, 'airplane'), 'utf8').trim() } catch (e) {} try { mob = fs.readFileSync(path.join(D, 'mobile'), 'utf8').trim() } catch (e) {}
  return 'dnd=' + (st.dnd ? 1 : 0) + ' nmute=' + (st.nmute ? 1 : 0) + ' dm=' + (st.dm || 'auto') + ' notes=' + nt.length + ' airplane=' + air + ' mobile=' + mob };
const STEPS = [['qs', 'open', 'qs'], ['dnd', 'tile', 'Do not disturb'], ['dark', 'tile', 'Dark mode'], ['light', 'tile', 'Dark mode'], ['air', 'tile', 'Airplane mode'], ['air-off', 'tile', 'Airplane mode'], ['mob-off', 'tile', 'Mobile data'], ['mob-on', 'tile', 'Mobile data'], ['qs-again', 'open', 'qs'],
  ['nc', 'open', 'notes'], ['dismiss', 'nx', 0], ['nc-dnd', 'dnd'], ['nc-mute', 'mute'], ['nc-unmute', 'mute'], ['clear', 'clear'], ['qs2', 'open', 'qs'], ['to-nc', 'open', 'notes'], ['outside', 'at', [700, 400]], ['qs3', 'open', 'qs'], ['settings', 'btn', 'settings']];
async function run(e) {
  const O = {}; fs.writeFileSync(path.join(D, 'mobile'), 'connected');   /* (a modem with its SIM unlocked, connected: the tenth switch) */
  for (const [n, what, arg] of STEPS) {
    if (what == 'open') await e.click(await e.bar(arg)); else if (what == 'at') await e.click(arg); else await e.click(await e.part(what, arg));
    await wait(what == 'open' || what == 'at' ? 1500 : 9000); O[n] = [await e.shown(), await e.apps(), await kept()].join(' · ');
    if (n == 'nc-mute' && e.muteToast) O.muteToast = await e.muteToast();
    if (n == 'settings') await e.closeApps() }
  return O;
}
function std(p) {
  const q = s => p.evaluate(s => { const e = typeof s == 'string' ? document.querySelector(s) : null; if (!e) return null; const r = e.getBoundingClientRect(); return [r.left + r.width / 2, r.top + r.height / 2] }, s);
  return {
    bar: k => q(k == 'qs' ? '#qsb' : '#ncb'),
    part: (w, a) => w == 'tile' ? p.evaluate(a => { const e = [...document.querySelectorAll('#qsp .qst')].find(x => x.querySelector('span').textContent == a); const r = e.getBoundingClientRect(); return [r.left + r.width / 2, r.top + r.height / 2] }, a)
      : w == 'nx' ? p.evaluate(i => { const r = document.querySelectorAll('#ncp .nx')[i].getBoundingClientRect(); return [r.left + r.width / 2, r.top + r.height / 2] }, a)
      : q(w == 'dnd' ? '#ncp .dnd' : w == 'mute' ? '#ncp .nmute' : w == 'clear' ? '#ncp .clr' : '#qsp .qset'),
    click: async c => { await p.mouse.click(c[0], c[1]); await wait(300) },
    shown: () => p.evaluate(() => { const qp = document.querySelector('#qsp'), np = document.querySelector('#ncp');
      if (qp) return 'qs:' + [...qp.querySelectorAll('.qst.on span')].map(s => s.textContent).join(',');
      if (np) return 'nc:' + (np.querySelector('.dnd').checked ? 1 : 0) + ':' + (np.querySelector('.nmute').checked ? 1 : 0) + ':' + [...np.querySelectorAll('.nitem b')].map(b => b.textContent).join(','); return '-' }),
    /* an error's notification while muted: no pop-up (quiet: not kept, so the lists stay the same as Native's) */
    muteToast: () => p.evaluate(() => { const h = document.getElementById('ntoasts'), k = h ? h.children.length : 0; notify('Muted test', '', {kind: 'error', quiet: true}); const h2 = document.getElementById('ntoasts'); return (h2 ? h2.children.length : 0) - k }),
    apps: () => p.evaluate(() => wins.map(W => W.app).sort().join(',') || 'none'),
    closeApps: () => p.evaluate(() => wins.slice().forEach(W => W.close()))
  };
}
let NP = null;
function nlines(k) { try { return fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } }
const nst = k => { const l = nlines(k)[0]; return l ? l.slice(k.length + 1) : null };
const nhit = k => { const l = nlines('hit').find(x => x.startsWith('hit ' + k + ' ')); if (!l) return null; const a = l.split(' ').slice(3).map(Number); return [a[0] + a[2] / 2, a[1] + a[3] / 2] };
async function until(f, ms) { const t = Date.now(); while (Date.now() - t < (ms || 20000)) { if (await f()) return true; await wait(200) } return false }
const QSL = ['Dark mode', 'Night light', 'Do not disturb', 'Screen reader', 'Magnifier', 'High contrast', 'On-screen keyboard', 'Touch mode', 'Airplane mode', 'Mobile data'];
function nat() {
  const btn = k => { const l = nlines('flybtn').find(x => x.split(' ')[1] == k); if (!l) return null; const a = l.split(' '); return [+a[2], +a[3]] };
  return {
    bar: async k => nhit(k + ' -'),
    part: async (w, a) => w == 'tile' ? btn('qs:' + QSL.indexOf(a)) : w == 'nx' ? btn('nx:' + a) : btn(w == 'dnd' ? 'dnd' : w == 'mute' ? 'mute' : w == 'clear' ? 'clear' : 'settings'),
    click: async c => { if (c) xdo('mousemove ' + Math.round(c[0]) + ' ' + Math.round(c[1]) + ' click 1'); await wait(400) },
    shown: async () => { const f = (nst('fly') || '-').split(' ')[0]; if (f == 'qs') return 'qs:' + (nst('flyqs') || '').trim().split(' ').filter(Boolean).join(' ').replace(/(\w) (\w)/g, '$1 $2').split(/ (?=[A-Z])/).join(',');
      if (f == 'nc') { const l = nst('flync') || ''; const [h, ...t] = l.split('|'); const [n, d] = h.split(' '); return 'nc:' + d + ':' + (nst('nmute') || '0').trim() + ':' + t.join(',') } return '-' },
    apps: async () => nlines('client').map(x => (x.match(/ app=(\S+)/) || [])[1]).filter(k => k && k != '-' && !/^do-/.test(k)).sort().join(',') || 'none',
    closeApps: async () => { for (const l of nlines('client')) { const a = l.split(' '); if (!/ app=(-|do-)/.test(l)) xdo('mousemove ' + (+a[3] + +a[5] + 2 - 9 - 12) + ' ' + (+a[4] + 7 + 11) + ' click 1') } await until(() => !nlines('client').some(l => !/ app=(-|do-)/.test(l)), 15000); await wait(800) }
  };
}
(async () => {
  const OUTS = {};
  ST.state(D, {}, MORE);
  { const br = await chromium.launch(); const ctx = await br.newContext({viewport: {width: W, height: H}, timezoneId: 'UTC'});
    await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => r.abort());
    const p = await ctx.newPage(); await p.goto(BASE + '/'); await p.waitForFunction(() => typeof userLogin == 'function' && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
    await wait(1000); await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
    await p.waitForFunction(() => USER && getComputedStyle(document.getElementById('login')).display == 'none', null, {timeout: 60000}); await wait(3000);
    OUTS.std = await run(std(p)); await br.close() }
  ST.state(D, {}, MORE); try { fs.unlinkSync(path.join(D, 'session')) } catch (e) {}
  const env = Object.assign({}, process.env, {DISPLAY: DSP, HOME: D, RUDOLF_NATIVE_STATE: path.join(D, 'state'), LIBGL_ALWAYS_SOFTWARE: '1', TZ: 'UTC'});
  const lg = fs.openSync(path.join(OUT, 'native.log'), 'a'); NP = cp.spawn(BIN, ['--size=' + W + 'x' + H, '--draw=soft', '--base=' + BASE], {env, stdio: ['ignore', lg, lg], cwd: D});
  await until(() => nst('signin') == '1', 40000); await wait(2000); xdo("type --delay 40 '" + PW.replace(/'/g, "'\\''") + "'"); xdo('key Return');
  await until(() => nst('user') == 'Ann' && nst('signin') == '0', 30000); await wait(3000);
  OUTS.nat = await run(nat()); NP.kill();
  for (const [s] of STEPS) ok(s + ': ' + OUTS.std[s], OUTS.std[s] == OUTS.nat[s], {std: OUTS.std[s], nat: OUTS.nat[s]});
  ok('the page\'s own outcomes: Do not disturb kept, Dark mode on then off, one dismissed, all cleared', /dnd=1/.test(OUTS.std.dnd) && /dm=dark/.test(OUTS.std.dark) && /dm=light/.test(OUTS.std.light) && /notes=1/.test(OUTS.std.dismiss) && /notes=0/.test(OUTS.std.clear), OUTS.std);
  ok('Mute: kept on, then off; no pop-up while on, an error\'s included', /nmute=1/.test(OUTS.std['nc-mute']) && /nmute=0/.test(OUTS.std['nc-unmute']) && OUTS.std.muteToast === 0, {mute: OUTS.std['nc-mute'], unmute: OUTS.std['nc-unmute'], toast: OUTS.std.muteToast});
  fs.writeFileSync(path.join(OUT, 'qsbeh.json'), JSON.stringify(OUTS, null, 1));
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('FAIL ' + e.stack); if (NP) NP.kill(); process.exit(1) });
