// 1.5.3 (core25.js shutdownDlg, rudolfd power): Restart while a job runs that must not be cut off (here: Make a USB
// stick) is refused with the reason, and only "Restart anyway" sends force=1. Twice a stick was left without its
// system by a restart in the middle. The desktop page with a stand-in rudolfd, in Chromium.
//   node powerbusy_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
let busy = true; const calls = [];
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
    if (u == '/api/power') { const sp = new URLSearchParams(qs); calls.push({a: sp.get('action'), force: sp.get('force')});
      if (busy && sp.get('force') != '1') { s.writeHead(409, {'content-type': 'application/json'}); return s.end(JSON.stringify({ok: false, error: 'busy: Rudolf OS is still making a Rudolf OS USB stick. Wait until it says it is finished: stopping it now would leave it half done.'})) }
      s.writeHead(200, {'content-type': 'application/json'}); return s.end('{"ok":true,"message":"' + sp.get('action') + '"}') }
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end('{"ok":true}') }
    s.writeHead(404); s.end() }) }).listen(8096, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8096/'); await p.waitForFunction(() => typeof shutdownDlg == 'function', null, {timeout: 30000});
  await p.evaluate(() => { window.sfx = () => {}; shutdownDlg() });
  await p.evaluate(() => document.querySelector(".rmodal button[data-a='reboot']").click()); await wait(4500);
  let st = await p.evaluate(() => ({msg: document.querySelector('.rmodal .rmmsg').textContent, force: (document.querySelector('.rmodal .rmforce') || {}).textContent || '', open: !!document.querySelector('.rmodal'), dis: [...document.querySelectorAll('.rmodal button')].some(b => b.disabled)}));
  ok('1. Restart while a stick is being made: refused, the reason shown', st.open && /still making a Rudolf OS USB stick/.test(st.msg) && !/^busy/.test(st.msg), st);
  ok('2. ...with a "Restart anyway" button, and the buttons usable again', st.force == 'Restart anyway' && !st.dis, st);
  ok('3. ...the first request did not ask to force it', calls.length == 1 && calls[0].a == 'reboot' && !calls[0].force, calls);
  await p.evaluate(() => document.querySelector('.rmodal .rmforce').click()); await wait(4500);
  ok('4. "Restart anyway" sends force=1', calls.length == 2 && calls[1].a == 'reboot' && calls[1].force == '1', calls);
  busy = false; calls.length = 0; await p.evaluate(() => { document.querySelectorAll('.rmodal').forEach(o => o.remove()); shutdownDlg() });
  await p.evaluate(() => document.querySelector(".rmodal button[data-a='poweroff']").click()); await wait(4500);
  ok('5. nothing running: Shut down goes ahead at once, not forced', calls.length == 1 && calls[0].a == 'poweroff' && !calls[0].force, calls);
  ok('6. no page errors', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1)
})().catch(e => { console.log('FAIL ' + e.stack); process.exit(1) });
