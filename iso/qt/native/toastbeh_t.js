// 1.6.0 (plan Part 12, item 3, "The rule": Native works as Standard): the notifications' pop-ups and the call's 🎙️ used the
// same way on both engines and each step's outcome compared: a page's notification (rudolfd's "memory closed" event, as
// apps57.js tells it) shown in the screen's corner, its click doing the notification's own action (Settings), one going by
// itself after its time, none shown with Do not disturb but counted on the bell, the 🎙️ of an app using the microphone
// muting it for everything and back.
// Run by same_t.sh with SAME_SCRIPT=toastbeh_t.js.   node toastbeh_t.js RUDOLF-NATIVE DIR OUT PORT DISPLAY
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path'), cp = require('child_process');
const [BIN, D, OUT, PORT, DSP] = process.argv.slice(2);
const W = 1920, H = 1080, BASE = 'http://127.0.0.1:' + PORT, TOK = {'X-Rudolf-Token': 'tok160same'};
const KEYS = JSON.parse(fs.readFileSync(path.join(D, 'keys.json'))), PW = KEYS.Ann.pw;
const ST = require('./same_state.js');
const wait = ms => new Promise(r => setTimeout(r, ms));
const xdo = a => cp.execSync('xdotool ' + a, {env: Object.assign({}, process.env, {DISPLAY: DSP, LC_ALL: 'C.UTF-8'})}).toString();
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 700) : '')) };
fs.mkdirSync(OUT, {recursive: true});
const ev = e => fs.writeFileSync(path.join(D, 'event'), e);
const micMuted = async () => (await (await fetch(BASE + '/api/mic', {headers: TOK})).json()).muted ? 'mic muted' : 'mic on';
const STEPS = ['toast', 'click', 'toast2', 'by-itself', 'dnd', 'mic', 'mic-click', 'mic-click2'];
let OPART = null;
async function run(e) {
  const O = {}; OPART = O; await fetch(BASE + '/api/mic?muted=0', {method: 'POST', headers: TOK});
  for (const s of STEPS) {
    if (s == 'toast' || s == 'toast2') { ev('memory closed Paint 300'); await e.until(async () => (await e.titles()).includes('Paint was closed to free memory')) }
    else if (s == 'click') await e.click(await e.at('toast'));
    else if (s == 'by-itself') await wait(5500);
    else if (s == 'dnd') { await e.dnd(); ev('memory closed Calculator 2048'); await wait(4000) }
    else if (s == 'mic') { fs.writeFileSync(path.join(D, 'mic'), '["Oracle"]'); await e.until(async () => !!(await e.at('call')), 12000) }
    else if (/^mic-click/.test(s)) await e.click(await e.at('call'));
    await wait(1500); O[s] = ['toasts ' + await e.toasts() + ' ' + await e.titles(), await e.bell(), await e.apps(), await e.call(), await micMuted()].join(' · ');
    if (s == 'click') { await e.closeOthers(); await wait(3500) }   /* (rudolfd gives an event to the page asking first: not to a page closing) */
  }
  fs.writeFileSync(path.join(D, 'mic'), '[]');
  return O;
}
async function until0(f, ms) { const t = Date.now(); while (Date.now() - t < (ms || 20000)) { if (await f()) return true; await wait(200) } return false }
function std(p) {
  return {
    at: k => p.evaluate(k => { const q = e => { if (typeof e == 'string') e = document.querySelector(e); if (!e) return null; const r = e.getBoundingClientRect(); return [r.left + r.width / 2, r.top + r.height / 2] };
      if (k == 'toast') return q('#ntoasts .ntoast'); if (k == 'call') return q('#bar .tapp.call59'); return null }, k),
    click: async q => { if (q) await p.mouse.click(q[0], q[1]); await wait(300) },
    until: (f, ms) => until0(f, ms),
    toasts: () => p.evaluate(() => [...document.querySelectorAll('#ntoasts .ntoast')].filter(d => !d.classList.contains('out')).length),
    titles: () => p.evaluate(() => [...document.querySelectorAll('#ntoasts .ntoast b')].map(b => b.textContent).join('|')),
    bell: () => p.evaluate(() => { const b = document.getElementById('ncb'); return b.textContent + (b.dataset.n || '') }),
    apps: () => p.evaluate(() => wins.map(W => W.app).sort().join(',') || 'none'),
    call: () => p.evaluate(() => { const b = document.querySelector('#bar .tapp.call59'); return b ? b.textContent : 'no call' }),
    dnd: () => p.evaluate(() => { S.dnd = true; if (SC) SC.dnd = true; persistS(); ncBadge() }),
    closeOthers: () => p.evaluate(() => wins.filter(W => W.app != 'notepad').forEach(W => W.close()))
  };
}
let NP = null;
function nlines(k) { try { return fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } }
const nst = k => { const l = nlines(k)[0]; return l ? l.slice(k.length + 1) : null };
const nhit = k => { const l = nlines('hit').find(x => x.startsWith('hit ' + k + ' ')); if (!l) return null; const a = l.split(' ').slice(3).map(Number); return [a[0] + a[2] / 2, a[1] + a[3] / 2] };
function nat() {
  return {
    at: async k => { if (k == 'call') return nhit('call -'); if (k == 'toast') { const l = nlines('toast')[0]; if (!l) return null; const a = l.split(' '); return [+a[2] + +a[4] / 2, +a[3] + 20] } return null },
    click: async q => { if (q) xdo('mousemove ' + Math.round(q[0]) + ' ' + Math.round(q[1]) + ' click 1'); await wait(400) },
    until: (f, ms) => until0(f, ms),
    toasts: async () => nlines('toast').length,
    titles: async () => nlines('toast').map(l => l.slice(l.indexOf('|') + 1)).join('|'),
    bell: async () => { const n = (nst('notes') || '0 0 0').split(' '); return (n[0] == '1' ? '🔕' : '🔔') + (n[1] != '0' ? n[1] : '') },
    apps: async () => { const l = nlines('client').map(x => (x.match(/ app=(\S+)/) || [])[1]).filter(k => k && k != '-' && !/^do-/.test(k)); return l.sort().join(',') || 'none' },
    call: async () => { const m = (nst('mic') || '0 0').split(' '); return m[0] == '0' ? 'no call' : m[1] == '1' ? '🔇' : '🎙️' },
    dnd: async () => { xdo('mousemove ' + nhit('qs -').map(Math.round).join(' ') + ' click 1'); await until0(() => (nst('fly') || '').startsWith('qs'), 5000);
      const l0 = nlines('flybtn').find(x => x.startsWith('flybtn qs:2 ')); if (!l0) throw new Error('no quick settings: ' + fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').filter(x => /^(fly|hit qs|client|toast|menu|locked)/.test(x)).join(' / ')); const l = l0.split(' '); xdo('mousemove ' + l[2] + ' ' + l[3] + ' click 1'); await wait(500); xdo('key Escape'); xdo('mousemove 700 400 click 1'); await wait(3000) },
    closeOthers: async () => { for (const l of nlines('client')) { const a = l.split(' '); if (!/ app=(-|do-|notepad)/.test(l)) xdo('mousemove ' + (+a[3] + +a[5] + 2 - 9 - 12) + ' ' + (+a[4] + 7 + 11) + ' click 1') } await until0(() => !nlines('client').some(l => !/ app=(-|do-|notepad)/.test(l)), 15000); await wait(800) }
  };
}
(async () => {
  const OUTS = {}; fs.writeFileSync(path.join(D, 'mic'), '[]');
  // Standard
  ST.state(D, {}, {});
  { const br = await chromium.launch(); const ctx = await br.newContext({viewport: {width: W, height: H}, timezoneId: 'UTC'});
    await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => /generate_204/.test(r.request().url()) ? r.fulfill({status: 204, body: ''}) : r.abort());
    const p = await ctx.newPage(); await p.goto(BASE + '/'); await p.waitForFunction(() => typeof userLogin == 'function' && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
    await wait(1000); await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
    await p.waitForFunction(() => USER && getComputedStyle(document.getElementById('login')).display == 'none', null, {timeout: 60000}); await wait(4000);
    await p.evaluate(() => launch('notepad')); await wait(1500);
    OUTS.std = await run(std(p)); await br.close() }
  // Native
  ST.state(D, {}, {}); try { fs.unlinkSync(path.join(D, 'session')) } catch (e) {}
  const env = Object.assign({}, process.env, {DISPLAY: DSP, HOME: D, RUDOLF_NATIVE_STATE: path.join(D, 'state'), LIBGL_ALWAYS_SOFTWARE: '1', TZ: 'UTC'});
  const lg = fs.openSync(path.join(OUT, 'native.log'), 'a'); NP = cp.spawn(BIN, ['--size=' + W + 'x' + H, '--draw=soft', '--base=' + BASE], {env, stdio: ['ignore', lg, lg], cwd: D});
  await until0(() => nst('signin') == '1', 40000); await wait(2000); xdo("type --delay 40 '" + PW.replace(/'/g, "'\\''") + "'"); xdo('key Return');
  await until0(() => nst('user') == 'Ann' && nst('signin') == '0', 30000); await wait(3000);
  { const h = nhit('pin notepad'); xdo('mousemove ' + h.map(Math.round).join(' ') + ' click 1'); await until0(() => nlines('client').some(l => / app=notepad /.test(l)), 30000); await wait(3000) }
  OUTS.nat = await run(nat()); NP.kill();
  for (const s of STEPS) ok(s + ': ' + OUTS.std[s], OUTS.std[s] == OUTS.nat[s], {std: OUTS.std[s], nat: OUTS.nat[s]});
  ok('the toast\'s click opened Settings; none with Do not disturb; the microphone muted, then not', /settings/.test(OUTS.std.click) && /^toasts 0 +· 🔕/.test(OUTS.std.dnd) && /🔇 · mic muted$/.test(OUTS.std['mic-click']) && /🎙️ · mic on$/.test(OUTS.std['mic-click2']), OUTS.std);
  fs.writeFileSync(path.join(OUT, 'toastbeh.json'), JSON.stringify(OUTS, null, 1));
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('FAIL ' + e.stack, JSON.stringify(OPART)); if (NP) NP.kill(); process.exit(1) });
