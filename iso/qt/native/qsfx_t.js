// 1.6.0 ("The rule": Native works as Standard): quick settings' tiles and buttons, the power key, the hardware keys and the global
// shortcuts, each used on both engines with two app windows open (Notepad and Calculator) and each outcome read: in every open
// window's page (html.dark, #nightl, html.sr, the magnifier's zoom, the contrast filter, the on-screen keyboard, html.touch, S.dnd),
// in the shell (rudolf-native's state: the screen's colour (the X screen's gamma ramp, xrandr), the magnifier, the on-screen keyboard,
// the lock screen over every window, the power dialog, the indicator, Task Manager), and in rudolfd (the stand-in's calls: airplane,
// volume, brightness). The keys are sent with xdotool with each kind of window in front (the desktop, an app's window), and rudolfd's
// events (the power key, the volume and brightness keys' "osd" lines, Ctrl+Alt+Del's "taskmgr") as rudolf-display writes them: the
// dialog or the indicator at once, once, drawn by the shell above every window and never inside an app's window, and an app window's
// page never taking an event. Run by same_t.sh with SAME_SCRIPT=qsfx_t.js.   node qsfx_t.js RUDOLF-NATIVE DIR OUT PORT DISPLAY
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path'), cp = require('child_process');
const [BIN, D, OUT, PORT, DSP] = process.argv.slice(2);
const W = 1920, H = 1080, BASE = 'http://127.0.0.1:' + PORT;
const KEYS = JSON.parse(fs.readFileSync(path.join(D, 'keys.json'))), PW = KEYS.Ann.pw;
const ST = require('./same_state.js'), ACCT = ST.ACCT;
const wait = ms => new Promise(r => setTimeout(r, ms));
const env0 = Object.assign({}, process.env, {DISPLAY: DSP, LC_ALL: 'C.UTF-8'});
const xdo = a => cp.execSync('xdotool ' + a, {env: env0}).toString();
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 900) : '')) };
const ONLY = process.env.QSFX_ONLY || '';   /* (diagnosis: QSFX_REPORT=1 prints what each engine did without failing) */
fs.mkdirSync(OUT, {recursive: true});
const MORE = {['bd95pins:' + ACCT]: ['explorer', 'notepad', 'calc', 'settings']};
const TILES = ['Dark mode', 'Night light', 'Do not disturb', 'Screen reader', 'Magnifier', 'High contrast', 'On-screen keyboard', 'Touch mode', 'Airplane mode'];
/* a page's outcome: what the tile changes in a page on Standard */
const SIG = `(()=>{const h=document.documentElement,n=document.getElementById('nightl'),v=document.getElementById('vkb');
 return JSON.stringify({dark:h.classList.contains('dark'),nl:!!(n&&getComputedStyle(n).display!='none'&&+getComputedStyle(n).opacity>0),dnd:!!S.dnd,sr:h.classList.contains('sr'),
  mag:/scale/.test(document.body.style.transform||''),hc:/contrast|saturate/.test(h.style.filter||''),vk:!!(v&&v.style.display=='block'),touch:h.classList.contains('touch'),
  shellA11y:h.classList.contains('oashella11y')})})()`;
const calls = () => { try { return fs.readFileSync(path.join(D, 'calls'), 'utf8') } catch (e) { return '' } };
function gamma() { try { const o = cp.execSync('xrandr --verbose', {env: env0}).toString(); const m = o.match(/Gamma:\s*([\d.]+):([\d.]+):([\d.]+)/); return m ? [+m[1], +m[2], +m[3]] : null } catch (e) { return null } }
let NP = null;
function nlines(k) { try { return fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } }
const nst = k => { const l = nlines(k)[0]; return l ? l.slice(k.length + 1) : null };
const nhit = k => { const l = nlines('hit').find(x => x.startsWith('hit ' + k + ' ')); if (!l) return null; const a = l.split(' ').slice(3).map(Number); return [a[0] + a[2] / 2, a[1] + a[3] / 2] };
async function until(f, ms) { const t = Date.now(); while (Date.now() - t < (ms || 20000)) { if (await f()) return true; await wait(200) } return false }
let TEV = null;
function TEVAL() { if (!TEV) { TEV = path.join(D, 'teval'); try { fs.unlinkSync(TEV) } catch (e) {} cp.execSync('mkfifo ' + TEV) } return TEV }
async function neval(k, js) {
  const o = TEV + '.out', got = () => { try { return fs.readFileSync(o, 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } }, n0 = got().length;
  const fd = fs.openSync(TEV, fs.constants.O_RDWR | fs.constants.O_NONBLOCK); fs.writeSync(fd, k + ' ' + js.replace(/\n/g, ' ') + '\n'); fs.closeSync(fd);
  await until(() => got().length > n0, 8000); const r = got()[n0]; return r == null ? null : r.slice(k.length + 1);
}
const QSL = ['Dark mode', 'Night light', 'Do not disturb', 'Screen reader', 'Magnifier', 'High contrast', 'On-screen keyboard', 'Touch mode', 'Airplane mode', 'Mobile data'];
const flybtn = k => { const l = nlines('flybtn').find(x => x.split(' ')[1] == k); if (!l) return null; const a = l.split(' '); return [+a[2], +a[3]] };
const nclick = c => { if (c) xdo('mousemove ' + Math.round(c[0]) + ' ' + Math.round(c[1]) + ' click 1') };
const shot = n => { try { cp.execSync('import -window root ' + path.join(OUT, n + '.png'), {env: env0}) } catch (e) {} };
/* rudolfd's event, as rudolf-display writes it (DIR/event: the stand-in's queue) */
const event = t => fs.writeFileSync(path.join(D, 'event'), t + '\n');
const evLeft = () => fs.existsSync(path.join(D, 'event'));
async function pageSigs() { const o = {}; for (const k of ['notepad', 'calc']) { const r = await neval(k, SIG); try { o[k] = JSON.parse(r) } catch (e) { o[k] = r } } return o }
/* what each window shows on Native: every window and the shell together, as Standard's one page */
function effect(t, sigs, sh) {
  const all = f => Object.values(sigs).every(s => s && typeof s == 'object' && f(s));
  switch (t) {
    case 'Dark mode': return all(s => s.dark) && sh.dark == '1';
    case 'Night light': return (sh.gammaOk && sh.gammaOn && sh.gamma && sh.gamma.some(g => Math.abs(g - 1) > .01) && all(s => !s.nl)) || (!sh.gammaOk && all(s => s.nl));
    case 'Do not disturb': return all(s => s.dnd) && sh.dnd == '1';
    case 'Screen reader': return all(s => s.sr) && sh.sr == '1';
    case 'Magnifier': return (sh.mag == '1' && all(s => !s.mag)) || (sh.mag != '1' && all(s => s.mag));
    case 'High contrast': return all(s => s.hc) && sh.hc == '1' && (!sh.gammaOk || (sh.gammaOn && sh.gamma && sh.gamma.some(g => Math.abs(g - 1) > .01)));
    case 'On-screen keyboard': return sh.vk == '1' && all(s => !s.vk);
    case 'Touch mode': return all(s => s.touch) && sh.touch == '1';
    case 'Airplane mode': return /POST mobile\/airplane\?on=1/.test(sh.calls);
  }
}
function offEffect(t, sigs, sh) {
  const all = f => Object.values(sigs).every(s => s && typeof s == 'object' && f(s));
  switch (t) {
    case 'Dark mode': return all(s => !s.dark) && sh.dark == '0';
    case 'Night light': return !sh.gammaOn && (!sh.gamma || sh.gamma.every(g => Math.abs(g - 1) <= .01)) && all(s => !s.nl);
    case 'Do not disturb': return all(s => !s.dnd) && sh.dnd == '0';
    case 'Screen reader': return all(s => !s.sr) && sh.sr == '0';
    case 'Magnifier': return sh.mag != '1' && all(s => !s.mag);
    case 'High contrast': return all(s => !s.hc) && sh.hc == '0';
    case 'On-screen keyboard': return sh.vk != '1' && all(s => !s.vk);
    case 'Touch mode': return all(s => !s.touch) && sh.touch == '0';
    case 'Airplane mode': return /POST mobile\/airplane\?on=0/.test(sh.calls);
  }
}
const shell = c0 => { const a = {}; for (const kv of (nst('a11y') || '').split(' ')) { const [k, v] = kv.split('='); if (k) a[k] = v } a.gamma = gamma(); const g = (nst('gamma') || '0 0').split(' '); a.gammaOk = g[0] == '1'; a.gammaOn = g[1] == '1'; a.calls = calls().slice(c0); return a };
async function openApps() {
  for (const k of ['notepad', 'calc']) { nclick(nhit('pin ' + k)); await until(() => nlines('client').some(l => / app=/.test(l) && l.includes('app=' + k)), 30000); await wait(2500) }
}
async function nat() {
  const O = {};
  await openApps();
  O.before = await pageSigs();
  ok('Native: two app windows open, their pages read', O.before.notepad && O.before.calc && typeof O.before.notepad == 'object', O.before);
  /* ---- A: each tile on and off, with the two windows open */
  for (const t of TILES) {
    if (ONLY && !ONLY.split(',').includes(t)) continue;
    for (const on of [1, 0]) {
      nclick(nhit('qs -')); await until(() => (nst('fly') || '').startsWith('qs'), 5000); await wait(500);
      const c0 = calls().length; nclick(flybtn('qs:' + QSL.indexOf(t))); await wait(500); xdo('key Escape'); nclick([W / 2, 300]); await wait(9000);
      const sigs = await pageSigs(), sh = shell(c0); O[t + (on ? ' on' : ' off')] = {sigs, sh};
      ok('Native: ' + t + (on ? ' on' : ' off') + ': the setting, the tile, every window and the shell', on ? effect(t, sigs, sh) : offEffect(t, sigs, sh), {sigs, sh: Object.assign({}, sh, {calls: undefined})});
      if (on) shot('tile-' + t.replace(/ /g, '-'));
      if (on && t == 'On-screen keyboard') {   /* a key of the shell's keyboard typed into the window in front (Notepad's text) */
        nclick(nhit('pin notepad')); await wait(1500); await neval('notepad', `(()=>{const t=document.querySelector('textarea');t.value='';t.focus();return 1})()`); await wait(500);
        const kb = nlines('vkkey').map(l => l.split(' ')), k = kb.find(a => a[1] == 'q') || null;
        if (k) { xdo('mousemove ' + k[2] + ' ' + k[3] + ' click 1'); await wait(1500) }
        const v = await neval('notepad', `document.querySelector('textarea').value`);
        ok('Native: the on-screen keyboard\'s "q" types into the window in front (Notepad), which keeps the keyboard', v == 'q' && nlines('client').some(l => l.includes('app=notepad') && / focus=1 /.test(l)), {v, k, vk: nlines('vkkey').length});
      }
    }
  }
  /* the tile shows its state: quick settings open again after Dark mode on */
  /* ---- the buttons: Settings (its own window), Lock (over every window), Power (the dialog, its choices) */
  nclick(nhit('qs -')); await until(() => (nst('fly') || '').startsWith('qs'), 5000); await wait(400); nclick(flybtn('settings'));
  ok('Native: quick settings › Settings opens Settings in its own window', await until(() => nlines('client').some(l => l.includes('app=settings')), 30000));
  await wait(2000);
  nclick(nhit('qs -')); await until(() => (nst('fly') || '').startsWith('qs'), 5000); await wait(400); nclick(flybtn('power'));
  await until(() => nst('power') == '1', 5000); O.power = nst('powerrows');
  ok('Native: quick settings › Power shows the power dialog above every window, with Standard\'s choices', nst('power') == '1' && nst('powertop') == '1', {power: nst('power'), top: nst('powertop'), rows: O.power});
  xdo('key Escape'); await wait(300); if (nst('power') == '1') { const b = (nst('powerbtn') || '').split(' '); }
  nclick(nhit('qs -')); await until(() => (nst('fly') || '').startsWith('qs'), 5000); await wait(400); nclick(flybtn('lock'));
  await until(() => nst('signin') == '1', 8000); await wait(800);
  ok('Native: quick settings › Lock locks the session above every window', nst('signin') == '1' && nst('locked') == '1' && nst('signtop') == '1', {signin: nst('signin'), locked: nst('locked'), top: nst('signtop')});
  xdo("type --delay 40 '" + PW.replace(/'/g, "'\\''") + "'"); xdo('key Return'); await until(() => nst('signin') == '0', 20000); await wait(1500);
  /* ---- B: rudolfd's events and the keys, with each kind of window in front */
  const fronts = [['the desktop', async () => { nclick([W / 2, 200]); xdo('key super+d'); await wait(800) }], ['an app\'s window', async () => { nclick(nhit('pin notepad')); await wait(1200) }]];
  for (const [fn, front] of fronts) {
    await front();
    const pv0 = await neval('notepad', `document.querySelectorAll('.rmodal').length`);
    event('power ' + Date.now()); await until(() => nst('power') == '1', 3000);
    const got = nst('power') == '1', top = nst('powertop') == '1', inPage = await neval('notepad', `document.querySelectorAll('.rmodal').length`);
    ok('Native, ' + fn + ' in front: the power key shows the power dialog at once, above every window, never in a page', got && top && !evLeft() && inPage == pv0, {got, top, left: evLeft(), inPage, pv0});
    xdo('key Escape'); await wait(500);
    for (const [ev, k, lv] of [['osd volume 45', 'volume', '45'], ['osd volume muted', 'volume', '0'], ['osd brightness 60', 'brightness', '60'], ['osd mic muted', 'mic', '']]) {
      event(ev); await until(() => (nst('osd') || '-') != '-', 3000);
      const o = nst('osd') || '-'; ok('Native, ' + fn + ' in front: "' + ev + '" shows Rudolf\'s indicator at once', o.startsWith(k) && !evLeft(), {o});
      await until(() => (nst('osd') || '-') == '-', 4000);
    }
    event('taskmgr'); const tm = await until(() => (nlines('client').some(l => l.includes('app=taskmgr') && / focus=1 /.test(l)) && nlines('ontop').some(l => l == 'ontop taskmgr')), 20000);
    ok('Native, ' + fn + ' in front: Ctrl+Alt+Del (rudolf-display\'s "taskmgr") opens Task Manager above every window', tm && !evLeft(), nlines('client'));
    await wait(1500);
  }
  /* the events an app window's page needs (a drive, Wi-Fi) reach one page, once */
  const PG = ['notepad', 'calc', 'settings', 'taskmgr'];
  for (const k of PG) await neval(k, `(()=>{window.__evN=0;evOn(t=>{if(/^probe /m.test(t))__evN++});return 1})()`);
  event('probe 1'); await wait(4000);
  { const n = []; for (const k of PG) n.push(+(await neval(k, `window.__evN`)) || 0); ok('Native: an event that is a page\'s reaches one app window\'s page, once', n.reduce((a, b) => a + b, 0) == 1 && !evLeft(), n) }
  /* the keys' indicator after the volume key (the shell's own, at the top of the screen) */
  /* ---- the keys as X sends them (QEMU's sendkey, xdotool): the shell takes Rudolf's own, whichever window is in front */
  nclick(nhit('pin notepad')); await wait(1000);
  for (const [key, want] of [['XF86AudioRaiseVolume', /POST volume\?step=up/], ['XF86AudioLowerVolume', /POST volume\?step=down/], ['XF86AudioMute', /POST volume\?step=mute/], ['XF86MonBrightnessUp', /POST brightness\/step\?d=up/], ['XF86MonBrightnessDown', /POST brightness\/step\?d=down/]]) {
    await until(() => !nst('osd'), 4000);   /* (the last key's indicator gone first) */
    const c0 = calls().length; xdo('key ' + key); await until(() => /^(volume|brightness) /.test(nst('osd') || '') && want.test(calls().slice(c0)), 1400);
    ok('Native, an app\'s window in front: ' + key + ' changes it through rudolfd, with Rudolf\'s indicator', want.test(calls().slice(c0)) && /^(volume|brightness) /.test(nst('osd') || '') , [calls().slice(c0).split('\n').filter(l => /volume|bright/.test(l)), nst('osd')]);
    if (key == 'XF86MonBrightnessUp') shot('key-brightness');
  }
  { xdo('key XF86PowerOff'); const g = await until(() => nst('power') == '1', 3000); ok('Native, an app\'s window in front: XF86PowerOff shows the power dialog', g); xdo('key Escape'); await wait(400) }
  { xdo('key ctrl+alt+Delete'); const g = await until(() => (nlines('client').some(l => l.includes('app=taskmgr') && / focus=1 /.test(l)) && nlines('ontop').some(l => l == 'ontop taskmgr')), 15000); ok('Native, an app\'s window in front: Ctrl+Alt+Delete opens Task Manager above every window', g) }
  { xdo('key ctrl+shift+Escape'); const g = await until(() => nlines('client').some(l => l.includes('app=taskmgr')), 15000); ok('Native: Ctrl+Shift+Esc opens Task Manager', g) }
  /* the brightness button on the taskbar (rudolfd's info says there is a backlight) */
  ok('Native: the taskbar has the brightness button', !!nhit('bri -'), nlines('hit').map(l => l.split(' ')[1]).join(','));
  { nclick(nhit('bri -')); await until(() => (nst('fly') || '').startsWith('bri'), 5000); await wait(500); shot('bri-popup');
    const r = flybtn('range'), b0 = +fs.readFileSync(path.join(D, 'backlight'), 'utf8'); if (r) { const f = nlines('fly')[0].split(' '); xdo('mousemove ' + (r[0] - 50) + ' ' + r[1] + ' click 1') } await wait(1500);
    const b1 = +fs.readFileSync(path.join(D, 'backlight'), 'utf8');
    ok('Native: the brightness button\'s pop-up (its range) sets the brightness', (nst('fly') || '').startsWith('bri') && r && b1 != b0, {fly: nst('fly'), b0, b1}); xdo('key Escape'); nclick([W / 2, 300]); await wait(800) }
  /* the power dialog with Windows on the computer: "Restart into Windows" after Restart, as boot30.js adds it */
  fs.writeFileSync(path.join(D, 'bootmenu'), JSON.stringify({windows: [{n: '0001', title: 'Windows Boot Manager'}]}));
  event('power ' + Date.now()); await until(() => nst('power') == '1', 8000); await wait(600); shot('power-windows');
  ok('Native: the power dialog has Standard\'s choices, Restart into Windows too', nst('powerrows') == '|⏻ Shut down|↻ Restart|⊞ Restart into Windows|☾ Sleep|Cancel', nst('powerrows'));
  xdo('key Escape'); await wait(500); ok('Native: Escape is the power dialog\'s Cancel', nst('power') == '0');
  return O;
}
/* Standard (the reference, unchanged): the desktop page in Chromium hears the same events (the power key, the keys' indicator, Ctrl+Alt+Del)
   and its quick settings' tiles change the page */
async function std() {
  const br = await chromium.launch(); const ctx = await br.newContext({viewport: {width: W, height: H}, timezoneId: 'UTC'});
  await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => r.abort());
  const p = await ctx.newPage(); await p.goto(BASE + '/'); await p.waitForFunction(() => typeof userLogin == 'function' && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
  await wait(1000); await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
  await p.waitForFunction(() => USER && getComputedStyle(document.getElementById('login')).display == 'none', null, {timeout: 60000}); await wait(3000);
  event('power ' + Date.now()); const pw = await p.waitForFunction(() => document.querySelectorAll('.rmodal').length > 0, null, {timeout: 8000}).then(() => true, () => false);
  ok('Standard: the power key\'s event shows the page\'s power dialog', pw);
  await p.evaluate(() => document.querySelectorAll('.rmodal').forEach(e => e.remove()));
  event('osd volume 45'); const os = await p.waitForFunction(() => { const o = document.getElementById('osd'); return o && o.style.display != 'none' && /45%/.test(o.textContent) }, null, {timeout: 8000}).then(() => true, () => false);
  ok('Standard: "osd volume 45" shows the page\'s indicator', os);
  event('taskmgr'); const tm = await p.waitForFunction(() => wins.some(w => w.app == 'taskmgr' && w.el.classList.contains('ontop')), null, {timeout: 15000}).then(() => true, () => false);
  ok('Standard: Ctrl+Alt+Del\'s "taskmgr" opens Task Manager above every window', tm);
  const T = {}; for (const t of TILES.slice(0, 8)) { await p.evaluate(t => { qsToggle(); const b = [...document.querySelectorAll('#qsp .qst')].find(x => x.querySelector('span').textContent == t); b.click(); qsClose() }, t); await wait(300); T[t] = JSON.parse(await p.evaluate(SIG)) }
  ok('Standard: each tile changes the page as before (dark, night light, dnd, screen reader, magnifier, high contrast, keyboard, touch)', T['Dark mode'].dark && T['Night light'].nl && T['Do not disturb'].dnd && T['Screen reader'].sr && T['Magnifier'].mag && T['High contrast'].hc && T['On-screen keyboard'].vk && T['Touch mode'].touch, T);
  await br.close();
}
(async () => {
  if (!process.env.QSFX_NOSTD) { ST.state(D, {}, MORE); try { await std() } catch (e) { ok('Standard ran: ' + e.message, false) } }
  ST.state(D, {}, MORE); try { fs.unlinkSync(path.join(D, 'session')) } catch (e) {}
  fs.writeFileSync(path.join(D, 'backlight'), '70');   /* (the stand-in rudolfd: a screen with a backlight) */
  if (!process.env.QSFX_BEFORE) fs.writeFileSync(path.join(D, 'wm-native'), '');   /* (rudolfd on the native desktop: its events the shell's; QSFX_BEFORE: as before 1.6.0's fix) */
  const env = Object.assign({}, process.env, {DISPLAY: DSP, HOME: D, RUDOLF_NATIVE_STATE: path.join(D, 'state'), RUDOLF_NATIVE_TEST_EVAL: TEVAL(), LIBGL_ALWAYS_SOFTWARE: '1', TZ: 'UTC'});
  for (const k of ['http_proxy', 'https_proxy', 'HTTP_PROXY', 'HTTPS_PROXY', 'all_proxy', 'ALL_PROXY']) delete env[k];
  const lg = fs.openSync(path.join(OUT, 'native.log'), 'a'); NP = cp.spawn(BIN, ['--size=' + W + 'x' + H, '--draw=soft', '--base=' + BASE], {env, stdio: ['ignore', lg, lg], cwd: D});
  await until(() => nst('signin') == '1', 40000); await wait(2000); xdo("type --delay 40 '" + PW.replace(/'/g, "'\\''") + "'"); xdo('key Return');
  await until(() => nst('user') == 'Ann' && nst('signin') == '0', 30000); await wait(3000);
  let O = {}; try { O = await nat() } catch (e) { ok('Native ran: ' + e.message, false) } NP.kill();
  fs.writeFileSync(path.join(OUT, 'qsfx.json'), JSON.stringify(O, null, 1));
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('FAIL ' + e.stack); if (NP) NP.kill(); process.exit(1) });
