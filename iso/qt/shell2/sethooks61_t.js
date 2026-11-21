// 1.6.1: the Settings pages and applySet/themeApply built from one ordered list of additions (desktop/hooks61.js)
// instead of a wrapper around a wrapper. Every Settings page opened on Standard and in Native's app window (the page's
// one-app mode, #app=settings), and the settings applied with several choices; what they make is written down.
//   node sethooks61_t.js [CHROME]                      the checks on this tree's page
//   node sethooks61_t.js --compare OLD.html [CHROME]   also: the same done with an older build of the page (OLD.html,
//                                                      and its a/ folder beside it as OLD-a/), and both must be the same
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const argv = process.argv.slice(2), ci = argv.indexOf('--compare'), OLD = ci >= 0 ? argv[ci + 1] : null;
const CH = argv.find((a, i) => !a.startsWith('--') && (ci < 0 || i != ci + 1));
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 900) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.html': 'text/html', '.webp': 'image/webp', '.jpg': 'image/jpeg', '.woff2': 'font/woff2', '.woff': 'font/woff'};
const PORT = +process.env.SETHOOKS61_PORT || 8640;
let PAGE = path.join(WWW, 'index.html'), ADIR = path.join(WWW, 'a'); const M = {session: null};
const srv = http.createServer((q, s) => { const [u0] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(PAGE, 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0' + (M.engine ? ',engine:"' + M.engine + '"' : '') + '};</script>')) }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      if (u == '/api/desktop/session') { s.writeHead(M.session ? 200 : 404, {'content-type': 'application/json'}); return s.end(JSON.stringify(M.session || {ok: false})) }
      s.writeHead(200, {'content-type': 'application/json'}); return s.end('{"ok":true}') }
    const f = u.startsWith('/a/') ? path.join(ADIR, path.normalize(u.slice(3))) : path.join(WWW, path.normalize(u));
    if (!fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(PORT, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
const URL0 = 'http://127.0.0.1:' + PORT + '/';
// the same random numbers and the same clock each time, so two builds can be compared
const FIX = `(()=>{let x=12345;Math.random=()=>{x=(x*1103515245+12345)%2147483648;return x/2147483648};
  const D=Date,T0=D.UTC(2026,10,21,15,0,0),p0=performance.now();
  class FD extends D{constructor(...a){a.length?super(...a):super(T0+Math.floor((performance.now()-p0)/60000)*60000)}static now(){return T0+Math.floor((performance.now()-p0)/60000)*60000}}
  window.Date=FD})()`;
// what a page shows, without what changes by itself (the clock, counters, sizes measured from the browser)
const norm = h => h.replace(/\b\d{1,2}:\d{2}(:\d{2})?( ?[AP]M)?\b/g, 'T').replace(/\b\d+(\.\d+)? ?(ms|s|MB|KB|GB|kB|B|%|fps|px)\b/g, 'N$2')
  .replace(/ style="[^"]*"/g, m => m.replace(/\d+(\.\d+)?px/g, 'Npx'));
async function snap(page, native) {
  PAGE = page.html; ADIR = page.a;
  const b = await chromium.launch({executablePath: CH || undefined}); const ctx = await b.newContext({viewport: {width: 1280, height: 900}});
  await ctx.addInitScript(FIX);
  const out = {pages: {}, applied: [], errs: [], hooks: {}};
  const watch = p => p.on('pageerror', e => out.errs.push(e.message + ' ' + ((e.stack || '').split('\n')[1] || '').trim().replace(/:\d+:\d+\)?$/, '')));   // (without the line: the builds differ in length)
  let p = await ctx.newPage(); watch(p);
  const warns = []; p.on('console', m => { if (m.type() == 'warning' && /^\[\w+\.js\]/.test(m.text())) warns.push(m.text()) });
  M.engine = native ? 'native' : null; M.session = null;
  await p.goto(URL0); await p.waitForFunction(() => typeof settings == 'function' && typeof acctNew == 'function', null, {timeout: 30000});
  const acct = await p.evaluate(`(async()=>{svj('bd95setup',1);['oobe'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    const a=await acctNew('Ann','pw-one','admin');ACCTS=[a];window.__sys=1;saveUser();USER=a;await userLogin(a,'pw-one');
    await new Promise(r=>setTimeout(r,800));const hex=u=>[...new Uint8Array(u)].map(x=>x.toString(16).padStart(2,'0')).join('');
    return {id:a.id,pk:hex(await pbkBits('pw-one',a.ks,256)),fk:hex(FKRAW)}})()`);
  if (native) { await p.close(); M.session = {ok: true, id: acct.id, n: 'Ann', r: 'admin', pk: acct.pk, fk: acct.fk};
    p = await ctx.newPage(); watch(p); p.on('console', m => { if (m.type() == 'warning' && /^\[\w+\.js\]/.test(m.text())) warns.push(m.text()) });
    await p.goto(URL0 + '#app=settings'); await p.waitForFunction(() => typeof SW == 'object' && SW && SW.SEC, null, {timeout: 30000}) }
  else { await p.evaluate('settings()'); await wait(800) }
  for (const n of ['dispPanelS', 'usrPanelS', 'appsPanel', 'secPanelS', 'volPanelS', 'netPanelS', 'storPanel', 'updPanel', 'sysPanelS', 'accPanelS', 'healthPanel', 'applySet', 'themeApply'])
    out.hooks[n] = await p.evaluate(n => { const L = []; let f = window[n]; while (f) { if (f.__hooks) L.unshift(f.__hooks.map(h => h[1])); f = f.__inner } return L }, n);
  const keys = await p.evaluate('Object.keys(SW.SEC)');
  for (const k of keys) { await p.evaluate(`SW.show('${k}',1)`); await wait(1500);
    out.pages[k] = norm(await p.evaluate("(()=>{const b=SW.body.querySelector('.sc');return b?b.innerHTML:''})()")).split(acct.id).join('ACCT') }
  // applySet and themeApply with several choices: what they leave on the page
  const state = `(()=>{const a=e=>e?[...e.attributes].map(x=>x.name+'='+x.value).sort().join(' '):'';
    const st=[...document.querySelectorAll('style')].map(s=>(s.id||'-')+':'+s.textContent.length+':'+s.textContent.slice(0,200));
    const cs=getComputedStyle(document.documentElement),vars=[...new Set([...document.styleSheets].flatMap(sh=>{try{return [...sh.cssRules]}catch(e){return []}}).flatMap(r=>r.style?[...r.style].filter(x=>x.startsWith('--')):[]))].sort().map(v=>v+'='+cs.getPropertyValue(v).trim());
    const ls=Object.keys(localStorage).sort().filter(k=>!/time|stamp|last|seen|boot/i.test(k)).map(k=>/^bd95user$/.test(k)?k:k+'='+String(localStorage[k]).length);   /* (the account's record: its random salt's length varies) */
    return {html:a(document.documentElement),body:a(document.body),styles:st,vars,ls}})()`;
  const choices = ["S.dm=1", "S.dm=0", "S.accent='#16a34a'", "S.tablet='on'", "S.tablet='off'", "S.tbbstyle='retro'", "S.tbbstyle='standard'",
    "S.aur=1", "S.aur=0", "S.hc=1", "S.hc=0", "S.fs=1.25", "S.fs=1"];
  for (const c of choices) { await p.evaluate(`(()=>{try{${c}}catch(e){};try{applySet()}catch(e){};try{themeApply()}catch(e){}})()`); await wait(400);
    const s = await p.evaluate(state); out.applied.push({c, s: JSON.parse(norm(JSON.stringify(s)).split(acct.id).join('ACCT').split(acct.id.toLowerCase()).join('ACCT'))}) }   // (the account's id is new each time)
  out.warns = warns; await b.close(); return out
}
const diff = (a, b) => { const r = []; for (const k of new Set([...Object.keys(a), ...Object.keys(b)])) if (a[k] !== b[k]) {
  const x = a[k] || '', y = b[k] || ''; let i = 0; while (i < x.length && x[i] == y[i]) i++;
  r.push(k + ' @' + i + ': OLD …' + x.slice(Math.max(0, i - 80), i + 160) + '… NEW …' + y.slice(Math.max(0, i - 80), i + 160) + '…') } return r };
(async () => {
  const cur = {html: path.join(WWW, 'index.html'), a: path.join(WWW, 'a')};
  for (const native of [false, true]) { const nm = native ? 'Native' : 'Standard';
    const now = await snap(cur, native);
    const keys = Object.keys(now.pages);
    ok(`${nm} 1. every Settings page shows something (${keys.length} pages)`, keys.length >= 20 && keys.every(k => now.pages[k].length > 50), keys.filter(k => now.pages[k].length <= 50));
    ok(`${nm} 2. Display Properties' additions are one list, in the order the modules load`, JSON.stringify(now.hooks.dispPanelS.flat()) == JSON.stringify(['settings25.js', 'native25.js', 'settings31.js', 'saver31.js', 'wallpaper31.js', 'accent56.js', 'tablet58.js', 'gpudrv61.js', 'lock58.js', 'aurora58.js', 'statue58.js', 'titlebar59.js', 'tbbtn59.js', 'mon61.js', 'litepoint60.js', 'tbprog61.js'].filter(m => !native || now.hooks.dispPanelS.flat().includes(m))) && now.hooks.dispPanelS.length == 1, now.hooks.dispPanelS);
    ok(`${nm} 3. no addition failed (no warnings from the lists) and no page error from Settings`, !now.warns.length && !now.errs.filter(e => /Panel|settings|__after|hooks61/.test(e)).length, {warns: now.warns, errs: now.errs.slice(0, 8)});
    if (OLD) { const old = await snap({html: OLD, a: OLD.replace(/\.html$/, '-a')}, native);
      const dp = diff(old.pages, now.pages);
      ok(`${nm} 4. every Settings page is the same as the older build's (${keys.length} pages)`, !dp.length, dp);
      const da = []; old.applied.forEach((o, i) => { const n = now.applied[i]; if (JSON.stringify(o.s) != JSON.stringify(n.s)) da.push(o.c + ': ' + diff(Object.fromEntries(Object.entries(o.s).map(([k, v]) => [k, JSON.stringify(v)])), Object.fromEntries(Object.entries(n.s).map(([k, v]) => [k, JSON.stringify(v)]))).join(' | ')) });
      ok(`${nm} 5. applying ${old.applied.length} choices leaves the page as the older build does (theme, accent, tablet, taskbar buttons, aurora, contrast, text size)`, !da.length, da);
      ok(`${nm} 6. no page error the older build did not have`, now.errs.filter(e => !old.errs.includes(e)).length == 0, now.errs.filter(e => !old.errs.includes(e))) }
  }
  srv.close(); const f = R.filter(x => !x).length; console.log(f ? f + ' FAILED of ' + R.length : 'all ' + R.length + ' passed'); process.exit(f ? 1 : 0)
})().catch(e => { console.log('FAIL ' + e.stack); process.exit(1) });
