// 1.5.7 (plan Part 4, steps 2-3): System Settings › Desktop engine (Standard: Chromium, or Lite: WebKit; applied at the
// next restart of the computer), and websites
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
        if (a == 'desktop/engine') { if (o && o.method == 'POST') { const e = q.searchParams.get('engine'); if (e) M.choice = e; return J({message: 'saved'}) }
          return J({choice: M.choice, now: M.now, available: M.avail !== false, failed: false, persistent: true, gaveUp: M.gaveUp || ''}) }
        if (a == 'shell') return J({id: 'x', on: true, failed: false, active: false, ready: true, installed: true});
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
  await wait(800); await q("(()=>{settings('sys');return 1})()"); await wait(1500);
  let t = await q("(()=>{const d=document.querySelector('.eng57');return d?{txt:d.innerText,v:d.querySelector('#eng57').value,opts:[...d.querySelectorAll('#eng57 option')].map(o=>o.textContent)}:null})()");
  ok('2. System Settings › Desktop engine: a list with Standard (Chromium) and Lite (WebKit, test); what is in use now', t && t.v == 'chromium' && JSON.stringify(t.opts) == '["Standard (Chromium)","Lite (WebKit, test)"]' && /In use now: Lite \(WebKit\)/.test(t.txt) && /changes when the computer restarts/.test(t.txt), t);
  await q("(()=>{const s=document.querySelector('#eng57');s.value='webkit';s.dispatchEvent(new Event('change'));return 1})()"); await wait(1500);
  const c = await q("M.calls.filter(x=>/desktop\\/engine\\?/.test(x))"); t = await q("(()=>{const d=document.querySelector('.eng57');return {v:d.querySelector('#eng57').value,txt:d.innerText}})()");
  ok('3. choosing Lite saves it; it is used after the computer restarts', JSON.stringify(c) == '["POST desktop/engine?engine=webkit"]' && t.v == 'webkit', [c, t]);
  await q("(()=>{M.now='chromium';document.querySelectorAll('.eng57').forEach(e=>e.remove());enginePanel57([...document.querySelectorAll('.win')].at(-1).querySelector('.eng57')?.parentElement||document.querySelector('#sdp').closest('div').parentElement);return 1})()"); await wait(800);
  t = await q("document.querySelector('.eng57 .engnow').textContent");
  ok('4. chosen but not yet in use: it says the change comes with the next restart (nothing restarts now)', /In use now: Standard \(Chromium\)\. Lite \(WebKit\) is used after the computer restarts/.test(t) && !(await q("M.calls.some(x=>/apply/.test(x))")), t);
  await q("(()=>{M.gaveUp='2026-11-10T21:59:00Z it closed 3 times in a row as it started';M.choice='chromium';document.querySelectorAll('.eng57').forEach(e=>e.remove());enginePanel57(document.querySelector('#sdp').closest('div').parentElement);return 1})()"); await wait(800);
  t = await q("document.querySelector('.eng57 .engnow').textContent");
  ok('4b. Lite given up by the session: Settings says when and why, and that choosing it again tries once more', /Lite was turned off on .+ because it closed 3 times in a row as it started\. Choose it again/.test(t), t);
  await q("M.gaveUp=''");
  await q("(()=>{M.avail=false;M.choice='chromium';document.querySelectorAll('.eng57').forEach(e=>e.remove());enginePanel57(document.querySelector('#sdp').closest('div').parentElement);return 1})()"); await wait(800);
  const o2 = await q("(()=>{const o=document.querySelector('#eng57 option[value=webkit]');return {d:o.disabled,t:o.textContent}})()");
  ok('4b. a Rudolf OS without the Lite engine (before this update): Lite cannot be chosen, and it says why', o2.d && /update Rudolf OS first/.test(o2.t), o2);
  await q("M.calls.length=0;shellOffer()"); await wait(1500);
  ok('6. 1.5.7: the Lite desktop never offers the shell\'s restart ("Real browser tabs are ready": a loop on the Vivobook)', !(await q("M.calls.some(x=>/shell\\/switch/.test(x))||!!document.querySelector('.rmodal h3')&&/Real browser tabs/.test(document.querySelector('.rmodal h3').textContent)")), await q('M.calls'));
  ok('no page error (WebKit)', !errs.length, errs);
  await p.close(); ({p, errs} = await page('chromium'));
  ok('5. desktop in Chromium: only the sign-in sites go to the browser window, as before', await p.evaluate("webWants('https://accounts.google.com/')&&!webWants('https://example.com/a')"));
  await p.evaluate("M.calls.length=0;shellOffer()"); await wait(1500);
  ok('7. ...the Standard desktop still does, once', await p.evaluate("M.calls.some(x=>/POST shell\\/switch/.test(x))"), await p.evaluate('M.calls'));
  ok('no page error (Chromium)', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
