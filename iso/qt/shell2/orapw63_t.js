// 1.6.3 (docs/plan-1.6.3.md Part 7, step 1): Oracle's own password manager in the page (desktop/orapw63.js), with a stand-in shell
// and a stand-in rudolfd (its oracle/pw/* requests: a list kept here; the real store is iso/qt/host/webpw63_t.sh's, the scripts in
// the page orapwjs63_t.js's). Made-up passwords only.
//   1-6   a sign-in sent: "Save your password for shop.example (ann)?" in Oracle's bar; Save keeps it for that exact origin; the
//         same again is not asked; a changed one asks to Update; Never for this site; Not now
//   7-10  never in a private window, a Tor window or Guest (nothing asked of rudolfd either); never an http: or odd origin; a
//         look-alike (xn--) host is said
//   11-15 a password field with the keyboard: "Sign in to shop.example as [ann]" and Fill; nothing goes to the page before Fill is
//         clicked; then only that origin's password, to that tab; a page gone to another site: said; a new password: a strong one
//         shown first, then filled as new
//   16-19 ☰ › Passwords: the sites and users; Show asks for the account's password (a wrong one refused); Remove; the Never list
//   node orapw63_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 500) : '')) };
const wait = ms => new Promise(r => setTimeout(r, ms));
let KEPT = [], CALLS = [];
const srv = http.createServer((q, s) => {
  if (q.url.startsWith('/api/')) { let b = ''; q.on('data', d => b += d); q.on('end', () => {
    const p = q.url.split('?')[0].slice(5); let j = {ok: true};
    if (p.startsWith('oracle/pw/')) { CALLS.push([p, b]); const [o, u, pw] = b.split('\n'), k = KEPT.findIndex(x => x.origin == o && x.user == u);
      if (p == 'oracle/pw/list') j = {ok: true, signedIn: true, items: KEPT.map(x => ({origin: x.origin, user: x.user, when: 1}))};
      else if (p == 'oracle/pw/get') j = k >= 0 ? {ok: true, pw: KEPT[k].pw} : {ok: false, error: 'none kept for this site'};
      else if (p == 'oracle/pw/set') { if (k >= 0) KEPT[k].pw = pw; else KEPT.push({origin: o, user: u, pw}) }
      else if (p == 'oracle/pw/del') { if (k >= 0) KEPT.splice(k, 1) } }
    s.writeHead(200, {'content-type': 'application/json'}); s.end(JSON.stringify(j)) }); return }
  s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html')));
}).listen(8729, '127.0.0.1');
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  const p = await b.newPage({viewport: {width: 1280, height: 800}}); const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.addInitScript(() => { window.RUDOLFD = {token: 'test', engine: 'chromium'} });
  await p.goto('http://127.0.0.1:8729/'); await p.waitForFunction(() => typeof opwEvent == 'function' && typeof browser == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    USER={n:'Ann',r:'admin',id:'a1'};window.__sent=[];window.__net=0;SHELL=window;SHELL_CAPS=['tabs'];window.postMessage=m=>{__sent.push(m)};return 1})()`);
  const open = async (u, o) => { await p.evaluate(([u, o]) => { if (o == 'priv') browser(u, {priv: 1}); else if (o == 'tor') browser(u, {tor: 1}); else browser(u) }, [u, o]); await wait(300);
    return p.evaluate(() => { const m = __sent.filter(m => m.op == 'open').pop(); return m.tid }) };
  const ev = (tid, e, o) => p.evaluate(([tid, e, o]) => { dispatchEvent(new MessageEvent('message', {source: window, data: Object.assign({rudolfShell: 'tabev', tid, ev: e}, o)})) }, [tid, e, o]);
  const bar = () => p.evaluate(() => { const b = document.querySelector('.win.act .opwbar'); return b && !b.hidden ? b.textContent : '' });
  const click = t => p.evaluate(t => { const x = [...document.querySelectorAll('.win.act .opwbar button')].find(e => e.textContent == t); if (!x) return false; x.click(); return true }, t);
  const O = 'https://shop.example';
  // ---- saving
  const t1 = await open(O + '/login');
  await ev(t1, 'pwsave', {origin: O, user: 'ann', pw: 'Made-up-1'}); await wait(400);
  ok('1. a sign-in sent: "Save your password for shop.example (ann)?" in Oracle\'s bar, with Save, Never for this site, Not now', /Save your password for shop\.example \(ann\)\?SaveNever for this siteNot now/.test(await bar()), await bar());
  ok('2. ...nothing kept before Save is clicked', KEPT.length == 0 && !CALLS.some(c => c[0] == 'oracle/pw/set'));
  await click('Save'); await wait(300);
  ok('3. Save: kept for that exact origin and user (rudolfd oracle/pw/set, one a line), said in the bar', KEPT.length == 1 && CALLS.find(c => c[0] == 'oracle/pw/set')[1] == O + '\nann\nMade-up-1' && /Saved in your keyring, for shop\.example only/.test(await bar()), [KEPT, await bar()]);
  await wait(3200); await ev(t1, 'pwsave', {origin: O, user: 'ann', pw: 'Made-up-1'}); await wait(400);
  ok('4. the same password again: not asked', (await bar()) == '', await bar());
  await ev(t1, 'pwsave', {origin: O, user: 'ann', pw: 'Made-up-2'}); await wait(400);
  ok('5. a changed one: "Update your saved password"', /Update your saved password for shop\.example \(ann\)\?Update/.test(await bar()), await bar());
  await click('Update'); await wait(300); ok('...updated', KEPT.length == 1 && KEPT[0].pw == 'Made-up-2', KEPT);
  await ev(t1, 'pwsave', {origin: 'https://news.example', user: 'bob', pw: 'Made-up-3'}); await wait(400); await click('Never for this site'); await wait(200);
  await ev(t1, 'pwsave', {origin: 'https://news.example', user: 'bob', pw: 'Made-up-4'}); await wait(400);
  ok('6. Never for this site: not asked again for it, nothing kept', (await bar()) == '' && (await q("S.opwNever")).includes('https://news.example') && !KEPT.some(x => /news/.test(x.origin)), await bar());
  // ---- never in private, Tor or Guest windows; never odd origins
  CALLS = [];
  const tp = await open(O + '/login', 'priv'); await ev(tp, 'pwsave', {origin: O, user: 'ann', pw: 'Made-up-5'}); await ev(tp, 'pwfocus', {origin: O, newpw: false}); await wait(400);
  ok('7. a private window: never asked to save, never offered a fill (nothing asked of rudolfd)', (await bar()) == '' && CALLS.length == 0, [await bar(), CALLS]);
  const tt = await open(O + '/login', 'tor'); await ev(tt, 'pwsave', {origin: O, user: 'ann', pw: 'Made-up-5'}); await ev(tt, 'pwfocus', {origin: O, newpw: false}); await wait(400);
  ok('8. a Tor window: the same', (await bar()) == '' && CALLS.length == 0, [await bar(), CALLS]);
  await q("(()=>{USER.r='guest';return 1})()"); const tg = await open(O + '/login'); await ev(tg, 'pwsave', {origin: O, user: 'ann', pw: 'Made-up-5'}); await ev(tg, 'pwfocus', {origin: O, newpw: false}); await wait(400);
  ok('9. Guest: the same', (await bar()) == '' && CALLS.length == 0, [await bar(), CALLS]); await q("(()=>{USER.r='admin';return 1})()");
  const t2 = await open(O + '/login');
  for (const o of ['http://shop.example', 'https://shop.example/path', 'https://user@shop.example', 'javascript:alert(1)', 'https://-x.example']) await ev(t2, 'pwsave', {origin: o, user: 'ann', pw: 'Made-up-6'});
  await wait(400);
  ok('10. never for an http: or odd origin (a path, a user, another scheme)', (await bar()) == '' && !CALLS.some(c => c[0] == 'oracle/pw/set'), [await bar(), CALLS]);
  await ev(t2, 'pwsave', {origin: 'https://xn--shp-tma.example', user: 'ann', pw: 'Made-up-7'}); await wait(400);
  ok('...a look-alike host (letters of other alphabets, xn--) is said in the bar', /letters of other alphabets \(xn--shp-tma\.example\)/.test(await bar()), await bar());
  await click('Not now'); await wait(100);
  // ---- filling
  await q("(()=>{__sent.length=0;return 1})()"); CALLS = [];
  await ev(t2, 'pwfocus', {origin: O, newpw: false}); await wait(400);
  ok('11. a password field with the keyboard: "Sign in to shop.example as [ann]" and Fill', /Sign in to shop\.example as.*annFill/.test(await bar()), await bar());
  ok('12. nothing goes to the page before Fill is clicked (no password asked of rudolfd either)', !(await q("__sent.some(m=>m.op=='pwfill')")) && !CALLS.some(c => c[0] == 'oracle/pw/get'), CALLS);
  await click('Fill'); await wait(400);
  const f = await q("__sent.filter(m=>m.op=='pwfill')");
  ok('13. Fill: that origin\'s password for ann, to that tab only', f.length == 1 && f[0].tid == t2 && f[0].origin == O && f[0].user == 'ann' && f[0].pw == 'Made-up-2' && !f[0].gen && CALLS.find(c => c[0] == 'oracle/pw/get')[1] == O + '\nann', f);
  await ev(t2, 'pwfilled', {id: f[0].id, r: 'ok'}); await wait(200); ok('...filled: the bar goes', (await bar()) == '');
  await ev(t2, 'pwfocus', {origin: 'https://other.example', newpw: false}); await wait(400);
  ok('14. a site with nothing saved (and no new password): no bar', (await bar()) == '', await bar());
  await ev(t2, 'pwfocus', {origin: O, newpw: true}); await wait(400); await click('Use a strong password'); await wait(200);
  const gen = await q("(document.querySelector('.win.act .opwbar .opwgen')||{}).textContent||''");
  ok('15. a new password: a strong one shown first (20 characters, both cases, digits and signs)', gen.length == 20 && /[A-Z]/.test(gen) && /[a-z]/.test(gen) && /[0-9]/.test(gen) && /[-_.!?@#%+=]/.test(gen) && !(await q("__sent.some(m=>m.op=='pwfill'&&m.gen)")), gen);
  await click('Use it'); await wait(200);
  const g2 = await q("__sent.filter(m=>m.op=='pwfill'&&m.gen)");
  ok('...then filled as the new password, into that tab', g2.length == 1 && g2[0].pw == gen && g2[0].origin == O && g2[0].tid == t2, g2);
  await ev(t2, 'pwfilled', {id: g2[0].id, r: 'origin'}); await wait(200);
  ok('...a page gone to another site meanwhile: nothing filled, said', /no longer that site: nothing was filled/.test(await bar()), await bar());
  // ---- ☰ › Passwords
  const menu = await q("(()=>{const W=document.querySelector('.win.act');const m=[...W.querySelectorAll('.mn')].find(b=>b.textContent.trim()=='☰');if(!m)return ['no menu button',W.className];m.click();const d=document.querySelector('.odd');const r=d?[...d.children].map(x=>x.textContent):['no menu',m.outerHTML.slice(0,200),String(m.onclick).slice(0,80)];d&&d.remove();return r})()");
  ok('16. ☰ has Passwords', menu.includes('Passwords'), menu);
  await q("(()=>{opwWindow();return 1})()"); await wait(500);
  const rows = await q("[...document.querySelectorAll('.opwwin .opwl .opwrow')].map(r=>r.textContent)");
  ok('17. ☰ › Passwords: the sites and user names, the passwords hidden', rows.length == 1 && /^shop\.exampleann••••••••ShowCopyChangeRemove$/.test(rows[0]), rows);
  await q("(()=>{pwCheck=async(a,p)=>p==='Ann-made-up-account-pw';return 1})()");
  await q("(()=>{[...document.querySelectorAll('.opwwin button')].find(b=>b.textContent=='Show').click();return 1})()"); await wait(300);
  await q("(()=>{const i=document.querySelector('.rmodal .ep');i.value='wrong';i.dispatchEvent(new Event('input'));document.querySelector('.rmodal .ey').click();return 1})()"); await wait(300);
  ok('18. Show asks for the account\'s password: a wrong one refused, nothing shown', /not the password/.test(await q("document.querySelector('.rmodal .rmmsg').textContent")) && (await q("document.querySelector('.opwwin .opwpw').textContent")) == '••••••••');
  await q("(()=>{document.querySelector('.rmodal').remove();[...document.querySelectorAll('.opwwin button')].find(b=>b.textContent=='Show').click();return 1})()"); await wait(300);
  await p.focus('.rmodal .ep'); await p.keyboard.type('Ann-made-up-account-pw'); await q("(()=>{document.querySelector('.rmodal .ey').click();return 1})()"); await wait(500);
  ok('...the right one: shown', (await q("document.querySelector('.opwwin .opwpw').textContent")) == 'Made-up-2', await q("document.querySelector('.opwwin .opwpw').textContent"));
  await q("(()=>{window.confirm=()=>true;[...document.querySelectorAll('.opwwin .opwl button')].find(b=>b.textContent=='Remove').click();return 1})()"); await wait(500);
  ok('19. Remove: gone from the keyring and the list; the Never list shows news.example', KEPT.length == 0 && (await q("document.querySelectorAll('.opwwin .opwl .opwrow').length")) == 0 && /news\.example/.test(await q("document.querySelector('.opwwin .opwn').textContent")));
  ok('20. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  const fl = R.filter(x => !x).length; console.log(fl ? `FAIL ${fl} of ${R.length}` : `PASS ${R.length}/${R.length}`); process.exit(fl ? 1 : 0);
})().catch(e => { console.log('FAIL ' + e.stack); process.exit(1) });
