// 1.6.3 (docs/plan-1.6.3.md Part 4): each of Oracle's tabs says its memory in its tooltip (desktop/oratip63.js). With a stand-in
// shell and a stand-in rudolfd whose processes reply is what rudolf-launch writes (iso/qt/native/oratabmem63_t.js checks that side
// with a real Chromium): the tooltip after the mouse rests on a tab (title, address, memory); a tab alone in its process; one with
// frames of other sites; two tabs sharing a process; a freed tab (💤); Oracle's own page; not measured yet; asked again only while
// it shows (nothing asked once it hides); the address open twice in different processes; the measurement from Oracle's own
// Chromium (Native, the encrypted setting: "oframes"); Lite's own way; the browser version (no rudolfd).
//   node oratip63_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 500) : '')) };
const wait = ms => new Promise(r => setTimeout(r, ms));
const fnv = u => { u = u.split('#')[0]; let h = 0x811c9dc5; for (let i = 0; i < u.length; i++) { h ^= u.charCodeAt(i) & 0xff; h = Math.imul(h, 0x01000193) >>> 0 } return h };
/* the stand-in rudolfd: /api/processes as rudolfd answers it (its items; "pages" from the desktop's rudolf-launch, "oframes" from
   Oracle's own); every other request a plain {ok:true} */
let ASKED = 0, MODE = 'pages', LITE = false;
const U = {alone: 'https://alone.example/a', ads: 'https://news.example/front#top', s1: 'https://shop.example/one', s2: 'https://shop.example/two', twice: 'https://twice.example/'};
function procs() {
  const now = Math.floor(Date.now() / 1000), fr = [], mem = {};
  const add = (u, p, f, up, m) => fr.push({h: fnv(u), p, m: m ? 1 : 0, f, up: up || ''});
  add('isolated-app://shell/', 100, 'SHELL', '', 1); add('http://127.0.0.1:8080/', 101, 'DESK', 'SHELL', 1);
  add(U.alone, 201, 'A', '', 1);
  add(U.ads, 202, 'N', '', 1); add('https://ads.example/x', 203, 'N1', 'N', 0); add('https://cdn.example/y', 204, 'N2', 'N', 0); add('https://cdn.example/z', 204, 'N3', 'N1', 0);
  add(U.s1, 205, 'S1', '', 1); add(U.s2, 205, 'S2', '', 1); add('https://other.example/', 205, 'S3', '', 1);
  add(U.twice, 206, 'T1', '', 1); add(U.twice, 207, 'T2', '', 1);
  Object.assign(mem, {100: [40000, 30000], 101: [90000, 80000], 201: [122880, 110000], 202: [102400, 90000], 203: [51200, 40000], 204: [20480, 15000], 205: [184320, 160000], 206: [30000, 20000], 207: [31000, 20000]});
  const items = [{pid: 201, rss: 200 * 1048576, name: 'chrome'}, {pid: 301, rss: 150 * 1048576, name: 'WebKitWebProces'}, {pid: 302, rss: 250 * 1048576, name: 'WebKitWebProces'}, {pid: 303, rss: 50 * 1048576, name: 'rudolf-webkit'}];
  const m = {time: now, ftime: now, frames: fr, mem};
  return {ok: true, threads: 4, items, pages: MODE == 'pages' ? Object.assign({pages: []}, m) : {time: now, pages: []}, oframes: MODE == 'oframes' ? [m] : MODE == 'stale' ? [Object.assign({}, m, {ftime: now - 100})] : []};
}
const srv = http.createServer((q, s) => {
  if (q.url.startsWith('/api/')) { if (q.url.startsWith('/api/processes')) ASKED++;
    s.writeHead(200, {'content-type': 'application/json'}); s.end(JSON.stringify(q.url.startsWith('/api/processes') ? procs() : {ok: true})); return }
  s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html')));
}).listen(8728, '127.0.0.1');
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  async function desk(rd) {
    const p = await b.newPage({viewport: {width: 1280, height: 800}}); const errs = []; p.on('pageerror', e => errs.push(e.message));
    if (rd) await p.addInitScript(e => { window.RUDOLFD = {token: 'test', engine: e} }, rd);
    await p.goto('http://127.0.0.1:8728/'); await p.waitForFunction(() => typeof oTabTip == 'function' && typeof browser == 'function', null, {timeout: 30000});
    await p.evaluate(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
      window.__sent=[];window.__net=0;SHELL=window;SHELL_CAPS=['tabs'];window.postMessage=m=>{__sent.push(m)};return 1})()`);
    return {p, errs};
  }
  const tabs = async (p, urls) => { await p.evaluate(u => { browser(u[0]); const W = document.querySelector('.win.act');
    u.slice(1).forEach(x => { W.querySelector('.otabs button').click(); const a = W.querySelector('input.ad, .ad'); a.value = x; a.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'})) }) }, urls); await wait(300) };
  const tip = async (p, k, ms) => { await p.mouse.move(5, 5); await wait(150); const r = await p.evaluate(k => { const e = document.querySelectorAll('.win.act .otab')[k].getBoundingClientRect(); return [e.left + 20, e.top + 8] }, k);
    await p.mouse.move(r[0], r[1]); await wait(ms || 2200);
    return p.evaluate(() => { const e = document.getElementById('otip63'); return e && !e.hidden ? [...e.children].map(x => x.hidden ? '' : x.textContent) : null }) };
  // ---- the desktop's Chromium (rudolfd's reply: "pages")
  {
    const {p, errs} = await desk('chromium');
    await tabs(p, [U.alone, U.ads, U.s1, U.s2, U.twice, U.twice, 'about:history']);
    ok('1. seven tabs open, six of them real (the shell\'s)', (await p.evaluate("document.querySelectorAll('.win.act .otab').length")) == 7 && (await p.evaluate("__sent.filter(m=>m.op=='open').length")) == 6);
    ok('2. each tab is named for screen readers (its title and address)', await p.evaluate("[...document.querySelectorAll('.win.act .otab')].every(d=>/alone|news|shop|twice|History/.test(d.getAttribute('aria-label')))"));
    const t0 = await p.evaluate(() => { const k = document.querySelectorAll('.win.act .otab')[0].getBoundingClientRect(); return [k.left + 20, k.top + 8] });
    await p.mouse.move(t0[0], t0[1]); await wait(300);
    ok('3. no tooltip before the mouse has rested on the tab (0.6 s), and the browser\'s own (the title) not shown while the mouse is on it', await p.evaluate("(!document.getElementById('otip63')||document.getElementById('otip63').hidden)&&!document.querySelectorAll('.win.act .otab')[0].hasAttribute('title')"));
    await p.mouse.move(5, 5); let t = await tip(p, 0);
    ok('4. a tab alone in its process: its title, its address, its process\'s memory (120 MB)', t && t[0] == 'alone.example' && t[1] == U.alone && t[2] == 'Memory: 120 MB', t);
    t = await tip(p, 1);
    ok('5. a tab with frames of other sites: the page and the frames said apart (3 frames in 2 processes: 50 + 20 MB)', t && t[2] == 'Memory: 170 MB (the page 100 MB; 3 frames from other sites 70 MB)', t);
    t = await tip(p, 2);
    ok('6. tabs sharing a process: "shares a process with 2 other tabs" (one of them in another window), together', t && t[2] == 'Memory: shares a process with 2 other tabs: 180 MB together', t);
    t = await tip(p, 4);
    ok('7. one address open in two tabs in different processes: not told apart, said so', t && /not told apart/.test(t[2]), t);
    t = await tip(p, 6);
    ok('8. Oracle\'s own page (History): part of Oracle\'s page', t && t[2] == 'Memory: part of Oracle\'s own page', t);
    // asked only while shown
    await p.mouse.move(5, 5); await wait(500); const a0 = ASKED; await wait(4000);
    ok('9. nothing asked of rudolfd while no tooltip shows', ASKED == a0, [a0, ASKED]);
    t = await tip(p, 0, 7500);
    ok('10. while it shows, measured again every few seconds (fresh numbers)', ASKED - a0 >= 3 && t && t[2] == 'Memory: 120 MB', [ASKED - a0, t]);
    await p.mouse.move(5, 5); await wait(300);
    ok('11. it hides when the mouse leaves, and the tab has its title again (as other tests and tools read it)', await p.evaluate("document.getElementById('otip63').hidden&&document.querySelectorAll('.win.act .otab')[0].title=='alone.example'"));
    // a freed tab
    await p.evaluate("(()=>{const w=[...RT.values()].find(w=>!w.shown()&&/alone/.test(w.url()));return w.sleep(0)})()"); await wait(200);
    t = await tip(p, 0);
    ok('12. a freed tab (💤): none until chosen', t && t[2] == 'Memory: none until you choose it (💤 freed)', t);
    // not measured yet: a tab whose address is in no measurement
    await p.evaluate(u => { const W = document.querySelector('.win.act'); W.querySelector('.otabs button').click(); const a = W.querySelector('input.ad, .ad'); a.value = u; a.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'})) }, 'https://new.example/'); await wait(200);
    t = await tip(p, 7, 2500);
    ok('13. a tab not in the measurement yet: "not measured yet"', t && t[2] == 'Memory: not measured yet', t);
    ok('14. the tooltip draws no page content as HTML (text only) and stays on the screen', await p.evaluate("(()=>{const e=document.getElementById('otip63'),r=e.getBoundingClientRect();return e.querySelectorAll('*').length==3&&r.left>=0&&r.right<=innerWidth&&r.bottom<=innerHeight})()"));
    ok('15. the page of the shown tab is cut around the tooltip (as for a menu: the shell is told)', await p.evaluate("(()=>{const w=[...RT.values()].find(w=>w.shown());const c=w&&rtCover(w.el);return !!(c&&c.vis)})()"));
    ok('16. no page errors', errs.length == 0, errs);
    await p.close();
  }
  // ---- Oracle's own Chromium (Native, the encrypted setting): rudolfd's "oframes"; and a stale one
  {
    MODE = 'oframes'; const {p, errs} = await desk('chromium');
    await tabs(p, [U.alone, U.ads]);
    let t = await tip(p, 1);
    ok('17. Oracle\'s own Chromium (Native, encrypted): the same from its own rudolf-launch\'s file', t && t[2] == 'Memory: 170 MB (the page 100 MB; 3 frames from other sites 70 MB)', t);
    MODE = 'stale'; t = await tip(p, 0, 2500);
    ok('18. a measurement from before the tooltip opened is not shown as fresh: measuring…', t && t[2] == 'Memory: measuring…', t);
    t = await tip(p, 0, 9500);
    ok('19. ...and if none comes, the last one (under 30 s) is not used either: said not known', t && /not known/.test(t[2]), t);
    ok('20. no page errors', errs.length == 0, errs);
    await p.close(); MODE = 'pages';
  }
  // ---- Lite (WebKit): its own way, said plainly
  {
    const {p, errs} = await desk('webkit');
    await tabs(p, [U.alone]);
    const t = await tip(p, 0);
    ok('21. Lite: WebKit does not say which process shows which tab: said, with its page processes together (400 MB)', t && t[2] == 'Memory: Lite\'s WebKit does not say which page process shows which tab; its 2 page processes (the desktop\'s own among them) use 400 MB together', t);
    ok('22. no page errors', errs.length == 0, errs);
    await p.close();
  }
  // ---- the browser version (no rudolfd)
  {
    const {p, errs} = await desk(null);
    await tabs(p, [U.alone]);
    const t = await tip(p, 0);
    ok('23. the browser version: measured on Rudolf OS itself', t && t[2] == 'Memory: measured on Rudolf OS itself', t);
    ok('24. no page errors', errs.length == 0, errs);
    await p.close();
  }
  await b.close(); srv.close();
  const f = R.filter(x => !x).length; console.log(f ? `FAIL ${f} of ${R.length}` : `PASS ${R.length}/${R.length}`); process.exit(f ? 1 : 0);
})().catch(e => { console.log('FAIL ' + e.stack); process.exit(1) });
