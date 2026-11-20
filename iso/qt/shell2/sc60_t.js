// 1.6.0 (plan-1.6.0 Part 20): smart cards in Settings with a stand-in rudolfd and a test PKI (iso/qt/host/mkscpki.py):
// the card's certificates read and their expiry, the DoD bundle check (pinned roots, each link's signature: RSA and
// EC; another root, its child and a forgery left out), the install request, and locking when the card is taken out.
//   node sc60_t.js PKI_DIR [CHROME]   (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www'), PKI = process.argv[2];
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 500) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8147, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
const B = f => fs.readFileSync(path.join(PKI, f)), EXP = JSON.parse(B('expect.json')), PINS = JSON.parse(B('pins.json'));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[3] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.addInitScript(([card, bundle]) => {
    window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0};
    window.T = {posts: [], card: true, dod: []};
    const f0 = window.fetch;
    window.fetch = (u, o) => { u = String(u); const q = new URL(u, location.href), J = x => Promise.resolve(new Response(JSON.stringify(Object.assign({ok: true}, x)), {headers: {'content-type': 'application/json'}}));
      if (q.pathname == '/api/sc/status') return J({pcscd: true, module: true, readers: [{name: 'Test CAC reader 00 00', card: T.card, label: T.card ? 'DOE.JOHN.Q.1234567890' : '', maker: 'piv_II', certs: T.card ? [{label: 'Certificate for PIV Authentication', id: '01', der: card}] : []}]});
      if (q.pathname == '/api/sc/dod/list') return J({certs: T.dod});
      if (q.pathname == '/api/sc/dod/fetch') { T.posts.push(['fetch', JSON.parse(o.body)]); return J({url: 'https://dl.dod.cyber.mil/x.zip', hex: bundle}) }
      if (q.pathname == '/api/sc/dod/install') { const bd = JSON.parse(o.body); T.posts.push(['install', bd]); T.dod = bd.certs.split('\n').map((l, i) => 'Rudolf DoD ' + i); return J({installed: T.dod.length}) }
      if (q.pathname == '/api/sc/dod/remove') { T.posts.push(['remove']); T.dod = []; return J({}) }
      if (u.includes('/api/')) return J({});
      return f0(u, o) } }, [B('card.der').toString('base64'), B('bundle.p7b').toString('hex')]);
  await p.goto('http://127.0.0.1:8147/'); await p.waitForFunction(() => typeof scDodCheck == 'function' && IDB && IDB.db, null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    ACCTS.length=0;ACCTS.push({n:'boss',h:await pwMake('b1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'b1');
    window.elevate=async()=>({n:'boss',p:'b1'});window.LOCKED=0;window.lockScreen=()=>{LOCKED++};Object.assign(SC_DOD_ROOTS,${JSON.stringify(PINS)});return 1})()`);
  // the check itself
  const k = await q(`(async()=>{const r=await scDodCheck(scUnhex(${JSON.stringify(B('bundle.p7b').toString('hex'))}),${JSON.stringify(PINS)});return{roots:r.roots.map(x=>x.cn),cas:r.cas.map(x=>x.cn).sort(),left:r.left.map(x=>x.cn).sort()}})()`);
  ok('1. the bundle\'s pinned root kept', JSON.stringify(k.roots) == JSON.stringify(EXP.roots), k);
  ok('2. ...everything it signs, link by link (RSA, and EC P-384 and P-256)', JSON.stringify(k.cas) == JSON.stringify(EXP.cas), k);
  ok('3. another root, its child, and a forgery claiming the DoD root\'s name: left out', JSON.stringify(k.left) == JSON.stringify(EXP.left), k);
  const none = await q(`(async()=>{const r=await scDodCheck(scUnhex(${JSON.stringify(B('bundle.p7b').toString('hex'))}),{});return r.roots.length+r.cas.length})()`);
  ok('4. without the pinned thumbprint nothing is kept', none === 0, none);
  // Settings
  await q("settings('sec')"); await wait(1500);
  const ui = await q("(()=>{const b=document.querySelector('.sc60');return b?{readers:b.querySelector('.scr').textContent,dod:b.querySelector('.scd').textContent,btn:b.querySelector('.scdi').disabled,text:b.textContent}:null})()");
  ok('5. Security Settings › Smart cards: the reader, the card, its certificate\'s name and expiry, "Expires in 10 days"', ui && /Test CAC reader/.test(ui.readers) && /DOE\.JOHN\.Q\.1234567890, until/.test(ui.readers) && /Expires in 1[01] days/.test(ui.readers), ui);
  ok('6. says the PIN is never seen, not in Tor windows, and the DoD does not support Linux', ui && /never sees the PIN/.test(ui.text) && /Not used in Tor windows/.test(ui.text) && /does not support Linux/.test(ui.text) && /Not installed/.test(ui.dod), ui);
  await q("document.querySelector('.sc60 .scdi').click()"); await wait(2500);
  const posts = await q('T.posts'); const inst = posts.find(x => x[0] == 'install');
  const lines = inst ? inst[1].certs.split('\n') : [];
  ok('7. installing: fetched and installed with the administrator\'s name, 1 root and 3 below it, nothing else', posts[0][0] == 'fetch' && posts[0][1].n == 'boss' && inst && lines.filter(l => /^root [0-9a-f]+$/.test(l)).length == 1 && lines.filter(l => /^ca [0-9a-f]+$/.test(l)).length == 3 && lines.length == 4, posts.map(x => x[0]));
  const nt = await q("NC.slice(0,3).map(n=>n.t+': '+n.b)");
  ok('8. ...and said: how many, which root, and 3 left out', nt.some(n => /DoD certificates: 4 installed \(Test DoD Root CA 9 and 3 below them\); 3 left out/.test(n)), nt);
  ok('9. ...Settings shows them installed', /Installed \(4 certificates\)/.test(await q("document.querySelector('.sc60 .scd').textContent")));
  // lock on removal
  await q("document.querySelector('.sc60 .scl').click()"); await wait(3500);
  await q('T.card=false'); await wait(3600);
  ok('10. "Lock the screen when my card is taken out": the card out, the screen locked', await q('LOCKED') == 1, await q('LOCKED'));
  await wait(3500); ok('11. ...once', await q('LOCKED') == 1);
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
