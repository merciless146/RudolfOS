// 1.6.3 (docs/plan-1.6.3.md Part 2, step 2): the page's side of fingerprint readers (desktop/fprint63.js) in User Settings ›
// Sign-in options › Fingerprints, with a stand-in rudolfd answering fp/* as rudolfd-fprint.inc does (its reader played by
// the test): no reader said plainly; a reader libfprint does not support named; an account whose files are not on the
// disk; adding a finger (the password asked, the reader's scans shown as they come, the request carrying the account and
// the password), removing one; an administrator's Fingerprints… for another account (the administrator's prompt, adding
// only while that account's files are open); Guest has none; no page errors.
//   node fprint63_t.js [CHROME]   (build iso/www first; rudolfd's own side: iso/qt/host/fprint163_t.sh)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 600) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8730, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.addInitScript(() => {
    window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0};
    // the stand-in rudolfd: T.reader (null, 'unsupported' or a name), each account's fingers, disk and open; T.stage the scans so far
    window.T = {reader: null, fingers: {}, disk: {}, open: {}, stage: 0, calls: []};
    const f0 = window.fetch;
    window.fetch = async (u, o) => { u = String(u); const q = new URL(u, location.href), J = (x, st) => new Response(JSON.stringify(Object.assign({ok: !st}, x)), {status: st || 200, headers: {'content-type': 'application/json'}});
      const pth = q.pathname.replace('/api/', '');
      if (pth == 'fp/status') { const id = q.searchParams.get('acct');
        return J({helper: true, readers: T.reader && T.reader != 'unsupported' ? [{name: T.reader, driver: 'x', scan: 'press', stages: 5}] : [],
          unsupported: T.reader == 'unsupported' ? [{usb: '04f3:036b', name: 'ELAN Fingerprint Sensor'}] : [], disk: !!T.disk[id], open: !!T.open[id],
          fingers: T.fingers[id] || [], signed: true, fails: 0, ready: !!(T.fingers[id] || []).length, why: ''}) }
      if (pth == 'fp/progress') return J({stage: T.stage, of: 5, msg: T.stage < 5 ? 'Lift your finger and put it on the reader again.' : 'Done.'});
      if (pth.startsWith('fp/')) { const bd = JSON.parse(o.body || '{}'), w = pth.slice(3); T.calls.push({w, acct: bd.acct, n: bd.n, p: bd.p, finger: bd.finger, all: bd.all});
        if (w == 'cancel') return J({});
        const a = ACCTS.find(x => x.n == bd.n); if (!a || !(await pwCheck(a, bd.p))) return J({error: 'Wrong password.'}, 403);
        if (w == 'enroll') { for (T.stage = 0; T.stage < 5; T.stage++) await new Promise(r => setTimeout(r, 400)); (T.fingers[bd.acct] = T.fingers[bd.acct] || []).push(bd.finger); return J({finger: bd.finger}) }
        if (w == 'remove') { T.fingers[bd.acct] = bd.all ? [] : (T.fingers[bd.acct] || []).filter(f => f != bd.finger); return J({}) } }
      if (u.includes('/api/')) return J({});
      return f0(u, o) } });
  await p.goto('http://127.0.0.1:8730/'); await p.waitForFunction(() => typeof fpRow == 'function' && IDB && IDB.db, null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    ACCTS.length=0;ACCTS.push(await acctNew('boss','b1','admin'));ACCTS.push(await acctNew('kid','k1','user'));saveUser();USER=ACCTS[0];await userLogin(USER,'b1');
    window.elevate=async(t,o)=>{window.__elev=t;return o&&o.cred?{n:'boss',p:'b1'}:true};
    document.getElementById('login').style.display='none';return 1})()`);
  const me = await q('USER.id'), kid = await q('ACCTS[1].id');
  const open = async () => { await q("document.querySelectorAll('.win').forEach(w=>{try{w.querySelector('.xb')&&w.querySelector('.xb').click()}catch(e){}})"); await q("settings('usr')"); await wait(1200) };
  const says = () => q("(document.querySelector('.fpsec .fps')||{}).textContent||''");
  await open();
  ok('1. User Settings › Sign-in options has Fingerprints; no reader: said plainly (a touchpad cannot read one)', /No fingerprint reader is connected/.test(await says()) && /touchpad/.test(await says()) && await q("document.querySelector('.fpsec .fpa').disabled"), await says());
  await q("T.reader='unsupported'"); await open();
  ok('2. a reader libfprint does not support: named, with its USB id, and why', /ELAN Fingerprint Sensor/.test(await says()) && /04f3:036b/.test(await says()) && /does not support/.test(await says()), await says());
  await q("T.reader='Synaptics Sensors'"); await open();
  ok('3. a reader, but the account\'s files are not on the disk: said; Add stays off', /not on this computer/.test(await says()) && await q("document.querySelector('.fpsec .fpa').disabled"), await says());
  await q(`T.disk['${me}']=T.open['${me}']=true`); await open();
  ok('4. a reader and the files open: no fingerprints yet; Add on', /Synaptics Sensors\. No fingerprints yet/.test(await says()) && await q("!document.querySelector('.fpsec .fpa').disabled"), await says());
  await q("document.querySelector('.fpsec .fpa').click()"); await wait(300);
  await q("(()=>{const d=document.querySelector('.fidoask');d.querySelector('.fpw').value='b1';d.querySelector('.y').click()})()"); await wait(400);
  const opts = await q("[...document.querySelectorAll('.fpdlg .fpf option')].map(o=>o.value)");
  await q("document.querySelector('.fpdlg .fpf').value='left-thumb';document.querySelector('.fpdlg .y').click()"); await wait(1300);
  const mid = await q("({m:document.querySelector('.fpdlg .fpm').textContent,w:document.querySelector('.fpdlg .fpbv').style.width})");
  await wait(2200);
  const calls = await q('T.calls');
  ok('5. adding: the password asked, a finger chosen (all ten offered), the scans shown as they come', opts.length == 10 && /\(\d of 5\)/.test(mid.m) && parseInt(mid.w) > 0, [opts.length, mid]);
  ok('...the request carries the account, the finger and its own password; the dialog closes; the finger listed', calls.some(c => c.w == 'enroll' && c.acct == me && c.n == 'boss' && c.p == 'b1' && c.finger == 'left-thumb') && !(await q("!!document.querySelector('.fpdlg')")) && /Left thumb/.test(await q("document.querySelector('.fpsec .fpl').textContent")) && /1 fingerprint/.test(await says()), [calls, await says()]);
  await q("document.querySelector('.fpsec .fpl button').click()"); await wait(300);
  await q("(()=>{const d=document.querySelector('.fidoask');d.querySelector('.fpw').value='nope';d.querySelector('.y').click()})()"); await wait(800);
  ok('6. removing with a wrong password: not removed, said', /Left thumb/.test(await q("document.querySelector('.fpsec .fpl').textContent")));
  await q("document.querySelector('.fpsec .fpl button').click()"); await wait(300);
  await q("(()=>{const d=document.querySelector('.fidoask');d.querySelector('.fpw').value='b1';d.querySelector('.y').click()})()"); await wait(800);
  ok('...with the password: removed', !/Left thumb/.test(await q("document.querySelector('.fpsec .fpl').textContent")) && /No fingerprints yet/.test(await says()), await says());
  // ---- an administrator and another account's fingers
  const kb = await q("[...document.querySelectorAll('.fpsec .fpo button')].map(b=>b.textContent)");
  ok('7. an administrator sees the other accounts\' Fingerprints…', JSON.stringify(kb) == '["kid…"]', kb);
  await q(`T.disk['${kid}']=true`); await q("document.querySelector('.fpsec .fpo button').click()"); await wait(800);
  const ow = await q("document.querySelector('.fpother .fpw').textContent");
  ok('8. kid\'s files not open: adding is off, and why', await q("document.querySelector('.fpother .fpa').disabled") && /kid’s files are not open/.test(ow), ow);
  await q("document.querySelector('.fpother .n').click()"); await q(`T.disk['${kid}']=T.open['${kid}']=true;T.calls=[]`);
  await q("document.querySelector('.fpsec .fpo button').click()"); await wait(800);
  await q("document.querySelector('.fpother .fpa').click()"); await wait(400);
  await q("document.querySelector('.fpdlg .y').click()"); await wait(4000);
  const c2 = await q('T.calls'), ev = await q('window.__elev');
  ok('9. kid\'s files open: an administrator adds one (the administrator\'s prompt, the request with the administrator\'s name)', c2.some(c => c.w == 'enroll' && c.acct == kid && c.n == 'boss') && /Add a fingerprint \(kid\)/.test(ev || ''), [c2, ev]);
  ok('...listed in kid\'s dialog', /Right index finger/.test(await q("document.querySelector('.fpother .fpl').textContent")));
  await q("document.querySelector('.fpother .n').click()");
  // ---- the account that is not an administrator, and Guest
  await q("USER=ACCTS[1]"); await open();
  ok('10. an account that is not an administrator sees only its own', await q("!!document.querySelector('.fpsec')&&!document.querySelector('.fpsec .fpo')"));
  await q("USER={n:'Guest',r:'guest',id:'guest'}"); await open();
  ok('11. Guest: no fingerprints', await q("!document.querySelector('.fpsec')"));
  ok('12. no page errors', !errs.length, errs);
  console.log(`${R.filter(x => x).length}/${R.length} passed`); await b.close(); srv.close(); process.exit(R.every(x => x) ? 0 : 1)
})().catch(e => { console.log('FAIL ' + e.stack); process.exit(1) });
