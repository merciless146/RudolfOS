// 1.5.4 (tb54.js, devices26.js): a Thunderbolt dock waiting to be allowed asks, wherever you are; "Allow" needs an
// administrator and approves it; "Don't allow" does not; Settings › Devices lists Thunderbolt and USB4 devices with
// Forget, including kept devices that are not plugged in; a computer without Thunderbolt has no such section.
// The desktop page with a stand-in rudolfd.   node tb_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
let ask = {id: '0-1', uid: 'd501', name: 'CalDigit TS4', t: 'd501'}, present = true; const posts = [];
const TB = () => ({ok: true, present, dma: false, security: 'user', devices: present ? [{id: '0-1', uid: 'd501', name: 'CalDigit TS4', approved: false, kept: false, canDisconnect: true}, {id: '0-3', uid: 'aa01', name: 'OWC Envoy Express', approved: true, kept: true, canDisconnect: true}] : [],
  kept: present ? [{uid: 'aa01', name: 'OWC Envoy Express'}, {uid: 'bb02', name: 'Dell WD22TB4'}] : []});
const api = a => { if (a == 'tb') return TB(); if (a == 'tb/ask') return {ok: true, ask};
  if (a == 'devices') return {ok: true, drives: [], usb: [], inputs: []}; if (a == 'devices/unsupported') return {ok: true, devices: []};
  if (a == 'events/wait') return {ok: true, events: ''}; return {ok: true} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":""}') }, 20000);
    if (u.startsWith('/api/')) { const a = u.slice(5); if (q.method == 'POST' && a.startsWith('tb/')) { posts.push(a + '?' + qs); if (a == 'tb/allow' || a == 'tb/deny') ask = null }
      s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(Object.assign({message: 'approved'}, api(a)))) }
    s.writeHead(404); s.end() }) }).listen(8097, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8097/'); await p.waitForFunction(() => typeof tbAskBox == 'function', null, {timeout: 30000});
  await p.evaluate(() => { window.__elevAsked = []; elevate = r => { __elevAsked.push(r); return Promise.resolve(window.__elevAnswer !== false) } });
  await p.waitForSelector('.tb54q', {timeout: 15000}).catch(() => {});
  let t = await p.evaluate(() => { const q = document.querySelector('.tb54q'); return q ? q.textContent : '' });
  ok('1. a dock waiting to be allowed asks by itself', /Allow CalDigit TS4\?/.test(t) && /trust/.test(t), t);
  await p.evaluate(() => { window.__elevAnswer = false; [...document.querySelectorAll('.tb54q button')].find(x => x.textContent == 'Allow').click() }); await wait(500);
  ok('2. Allow without an administrator: nothing is approved', !posts.some(x => x.startsWith('tb/allow')) && await p.evaluate(() => !!document.querySelector('.tb54q') && __elevAsked.length == 1), posts);
  await p.evaluate(() => { window.__elevAnswer = true; [...document.querySelectorAll('.tb54q button')].find(x => x.textContent == 'Allow').click() }); await wait(800);
  ok('3. Allow with an administrator: approved, and the question goes', posts.includes('tb/allow?id=0-1') && await p.evaluate(() => !document.querySelector('.tb54q')), posts);
  ask = {id: '0-5', uid: 'cc03', name: 'Some Dock', t: 'cc03'}; await p.waitForSelector('.tb54q', {timeout: 10000}).catch(() => {});
  await p.evaluate(() => [...document.querySelectorAll('.tb54q button')].find(x => /Don/.test(x.textContent)).click()); await wait(800);
  ok("4. Don't allow: refused, no password asked", posts.includes('tb/deny?id=0-5') && await p.evaluate(() => __elevAsked.length == 2 && !document.querySelector('.tb54q')), posts);
  const panel = () => p.evaluate(async () => { let d = document.getElementById('dvt'); if (d) d.remove(); d = document.createElement('div'); d.id = 'dvt'; document.body.appendChild(d); devPanel(d);
    await new Promise(r => setTimeout(r, 800)); const h = [...d.querySelectorAll('h4')].map(x => x.textContent); const tb = d.querySelector('.dvtb');
    return {h, rows: tb ? [...tb.querySelectorAll('tr')].map(r => r.textContent) : [], forget: d.querySelectorAll('[data-tbforget]').length, allow: d.querySelectorAll('[data-tballow]').length, msg: d.textContent} });
  let g = await panel();
  ok('5. Settings › Devices: a Thunderbolt and USB4 section', g.h.includes('Thunderbolt and USB4'), g);
  ok('6. ...the dock waiting, the drive connected, and a kept dock not plugged in', g.rows.length == 3 && /Waiting to be allowed/.test(g.rows[0]) && /Connected/.test(g.rows[1]) && /Not plugged in/.test(g.rows[2]), g.rows);
  ok('7. ...with Allow for the waiting one and Forget for the kept ones', g.allow == 1 && g.forget == 2, g);
  ok('8. ...and says why devices are asked about (no DMA protection)', /does not report DMA protection/.test(g.msg), g.msg.slice(0, 300));
  await p.evaluate(() => document.querySelector('#dvt [data-tbforget]').click()); await wait(600);
  ok('9. Forget: asks for an administrator, then forgets it', posts.includes('tb/forget?uid=aa01'), posts);
  present = false; g = await panel(); ok('10. no Thunderbolt here: no section', !g.h.includes('Thunderbolt and USB4'), g.h);
  ok('11. no page errors', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1)
})().catch(e => { console.log('FAIL ' + e.stack); process.exit(1) });
