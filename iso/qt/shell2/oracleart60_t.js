// 1.6.0 (2026-11-16): Oracle's own artwork (oracleart60.js). Oracle's icon is the owner's Stonehenge circle in every
// icon style and accent colour (the start menu, the window's title bar, the desktop), while the network's plain 🌐 keeps
// the globe; the start page fills the page with the softened navy and a white logo in dark mode, and is white with a
// navy logo in light mode, following Rudolf's dark theme and Oracle's Dark pages (which do not invert it).
// node oracleart60_t.js [CHROME] [SHOTDIR]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www'), SHOT = process.argv[3] || '';
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }
  const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200); fs.createReadStream(f).pipe(s) }).listen(8193, '127.0.0.1');
const prep = "(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';return 1})()";
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const p = await (await b.newContext({viewport: {width: 1280, height: 800}})).newPage(); const errs = []; p.on('pageerror', e => errs.push(e.stack.slice(0, 300)));
  await p.goto('http://127.0.0.1:8193/'); await p.waitForFunction(() => typeof browser == 'function' && typeof oracleHome == 'function', null, {timeout: 30000});
  await p.waitForTimeout(1500); await p.evaluate(prep);
  const ic = await p.evaluate(() => ({o: icSvg('oracle'), g: icSvg('globe'), map: EMO2IC['\u{1F310}️'], net: EMO2IC['\u{1F310}']}));
  ok('Oracle\'s emoji maps to its own icon, the plain globe stays the globe', ic.map == 'oracle' && ic.net == 'globe', ic);
  ok('the icon is the white-edged circle with the stones', /circle%20cx%3D%27500%27%20cy%3D%27500%27%20r%3D%27500%27%20fill%3D%27%23fff%27/.test(ic.o) && ic.o.length > 5000, ic.o.slice(0, 200));
  const styles = await p.evaluate(() => { const r = {}; for (const st of ['rudolf', 'flat', 'circle', 'outline', 'glyph', 'glass', 'retro', 'sticker']) { S.icstyle = st; r[st] = icSvg('oracle') } S.icstyle = 'rudolf'; return {same: new Set(Object.values(r)).size, acc: icSvg('oracle', '#e11d48') == r.rudolf} });
  ok('the same icon in every icon style and accent colour', styles.same == 1 && styles.acc, styles);
  await p.evaluate(() => { S.icons = 'rudolf'; iconsApply() });
  await p.evaluate(() => browser()); await p.waitForTimeout(1200);
  const where = await p.evaluate(() => { const o = icSvg('oracle'), W = wins.filter(w => w.el.classList.contains('oracle')).pop(); iconsApply();
    const tb = W && W.el.querySelector('.tb img.ico'), li = document.querySelector('#menu li[data-a="browser"] img.ico'), dk = [...document.querySelectorAll('#desk > .ic')].find(d => /Oracle/.test(d.textContent) || (d.querySelector('img.ico') || {}).dataset?.emo == '\u{1F310}️');
    const di = dk && dk.querySelector('img.ico');
    return {tb: !!tb && tb.src == o, li: !!li && li.src == o, desk: !dk || (di && di.src == o), hasDesk: !!dk} });
  ok('Oracle\'s window, start menu row and desktop icon draw the new icon', where.tb && where.li && where.desk, where);
  const frameOf = async () => { await p.waitForTimeout(700); for (const f of p.frames()) { try { if (await f.evaluate(() => !!document.documentElement.dataset.oracleHome)) return f } catch (e) {} } return null };
  const look = async () => { const f = await frameOf(); if (!f) return null; return f.evaluate(() => ({mode: document.documentElement.dataset.oracleHome, bg: getComputedStyle(document.body).backgroundColor, logo: getComputedStyle(document.querySelector('svg')).color, inv: !!document.getElementById('rudolf-dark'), links: document.querySelectorAll('a[data-u]').length})) };
  await p.evaluate(() => { document.documentElement.classList.remove('dark'); oracleHomeRefresh() });
  const shot = async n => { if (!SHOT) return; const W = await p.evaluateHandle(() => wins.filter(w => w.el.classList.contains('oracle')).pop().el); await W.asElement().screenshot({path: path.join(SHOT, n)}) };
  let L = await look();
  ok('light mode: a white page with the logo in the navy', L && L.mode == 'light' && L.bg == 'rgb(255, 255, 255)' && L.logo == 'rgb(17, 28, 44)' && !L.inv && L.links == 9, L); await shot('oracle-home-light.png');
  await p.evaluate(() => { document.documentElement.classList.add('dark'); oracleHomeRefresh() });
  L = await look();
  ok('Rudolf\'s dark theme: the page in the softened navy with a white logo', L && L.mode == 'dark' && L.bg == 'rgb(17, 28, 44)' && L.logo == 'rgb(255, 255, 255)' && !L.inv, L); await shot('oracle-home-dark.png');
  await p.evaluate(() => { document.documentElement.classList.remove('dark'); oracleHomeRefresh(); webDarkSet(true) });
  L = await look();
  ok('Oracle\'s Dark pages: the dark start page, not an inverted one', L && L.mode == 'dark' && !L.inv, L);
  const other = await p.evaluate(() => webDarkHtml('<p>x</p>').includes('rudolf-dark')); await p.evaluate(() => webDarkSet(false));
  ok('Dark pages still invert Oracle\'s other pages', other);
  ok('no page errors', !errs.length, errs);
  await b.close(); srv.close(); const n = R.filter(Boolean).length; console.log(n + '/' + R.length + ' passed'); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
