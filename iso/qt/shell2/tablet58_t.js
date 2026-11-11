// 1.5.8 (plan 8.1): the tablet layout. On a touch-only screen (a phone in portrait, a tablet) tablet mode is on by
// itself: each window fills the desktop, the taskbar is a dock of icons, swipes from the bottom and right edges open
// Start and the notifications. With a mouse it is off, and Settings › Display › Tablet mode turns it on by hand.
// node tablet58_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
const srv = http.createServer((q, s) => { let u = decodeURIComponent(q.url.split('?')[0]); if (u == '/') u = '/index.html';
  const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': u.endsWith('.html') ? 'text/html' : u.endsWith('.js') || u.endsWith('.mjs') ? 'text/javascript' : 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8184, '127.0.0.1');
const prep = "(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';return 1})()";
const swipe = (x0, y0, x1, y1) => `(()=>{const W=innerWidth,H=innerHeight;const ev=(t,x,y)=>dispatchEvent(new PointerEvent(t,{pointerId:7,pointerType:'touch',clientX:x,clientY:y,bubbles:true}));ev('pointerdown',${x0},${y0});for(let i=1;i<=6;i++)ev('pointermove',${x0}+(${x1}-${x0})*i/6,${y0}+(${y1}-${y0})*i/6);ev('pointerup',${x1},${y1});return 1})()`;
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  // a phone in portrait: touch only
  const pc = await b.newContext({viewport: {width: 412, height: 915}, hasTouch: true, isMobile: true, deviceScaleFactor: 1});
  const p = await pc.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.stack.slice(0, 300)));
  await p.goto('http://127.0.0.1:8184/'); await p.waitForFunction(() => typeof tabletOn == 'function' && typeof notepad == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s); await q(prep); await q("themeApply()");
  ok('1. a touch-only phone screen: tablet mode on by itself, portrait', await q("document.documentElement.classList.contains('tablet')&&document.documentElement.classList.contains('portrait')&&document.documentElement.classList.contains('touch')"));
  await q("notepad()"); await p.waitForTimeout(300);
  const g = await q("(()=>{const w=wins.at(-1).el.getBoundingClientRect(),d=document.getElementById('desk').getBoundingClientRect();return{w:[w.left,w.top,w.width,w.height].map(Math.round),d:[d.left,d.top,d.width,d.height].map(Math.round)}})()");
  ok('2. a window fills the desktop (above the dock)', Math.abs(g.w[2] - g.d[2]) <= 2 && Math.abs(g.w[3] - g.d[3]) <= 2 && Math.abs(g.w[0] - g.d[0]) <= 2, g);
  ok('...no maximize button or resize edges', await q("(()=>{const e=wins.at(-1).el;return getComputedStyle(e.querySelector('.mx')).display=='none'&&[...e.children].filter(c=>/resize/.test(c.style.cssText)).every(c=>getComputedStyle(c).display=='none')})()"));
  const dock = await q("(()=>{const b=wins.at(-1).tb;const r=b.getBoundingClientRect(),im=b.querySelector('img.ico');return{ic:im?Math.round(im.getBoundingClientRect().width):b.dataset.ic,w:Math.round(r.width),h:Math.round(r.height),bar:Math.round(document.getElementById('bar').getBoundingClientRect().height)}})()");
  ok('3. the taskbar is a dock: its icon, large', (dock.ic >= 30 || dock.ic == '🗒️') && dock.w >= 48 && dock.h >= 44 && dock.bar >= 56, dock);
  await q(swipe('W/2', 'H-3', 'W/2', 'H-200')); await p.waitForTimeout(200);
  ok('4. swipe up from the bottom edge: the Start menu', await q("document.getElementById('menu').style.display=='flex'"));
  await q("document.getElementById('menu').style.display='none'");
  await q(swipe('W-2', 'H/2', 'W-160', 'H/2')); await p.waitForTimeout(300);
  ok('5. swipe in from the right edge: the notifications', await q("!!document.getElementById('ncp')"));
  await q("ncToggle()");
  await q(swipe('W/2', 'H/2', 'W/2', 'H/2-200')); await p.waitForTimeout(200);
  ok('...a swipe that does not start at the edge does neither', await q("document.getElementById('menu').style.display!='flex'"));
  // a laptop with a mouse: off; by hand: on
  const lc = await b.newContext({viewport: {width: 1280, height: 800}}); const p2 = await lc.newPage(); p2.on('pageerror', e => errs.push(e.stack.slice(0, 300)));
  await p2.goto('http://127.0.0.1:8184/'); await p2.waitForFunction(() => typeof tabletOn == 'function', null, {timeout: 30000}); await p2.evaluate(prep);
  ok('6. with a mouse: tablet mode off', await p2.evaluate("!document.documentElement.classList.contains('tablet')"));
  await p2.evaluate("settings('disp')"); await p2.waitForSelector('#dtab', {timeout: 5000}); await p2.selectOption('#dtab', 'on'); await p2.waitForTimeout(200);
  ok('7. Settings › Display › Tablet mode: On', await p2.evaluate("document.documentElement.classList.contains('tablet')&&S.tablet=='on'&&/Tablet mode is on/.test(document.querySelector('.dtabm').textContent)"));
  await p2.selectOption('#dtab', 'auto'); await p2.waitForTimeout(200);
  ok('...back to Automatic: off again with the mouse', await p2.evaluate("!document.documentElement.classList.contains('tablet')"));
  await p.setViewportSize({width: 915, height: 412}); await p.waitForTimeout(200);
  ok('8. turned to landscape: the layout follows', await q("document.documentElement.classList.contains('tablet')&&!document.documentElement.classList.contains('portrait')"));
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); const k = R.filter(Boolean).length; console.log(`${k}/${R.length} passed`); process.exit(k == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
