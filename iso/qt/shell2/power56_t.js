// 1.5.6: the power button asks before signing out. On the Vivobook the power button, pressed to get out of a stuck
// game, signed the person out first and Cancel left them at the sign-in screen. Now: Cancel goes back to the desktop
// as it was; Shut down and Restart sign out once chosen; Sleep keeps the session behind the lock screen.
//   node power56_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8155, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd|debian/.test(e.stack || e.message || '')) errs.push(e.message) });
  await p.addInitScript(() => {
    window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0};
    window.T = {calls: []};
    const f0 = window.fetch;
    window.fetch = (u, o) => { u = String(u); const J = x => Promise.resolve(new Response(JSON.stringify(Object.assign({ok: true}, x)), {headers: {'content-type': 'application/json'}}));
      const q = new URL(u, location.href);
      if (/\/api\/power/.test(q.pathname)) T.calls.push(q.pathname.slice(5) + q.search);
      if (u.includes('/api/')) return J({items: [], apps: []});
      return f0(u, o) } });
  await p.goto('http://127.0.0.1:8155/'); await p.waitForFunction(() => typeof shutdownDlg == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  const q = s => p.evaluate(s);
  const signIn = () => q(`(async()=>{try{svj('bd95setup',1)}catch(e){}const o=document.getElementById('oobe');o&&o.remove();
    if(!ACCTS.length){ACCTS.push({n:'ann',h:await pwMake('a1'),r:'admin'});saveUser()}USER=ACCTS[0];await userLogin(USER,'a1');LOCKED=false;
    const L=document.getElementById('login');if(L)L.style.display='none';document.querySelectorAll('.rmodal').forEach(m=>m.remove());return 1})()`);
  const loginShown = () => q("(()=>{const L=document.getElementById('login');return !!L&&getComputedStyle(L).display!='none'})()");
  const press = a => q(`(()=>{const o=[...document.querySelectorAll('.rmodal')].at(-1);o.querySelector("[data-a='${a}']").click();return 1})()`);
  await signIn();
  // the power button (rudolfd's event "power 1")
  await q("(()=>{evDispatch('power 1');return 1})()"); await wait(200);
  ok('1. the power button asks what to do, and the person is still signed in meanwhile', await q("!!document.querySelector('.rmodal [data-a=poweroff]')&&!!SESSION"), await q("!!SESSION"));
  await press('x'); await wait(200);
  ok('2. Cancel goes back to the desktop: still signed in, no sign-in screen', await q("!!SESSION&&!document.querySelector('.rmodal')") && !(await loginShown()), [await q("!!SESSION"), await loginShown()]);
  // Start › Shut down (A.off) is the same
  await q("(()=>{A.off();return 1})()"); await wait(200);
  ok('3. Start › Shut down asks without signing out too', await q("!!document.querySelector('.rmodal [data-a=poweroff]')&&!!SESSION"));
  await press('x'); await wait(200);
  ok('4. ...and Cancel keeps the session', await q("!!SESSION") && !(await loginShown()));
  // Sleep: the session stays, behind the lock screen
  await q("(()=>{evDispatch('power 1');return 1})()"); await wait(200); await press('suspend'); await wait(600);
  ok('5. Sleep keeps the person signed in, behind the lock screen', await q("!!SESSION&&LOCKED") && (await q("T.calls")).some(c => /power\?action=suspend/.test(c)), [await q("!!SESSION"), await q("LOCKED"), await q("T.calls")]);
  await signIn();
  // Shut down: signs out, saves, turns off
  await q("(()=>{evDispatch('power 1');return 1})()"); await wait(200); await press('poweroff'); await wait(3800);
  ok('6. Shut down signs out once chosen, then turns off', await q("!SESSION") && (await q("T.calls")).some(c => /power\?action=poweroff/.test(c)), [await q("!!SESSION"), await q("T.calls")]);
  // on the sign-in screen, Cancel stays there
  await q("(()=>{document.querySelectorAll('.rmodal').forEach(m=>m.remove());shutdownDlg();return 1})()"); await wait(200); await press('x'); await wait(200);
  ok('7. from the sign-in screen, Cancel stays at the sign-in screen', await q("!SESSION") && await loginShown());
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
