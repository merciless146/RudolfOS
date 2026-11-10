// 1.5.7 (plan Part 4, steps 2-3): Settings › System health › Desktop engine (Chromium or WebKit, a test), and websites
// in Chromium's own window when the desktop runs in WebKit.   node engine57_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 600) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8171, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  const page = async engine => { const p = await b.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message));
    await p.addInitScript(engine => {
      window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0, engine};
      window.M = {calls: [], choice: 'chromium', now: engine, ready: false};
      const f0 = window.fetch;
      window.fetch = (u, o) => { u = String(u); const q = new URL(u, location.href); const a = q.pathname.replace('/api/', ''); const J = (x, st) => Promise.resolve(new Response(JSON.stringify(Object.assign({ok: !st}, x)), {status: st || 200, headers: {'content-type': 'application/json'}}));
        if (u.includes('/api/')) M.calls.push(((o && o.method) || 'GET') + ' ' + a + q.search);
        if (a == 'desktop/engine') { if (o && o.method == 'POST') { const e = q.searchParams.get('engine'); if (e) { M.choice = e; if (e == 'webkit' && !M.ready) { M.ready = true; return J({message: 'installing'}) } } return J({message: 'saved'}) }
          return J({choice: M.choice, now: M.now, debian: true, ready: M.ready, failed: false, persistent: true}) }
        if (a == 'job') return J({running: false, status: 0, log: 'Setting up gir1.2-webkit2-4.1\ndone installed\n'});
        if (a == 'health') return J({events: [], now: 0});
        if (u.includes('/api/')) return J({items: [], apps: [], windows: []});
        return f0(u, o) } }, engine);
    await p.goto('http://127.0.0.1:8171/'); await p.waitForFunction(() => typeof enginePanel57 == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
    return {p, errs} };
  let {p, errs} = await page('webkit');
  const q = s => p.evaluate(s);
  ok('1. desktop in WebKit: every website goes to Chromium\'s own window; the desktop\'s own address does not', await q("webWants('https://example.com/a')&&webWants('http://news.example.org/')&&!webWants('http://127.0.0.1:8080/x')&&!webWants('about:blank')"));
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS.length=0;ACCTS.push({n:'ann',h:await pwMake('a1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'a1');window.__elevTrust=Date.now();return 1})()`);
  await wait(800); await q("(()=>{settings('health');return 1})()"); await wait(1500);
  let t = await q("(()=>{const d=document.querySelector('.eng57');return d?{txt:d.innerText,ch:[...d.querySelectorAll('input[name=eng57]')].find(r=>r.checked)?.value,ap:!d.querySelector('.engap').hidden}:null})()");
  ok('2. System health › Desktop engine (test): Chromium chosen, WebKit offered, what runs now', t && /Desktop engine \(test\)/i.test(t.txt) && t.ch == 'chromium' && /Now\s*WebKit/.test(t.txt) && t.ap === true, t);
  await q("(()=>{const r=document.querySelector('.eng57 input[value=webkit]');r.checked=true;r.dispatchEvent(new Event('change'));return 1})()"); await wait(1500);
  const c = await q("M.calls.filter(x=>/desktop\\/engine\\?/.test(x))"); t = await q("(()=>{const d=document.querySelector('.eng57');return {log:d.querySelector('.englog')?.textContent||'',ch:[...d.querySelectorAll('input[name=eng57]')].find(r=>r.checked)?.value}})()");
  ok('3. choosing WebKit saves it, and installs it in Linux when it is missing (with the log)', JSON.stringify(c) == '["POST desktop/engine?engine=webkit"]' && t.ch == 'webkit', [c, t]);
  await q("(()=>{M.now='chromium';document.querySelectorAll('.eng57').forEach(e=>e.remove());enginePanel57(document.querySelector('.eng57')?.parentElement||[...document.querySelectorAll('.win')].at(-1).querySelector('.hls').parentElement);return 1})()"); await wait(800);
  await q("(()=>{const b=document.querySelector('.eng57 .engap');b.click();return 1})()"); await wait(2500);
  ok('4. "Restart the desktop with it" asks rudolfd to start the desktop again', await q("M.calls.some(x=>x=='POST desktop/engine?apply=1')"), await q("M.calls"));
  ok('no page error (WebKit)', !errs.length, errs);
  await p.close(); ({p, errs} = await page('chromium'));
  ok('5. desktop in Chromium: only the sign-in sites go to the browser window, as before', await p.evaluate("webWants('https://accounts.google.com/')&&!webWants('https://example.com/a')"));
  ok('no page error (Chromium)', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
