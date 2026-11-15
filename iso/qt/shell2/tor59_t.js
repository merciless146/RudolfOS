// 1.5.9 (docs/plan-1.5.9.md 16.2 and 16.3): Browse with Tor in Oracle, the page's part, in Chromium with a stand-in rudolfd.
// The button: at the far right of the bookmarks bar, after its "+"; its tooltip; black outline (not pressed) and white
// (pressed, in a Tor window); its onion drawn here (an SVG of lines, no picture, no words: never the Tor logo). Pressed on
// Standard: tor/start, "Connecting to Tor… N %" from tor/status, then the window's tabs in a Tor window (torwin/open with
// each tab) and the window closes; a Tor that failed, or did not connect in 60 s: said, tor/stop, the window stays normal;
// the tabs a Tor window gives back (torwin/back) open in a normal window. The Tor window's page (#app=browser&tor=1&url=..):
// Oracle alone, private, its tabs, the onion beside "Private · Tor", (Tor) in its title, the start page saying what Tor
// does and does not and linking Tor Browser on Flathub; pressed again: torwin/back with its tabs and it closes; new windows
// from it stay Tor windows; it asks for Tor every 20 s and says when it closes (tor/stop). On the native desktop: op "tor"
// and "untor" to the native desktop.   node tor59_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www'), PORT = +process.env.TOR59_PORT || 8341, URL0 = 'http://127.0.0.1:' + PORT + '/';
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 600) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.html': 'text/html', '.webp': 'image/webp', '.jpg': 'image/jpeg', '.woff2': 'font/woff2'};
const M = {calls: [], session: null, engine: 'chromium', st: [], back: null, open: true};
const LS = "window.__rudolfLsFlush=()=>fetch('/api/lsync',{method:'POST',headers:{'X-Rudolf-Token':'t'},body:'{}'});";
const J = (s, o, c) => { s.writeHead(c || 200, {'content-type': 'application/json'}); s.end(JSON.stringify(o)) };
http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0,engine:"' + M.engine + '"};' + LS + (M.native ? 'window.__nat=[];window.rudolfNativeOracle=j=>__nat.push(JSON.parse(j));' : '') + '</script>')) }
    if (u.startsWith('/api/')) { M.calls.push(q.method + ' ' + u.slice(5) + (qs ? '?' + qs : ''));
      if (u == '/api/events/wait') return setTimeout(() => J(s, {ok: true, events: []}), 20000);
      if (u == '/api/desktop/session') return M.session ? J(s, M.session) : J(s, {ok: false, error: 'no native session'}, 404);
      if (u == '/api/tor/status') return J(s, Object.assign({ok: true, pct: 0, line: '', secs: 1, stopping: false}, M.st.length > 1 ? M.st.shift() : M.st[0] || {state: 'off'}));
      if (u == '/api/tor/start' && M.startErr) return J(s, {ok: false, error: M.startErr}, 500);
      if (u == '/api/torwin/back' && q.method == 'GET') { const b = M.back; M.back = null; return J(s, {ok: true, open: M.open, urls: b || []}) }
      return J(s, {ok: true}) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(PORT, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
const calls = re => M.calls.filter(c => re.test(c));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']}); const ctx = await b.newContext({viewport: {width: 1100, height: 760}});
  const errs = []; const watch = p => p.on('pageerror', e => { if (/tor|oTor|browser|oaOpen|ONEAPP/.test(e.stack || '')) errs.push(e.stack.slice(0, 300)) });
  // ---- Standard: the desktop page (an account signed in, as the page does)
  let p = await ctx.newPage(); watch(p);
  await p.goto(URL0); await p.waitForFunction(() => typeof oTorBtn == 'function' && typeof acctNew == 'function', null, {timeout: 30000});
  const acct = await p.evaluate(`(async()=>{svj('bd95setup',1);['oobe'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    const a=await acctNew('Ann','pw-one','admin');ACCTS=[a];window.__sys=1;saveUser();USER=a;await userLogin(a,'pw-one');fsFlush();await new Promise(r=>setTimeout(r,800));
    const hex=u=>[...new Uint8Array(u)].map(x=>x.toString(16).padStart(2,'0')).join('');['login'].forEach(i=>{const o=document.getElementById(i);o&&(o.style.display='none')});
    return {id:a.id,pk:hex(await pbkBits('pw-one',a.ks,256)),fk:hex(FKRAW)}})()`);
  M.session = {ok: true, id: acct.id, n: 'Ann', r: 'admin', pk: acct.pk, fk: acct.fk};
  const q = s => p.evaluate(s);
  await q("wins.slice().forEach(w=>w.close());browser('https://example.com/');wins.find(w=>w.app=='browser'||/Oracle/.test(w.el.querySelector('.t').textContent)).oNewTab('https://example.org/x?y=1'),1"); await wait(600);
  const g = await q(`(()=>{const W=wins.find(w=>w.oNewTab),bar=W.body.querySelector('.obm'),b=bar.lastElementChild,r=b.getBoundingClientRect(),br=bar.getBoundingClientRect(),cs=getComputedStyle(b);
    return {cls:b.className,prev:b.previousElementSibling&&b.previousElementSibling.className,title:b.title,pressed:b.getAttribute('aria-pressed'),gap:Math.round(br.right-r.right),top:Math.round(r.top-br.top),bl:cs.borderTopColor,bw:cs.borderTopWidth,
     svg:!!b.querySelector('svg.torsvg'),img:b.querySelectorAll('image,img,text,use,foreignObject').length,fills:[...b.querySelectorAll('svg [fill]')].map(e=>e.getAttribute('fill')).join(),txt:b.textContent.trim(),
     left:Math.round(r.left-(b.previousElementSibling.getBoundingClientRect().right))}})()`);
  ok('1. the button: the bookmarks bar\'s last, after its "+", at the bar\'s far right (right-aligned)', g.cls == 'otor' && g.prev == 'obmadd' && g.gap <= 8 && g.left > 100, g);
  ok('...its tooltip exactly "Browse Oracle with Tor"; not pressed: a black outline', g.title == 'Browse Oracle with Tor' && g.pressed == 'false' && g.bl == 'rgb(0, 0, 0)' && g.bw == '2px', g);
  ok('...its onion drawn here: lines only (no picture, no words), lavender', g.svg && g.img == 0 && /#c4b5fd/.test(g.fills) && !g.txt, g);
  // pressed: Tor connects (30 %, then on), the tabs go to a Tor window, the window closes
  M.calls = []; M.st = [{state: 'starting', pct: 30, line: 'Bootstrapped 30% (loading_status)'}, {state: 'starting', pct: 30}, {state: 'on', pct: 100}];
  await q("wins.find(w=>w.oNewTab).body.querySelector('.otor').click(),1"); await wait(500);
  const bar = await q("(()=>{const W=wins.find(w=>w.oNewTab);const d=W&&W.body.querySelector('.otorbar');return d&&d.textContent})()");
  ok('2. pressed: Tor started (POST tor/start), the window says "Connecting to Tor… N %"', calls(/^POST tor\/start/).length == 1 && /Connecting to Tor… (0|30) %/.test(bar || ''), [bar, M.calls]);
  await wait(3500);
  const ow = calls(/^POST torwin\/open/);
  ok('3. connected: the window\'s tabs open in a Tor window (rudolfd\'s torwin/open, each tab), and the window closes', ow.length == 1 && ow[0].includes('url=' + encodeURIComponent('https://example.com/')) && ow[0].includes('url=' + encodeURIComponent('https://example.org/x?y=1')) && await q("!wins.some(w=>w.oNewTab)"), [ow, await q("wins.length")]);
  // the tabs given back by a Tor window: a normal Oracle window with them
  M.back = ['https://example.net/', 'about:home']; await wait(2600);
  const bk = await q("(()=>{const W=wins.find(w=>w.oNewTab);return W&&{tabs:W.body.querySelectorAll('.otab').length,tor:!!W.tor,priv:!!W.priv}})()");
  ok('4. (Standard) a Tor window\'s tabs given back (torwin/back): a normal Oracle window with them', bk && bk.tabs == 2 && !bk.tor && !bk.priv, bk);
  // Tor failed: said, stopped, the window stays
  M.calls = []; M.st = [{state: 'failed', pct: 0, line: 'Could not bind to 127.0.0.1:9050: Address already in use'}];
  await q("wins.find(w=>w.oNewTab).body.querySelector('.otor').click(),1"); await wait(1800);
  const fl = await q("(()=>{const W=wins.find(w=>w.oNewTab);const d=W&&W.body.querySelector('.otorbar');return d&&d.textContent})()");
  ok('5. Tor failed: the window says so with Tor\'s line, and stays a normal window; Tor stopped (tor/stop)', /Tor could not connect: Could not bind.*stays a normal window/.test(fl || '') && calls(/^POST tor\/stop/).length == 1 && !calls(/torwin\/open/).length && await q("wins.some(w=>w.oNewTab)"), [fl, M.calls]);
  // not connected in 60 s
  M.calls = []; M.st = [{state: 'starting', pct: 10, line: 'Bootstrapped 10% (conn_done): Connected to a relay'}];
  await q("(()=>{const W=wins.find(w=>w.oNewTab);const d=W.body.querySelector('.otorbar');d&&d.remove();W.body.querySelector('.otor').click();const D=Date.now;setTimeout(()=>{Date.now=()=>D()+61e3},1200);return 1})()"); await wait(3200);
  const to = await q("(()=>{const W=wins.find(w=>w.oNewTab);const d=W&&W.body.querySelector('.otorbar');return d&&d.textContent})()");
  ok('6. Tor not connected in 60 s: said (with Tor\'s last line), Tor stopped, the window stays normal', /could not connect in 60 seconds.*Connected to a relay.*stays a normal window/.test(to || '') && calls(/^POST tor\/stop/).length == 1 && !calls(/torwin\/open/).length, [to, M.calls]);
  M.startErr = 'Tor is not on this system'; await q("(()=>{Date.now=Date.prototype.constructor.now;const W=wins.find(w=>w.oNewTab);W.body.querySelector('.otorbar').remove();W.body.querySelector('.otor').click();return 1})()"); await wait(800); M.startErr = null;
  const ns = await q("(()=>{const d=wins.find(w=>w.oNewTab).body.querySelector('.otorbar');return d&&d.textContent})()");
  ok('...no Tor on the system: said', /Tor could not start: Tor is not on this system/.test(ns || ''), ns);
  await p.close();
  // ---- the Tor window's page (Standard: rudolfd opened it)
  M.calls = []; M.st = [{state: 'on', pct: 100}];
  p = await ctx.newPage(); watch(p);
  await p.goto(URL0 + '#app=browser&tor=1&url=' + encodeURIComponent('https://example.com/') + '&url=' + encodeURIComponent('about:home'));
  await p.waitForSelector('.win.oamain .otor', {timeout: 30000}); await wait(800);
  const tw = await p.evaluate(`(()=>{const W=OA.main,b=W.body.querySelector('.otor'),cs=getComputedStyle(b),pv=W.body.querySelector('.opv');
    const fr=[...W.body.querySelectorAll('.fh iframe')].map(f=>f.srcdoc||'').join('');
    return {tor:!!W.tor,priv:!!W.priv,cls:W.el.classList.contains('opriv')&&W.el.classList.contains('otorw'),tabs:W.body.querySelectorAll('.otab').length,pressed:b.getAttribute('aria-pressed'),bl:cs.borderTopColor,
     badge:pv&&pv.textContent,onion:!!(pv&&pv.querySelector('svg.torsvg')),title:document.title,home:fr.includes('This window goes through Tor')&&fr.includes('not Tor Browser')&&fr.includes('flathub.org/apps/org.torproject.torbrowser-launcher'),
     ob:getComputedStyle(W.body.querySelector('.ob')).backgroundColor}})()`);
  ok('7. the Tor window: Oracle alone, private, its two tabs, the button pressed (white outline)', tw.tor && tw.priv && tw.cls && tw.tabs == 2 && tw.pressed == 'true' && tw.bl == 'rgb(255, 255, 255)', tw);
  ok('...the onion beside "Private · Tor", violet, "(Tor)" in its title', tw.onion && /Private · Tor/.test(tw.badge) && /\(Tor\)/.test(tw.title) && tw.ob == 'rgb(46, 16, 101)', tw);
  await p.evaluate("(()=>{const W=OA.main,a=[...W.body.querySelectorAll('.otab')].pop();a.click();return 1})()"); await wait(400);
  const hm = await p.evaluate("[...OA.main.body.querySelectorAll('.fh iframe')].map(f=>f.srcdoc||'').join('')");
  ok('...its start page says plainly that Tor hides the address but is not Tor Browser\'s anonymity, and links Tor Browser (Flathub)', /This window goes through Tor/.test(hm) && /not Tor Browser's anonymity/.test(hm) && /flathub\.org\/apps\/org\.torproject\.torbrowser-launcher/.test(hm), hm.slice(0, 300));
  // new windows from it stay Tor windows
  M.calls = []; await p.evaluate("browser('https://example.org/'),1"); await wait(400);
  ok('8. a new window from the Tor window (browser(): Ctrl+N, a link\'s new window): another Tor window, not a normal one', calls(/^POST torwin\/open\?url=https%3A%2F%2Fexample\.org%2F/).length == 1 && await p.evaluate("wins.filter(w=>w.oNewTab).length==1"), M.calls);
  await p.evaluate("oraclePrivate('https://example.net/'),1"); await wait(300);
  ok('...New private tab there: a tab of the Tor window', await p.evaluate("OA.main.body.querySelectorAll('.otab').length==3&&wins.filter(w=>w.oNewTab).length==1"), await p.evaluate("OA.main.body.querySelectorAll('.otab').length"));
  // pressed again: its tabs back to a normal window (rudolfd), and it closes
  M.calls = []; await p.evaluate("OA.main.body.querySelector('.otor').click(),1"); await wait(1500);
  const bkc = calls(/^POST torwin\/back/);
  ok('9. pressed again in the Tor window: its tabs go back (torwin/back, each tab) and it closes; Tor told (tor/stop)', bkc.length == 1 && bkc[0].includes('url=' + encodeURIComponent('https://example.com/')) && calls(/^POST tor\/stop/).length >= 1 && await p.evaluate("!OA.main.el.isConnected"), M.calls);
  await p.close();
  // it asks for Tor every 20 s
  p = await ctx.newPage(); watch(p); M.calls = [];
  await p.goto(URL0 + '#app=browser&tor=1&url=about:home'); await p.waitForSelector('.win.oamain .otor', {timeout: 30000}); await wait(21500);
  ok('10. an open Tor window asks rudolfd for Tor every 20 s (tor/start)', calls(/^POST tor\/start/).length >= 1, M.calls);
  await p.close();
  // ---- the native desktop: op "tor" and "untor" (Oracle's page in Chromium, the binding)
  M.engine = 'native'; M.native = 1; M.st = [{state: 'on', pct: 100}];
  p = await ctx.newPage(); watch(p);
  await p.goto(URL0 + '#app=browser'); await p.waitForSelector('.win.oamain .otor', {timeout: 30000}); await wait(500);
  M.calls = []; await p.evaluate("OA.main.oNewTab('https://example.com/a'),OA.main.body.querySelector('.otor').click(),1"); await wait(2500);
  const nt = await p.evaluate("__nat.filter(m=>m.op=='tor')");
  ok('11. (Native) pressed: Tor started, then op "tor" with the window\'s tabs to the native desktop (not rudolfd), and the window closes', nt.length == 1 && nt[0].url.split('\n').includes('https://example.com/a') && !calls(/torwin/).length && calls(/^POST tor\/start/).length == 1, [nt, M.calls]);
  await p.close();
  p = await ctx.newPage(); watch(p);
  await p.goto(URL0 + '#app=browser&tor=1&url=' + encodeURIComponent('https://example.com/b')); await p.waitForSelector('.win.oamain .otor', {timeout: 30000}); await wait(500);
  await p.evaluate("browser('https://example.org/c'),1"); await wait(300);
  const n2 = await p.evaluate("__nat.filter(m=>m.op=='tor').map(m=>m.url)");
  ok('...(Native) a new window from a Tor window: op "tor" again (another Tor window)', n2.length == 1 && n2[0] == 'https://example.org/c', n2);
  M.calls = []; await p.evaluate("OA.main.body.querySelector('.otor').click(),1"); await wait(800);
  const n3 = await p.evaluate("__nat.filter(m=>m.op=='untor')");
  ok('...(Native) pressed again: op "untor" with its tabs, and the Tor window closes', n3.length == 1 && n3[0].url == 'https://example.com/b' && !calls(/torwin/).length, [n3, M.calls]);
  await p.close();
  // a normal window given back the tabs (#app=browser&untor=1&url=..)
  p = await ctx.newPage(); watch(p);
  await p.goto(URL0 + '#app=browser&untor=1&url=' + encodeURIComponent('https://example.com/b') + '&url=' + encodeURIComponent('https://example.org/d')); await p.waitForSelector('.win.oamain .otor', {timeout: 30000}); await wait(500);
  const n4 = await p.evaluate("({tabs:OA.main.body.querySelectorAll('.otab').length,tor:!!OA.main.tor,priv:!!OA.main.priv,pressed:OA.main.body.querySelector('.otor').getAttribute('aria-pressed')})");
  ok('12. a normal window given a Tor window\'s tabs (untor=1): not private, the button not pressed, the tabs there', n4.tabs == 2 && !n4.tor && !n4.priv && n4.pressed == 'false', n4);
  await p.close();
  ok('13. no page errors', !errs.length, errs);
  await b.close(); console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('FAIL ' + e.stack); process.exit(1) });
