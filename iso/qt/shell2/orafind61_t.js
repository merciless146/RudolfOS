// 1.6.1 (docs/plan-1.6.1.md Part 10, docs/plan-oracle.md item 1): Oracle's find in page. A. the walker (shell.js FIND_FN, the same
// function as orafind61.js oraFindFn, which Oracle's own pages get): matches across inline elements, never across blocks or in
// hidden text, next and back round the ends, Match case, the selection, stop clearing the highlights. B. the desktop with a
// stood-in shell (rudolf-webkit's protocol, the same tab messages as the Chromium shell): Ctrl+F opens the bar under the bookmarks
// bar, typing asks the tab ("find"), its answer shows "3 of 17", Enter / Shift+Enter / ▲ ▼, Match case, Escape closes (findstop),
// the bar follows its tab, Ctrl+F and F3 from inside the page (tabev keys), the selected text fills the box, ☰ › Find in page…;
// Oracle's own pages searched by their own script; a website in a frame (no shell): "Find is not available for this page".
//   node orafind61_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const api = a => a == 'events' ? {ok: true, events: ''} : a == 'info' ? {ok: true, backlight: false, live: 0, boot: 'uefi', kernel: 'test'} : {ok: true};
let WK = true;
const PAGE = '<!doctype html><meta charset=utf-8><title>Find</title><body>' + Array.from({length: 15}, (_, i) => `<p>Line ${i}: the ${i < 10 ? 'reindeer' : 'Reindeer'} pulls the sleigh.</p>`).join('')
  + '<p>A <a href="/x">reindeer link</a> and a split rein<b>deer</b> word.</p><p style="display:none">reindeer hidden</p><div>rein</div><div>deer across two blocks</div><p id=s>select me please</p>';
const srv = http.createServer((q, s) => { const [u0] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', async () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0,engine:"webkit"};window.__out=[];' + (WK ? 'window.webkit={messageHandlers:{rudolfShell:{postMessage:m=>__out.push(m)}}};' : '') + '</script>')) }
    if (u == '/find') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(PAGE) }
    if (u == '/api/events/wait') await new Promise(r => setTimeout(r, 800));
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5) == 'events/wait' ? 'events' : u.slice(5)))) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8612, '127.0.0.1');
const prep = "(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';window.__net=false;window.__sys=false;return 1})()";
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const ctx = await b.newContext({viewport: {width: 1280, height: 800}});
  // A. the walker
  {const src = fs.readFileSync(path.join(ROOT, 'iso/shell/shell.js'), 'utf8'), mine = fs.readFileSync(path.join(ROOT, 'desktop/orafind61.js'), 'utf8');
   const m = /^const FIND_FN = (function \(o\) \{\n[\s\S]*?\n\});\n/m.exec(src), m2 = /^function oraFindFn(\(o\) \{\n[\s\S]*?\n\})\n/m.exec(mine);
   ok('A0. shell.js FIND_FN is orafind61.js oraFindFn (one walker for real tabs without the frame\'s own find and for Oracle\'s pages)', !!m && !!m2 && m[1] == 'function ' + m2[1]);
   ok('A0b. shell.js: the tab ops find, findstop, findsel; the frame\'s own find first; its answers as tabev find', /find: \(m, t\) => tabFind\(t, m\)/.test(src) && /typeof t\.f\.find == 'function'/.test(src) && /toTab\(t\.tid, 'find'/.test(src) && /findupdate/.test(src));
   const p = await ctx.newPage(); await p.goto('http://127.0.0.1:8612/find'); const F = o => p.evaluate(`(${m ? m[1] : 'null'})(${JSON.stringify(o)})`);
   const nm = r => r && [r.n, r.i];
   ok('A1. 17 matches: across inline elements (a link, <b>), not in hidden text, not across two blocks; the first is current', JSON.stringify(nm(await F({text: 'reindeer'}))) == '[17,1]');
   ok('A2. the matches are highlighted (CSS highlights: the page\'s HTML is not changed)', await p.evaluate("CSS.highlights.has('rudolf-find')&&CSS.highlights.get('rudolf-find').size==17&&!document.querySelector('mark')&&CSS.highlights.has('rudolf-find-cur')"));
   ok('A3. again: the next', JSON.stringify(nm(await F({text: 'reindeer', again: true}))) == '[17,2]');
   for (let i = 0; i < 15; i++) await F({text: 'reindeer', again: true});
   ok('A4. again at the last: round to the first', JSON.stringify(nm(await F({text: 'reindeer', again: true}))) == '[17,1]');
   ok('A5. back at the first: round to the last', JSON.stringify(nm(await F({text: 'reindeer', again: true, back: true}))) == '[17,17]');
   ok('A6. the current match is scrolled into view', await p.evaluate("(()=>{const r=window.__rudolfFind.r[window.__rudolfFind.i].getBoundingClientRect();return r.top>=0&&r.bottom<=innerHeight})()"));
   ok('A7. Match case: "Reindeer" 5 times', JSON.stringify(nm(await F({text: 'Reindeer', mc: true}))) == '[5,1]');
   ok('A8. no match: 0', JSON.stringify(nm(await F({text: 'no such words'}))) == '[0,0]');
   await p.evaluate("(()=>{const r=document.createRange();r.selectNodeContents(document.getElementById('s'));getSelection().removeAllRanges();getSelection().addRange(r)})()");
   ok('A9. the selected text, for the bar\'s box', (await F({sel: 1})).sel == 'select me please');
   await F({text: 'reindeer'}); await F({stop: 1});
   ok('A10. stop: the highlights are gone', await p.evaluate("!CSS.highlights.has('rudolf-find')&&!CSS.highlights.has('rudolf-find-cur')"));
   await p.close()}
  // B. the desktop
  const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.stack.slice(0, 300)));
  await p.goto('http://127.0.0.1:8612/'); await p.waitForFunction(() => typeof browser == 'function' && typeof rtOn == 'function' && typeof oraFind == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s); await q(prep);
  await q("__rwkIn({rudolfShell:'hello',version:'webkit-1.6.1',caps:['tabs']})"); await p.waitForTimeout(200);
  await q("browser('https://example.com/')"); await p.waitForTimeout(600);
  const tid = await q("(__out.find(m=>m.rudolfShell=='tab'&&m.op=='open')||{}).tid");
  const ev = o => q(`__rwkIn(Object.assign({rudolfShell:'tabev',tid:${JSON.stringify(tid)}},${JSON.stringify(o)}))`);
  const W = "wins.find(w=>w.app=='browser'||/Oracle/.test(w.el.querySelector('.t').textContent))";
  const bar = () => q(`(()=>{const W=${W},b=W.body.querySelector('.ofind');return b?{shown:!b.hidden,q:b.querySelector('.ofq').value,n:b.querySelector('.ofn').textContent,below:b.previousElementSibling&&b.previousElementSibling.className,focus:document.activeElement===b.querySelector('.ofq')}:null})()`);
  const sent = op => q(`__out.filter(m=>m.rudolfShell=='tab'&&m.op==${JSON.stringify(op)}).pop()||null`);
  ok('B0. the window has a find bar, hidden, under the bookmarks bar', JSON.stringify(await bar()) == JSON.stringify({shown: false, q: '', n: '', below: 'obm', focus: false}), await bar());
  await q(`${W}.body.querySelector('.ad').focus()`); await p.keyboard.press('Control+f'); await p.waitForTimeout(200);
  let B = await bar(); ok('B1. Ctrl+F (in the address box): the bar opens, its box has the keyboard', B.shown && B.focus, B);
  const fs1 = await sent('findsel'); ok('B2. ...the tab is asked for its selected text', fs1 && fs1.tid == tid, fs1);
  await ev({ev: 'find', id: fs1.id, sel: 'sleigh'}); await p.waitForTimeout(250);
  let f = await sent('find'); B = await bar();
  ok('B3. the selection fills the box and is searched', B.q == 'sleigh' && f && f.text == 'sleigh' && !f.again, [B, f]);
  await p.keyboard.press('Control+a'); await p.keyboard.type('reindeer'); await p.waitForTimeout(400);
  f = await sent('find'); ok('B4. typing searches the tab (op find, the words, not "again")', f && f.text == 'reindeer' && !f.again && !f.mc && f.tid == tid, f);
  await ev({ev: 'find', id: f.id, n: 17, i: 1}); await p.waitForTimeout(100);
  ok('B5. its answer: "1 of 17"', (await bar()).n == '1 of 17', await bar());
  await p.keyboard.press('Enter'); await p.waitForTimeout(150); f = await sent('find');
  ok('B6. Enter: the next (again, forward)', f.again && !f.back && f.text == 'reindeer', f);
  await ev({ev: 'find', id: f.id, n: 17, i: 2}); await p.keyboard.press('Shift+Enter'); await p.waitForTimeout(150); f = await sent('find');
  ok('B7. Shift+Enter: the previous (again, back)', f.again && f.back, f);
  await ev({ev: 'find', id: f.id - 1, n: 17, i: 9}); await p.waitForTimeout(100);
  ok('B8. an answer to an older question is not shown', (await bar()).n == '2 of 17', await bar());
  await q(`${W}.body.querySelector('.ofx').click()`); await p.waitForTimeout(100); f = await sent('find');
  ok('B9. ▼ is the next too', f.again && !f.back, f);
  await q(`(()=>{const c=${W}.body.querySelector('.ofc');c.checked=true;c.dispatchEvent(new Event('change'))})()`); await p.waitForTimeout(150); f = await sent('find');
  ok('B10. Match case: searched again with it', f.mc && !f.again, f);
  await ev({ev: 'find', id: f.id, n: 0, i: 0}); await p.waitForTimeout(100);
  ok('B11. nothing found: "No results"', (await bar()).n == 'No results', await bar());
  // the bar follows its tab
  await q(`${W}.oNewTab('https://example.org/')`); await p.waitForTimeout(400);
  B = await bar(); ok('B12. another tab: the bar is hidden', !B.shown, B);
  await q(`${W}.body.querySelectorAll('.otab')[0].click()`); await p.waitForTimeout(200);
  B = await bar(); ok('B13. back to the first tab: the bar again, with its words and count', B.shown && B.q == 'reindeer' && B.n == 'No results', B);
  await q(`${W}.body.querySelector('.ofq').focus()`); await p.keyboard.press('Escape'); await p.waitForTimeout(150);
  B = await bar(); const stop = await sent('findstop');
  ok('B14. Escape: the bar closes, the tab\'s highlights are cleared (findstop)', !B.shown && stop && stop.tid == tid, [B, stop]);
  await ev({ev: 'keys', k: 'cf'}); await p.waitForTimeout(200);
  ok('B15. Ctrl+F inside the page (tabev keys cf): the bar opens', (await bar()).shown);
  await q(`${W}.body.querySelector('.ofz').click()`); await ev({ev: 'keys', k: 'f3'}); await p.waitForTimeout(200);
  ok('B16. F3 inside the page with the bar closed: it opens', (await bar()).shown);
  await p.keyboard.press('Escape');
  await q(`${W}.body.querySelector('.mn').click()`); await p.waitForTimeout(150);
  const items = await q(`[...${W}.body.querySelectorAll('.odd>div')].map(d=>d.textContent)`);
  ok('B17. ☰ has "Find in page…"', items.includes('Find in page…'), items);
  await q(`[...${W}.body.querySelectorAll('.odd>div')].find(d=>d.textContent=='Find in page…').click()`); await p.waitForTimeout(150);
  ok('B18. ...which opens the bar', (await bar()).shown);
  // Oracle's own pages (no real tab): searched by their own script
  await q("wins.slice().forEach(w=>w.close())"); await q("browser('about:oracle')"); await p.waitForTimeout(800);
  await q(`${W}.body.querySelector('.ad').focus()`); await p.keyboard.press('Control+f'); await p.waitForTimeout(150); await p.keyboard.type('Oracle'); await p.waitForTimeout(700);
  B = await bar(); ok('B19. Oracle\'s own page (About Oracle): found by its own script, "1 of N"', /^1 of [1-9]\d*$/.test(B.n), B);
  await p.keyboard.press('Enter'); await p.waitForTimeout(400);
  ok('B20. ...Enter: the next', /^2 of [2-9]\d*$/.test((await bar()).n), await bar());
  const fr = p.frameLocator('.win.act iframe:visible');
  ok('B21. ...highlighted inside the page', await fr.locator('body').evaluate(() => CSS.highlights.has('rudolf-find') && CSS.highlights.get('rudolf-find').size > 1));
  await q(`${W}.body.querySelector('.ofz').click()`); await p.waitForTimeout(200);
  ok('B22. ✕: its highlights are cleared', await fr.locator('body').evaluate(() => !CSS.highlights.has('rudolf-find')));
  await fr.locator('body').click(); await p.keyboard.press('Control+f'); await p.waitForTimeout(250);
  ok('B23. Ctrl+F inside Oracle\'s own page opens the bar', (await bar()).shown);
  ok('no page errors', !errs.length, errs);
  // C. a website in a frame (no shell)
  WK = false; const p2 = await ctx.newPage(); const errs2 = []; p2.on('pageerror', e => errs2.push(e.stack.slice(0, 300)));
  await p2.goto('http://127.0.0.1:8612/'); await p2.waitForFunction(() => typeof browser == 'function' && typeof oraFind == 'function', null, {timeout: 30000});
  await p2.evaluate(prep); await p2.evaluate("browser('http://127.0.0.1:8612/find')"); await p2.waitForTimeout(800);
  await p2.evaluate(`${W}.body.querySelector('.ad').focus()`); await p2.keyboard.press('Control+f'); await p2.keyboard.type('reindeer'); await p2.waitForTimeout(400);
  const n2 = await p2.evaluate(`${W}.body.querySelector('.ofn').textContent`);
  ok("C1. a website in a frame (no shell): \"Find is not available for this page\"", n2 == "Find is not available for this page", await p2.evaluate(`(()=>{const W=${W};return [!!W,W&&W.body.querySelector(".ofind").hidden,W&&W.body.querySelector(".ofq").value,W&&[...W.body.querySelectorAll("iframe")].map(f=>f.getAttribute("src")+" "+f.style.display)]})()`));
  ok('no page errors (frames)', !errs2.length, errs2);
  await b.close(); srv.close();
  const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
