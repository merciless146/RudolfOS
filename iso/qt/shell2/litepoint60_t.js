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
  const KINDS = ['c2c', 's2s', 'max', 'min', 'res', 'cls', 'smo', 'smc', 'pop', 'ctx', 'tbb'], TS = []; for (let i = 0; i <= 80; i++) TS.push(i / 80); TS.push(.3, .52, .66, .7, .985, .999);
  /* (Part 17b: the taskbar on each side, a right-click menu from each corner; the first of each the bottom and top-left) */
  const lines = [], keys = []; for (const k of KINDS) for (const dir of ['min', 'smc', 'smo', 'pop', 'tbb'].includes(k) ? [0, 1, 2, 3] : [0]) for (const corner of k == 'ctx' ? [0, 1, 2, 3] : [0])
    for (const g of GEO) for (const t of TS) { const G = {f: g.f, s: g.s, b: g.b, dir, corner};
    lines.push([k, t, G.f.x, G.f.y, G.f.w, G.f.h, G.s.x, G.s.y, G.s.w, G.s.h, G.b[0], G.b[1], dir, corner].join(' ')); keys.push({k, t, G}) }
  const nat = cp.execFileSync(bin, {input: lines.join('\n') + '\n', maxBuffer: 1 << 28}).toString().trim().split('\n').map(l => JSON.parse(l));
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
  const near = (a, b) => Math.abs(a - b) < .02, LP_TIMES_OK = true;
  const same = (a, b) => a.ms == b.ms && a.geo == b.geo && near(a.lit, b.lit) && (!a.clip == !b.clip) && (!a.clip || ['x', 'y', 'w', 'h'].every(k => near(a.clip[k], b.clip[k]))) &&
    a.L.length == b.L.length && a.L.every((l, i) => l.every((v, j) => near(v, b.L[i][j]))) && a.P.length == b.P.length && a.P.every((l, i) => l.every((v, j) => near(v, b.P[i][j])));
  const bad = []; pg.forEach((a, i) => { if (!same(a, nat[i])) bad.push({k: keys[i].k, t: keys[i].t, page: a, native: nat[i]}) });
  ok('1. the page and the native desktop draw the same: ' + pg.length + ' moments of the eleven transitions, two windows, the taskbar on each side, the menus\' corners', !bad.length, bad.slice(0, 2));
  const scd = (k, t, dir, corner = 0) => pg[keys.findIndex(x => x.k == k && x.t == t && x.G.dir == dir && x.G.corner == corner && x.G.f.x == GEO[0].f.x)];
  { const f = GEO[0].f, e = (a, b) => Math.abs(a - b) < .5, fl = d => scd('smo', .9, d).clip;
    ok('1b. (Part 17b) the start menu fills from the taskbar\'s side: up from a bar at the bottom, down from one at the top, right from one at the left, left from one at the right',
      e(fl(0).y + fl(0).h, f.y + f.h) && fl(0).h < f.h && e(fl(1).y, f.y) && fl(1).h < f.h && e(fl(2).x, f.x) && fl(2).w < f.w && e(fl(3).x + fl(3).w, f.x + f.w) && fl(3).w < f.w, [0, 1, 2, 3].map(fl));
    ok('1c. ...it rises from the start button first; its points meet at the side away from the taskbar (red spark)', scd('smo', .05, 0).L.length == 1 && near(scd('smo', .05, 0).L[0][0], GEO[0].b[0]) &&
      scd('smo', .7375, 0).P.some(x => near(x[1], f.y) && x[3] > .5) && scd('smo', .7375, 1).P.some(x => near(x[1], f.y + f.h) && x[3] > .5), [scd('smo', .7375, 0).P, scd('smo', .7375, 1).P]);
    ok('1d. ...closing, it clears toward the taskbar and one point drops into the start button', near(scd('smc', .2, 1).clip.y, f.y) && scd('smc', .95, 1).P.length == 1 && near(scd('smc', .95, 1).L[0][0], f.x + f.w / 2) && near(scd('smc', .95, 1).L[0][1], f.y), [scd('smc', .2, 1).clip, scd('smc', .95, 1)]);
    ok('1e. a right-click menu opens from its corner at the pointer (the spark there first)', [0, 1, 2, 3].every(c => { const P = scd('ctx', .025, 0, c).P[0], X = c == 1 || c == 2 ? f.x + f.w : f.x, Y = c >= 2 ? f.y + f.h : f.y; return near(P[0], X) && near(P[1], Y) }));
    ok('1f. a taskbar button: only its outline (never hidden), smaller lights', [0, 1, 2, 3].every(d => scd('tbb', .5, d).clip == null && scd('tbb', .5, d).P.every(x => x[2] <= .5 + 1e-9)) && LP_TIMES_OK);
    ok('1g. a window minimizing clears toward the taskbar too, wherever it is', near(scd('min', .2, 1).clip.y, f.y) && near(scd('min', .2, 2).clip.x, f.x) && near(scd('min', .2, 3).clip.x + scd('min', .2, 3).clip.w, f.x + f.w)) }
  ok('2. the times the owner chose (2026-11-19): open 160 ms corner to corner, 220 ms side to side; maximize, minimize, restore, close 160; start menu and pop-ups 120; menus and taskbar buttons 80', JSON.stringify(await q('LP_MS')) == JSON.stringify({c2c: 160, s2s: 220, max: 160, min: 160, res: 160, cls: 160, smo: 120, smc: 120, pop: 120, ctx: 80, tbb: 80}) && nat.find((x, i) => keys[i].k == 's2s').ms == 220);
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
  ok('15. close: gone at once (from the windows, the taskbar and the page), its frame folding away where it was', c1.n == n0 - 1 && c1.r.includes('cls') && c2 == c1.on, [n0, c1, c2]);
  /* Reduce motion */
  await q("S.rmotion=true;applySet();launch('notepad');1"); await wait(30); const rm1 = await q("({r:LPT.runs.size,c:wins.at(-1).el.style.clipPath})");
  await q("wins.at(-1).close();1"); const rm2 = await q("[...document.querySelectorAll('#desk > .win')].length");
  ok('16. Reduce motion: no transitions (opened and closed at once)', rm1.r == 0 && rm1.c == '' && rm2 == c2 - 0, [rm1, rm2, c2]);
  await q("S.rmotion=false;applySet();1");
  /* (Part 17b) the start menu, the taskbar's pop-ups, right-click menus, the taskbar's buttons */
  const pr = () => q('[...LPT.runs].filter(r=>r.page).map(r=>r.kind+":"+(r.G.dir|0))');
  await q("document.body.classList.remove('top','left','right');$('#start').click();1"); await wait(30); const sm1 = await pr(), smc1 = await q("$('#menu').style.clipPath");
  await wait(250); const sm2 = await q("({c:$('#menu').style.clipPath,d:getComputedStyle($('#menu')).display,pv:!!document.getElementById('lppv')})");
  ok('23. the start menu opens from the start button, upward with the taskbar at the bottom (hidden at first, all of it after; the layer goes)', sm1.includes('smo:0') && /inset/.test(smc1) && sm2.c == '' && sm2.d != 'none' && !sm2.pv, [sm1, smc1, sm2]);
  await q("$('#start').click();1"); await wait(30); const sm3 = await q("({d:getComputedStyle($('#menu')).display,r:[...LPT.runs].filter(r=>r.page).map(r=>r.kind)})");
  ok('24. ...closing: gone at once, its frame folding back into the start button', sm3.d == 'none' && sm3.r.includes('smc'), sm3); await wait(250);
  await q("document.body.classList.add('top');$('#start').click();1"); await wait(30); const sm4 = await pr(); await wait(250); await q("$('#start').click();document.body.classList.remove('top');1"); await wait(250);
  ok('25. with the taskbar at the top: downward (the same menu, where it was)', sm4.includes('smo:1'), sm4);
  await q("(()=>{const r=$('#tray').getBoundingClientRect();$('#tray').dispatchEvent(new PointerEvent('pointerdown',{bubbles:true,clientX:r.left+5,clientY:r.top+5}));$('#tray').click();return 1})()"); await wait(30);
  const fl = await q("({r:[...LPT.runs].filter(r=>r.page).map(r=>r.kind),c:($('#calp')||{style:{}}).style.clipPath})"); await wait(250);
  ok('26. a taskbar pop-up (the calendar) from above the button clicked, hidden at first', fl.r.includes('pop') && /inset/.test(fl.c), fl);
  await q("($('#calp')||{remove(){}}).remove();cmenu(300,200,[['One',()=>0],['Two',()=>0]]);1"); await wait(30);
  const cm = await q("({r:[...LPT.runs].filter(r=>r.page).map(r=>r.kind+':'+r.G.corner)})"); await wait(250); await q("cmenuClose();1");
  ok('27. a right-click menu opens from its corner at the pointer', cm.r.some(x => /^ctx:/.test(x)), cm);
  await q("launch('notepad');1"); await wait(300); await q("launch('calc');1"); await wait(300);
  await q("(()=>{const W=wins.find(w=>w.app=='notepad');W.tb.click();return 1})()"); await wait(20); const tbb = await q("[...LPT.runs].filter(r=>r.page).map(r=>r.kind)"); await wait(250);
  ok('28. a window\'s taskbar button, clicked to switch to it: its outline traced', tbb.includes('tbb'), tbb);
  await q("S.rmotion=true;applySet();$('#start').click();1"); await wait(20); const rmm = await q("({r:LPT.runs.size,c:$('#menu').style.clipPath})"); await q("$('#start').click();S.rmotion=false;applySet();1");
  ok('29. Reduce motion: none for menus either', rmm.r == 0 && rmm.c == '', rmm);
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
