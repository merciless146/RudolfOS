// 1.6.0 (plan Part 12, item 3, second part, "The rule": Native works as Standard): the tray's other pop-ups used the same
// way on both engines and each step's outcome compared: quick settings (a tile on and off, Do not disturb turning the bell
// into 🔕 and kept in the account's settings, Lock, Settings, Power, Meta+A), the notifications' centre (Do not disturb's
// checkbox, a notification dismissed with ✕ and Clear all, kept in the page's bd95notif:ID; Meta+N), one pop-up giving way
// to the other, a click elsewhere, the network's (Network Settings…, its button again), the battery's button.
// Run by same_t.sh with SAME_SCRIPT=traybeh_t.js.   node traybeh_t.js RUDOLF-NATIVE DIR OUT PORT DISPLAY
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path'), cp = require('child_process');
const [BIN, D, OUT, PORT, DSP] = process.argv.slice(2);
const W = 1920, H = 1080, BASE = 'http://127.0.0.1:' + PORT;
const KEYS = JSON.parse(fs.readFileSync(path.join(D, 'keys.json'))), PW = KEYS.Ann.pw;
const ST = require('./same_state.js'), ACCT = ST.ACCT;
const wait = ms => new Promise(r => setTimeout(r, ms));
const xdo = a => cp.execSync('xdotool ' + a, {env: Object.assign({}, process.env, {DISPLAY: DSP, LC_ALL: 'C.UTF-8'})}).toString();
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 700) : '')) };
fs.mkdirSync(OUT, {recursive: true});
const MORE = {['bd95notif:' + ACCT]: [{id: 2, t: 'Backup finished', b: 'Your files were copied to the USB drive.', k: 'info', app: '', at: Date.parse('2026-11-14T09:30:00Z')}, {id: 1, t: 'Update failed', b: 'Rudolf OS could not reach the update server.', k: 'error', app: '', at: Date.parse('2026-11-14T08:05:00Z')}]};
const QSL = ['Dark mode', 'Night light', 'Do not disturb', 'Screen reader', 'Magnifier', 'High contrast', 'On-screen keyboard', 'Touch mode'];
// (what the page keeps: Do not disturb in the account's settings, the notifications)
const kept = () => { try { const d = JSON.parse(fs.readFileSync(path.join(D, 'ls.json'))).data; const s = JSON.parse(d['bd95set:' + ACCT] || '{}'), n = JSON.parse(d['bd95notif:' + ACCT] || '[]'); return 'dnd=' + !!s.dnd + ' notes=' + n.length } catch (e) { return '?' } };
// the steps; after each: what is open (qs and its tiles on, nc and its number, net), the bell, the apps' windows, the lock, the
// Power dialog, what is kept
const STEPS = ['qs', 'qs-dnd', 'qs-dnd2', 'to-nc', 'nc-dnd', 'nc-dnd2', 'nc-x', 'nc-clear', 'outside', 'net', 'net-again', 'net2', 'net-set', 'qs2', 'qs-set', 'qs3', 'qs-lock', 'qs4', 'qs-off', 'meta-a', 'meta-n', 'meta-n2', 'bat'];
async function run(e) {
  const O = {};
  for (const s of STEPS) {
    if (/^qs\d?$/.test(s)) await e.click(await e.at('qs'));
    else if (s == 'qs-dnd' || s == 'qs-dnd2') await e.click(await e.at('tile:Do not disturb'));
    else if (s == 'to-nc') await e.click(await e.at('notes'));
    else if (s == 'nc-dnd' || s == 'nc-dnd2') await e.click(await e.at('dnd'));
    else if (s == 'nc-x') await e.click(await e.at('nx:0'));
    else if (s == 'nc-clear') await e.click(await e.at('clr'));
    else if (s == 'outside') await e.click([700, 400]);
    else if (/^net(-again|2)?$/.test(s)) await e.click(await e.at('net'));
    else if (s == 'net-set') await e.click(await e.at('npo'));
    else if (s == 'qs-set') await e.click(await e.at('qset'));
    else if (s == 'qs-lock') await e.click(await e.at('qlock'));
    else if (s == 'qs-off') await e.click(await e.at('qoff'));
    else if (s == 'meta-a') await e.key('a');
    else if (/^meta-n/.test(s)) await e.key('n');
    else if (s == 'bat') await e.click(await e.at('bat'));
    await wait(2500); O[s] = [await e.open(), await e.bell(), await e.apps(), await e.locked(), await e.power(), kept()].join(' · ');
    if (s == 'net-set' || s == 'qs-set' || s == 'bat') await e.closeApps();
    if (s == 'qs-lock') await e.unlock();
    if (s == 'qs-off') await e.escape();   /* (its Cancel) */
  }
  return O;
}
function std(p) {
  return {
    at: k => p.evaluate(k => { const q = e => { if (typeof e == 'string') e = document.querySelector(e); if (!e) return null; const r = e.getBoundingClientRect(); return [r.left + r.width / 2, r.top + r.height / 2] };
      if (k == 'qs') return q('#qsb'); if (k == 'notes') return q('#ncb'); if (k == 'net') return q('#net'); if (k == 'bat') return q('#bat');
      if (k.startsWith('tile:')) return q([...document.querySelectorAll('#qsp .qst')].find(b => b.querySelector('span').textContent == k.slice(5)));
      if (k == 'dnd') return q('#ncp input.dnd'); if (k == 'nx:0') return q('#ncp .nx'); if (k == 'clr') return q('#ncp .clr'); if (k == 'npo') return q('.np .npo');
      if (k == 'qset') return q('#qsp .qset'); if (k == 'qlock') return q('#qsp .qlock'); if (k == 'qoff') return q('#qsp .qoff'); return null }, k),
    click: async q => { if (q) await p.mouse.click(q[0], q[1]); await wait(300) },
    key: async k => { await p.keyboard.press('Meta+' + k); await wait(300) },
    open: () => p.evaluate(QSL => { const qs = document.querySelector('#qsp'), nc = document.querySelector('#ncp'), np = document.querySelector('.np');
      if (qs) return 'qs ' + [...qs.querySelectorAll('.qst')].map((b, i) => b.classList.contains('on') ? i : '').join('');
      if (nc) return 'nc ' + nc.querySelectorAll('.nitem').length + (nc.querySelector('input.dnd').checked ? ' dnd' : '');
      if (np) return 'net ' + np.querySelectorAll('.npr').length; return '-' }, QSL),
    bell: () => p.evaluate(() => { const b = document.getElementById('ncb'); return b.textContent + (b.dataset.n || '') }),
    apps: () => p.evaluate(() => wins.map(W => W.app).sort().join(',') || 'none'),
    locked: () => p.evaluate(() => LOCKED ? 'locked' : 'unlocked'),
    power: () => p.evaluate(() => [...document.querySelectorAll('.rmodal .rmbox h3')].some(h => h.textContent == 'Power') ? 'power' : '-'),
    closeApps: () => p.evaluate(() => wins.slice().forEach(W => W.close())),
    unlock: async () => { await p.keyboard.type(PW); await p.keyboard.press('Enter'); await wait(1500) },
    escape: async () => { await p.click('.rmodal button[data-a=x]'); await wait(300) }
  };
}
let NP = null;
function nlines(k) { try { return fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } }
const nst = k => { const l = nlines(k)[0]; return l ? l.slice(k.length + 1) : null };
const nhit = k => { const l = nlines('hit').find(x => x.startsWith('hit ' + k + ' ')); if (!l) return null; const a = l.split(' ').slice(3).map(Number); return [a[0] + a[2] / 2, a[1] + a[3] / 2] };
async function until(f, ms) { const t = Date.now(); while (Date.now() - t < (ms || 20000)) { if (await f()) return true; await wait(200) } return false }
function nat() {
  return {
    at: async k => { if (['qs', 'notes', 'net', 'bat'].includes(k)) return nhit(k + ' -');
      if (k.startsWith('tile:')) k = 'qs:' + ['dm', 'nl', 'dnd', 'sr', 'mag', 'hc', 'vk', 'tm'][QSL.indexOf(k.slice(5))];
      const l = nlines('flybtn').find(x => x.split(' ')[1] == k); if (!l) return null; const a = l.split(' '); return [+a[2], +a[3]] },
    click: async q => { if (q) xdo('mousemove ' + Math.round(q[0]) + ' ' + Math.round(q[1]) + ' click 1'); await wait(400) },
    key: async k => { xdo('key super+' + k); await wait(400) },
    open: async () => { const f = (nst('fly') || '-').split(' '); const v = (nst('qsv') || '').split(' '), n = (nst('notes') || '0 0 0').split(' ');
      if (f[0] == 'qs') return 'qs ' + v.map((x, i) => x == '1' ? i : '').join(''); if (f[0] == 'nc') return 'nc ' + n[2] + (n[0] == '1' ? ' dnd' : '');
      if (f[0] == 'net') return 'net ' + nlines('flybtn').filter(x => x.startsWith('flybtn row:')).length; return f[0] == '-' ? '-' : f[0] },
    bell: async () => { const n = (nst('notes') || '0 0 0').split(' '); return (n[0] == '1' ? '🔕' : '🔔') + (n[1] != '0' ? n[1] : '') },
    apps: async () => { const l = nlines('client').map(x => (x.match(/ app=(\S+)/) || [])[1]).filter(k => k && k != '-' && !/^do-/.test(k)); return l.sort().join(',') || 'none' },
    locked: async () => nst('locked') == '1' ? 'locked' : 'unlocked',
    power: async () => nst('power') == '1' ? 'power' : '-',
    closeApps: async () => { for (const l of nlines('client')) { const a = l.split(' '); if (!/ app=(-|do-)/.test(l)) xdo('mousemove ' + (+a[3] + +a[5] + 2 - 9 - 12) + ' ' + (+a[4] + 7 + 11) + ' click 1') } await until(() => !nlines('client').some(l => !/ app=(-|do-)/.test(l)), 15000); await wait(800) },
    unlock: async () => { xdo("type --delay 40 '" + PW.replace(/'/g, "'\\''") + "'"); xdo('key Return'); await until(() => nst('locked') == '0', 8000); await wait(800) },
    escape: async () => { xdo('key Tab Tab Tab Return'); await until(() => nst('power') == '0', 5000); await wait(300) }
  };
}
(async () => {
  const OUTS = {};
  // Standard
  ST.state(D, {}, MORE);
  { const br = await chromium.launch(); const ctx = await br.newContext({viewport: {width: W, height: H}, timezoneId: 'UTC'}); await ctx.clock.install({time: new Date('2026-11-14T09:41:00Z')});
    await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => /generate_204/.test(r.request().url()) ? r.fulfill({status: 204, body: ''}) : r.abort());
    const p = await ctx.newPage(); await p.goto(BASE + '/'); await p.waitForFunction(() => typeof userLogin == 'function' && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
    await wait(1000); await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
    await p.waitForFunction(() => USER && getComputedStyle(document.getElementById('login')).display == 'none', null, {timeout: 60000});
    for (let i = 0; i < 8; i++) { await p.clock.runFor(500); await wait(150) } await p.clock.resume();
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
  ok('Do not disturb kept, then the bell 🔕', /🔕 .* dnd=true/.test(OUTS.std['qs-dnd']), OUTS.std['qs-dnd']);
  ok('a notification dismissed, then all cleared', /notes=1$/.test(OUTS.std['nc-x']) && /notes=0$/.test(OUTS.std['nc-clear']), [OUTS.std['nc-x'], OUTS.std['nc-clear']]);
  fs.writeFileSync(path.join(OUT, 'traybeh.json'), JSON.stringify(OUTS, null, 1));
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('FAIL ' + e.stack); if (NP) NP.kill(); process.exit(1) });
