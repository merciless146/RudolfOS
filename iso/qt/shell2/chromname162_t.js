// 1.6.2 (docs/plan-1.6.2.md, Names): the Standard desktop is called Chromium, and the desktop engine's choices read
// Native, Chromium, Lite in that order: on the Chromium desktop's page and in Native's app window (index.html#app=settings,
// the same page alone), with a stand-in rudolfd. A computer whose saved choice is the old value (no
// /persist/etc/rudolf-desktop-engine: rudolfd says "chromium") keeps starting the same desktop: the value is unchanged,
// Settings shows it as "Chromium", nothing is saved by opening Settings, and System health's Last starts, the Help pages
// and the Terminal's rudolf-fetch say Chromium. (Help's What's new keeps the old name in released versions' entries.)
//   node chromname162_t.js [CHROME]   (build iso/www first; port 8610, or CHROMNAME_PORT)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path'), os = require('os'), cp = require('child_process');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 700) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.mjs': 'text/javascript', '.html': 'text/html', '.webp': 'image/webp', '.jpg': 'image/jpeg', '.png': 'image/png', '.woff2': 'font/woff2', '.json': 'application/json'};
const PORT = +process.env.CHROMNAME_PORT || 8610, URL0 = 'http://127.0.0.1:' + PORT + '/';
const M = {calls: [], session: null, eng: {choice: 'chromium', now: 'chromium'}};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    const J = (x, st) => { s.writeHead(st || 200, {'content-type': 'application/json'}); s.end(JSON.stringify(x)) };
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }
    if (u.startsWith('/api/')) { const a = u.slice(5); M.calls.push(q.method + ' ' + a + (qs ? '?' + qs : ''));
      if (a == 'events/wait') return setTimeout(() => J({ok: true, events: []}), 20000);
      if (a == 'desktop/session') return M.session ? J(M.session) : J({ok: false, error: 'no native session'}, 404);
      if (a == 'desktop/engine') { if (q.method == 'POST') { const e = new URLSearchParams(qs || '').get('engine'); if (e) M.eng.choice = e; return J({ok: true, message: 'saved'}) }
        return J(Object.assign({ok: true, available: true, nativeAvailable: true, failed: false, persistent: true, gaveUp: '', offer: false}, M.eng)) }
      if (a == 'system/starts') return J({ok: true, starts: [{n: 0, time: '2026-11-22T15:00:00Z', slot: 'a', release: '2026112201', desktopUp: 14.2, engine: M.eng.now, exits: 1}]});
      if (a == 'info') return J({ok: true, live: false, media: 'disk', boot: 'uefi', recovery: false, bootSecure: 1});
      if (/^sys\//.test(a)) return J({ok: false, error: 'not in this test'}, 404);
      return J({ok: true, items: [], apps: [], windows: [], events: [], message: 'done'}) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(PORT, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
const PANEL = `(()=>{const d=document.querySelector('.eng57');if(!d)return null;const s=d.querySelector('#eng57');if(!s)return null;
  return {v:s.value,sel:s.options[s.selectedIndex].text,opts:[...s.options].map(o=>o.textContent),vals:[...s.options].map(o=>o.value),now:d.querySelector('.engnow').textContent,txt:d.innerText}})()`;
const noStd = t => !/Standard/.test(String(t).replace('(before 1.6.2 it was called Standard)', ''));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const ctx = await b.newContext({viewport: {width: 1200, height: 860}});
  const errs = []; const watch = p => p.on('pageerror', e => errs.push(e.message + ' @ ' + String(e.stack).split('\n').slice(1, 3).join(' ')));
  const open = async (engine, hash) => { const p = await ctx.newPage(); watch(p);
    await p.addInitScript(engine => { window.RUDOLFD = {token: 't', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0, engine} }, engine);
    await p.goto(URL0 + (hash || '')); await p.waitForFunction(() => typeof enginePanel57 == 'function' && typeof acctNew == 'function', null, {timeout: 30000}); return p };
  const until = async (p, js, ms = 15000) => { const t0 = Date.now(); let v; while (Date.now() - t0 < ms) { v = await p.evaluate(js); if (v) return v; await wait(250) } return v };

  // ---- the Chromium desktop (the page with its own windows), a computer that was on "Standard": the saved choice is the old value
  let p = await open('chromium');
  const acct = await p.evaluate(`(async()=>{svj('bd95setup',1);['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    const a=await acctNew('Ann','pw-one','admin');ACCTS=[a];window.__sys=1;saveUser();USER=a;await userLogin(a,'pw-one');window.__elevTrust=Date.now();fsSave();fsFlush();await new Promise(r=>setTimeout(r,800));
    const hex=u=>[...new Uint8Array(u)].map(x=>x.toString(16).padStart(2,'0')).join('');
    return {id:a.id,pk:hex(await pbkBits('pw-one',a.ks,256)),fk:hex(FKRAW)}})()`);
  M.session = {ok: true, id: acct.id, n: 'Ann', r: 'admin', pk: acct.pk, fk: acct.fk};
  M.calls.length = 0; await p.evaluate("settings('sys')"); let d = await until(p, PANEL);
  ok('C1. Chromium desktop: Settings › Desktop engine reads Native, Chromium, Lite, in that order', d && JSON.stringify(d.opts) == '["Native","Chromium","Lite (WebKit, test)"]', d);
  ok('C2. ...the saved values are the same as before (native, chromium, webkit)', d && JSON.stringify(d.vals) == '["native","chromium","webkit"]', d && d.vals);
  ok('C3. the old saved choice (rudolfd: "chromium") is selected and shown as "Chromium"; in use now: Chromium', d && d.v == 'chromium' && d.sel == 'Chromium' && /^In use now: Chromium\.$/.test(d.now), d);
  ok('C4. ...opening Settings saves nothing (the computer starts the same desktop as before)', !M.calls.some(c => /^POST desktop\/engine/.test(c)), M.calls.filter(c => /engine/.test(c)));
  ok('C5. ...no "Standard" in the panel (but the one note of its old name)', d && noStd(d.txt) && /before 1\.6\.2 it was called Standard/.test(d.txt), d && d.txt);
  ok('C6. ...the descriptions in the same order: Native, then Chromium, then Lite', d && d.txt.indexOf('Native draws') < d.txt.indexOf('Chromium runs') && d.txt.indexOf('Chromium runs') < d.txt.indexOf('Lite runs'), d && d.txt);
  // choosing Native, then Chromium again: the same requests as always
  await p.evaluate("(()=>{const s=document.querySelector('#eng57');s.value='native';s.dispatchEvent(new Event('change'));return 1})()"); await wait(1200);
  await p.evaluate("(()=>{const s=document.querySelector('#eng57');s.value='chromium';s.dispatchEvent(new Event('change'));return 1})()"); await wait(1200);
  d = await p.evaluate(PANEL); const posts = M.calls.filter(c => /^POST desktop\/engine\?engine=/.test(c));
  ok('C7. choosing Native, then Chromium: rudolfd is asked for engine=native, then engine=chromium (the same values as on 1.6.1)', JSON.stringify(posts) == '["POST desktop/engine?engine=native","POST desktop/engine?engine=chromium"]' && d.v == 'chromium', [posts, d && d.now]);
  M.eng.choice = 'native'; await p.evaluate("(()=>{document.querySelectorAll('.eng57').forEach(e=>e.remove());enginePanel57(document.querySelector('#sdp').closest('div').parentElement);return 1})()"); await wait(800);
  d = await p.evaluate(PANEL);
  ok('C8. Native chosen, Chromium in use: "In use now: Chromium. Native is used after the computer restarts."', d && /^In use now: Chromium\. Native is used after the computer restarts\./.test(d.now), d && d.now);
  M.eng.choice = 'chromium';
  // System health › Last starts, and the Help pages that name the safe desktop
  await p.evaluate("startsShow()"); const st = await until(p, "(()=>{const w=[...document.querySelectorAll('.win')].find(x=>/Last starts/.test(x.textContent));return w&&/desktop came up/.test(w.textContent)&&w.querySelector('.sysucard').textContent})()");
  ok('C9. System health › Last starts: a start on the old value shows "(Chromium)"', st && /The desktop came up after 14\.2 s \(Chromium\)/.test(st) && !/Standard/.test(st), st);
  const help = await p.evaluate("(()=>{const s=HELP.map(h=>{try{return String(h[2]())}catch(e){return ''}}).join(' ');return {n:HELP.length,keys:/Safe desktop: start the desktop again with the Chromium engine/.test(s),rescue:/the Chromium desktop engine, no shell, safe graphics/.test(s),news:HELP.some(h=>{try{return /the Standard desktop is now called Chromium/.test(String(h[2]()))}catch(e){return false}}),old:HELP.filter(h=>{try{return h[0]!='new'&&/standard (desktop )?engine|Standard/.test(String(h[2]()))}catch(e){return false}}).map(h=>h[0]+': '+(String(h[2]()).match(/.{0,80}(standard (desktop )?engine|Standard).{0,40}/)||[''])[0])}})()");
  ok('C10. Help (Keyboard shortcuts, Starting from a USB stick) says the Chromium engine, not the standard engine; What\'s new tells of the rename', help.keys && help.rescue && help.news && !help.old.length, help);
  await p.close();

  // ---- Native's app window: the same page alone, the session's account
  M.eng = {choice: 'chromium', now: 'native'};
  p = await open('native', '#app=settings'); await until(p, "typeof ONEAPP!='undefined'&&ONEAPP&&!!USER&&!!document.querySelector('.win.oamain')", 30000);
  await p.evaluate("SW.show('sys',1)"); d = await until(p, PANEL);
  ok("N1. Native's app window: the same choice reads Native, Chromium, Lite, in that order", d && JSON.stringify(d.opts) == '["Native","Chromium","Lite (WebKit, test)"]' && JSON.stringify(d.vals) == '["native","chromium","webkit"]', d);
  ok('N2. ...the old saved choice shown as "Chromium"; in use now: Native, Chromium after the restart', d && d.v == 'chromium' && d.sel == 'Chromium' && /^In use now: Native\. Chromium is used after the computer restarts\./.test(d.now), d);
  ok('N3. ...the same words as on the Chromium desktop, no "Standard"', d && noStd(d.txt) && /Chromium runs the desktop page in Rudolf's Chromium/.test(d.txt), d && d.txt);
  M.eng = {choice: 'native', now: 'native'}; await p.evaluate("(()=>{document.querySelectorAll('.eng57').forEach(e=>e.remove());enginePanel57(document.querySelector('.win.oamain .body')||document.body);return 1})()"); await wait(800);
  d = await p.evaluate(PANEL);
  ok('N4. Native chosen and in use: "Native" selected, "In use now: Native."', d && d.sel == 'Native' && /^In use now: Native\.$/.test(d.now), d);
  await p.close();
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close();

  // ---- the Terminal's rudolf-fetch (its facts file pointed at a temporary one): the old values say Chromium
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'chromname-')), sh = path.join(tmp, 'rudolf-fetch'), facts = path.join(tmp, 'facts');
  fs.writeFileSync(sh, fs.readFileSync(path.join(ROOT, 'iso/sys/root/usr/lib/rudolf/rudolf-fetch'), 'utf8').replace(/^F=.*$/m, 'F=' + facts).replace(/^D=.*$/m, 'D=' + tmp));
  fs.writeFileSync(path.join(tmp, 'logo.txt'), 'R\n'.repeat(14)); fs.writeFileSync(path.join(tmp, 'VERSION'), '1.6.2.0\n');
  const fetchSays = eng => { fs.writeFileSync(facts, 'engine=' + eng + '\nchannel=stable\n'); try { return (cp.execFileSync('sh', [sh, '--plain'], {encoding: 'utf8', timeout: 20000}).match(/Desktop: *(\S+)/) || [])[1] } catch (e) { return 'error ' + e.message } };
  const fr = {chromium: fetchSays('chromium'), standard: fetchSays('standard'), native: fetchSays('native'), webkit: fetchSays('webkit')};
  ok('F1. rudolf-fetch: the saved values chromium and standard say "Desktop: Chromium"; native and webkit as before', fr.chromium == 'Chromium' && fr.standard == 'Chromium' && fr.native == 'Native' && fr.webkit == 'Lite', fr);
  fs.rmSync(tmp, {recursive: true, force: true});

  // ---- what starts the desktop is unchanged: rudolfd still maps any other saved value to chromium, the session keys on native/webkit only
  const rdc = fs.readFileSync(path.join(ROOT, 'iso/sys/rudolfd.c'), 'utf8'), ses = fs.readFileSync(path.join(ROOT, 'iso/sys/root/usr/lib/rudolf/session'), 'utf8');
  ok('S1. rudolfd: a saved choice other than webkit or native is "chromium"; Chromium is saved as no file, as before',
    rdc.includes('const char *ch = c && !strcmp(c, "webkit") ? "webkit" : c && !strcmp(c, "native") ? "native" : "chromium";') && rdc.includes('if (!strcmp(e, "chromium")) unlink(PERSIST "/etc/rudolf-desktop-engine");'));
  ok('S2. the session starts Lite or Native only for "webkit" or "native" (anything else: the Chromium desktop)', /engine-choice 2>\/dev\/null\)" = webkit \] \|\| return 1/.test(ses) && /engine-choice 2>\/dev\/null\)" = native \] \|\| return 1/.test(ses));
  const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
