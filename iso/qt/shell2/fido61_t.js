// 1.6.1 (docs/plan-1.6.1.md Part 18): the page's side of security keys that unlock the files (desktop/fido61.js), with a
// stand-in rudolfd that answers desktop/keys* as rudolfd-fido.inc does (its touch played by the test): User Settings ›
// Sign-in options › Security keys (none, adding one and its "fks" box kept, the PIN asked, a spare, requiring a key: the
// password's copies of the file key and the administrator's key deleted), the sign-in screen (the password, then the key;
// the files opened with the keys rudolfd gives, the same file key as before; a wrong key and Cancel: not signed in), the
// lock screen, a new password (the box sealed again, no password copy made again), the old page-only key moved over with
// consent, an administrator's password reset removing the account's keys first.
//   node fido61_t.js [CHROME]   (build iso/www first; rudolfd's own side: iso/qt/host/fido161_t.sh)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 600) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8613, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.addInitScript(() => {
    window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0};
    // the stand-in rudolfd: the keys of each account; T.touch: what the next touch does ('ok', 'wrong', 'none')
    window.T = {keys: {}, req: {}, touch: 'ok', calls: [], fk: {}, pk: {}, pv: {}, pws: {}, pinKey: false};
    const f0 = window.fetch, hex = u => [...u].map(x => x.toString(16).padStart(2, '0')).join('');
    window.fetch = async (u, o) => { u = String(u); const q = new URL(u, location.href), J = (x, st) => new Response(JSON.stringify(Object.assign({ok: !st}, x)), {status: st || 200, headers: {'content-type': 'application/json'}});
      const pth = q.pathname.replace('/api/', '');
      if (pth == 'desktop/keys') { const id = q.searchParams.get('acct'); return J({lib: true, devs: 1, req: !!T.req[id], bad: false, keys: (T.keys[id] || []).map((n, i) => ({i, name: n, added: 1, last: 0}))}) }
      if (pth.startsWith('desktop/keys/')) { const b = JSON.parse(o.body), w = pth.slice(13); T.calls.push(w + (b.k ? '+k' : ''));
        const a = ACCTS.find(x => x.n == b.n); if (!a || !(await pwCheck(a, b.p))) return J({error: 'The user name or password is incorrect.'}, 403);
        const id = a.id, ks = T.keys[id] || (T.keys[id] = []);
        if (w == 'add') { if (ks.length && !b.k && T.req[id]) return J({need: 'key', error: 'Touch one of your security keys first.'}, 401);
          if (!b.k) return J({need: 'key', error: 'Touch your security key.'}, 401);
          if (T.pinKey && !b.pin) return J({need: 'pin', error: 'This security key has a PIN: type it to add the key.'}, 401);
          if (T.pinKey && b.pin != '2468') return J({need: 'pin', error: 'Wrong PIN for the security key.'}, 401);
          let fks; if (!ks.length) { const pk = await deriveKey(b.p, a.ks); T.fk[id] = hex(await acDec(pk, a.fk)); T.pk[id] = hex(new Uint8Array(await pbkBits(b.p, a.ks, 256)));
            if (a.priv) T.pv[id] = b64e(await acDec(pk, a.priv)); const pws = crypto.getRandomValues(new Uint8Array(32)); T.pws[id] = hex(pws); fks = await acEnc(pk, pws) }
          ks.push(b.name); return J({n: ks.length, req: !!T.req[id], fks}) }
        if (w == 'signin') { if (!ks.length) return J({error: 'none'}, 404); if (!b.k) return J({need: 'key', error: 'Touch your security key.'}, 401);
          if (T.touch == 'wrong') return J({need: 'key', error: "This security key is not one of this account's keys."}, 403);
          return J({req: !!T.req[id], pk: hex(new Uint8Array(await pbkBits(b.p, a.ks, 256))), fk: T.fk[id], pv: T.pv[id]}) }
        if (w == 'require') { if (b.on && ks.length < 2) return J({error: 'Add a second security key first.'}, 409); if (!b.k) return J({need: 'key', error: 'Touch your security key.'}, 401);
          T.req[id] = !!b.on; return J({req: !!b.on}) }
        if (w == 'remove') { ks.splice(b.i, 1); return J({n: ks.length, req: !!T.req[id]}) }
        if (w == 'reset') { T.keys[b.acct] = []; T.req[b.acct] = false; return J({message: 'removed'}) } }
      if (u.includes('/api/')) return J({});
      return f0(u, o) } });
  await p.goto('http://127.0.0.1:8613/'); await p.waitForFunction(() => typeof fidoSignin == 'function' && IDB && IDB.db, null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    ACCTS.length=0;ACCTS.push(await acctNew('boss','b1','admin'));ACCTS.push(await acctNew('kid','k1','user'));saveUser();USER=ACCTS[0];await userLogin(USER,'b1');
    HOME.DOCUMENTS['KEEP.TXT']='mine';fsSave();fsFlush();window.elevate=async(t,o)=>o&&o.cred?{n:'boss',p:'b1'}:true;
    document.getElementById('login').style.display='none';return 1})()`);
  const fk0 = await q("[...FKRAW].map(x=>x.toString(16).padStart(2,'0')).join('')");
  // ---- Settings
  await q("settings('usr')"); await wait(1200);
  ok('1. User Settings › Sign-in options: Security keys, none; the old page-only "Add a security key or passkey" hidden', /Security keys/.test(await q("document.body.textContent")) && await q("document.querySelector('.fidorow .fks').textContent") == 'None.' &&
     await q("getComputedStyle(document.querySelector('.pka')).display") == 'none');
  ok('2. ...requiring is off and cannot be turned on with no keys', await q("document.querySelector('.fkq').disabled&&!document.querySelector('.fkq').checked"));
  const add = async (name, pw, pin) => { await q("document.querySelector('.fka').click()"); await wait(300);
    await q(`(()=>{const d=document.querySelector('.fidoask');d.querySelector('.fnm').value=${JSON.stringify(name)};d.querySelector('.fpw').value=${JSON.stringify(pw)};d.querySelector('.y').click()})()`); await wait(600);
    if (pin !== undefined) { const has = await q("!!document.querySelector('.fidoask .fpin')&&getComputedStyle(document.querySelector('.fidoask .fpin')).display!='none'");
      if (!has) return 'no pin box'; await q(`(()=>{const d=document.querySelector('.fidoask');d.querySelector('.fpw').value=${JSON.stringify(pw)};d.querySelector('.fpin').value=${JSON.stringify(pin)};d.querySelector('.y').click()})()`); await wait(600) }
    await wait(500); return 'ok' };
  await add('Blue key', 'b1');
  ok('3. a key added: the password, then the touch (two requests: need key, then with k); the account keeps its box (fks)', JSON.stringify(await q('T.calls')) == '["add","add+k"]' && await q("!!(USER.fks&&USER.fks.iv&&USER.fks.ct)&&USER.fido.n==1"), await q('[T.calls,USER.fido]'));
  ok('4. ...the list shows it; requiring still off (one key: add a spare)', /Blue key/.test(await q("document.querySelector('.fkl').textContent")) && await q("document.querySelector('.fkq').disabled") && /spare/.test(await q("document.querySelector('.fkw').textContent")));
  await q('T.pinKey=true;T.calls=[]'); const pr = await add('Spare', 'b1', '2468'); await q('T.pinKey=false');
  ok('5. a key with a PIN: the PIN asked (never kept), added; two keys now', pr == 'ok' && await q("USER.fido.n==2&&!JSON.stringify(USER).includes('2468')"), [pr, await q('T.calls')]);
  await wait(500);
  ok('6. ...requiring can now be turned on', await q("!document.querySelector('.fkq').disabled"));
  await q("document.querySelector('.fkq').click()"); await wait(300);
  await q("(()=>{const d=document.querySelector('.fidoask');d.querySelector('.fpw').value='b1';d.querySelector('.y').click()})()"); await wait(1200);
  ok('7. required: a touch asked; the password\'s copies of the file key and the administrator\'s key deleted from the account', await q("T.req[USER.id]&&!USER.fk&&!USER.priv&&USER.fido.req&&!!USER.fks"), await q("({req:T.req[USER.id],fk:!!USER.fk,priv:!!USER.priv,fido:USER.fido})"));
  ok('8. ...Settings says the files need a key, and that administrators still have their copy', /files need one of them/.test(await q("document.querySelector('.fks').textContent")) && /Administrators can still open/.test(await q("document.querySelector('.fkw').textContent")));
  // ---- the sign-in screen
  await q("userLogout&&userLogout()"); await wait(300);
  await q("showLogin()"); await wait(200);
  const signin = async (pw, touch, cancel) => { await q(`T.touch='${touch}'`); await p.fill('#lu', 'boss'); await p.click('#lp'); await p.keyboard.type(pw); await p.keyboard.press('Enter'); await wait(1500);
    const dlg = await q("(()=>{const d=document.querySelector('.fidodlg');return d?d.querySelector('.fidom').textContent:null})()");
    if (dlg !== null && cancel) { await q("document.querySelector('.fidodlg .n').click()"); await wait(500) } return dlg };
  let m = await signin('b1', 'wrong', true);
  ok('9. sign-in with a key that is not this account\'s: said in the dialog; Cancel: not signed in', /not one of this account/.test(m || '') && await q("getComputedStyle(document.getElementById('login')).display") != 'none', m);
  await q('T.calls=[]'); m = await signin('b1', 'ok');
  const after = await q("({u:USER&&USER.n,fk:FKRAW&&[...FKRAW].map(x=>x.toString(16).padStart(2,'0')).join(''),home:HOME&&HOME.DOCUMENTS&&HOME.DOCUMENTS['KEEP.TXT'],login:getComputedStyle(document.getElementById('login')).display,adm:!!ADMPRIV,fk2:!!USER.fk})");
  ok('10. the right key: signed in with the keys rudolfd gave; the same file key, the files there; no password copy made again', after.u == 'boss' && after.fk == fk0 && after.home == 'mine' && after.login == 'none' && !after.fk2, after);
  ok("11. ...an administrator's private key from rudolfd (its password copy is gone)", after.adm, after);
  ok('12. ...no code or old key asked after the key (the key is the second step)', JSON.stringify(await q('T.calls')) == '["signin","signin+k"]', await q('T.calls'));
  // ---- the lock screen
  await q('lockScreen()'); await wait(300); await q("T.touch='ok';T.calls=[]");
  await q("document.querySelector('#lockov .lkp').value='b1';document.querySelector('#lockov .lkp').dispatchEvent(new Event('input'));document.querySelector('#lockov .lku').click()"); await wait(1500);
  ok('13. the lock screen: the password, then the key; unlocked', !(await q("!!document.getElementById('lockov')")) && JSON.stringify(await q('T.calls')) == '["signin","signin+k"]', await q('T.calls'));
  // ---- a new password
  const pws0 = await q("(async()=>[...await acDec(PKEY,USER.fks)].map(x=>x.toString(16).padStart(2,'0')).join(''))()");
  await q("(async()=>{USER.h=await pwMake('b2');await homeRekey('b2')})()"); await wait(500);
  const pws1 = await q("(async()=>{try{return [...await acDec(await deriveKey('b2',USER.ks),USER.fks)].map(x=>x.toString(16).padStart(2,'0')).join('')}catch(e){return 'x'}})()");
  ok('14. a new password: the box sealed again with it (the same secret: no key needed), no password copy of the file key made', pws1 == pws0 && pws0.length == 64 && !(await q('!!USER.fk||!!USER.priv')), [pws0, pws1]);
  // ---- an old page-only key moved over
  await q("(async()=>{userLogout();USER=ACCTS.find(a=>a.n=='kid');await userLogin(USER,'k1');USER.pk={id:'AAAA',alg:-7,jwk:{}};saveUser()})()"); await wait(500);
  await q("settings('usr')"); await wait(1200);
  ok('15. an account with an old page-only key: Settings says to set it up again (files, Native and Lite)', /Native and Lite cannot use it/.test(await q("document.querySelector('.fko').textContent")));
  await q('T.calls=[]'); await add('New key', 'k1');
  ok('16. ...added again here (consent in the dialog): the old one removed', await q('!USER.pk&&USER.fido.n==1'), await q('[T.calls,!!USER.pk]'));
  // ---- an administrator resets the password
  await q("(async()=>{userLogout();USER=ACCTS[0];await userLogin(USER,'b2',{pk:Uint8Array.from((T.pk[USER.id]||'').match(/../g).map(x=>parseInt(x,16))),fk:Uint8Array.from(T.fk[USER.id].match(/../g).map(x=>parseInt(x,16)))})})()"); await wait(500);
  const kid = await q("ACCTS.find(a=>a.n=='kid').id"); await q("T.calls=[];window.elevate=async(t,o)=>o&&o.cred?{n:'boss',p:'b2'}:true");
  const kept = await q("(async()=>{try{return await acReset(ACCTS.find(a=>a.n=='kid'),'new-kid')}catch(e){return 'ERR '+e.message}})()");
  ok("17. an administrator resets kid's password: kid's keys removed first (rudolfd), kid's box and hint gone", JSON.stringify(await q('T.calls')).includes('reset') && await q(`(T.keys['${kid}']||[]).length==0`) && await q("(()=>{const k=ACCTS.find(a=>a.n=='kid');return !k.fks&&!k.fido})()"), [kept, await q('T.calls')]);
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
