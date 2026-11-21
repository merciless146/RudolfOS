// 1.6.1 (docs/plan-1.6.1.md Part 10, docs/plan-oracle.md item 5): Oracle's ad and tracker blocking, the desktop's side (orablock61.js)
// with a stood-in shell (rudolf-webkit's protocol, the same tab messages as the Chromium shell): the list in the page is the one
// blocklist.py makes from Rudolf's own list; the lists go to the shell before the first tab (op block) and again after a new hello;
// the shield shows the tab's count (tabev adblock) and hides on Oracle's own pages; its menu: the site, the count, Turn off / on for
// this site (the lists sent again, the page loaded again), Report a broken site (System health only); a private window always
// blocks; Settings › Privacy › Ads and trackers in Oracle: the switch, the sites it is off for, sites of your own; a website in a
// plain frame: "not possible". The shell's own check (shell.js blockWhy) on its cases.   node orablock61_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path'), cp = require('child_process');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const api = a => a == 'events' ? {ok: true, events: ''} : a == 'info' ? {ok: true, backlight: false, live: 0, boot: 'uefi', kernel: 'test'} : {ok: true};
let WK = true;
const srv = http.createServer((q, s) => { const [u0] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', async () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0,engine:"webkit"};window.__out=[];' + (WK ? 'window.webkit={messageHandlers:{rudolfShell:{postMessage:m=>__out.push(m)}}};' : '') + '</script>')) }
    if (u == '/api/events/wait') await new Promise(r => setTimeout(r, 800));
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5) == 'events/wait' ? 'events' : u.slice(5)))) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8615, '127.0.0.1');
const prep = "(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';window.__net=false;window.__sys=false;delete S.oblock;return 1})()";
(async () => {
  // A. the shell's own check
  {const src = fs.readFileSync(path.join(ROOT, 'iso/shell/shell.js'), 'utf8');
   const pick = re => { const m = re.exec(src); return m ? m[0] : '' };
   const code = [pick(/^const SLD2 = [^\n]*\n/m), pick(/^const siteOf = [^\n]*\n/m), pick(/^const inSet = [^\n]*\n/m), pick(/^function blockWhy\(t, u, ty\) \{\n[\s\S]*?\n\}\n/m)].join('');
   const hostOf = u => { try { return new URL(u).hostname.toLowerCase() } catch (e) { return '' } };
   const mk = o => ({on: true, b: new Set(o.b || []), tp: new Set(o.tp || []), ex: new Set(o.ex || []), off: new Set(o.off || [])});
   const W = (cfg, top, u, ty, part) => new Function('hostOf', 'B', 'let BLOCK=B;' + code + ';return blockWhy;')(hostOf, cfg)({part: part || 'a1', url: top}, u, ty || 'image');
   const c1 = mk({b: ['doubleclick.net'], tp: ['tracker.example'], ex: ['ok.doubleclick.net'], off: ['news.example']});
   ok('A1. a blocked domain and its sub-domains (not the page itself, not an exception)', W(c1, 'https://a.example/', 'https://x.doubleclick.net/p') && W(c1, 'https://a.example/', 'https://doubleclick.net/p') && !W(c1, 'https://a.example/', 'https://ok.doubleclick.net/p') && !W(c1, 'https://a.example/', 'https://doubleclick.net/', 'main-frame') && !W(c1, 'https://a.example/', 'https://notdoubleclick.net/p'));
   ok('A2. blocked from other sites only: not on its own site', W(c1, 'https://a.example/', 'https://cdn.tracker.example/t.js', 'script') && !W(c1, 'https://www.tracker.example/', 'https://cdn.tracker.example/t.js', 'script'));
   ok('A3. turned off for a site: nothing blocked there, except in a private window', !W(c1, 'https://www.news.example/', 'https://doubleclick.net/p') && W(c1, 'https://www.news.example/', 'https://doubleclick.net/p', 'image', 'priv-x'));
   const c2 = mk({b: ['doubleclick.net']}); c2.on = false;
   ok('A4. switched off: nothing, except in a private window', !W(c2, 'https://a.example/', 'https://doubleclick.net/p') && W(c2, 'https://a.example/', 'https://doubleclick.net/p', 'image', 'priv-g1'));
   ok('A5. the site of co.uk addresses (third parties judged by it)', !W(mk({tp: ['tracker.co.uk']}), 'https://www.tracker.co.uk/', 'https://cdn.tracker.co.uk/x') && W(mk({tp: ['tracker.co.uk']}), 'https://other.co.uk/', 'https://cdn.tracker.co.uk/x'));
   ok('A6. shell.js: the interceptor asks it before the firewall\'s answer is acted on; tabev adblock; op block', /if \(!why && tabBlock\(t, e, u\)\) return;/.test(src) && /toTab\(t\.tid, 'adblock'/.test(src) && /m\.op == 'block'/.test(src));}
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const ctx = await b.newContext({viewport: {width: 1280, height: 800}});
  const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.stack.slice(0, 300)));
  await p.goto('http://127.0.0.1:8615/'); await p.waitForFunction(() => typeof browser == 'function' && typeof rtOn == 'function' && typeof oraBlock == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s); await q(prep);
  const want = JSON.parse(cp.execFileSync('python3', [path.join(ROOT, 'iso/systools/blocklist.py'), path.join(ROOT, 'iso/systools/blocklists/rudolf.txt')]).toString());
  const have = await q('ORA_BLOCK_LIST');
  ok('B0. the page\'s list is blocklist.py\'s compile of Rudolf\'s own list (desktop/orablock61.json up to date)', JSON.stringify(have) == JSON.stringify(want) && have.b.includes('doubleclick.net') && have.b.length > 40, [have && have.b.length, want.b.length]);
  await q("window.__hl=[];hlNote=(k,m)=>__hl.push([k,m])");
  await q("__rwkIn({rudolfShell:'hello',version:'webkit-1.6.1',caps:['tabs']})"); await p.waitForTimeout(200);
  await q("browser('https://example.com/news')"); await p.waitForTimeout(600);
  const ops = await q("__out.filter(m=>m.rudolfShell=='tab').map(m=>m.op)");
  const blk = await q("__out.filter(m=>m.op=='block').pop()");
  ok('B1. the lists go to the shell before the first tab (op block): on, Rudolf\'s list', ops.indexOf('block') >= 0 && ops.indexOf('block') < ops.indexOf('open') && blk.cfg.on === true && blk.cfg.b.includes('doubleclick.net') && Array.isArray(blk.cfg.off), [ops, blk && blk.cfg.on]);
  const tid = await q("(__out.find(m=>m.rudolfShell=='tab'&&m.op=='open')||{}).tid");
  const ev = o => q(`__rwkIn(Object.assign({rudolfShell:'tabev',tid:${JSON.stringify(tid)}},${JSON.stringify(o)}))`);
  const W = "wins.find(w=>w.app=='browser'&&w.el.isConnected&&!w.priv)";
  const shield = (w = W) => q(`(()=>{const b=${w}.body.querySelector('.oblk');return b.hidden?'hidden':b.textContent+(b.classList.contains('oblkoff')?' off':'')})()`);
  await ev({ev: 'load', loading: false}); await ev({ev: 'adblock', n: 3}); await p.waitForTimeout(100);
  ok('B2. the tab counts 3: the shield says "🛡 3"', await shield() == '🛡 3', await shield());
  const menu = async (w = W) => { await q(`${w}.body.querySelector('.oblk').click()`); await p.waitForTimeout(100); return q(`[...${w}.body.querySelectorAll('.oblkm>div')].map(d=>d.textContent)`) };
  let m = await menu();
  ok('B3. its menu: "Blocking ads and trackers on example.com", "3 blocked on this page", Turn off, Report, Settings', m[0] == 'Blocking ads and trackers on example.com' && m[1] == '3 blocked on this page' && m.includes('Turn off for this site') && m.includes('Report a broken site') && m.includes('Settings…'), m);
  const n0 = await q("__out.length");
  await q(`[...${W}.body.querySelectorAll('.oblkm>div')].find(d=>d.textContent=='Turn off for this site').click()`); await p.waitForTimeout(200);
  const after = await q(`__out.slice(${n0})`);
  ok('B4. Turn off for this site: kept (S.oblock.off), the lists sent again with it, the page loaded again', JSON.stringify(await q('S.oblock.off')) == '["example.com"]' && after.some(m => m.op == 'block' && m.cfg.off.includes('example.com')) && after.some(m => m.op == 'reload' && m.tid == tid), after.map(m => m.op));
  ok('B5. ...the shield says off', await shield() == '🛡 off', await shield());
  m = await menu(); ok('B6. its menu says so, with Turn on', m[0] == 'Blocking is off on example.com' && m.includes('Turn on for this site'), m);
  await q(`[...${W}.body.querySelectorAll('.oblkm>div')].find(d=>d.textContent=='Turn on for this site').click()`); await p.waitForTimeout(150);
  ok('B7. Turn on: the site is out of the list', JSON.stringify(await q('S.oblock.off')) == '[]');
  await menu(); await q(`[...${W}.body.querySelectorAll('.oblkm>div')].find(d=>d.textContent=='Report a broken site').click()`); await p.waitForTimeout(100);
  const hl = await q('__hl');
  ok('B8. Report a broken site: a note in System health (kind firewall), nothing else', hl.length == 1 && hl[0][0] == 'firewall' && /example\.com/.test(hl[0][1]) && /nothing was sent/i.test(hl[0][1]), hl);
  await q("browser('about:oracle')"); await p.waitForTimeout(400);
  ok('B9. Oracle\'s own page: no shield', await q(`wins.filter(w=>w.app=='browser').pop().body.querySelector('.oblk').hidden`));
  // a private window
  await q("oraclePrivate?oraclePrivate('https://example.org/'):browser('https://example.org/',{priv:1})"); await p.waitForTimeout(500);
  const PW = "wins.find(w=>w.priv&&w.el.isConnected)";
  const ptid = await q("__out.filter(m=>m.op=='open').pop().tid"); await q(`__rwkIn({rudolfShell:'tabev',tid:${JSON.stringify(ptid)},ev:'adblock',n:1})`); await p.waitForTimeout(100);
  m = await menu(PW);
  ok('B10. a private window: "Private windows always block", no Turn off', m.includes('Private windows always block') && !m.includes('Turn off for this site') && await shield(PW) == '🛡 1', [m, await shield(PW)]);
  await q(`${PW}.close()`);
  // Settings › Privacy
  await q("settings('priv')"); await p.waitForTimeout(500);
  const SW = "document.querySelector('.oblkbox')";
  ok('B11. Settings › Privacy › Ads and trackers in Oracle: on, Rudolf\'s list counted, nothing sent anywhere', await q(`!!${SW}&&${SW}.querySelector('.oblkon').checked&&/blocks ${want.b.length + want.tp.length} ad and tracker services/.test(${SW}.textContent)&&/nothing about the websites you visit is sent anywhere/.test(${SW}.textContent)`), await q(`${SW}&&${SW}.textContent.slice(0,300)`));
  await q(`(()=>{const i=${SW}.querySelector('.oblkadd input');i.value='https://ads.example.net/x';${SW}.querySelector('.oblkadd button').click();return 1})()`); await p.waitForTimeout(150);
  ok('B12. a site of your own to block (from an address): kept and sent', JSON.stringify(await q('S.oblock.mine')) == '["ads.example.net"]' && (await q("__out.filter(m=>m.op=='block').pop().cfg.b.includes('ads.example.net')")));
  await q(`(()=>{const i=${SW}.querySelector('.oblkadd input');i.value='not a site';${SW}.querySelector('.oblkadd button').click();return 1})()`); await p.waitForTimeout(100);
  ok('B13. ...something that is not a site\'s name: refused', JSON.stringify(await q('S.oblock.mine')) == '["ads.example.net"]' && /not a site/.test(await q(`${SW}.querySelector('.oblkadd .msg').textContent`)));
  await q(`${SW}.querySelector('.oblkmine button').click()`); await p.waitForTimeout(100);
  ok('B14. ...removed with its ✕', JSON.stringify(await q('S.oblock.mine')) == '[]');
  await q(`(()=>{const c=${SW}.querySelector('.oblkon');c.checked=false;c.dispatchEvent(new Event('change'));return 1})()`); await p.waitForTimeout(100);
  ok('B15. switched off: kept, and the shell told', (await q('S.oblock.on')) === false && (await q("__out.filter(m=>m.op=='block').pop().cfg.on")) === false);
  await q("__rwkIn({rudolfShell:'hello',version:'webkit-1.6.1',caps:['tabs']})"); const nb = await q("__out.filter(m=>m.op=='block').length");
  await q("browser('https://example.net/')"); await p.waitForTimeout(400);
  ok('B16. a shell that said hello again (started again) is sent the lists again', (await q("__out.filter(m=>m.op=='block').length")) > nb);
  ok('no page errors', !errs.length, errs);
  // C. a website in a plain frame
  WK = false; const p2 = await ctx.newPage(); const errs2 = []; p2.on('pageerror', e => errs2.push(e.stack.slice(0, 300)));
  await p2.goto('http://127.0.0.1:8615/'); await p2.waitForFunction(() => typeof browser == 'function' && typeof oraBlock == 'function', null, {timeout: 30000});
  await p2.evaluate(prep); await p2.evaluate("browser('http://127.0.0.1:8615/x')"); await p2.waitForTimeout(800);
  await p2.evaluate(`${W}.body.querySelector('.oblk').click()`); await p2.waitForTimeout(100);
  const m2 = await p2.evaluate(`[...${W}.body.querySelectorAll('.oblkm>div')].map(d=>d.textContent)`);
  ok('C1. a website in a plain frame: the shield says blocking is not possible here', /not possible here/.test(m2[0]) && !m2.includes('Turn off for this site'), m2);
  ok('no page errors (frames)', !errs2.length, errs2);
  await b.close(); srv.close();
  const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
