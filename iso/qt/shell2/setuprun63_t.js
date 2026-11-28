// 1.6.3 (the owner, 2026-11-28): a new installation from a stick made in Update and recovery › Make a USB stick booted, its
// setup wizard (the first account's, an administrator's) came up, and the browser started again at once over it, because the
// desktop's one-time move to the shell (shell26.js shellOffer) fires by itself right after start-up on a plain page. The person
// was left at the sign-in screen, the only account the placeholder "admin" nobody chose a password for, so the computer could
// not be used. Now the move waits until Rudolf OS is set up ("bd95setup", the wizard's last step), and never happens with the
// wizard on the screen; a desktop that is set up still moves as before.   node setuprun63_t.js [CHROME]  (build the page first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www'), PORT = 8764;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 700) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(PORT, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  // a plain Chromium desktop page that can use the shell: rudolfd's answers given here (the shell there, ready and installed)
  const page = async () => { const p = await b.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message));
    await p.addInitScript(() => {
      window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0, engine: 'chromium'};
      window.M = {calls: []};
      const f0 = window.fetch;
      window.fetch = (u, o) => { u = String(u); const q = new URL(u, location.href); const a = q.pathname.replace('/api/', ''); const m = (o && o.method) || 'GET';
        const J = (x, st) => Promise.resolve(new Response(JSON.stringify(Object.assign({ok: !st}, x)), {status: st || 200, headers: {'content-type': 'application/json'}}));
        if (u.includes('/api/')) M.calls.push(m + ' ' + a + q.search);
        if (a == 'shell') return J({id: 'x', on: true, failed: false, active: false, ready: true, installed: true});
        if (a == 'shell/switch') return J({message: 'the browser starts again'});
        if (a == 'desktop/engine') return J({choice: 'chromium', now: 'chromium', available: true, nativeAvailable: true, failed: false, persistent: true, gaveUp: '', offer: false});
        if (a == 'job') return J({running: false, status: 0, log: ''});
        if (a == 'health') return J({events: [], now: 0});
        if (u.includes('/api/')) return J({items: [], apps: [], windows: []});
        return f0(u, o) } });
    await p.goto(`http://127.0.0.1:${PORT}/`);
    await p.waitForSelector('#oobe', {timeout: 30000});
    await p.waitForFunction(() => typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db && typeof shellOffer == 'function', null, {timeout: 30000});
    return {p, errs} };
  const calls = p => p.evaluate("M.calls.filter(x=>/POST shell\\/switch/.test(x)).length");
  const boxed = p => p.evaluate("(()=>{const h=document.querySelector('.rmodal h3');return !!h&&/Real browser tabs are ready/.test(h.textContent)})()");

  // 1. a new installation: the wizard is up, and the automatic move (about 5.5 s after the start) leaves it alone
  const A = await page(), pa = A.p;
  ok('1. a new installation: the setup wizard is on the screen (step 1)', (await pa.textContent('#oobe .oostep')).trim() == 'Step 1 of 5', await pa.textContent('#oobe .oostep'));
  await wait(8000);
  ok('2. ...and the browser is not started again over it (no move to the shell, no "Real browser tabs are ready")',
    await calls(pa) == 0 && !await boxed(pa), [await calls(pa), await boxed(pa)]);
  await pa.evaluate("shellOffer()"); await wait(1500);
  ok('3. ...not even when it offers again: the wizard is still there, at the step it was at',
    await calls(pa) == 0 && (await pa.textContent('#oobe .oostep')).trim() == 'Step 1 of 5', [await calls(pa), await pa.textContent('#oobe .oostep')]);
  ok('no page error (a new installation)', !A.errs.length, A.errs);
  await pa.close();

  // 2. a desktop that is set up: the move still happens, by itself, as before
  const B = await page(), pb = B.p;
  await pb.evaluate("(()=>{svj('bd95setup',1);const o=document.getElementById('oobe');o&&o.remove();return ld('bd95setup',0)})()");
  await pb.evaluate("M.calls.length=0");
  await wait(8000);
  ok('4. set up: the browser starts again by itself for the shell, once, with the box saying why',
    await calls(pb) >= 1 && await boxed(pb), [await calls(pb), await boxed(pb), await pb.evaluate('M.calls')]);
  ok('no page error (set up)', !B.errs.length, B.errs);
  await pb.close();

  console.log(R.filter(Boolean).length + '/' + R.length + ' passed');
  await b.close(); srv.close(); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.log('FAIL ' + e.message); process.exit(1) });
