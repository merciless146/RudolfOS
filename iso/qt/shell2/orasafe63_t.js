// 1.6.3 (the owner, 2026-11-24): Oracle's safety and your data (about:safety, orasafe63.js): in the default bookmarks
// (and added once to bookmarks kept from before), every entry there, and about:safety#ENTRY scrolling to that entry and
// highlighting it (the extensions reminder's "Learn more" goes to about:safety#extensions-desktop).
// node orasafe63_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 400) : '')) };
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }
  const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200); fs.createReadStream(f).pipe(s) }).listen(8711, '127.0.0.1');
const prep = "(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';return 1})()";
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const ctx = await b.newContext({viewport: {width: 1280, height: 800}}); const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.stack.slice(0, 300)));
  const start = async () => { await p.goto('http://127.0.0.1:8711/'); await p.waitForFunction(() => typeof browser == 'function' && typeof oraSafetyPage == 'function', null, {timeout: 30000}); await p.waitForTimeout(1200); await p.evaluate(prep) };
  const open = async () => { await p.evaluate(() => { wins.filter(w => w.el.classList.contains('oracle')).forEach(w => w.close()); browser() }); await p.waitForTimeout(1200) };
  const marks = () => p.evaluate(() => { const W = wins.filter(w => w.el.classList.contains('oracle')).pop(); return [...W.body.querySelectorAll('.obm button')].map(x => [x.textContent, x.title]).filter(x => /:/.test(x[1])) });
  // 1. fresh: Safety in the default bookmarks, before About
  await start(); await p.evaluate(() => { localStorage.removeItem('bd95bm'); localStorage.removeItem('bd95bmsafe') }); await open();
  let m = await marks(); let i = m.findIndex(x => x[1] == 'about:safety');
  ok('1. the default bookmarks have Safety (about:safety), just before About', i >= 0 && m[i][0] == 'Safety' && m[i + 1] && m[i + 1][1] == 'about:oracle', m);
  // 2. bookmarks kept from before: Safety added once; removed by the person, it stays removed
  await p.evaluate(() => { localStorage.setItem('bd95bm', JSON.stringify([['Home', 'about:home'], ['Mine', 'https://example.org/'], ['About', 'about:oracle']])); localStorage.removeItem('bd95bmsafe') });
  await start(); await open(); m = await marks();
  ok('2. bookmarks kept from before get Safety once, before About, with theirs kept', JSON.stringify(m.map(x => x[1])) == JSON.stringify(['about:home', 'https://example.org/', 'about:safety', 'about:oracle']), m);
  await p.evaluate(() => { localStorage.setItem('bd95bm', JSON.stringify([['Home', 'about:home'], ['About', 'about:oracle']])) });
  await start(); await open(); m = await marks();
  ok('...and once only: removed by the person, it is not added again', !m.some(x => x[1] == 'about:safety'), m);
  // 3. the page and its entries
  const go = async u => { await p.evaluate(u => { const W = wins.filter(w => w.el.classList.contains('oracle')).pop(), a = W.body.querySelector('.ad'); a.value = u; a.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'})) }, u); await p.waitForTimeout(1200) };
  const frame = async () => { for (const f of p.frames()) { try { if (await f.evaluate(() => !!document.getElementById('os-toc'))) return f } catch (e) {} } return null };
  await go('about:safety'); let f = await frame();
  const info = f && await f.evaluate(() => ({title: document.title, ids: [...document.querySelectorAll('.os-e')].map(e => e.id), toc: document.querySelectorAll('#os-toc a').length, hl: document.querySelectorAll('.os-e.hl').length}));
  ok('3. about:safety: "Oracle\'s safety and your data", nine entries with a contents list, none highlighted', info && /safety and your data/.test(info.title) && info.ids.length == 9 && info.toc == 9 && info.hl == 0 && info.ids.includes('extensions-desktop'), info);
  const txt = f && await f.evaluate(() => document.getElementById('extensions-desktop').textContent);
  ok('...the extensions entry says why: the desktop\'s key, no extension at all, where they may run', txt && /holds the key/.test(txt) && /no extension runs at all/.test(txt) && /Settings › Oracle › Extensions/.test(txt) && /Keep my websites encrypted/.test(txt), txt && txt.slice(0, 200));
  // 4. about:safety#extensions-desktop: scrolled to it and highlighted (what "Learn more" opens)
  await go('about:safety#extensions-desktop'); f = await frame();
  const hl = f && await f.evaluate(() => { const e = document.getElementById('extensions-desktop'), r = e.getBoundingClientRect(); return {hl: e.classList.contains('hl'), others: document.querySelectorAll('.os-e.hl').length, inView: r.top >= 0 && r.top < innerHeight, scrolled: scrollY > 0 || document.scrollingElement.scrollTop > 0, bg: getComputedStyle(e).backgroundColor} });
  ok('4. about:safety#extensions-desktop: scrolled to that entry and only it highlighted', hl && hl.hl && hl.others == 1 && hl.inView && hl.bg != 'rgba(0, 0, 0, 0)', hl);
  // 5. a contents link moves the highlight
  if (f) await f.evaluate(() => [...document.querySelectorAll('#os-toc a')].find(a => /Tor windows/.test(a.textContent)).click()); await p.waitForTimeout(300);
  const mv = f && await f.evaluate(() => [...document.querySelectorAll('.os-e.hl')].map(e => e.id));
  ok('5. a contents link highlights its entry instead', JSON.stringify(mv) == '["tor"]', mv);
  // 6. an unknown entry: the page, nothing highlighted
  await go('about:safety#nope'); f = await frame();
  const un = f && await f.evaluate(() => document.querySelectorAll('.os-e.hl').length);
  ok('6. an unknown entry opens the page with nothing highlighted', un === 0, un);
  ok('7. no page errors', !errs.length, errs);
  await b.close(); srv.close(); const P = R.filter(Boolean).length; console.log(`${P} of ${R.length} passed`); process.exit(P == R.length ? 0 : 1)
})().catch(e => { console.log('FAIL', e.stack); process.exit(1) });
