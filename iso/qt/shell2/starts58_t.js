// 1.5.8: System health › Last starts: each start, newest first: whether the desktop came up (and how soon, with which
// engine) or never did, Recovery mode, the system slot and release, the browser started again, what was given up, and
// what the start-up said.   node starts58_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 500) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8175, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.addInitScript(() => { window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0};
    window.STARTS = {ok: true, starts: [
      {n: 0, time: '2026-11-10T23:30:00Z', slot: 'b', release: '2026111100', entry: 'rudolf-start.conf', recovery: false, desktopUp: 41, engine: 'chromium', exits: 1, gaveUp: '', startup: ''},
      {n: 1, time: '2026-11-10T23:20:00Z', slot: 'b', release: '2026111100', entry: 'rudolf-start.conf', recovery: false, desktopUp: null, engine: '', exits: 3, gaveUp: 'the WebKit desktop did not start (it closed 3 times in a row as it started) - using Chromium', startup: '23:19:58 Rudolf OS: <b>x</b>'},
      {n: 2, time: '2026-11-10T23:10:00Z', slot: 'a', release: '2026111023', entry: 'rudolf-recovery.conf', recovery: true, desktopUp: 30, engine: 'chromium', exits: 1, gaveUp: '', startup: ''}]};
    const f0 = window.fetch; window.fetch = (u, o) => { u = String(u); if (u.includes('/api/')) { const a = u.split('/api/')[1];
      const body = a == 'system/starts' ? STARTS : {ok: true, items: [], apps: [], windows: [], events: '', lines: []};
      return Promise.resolve(new Response(JSON.stringify(body), {headers: {'content-type': 'application/json'}})) } return f0(u, o) } });
  await p.goto('http://127.0.0.1:8175/'); await p.waitForFunction(() => typeof startsShow == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  await p.evaluate(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS.length=0;ACCTS.push({n:'ann',h:await pwMake('a1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'a1');return 1})()`);
  await wait(800);
  await p.evaluate("(()=>{const d=document.createElement('div');document.body.appendChild(d);healthPanel(d);window.__hp=d;return 1})()"); await wait(500);
  ok('1. System health has "Last starts"', await p.evaluate("!!__hp.querySelector('.hss')&&!__hp.querySelector('.hss').hidden"));
  await p.evaluate("__hp.querySelector('.hss').click()"); await wait(1200);
  const t = await p.evaluate("(()=>{const w=wins.find(w=>/Last starts/.test(w.el.querySelector('.t').textContent));return w?w.body.innerText:''})()");
  ok('2. a start that came up: when, how soon, with which engine', /The desktop came up after 41 s \(Standard \(Chromium\)\)/.test(t), t);
  ok('3. a start whose desktop never came up: said so, with how often the browser started and what was given up', /The desktop never came up/.test(t) && /the browser started 3 times/.test(t) && /closed 3 times in a row/.test(t), t);
  ok('4. Recovery mode and the slot and release are shown', /Recovery mode · system slot A · release 2026111023/.test(t), t);
  ok('5. the start-up\'s lines are shown as text, not as markup', await p.evaluate("(()=>{const w=wins.find(w=>/Last starts/.test(w.el.querySelector('.t').textContent));return !w.body.querySelector('pre b')&&/<b>x<\\/b>/.test(w.body.querySelector('pre').textContent)})()"));
  const hp = await p.evaluate("(()=>{const h=HELP.find(h=>h[0]=='rescue');return h?h[2]():''})()");
  ok('6. Help: starting from a USB stick (full image in DD mode, the boot-menu keys, Fast Boot, Recovery mode, Ctrl+Alt+Backspace)', /DD mode/.test(hp) && /not accessible/.test(hp) && /Fast Boot/.test(hp) && /Recovery mode/.test(hp) && /Ctrl\+Alt\+Backspace/.test(hp), hp.slice(0, 200));
  ok('7. Help: the keyboard shortcuts list Ctrl+Alt+Backspace', await p.evaluate("HELP.find(h=>h[0]=='keys')[2]().includes('Ctrl+Alt+Backspace')"));
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
