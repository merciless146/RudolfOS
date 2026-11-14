// 1.5.9: private browsing in Oracle: a private window's tabs get a private part ("priv-…": in memory only), no
// history is kept, the private start page, the look; New private tab from Oracle's menu, Ctrl+Shift+N, and right-click
// on Oracle's icon (taskbar pin, Start menu, an open window's taskbar button); the part is cleared when the last private
// window closes and a new one is used next time. The shell is rudolf-webkit, stood in (as oracle58_t.js).
// node private59_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const srv = http.createServer((q, s) => { const [u0] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', async () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0,engine:"webkit"};window.__out=[];window.webkit={messageHandlers:{rudolfShell:{postMessage:m=>__out.push(m)}}};</script>')) }
    if (u == '/api/events/wait') await new Promise(r => setTimeout(r, 800));
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(u == '/api/events/wait' ? '{"ok":true,"events":""}' : '{"ok":true}') }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8300, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const p = await (await b.newContext({viewport: {width: 1280, height: 800}})).newPage(); const errs = [];
  p.on('pageerror', e => { if (/priv|browser|cmenu|winMenu|iconMenu|tabs26|rtOpen/.test(e.stack || '')) errs.push(e.stack.slice(0, 300)) });
  await p.goto('http://127.0.0.1:8300/'); await p.waitForFunction(() => typeof oraclePrivate == 'function' && typeof browser == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';window.__net=false;window.__sys=false;localStorage.setItem('bd95hist','[]');pinDraw();return 1})()");
  await q("__rwkIn({rudolfShell:'hello',version:'webkit-1.5.9',caps:['tabs','dark']})"); await wait(200);
  const opens = () => q("__out.filter(m=>m.rudolfShell=='tab'&&m.op=='open').map(m=>m.part)");
  // a normal window first: the account's part, history kept
  await q("browser('https://example.com/')"); await wait(500);
  const p0 = (await opens()).pop();
  ok('1. a normal Oracle window: the account\'s part, and the visit in History', /^u[0-9a-z]+$/.test(p0 || '') && await q("JSON.parse(localStorage.getItem('bd95hist')).some(h=>h[1]=='https://example.com/')"), p0);
  await q("wins.slice().forEach(w=>w.close());localStorage.setItem('bd95hist','[]')"); await wait(200);
  // the pin's right-click menu on the taskbar
  await p.click('#pins .pin[data-k=browser]', {button: 'right'}); await wait(250);
  const pm = await q("[...document.querySelectorAll('.cmenu .ci, .cmenu > div')].map(x=>x.textContent.trim()).filter(Boolean)");
  ok('2. right-click Oracle\'s taskbar icon: New private tab, after Open a new window', pm.findIndex(x => /New private tab/.test(x)) == pm.findIndex(x => /Open a new window/.test(x)) + 1 && pm.some(x => /New private tab/.test(x)), pm);
  await q("[...document.querySelectorAll('.cmenu *')].find(x=>x.children.length<4&&/New private tab/.test(x.textContent)&&x.offsetParent).click()"); await wait(600);
  const w1 = await q("(()=>{const W=wins.find(w=>w.priv);return W&&{cls:W.el.classList.contains('opriv'),badge:!!W.body.querySelector('.opv'),title:W.el.querySelector('.t').textContent,home:W.body.querySelector('.fh iframe').srcdoc.includes('You are browsing privately')}})()");
  ok('3. ...opens a private Oracle window: marked (dark violet, 🕶️ Private), titled (Private), the private start page', w1 && w1.cls && w1.badge && /\(Private\)/.test(w1.title) && w1.home, w1);
  const bgc = await q("getComputedStyle(wins.find(w=>w.priv).body.querySelector('.ob')).backgroundColor");
  ok('...its toolbar dark violet', bgc == 'rgb(59, 7, 100)', bgc);
  await q("(()=>{const W=wins.find(w=>w.priv),a=W.body.querySelector('.ad');a.value='https://example.org/';a.dispatchEvent(new KeyboardEvent('keydown',{key:'Enter',bubbles:true}))})()"); await wait(500);
  const p1 = (await opens()).pop();
  ok('4. a website in it: a private part ("priv-…", not the account\'s)', /^priv-u[0-9a-z]+-[0-9a-z]+$/.test(p1 || '') && p1 != p0, p1);
  await q("(()=>{const m=__out.filter(m=>m.rudolfShell=='tab'&&m.op=='open').pop();__rwkIn({rudolfShell:'tabev',tid:m.tid,ev:'nav',url:'https://example.org/page2',back:true,fwd:false});__rwkIn({rudolfShell:'tabev',tid:m.tid,ev:'title',title:'Page 2'})})()"); await wait(300);
  ok('5. ...and nothing of it in History (not even after the page moves on)', await q("JSON.parse(localStorage.getItem('bd95hist')||'[]').length==0"), await q("localStorage.getItem('bd95hist')"));
  // New private tab again: a tab in the same private window
  await p.keyboard.press('Control+Shift+N'); await wait(400);
  const t2 = await q("(()=>{const W=wins.filter(w=>w.priv);return [W.length,W[0]&&W[0].body.querySelectorAll('.otab').length]})()");
  ok('6. Ctrl+Shift+N in the private window: another private tab in it', t2[0] == 1 && t2[1] == 2, t2);
  // a page's new window (target=_blank) stays private
  await q("(()=>{const m=__out.filter(m=>m.rudolfShell=='tab'&&m.op=='open'&&/^priv-/.test(m.part)).pop();__rwkIn({rudolfShell:'tabev',tid:m.tid,ev:'newwin',url:'https://example.net/',ntid:'n9',popup:true})})()"); await wait(500);
  ok('7. a pop-up from a private page opens in a private window', await q("wins.filter(w=>w.priv).length==2"), await q("wins.map(w=>[w.el.querySelector('.t').textContent,!!w.priv])"));
  // the open window's taskbar button
  const tb = await q("(()=>{const W=wins.find(w=>w.priv);return !!W})()");
  await q("(()=>{const W=wins.find(w=>w.priv);winMenu(W,300,300)})()"); await wait(200);
  ok('8. right-click an open Oracle window\'s taskbar button: New private tab there too', tb && await q("[...document.querySelectorAll('.cmenu')].some(m=>/New private tab/.test(m.textContent))"));
  await q("cmenuClose()");
  // the Start menu's Oracle
  const sm = await q("(()=>{cmenu(10,10,[['Open',()=>{}],['Pin to taskbar',()=>{}]],{title:APPS.browser.name});const t=document.querySelector('.cmenu').textContent;cmenuClose();return t})()");
  ok('9. the Start menu\'s (and any menu titled Oracle Browser): New private tab after Open', /Open.*New private tab.*Pin to taskbar/.test(sm), sm);
  const other = await q("(()=>{cmenu(10,10,[['Open',()=>{}]],{title:APPS.notepad.name});const t=document.querySelector('.cmenu').textContent;cmenuClose();return t})()");
  ok('...not for other apps', !/private/i.test(other), other);
  // Oracle's ☰ menu in a normal window
  await q("browser()"); await wait(400);
  const mn = await q("(()=>{const W=wins.find(w=>!w.priv&&w.app=='browser'||!w.priv&&/Oracle/.test(w.el.querySelector('.t').textContent));W.body.querySelector('.mn').click();const t=[...W.body.querySelectorAll('.odd > div')].map(x=>x.textContent);W.body.querySelector('.odd').remove();return t})()");
  ok('10. Oracle\'s ☰ menu: New private tab, after New tab', mn[0] == 'New tab' && mn[1] == 'New private tab', mn);
  // closing the private windows: the part cleared once, a new one next time
  await q("__out.length=0;wins.filter(w=>w.priv).forEach(w=>w.close())"); await wait(300);
  const cl = await q("__out.filter(m=>m.op=='clear')");
  ok('11. the last private window closed: its private part is cleared (once)', cl.length == 1 && cl[0].part == p1, cl);
  await q("oraclePrivate('https://example.org/')"); await wait(500);
  const p2 = (await opens()).pop();
  ok('12. the next private window: a new private part (nothing of the last one)', /^priv-/.test(p2 || '') && p2 != p1, [p1, p2]);
  ok('13. the session does not reopen private windows as such (a private window is not its own app)', await q("(()=>{const W=wins.find(w=>w.priv);return !W.app||W.app=='browser'})()"));
  ok('14. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
