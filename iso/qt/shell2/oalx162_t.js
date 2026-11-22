// 1.6.2 (docs/plan-1.6.2.md Part 14, the owner's Vivobook on 2026-11-22: "oa: lx:com.valvesoftware.Steam asked of rudolfd 9840 ms after its
// page began", and do-bg's "desktop/apps given after 191721 ms"): the native desktop's page for a Linux app (#app=lx:ID) and the hidden
// page's catalogue (#app=do-bg), with a stand-in rudolfd whose listings take as long as a slow computer's (pkg/status, apps/installed
// with 40 Flatpak apps, debian/status, debian/apps; each icon). Measured: how long after the page began apps/run is asked; how long the
// catalogue takes; whether the app still opens when the saved list does not have it yet, and when an icon never comes.
//   node oalx162_t.js [CHROME]       (OALX_PORT, default 8612; OALX_WWW: another build's www folder; OALX_SLOW=1: each listing 2-3 s, as below; OALX_JSON=FILE: the numbers)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = process.env.OALX_WWW || path.join(__dirname, '../../../iso/www'), PORT = +process.env.OALX_PORT || 8612;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 600) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.mjs': 'text/javascript', '.html': 'text/html', '.webp': 'image/webp', '.jpg': 'image/jpeg', '.png': 'image/png', '.woff2': 'font/woff2', '.woff': 'font/woff', '.gz': 'application/gzip'};
// (the listings' times: as long as the Vivobook's could be with Steam, Discord and dozens of apps, while Steam starts)
const DL = {'pkg/status': 2500, 'apps/installed': 3000, 'debian/status': 1500, 'debian/apps': 2500, 'apps/icon': 400};
const APPS = [{id: 'com.valvesoftware.Steam', name: 'Steam'}, {id: 'com.discordapp.Discord', name: 'Discord'}];
for (let i = 0; i < 38; i++) APPS.push({id: 'org.example.App' + i, name: 'Example App ' + i});
const INST = {ok: true, apps: APPS.map(a => ({id: a.id, name: a.name, version: '1.0', size: '1 MB', origin: 'flathub', launchers: [{desktop: a.id, name: a.name, icon: a.id, comment: '', mime: '', wmclass: '', cats: 'Game;'}]}))};
const PNG = Buffer.from('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8DwHwAFBQIAX8jx0gAAAABJRU5ErkJggg==', 'base64');
const M = {calls: [], session: null, t: {}, noIcon: false};
const LS = "window.__rudolfLsFlush=()=>fetch('/api/lsync',{method:'POST',headers:{'X-Rudolf-Token':'t'},body:'{}'});";
const answer = (u, s) => { const j = o => { s.writeHead(200, {'content-type': 'application/json'}); s.end(JSON.stringify(o)) };
  if (u == 'pkg/status') return j({ok: true, installed: [], available: [], sources: []});
  if (u == 'apps/installed') return j(INST);
  if (u == 'debian/status') return j({ok: true, installed: false});
  if (u == 'debian/apps') return j({ok: true, apps: []});
  if (u == 'apps/icon') { if (M.noIcon) { s.writeHead(404, {'content-type': 'application/json'}); return s.end('{"ok":false,"error":"no icon"}') } s.writeHead(200, {'content-type': 'image/png'}); return s.end(PNG) }
  if (u == 'apps/run') { M.t.run = Date.now(); return j({ok: true}) }
  return j({ok: true}) };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  const bs = []; q.on('data', d => bs.push(d)); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0' + (M.engine ? ',engine:"' + M.engine + '"' : '') + '};' + LS + '</script>')) }
    if (u.startsWith('/api/')) { const a = u.slice(5); M.calls.push(q.method + ' ' + a + (qs ? '?' + qs : ''));
      if (a == 'events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      if (a == 'desktop/apps' && q.method == 'POST') { M.t.cat = Date.now(); M.cat = Buffer.concat(bs).toString() }
      if (a == 'desktop/session') { if (!M.session) { s.writeHead(404, {'content-type': 'application/json'}); return s.end('{"ok":false,"error":"no native session"}') }
        s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(M.session)) }
      return setTimeout(() => answer(a, s), DL[a] || 0) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(PORT, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
const URL0 = 'http://127.0.0.1:' + PORT + '/';
const NAT = "window.webkit={messageHandlers:{rudolfNative:{postMessage:m=>{(window.__msgs=window.__msgs||[]).push(JSON.parse(JSON.stringify(m)))}}}};";
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const ctx = await b.newContext({viewport: {width: 1100, height: 760}});
  const errs = []; const watch = p => p.on('pageerror', e => { if (/\boa[A-Z]|oneapp|ONEAPP|lxRegister|lxInstalled/.test(e.stack || '')) errs.push(e.message) });
  let p = await ctx.newPage(); watch(p);
  await p.goto(URL0); await p.waitForFunction(() => typeof oaOpen == 'function' && typeof acctNew == 'function', null, {timeout: 30000});
  const acct = await p.evaluate(`(async()=>{svj('bd95setup',1);['oobe'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    const a=await acctNew('Ann','pw-one','admin');ACCTS=[a];window.__sys=1;saveUser();USER=a;await userLogin(a,'pw-one');fsSave();fsFlush();await new Promise(r=>setTimeout(r,800));
    const hex=u=>[...new Uint8Array(u)].map(x=>x.toString(16).padStart(2,'0')).join('');
    return {id:a.id,pk:hex(await pbkBits('pw-one',a.ks,256)),fk:hex(FKRAW)}})()`);
  ok('0. the account was made and signed in once', acct.fk && acct.fk.length == 64, acct);
  // (the desktop page closed: on Native no Standard page shares the browser's connections; the storage set from a plain file's page)
  await p.close(); p = await ctx.newPage(); await p.goto(URL0 + 'favicon.png');
  M.session = {ok: true, id: acct.id, n: 'Ann', r: 'admin', pk: acct.pk, fk: acct.fk}; M.engine = 'native';
  const out = {};
  // the Linux app's page: how long after the page began apps/run is asked (the list known from before: the saved list, as on the Vivobook)
  const lxOpen = async (label, seed) => { await p.evaluate(s => { if (s) localStorage.setItem('bd95lx', JSON.stringify(s)); else localStorage.removeItem('bd95lx') }, seed);
    M.t = {}; const pg = await ctx.newPage(); watch(pg); await pg.addInitScript(NAT); const t0 = Date.now();
    await pg.goto(URL0 + '#app=lx:com.valvesoftware.Steam'); for (let i = 0; i < 400 && !M.t.run; i++) await wait(50);
    const ms = M.t.run ? M.t.run - t0 : null; const log = await pg.evaluate("performance.now()|0"); await pg.close(); out[label] = ms; return ms };
  const seed = INST.apps.map(a => ({id: a.id, name: a.name, launchers: a.launchers}));
  const known = await lxOpen('lxKnown', seed);
  ok(`1. a Linux app known from the saved list: apps/run asked ${known} ms after its page began (under 2 s: the page does not wait for the listings, ${DL['pkg/status'] + DL['apps/installed'] + DL['debian/status'] + DL['debian/apps']} ms here)`, known != null && known < 2000, known);
  const fresh = await lxOpen('lxNew', null);
  ok(`2. a Linux app not in the saved list yet (just installed): still opened once rudolfd lists it (${fresh} ms)`, fresh != null && fresh < 12000, fresh);
  // the hidden page's catalogue (#app=do-bg): how long desktop/apps takes, icons answered, then icons never coming
  await p.evaluate(s => localStorage.setItem('bd95lx', JSON.stringify(s)), seed);
  const cat = async (label, noIcon) => { M.noIcon = noIcon; M.t = {}; M.cat = null; const pg = await ctx.newPage(); watch(pg); await pg.addInitScript(NAT); const t0 = Date.now();
    await pg.goto(URL0 + '#app=do-bg'); for (let i = 0; i < 2400 && !M.t.cat; i++) await wait(50);
    const ms = M.t.cat ? M.t.cat - t0 : null; let c = null; try { c = JSON.parse(M.cat) } catch (e) {} await pg.close(); out[label] = ms; return [ms, c] };
  let [c1, j1] = await cat('catIcons', false);
  ok(`3. do-bg's catalogue (desktop/apps) given ${c1} ms after its page began, with the Linux apps and their pictures`, c1 != null && c1 < 20000 && j1 && j1.apps.some(a => a.k == 'lx:com.valvesoftware.Steam' && a.ic), [c1, j1 && j1.apps.filter(a => /^lx:/.test(a.k)).length]);
  let [c2, j2] = await cat('catNoIcons', true);
  ok(`4. ...and when no Linux app's icon can be had: given ${c2} ms after (not minutes: each picture waited for once, at most a few seconds)`, c2 != null && c2 < 25000 && j2 && j2.apps.some(a => a.k == 'lx:com.valvesoftware.Steam'), c2);
  ok('5. no page errors', errs.length == 0, errs);
  console.log('NUMBERS ' + JSON.stringify(out)); if (process.env.OALX_JSON) fs.writeFileSync(process.env.OALX_JSON, JSON.stringify(out));
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
