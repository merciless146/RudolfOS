// 1.6.0 (docs/plan-1.6.0.md Part 17): the window transitions, "Splitting–Merging LitePoint" (desktop/litepoint60.js).
// The page's scenes against the native desktop's (iso/sys/litepoint.h, built here as iso/qt/host/litepoint_dump.c); each
// transition on real windows (opening, minimizing and restoring, maximizing and restoring, closing) at its time; Reduce
// motion; Display Properties' row and its keys.   node litepoint60_t.js [CHROME]  (build the page first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path'), os = require('os'), cp = require('child_process');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8163, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  /* the native desktop's scenes */
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'lp-')), bin = path.join(tmp, 'lpd');
  cp.execFileSync('gcc', ['-O2', '-Wall', '-Wno-misleading-indentation', '-o', bin, path.join(ROOT, 'iso/qt/host/litepoint_dump.c'), '-lm']);
  const GEO = [{f: {x: 100, y: 80, w: 420, h: 300}, s: {x: 0, y: 0, w: 1274, h: 748}, b: [262, 777]}, {f: {x: 640, y: 300, w: 180, h: 120}, s: {x: 0, y: 48, w: 1274, h: 700}, b: [40, 20]}];
  const KINDS = ['c2c', 's2s', 'max', 'min', 'res', 'cls'], TS = []; for (let i = 0; i <= 80; i++) TS.push(i / 80); TS.push(.3, .52, .66, .7, .985, .999);
  const lines = [], keys = []; for (const k of KINDS) for (const g of GEO) for (const t of TS) { const G = k == 'max' ? g : k == 'res' ? {f: g.f, s: g.s, b: g.b} : g;
    lines.push([k, t, G.f.x, G.f.y, G.f.w, G.f.h, G.s.x, G.s.y, G.s.w, G.s.h, G.b[0], G.b[1]].join(' ')); keys.push({k, t, G}) }
  const nat = cp.execFileSync(bin, {input: lines.join('\n') + '\n'}).toString().trim().split('\n').map(l => JSON.parse(l));
  fs.rmSync(tmp, {recursive: true, force: true});

  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd|debian/.test(e.stack || e.message || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8163/'); await p.waitForFunction(() => typeof lpScene == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    ACCTS.length=0;ACCTS.push({n:'ann',h:await pwMake('a1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'a1');S.rmotion=false;applySet();wins.slice().forEach(w=>w.close());return 1})()`);
  await wait(600);

  /* 1. the same scenes on both desktops */
  const pg = await p.evaluate(K => K.map(({k, t, G}) => { const o = lpScene(k, t, G); return {ms: LP_MS[k], ...o} }), keys);
  const near = (a, b) => Math.abs(a - b) < .02;
  const same = (a, b) => a.ms == b.ms && a.geo == b.geo && near(a.lit, b.lit) && (!a.clip == !b.clip) && (!a.clip || ['x', 'y', 'w', 'h'].every(k => near(a.clip[k], b.clip[k]))) &&
    a.L.length == b.L.length && a.L.every((l, i) => l.every((v, j) => near(v, b.L[i][j]))) && a.P.length == b.P.length && a.P.every((l, i) => l.every((v, j) => near(v, b.P[i][j])));
  const bad = []; pg.forEach((a, i) => { if (!same(a, nat[i])) bad.push({k: keys[i].k, t: keys[i].t, page: a, native: nat[i]}) });
  ok('1. the page and the native desktop draw the same: ' + pg.length + ' moments of the six transitions, two windows', !bad.length, bad.slice(0, 2));
  ok('2. the times the owner chose: open 200 ms corner to corner, 220 ms side to side; maximize 180, minimize 200, restore 180, close 200', JSON.stringify(await q('LP_MS')) == JSON.stringify({c2c: 200, s2s: 220, max: 180, min: 200, res: 180, cls: 200}) && nat.find((x, i) => keys[i].k == 's2s').ms == 220);
  const sc = (k, t, g = 0) => pg[keys.findIndex(x => x.k == k && x.t == t && x.G.f.x == GEO[g].f.x)];
  ok('3. opening: the window hidden while its frame is drawn, then filled out from the corner where the points met (red spark there)', sc('c2c', .5).clip.w == 0 && sc('c2c', .85).clip.w > 0 && sc('c2c', .85).clip.x > GEO[0].f.x && sc('c2c', 1).clip == null && sc('c2c', .7).P.some(x => x[3] > .5 && near(x[0], 520) && near(x[1], 380)), [sc('c2c', .5).clip, sc('c2c', .85).clip]);
  ok('4. side to side: the points from the middle of the left side, meeting in the middle of the right', sc('s2s', .025).P.some(x => near(x[0], 100) && near(x[1], 230)) && sc('s2s', .7).P.some(x => near(x[0], 520) && near(x[1], 230)));
  ok('5. minimize: the window cleared into the middle of its bottom edge, then a point drops into its taskbar button, which lights', sc('min', .3).clip.w < 1 && sc('min', .95).P.some(x => x[1] > 700) && sc('min', .95).lit > 0 && sc('min', 1).lit == 0);
  ok('6. maximize: as it was until the frame is drawn, then resized once and shown growing to the frame', sc('max', .5).geo == 'from' && sc('max', .75).geo == 'to' && sc('max', .75).clip.w < 1274 && sc('max', 1).clip == null);
  ok('7. restore: maximized until it settles, then shown shrinking into its corners; its size changed once, at the end', sc('res', .9).geo == 'from' && sc('res', .9).clip.w < 1274 && sc('res', 1).geo == 'to');
  ok('8. close: the contents shut into the middle line, the ends meet in the centre in a red spark', sc('cls', .2).clip.h < 300 && sc('cls', .2).clip.w == 420 && sc('cls', .9).P.some(x => near(x[0], 310) && near(x[1], 230) && x[3] > .9));

  /* real windows */
  const runs = () => q('[...LPT.runs].map(r=>r.kind+(r.back?"<":""))');
  await q("launch('notepad');1"); await wait(30); const r1 = await runs(), cp1 = await q("wins.at(-1).el.style.clipPath");
  await wait(330); const done1 = await q("({c:wins.at(-1).el.style.clipPath,n:LPT.n,cv:!!document.getElementById('lpcv')})");
  ok('9. a window opened from the start menu (launch): corner to corner, hidden at first, all of it shown after; the layer goes', r1.includes('c2c') && /inset/.test(cp1) && done1.c == '' && done1.n >= 1 && !done1.cv, [r1, cp1, done1]);
  await q("(()=>{const b=document.createElement('button');wins.at(-1).body.appendChild(b);b.onclick=()=>calc();b.click();return 1})()"); await wait(30);
  ok('10. a window opened by a click in another window: side to side', (await runs()).includes('s2s'), await runs()); await wait(350);
  const W0 = "wins.find(w=>w.app=='notepad')";
  await q(W0 + ".el.querySelector('.mn').click();1"); await wait(60); const m1 = await q(`({d:${W0}.el.style.display,min:${W0}.min,r:[...LPT.runs].map(r=>r.kind),pe:${W0}.el.style.pointerEvents})`);
  await wait(300); const m2 = await q(`${W0}.el.style.display`);
  ok('11. minimize: still on the screen (taking no clicks) while it goes, hidden after; minimized at once for the rest of the desktop', m1.d == 'flex' && m1.min && m1.r.includes('min') && m1.pe == 'none' && m2 == 'none', [m1, m2]);
  await q(W0 + ".focus();1"); await wait(30); const m3 = await runs(); await wait(300);
  ok('12. restoring it: the drop backward, and it is all there after', m3.includes('min<') && await q(`${W0}.el.style.display=='flex'&&${W0}.el.style.clipPath==''`), m3);
  const g0 = await q(`(()=>{const e=${W0}.el;return [e.offsetLeft,e.offsetTop,e.offsetWidth,e.offsetHeight]})()`);
  await q(W0 + ".el.querySelector('.mx').click();1"); await wait(40); const x1 = await q(`(()=>{const e=${W0}.el;return {w:e.offsetWidth,max:e.classList.contains('max'),r:[...LPT.runs].map(r=>r.kind)}})()`);
  await wait(300); const x2 = await q(`(()=>{const e=${W0}.el;return {w:e.offsetWidth,dw:$('#desk').clientWidth,c:e.style.clipPath}})()`);
  ok('13. maximize: its size as it was while the frame is drawn, the whole screen after (its size changed once)', x1.max && x1.r.includes('max') && x1.w == g0[2] && x2.w >= x2.dw - 10 && x2.c == '', [g0, x1, x2]);
  await q(W0 + ".el.querySelector('.mx').click();1"); await wait(40); const x3 = await q(`(()=>{const e=${W0}.el;return {w:e.offsetWidth,r:[...LPT.runs].map(r=>r.kind)}})()`);
  await wait(300); const x4 = await q(`(()=>{const e=${W0}.el;return [e.offsetLeft,e.offsetTop,e.offsetWidth,e.offsetHeight]})()`);
  ok('14. restore: maximized while the frame gathers, its own size and place after', x3.r.includes('res') && x3.w > g0[2] && JSON.stringify(x4) == JSON.stringify(g0), [x3, x4, g0]);
  const n0 = await q('wins.length'); await q(W0 + ".close();1"); await wait(40);
  const c1 = await q(`({n:wins.length,on:[...document.querySelectorAll('#desk > .win')].length,r:[...LPT.runs].map(r=>r.kind)})`); await wait(300);
  const c2 = await q("[...document.querySelectorAll('#desk > .win')].length");
  ok('15. close: gone from the windows (and the taskbar) at once, on the screen until its frame has gone, then removed', c1.n == n0 - 1 && c1.r.includes('cls') && c2 == c1.on - 1, [n0, c1, c2]);
  /* Reduce motion */
  await q("S.rmotion=true;applySet();launch('notepad');1"); await wait(30); const rm1 = await q("({r:LPT.runs.size,c:wins.at(-1).el.style.clipPath})");
  await q("wins.at(-1).close();1"); const rm2 = await q("[...document.querySelectorAll('#desk > .win')].length");
  ok('16. Reduce motion: no transitions (opened and closed at once)', rm1.r == 0 && rm1.c == '' && rm2 == c2 - 0, [rm1, rm2, c2]);
  await q("S.rmotion=false;applySet();1");
  /* Display Properties */
  await q("(()=>{settings('disp');return 1})()"); await wait(800);
  const dp = await q("(()=>{const t=document.querySelector('#dlpac'),m=document.querySelector('#dlpacm'),r=document.querySelector('#dlpbr');return t&&m&&r?{on:t.checked,dis:m.disabled,opts:[...m.options].map(o=>o.textContent).join(),min:r.min,max:r.max,v:r.value,lab:t.parentElement.textContent.trim(),out:document.querySelector('#dlpbrv').textContent}:null})()");
  ok('17. Display Properties › Colours and theme: "The window transitions (LitePoint)" with Normal / Negative / Complementary, off (white) by default; the light from 0 to 100, at 100', dp && !dp.on && dp.dis && dp.opts == 'Normal,Negative,Complementary' && dp.min == '0' && dp.max == '100' && dp.v == '100' && /window transitions \(LitePoint\)/.test(dp.lab), dp);
  const white = await q('lpCols().core');
  await q("(()=>{const c=document.querySelector('#dlpac');c.checked=true;c.onchange({target:c});const s=document.querySelector('#dlpacm');s.value='comp';s.onchange({target:s});return 1})()"); await wait(100);
  const col = await q("({c:lpCols(),hex:acVariant(S.accent,'comp'),k:[S.lpac,S.lpacm]})");
  ok('18. ticked: the light is the accent colour (here Complementary), lifted to glow; white when not ticked', white == '#f2f7ff' && col.c.hex == col.hex && col.c.core != white && col.k[0] === true && col.k[1] == 'comp', [white, col]);
  await q("(()=>{const r=document.querySelector('#dlpbr');r.value='0';r.oninput({target:r});return 1})()"); await wait(50);
  const z = await q("({b:lpBr(),o:document.querySelector('#dlpbrv').textContent,k:S.lpbr})");
  await q("launch('notepad');1"); await wait(30); const z1 = await q("({r:[...LPT.runs].map(r=>r.kind)})"); await wait(330);
  ok('19. the light at 0: off (no light), the window still opens on the same timing', z.b == 0 && z.k === 0 && /Off/.test(z.o) && z1.r.includes('c2c'), [z, z1]);
  ok('20. its keys are Display Properties\' (kept with the account\'s settings)', await q("['lpac','lpacm','lpbr'].every(k=>DKEYS.includes(k)&&k in DEFS.disp)"));
  ok('21. the search in Settings finds it', /LitePoint/.test(fs.readFileSync(path.join(ROOT, 'desktop/settings-index.json'), 'utf8')));
  const nsrc = fs.readFileSync(path.join(ROOT, 'iso/sys/rudolf-native.c'), 'utf8');
  ok('22. the native desktop uses the same scenes (litepoint.h), and reads the same keys', /#include "litepoint.h"/.test(nsrc) && /jbool\(s, "lpac"\)/.test(nsrc) && /"lpacm"/.test(nsrc) && /"lpbr"/.test(nsrc));
  ok('no page errors', !errs.length, errs);
  await b.close(); srv.close();
  const n = R.filter(x => x).length; console.log(`\n${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
