// 1.6.1 (docs/plan-1.6.1.md Part 10, docs/plan-oracle.md item 2): Oracle's closed tabs (desktop/oraclosed61.js). The desktop with a
// stood-in shell (rudolf-webkit's protocol, the same tab messages as the Chromium shell): three tabs closed and Ctrl+Shift+T three
// times (newest first, each at its place, Oracle's back list kept, a website's tab a real tab again at its address and zoom); Ctrl+Shift+T
// from inside a website (tabev keys cst); ☰ › Reopen closed tab and Recently closed…; a closed window one entry, reopened as a window
// with its tabs; Oracle's last window closed: the list forgotten; a private window's tabs only in its own list, gone with it; sign-out
// forgets the list; nothing of it is written to the page's storage.   node oraclosed61_t.js [CHROME]   (build iso/www/index.html first)
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
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8613, '127.0.0.1');
const prep = "(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';USER.id=USER.id||'utest';window.__net=false;window.__sys=false;return 1})()";
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const ctx = await b.newContext({viewport: {width: 1280, height: 800}});
  const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.stack.slice(0, 300)));
  await p.goto('http://127.0.0.1:8613/'); await p.waitForFunction(() => typeof browser == 'function' && typeof rtOn == 'function' && typeof oraClosed == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s); await q(prep);
  await q("__rwkIn({rudolfShell:'hello',version:'webkit-1.6.1',caps:['tabs']})"); await p.waitForTimeout(200);
  const ls0 = await q("JSON.stringify(Object.fromEntries(Object.keys(localStorage).filter(k=>!/^(bd95hist|__rudolf)/.test(k)).map(k=>[k,localStorage.getItem(k)])))");
  const OW = "wins.filter(w=>w.app=='browser'&&w.el.isConnected)";
  const W = `${OW}.sort((a,b)=>(+a.el.style.zIndex||0)-(+b.el.style.zIndex||0)).pop()`;
  const tabsOf = (w = W) => q(`(()=>{const W=${w};return [...W.body.querySelectorAll('.otab')].map(d=>d.textContent.replace(/✕$/,''))})()`);
  const cur = () => q(`(()=>{const W=${W};return W.body.querySelector('.ad').value})()`);
  const ctrlShiftT = async () => { await q(`${W}.body.querySelector('.ad').focus()`); await p.keyboard.press('Control+Shift+T'); await p.waitForTimeout(300) };
  const closeTabAt = async k => { await q(`${W}.body.querySelectorAll('.otab')[${k}].querySelector('u').click()`); await p.waitForTimeout(200) };
  // a window: Oracle's own page with a back list (About, then Bookmarks), and three websites (real tabs)
  await q("browser('about:oracle')"); await p.waitForTimeout(500);
  await q(`${W}.body.querySelector('.ad').value='about:bookmarks';${W}.body.querySelector('.ad').dispatchEvent(new KeyboardEvent('keydown',{key:'Enter'}))`); await p.waitForTimeout(300);
  for (const u of ['https://one.example/', 'https://two.example/', 'https://three.example/']) { await q(`${W}.oNewTab('${u}')`); await p.waitForTimeout(250) }
  await q(`(()=>{const W=${W};return 1})()`);
  let T0 = await tabsOf(); ok('0. four tabs: Bookmarks and three websites', T0.length == 4, T0);
  // zoom on "two", then close two, three (index 2 then 2), and Bookmarks (0)
  await q(`${W}.body.querySelectorAll('.otab')[2].click()`); await p.waitForTimeout(100); await q(`(()=>{const W=${W};W.el.dispatchEvent(new KeyboardEvent('keydown',{key:'+',ctrlKey:true,bubbles:true}));return 1})()`);
  await closeTabAt(2); await closeTabAt(2); await closeTabAt(0);
  ok('1. three tabs closed: one left', (await tabsOf()).length == 1, await tabsOf());
  const opens0 = await q("__out.filter(m=>m.op=='open').length");
  await ctrlShiftT(); let T = await tabsOf();
  ok('2. Ctrl+Shift+T: the last closed (Bookmarks) back at its place (first)', T.length == 2 && /Bookmarks/.test(T[0]), T);
  await q(`${W}.body.querySelector('.ad').blur()`); await q(`${W}.body.querySelector('.bk').click()`); await p.waitForTimeout(300);
  ok('3. ...with its back list: Back goes to About Oracle', await cur() == 'about:oracle' && /About Oracle/.test(await q(`${W}.body.querySelector('.otab.on').textContent`)), await cur());
  await ctrlShiftT(); T = await tabsOf();
  ok('4. again: "three" back, at its place (third)', T.length == 3 && /three\.example/.test(T[2]), T);
  const op1 = await q("__out.filter(m=>m.op=='open').slice(-1)[0]");
  ok('5. ...a real tab again at its address (op open)', op1 && op1.url == 'https://three.example/' && (await q("__out.filter(m=>m.op=='open').length")) == opens0 + 1, op1);
  // the third reopen from inside a website (tabev keys cst)
  const tid = op1.tid; await q(`__rwkIn({rudolfShell:'tabev',tid:${JSON.stringify(tid)},ev:'keys',k:'cst'})`); await p.waitForTimeout(300); T = await tabsOf();
  const z = await q("__out.filter(m=>m.op=='zoom').slice(-1)[0]");
  ok('6. Ctrl+Shift+T inside a website (keys cst): "two" back at its place, with its zoom', T.length == 4 && /two\.example/.test(T[2]) && z && Math.abs(z.z - 1.1) < .01, [T, z]);
  await ctrlShiftT(); ok('7. nothing left: "No recently closed tabs", no tab added', (await tabsOf()).length == 4 && (await q(`${W}.body.querySelector('.stat').textContent`)) == 'No recently closed tabs', await q(`${W}.body.querySelector('.stat').textContent`));
  // the menu
  await closeTabAt(3);
  await q(`${W}.body.querySelector('.mn').click()`); await p.waitForTimeout(150);
  const items = await q(`[...${W}.body.querySelectorAll('.odd>div')].map(d=>d.textContent)`);
  ok('8. ☰ has Reopen closed tab (Ctrl+Shift+T) and Recently closed…', items.includes('Reopen closed tab (Ctrl+Shift+T)') && items.includes('Recently closed…'), items);
  await q(`[...${W}.body.querySelectorAll('.odd>div')].find(d=>d.textContent=='Recently closed…').click()`); await p.waitForTimeout(150);
  const rc = await q(`[...${W}.body.querySelectorAll('.odd.ocl>div')].map(d=>d.textContent)`);
  ok('9. Recently closed… lists the closed tab', rc.length == 1 && /three\.example/.test(rc[0]), rc);
  await q(`${W}.body.querySelector('.odd.ocl>div').click()`); await p.waitForTimeout(300);
  ok('10. ...clicked: reopened', (await tabsOf()).length == 4);
  // a window closed: one entry, reopened as a window of its own with its tabs
  await q("browser('https://win.example/a')"); await p.waitForTimeout(300); await q(`${W}.oNewTab('https://win.example/b')`); await p.waitForTimeout(300);
  ok('11. a second window with two tabs', (await q(`${OW}.length`)) == 2 && (await tabsOf()).length == 2);
  await q(`${W}.close()`); await p.waitForTimeout(300);
  await q(`${W}.body.querySelector('.mn').click()`); await p.waitForTimeout(100); await q(`[...${W}.body.querySelectorAll('.odd>div')].find(d=>d.textContent=='Recently closed…').click()`); await p.waitForTimeout(150);
  const rc2 = await q(`[...${W}.body.querySelectorAll('.odd.ocl>div')].map(d=>d.textContent)`);
  ok('12. the closed window is one entry ("Window: …, 2 tabs")', rc2.length == 1 && /^🗗 Window: .*\(2 tabs\)$/.test(rc2[0]), rc2);
  await q(`${W}.body.querySelector('.odd.ocl>div').click()`); await p.waitForTimeout(600);
  const nw = await q(`${OW}.length`); const wt = await tabsOf();
  ok('13. ...reopened: a window of its own with both tabs', nw == 2 && wt.length == 2 && wt.every(x => /win\.example/.test(x)), [nw, wt]);
  // a private window: its own list
  await q("oraclePrivate?oraclePrivate('https://priv.example/'):browser('https://priv.example/',{priv:1})"); await p.waitForTimeout(500);
  const PW = `${OW}.find(w=>w.priv)`;
  await q(`${PW}.oNewTab('https://priv2.example/')`); await p.waitForTimeout(300);
  await q(`${PW}.body.querySelectorAll('.otab')[1].querySelector('u').click()`); await p.waitForTimeout(200);
  ok('14. a private window\'s closed tab is not in the shared list', !(await q("ORA_CL.l.some(e=>JSON.stringify(e).includes('priv2'))")));
  await q(`${PW}.body.querySelector('.ad').focus()`); await p.keyboard.press('Control+Shift+T'); await p.waitForTimeout(300);
  ok('15. ...Ctrl+Shift+T in the private window brings it back there', (await q(`[...${PW}.body.querySelectorAll('.otab')].map(d=>d.textContent).join(' ')`)).includes('priv2'));
  await q(`${PW}.close()`); await p.waitForTimeout(300);
  ok('16. the private window closed: nothing of it kept', !(await q("JSON.stringify(ORA_CL.l).includes('priv')")));
  // nothing written to the page's storage
  const ls1 = await q("JSON.stringify(Object.fromEntries(Object.keys(localStorage).filter(k=>!/^(bd95hist|__rudolf)/.test(k)).map(k=>[k,localStorage.getItem(k)])))");
  ok('17. the closed tabs are kept in memory only: nothing in the page\'s storage names them', !/three\.example|win\.example|priv/.test(ls1) && !/closed/i.test(Object.keys(JSON.parse(ls1)).join(' ')), Object.keys(JSON.parse(ls1)).filter(k => !(k in JSON.parse(ls0))));
  // Oracle's last window closed: the list forgotten
  await q(`(()=>{const W=${W};W.body.querySelectorAll('.otab')[0].querySelector('u').click();return 1})()`); await p.waitForTimeout(200);
  ok('18. a tab closed: listed', (await q("ORA_CL.l.length")) > 0);
  await q(`${OW}.forEach(w=>w.close())`); await p.waitForTimeout(300);
  ok('19. Oracle\'s last window closed: the list is forgotten', (await q("ORA_CL.l.length")) == 0, await q("ORA_CL.l.map(e=>e.title)"));
  await q("browser('about:oracle')"); await p.waitForTimeout(400); await ctrlShiftT();
  ok('20. ...a new Oracle window: Ctrl+Shift+T has nothing to reopen', (await tabsOf()).length == 1);
  // sign-out forgets it
  await q(`${W}.oNewTab('https://four.example/')`); await p.waitForTimeout(250); await q("browser('about:oracle')"); await p.waitForTimeout(300);
  await q(`${OW}[0].body.querySelectorAll('.otab')[1].querySelector('u').click()`); await p.waitForTimeout(200);
  ok('21. before signing out: one closed tab listed', (await q("ORA_CL.l.length")) == 1);
  await q("userLogout()"); await p.waitForTimeout(300);
  ok('22. signed out: the list is empty, and stays so (windows closing as the session ends add nothing)', (await q(`${OW}.forEach(w=>w.close()),ORA_CL.l.length`)) == 0 && (await q("ORA_CL.off")) === true);
  ok('23. Native: window.__oaLogout forgets it too', await q("ORA_CL.off=false;ORA_CL.l.push({uid:'x'});window.__oaLogout();ORA_CL.l.length==0&&ORA_CL.off"));
  ok('no page errors', !errs.length, errs);
  await b.close(); srv.close();
  const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
