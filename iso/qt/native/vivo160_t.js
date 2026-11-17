// 1.6.0 (the owner's Vivobook report of 2026-11-16, beta 2026111620, Native; "The rule": Native appears and works as Standard), checked
// on both engines where Standard has the same thing, with timings:
//  1. an app's window never shows the sign-in screen or the whole desktop page at a large size: each window watched from the moment the
//     shell makes it (fixtures/wincap.c, every 40 ms) is on the screen only at its own size, its page showing either the app as it ends up
//     or a plain colour, never anything else; it opens (N15) at that size
//  2. the desktop's menu, then a second right-click elsewhere (menus of different sizes: the desktop's, an icon's, the taskbar's; the
//     start menu replaced by the taskbar's menu): no part of the first menu left on the screen, in the new menu's own picture or beside it
//  3. the desktop's icons after signing in within Standard's time (and a second); New folder, New text document, a file saved to the
//     Desktop folder by an app and a file deleted there on the desktop within 500 ms (Standard: at once)
//  4. a background chosen in Settings › Display Properties on the desktop within 300 ms, before Apply, as Standard previews it; back on
//     Revert, and when Settings is closed without applying
// Run by same_t.sh with SAME_SCRIPT=vivo160_t.js.   node vivo160_t.js RUDOLF-NATIVE DIR OUT PORT DISPLAY
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path'), cp = require('child_process');
const [BIN, D, OUT, PORT, DSP] = process.argv.slice(2);
const W = +(process.env.SAME_W || 1920), H = +(process.env.SAME_H || 1080), BASE = 'http://127.0.0.1:' + PORT;
const KEYS = JSON.parse(fs.readFileSync(path.join(D, 'keys.json'))), PW = KEYS.Ann.pw;
const ST = require('./same_state.js');
const wait = ms => new Promise(r => setTimeout(r, ms));
const env0 = Object.assign({}, process.env, {DISPLAY: DSP, LC_ALL: 'C.UTF-8'});
const xdo = a => cp.execSync('xdotool ' + a, {env: env0}).toString();
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 900) : '')) };
const T = {};   /* the timings, written to OUT/timings.json */
fs.mkdirSync(OUT, {recursive: true});
let NP = null;
function nlines(k) { try { return fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } }
const nst = k => { const l = nlines(k)[0]; return l ? l.slice(k.length + 1) : null };
const nhit = k => { const l = nlines('hit').find(x => x.startsWith('hit ' + k + ' ')); if (!l) return null; const a = l.split(' ').slice(3).map(Number); return [a[0] + a[2] / 2, a[1] + a[3] / 2] };
async function until(f, ms, step) { const t = Date.now(); while (Date.now() - t < (ms || 20000)) { if (await f()) return true; await wait(step || 20) } return false }
let TEV = null;
function TEVAL() { if (!TEV) { TEV = path.join(D, 'teval'); try { fs.unlinkSync(TEV) } catch (e) {} cp.execSync('mkfifo ' + TEV) } return TEV }
async function neval(k, js) {
  const o = TEV + '.out', got = () => { try { return fs.readFileSync(o, 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } }, n0 = got().length;
  const fd = fs.openSync(TEV, fs.constants.O_RDWR | fs.constants.O_NONBLOCK); fs.writeSync(fd, k + ' ' + js.replace(/\n/g, ' ') + '\n'); fs.closeSync(fd);
  await until(() => got().length > n0, 8000, 50); const r = got()[n0]; return r == null ? null : r.slice(k.length + 1);
}
const nclick = (c, b) => { if (c) xdo('mousemove ' + Math.round(c[0]) + ' ' + Math.round(c[1]) + ' click ' + (b || 1)) };
const shot = n => { const f = path.join(OUT, n + '.png'); try { cp.execSync('import -window root ' + f, {env: env0}) } catch (e) {} return f };
const clientOf = k => nlines('client').find(l => l.includes(' app=' + k + ' '));
const dicons = () => nlines('dicon').map(l => l.split(' ').slice(7).join(' '));
const hasIcon = n => dicons().some(x => x.endsWith('|' + n));
/* (pictures compared with python's PIL: in BOX (x y w h) less EX (x0 y0 x1 y1), the pixels differing from REF by more than 60 in a
   channel: what is left of an old menu (its words, its border, its ring) is far brighter than the desktop under it; the new menu's
   shadow darkens by less than that) */
const PYCMP = `import sys
from PIL import Image
a=Image.open(sys.argv[1]).convert('RGB').load();b=Image.open(sys.argv[2]).convert('RGB').load()
x,y,w,h=[int(v) for v in sys.argv[3:7]];e=[int(v) for v in sys.argv[7:11]];n=t=0
for j in range(y,y+h):
  for i in range(x,x+w):
    if e[0]<=i<e[2] and e[1]<=j<e[3]: continue
    t+=1;p=a[i,j];q=b[i,j]
    if max(abs(p[0]-q[0]),abs(p[1]-q[1]),abs(p[2]-q[2]))>60: n+=1
print(n,t)`;
const leftover = (ref, now, box, ex) => { try { const [n, t] = cp.execFileSync('python3', ['-c', PYCMP, ref, now, ...box.map(Math.round), ...ex.map(Math.round)]).toString().trim().split(' ').map(Number); return {n, t} } catch (e) { return {n: -1, t: 0, e: String(e)} } };
const PYPX = `import sys
from PIL import Image
a=Image.open(sys.argv[1]).convert('RGB').load();print(*a[int(sys.argv[2]),int(sys.argv[3])])`;
const px = (f, x, y) => { try { return cp.execFileSync('python3', ['-c', PYPX, f, String(x), String(y)]).toString().trim().split(' ').map(Number) } catch (e) { return [-1, -1, -1] } };
const far = (p, q, t) => Math.max(...p.map((v, i) => Math.abs(v - q[i]))) > (t || 24);

/* ---- Standard (the reference, unchanged) */
async function std() {
  const br = await chromium.launch(); const ctx = await br.newContext({viewport: {width: W, height: H}, timezoneId: 'UTC'});
  await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => r.abort());
  const p = await ctx.newPage(); await p.goto(BASE + '/'); await p.waitForFunction(() => typeof userLogin == 'function' && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
  await wait(1000); await p.focus('#lp'); await p.keyboard.type(PW);
  const t0 = Date.now(); await p.keyboard.press('Enter');
  const got = await p.waitForFunction(() => [...document.querySelectorAll('#desk > .ic')].filter(e => e.offsetWidth && getComputedStyle(e).display != 'none').length > 4, null, {timeout: 60000, polling: 20}).then(() => true, () => false);
  T.stdIcons = Date.now() - t0;
  ok('Standard: the desktop\'s icons after signing in (' + T.stdIcons + ' ms)', got, T.stdIcons);
  await wait(2500);
  /* New folder from the desktop's menu: on the desktop at once */
  for (const [item, name, key] of [['New folder', 'NEW FOLDER', 'stdNewFolder'], ['New text document', 'NEW TEXT DOCUMENT.TXT', 'stdNewText']]) {
    await p.mouse.click(W / 2, H / 3, {button: 'right'}); await wait(300);
    const t1 = Date.now(); await p.evaluate(i => [...document.querySelectorAll('.ctx .cmi')].find(e => e.querySelector('span').textContent.trim() == i).click(), item);
    const ic = await p.waitForFunction(n => !!document.querySelector(`#desk > .ic.fic[data-fn="${n}"]`), name, {timeout: 5000, polling: 10}).then(() => true, () => false);
    T[key] = Date.now() - t1; ok('Standard: ' + item + ' on the desktop (' + T[key] + ' ms)', ic, T[key]);
    await wait(200); await p.keyboard.press('Escape'); await p.evaluate(() => document.querySelectorAll('.rmodal').forEach(m => m.remove())); await wait(300);
  }
  /* a background chosen in Display Properties: the desktop at once, before Apply; back on Revert */
  await p.evaluate(() => settings('disp')); await wait(800);
  const bg0 = await p.evaluate(() => getComputedStyle(document.body).backgroundImage + getComputedStyle(document.body).backgroundColor);
  const t2 = Date.now(); await p.evaluate(() => document.querySelector('#dyns .dynb[data-dyn=teal]').click());
  const ch = await p.waitForFunction(b => getComputedStyle(document.body).backgroundImage + getComputedStyle(document.body).backgroundColor != b, bg0, {timeout: 3000, polling: 10}).then(() => true, () => false);
  T.stdPreview = Date.now() - t2; ok('Standard: a background chosen is the desktop\'s at once, before Apply (' + T.stdPreview + ' ms)', ch, T.stdPreview);
  await p.evaluate(() => secRevert('disp')); await wait(300);
  ok('Standard: ...back on Revert', await p.evaluate(b => getComputedStyle(document.body).backgroundImage + getComputedStyle(document.body).backgroundColor == b, bg0));
  await br.close();
}

/* ---- Native */
const WC = path.join(D, 'wincap');
/* a window watched from when the shell makes it: [{t, v, w, h, file}] */
async function watch(k, open) {
  const dir = path.join(OUT, 'win-' + k); fs.rmSync(dir, {recursive: true, force: true}); fs.mkdirSync(dir, {recursive: true});
  const pr = cp.spawn(WC, [path.join(D, 'state'), k, '40', '30', dir], {env: env0, stdio: 'ignore'});
  const done = new Promise(r => pr.on('exit', r)); await wait(50); open(); await Promise.race([done, wait(40000)]); try { pr.kill() } catch (e) {}
  let log = []; try { log = fs.readFileSync(path.join(dir, 'log'), 'utf8').trim().split('\n').map(l => { const a = l.split(' ').map(Number); return {i: a[0], t: a[1], v: a[2], w: a[3], h: a[4], file: path.join(dir, a[0] + '.ppm')} }) } catch (e) {}
  return log;
}
function ppm(f) { const b = fs.readFileSync(f); let o = 0, tok = []; while (tok.length < 4) { while (/\s/.test(String.fromCharCode(b[o]))) o++; let s = ''; while (!/\s/.test(String.fromCharCode(b[o]))) s += String.fromCharCode(b[o++]); tok.push(s) } o++; return {w: +tok[1], h: +tok[2], d: b.subarray(o)} }
/* the page's part of a picture (inside the frame's edge, below the title bar): plain (one colour), or as the last picture */
function judge(log, bw, th) {
  const shown = log.filter(s => s.v); if (!shown.length) return {err: 'never shown'};
  const fin = shown[shown.length - 1], F = ppm(fin.file), out = [];
  for (const s of shown) {
    if (s.w != fin.w || s.h != fin.h) { out.push({t: s.t, size: s.w + 'x' + s.h}); continue }
    const P = ppm(s.file); let diff = 0, n = 0, c0 = null, plain = true;
    for (let y = th; y < P.h - bw; y += 2) for (let x = bw; x < P.w - bw; x += 2) { const o = 3 * (y * P.w + x); const a = [P.d[o], P.d[o + 1], P.d[o + 2]], f = [F.d[o], F.d[o + 1], F.d[o + 2]];
      n++; if (Math.max(Math.abs(a[0] - f[0]), Math.abs(a[1] - f[1]), Math.abs(a[2] - f[2])) > 40) diff++;
      if (!c0) c0 = a; else if (Math.max(Math.abs(a[0] - c0[0]), Math.abs(a[1] - c0[1]), Math.abs(a[2] - c0[2])) > 6) plain = false }
    /* (as it ends up: under 1.5 % of its pixels otherwise, a caret or a hover) */
    if (!plain && diff / n > .015) out.push({t: s.t, differs: Math.round(1000 * diff / n) / 10 + '%'});
  }
  return {bad: out, first: shown[0].t, size: fin.w + 'x' + fin.h, samples: shown.length, before: log.filter(s => !s.v).map(s => s.w + 'x' + s.h).filter((x, i, a) => a.indexOf(x) == i)};
}
async function nat() {
  /* ---- 3 (sign-in): the icons. Standard's page is loaded behind its sign-in screen and shows them at once; the native shell has the
     account's catalogue from before (desktop/apps?acct=), and this sign-in's pages give it again; the first sign-in of an account has
     none yet, and waits for its first page */
  ok('Native: the first sign-in of an account (no catalogue yet): its icons once its first page is signed in (' + T.natFirst + ' ms, under 5 s)', T.natFirst >= 0 && T.natFirst < 5000, T.natFirst);
  ok('Native: the desktop\'s icons after signing in within Standard\'s time (' + T.natIcons + ' ms; Standard ' + T.stdIcons + ' ms; 300 ms allowed for the screen)', T.natIcons >= 0 && (T.stdIcons == null || T.natIcons <= T.stdIcons + 300), {nat: T.natIcons, std: T.stdIcons, n: dicons().length});
  ok('Native: ...each with its picture (the page\'s, else its emoji as Standard draws it: none blank)', nlines('dicpic').length > 4 && nlines('dicpic').every(l => l.split(' ')[1] != '0'), nlines('dicpic').filter(l => l.split(' ')[1] == '0'));
  await wait(2000);
  /* ---- 1: the app windows */
  for (const k of ['notepad', 'calc', 'settings', 'explorer']) {
    const log = await watch(k, () => nclick(nhit('pin ' + k)));
    await until(() => clientOf(k), 20000); await wait(400);
    const c = (clientOf(k) || '').split(' '), cw = +c[5], ch = +c[6];
    const last = log.filter(s => s.v).pop() || {w: 0, h: 0}, bw = Math.round((last.w - cw) / 2), th = last.h - ch - bw;
    const j = judge(log, bw, th); T['win_' + k] = j.first;
    ok('Native: ' + k + '\'s window first shown ' + j.first + ' ms after it was made, at its own size (' + j.size + '), never larger', !j.err && j.bad.length == 0 && !j.bad.some(b => b.size), j);
    ok('Native: ...its page never anything but the app as it ends up, or a plain colour (no sign-in screen, no desktop page)', !j.err && !j.bad.some(b => b.differs), j.bad);
    await neval(k, `(()=>{window.__oneAppClose();return 1})()`); await until(() => !clientOf(k), 10000); await wait(600);
  }
  /* ---- 2: menus replaced by others */
  xdo('mousemove ' + (W - 300) + ' ' + (H / 2)); await wait(500);
  const box = l => l.split(' ').slice(1, 5).map(Number);   /* cmenu X Y W H (the box) */
  const cmwin = b => [b[0] - 34, b[1] - 34, b[0] + b[2] + 34, b[1] + b[3] + 54];   /* (its window: the box, its ring and shadow, as cm_open) */
  async function twoMenus(name, first, second, firstBox) {
    xdo('key Escape'); await wait(400); const ref = shot('menu-' + name + '-ref');
    first(); await until(() => firstBox(), 5000); await wait(300); const b1 = firstBox();
    const c0 = nst('cmenu'); second(); await until(() => nst('cmenu') && nst('cmenu') != c0, 5000); await wait(150);
    const now = shot('menu-' + name), b2 = box('cmenu ' + nst('cmenu'));
    /* the old box less the new one's box and ring; and less what the new menu's own window is (its copy of the screen and its shadow: checked by the 60 levels) */
    const r = b1 ? leftover(ref, now, b1, [b2[0] - 5, b2[1] - 5, b2[0] + b2[2] + 5, b2[1] + b2[3] + 5]) : {n: -1};
    ok('Native: ' + name + ': nothing of the first menu left on the screen (' + (r.n) + ' of ' + r.t + ' pixels)', b1 && r.n == 0, {b1, b2, r});
    xdo('key Escape'); await wait(300);
  }
  await twoMenus('desktop-then-desktop', () => nclick([900, 300], 3), () => nclick([760, 260], 3), () => nst('cmenu') && box('cmenu ' + nst('cmenu')));
  const ic = nlines('dicon')[0].split(' ').slice(2, 6).map(Number);   /* dicon C R X Y W ... */
  await twoMenus('icon-then-desktop', () => nclick([ic[2] + 40, ic[3] + 30], 3), () => nclick([ic[2] + 150, ic[3] + 60], 3), () => nst('cmenu') && box('cmenu ' + nst('cmenu')));
  await twoMenus('desktop-then-taskbar', () => nclick([300, H - 420], 3), () => nclick([420, H - 20], 3), () => nst('cmenu') && box('cmenu ' + nst('cmenu')));
  await twoMenus('start-then-taskbar', () => nclick(nhit('start -')), () => nclick([200, H - 20], 3), () => nst('mbox') && nst('mbox').split(' ').map(Number));
  /* ---- 3: the Desktop folder's changes */
  for (const [item, name, key] of [['New folder', 'NEW FOLDER', 'natNewFolder'], ['New text document', 'NEW TEXT DOCUMENT.TXT', 'natNewText']]) {
    xdo('key Escape'); await wait(300); nclick([W / 2, H / 3], 3); await until(() => nlines('cmitem').some(l => l.endsWith('|' + item)), 5000);
    const l = nlines('cmitem').find(l => l.endsWith('|' + item)).split(' '); const t0 = Date.now(); nclick([+l[1], +l[2]]);
    const g = await until(() => hasIcon(name), 10000, 10); T[key] = Date.now() - t0;
    ok('Native: ' + item + ' on the desktop within 500 ms (' + T[key] + ' ms; Standard ' + T[key.replace('nat', 'std')] + ' ms)', g && T[key] <= 500, {t: T[key], icons: dicons()});
    ok('Native: ...then its Rename, as Standard\'s', await until(() => (nst('dialog') || '').startsWith('rename ' + name), 5000), nst('dialog'));
    xdo('key Escape'); await wait(400);
  }
  /* a file saved to the Desktop folder by an app (Notepad's page saving as Standard's saves), then deleted (File Explorer's) */
  nclick(nhit('pin notepad')); await until(() => clientOf('notepad'), 20000); await wait(1500);
  { const t0 = Date.now(); await neval('notepad', `(()=>{const d=deskFiles();d['SAVED.TXT']='hello';fsSave();EXPL.forEach(f=>f());return 1})()`);
    const g = await until(() => hasIcon('SAVED.TXT'), 10000, 10); T.natSaved = Date.now() - t0;
    ok('Native: a file an app saves to the Desktop folder on the desktop within 500 ms (' + T.natSaved + ' ms)', g && T.natSaved <= 500, {t: T.natSaved, icons: dicons()}) }
  { const t0 = Date.now(); await neval('notepad', `(()=>{const d=deskFiles();delete d['SAVED.TXT'];fsSave();EXPL.forEach(f=>f());return 1})()`);
    const g = await until(() => !hasIcon('SAVED.TXT'), 10000, 10); T.natDeleted = Date.now() - t0;
    ok('Native: ...deleted, gone from the desktop within 500 ms (' + T.natDeleted + ' ms)', g && T.natDeleted <= 500, {t: T.natDeleted, icons: dicons()}) }
  await neval('notepad', `(()=>{window.__oneAppClose();return 1})()`); await until(() => !clientOf('notepad'), 10000); await wait(600);
  /* ---- 4: Display Properties' background, previewed */
  nclick(nhit('pin settings')); await until(() => clientOf('settings'), 20000); await wait(2500);
  const P = [W - 200, Math.round(H / 2)]; const w0 = nst('wallpaper'), s0 = shot('wall-0'), p0 = px(s0, P[0], P[1]);
  { const t0 = Date.now(); await neval('settings', `(()=>{document.querySelector('#dyns .dynb[data-dyn=teal]').click();return S.dyn})()`);
    const g = await until(() => /^teal\|/.test(nst('wallpaper') || '') && / prev=1$/.test(nst('wallpaper')), 3000, 10); T.natPreview = Date.now() - t0; await wait(100);
    const s1 = shot('wall-teal'), p1 = px(s1, P[0], P[1]);
    ok('Native: a background chosen in Display Properties on the desktop within 300 ms, before Apply (' + T.natPreview + ' ms; Standard ' + T.stdPreview + ' ms)', g && T.natPreview <= 300 && far(p0, p1), {t: T.natPreview, w0, w: nst('wallpaper'), p0, p1}) }
  { await neval('settings', `(()=>{const c=document.querySelector('.sfoot .sfv');c.click();setTimeout(()=>{const b=[...document.querySelectorAll('.sdlg button')].find(x=>x.textContent.trim()=='Confirm');b&&b.click()},100);return 1})()`);
    const g = await until(() => nst('wallpaper') == w0, 3000, 10); await wait(150); const p2 = px(shot('wall-revert'), P[0], P[1]);
    ok('Native: ...back on Revert', g && !far(p0, p2, 8), {w0, w: nst('wallpaper'), p0, p2}) }
  { await neval('settings', `(()=>{document.querySelector('#dyns .dynb[data-dyn=teal]').click();return 1})()`); await until(() => / prev=1$/.test(nst('wallpaper') || ''), 3000, 10);
    /* closed without applying: the page asks (Unapplied changes), Discard */
    await neval('settings', `(()=>{window.__oneAppClose();setTimeout(()=>{const b=[...document.querySelectorAll('.sdlg button')].find(x=>x.textContent.trim()=='Discard');b&&b.click()},200);return 1})()`);
    const g = await until(() => nst('wallpaper') == w0, 5000, 10); await wait(150); const p3 = px(shot('wall-closed'), P[0], P[1]);
    ok('Native: ...and back when Settings closes without applying', g && !far(p0, p3, 8), {w0, w: nst('wallpaper'), p0, p3}) }
}
(async () => {
  cp.execSync('gcc -O2 -o ' + WC + ' ' + path.join(__dirname, '../host/fixtures/wincap.c') + ' -lX11');
  if (!process.env.VIVO_NOSTD) { ST.state(D, {}); try { await std() } catch (e) { ok('Standard ran: ' + e.message, false) } }
  ST.state(D, {}); for (const f of fs.readdirSync(D)) if (f == 'session' || /^apps(\..*)?\.json$/.test(f)) fs.unlinkSync(path.join(D, f));   /* (no catalogue yet: the icons come from this sign-in's pages) */
  fs.writeFileSync(path.join(D, 'wm-native'), '');
  const env = Object.assign({}, process.env, {DISPLAY: DSP, HOME: D, RUDOLF_NATIVE_STATE: path.join(D, 'state'), RUDOLF_NATIVE_TEST_EVAL: TEVAL(), LIBGL_ALWAYS_SOFTWARE: '1', TZ: 'UTC'});
  for (const k of ['http_proxy', 'https_proxy', 'HTTP_PROXY', 'HTTPS_PROXY', 'all_proxy', 'ALL_PROXY']) delete env[k];
  const lg = fs.openSync(path.join(OUT, 'native.log'), 'a');
  /* signed in twice: the account's first sign-in (no catalogue), then again (the shell started again, as after a restart) */
  for (const k of ['natFirst', 'natIcons']) {
    if (NP) { NP.kill(); await wait(1500); try { fs.unlinkSync(path.join(D, 'session')) } catch (e) {} try { fs.unlinkSync(path.join(D, 'state')) } catch (e) {} }
    NP = cp.spawn(BIN, ['--size=' + W + 'x' + H, '--draw=soft', '--base=' + BASE], {env, stdio: ['ignore', lg, lg], cwd: D});
    await until(() => nst('signin') == '1', 40000); await wait(2000); xdo("type --delay 40 '" + PW.replace(/'/g, "'\\''") + "'");
    const t0 = Date.now(); xdo('key Return');
    T[k] = await until(() => nlines('dicon').length > 4, 60000, 10) ? Date.now() - t0 : -1;
    await until(() => nst('user') == 'Ann' && nst('signin') == '0', 30000); await wait(k == 'natFirst' ? 6000 : 3000);
  }
  try { await nat() } catch (e) { ok('Native ran: ' + e.stack, false) } NP.kill();
  fs.writeFileSync(path.join(OUT, 'timings.json'), JSON.stringify(T, null, 1)); console.log('timings ' + JSON.stringify(T));
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('FAIL ' + e.stack); if (NP) NP.kill(); process.exit(1) });
