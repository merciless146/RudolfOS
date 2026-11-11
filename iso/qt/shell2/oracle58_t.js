// 1.5.8 (plan Part 9): Oracle's tabs on the Lite desktop are real WebKit pages made by rudolf-webkit, which answers as
// the shell does (hello, tab ops, tabev events); Widevine sites go to Chromium's window by themselves; Oracle's menu
// has Open in Chromium and Sign-in with FedCM (rudolfd web/fedcm); without rudolf-webkit's tabs, Lite sends every
// website to Chromium's window as in 1.5.7. rudolf-webkit and rudolfd stood in.
// node oracle58_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const M = {posts: [], fedcm: false};
const api = (a, q, method) => {
  if (method == 'POST') M.posts.push(a + (q ? '?' + q : ''));
  if (a == 'web/fedcm') { if (method == 'POST') M.fedcm = /on=1/.test(q); return {ok: true, on: M.fedcm, running: false} }
  if (a == 'web/status') return {ok: true, running: true, shown: true};
  if (a == 'events') return {ok: true, events: ''};
  if (a == 'info') return {ok: true, backlight: false, live: 0, boot: 'uefi', kernel: 'test'};
  return {ok: true};
};
let WK = true;
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', async () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0,engine:"webkit"};' + (WK ? 'window.__out=[];window.webkit={messageHandlers:{rudolfShell:{postMessage:m=>__out.push(m)}}};' : '') + '</script>')) }
    if (u == '/api/events/wait') await new Promise(r => setTimeout(r, 800));
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5) == 'events/wait' ? 'events' : u.slice(5), qs, q.method))) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8186, '127.0.0.1');
const prep = "(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';window.__net=false;return 1})()";
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const p = await (await b.newContext({viewport: {width: 1280, height: 800}})).newPage(); const errs = []; p.on('pageerror', e => errs.push(e.stack.slice(0, 300)));
  await p.goto('http://127.0.0.1:8186/'); await p.waitForFunction(() => typeof rtChromium == 'function' && typeof browser == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s); await q(prep);
  ok('0. before rudolf-webkit says hello: websites go to Chromium\'s window (as 1.5.7)', await q("webWants('https://example.com/')"));
  await q("__rwkIn({rudolfShell:'hello',version:'webkit-1.5.8',caps:['tabs','dark']})"); await p.waitForTimeout(200);
  ok('1. rudolf-webkit\'s hello: the desktop answers ready, and Oracle has real tabs', await q("rtOn()&&__out.some(m=>m.rudolfShell=='ready')&&SHELL_VER=='webkit-1.5.8'"), await q("__out.slice(-3)"));
  ok('...websites no longer go to Chromium\'s window', await q("!webWants('https://example.com/')"));
  await q("browser('https://example.com/')"); await p.waitForTimeout(600);
  const open = await q("__out.find(m=>m.rudolfShell=='tab'&&m.op=='open')");
  ok('2. Oracle opens example.com: a WebKit tab is asked for (open, url, the account\'s part)', open && open.url == 'https://example.com/' && /^u[0-9a-z]+$/.test(open.part), open);
  const place = await q(`__out.filter(m=>m.rudolfShell=='tab'&&m.op=='place'&&m.tid==${JSON.stringify(open && open.tid)}).pop()`);
  ok('...and where it is on the screen (place, a rectangle)', place && Array.isArray(place.r) && place.r[2] > 100 && place.r[3] > 100, place);
  await q(`__rwkIn({rudolfShell:'tabev',tid:${JSON.stringify(open.tid)},ev:'nav',url:'https://example.com/hello',back:true,fwd:false});__rwkIn({rudolfShell:'tabev',tid:${JSON.stringify(open.tid)},ev:'title',title:'Hello page'})`);
  await p.waitForTimeout(300);
  const st = await q("(()=>{const W=wins.find(w=>/Oracle/.test(w.el.querySelector('.t').textContent));return{ad:W.body.querySelector('.ad,input').value,t:W.el.querySelector('.t').textContent}})()");
  ok('3. the page\'s address and title come back into Oracle', /example\.com\/hello/.test(st.ad) && /Hello page/.test(st.t), st);
  // Widevine: Chromium
  M.posts = []; await q("__out.length=0;browser('https://www.netflix.com/browse')"); await p.waitForTimeout(800);
  ok('4. Netflix (Widevine): opens in Chromium\'s window, not as a WebKit tab', M.posts.some(x => /^web\/open\?url=https%3A%2F%2Fwww\.netflix\.com/.test(x)) && !(await q("__out.some(m=>m.op=='open')")), M.posts);
  // the menu
  await q("wins.slice().forEach(w=>w.close());browser('https://example.com/')"); await p.waitForTimeout(500);
  await p.click('.win.act .body .mn'); await p.waitForTimeout(200);
  const items = await q("[...document.querySelectorAll('.win.act .odd div')].map(d=>d.textContent)");
  ok('5. Oracle\'s menu: Open in Chromium and Sign-in with FedCM', items.includes('Open in Chromium') && items.includes('Sign-in with FedCM'), items);
  M.posts = []; await q("[...document.querySelectorAll('.win.act .odd div')].find(d=>d.textContent=='Open in Chromium').click()"); await p.waitForTimeout(500);
  ok('...Open in Chromium: the page in Chromium\'s window', M.posts.some(x => /^web\/open\?url=https%3A%2F%2Fexample\.com/.test(x)), M.posts);
  await p.click('.win.act .body .mn'); await p.waitForTimeout(200); M.posts = [];
  await q("[...document.querySelectorAll('.win.act .odd div')].find(d=>/Sign-in with FedCM/.test(d.textContent)).click()"); await p.waitForTimeout(800);
  ok('6. Sign-in with FedCM: turned on (rudolfd web/fedcm?on=1)', M.posts.includes('web/fedcm?on=1') && M.fedcm, M.posts);
  await p.click('.win.act .body .mn'); await p.waitForTimeout(200);
  ok('...the menu shows it ticked', (await q("[...document.querySelectorAll('.win.act .odd div')].map(d=>d.textContent)")).includes('✓ Sign-in with FedCM'));
  await q("document.querySelector('.win.act .odd')&&document.querySelector('.win.act .odd').remove()");
  // a sign-in stuck on Google's FedCM page
  await q("window.__n=[];const o=notify;notify=function(t,m,op){__n.push([t,m]);return o.apply(this,arguments)}");
  const tid2 = await q("[...RT.keys()].pop()");
  await q(`__rwkIn({rudolfShell:'tabev',tid:${JSON.stringify(tid2)},ev:'nav',url:'https://accounts.google.com/gsi/transform',back:false,fwd:false})`); await p.waitForTimeout(200);
  ok('7. a sign-in on Google\'s blank FedCM page: Oracle offers to switch FedCM, once', (await q("__n.filter(n=>/Sign-in stuck/.test(n[0])).length")) == 1, await q('__n'));
  // older rudolf-webkit (no tabs): 1.5.7's way
  WK = false; const p2 = await (await b.newContext()).newPage(); p2.on('pageerror', e => errs.push(e.stack.slice(0, 300)));
  await p2.goto('http://127.0.0.1:8186/'); await p2.waitForFunction(() => typeof rtChromium == 'function', null, {timeout: 30000}); await p2.evaluate(prep);
  ok('8. a rudolf-webkit without tabs: every website goes to Chromium\'s window', await p2.evaluate("!rtOn()&&webWants('https://example.com/')"));
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); const k = R.filter(Boolean).length; console.log(`${k}/${R.length} passed`); process.exit(k == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
