// 1.6.0: Oracle's pages in sandboxed frames are drawn (the Vivobook, 2026-11-15: Welcome, README and an .HTML file from File
// Explorer showed a blank white page). In Rudolf's Chromium 158 a sandboxed frame's own process sometimes started with stale
// owner properties: its document took itself for not shown (no layout, height 0) and stayed blank. Each page is opened in a
// fresh Oracle window and in one window through its address box, several rounds, and checked from inside its frame (laid out,
// its text there); the frames stay sandboxed (allow-scripts, the desktop not reachable); a tel: link clicked in a local
// page asks how to call it. Needs Rudolf's Chromium (158, from the release's chromium.sfs: opt/chromium/chrome) to show the
// old fault; Playwright's own Chromium (153) draws these pages either way.
// node oraclefile60_t.js [CHROME]   (build iso/www/index.html first; PAGE=another index.html to test that one)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www'), PAGE = process.env.PAGE || path.join(WWW, 'index.html');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 700) : '')) };
const PORT = 8556;
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]); q.on('data', () => {}); q.on('end', () => {
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html; charset=utf-8', 'content-security-policy': "object-src 'none'; base-uri 'self'"});   // (rudolfd's)
    return s.end(fs.readFileSync(PAGE, 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0,engine:"chromium"};</script>')) }
  if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end('{"ok":true}') }
  const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(PORT, '127.0.0.1');
const CALL = '<!doctype html><title>Call test</title><body style="font:16px Arial;padding:20px"><h1>Call test</h1><p><a id="c" href="tel:5551234567">Call this number</a></p></body>';
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  console.log('Chromium', b.version());
  const p = await (await b.newContext({viewport: {width: 1280, height: 800}})).newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message.slice(0, 200)));
  await p.goto(`http://127.0.0.1:${PORT}/`); await p.waitForFunction(() => typeof browser == 'function' && typeof ACCTS != 'undefined' && typeof telOpen == 'function', null, {timeout: 60000});
  await p.waitForTimeout(2500);
  await p.evaluate(c => { try { svj('bd95setup', 1) } catch (e) {} ['oobe', 'login'].forEach(i => { const o = document.getElementById(i); o && o.remove() }); wins.slice().forEach(w => w.close()); USER = USER || ACCTS[0];
    let d = fs['C:']; for (const k of homePath('DOCUMENTS').slice(1)) d = d[k] || (d[k] = {}); d['CALL-TEST.HTML'] = c; S.telApp = ''; return 1 }, CALL);
  const DOCS = await p.evaluate(() => homePath('DOCUMENTS').join('/'));
  const shown = async () => { const h = await p.evaluateHandle(() => { const ws = wins.filter(w => w.body && w.body.querySelector('.otabs')); return [...ws.pop().body.querySelectorAll('.fh iframe')].find(f => f.style.display == 'block') });
    const f = await h.asElement().contentFrame(); if (!f) return {err: 'no frame'};
    const r = await f.evaluate(() => ({h: document.documentElement.getBoundingClientRect().height, t: (document.body ? document.body.innerText : '').slice(0, 40)}));
    r.attr = await h.evaluate(fr => ({sb: fr.getAttribute('sandbox'), cd: fr.contentDocument === null})); return r };
  const PAGES = [['file:///C:/README.TXT', /^README\.TXT\n/], ['file:///C:/WELCOME.HTM', /Welcome to Oracle/], ['file:///' + DOCS + '/CALL-TEST.HTML', /Call test\n/], ['about:oracle', /^About Oracle\n/], ['about:files?p=C:', /^Index of C:\n/]];
  const ROUNDS = +process.env.ROUNDS || 4; const bad = [], bad2 = [], open = [];
  for (let k = 0; k < ROUNDS; k++) for (const [u, re] of PAGES) {
    // a fresh window (File Explorer's double-click on an .HTML file opens one: openPath)
    await p.evaluate(([u, k]) => { wins.slice().forEach(w => w.close()); if (/CALL-TEST/.test(u) && k % 2) openPath(homePath('DOCUMENTS').concat('CALL-TEST.HTML')); else browser(u) }, [u, k]);
    await p.waitForTimeout(1500); let r = await shown();
    if (!(r.h > 0 && re.test(r.t))) bad.push([k, u, r]); if (!(r.attr && r.attr.sb == 'allow-scripts' && r.attr.cd)) open.push([u, r.attr]);
  }
  ok(`1. every page in a fresh Oracle window is drawn (${ROUNDS * PAGES.length} times: laid out, its text there)`, !bad.length, bad.slice(0, 4));
  await p.evaluate(() => { wins.slice().forEach(w => w.close()); browser('about:home') }); await p.waitForTimeout(1200);
  for (let k = 0; k < ROUNDS; k++) for (const [u, re] of PAGES) {
    // one window, the address typed (as a bookmark's click: nav)
    await p.evaluate(u => { const W = wins.filter(w => w.body && w.body.querySelector('.otabs')).pop(), ad = W.body.querySelector('.ad'); ad.value = u; ad.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter', bubbles: true})) }, u);
    await p.waitForTimeout(1200); const r = await shown();
    if (!(r.h > 0 && re.test(r.t))) bad2.push([k, u, r]); if (!(r.attr && r.attr.sb == 'allow-scripts' && r.attr.cd)) open.push([u, r.attr]);
  }
  ok(`2. ...and in one window, one page after another (${ROUNDS * PAGES.length} times)`, !bad2.length, bad2.slice(0, 4));
  ok('3. the pages stay in sandboxed frames (allow-scripts only; the desktop page not reachable from them)', !open.length, open.slice(0, 3));
  // the phone-number test: a local page's tel: link, clicked
  await p.evaluate(() => { wins.slice().forEach(w => w.close()); openPath(homePath('DOCUMENTS').concat('CALL-TEST.HTML')) }); await p.waitForTimeout(1500);
  const fh = await p.evaluateHandle(() => [...wins.filter(w => w.body && w.body.querySelector('.otabs')).pop().body.querySelectorAll('.fh iframe')].find(f => f.style.display == 'block'));
  const fr = await fh.asElement().contentFrame(); await fr.click('#c', {timeout: 5000}).catch(e => console.log('(the link could not be clicked: ' + e.message.split('\n')[0] + ')')); await p.waitForTimeout(800);
  const dlg = await p.evaluate(() => { const d = document.querySelector('.rmodal [aria-label="Call a phone number"]'); return d ? d.innerText.slice(0, 400) : null });
  ok('4. a tel: link clicked in a local page: the desktop asks how to call it (5551234567)', dlg && /5551234567/.test(dlg) && /Copy the number/.test(dlg), dlg);
  await fr.evaluate(() => document.getElementById('c').click()); await p.waitForTimeout(500);   // (the question covers the page now)
  ok('5. ...once (a second click while it asks adds no second question)', await p.evaluate(() => document.querySelectorAll('.rmodal [aria-label="Call a phone number"]').length) == 1);
  await p.evaluate(() => document.querySelectorAll('.rmodal').forEach(o => o.remove()));
  // View source shows the file as it is: the tel: script is Oracle's, not the file's
  await p.evaluate(() => { const W = wins.filter(w => w.body && w.body.querySelector('.otabs')).pop(); W.body.querySelector('.mn').click(); [...W.body.querySelectorAll('.odd div')].find(d => d.textContent == 'View source').click() });
  await p.waitForTimeout(1200); const vs = await (async () => { const h = await p.evaluateHandle(() => [...wins.filter(w => w.body && w.body.querySelector('.otabs')).pop().body.querySelectorAll('.fh iframe')].find(f => f.style.display == 'block')); return (await h.asElement().contentFrame()).evaluate(() => document.body.innerText) })();
  ok('6. View source shows the file as it is (Oracle\'s tel: script is not in it)', /tel:5551234567/.test(vs) && !/closest/.test(vs), vs.slice(0, 300));
  ok('7. no page errors', !errs.length, errs);
  console.log(`${R.filter(x => x).length}/${R.length} passed`); await b.close(); srv.close(); process.exit(R.every(x => x) ? 0 : 1) })().catch(e => { console.log('FAIL', e.stack); process.exit(1) });
