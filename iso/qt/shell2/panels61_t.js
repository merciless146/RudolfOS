// 1.6.1 (docs/plan-1.6.1.md Part 2): the mobile build's three panels, the groundwork (panels61.js), in Chromium at phone
// sizes with touch (Playwright's phone emulation: a touch screen, no mouse; swipes as the touch screen gives them, through
// DevTools' Input.dispatchTouchEvent):
//   the model (pnSwipe, pnTarget); on the mobile build (RUDOLFD.mobile): each panel a workspace full screen (the tablet
//   layout on), a swipe to the left on the desktop the next panel, to the right the one before, not past the first or the
//   last; a short, slow or mostly vertical move, a swipe from the right edge (the notifications'), a swipe in a window's
//   page: no switch; a swipe on a full-screen window's title bar; the keys (Ctrl+Alt+arrows); the panel slides in from
//   its side; portrait and landscape. Not on the desktop builds: without the flag nothing of it, a swipe switches nothing
//   (at a phone's size with touch too); not in an app's own page (#app=, the native desktop's windows).
//   The pointer's swipe too (Rudolf OS's touch screens reach X as the pointer: rudolf-display's XTest).
//   node panels61_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
let MOBILE = 1;
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  q.on('data', () => {}); q.on('end', async () => {
    const J = (o, c) => { s.writeHead(c || 200, {'content-type': 'application/json'}); s.end(JSON.stringify(o)) };
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', `<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0,engine:"chromium"};${MOBILE ? 'window.RUDOLFD.mobile=1;' : ''}</script>`)) }
    if (u == '/api/events/wait') { await new Promise(r => setTimeout(r, 800)); s.writeHead(200); return s.end('{"ok":true,"events":""}') }
    if (u.startsWith('/api/')) return J({ok: true});
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8748, '127.0.0.1');
// (phone sizes with a touch screen, as the mobile build's Chromium has them on a phone: a desktop Chromium with touch, so no
// mobile browser's page zoom; Playwright's phone descriptors' sizes and pixel ratios)
const devices = {'Pixel 7': {viewport: {width: 412, height: 915}, deviceScaleFactor: 2.625, hasTouch: true}, 'Pixel 7 landscape': {viewport: {width: 915, height: 412}, deviceScaleFactor: 2.625, hasTouch: true},
  'iPhone 13': {viewport: {width: 390, height: 844}, deviceScaleFactor: 3, hasTouch: true}};
const wait = ms => new Promise(r => setTimeout(r, ms));
async function phone(b, dev, hash) {
  const ctx = await b.newContext(dev); const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(String(e.stack || e).slice(0, 300)));
  await p.goto('http://127.0.0.1:8748/' + (hash || '')); await p.waitForFunction(() => typeof wsGo == 'function' && typeof launch == 'function' && typeof pnSwipe == 'function', null, {timeout: 30000});
  await p.evaluate("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});USER=USER||ACCTS[0];return 1})()");
  const cdp = await ctx.newCDPSession(p);
  // a finger from (x0,y0) to (x1,y1) in N steps over ms
  const swipe = async (x0, y0, x1, y1, ms = 220, N = 8) => {
    await cdp.send('Input.dispatchTouchEvent', {type: 'touchStart', touchPoints: [{x: x0, y: y0}]});
    for (let i = 1; i <= N; i++) { await wait(ms / N); await cdp.send('Input.dispatchTouchEvent', {type: 'touchMove', touchPoints: [{x: x0 + (x1 - x0) * i / N, y: y0 + (y1 - y0) * i / N}]}) }
    await cdp.send('Input.dispatchTouchEvent', {type: 'touchEnd', touchPoints: []}); await wait(350) };
  return {ctx, p, errs, swipe, q: s => p.evaluate(s)};
}
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  // ---- the model
  { const {ctx, q} = await phone(b, devices['Pixel 7']);
    const m = await q("JSON.stringify([pnSwipe(-200,10,200,412),pnSwipe(200,-20,200,412),pnSwipe(-40,0,100,412),pnSwipe(-200,180,200,412),pnSwipe(-200,0,1500,412),pnSwipe(-70,0,100,0),pnTarget(1,-1,3),pnTarget(3,1,3),pnTarget(2,1,3)])");
    ok('1. the model: left swipe next, right swipe before; too short, too vertical or too slow none; never past the ends', m == '[1,-1,0,0,0,1,1,3,3]', m);
    await ctx.close() }
  // ---- the mobile build, portrait and landscape
  for (const [name, dev] of [['Pixel 7 portrait', devices['Pixel 7']], ['iPhone 13 portrait', devices['iPhone 13']], ['Pixel 7 landscape', devices['Pixel 7 landscape']]]) {
    const {ctx, p, errs, swipe, q} = await phone(b, dev);
    const W = dev.viewport.width, H = dev.viewport.height, my = Math.round(H * 0.4);
    ok(`2. ${name}: the mobile build's panels on: html.panels, the tablet layout, the first workspace`, await q("document.documentElement.classList.contains('panels')&&document.documentElement.classList.contains('tablet')&&WS==1"));
    await q("(()=>{wins.slice().forEach(w=>w.close());return 1})()");
    await swipe(W * 0.75, my, W * 0.15, my); const w1 = await q('WS');
    await swipe(W * 0.75, my, W * 0.15, my); const w2 = await q('WS');
    await swipe(W * 0.75, my, W * 0.15, my); const w3 = await q('WS');
    ok(`3. ${name}: a swipe to the left on the desktop: the next panel, then the third, not past it`, w1 == 2 && w2 == 3 && w3 == 3, [w1, w2, w3]);
    await swipe(W * 0.2, my, W * 0.85, my); ok(`...a swipe to the right: the one before`, await q('WS') == 2, await q('WS'));
    await swipe(W * 0.5, my, W * 0.4, my); await swipe(W * 0.7, H * 0.2, W * 0.4, H * 0.7); await swipe(W * 0.75, my, W * 0.15, my, 1400, 10);
    ok(`4. ${name}: a short move, a mostly vertical one, a slow one: no switch`, await q('WS') == 2, await q('WS'));
    const nc0 = await q("!!document.getElementById('ncp')"); await swipe(W - 4, my, W * 0.3, my); const nc1 = await q("!!document.getElementById('ncp')");
    ok(`...from the right edge: the notifications (the tablet layout's), not a panel`, await q('WS') == 2 && (nc1 || nc1 == nc0), [await q('WS'), nc0, nc1]);
    await q("(()=>{const n=document.getElementById('ncp');if(n&&typeof ncToggle=='function')ncToggle();return 1})()"); await wait(300);
    // a window on this panel: full screen; a swipe in its page stays the app's; one on its title bar switches
    await q("(()=>{launch('notepad');return 1})()"); await wait(1500);
    const g = await q("(()=>{const w=wins.find(o=>o.app=='notepad'&&o.ws==WS);if(!w)return null;const r=w.el.getBoundingClientRect(),d=document.getElementById('desk').getBoundingClientRect(),t=w.el.querySelector('.tb').getBoundingClientRect();return {r:[r.left,r.top,r.width,r.height],d:[d.left,d.top,d.width,d.height],tb:[t.left,t.top,t.width,t.height]}})()");
    ok(`5. ${name}: a window on a panel fills it (the panel is the workspace, full screen)`, g && Math.abs(g.r[2] - g.d[2]) < 2 && Math.abs(g.r[3] - g.d[3]) < 2 && Math.abs(g.r[0] - g.d[0]) < 2, g);
    if (g) {
      await swipe(W * 0.75, g.r[1] + g.r[3] * 0.5, W * 0.15, g.r[1] + g.r[3] * 0.5);
      ok(`...a swipe inside its page is the app's (no switch)`, await q('WS') == 2, await q('WS'));
      const ty = g.tb[1] + g.tb[3] / 2; await swipe(W * 0.65, ty, W * 0.1, ty); const a = await q('WS'); await swipe(W * 0.1, ty, W * 0.7, ty, 220);
      ok(`6. ${name}: a swipe on its title bar: the next panel (the window stays on its own)`, a == 3 && await q("wins.some(o=>o.app=='notepad'&&o.ws==2)"), a) }
    await q("wsGo(2)"); await wait(400);
    await p.keyboard.press('Control+Alt+ArrowRight'); await wait(60); const cls = await q("document.getElementById('desk').className"); await wait(400);
    ok(`7. ${name}: the keys (Ctrl+Alt+Right): the next panel, sliding in from the right`, await q('WS') == 3 && /pnr/.test(cls), [await q('WS'), cls]);
    await p.keyboard.press('Control+Alt+ArrowLeft'); await wait(60); const cls2 = await q("document.getElementById('desk').className"); await wait(400);
    ok(`...Ctrl+Alt+Left: the one before, from the left`, await q('WS') == 2 && /pnl/.test(cls2), [await q('WS'), cls2]);
    // (the pointer: on Rudolf OS a touch screen's finger reaches X, and the page, as the pointer through rudolf-display)
    await q("wsGo(1)"); await wait(400); await p.mouse.move(W * 0.8, my); await p.mouse.down(); await p.mouse.move(W * 0.5, my, {steps: 4}); await p.mouse.move(W * 0.1, my, {steps: 4}); await p.mouse.up(); await wait(350);
    ok(`...the same swipe as the pointer gives it (a touch screen through rudolf-display): the next panel`, await q('WS') == 2, await q('WS'));
    ok(`8. ${name}: no page errors`, !errs.length, errs);
    await ctx.close() }
  // ---- an app's own page (#app=: the native desktop's windows): the native shell's panels, not the page's
  { const {ctx, q} = await phone(b, devices['Pixel 7'], '#app=notepad');
    ok('9. an app\'s own page (#app=notepad), mobile build: no panels of its own (the native shell has them)', !(await q("document.documentElement.classList.contains('panels')")));
    await ctx.close() }
  // ---- the desktop builds: nothing of it
  MOBILE = 0;
  { const {ctx, swipe, q, errs} = await phone(b, devices['Pixel 7']); const W = 412, my = 360;
    const on = await q("document.documentElement.classList.contains('panels')"); await swipe(W * 0.75, my, W * 0.15, my); await swipe(W * 0.75, my, W * 0.15, my);
    ok('10. a desktop build at a phone\'s size with touch: no panels, a swipe switches nothing', !on && await q('WS') == 1, [on, await q('WS')]);
    ok('...no page errors', !errs.length, errs); await ctx.close() }
  { const ctx = await b.newContext({viewport: {width: 1280, height: 800}}); const p = await ctx.newPage(); await p.goto('http://127.0.0.1:8748/'); await p.waitForFunction(() => typeof pnSwipe == 'function', null, {timeout: 30000});
    ok('...and on a computer: no panels, no tablet layout', await p.evaluate("!document.documentElement.classList.contains('panels')&&!document.documentElement.classList.contains('tablet')&&!document.querySelector('style')?.textContent.includes('pnInL')"));
    await ctx.close() }
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
