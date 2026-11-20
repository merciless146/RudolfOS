// 1.6.0 (plan-1.6.1 Part 17, moved to 1.6.0): authenticator codes with a stand-in rudolfd (desktop/second): set up
// in User Settings (the key shown, a wrong code refused, the secret kept encrypted), the sign-in screen asking for a code
// after the password, a used code refused, the lock screen asking again, and an administrator requiring it.
//   node otp60_t.js [CHROME]   (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 500) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8149, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.addInitScript(() => {
    window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0};
    window.T = {last: {}, fails: {}, calls: []};
    const f0 = window.fetch;
    window.fetch = (u, o) => { u = String(u); const q = new URL(u, location.href), J = (x, st) => Promise.resolve(new Response(JSON.stringify(Object.assign({ok: !st}, x)), {status: st || 200, headers: {'content-type': 'application/json'}}));
      if (q.pathname == '/api/desktop/second') { const b = JSON.parse(o.body); T.calls.push(b); if (b.step < 0) { T.fails[b.id] = (T.fails[b.id] || 0) + 1; return J({error: 'wrong code'}, 403) }
        if (b.step <= (T.last[b.id] || 0)) return J({error: 'That code was used already: wait for the next one.'}, 409); T.last[b.id] = b.step; return J({}) }
      if (u.includes('/api/')) return J({});
      return f0(u, o) } });
  await p.goto('http://127.0.0.1:8149/'); await p.waitForFunction(() => typeof otpCheck == 'function' && IDB && IDB.db, null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    ACCTS.length=0;ACCTS.push({n:'boss',h:await pwMake('b1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'b1');
    window.elevate=async(t,o)=>o&&o.cred?{n:'boss',p:'b1'}:true;
    window.CODE=async(off)=>{const k=document.querySelector('.otpset .otpk');return k?null:null};return 1})()`);
  // set up
  await q("settings('usr')"); await wait(1200);
  ok('1. User Settings › Sign-in options: Authenticator app, off', /Authenticator app/.test(await q("document.body.textContent")) && await q("document.querySelector('.otps').textContent") == 'Off.');
  await q("document.querySelector('.otpa').click()"); await wait(400);
  const key = await q("document.querySelector('.otpset .otpk').textContent.replace(/ /g,'')");
  ok('2. the setup key (base32, 32 letters) and the otpauth link shown', /^[A-Z2-7]{32}$/.test(key) && /otpauth:\/\/totp\/Rudolf%20OS%3Aboss\?secret=/.test(await q("document.querySelector('.otpset .otpu').textContent")), key);
  const code = async (k, step) => q(`(async()=>{const a=${JSON.stringify(k)},m={};const B='ABCDEFGHIJKLMNOPQRSTUVWXYZ234567';let v=0,bits=0,o=[];for(const c of a){v=v<<5|B.indexOf(c);bits+=5;if(bits>=8){o.push((v>>>(bits-8))&255);bits-=8}}
    return otpCode(new Uint8Array(o),${step === undefined ? 'Math.floor(Date.now()/30000)' : step})})()`);
  await q("document.querySelector('.otpset .otpc').value='000000';document.querySelector('.otpset .otpp').value='b1';document.querySelector('.otpset .y').click()"); await wait(400);
  ok('3. a wrong code: refused, nothing turned on', /not right/.test(await q("document.querySelector('.otpset .otpm').textContent")) && !(await q('!!USER.otp')));
  const c1 = await code(key);
  await q(`document.querySelector('.otpset .otpc').value='${c1}';document.querySelector('.otpset .y').click()`); await wait(800);
  const acc = await q("({otp:USER.otp,keys:Object.keys(USER.otp||{})})");
  ok('4. the right code and password: on, the secret kept encrypted (iv, ct), not the key itself', acc.otp && acc.keys.join() == 'iv,ct' && !JSON.stringify(acc.otp).includes(key), acc);
  // sign-in: password right, then the code
  const pass = (c) => q(`(async()=>{const pr=otpCheck(ACCTS[0],'b1');await new Promise(r=>setTimeout(r,200));const d=document.querySelector('.otpdlg');if(!d)return 'no dialog';
    d.querySelector('.otpc').value='${c}';d.querySelector('.y').click();await new Promise(r=>setTimeout(r,400));const d2=document.querySelector('.otpdlg');const m=d2?d2.querySelector('.otpm').textContent:'';if(d2)d2.querySelector('.n').click();return {ok:await pr,m}})()`);
  let r = await pass('123456');
  ok('5. sign-in: a wrong code refused, said, counted by rudolfd', r.ok === false && /not right/.test(r.m) && (await q("T.fails[ACCTS[0].id]")) == 1, r);
  const step = await q('Math.floor(Date.now()/30000)'); const c2 = await code(key, step + 1);
  r = await pass(c2); ok('6. ...the right code (the next one, still in time): signed in', r.ok === true, r);
  r = await pass(c2); ok('7. ...the same code again: refused (used already)', r.ok === false && /used already/.test(r.m), r);
  // the lock screen
  await q('lockScreen()'); await wait(300);
  await q("document.querySelector('#lockov .lkp').value='b1';document.querySelector('#lockov .lkp').dispatchEvent(new Event('input'));document.querySelector('#lockov .lku').click()"); await wait(500);
  ok('8. the lock screen: after the password, the code asked for again', await q("!!document.querySelector('.otpdlg')") && await q("!!document.getElementById('lockov')"));
  await q("document.querySelector('.otpdlg .n').click()"); await wait(400);
  ok('9. ...cancelled: still locked', await q("!!document.getElementById('lockov')"));
  await q("document.getElementById('lockov').remove();LOCKED=false");
  // required
  await q("(async()=>{ACCTS.push(await acctNew('kid','k1','user'));saveUser();SYSC.mfa='all';sysSave()})()");
  await q("(async()=>{userLogout();USER=ACCTS.find(a=>a.n=='kid');await userLogin(USER,'k1');window.__afterLogin&&__afterLogin()})()"); await wait(1500);
  ok('10. required for every account: an account without it is asked to set it up before going on (Sign out, no Later)', await q("!!document.querySelector('.otpset')") && /requires two-step sign-in/.test(await q("document.querySelector('.otpset .otpwhy').textContent")) && await q("document.querySelector('.otpset .n').textContent") == 'Sign out');
  await q("settings('sec')"); await wait(1200);
  ok('11. Security Settings: Two-step sign-in, "Required for every account"', await q("document.querySelector('.otppol .otpq').value") == 'all');
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
