// 1.5.9: weeks start on Sunday in Rudolf OS's calendars: the Calendar app, the clock's calendar (click the clock) and
// the calendar widget: the first column is Sunday, and each month's 1st is under its own weekday. A stand-in rudolfd.
//   node calsun59_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const srv = http.createServer((q, s) => { const [u0] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/a/')) { const f = path.join(WWW, u); if (fs.existsSync(f)) { s.writeHead(200); return s.end(fs.readFileSync(f)) } }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      s.writeHead(200, {'content-type': 'application/json'}); return s.end('{"ok":true}') }
    s.writeHead(404); s.end() }) }).listen(8301, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => { if (/calendar|cal|wgDraw|tray/i.test(e.stack || '')) errs.push(e.message) });
  // 2026-08-14: August 2026 begins on a Saturday (6 empty cells before it on a Sunday-first week)
  await p.clock.install({time: new Date('2026-08-14T12:00:00')});
  await p.goto('http://127.0.0.1:8301/'); await p.waitForFunction(() => typeof calendarApp == 'function' && typeof wgDraw == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x'}];USER=ACCTS[0];return 1})()`);
  // where the 1st falls: its column in the first row that has it
  const grid = sel => q(`(()=>{const t=document.querySelector('${sel}');if(!t)return null;const rows=[...t.querySelectorAll('tr')];const hd=[...rows[0].querySelectorAll('th')].map(x=>x.textContent.trim());
    for(const r of rows.slice(1)){const c=[...r.children];const i=c.findIndex(x=>/^1(\\D|$)/.test(x.textContent.trim()));if(i>=0)return{hd,col:i}}return{hd}})()`);
  await q("calendarApp()"); await wait(500);
  const a = await grid('.calbig');
  ok('1. Calendar app: Sunday to Saturday', a && JSON.stringify(a.hd) == '["Sun","Mon","Tue","Wed","Thu","Fri","Sat"]', a);
  ok('...August 2026\'s 1st (a Saturday) is in the last column', a && a.col == 6, a);
  await q("document.querySelector('#tray').click()"); await wait(300);
  const c = await grid('#calp .calt');
  ok('2. the clock\'s calendar: Su to Sa', c && JSON.stringify(c.hd) == '["Su","Mo","Tu","We","Th","Fr","Sa"]', c);
  ok('...the 1st under Saturday', c && c.col == 6, c);
  await q("document.querySelector('#calp .cn').click()"); await wait(200);
  const c2 = await grid('#calp .calt');
  ok('...September 2026 (begins on a Tuesday): the 1st under Tuesday', c2 && c2.col == 2, c2);
  await q("document.querySelector('#tray').click()");
  await q("WG=[{t:'cal',x:40,y:40,txt:''}];wgDraw()"); await wait(300);
  const w = await grid('.wgt-cal .calt');
  ok('3. the calendar widget: S M T W T F S', w && w.hd.join('') == 'SMTWTFS', w);
  ok('...the 1st under Saturday', w && w.col == 6, w);
  ok('4. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
