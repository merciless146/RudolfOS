// 1.6.1 (docs/plan-1.6.1.md Part 3, Lite, from 1.5.8): the desktop's menus and windows over Oracle's tabs. A WebKit page cannot
// be cut on the screen, so while the desktop covers part of a tab rudolf-webkit sends a still picture of it (tabev 'still'
// {img}), shown in the tab's place under the desktop's menus, and null when the live page is back (iso/qt/webkit/tabs58_ci.py
// checks rudolf-webkit's side on a screen). Here the desktop's side, rudolf-webkit stood in: the Start menu over a tab is
// reported as a cut (place {vis}); the picture is shown in the tab's place and the menu stays above it; null takes it away;
// what is not a JPEG data address is not shown.   node litestill61_t.js [CHROME] (build the page first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const api = a => a == 'events' ? {ok: true, events: ''} : a == 'info' ? {ok: true, backlight: false, live: 0, boot: 'uefi', kernel: 'test'} : {ok: true};
const srv = http.createServer((q, s) => { const [u0] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', async () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0,engine:"webkit"};window.__out=[];window.webkit={messageHandlers:{rudolfShell:{postMessage:m=>__out.push(m)}}};</script>')) }
    if (u == '/api/events/wait') await new Promise(r => setTimeout(r, 800));
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5) == 'events/wait' ? 'events' : u.slice(5)))) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8612, '127.0.0.1');
const prep = "(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';window.__net=false;window.__sys=false;return 1})()";
// a small yellow JPEG (16x16), as rudolf-webkit sends
const JPEG = 'data:image/jpeg;base64,' + Buffer.from('/9j/4AAQSkZJRgABAQEASABIAAD/2wBDAAMCAgICAgMCAgIDAwMDBAYEBAQEBAgGBgUGCQgKCgkICQkKDA8MCgsOCwkJDRENDg8QEBEQCgwSExIQEw8QEBD/yQALCAAQABABAREA/8wABgAQEAX/2gAIAQEAAD8A0s8g/9k=', 'base64').toString('base64');
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const ctx = await b.newContext({viewport: {width: 1280, height: 800}});
  const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.stack.slice(0, 300)));
  await p.goto('http://127.0.0.1:8612/'); await p.waitForFunction(() => typeof browser == 'function' && typeof rtOn == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s); await q(prep);
  await q("__rwkIn({rudolfShell:'hello',version:'webkit-1.6.1',caps:['tabs']})"); await p.waitForTimeout(200);
  await q("browser('https://example.com/')"); await p.waitForTimeout(800);
  const tid = await q("(__out.find(m=>m.rudolfShell=='tab'&&m.op=='open')||{}).tid");
  ok('1. a real tab is opened (rudolf-webkit stood in)', !!tid, tid);
  // the Oracle window to the bottom left, where the Start menu opens
  await q("(()=>{const W=wins.find(w=>/Oracle/.test(w.el.querySelector('.t').textContent));W.el.style.left='0px';W.el.style.top='60px';W.el.style.width='900px';W.el.style.height='640px';return 1})()");
  await p.waitForTimeout(400);
  const place = () => q(`(__out.filter(m=>m.op=='place'&&m.tid==${JSON.stringify(tid)}).pop()||null)`);
  const p0 = await place();
  ok('2. placed, nothing over it: no cut', p0 && Array.isArray(p0.r) && p0.vis === null, p0);
  await p.click('#start'); await p.waitForTimeout(500);
  const p1 = await place();
  ok('3. the Start menu opened over the tab: reported as a cut (place with vis)', p1 && Array.isArray(p1.vis) && p1.vis.length > 0, p1);
  const ev = o => q(`__rwkIn(Object.assign({rudolfShell:'tabev',tid:${JSON.stringify(tid)}},${JSON.stringify(o)}))`);
  const area = () => q(`(()=>{const W=wins.find(w=>/Oracle/.test(w.el.querySelector('.t').textContent));const t=W&&RT.get(${JSON.stringify(tid)});return t?{bg:t.el.style.backgroundImage.slice(0,40),size:t.el.style.backgroundSize}:null})()`);
  await ev({ev: 'still', img: JPEG}); await p.waitForTimeout(150);
  const a1 = await area();
  ok("4. rudolf-webkit's still picture is shown in the tab's place", a1 && a1.bg.startsWith('url("data:image/jpeg;base64,') && a1.size == '100% 100%', a1);
  const top = await q(`(()=>{const t=RT.get(${JSON.stringify(tid)});const r=t.el.getBoundingClientRect();const v=__out.filter(m=>m.op=='place').pop().vis;
    const R=[Math.round(r.left),Math.round(r.top),Math.round(r.width),Math.round(r.height)];const inV=(x,y)=>v.some(([a,b,w,h])=>x>=R[0]+a&&x<R[0]+a+w&&y>=R[1]+b&&y<R[1]+b+h);
    for(let y=R[1]+R[3]-3;y>R[1];y-=7)for(let x=R[0]+3;x<R[0]+R[2];x+=7)if(!inV(x,y)){const e=document.elementFromPoint(x,y);return {x,y,inTab:t.el.contains(e)||e===t.el,id:e&&(e.id||e.className)}}return null})()`);
  ok('5. ...under the Start menu: where the tab is cut, the menu is on top', top && !top.inTab, top);
  await ev({ev: 'still', img: null}); await p.waitForTimeout(150);
  const a2 = await area();
  ok('6. null: the picture is taken away (the live page is back)', a2 && a2.bg === '' && a2.size === '', a2);
  for (const bad of ['data:image/svg+xml;base64,PHN2Zz48L3N2Zz4=', 'https://example.com/x.jpg', 'data:image/jpeg;base64,AA") ; background: red', 12])
    await ev({ev: 'still', img: bad});
  await p.waitForTimeout(150);
  const a3 = await area();
  ok('7. what is not a JPEG data address is not shown', a3 && a3.bg === '', a3);
  await p.keyboard.press('Escape'); await p.mouse.click(1100, 300); await p.waitForTimeout(500);
  const p2 = await place();
  ok('8. the menu closed: no cut again', p2 && p2.vis === null, p2);
  ok('no page errors', !errs.length, errs);
  await b.close(); srv.close();
  const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
