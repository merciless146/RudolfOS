// 1.5.8 (plan 7.5): the browser build as a web app on a phone: a manifest (full screen, the icon), and a service worker
// that keeps the page so it opens offline. On an installed Rudolf OS (rudolfd answers) no service worker is registered.
// node webapp58_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
const T = {'.js': 'text/javascript', '.mjs': 'text/javascript', '.html': 'text/html', '.webmanifest': 'application/manifest+json', '.png': 'image/png', '.wasm': 'application/wasm'};
let RDMODE = false;
const srv = http.createServer((q, s) => { let u = decodeURIComponent(q.url.split('?')[0]); if (u == '/') u = '/index.html';
  if (u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); let h = fs.readFileSync(path.join(WWW, 'index.html'), 'utf8'); if (RDMODE) h = h.replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>'); return s.end(h) }
  if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end('{"ok":true,"events":""}') }
  const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8183, '127.0.0.1');
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const ctx = await b.newContext(); const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto('http://127.0.0.1:8183/'); await p.waitForFunction(() => typeof notify == 'function', null, {timeout: 30000});
  const man = await p.evaluate(async () => { const l = document.querySelector('link[rel=manifest]'); if (!l) return null; return (await fetch(l.href)).json() });
  ok('1. a manifest: Rudolf OS, full screen, the icon', man && man.name == 'Rudolf OS' && man.display == 'fullscreen' && man.icons[0].src == 'favicon.png', man);
  const reg = await p.evaluate(async () => { const r = await navigator.serviceWorker.ready; return !!r.active });
  ok('2. the service worker is registered and active', reg);
  await p.reload(); await p.waitForTimeout(1500);
  await ctx.setOffline(true); await p.reload(); const t = await p.waitForFunction(() => typeof notify == 'function', null, {timeout: 20000}).then(() => true, () => false);
  ok('3. offline: the desktop still opens', t);
  await ctx.setOffline(false);
  // an installed Rudolf OS: none
  const c2 = await b.newContext(); const p2 = await c2.newPage(); RDMODE = true;
  await p2.goto('http://127.0.0.1:8183/'); await p2.waitForFunction(() => typeof notify == 'function', null, {timeout: 30000}); await p2.waitForTimeout(1500);
  const n = await p2.evaluate(async () => (await navigator.serviceWorker.getRegistrations()).length);
  ok('4. with rudolfd (installed Rudolf OS): no service worker', n == 0, n);
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); const k = R.filter(Boolean).length; console.log(`${k}/${R.length} passed`); process.exit(k == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
