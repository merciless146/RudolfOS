// 1.5.0 (vault50.js): data encryption from the desktop's side, against a stand-in for rudolfd's vault/*: setup with a
// recovery key, the data key wrapped per account, unlock at sign-in after a restart, new accounts get a copy, the
// recovery key unlocks and gives the account its copy. node vault50_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 300) : '')) };
const V = {state: 'off', data: null, key: null, unlocks: [], moved: {debian: false, flatpak: false, downloads: false}};
const vs = () => ({ok: true, state: V.state, feature: true, moved: V.moved, data: V.data});
const api = (q, s, body) => {
  const u = new URL(q.url, 'http://x'), p = u.pathname.slice(5); let j = q.method == 'POST' ? {ok: true} : {ok: false, error: 'not in the stand-in'}, code = 200;
  if (p == 'vault') j = vs();
  else if (p == 'vault/setup') { V.data = JSON.parse(body); V.state = 'ready'; j = vs() }
  else if (p == 'vault/store') { V.data = JSON.parse(body); j = {ok: true} }
  else if (p == 'vault/unlock') { V.unlocks.push(body); if (V.key && V.key != body) { code = 403; j = {ok: false, error: 'that key does not open the encrypted folder'} } else { const first = !V.key; V.key = body; V.state = first ? 'moving' : 'on'; if (first) V.moved = {debian: true, flatpak: true, downloads: true}; j = vs() } }
  else if (p == 'job') { if (V.state == 'moving') V.state = 'on'; j = {ok: true, name: 'vault', running: false, status: 0, log: 'Linux (Debian): done\nYour data is encrypted.'} }
  else if (p == 'health' && q.method == 'GET') j = {ok: true, now: 0, uptime: 0, events: []};
  else if (p == 'events/wait') return setTimeout(() => { s.writeHead(200, {'content-type': 'application/json'}); s.end('{"ok":true,"events":[]}') }, 20000);
  s.writeHead(code, {'content-type': 'application/json'}); s.end(JSON.stringify(j));
};
const srv = http.createServer((q, s) => { let b = ''; q.on('data', d => b += d); q.on('end', () => {
  if (q.url.startsWith('/api/')) return api(q, s, b);
  s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', "<head><script>window.RUDOLFD={token:'t',base:'',boot:'uefi',media:'disk',live:0,recovery:0}</script>")) }) }).listen(8099, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => errs.push(e.message + ' @ ' + (e.stack || '').split('\n').slice(1, 3).join(' ')));
  const q = s => p.evaluate(s);
  const boot = async () => { await p.goto('http://127.0.0.1:8099/'); await p.waitForFunction(() => typeof vaultSignin == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
    await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});window.elevate=async()=>true;
      window.IN=async(n,pw)=>{const a=ACCTS.find(x=>x.n==n);if(!(await pwCheck(a,pw)))return false;if(USER&&USER.r)userLogout();USER=a;await userLogin(a,pw);return true};
      window.OUT=async()=>{fsFlush();await homeChain;userLogout();await homeChain;USER=null};return 1})()`) };
  await boot();
  await q("(async()=>{ACCTS.length=0;ACCTS.push({n:'boss',h:await pwMake('b1'),r:'admin'});saveUser();await IN('boss','b1');return 1})()");
  await q("settings('sec')"); await wait(800);
  ok('1. Security Settings: Data encryption, off, with Encrypt my data', await q("(()=>{const v=SW.body.querySelector('.v50');return !!v&&/^Off\\./.test(v.querySelector('.v50t').textContent)&&[...v.querySelectorAll('button')].some(b=>b.textContent=='Encrypt my data…')})()"));
  await q("[...SW.body.querySelectorAll('.v50 button')].find(b=>b.textContent=='Encrypt my data…').click()"); await wait(500);
  const rk = await q("document.querySelector('.rmodal .rk').textContent");
  ok('2. the recovery key is shown (30 characters in groups of five)', /^([A-Z0-9]{5}-){5}[A-Z0-9]{5}$/.test(rk), rk);
  ok('3. Encrypt stays off until the key is said to be saved', await q("document.querySelector('.rmodal .y').disabled"));
  await p.type('.rmodal .pw', 'b1'); await q("(()=>{const c=document.querySelector('.rmodal .ok');c.checked=true;c.dispatchEvent(new Event('change'));document.querySelector('.rmodal .y').click()})()");
  await wait(4000);
  ok('4. setup stores the key wrapped for the account and for the recovery key, and unlocks with it', V.data && V.data.rec && Object.keys(V.data.accts).length == 1 && V.unlocks.length == 1 && /^[0-9a-f]{128}$/.test(V.unlocks[0]), {d: V.data, u: V.unlocks});
  const dk = V.unlocks[0];
  ok('5. the account\'s copy opens with its password and gives the same key', await q(`(async()=>vHex(await vUnwrap('b1',Object.values(VAULT50.data?VAULT50.data.accts:${JSON.stringify(V.data.accts)})[0])))()`) == dk);
  ok('6. ...and the recovery key opens its copy', await q(`(async()=>vHex(await vUnwrap(vRecNorm('${rk.toLowerCase()}').replace(/-/g,''),${JSON.stringify(V.data.rec)})))()`) == dk);
  ok('7. the move\'s progress is shown, then it says encrypted', await q("[...document.querySelectorAll('.rmodal .vl')].some(x=>/Your data is encrypted/.test(x.textContent))"));
  await q("(async()=>{ACCTS.push(await acctNew('kid','k1','user'));saveUser();return 1})()"); await wait(500);
  const kid = await q("ACCTS.find(a=>a.n=='kid').id");
  ok('8. a new account gets its copy right away', !!(V.data.accts[kid]), Object.keys(V.data.accts));
  await q("(async()=>{ACCTS.push(await acctNew('third','t1','user'));saveUser();return 1})()"); await wait(300);
  delete V.data.accts[await q("ACCTS.find(a=>a.n=='third').id")];   // as if made before encryption
  await q("OUT()");
  // a restart: locked
  V.state = 'locked'; V.unlocks = []; await boot();
  await q("IN('third','t1')"); await wait(800);
  ok('9. after a restart, an account without a copy does not unlock it', V.unlocks.length == 0 && V.state == 'locked');
  await q("OUT()"); await q("IN('kid','k1')"); await wait(1500);
  ok('10. an account with a copy unlocks it at sign-in, with the same key', V.unlocks.length == 1 && V.unlocks[0] == dk && V.state == 'on', V.unlocks);
  await q("OUT()"); await q("IN('third','t1')"); await wait(1500);
  ok('11. an account that signs in while the key is in memory gets its copy', !!V.data.accts[await q("ACCTS.find(a=>a.n=='third').id")]);
  // a restart, and only the recovery key
  await q("OUT()"); V.state = 'locked'; V.unlocks = []; const tid = await q("ACCTS.find(a=>a.n=='third').id"); delete V.data.accts[tid]; await boot();
  await q("IN('third','t1')"); await q("settings('sec')"); await wait(1000);
  ok('12. locked: Security Settings says so and offers the recovery key', await q("(()=>{const v=SW.body.querySelector('.v50');return /On, locked/.test(v.textContent)&&[...v.querySelectorAll('button')].some(b=>/recovery key/.test(b.textContent))})()"));
  await q("[...SW.body.querySelectorAll('.v50 button')].find(b=>/recovery key/.test(b.textContent)).click()"); await wait(300);
  await p.fill('.rmodal .rk', 'WRONG-WRONG-WRONG-WRONG-WRONG-WRONG'); await p.type('.rmodal .pw', 't1'); await q("document.querySelector('.rmodal .y').click()"); await wait(1500);
  ok('13. a wrong recovery key is refused', await q("document.querySelector('.rmodal .rmmsg').textContent") == 'That is not the recovery key.' && V.unlocks.length == 0);
  await p.fill('.rmodal .rk', rk.toLowerCase().replace(/-/g, ' ')); await q("document.querySelector('.rmodal .y').click()"); await wait(2500);
  ok('14. the recovery key unlocks it (any spacing or case) and the account gets its copy', V.unlocks[0] == dk && !!V.data.accts[tid], V.unlocks);
  await q("settings('sec')"); await wait(800);
  const t = await q("SW.body.querySelector('.v50').textContent");
  ok('15. Security Settings shows what is encrypted and whose password unlocks it', /Linux \(Debian\) and Windows programs: encrypted/.test(t) && /unlocks it: .*kid/.test(t), t);
  ok('16. no page errors', !errs.filter(e => !/volUpd/.test(e)).length, errs);
  console.log(`${R.filter(x => x).length}/${R.length} passed`); await b.close(); srv.close(); process.exit(R.every(x => x) ? 0 : 1);
})();
