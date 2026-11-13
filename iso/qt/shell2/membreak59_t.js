// 1.5.9: Task Manager › Performance shows where the memory goes (rudolfd memory/breakdown): programs, file cache,
// kernel, graphics, the SSD's buffer, compressed memory, and what cannot be itemised while the system is protected.
// node membreak59_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 400) : '')) };
const MiB = 1048576;
// the Vivobook at idle on 2026-11-12 (the test kernel's reading, in round numbers)
const BRK = {ok: true, total: 3415 * MiB, free: 1532 * MiB, available: 2341 * MiB, programs: 411 * MiB, cache: 1061 * MiB, shared: 45 * MiB, kernel: 168 * MiB, graphics: 17 * MiB, compressed: 0, ssd: 64 * MiB, other: 162 * MiB};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
  if (u == '/api/memory/breakdown') { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(BRK)) }
  if (u == '/api/stats') { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify({ok: true, cpu: 3, memTotal: BRK.total, memAvailable: BRK.available, battery: []})) }
  if (u == '/api/processes') { s.writeHead(200, {'content-type': 'application/json'}); return s.end('{"ok":true,"items":[]}') }
  if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end('{"ok":true}') }
  s.writeHead(404); s.end() }).listen(8270, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1366, height: 800}});
  const errs = []; p.on('pageerror', e => errs.push(e.message + ' @ ' + String(e.stack || '').split('\n').slice(1, 3).join(' ')));
  await p.goto('http://127.0.0.1:8270/'); await p.waitForFunction(() => typeof launch == 'function' && typeof RD != 'undefined', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});USER=USER||ACCTS[0];return 1})()`);
  await q("(()=>{const W=launch('taskmgr');setTimeout(()=>{const t=document.querySelector('.win.act .tmtabs button[data-t=perf]');t&&t.click()},300);return 1})()"); await wait(1500);
  const rows = await q("[...document.querySelectorAll('.win.act .tmmrow')].map(r=>[r.querySelector('span').textContent,r.querySelector('em').textContent,r.title])");
  ok('1. Performance has "Where the memory goes", one row per holder', await q("[...document.querySelectorAll('.win.act h4')].some(h=>h.textContent=='Where the memory goes')") && rows.length >= 6, rows);
  const names = rows.map(r => r[0]);
  ok('2. the rows: programs, file cache, kernel, graphics, the SSD\'s buffer, other (compressed memory left out when empty)', ['Programs and web pages', 'Files in memory (cache)', 'Kernel', 'Graphics', 'SSD buffer', 'Other drivers and kernel'].every(n => names.includes(n)) && !names.includes('Compressed memory'), names);
  ok('3. the SSD\'s buffer is named and sized (64 MB) and says what it is', rows.some(r => r[0] == 'SSD buffer' && /64/.test(r[1]) && /borrows/.test(r[2])));
  ok('4. what cannot be itemised says why (protected system), and what it mostly is', rows.some(r => r[0] == 'Other drivers and kernel' && /Secure Boot/.test(r[2]) && /graphics driver/.test(r[2])));
  ok('5. the note gives used and available', /used; .* is available/.test(await q("document.querySelector('.win.act .tmmem .msg').textContent")));
  ok('6. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  const k = R.filter(Boolean).length; console.log(`${k}/${R.length} passed`); process.exit(k == R.length ? 0 : 1);
})().catch(e => { console.log('ERROR', e.stack); process.exit(1) });
