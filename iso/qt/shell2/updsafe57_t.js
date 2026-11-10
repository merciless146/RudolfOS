// 1.5.7: Settings › Update and recovery says why updates are paused (Recovery mode, or Rudolf OS running from a USB
// stick on a computer where it is installed), offers no Download there, and the daily check does not download.
//   node updsafe57_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 400) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8174, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.addInitScript(() => { window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0};
    window.ST = {ok: true, persistent: true, booted: 'disc', trying: false, current: {serial: 2026110100, version: '1.5.6'}, state: {good: 'b', prev: 'a', next: '', tries: 0, failed: ''},
      canUpdate: false, recovery: false, slots: {a: {complete: true, serial: 1, version: '1.5.7'}, b: {complete: true, serial: 2, version: '1.5.7'}}, url: 'https://x/beta', auto: true, checked: 0,
      available: {serial: 2026111023, version: '1.5.7', notes: 'n', released: '', urgent: false, size: 1}};
    window.CALLS = [];
    const f0 = window.fetch; window.fetch = (u, o) => { u = String(u); if (u.includes('/api/')) { const a = u.split('/api/')[1]; CALLS.push(((o && o.method) || 'GET') + ' ' + a);
      const body = a.startsWith('sys/status') ? ST : {ok: true, items: [], apps: [], windows: [], running: false, status: 0, log: ''};
      return Promise.resolve(new Response(JSON.stringify(body), {headers: {'content-type': 'application/json'}})) } return f0(u, o) } });
  await p.goto('http://127.0.0.1:8174/'); await p.waitForFunction(() => typeof sysuTick == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  await p.evaluate(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS.length=0;ACCTS.push({n:'ann',h:await pwMake('a1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'a1');return 1})()`);
  await wait(800);
  const card = async () => { await p.evaluate("(()=>{document.querySelectorAll('.win').forEach(w=>{});settings('upd');return 1})()"); await wait(1500); return p.evaluate("(()=>{const c=[...document.querySelectorAll('.sysucard')].at(-1);return c?{t:c.textContent,dl:!!document.querySelector('.sysu .udl')}:null})()") };
  let c = await card();
  ok('1. running from a USB stick: Settings says to start from the computer\'s own drive, with no Download', c && /running from a USB drive or disc/.test(c.t) && !c.dl, c);
  await p.evaluate("CALLS.length=0;sysuTick()"); await wait(1500);
  ok('2. ...and the daily check downloads nothing', !(await p.evaluate("CALLS.some(x=>/POST sys\\/(check|download)/.test(x))")), await p.evaluate('CALLS'));
  await p.evaluate("ST.recovery=true;ST.booted='b'"); c = await card();
  ok('3. Recovery mode: updates are paused until a normal start', c && /Recovery mode: updates are paused/.test(c.t) && !c.dl, c);
  await p.evaluate("ST.recovery=false;ST.canUpdate=true"); c = await card();
  ok('4. a normal start offers the update as before', c && /Update available/.test(c.t) && c.dl, c);
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
