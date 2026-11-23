// 1.6.2 (docs/plan-1.6.2.md Part 39, step 1): librudolf-ui's look against the page's. The page's common controls (ui_sample.js: tabs,
// buttons, a primary and a greyed one, fields, a check box, a heading, a table with its selected row, the message line, a scrolled
// list, the right-click menu, the Confirm dialog) drawn by the desktop page in Chromium, signed in, in a window of its own; the same
// controls drawn by librudolf-ui's sample (iso/sys/ui/ui-sample.c) under Xvfb, from the same rudolfd stand-in (same_rd.py: the same
// settings, read by librudolf-ui's client with the token). Each control is cut out of both pictures by its own rect and compared
// (pxdiff.py, a pixel matched one pixel away counting as the same: text anti-aliasing aside): a control fails over 2 % of its pixels
// differing (UI_LIMIT), or a size more than 2 px apart. Light and dark, two accent colours, a larger font (Display Properties).
// node ui_t.js SAMPLE DIR OUT PORT DISPLAY [CASES]          (run by ui_t.sh)
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path'), cp = require('child_process');
const [BIN, D, OUT, PORT, DSP, ONLY] = process.argv.slice(2);
const LIMIT = +(process.env.UI_LIMIT || 2), PART_LIMIT = +(process.env.UI_PART_LIMIT || 4), TOL = +(process.env.UI_TOL || 32), BASE = 'http://127.0.0.1:' + PORT;
const KEYS = JSON.parse(fs.readFileSync(path.join(D, 'keys.json'))), PW = KEYS.Ann.pw;
const ST = require('./same_state.js'), SAMPLE = require('./ui_sample.js');
const wait = ms => new Promise(r => setTimeout(r, ms));
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
fs.mkdirSync(OUT, {recursive: true});
const WX = 120, WY = 90, WW = 640, WH = 520;   /* (the sample window's place and size: the page's appWin, Native's window) */
const CASES = [
  {n: 'light', set: {dm: 'light', darkui: true, accent: '#2563eb'}},
  {n: 'dark', set: {dm: 'dark', darkui: true, accent: '#2563eb'}},
  {n: 'light-pink', set: {dm: 'light', darkui: true, accent: '#db2777'}},
  {n: 'dark-amber', set: {dm: 'dark', darkui: true, accent: '#f59e0b'}},
  {n: 'light-font', set: {dm: 'light', darkui: true, accent: '#2563eb', ff: 'Liberation Sans', fs: 14}},
];
const PARTS = ['tabs', 'field', 'fieldv', 'btn', 'btnd', 'pri', 'check', 'h4', 'table', 'msg', 'scroll', 'menu', 'dlg'];
let BR;
async function pageCase(c) {
  ST.state(D, c.set, {});
  const ctx = await BR.newContext({viewport: {width: 1280, height: 800}, deviceScaleFactor: 1, timezoneId: 'UTC'});
  await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => /generate_204/.test(r.request().url()) ? r.fulfill({status: 204, body: ''}) : r.abort());
  const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto(BASE + '/'); await p.waitForFunction(() => typeof userLogin == 'function' && document.getElementById('login') && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
  await wait(800); await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
  await p.waitForFunction(() => typeof USER != 'undefined' && USER && getComputedStyle(document.getElementById('login')).display == 'none', null, {timeout: 60000});
  await wait(3000);
  const at = await p.evaluate(([html, menu, x, y, w, h]) => {
    document.querySelectorAll('.ntoast,#ntoasts>*').forEach(e => e.remove());
    const W = appWin('ruisample', 'Controls', '🧪', w, h); W.el.style.left = x + 'px'; W.el.style.top = y + 'px'; W.el.style.width = w + 'px'; W.el.style.height = h + 'px';
    W.body.style.cssText = 'display:flex;flex-direction:column;background:var(--wbg,#fff)'; W.body.innerHTML = html;
    window.__ruiW = W; const b = W.body.getBoundingClientRect(); return [Math.round(b.left), Math.round(b.top), Math.round(b.width), Math.round(b.height)];
  }, [SAMPLE.HTML, SAMPLE.MENU, WX, WY, WW, WH]);
  await wait(600); await p.mouse.move(5, 5);
  const rects = await p.evaluate(b => { const o = {}; document.querySelectorAll('[data-rui]').forEach(e => { const r = e.getBoundingClientRect(); o[e.dataset.rui] = [Math.round(r.left - b[0]), Math.round(r.top - b[1]), Math.round(r.width), Math.round(r.height)] }); return o }, at);
  await p.screenshot({path: path.join(OUT, c.n + '.page.png')});
  /* the menu, at a place in the window; then the Confirm dialog (sdlg, the page's own, as Task Manager's End process asks) */
  const mx = at[0] + 300, my = at[1] + 200;
  await p.evaluate(([x, y, m]) => cmenu(x, y, m.map(i => i == '-' ? '-' : [i[0], () => {}, i[1]]), {title: 'chrome (1234)'}), [mx, my, SAMPLE.MENU]); await wait(400); await p.mouse.move(5, 5);
  rects.menu = await p.evaluate(b => { const r = document.querySelector('.ctx').getBoundingClientRect(); return [Math.round(r.left - b[0]), Math.round(r.top - b[1]), Math.round(r.width), Math.round(r.height)] }, at);
  await p.screenshot({path: path.join(OUT, c.n + '.page-menu.png')});
  await p.evaluate(() => { cmenuClose(); document.querySelectorAll('.ctx').forEach(e => e.remove()); sdlg(__ruiW, 'End process', 'End chrome (process 1234)?', [['Cancel', () => {}], ['End process', () => {}, 1]]) }); await wait(400); await p.mouse.move(5, 5);
  rects.dlg = await p.evaluate(b => { const r = document.querySelector('.sdlg>div').getBoundingClientRect(); return [Math.round(r.left - b[0]), Math.round(r.top - b[1]), Math.round(r.width), Math.round(r.height)] }, at);
  await p.screenshot({path: path.join(OUT, c.n + '.page-dlg.png')});
  /* what the page's styles came to (for the PR and for finding a difference) */
  const css = await p.evaluate(() => { const g = (s, k) => { const e = document.querySelector(s); return e ? getComputedStyle(e)[k] : null };
    return {body: [g('.win .body', 'fontFamily'), g('.win .body', 'fontSize'), g('.win .body', 'color'), g('.win .body', 'backgroundColor')], btn: [g("[data-rui=btn]", 'backgroundColor'), g("[data-rui=btn]", 'borderColor'), g("[data-rui=btn]", 'height')]} });
  fs.writeFileSync(path.join(OUT, c.n + '.page.json'), JSON.stringify({body: at, rects, css, errs}, null, 1));
  await ctx.close(); return {body: at, rects};
}
/* librudolf-ui's sample: the same window, its controls' rects written by the sample (RUI_SAMPLE_RECTS) */
async function nativeCase(c, P) {
  ST.state(D, c.set, {});
  const rf = path.join(OUT, c.n + '.nat.json'); for (const x of ['', '.menu', '.dlg']) try { fs.unlinkSync(rf + x) } catch (e) {}
  const env = Object.assign({}, process.env, {DISPLAY: DSP, RUI_BASE: BASE, RUI_SAMPLE_RECTS: rf, LC_ALL: 'C.UTF-8', GTK_THEME: '', NO_AT_BRIDGE: '1'});
  for (const k of ['http_proxy', 'https_proxy', 'HTTP_PROXY', 'HTTPS_PROXY']) delete env[k];
  const shots = {};
  for (const mode of ['', 'menu', 'dlg']) {
    const pr = cp.spawn(BIN, ['--at=' + P.body[0] + ',' + P.body[1], '--size=' + P.body[2] + 'x' + P.body[3], mode ? '--show=' + mode : '--show=none'], {env, stdio: ['ignore', 'pipe', 'pipe']});
    let log = ''; pr.stdout.on('data', d => log += d); pr.stderr.on('data', d => log += d);
    const t = Date.now(); while (Date.now() - t < 20000 && !fs.existsSync(rf + (mode ? '.' + mode : ''))) await wait(100);
    await wait(700);
    const f = path.join(OUT, c.n + '.nat' + (mode ? '-' + mode : '') + '.png');
    cp.execSync('import -window root ' + f, {env}); shots[mode] = f; pr.kill(); await wait(300);
    if (!fs.existsSync(rf + (mode ? '.' + mode : ''))) { console.log(log.slice(0, 2000)); return null }
  }
  const j = JSON.parse(fs.readFileSync(rf)); for (const m of ['menu', 'dlg']) Object.assign(j.rects, JSON.parse(fs.readFileSync(rf + '.' + m)).rects);
  return j;
}
const px = (a, b, ra, rb, out) => JSON.parse(cp.execSync(`python3 ${path.join(__dirname, 'pxdiff.py')} ${a} ${b} ${ra.join(',')} ${rb.join(',')} ${TOL} ${out} 1`, {env: Object.assign({}, process.env, {PXDIFF_MAP: '1', PXDIFF_AA: '1'})}).toString());
(async () => {
  BR = await chromium.launch({ignoreDefaultArgs: ['--hide-scrollbars']});   /* (the scroll bars drawn, as the desktop's Chromium draws them) */
  const rows = [];
  for (const c of CASES) {
    if (ONLY && !ONLY.split(',').includes(c.n)) continue;
    const P = await pageCase(c), N = await nativeCase(c, P);
    if (!N) { ok(c.n + ': the sample started and said where its controls are', false); continue }
    /* the window as a whole (its body: every control where the page has it), the menu and the dialog: under LIMIT; and each control
       on its own, under PART_LIMIT (a small control is mostly writing: there one pixel of a letter's edge is a larger share) */
    for (const [k, sh, ra0, rb0] of [['window', '', [0, 0, P.body[2], P.body[3]], [0, 0, N.body[2], N.body[3]]], ['menu-whole', '-menu', P.rects.menu, N.rects.menu], ['dialog-whole', '-dlg', P.rects.dlg, N.rects.dlg]]) {
      if (!ra0 || !rb0) { ok(`${c.n} ${k}: drawn by both`, false); continue }
      const d = px(path.join(OUT, c.n + '.page' + sh + '.png'), path.join(OUT, c.n + '.nat' + sh + '.png'), [P.body[0] + ra0[0], P.body[1] + ra0[1], ra0[2], ra0[3]], [N.body[0] + rb0[0], N.body[1] + rb0[1], rb0[2], rb0[3]], path.join(OUT, c.n + '.' + k));
      rows.push([c.n, k, d.pct, ra0.slice(2).join('x'), rb0.slice(2).join('x')]);
      ok(`${c.n} ${k}: ${d.pct}% of its pixels differ`, d.pct < LIMIT, {box: d.box, map: d.map});
    }
    for (const k of PARTS) {
      const a = P.rects[k], b = N.rects[k];
      if (!a || !b) { ok(`${c.n} ${k}: drawn by both`, false, {page: a, native: b}); continue }
      const sh = k == 'menu' ? '-menu' : k == 'dlg' ? '-dlg' : '';
      const ra = [P.body[0] + a[0], P.body[1] + a[1], a[2], a[3]], rb = [N.body[0] + b[0], N.body[1] + b[1], b[2], b[3]];
      const d = px(path.join(OUT, c.n + '.page' + sh + '.png'), path.join(OUT, c.n + '.nat' + sh + '.png'), ra, rb, path.join(OUT, c.n + '.' + k));
      const size = Math.abs(a[2] - b[2]) <= 2 && Math.abs(a[3] - b[3]) <= 2;
      rows.push([c.n, k, d.pct, a.slice(2).join('x'), b.slice(2).join('x')]);
      const at = Math.abs(a[0] - b[0]) <= 2 && Math.abs(a[1] - b[1]) <= 2;
      ok(`${c.n} ${k}: ${d.pct}% of its pixels differ (page ${a.join(',')}, native ${b.join(',')})`, d.pct < PART_LIMIT && size && at, d.pct >= PART_LIMIT ? {box: d.box, map: d.map} : !size ? 'size' : 'place');
    }
  }
  fs.writeFileSync(path.join(OUT, 'summary.tsv'), rows.map(r => r.join('\t')).join('\n') + '\n');
  await BR.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) && R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
