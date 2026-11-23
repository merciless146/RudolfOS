// 1.6.2: Desert Camp, the third moving background (desktop/desert62.js, iso/systools/backgrounds62/desert.py), on the desktop
// page with a stand-in rudolfd, and Native's drawing of it (rudolf-native.c ds_draw, built here on its own with cairo) compared
// with the page's frame by frame.
//   node desert62_t.js [CHROME]   (build iso/www first; port 8611, or DESERT_PORT; Native's part needs gcc and cairo's headers)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path'), os = require('os'), cp = require('child_process');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 700) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.webp': 'image/webp', '.jpg': 'image/jpeg', '.png': 'image/png', '.woff2': 'font/woff2', '.json': 'application/json', '.wasm': 'application/wasm'};
const PORT = +process.env.DESERT_PORT || 8611, URL0 = 'http://127.0.0.1:' + PORT + '/';
const srv = http.createServer((q, s) => { const [u0] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    const J = (x, st) => { s.writeHead(st || 200, {'content-type': 'application/json'}); s.end(JSON.stringify(x)) };
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }
    if (u.startsWith('/api/')) { const a = u.slice(5); if (a == 'events/wait') return setTimeout(() => J({ok: true, events: []}), 20000);
      if (/^sys\//.test(a)) return J({ok: false, error: 'not in this test'}, 404); return J({ok: true, items: [], apps: [], windows: [], events: []}) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(PORT, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.addInitScript(() => { window.RUDOLFD = {token: 't', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0} });
  await p.goto(URL0); await p.waitForFunction(() => typeof dsUpdate == 'function' && typeof applySet == 'function', null, {timeout: 30000});
  const set = (dyn, dm, rm) => p.evaluate(([dyn, dm, rm]) => { ['oobe', 'login'].forEach(i => { const o = document.getElementById(i); o && o.remove() }); S.wpimg = ''; S.dyn = dyn; S.dm = dm; S.rmotion = rm; applySet(); dsUpdate() }, [dyn, dm, rm]);
  const st = () => p.evaluate(() => { const c = DSV.cv, e = DSV.el; return {on: DSV.on, shown: !!e && e.style.display != 'none', first: document.body.firstChild === e, timer: !!DSV.timer,
    box: c ? [c.style.left, c.style.top, c.style.width, c.style.height] : null, frame: c ? c.toDataURL() : ''} });
  // 1. in Display Properties' list, with the others that move
  const d = await p.evaluate(() => ({n: DYN.desert && DYN.desert.n, live: DYN_LIVE, l: !!(DYN.desert && DYN.desert.l), dk: !!(DYN.desert && DYN.desert.d), tl: !!(DYN.desert && DYN.desert.tl), td: !!(DYN.desert && DYN.desert.td)}));
  ok('1. Desert Camp is a dynamic background with its light and dark pictures and thumbnails, listed with the moving ones (Spirit Falls, Glacier Lake)', d.n == 'Desert Camp' && d.live.join() == 'oasis,lake,desert' && d.l && d.dk && d.tl && d.td, d);
  // 2. light: shown under everything, the picture's size and place (center/cover at 1280×800: 1600×1067 scaled by 0.8 then cover)
  await set('desert', 'light', false); await wait(400); let a = await st();
  ok('2. chosen in light mode: its moving parts are shown, under everything else on the page, laid where the picture is (cover)', a.on && a.shown && a.first && a.timer && a.box && a.box.join() == '0px,-26.8px,1280px,853.6px', a.box);
  const l1 = a.frame; await wait(700); const l2 = (await st()).frame;
  ok('3. ...and they move (the mirage shimmers, the smoke rises): two frames 0.7 s apart differ', l1 && l2 && l1 != l2);
  // the sun's mirage drawn at the sun's place, smoke above the front fire
  const px = await p.evaluate(() => { const g = DSV.cv.getContext('2d'), A = (x, y) => g.getImageData(Math.round(x / 2), Math.round(y / 2), 1, 1).data[3];
    return {sun: A(1030, 620), lobe: A(1030, 650), sky: A(300, 200), smoke: Math.max(...[40, 60, 80, 100, 120].map(dy => A(765, 930 - dy)))} });
  ok('4. light: the sun over the horizon and its mirage below it, smoke above the front campfire, the open sky left to the picture', px.sun > 200 && px.lobe > 100 && px.sky == 0 && px.smoke > 0, px);
  // 5. dark: stars, tents, flames
  await set('desert', 'dark', false); await wait(400); a = await st();
  const dk = await p.evaluate(() => { const g = DSV.cv.getContext('2d'), A = (x, y) => g.getImageData(Math.round(x / 2), Math.round(y / 2), 1, 1).data;
    let stars = 0; const im = g.getImageData(0, 0, 800, 300).data; for (let i = 3; i < im.length; i += 4) if (im[i] > 24) stars++;
    return {flame: A(760, 915), tent: A(548, 824)[3], stars} });
  ok('5. dark: the front campfire burning, the lit tent glowing, stars twinkling over the sky', dk.flame[3] > 150 && dk.flame[0] > 200 && dk.tent > 0 && dk.stars > 80, dk);
  const met = await p.evaluate(() => { const t = DS_MET[0][0] + .45, n0 = performance.now; DSV.t0 = 0; performance.now = () => t * 1000; dsDraw(); performance.now = n0;
    const [, x0, y0, an, ln] = DS_MET[0], u = .5, hx = x0 + (x0 < 800 ? 1 : -1) * Math.cos(an) * ln * u, hy = y0 + Math.sin(an) * ln * u, g = DSV.cv.getContext('2d');
    let best = 0; for (let dx = -3; dx <= 3; dx++) for (let dy = -3; dy <= 3; dy++) best = Math.max(best, g.getImageData(Math.round(hx / 2) + dx, Math.round(hy / 2) + dy, 1, 1).data[3]); return best });
  ok('6. ...and a shooting star streaks across the sky at its time', met > 120, met);
  // 7. Reduce motion: one still frame, no timer, no shooting star
  await set('desert', 'dark', true); await wait(300); a = await st(); const s1 = a.frame; await wait(600); const s2 = (await st()).frame;
  ok('7. with Reduce motion: one still frame (no timer), nothing moving', a.on && a.shown && !a.timer && s1 == s2);
  // 8. another background, or a picture of one's own: gone
  await set('lake', 'dark', false); await wait(300); a = await st(); ok('8. another background chosen: Desert Camp\'s moving parts gone, its timer stopped', !a.shown && !a.timer && !a.on, a);
  await set('desert', 'light', false); await p.evaluate(() => { window.__wpc = wpCss; wpCss = () => 'url(x)'; applySet(); dsUpdate() }); await wait(300); a = await st();
  ok('9. ...and under a picture of one\'s own', !a.shown && !a.timer, a); await p.evaluate(() => { wpCss = window.__wpc; applySet() });
  // 10. Native draws the same frames (rudolf-native.c's ds_draw built on its own with cairo)
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'ds62-')); let built = false;
  try { const c = fs.readFileSync(path.join(ROOT, 'iso/sys/rudolf-native.c'), 'utf8'), i = c.indexOf('static const double DS_HZ'), j = c.indexOf('static int fx_desert(void)');
    fs.writeFileSync(path.join(tmp, 'ds.c'), '#include <cairo.h>\n#include <math.h>\n#include <stdint.h>\n#include <stdlib.h>\nstatic struct { int rmotion; } S; static int DARK; static int is_dark(void){return DARK;}\ntypedef unsigned int guint;\n' + c.slice(i, j) +
      'int main(int c,char**v){DARK=atoi(v[1]);S.rmotion=atoi(v[4]);ds_draw(atof(v[2]));cairo_surface_write_to_png(DS_CV,v[3]);return 0;}\n');
    cp.execSync(`gcc -O2 -w -o ${tmp}/ds ${tmp}/ds.c $(pkg-config --cflags --libs cairo) -lm`, {stdio: 'pipe'}); built = true } catch (e) { console.log('SKIP 10: Native\'s drawing not built here (gcc or cairo\'s headers missing): ' + String(e.message).slice(0, 200)) }
  if (built) { const bad = [];
    for (const [m, tt] of [['light', 3.7], ['light', 17.2], ['dark', 8.2], ['dark', 'met']]) {
      await set('desert', m, false); const r = await p.evaluate(tt => { const t = tt == 'met' ? DS_MET[0][0] + .45 : tt, n0 = performance.now; DSV.t0 = 0; performance.now = () => t * 1000; dsDraw(); performance.now = n0; return [t, DSV.cv.toDataURL('image/png')] }, tt);
      const jf = path.join(tmp, `js_${m}.png`), cf = path.join(tmp, `c_${m}.png`); fs.writeFileSync(jf, Buffer.from(r[1].split(',')[1], 'base64')); cp.execFileSync(path.join(tmp, 'ds'), [m == 'dark' ? '1' : '0', String(r[0]), cf, '0']);
      const cmp = await p.evaluate(async ([a, b]) => { const ld = u => new Promise(res => { const i = new Image(); i.onload = () => res(i); i.src = u });
        const [A, B] = await Promise.all([ld(a), ld(b)]), cv = document.createElement('canvas'); cv.width = 800; cv.height = 534; const g = cv.getContext('2d', {willReadFrequently: true});
        g.drawImage(A, 0, 0); const da = g.getImageData(0, 0, 800, 534).data; g.clearRect(0, 0, 800, 534); g.drawImage(B, 0, 0); const db = g.getImageData(0, 0, 800, 534).data;
        let n = 0, mx = 0, drawn = 0; for (let i = 0; i < da.length; i += 4) { let d = 0; for (let k = 0; k < 3; k++) d = Math.max(d, Math.abs(da[i + k] * da[i + 3] - db[i + k] * db[i + 3]) / 255); d = Math.max(d, Math.abs(da[i + 3] - db[i + 3]));
          if (da[i + 3] > 8) drawn++; if (d > 24) n++; mx = Math.max(mx, d) } return {pct: 100 * n / (800 * 534), mx: Math.round(mx), drawn} },
        ['data:image/png;base64,' + fs.readFileSync(jf).toString('base64'), 'data:image/png;base64,' + fs.readFileSync(cf).toString('base64')]);
      if (!(cmp.pct < .05 && cmp.drawn > 5000)) bad.push([m, tt, cmp]) }
    ok('10. Native draws the same frames as the page (light at 3.7 s and 17.2 s, dark at 8.2 s and with a shooting star): under 0.05 % of pixels differ', bad.length == 0, bad) }
  fs.rmSync(tmp, {recursive: true, force: true});
  ok('11. no page errors', errs.length == 0, errs);
  const f = R.filter(x => !x).length; console.log((R.length - f) + '/' + R.length + ' passed'); await b.close(); srv.close(); process.exit(f ? 1 : 0)
})().catch(e => { console.log('FAIL ' + e.stack); process.exit(1) });
