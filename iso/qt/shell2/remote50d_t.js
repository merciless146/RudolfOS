// 1.5.0 (remote50.js): Settings › Remote access, the accounts rudolf-remote checks, the approval prompt and the marker,
// against a stand-in for rudolfd's remote API. node remote50d_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 300) : '')) };
const RM = {on: false, running: false, port: 8443, live: false, code: '', fingerprint: 'AB:CD', addresses: ['192.168.1.20'], session: null, pending: null, devices: 0};
const POSTS = [];
const api = (q, s, body) => {
  const u = new URL(q.url, 'http://x'), p = u.pathname.slice(5); let j = q.method == 'POST' ? {ok: true} : {ok: false, error: 'not in the stand-in'};
  if (q.method == 'POST') POSTS.push([p + u.search, body]);
  if (p == 'remote' && q.method == 'GET') j = {ok: true, ...RM};
  else if (p == 'remote' && q.method == 'POST') { RM.on = u.searchParams.get('on') == '1'; RM.running = RM.on; RM.code = RM.on ? '482913' : '' }
  else if (p == 'remote/answer') RM.pending = null;
  else if (p == 'remote/end') RM.session = null;
  else if (p == 'events/wait') return setTimeout(() => { s.writeHead(200, {'content-type': 'application/json'}); s.end('{"ok":true,"events":[]}') }, 20000);
  s.writeHead(200, {'content-type': 'application/json'}); s.end(JSON.stringify(j));
};
const srv = http.createServer((q, s) => { let b = ''; q.on('data', d => b += d); q.on('end', () => {
  if (q.url.startsWith('/api/')) return api(q, s, b);
  s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', "<head><script>window.RUDOLFD={token:'t',base:'',boot:'uefi',media:'disk',live:0,recovery:0}</script>")) }) }).listen(8099, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8099/'); await p.waitForFunction(() => typeof remotePanel == 'function' && typeof fs != 'undefined' && fs['C:'], null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});window.elevate=async()=>true;
    ACCTS.length=0;ACCTS.push({n:'boss',h:await pwMake('b1'),r:'admin'},{n:'kid',h:await pwMake('k1'),r:'user'});USER=ACCTS[0];saveUser();return 1})()`);
  await q("settings('remote')"); await wait(800);
  ok('1. Settings has Remote access, off', await q("(()=>{const c=SW.body.querySelector('.sc');return c.querySelector('h3').textContent=='Remote access'&&!c.querySelector('.rmo').checked&&c.querySelector('.rms').textContent=='Off'})()"));
  await q("(()=>{const c=SW.body.querySelector('.rmo');c.checked=true;c.dispatchEvent(new Event('change'))})()"); await wait(1200);
  const acc = POSTS.find(x => x[0] == 'remote/accounts');
  ok('2. turning it on sends the accounts (name, PBKDF2 hash, role, unattended) first, then on=1', acc && /^boss\tp2\$[^\t]+\tadmin\t0\nkid\tp2\$[^\t]+\tuser\t0\n$/.test(acc[1]) && POSTS.findIndex(x => x[0] == 'remote/accounts') < POSTS.findIndex(x => x[0] == 'remote?on=1'), POSTS.map(x => x[0]));
  const t = await q("SW.body.querySelector('.rmon').textContent");
  ok('3. it shows the address, the pairing code and the fingerprint', /https:\/\/192\.168\.1\.20:8443/.test(t) && /482913/.test(t) && /AB:CD/.test(t), t.slice(0, 300));
  POSTS.length = 0;
  await q("(()=>{const x=[...SW.body.querySelectorAll('.rmu input')][1];x.checked=true;x.dispatchEvent(new Event('change'))})()"); await wait(1500);
  ok('4. unattended access for an account: saved and sent to rudolf-remote', await q("ACCTS[1].ru===true") && POSTS.some(x => x[0] == 'remote/accounts' && /kid\t[^\t]+\tuser\t1/.test(x[1])), POSTS.map(x => x[0]));
  RM.pending = {id: '0123456789abcdef', user: 'boss', from: '192.168.1.33', time: Math.floor(Date.now() / 1000)}; await q("rmPoll()"); await wait(500);
  const ask = await q("(()=>{const o=document.querySelector('.rmodal[style*=\"99999\"]');return o?o.textContent:''})()");
  ok('5. a connection waiting: the person at the computer is asked, with who and from where', /Allow a remote connection/.test(ask) && /boss/.test(ask) && /192\.168\.1\.33/.test(ask), ask);
  await q("document.querySelector('.rmodal[style*=\"99999\"] .y').click()"); await wait(500);
  ok('6. Allow answers it', POSTS.some(x => x[0] == 'remote/answer?id=0123456789abcdef&allow=1'));
  RM.session = {user: 'boss', from: '192.168.1.33', since: Math.floor(Date.now() / 1000) - 180, unattended: false}; await q("rmPoll()"); await wait(400);
  ok('7. while someone is connected, the marker shows who, from where and how long', /boss is connected from 192\.168\.1\.33 \(3 minutes\)/.test(await q("(document.querySelector('.rm50bar')||{}).textContent||''")));
  await q("document.querySelector('.rm50bar button').click()"); await wait(400); await q("rmPoll()"); await wait(400);
  ok('8. End on the marker ends it, and the marker goes away', POSTS.some(x => x[0] == 'remote/end') && !(await q("!!document.querySelector('.rm50bar')")));
  RM.pending = {id: 'fedcba9876543210', user: 'kid', from: '10.0.0.9', time: Math.floor(Date.now() / 1000)}; await q("rmPoll()"); await wait(400);
  await q("document.querySelector('.rmodal[style*=\"99999\"] .n').click()"); await wait(400);
  ok('9. Refuse answers allow=0', POSTS.some(x => x[0] == 'remote/answer?id=fedcba9876543210&allow=0'));
  POSTS.length = 0; await q("(async()=>{ACCTS[0].h=await pwMake('new');saveUser();return 1})()"); await wait(1200);
  ok('10. a changed password reaches rudolf-remote', POSTS.some(x => x[0] == 'remote/accounts'));
  ok('11. no page errors', !errs.length, errs);
  console.log(`${R.filter(x => x).length}/${R.length} passed`); await b.close(); srv.close(); process.exit(R.every(x => x) ? 0 : 1);
})();
