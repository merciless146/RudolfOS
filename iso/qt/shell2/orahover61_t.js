// 1.6.1 (the owner, 2026-11-20): Oracle's status line says where a link goes while the mouse is over it: "Goes To:
// <address>", and what it said before once the mouse leaves. Real tabs (Standard and Native: iso/shell/shell.js's
// HOVER_JS, Lite: rudolf-webkit's hit test) send tabev 'hover'; Oracle's own pages (its home, Wikipedia's reader) are
// watched by the desktop itself. rudolf-webkit and rudolfd stood in.   node orahover61_t.js [CHROME] (build the page first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const api = a => a == 'events' ? {ok: true, events: ''} : a == 'info' ? {ok: true, backlight: false, live: 0, boot: 'uefi', kernel: 'test'} : {ok: true};
const srv = http.createServer((q, s) => { const [u0] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', async () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0,engine:"webkit"};window.__out=[];window.webkit={messageHandlers:{rudolfShell:{postMessage:m=>__out.push(m)}}};</script>')) }
    if (u == '/links') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(`<a id=a href="/next?x=1">next</a> <a id=j href="javascript:void 0">js</a> <span id=p>plain</span>
      <form id=f action="/search" method=get><input name=q><button id=b>Go</button><button id=bb type=button>not a submit</button></form>
      <svg width=50 height=20><a id=s href="/svg"><rect width=50 height=20 /></a></svg>`) }
    if (u == '/api/events/wait') await new Promise(r => setTimeout(r, 800));
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5) == 'events/wait' ? 'events' : u.slice(5)))) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8611, '127.0.0.1');
const prep = "(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';window.__net=false;window.__sys=false;return 1})()";
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const ctx = await b.newContext({viewport: {width: 1280, height: 800}});
  // A. the page script of Standard and Native (iso/shell/shell.js HOVER_JS), in a page of its own
  {const src = fs.readFileSync(path.join(ROOT, 'iso/shell/shell.js'), 'utf8');
   const m = /const HOVER_JS = (`[\s\S]*?`);\n/.exec(src); const NONCE = 'rudolfTEST';
   const code = m ? eval(m[1].replace(/\$\{JSON\.stringify\(NONCE\)\}/g, JSON.stringify(NONCE))) : '';
   ok('A0. shell.js has HOVER_JS, registered for every frame and sent on as tabev hover', !!m && /name: 'hover'[^\n]*allFrames: true/.test(src) && /o\.hov[^\n]*toTab\(t\.tid, 'hover'/.test(src) && /HOVER_JS, allFrames: true/.test(src));
   const p = await ctx.newPage(); const said = []; p.on('console', c => { const t = c.text(); if (t.startsWith(NONCE)) said.push(JSON.parse(t.slice(NONCE.length)).hov) });
   await p.goto('http://127.0.0.1:8611/links'); await p.evaluate(code); await p.evaluate(code);   // (twice: installed once)
   const last = async () => { await p.waitForTimeout(150); return said[said.length - 1] };
   await p.hover('#a'); ok('A1. a link: its whole address', await last() == 'http://127.0.0.1:8611/next?x=1', said);
   await p.hover('#p'); ok('A2. off the link: "" (the status line shows what it said)', await last() === '', said);
   await p.hover('#j'); ok('A3. a javascript: link is not shown', await last() === '', said);
   await p.hover('#b'); ok('A4. a form\'s submit button: where the form goes', await last() == 'http://127.0.0.1:8611/search', said);
   await p.hover('#bb'); ok('A5. a button that is not a submit: nothing', await last() === '', said);
   await p.hover('#s'); ok('A6. an SVG link: its address', await last() == 'http://127.0.0.1:8611/svg', said);
   const n = said.length; await p.hover('#s', {position: {x: 2, y: 2}}); await p.waitForTimeout(150);
   ok('A7. moving within the same link sends nothing more (changes only)', said.length == n, said);
   await p.focus('#a'); ok('A8. a link reached with the keyboard (Tab): its address', await last() == 'http://127.0.0.1:8611/next?x=1', said);
   await p.close()}
  // B. the desktop: real tabs' hover events, and Oracle's own pages
  const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.stack.slice(0, 300)));
  await p.goto('http://127.0.0.1:8611/'); await p.waitForFunction(() => typeof browser == 'function' && typeof rtOn == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s); await q(prep);
  const stat = () => q("(()=>{const W=wins.find(w=>/Oracle/.test(w.el.querySelector('.t').textContent));return W?W.body.querySelector('.stat').textContent:null})()");
  // Oracle's own home page (no real tabs yet)
  await q("browser('about:home')"); await p.waitForTimeout(800);
  const fr = p.frameLocator('.win.act iframe:visible');
  const before = await stat();
  await fr.locator('a', {hasText: 'Wikipedia'}).first().hover(); await p.waitForTimeout(200);
  ok('B1. Oracle\'s home page: over "Wikipedia", the status line says "Goes To: https://…wikipedia.org…"', /^Goes To: https:\/\/[a-z.]*wikipedia\.org/.test(await stat()), await stat());
  await p.mouse.move(5, 5); await p.waitForTimeout(200);
  ok('B2. ...the mouse leaves the page: it says what it said before', await stat() === before, [before, await stat()]);
  // (1.6.2: Chromium keeps the sandboxed frame in a process of its own and does not tell it the mouse left: the desktop's page sees it arrive)
  await fr.locator('a', {hasText: 'Wikipedia'}).first().hover(); await p.waitForTimeout(200); await p.hover('.win.act .ad'); await p.waitForTimeout(200);
  ok('B2b. ...over a link again, then onto Oracle\'s own address bar: it says what it said before', await stat() === before, [before, await stat()]);
  // real tabs (rudolf-webkit's protocol; the shell's is the same tabev)
  await q("wins.slice().forEach(w=>w.close());__rwkIn({rudolfShell:'hello',version:'webkit-1.6.1',caps:['tabs']})"); await p.waitForTimeout(200);
  await q("browser('https://example.com/')"); await p.waitForTimeout(600);
  const tid = await q("(__out.find(m=>m.rudolfShell=='tab'&&m.op=='open')||{}).tid");
  const ev = o => q(`__rwkIn(Object.assign({rudolfShell:'tabev',tid:${JSON.stringify(tid)}},${JSON.stringify(o)}))`);
  await ev({ev: 'load', loading: false}); await p.waitForTimeout(100);
  ok('B3. a real tab has loaded: "Done"', await stat() == 'Done', await stat());
  await ev({ev: 'hover', url: 'https://example.com/more?a=1#b'}); await p.waitForTimeout(100);
  ok('B4. the tab says the mouse is over a link: "Goes To: https://example.com/more?a=1#b"', await stat() == 'Goes To: https://example.com/more?a=1#b', await stat());
  await ev({ev: 'hover', url: ''}); await p.waitForTimeout(100);
  ok('B5. ...off it: "Done" again', await stat() == 'Done', await stat());
  await ev({ev: 'hover', url: 'https://example.com/x'}); await ev({ev: 'load', loading: true}); await ev({ev: 'hover', url: ''}); await p.waitForTimeout(100);
  ok('B6. a page that starts loading meanwhile: its "Loading…" stays (not the old "Done")', await stat() == 'Loading…', await stat());
  await q("browser('https://example.org/')"); await p.waitForTimeout(600);   // another window
  await ev({ev: 'hover', url: 'https://example.com/y'}); await p.waitForTimeout(100);
  const bothWins = await q("wins.filter(w=>/Oracle/.test(w.el.querySelector('.t').textContent)).map(w=>w.body.querySelector('.stat').textContent)");
  ok('B7. a tab\'s hover shows in its own window only', bothWins.filter(s => /^Goes To:/.test(s)).length == 1, bothWins);
  ok('no page errors', !errs.length, errs);
  await b.close(); srv.close();
  const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
