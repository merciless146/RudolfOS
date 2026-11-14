// 1.5.9: Display Properties › Dynamic Backgrounds: "Note: Dynamic effects are disabled on power-saver mode." under the
// heading, shown only while power saver is on (as aurora58.js's dynOvCheck stops the effects). node dynps59_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const srv = http.createServer((q, s) => { const [u0] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      s.writeHead(200, {'content-type': 'application/json'}); return s.end('{"ok":true}') }
    s.writeHead(404); s.end() }) }).listen(8302, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => { if (/dynOv|dispPanelS|aurora/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8302/'); await p.waitForFunction(() => typeof dynOvCheck == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  // (the stand-in's power/status says nothing: the power state is held for the test)
  await q("window.__pw={now:{src:'ac',saver:false,mode:'balanced',profile:'balanced'}};setInterval(()=>{PWR=window.__pw},50);settings('disp')"); await wait(1200);
  const st = () => q(`(()=>{const n=SW.body.querySelector('.dynps');if(!n)return null;const l=n.closest('label');return {txt:n.textContent,shown:!!n.getClientRects().length,under:!!l&&l.firstChild.textContent.trim()=='Dynamic Backgrounds'&&l.nextElementSibling&&l.nextElementSibling.id=='dynw'}})()`);
  const s0 = await st();
  ok('1. the note is under the Dynamic Backgrounds heading, beside its options', s0 && s0.under && s0.txt == 'Note: Dynamic effects are disabled on power-saver mode.', s0);
  ok('2. ...hidden while power saver is off', s0 && !s0.shown, s0);
  for (const [n, set] of [['power saver (the power mode)', "window.__pw={now:{src:'bat',saver:false,mode:'saver',profile:'quiet',epp:'power'}}"], ['battery saver (at a low battery)', "window.__pw={now:{src:'bat',saver:true,mode:'balanced'}}"], ['battery saver ("fewer animations")', "window.__pw={now:{}};window.__bsaver=true"]]) {
    await q(`(()=>{window.__bsaver=false;${set};return 1})()`); await wait(1300);
    const s = await st(); ok('3. shown with ' + n, s && s.shown, s) }
  await q("window.__bsaver=false;window.__pw={now:{src:'ac',saver:false,mode:'balanced',profile:'balanced',epp:'balance_performance'}}"); await wait(1300);
  ok('4. hidden again when power saver goes off', !(await st()).shown);
  await q("window.__pw={now:{src:'bat',mode:'saver',profile:'quiet',epp:'power'}}"); await wait(1300); await q("SW.show('disp',1)"); await wait(600);
  ok('5. shown at once when the page is opened in power saver', (await st()).shown);
  ok('6. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
