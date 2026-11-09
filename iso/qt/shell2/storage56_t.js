// 1.5.6 (1.5.5's plan items 4 and 5): Settings › Storage and backup says what fills the disk (rudolfd's storage, a
// stand-in here), largest first with what can be done about each; the Trash shows its size (large files at their size
// on the disk) and deletes items older than the account's setting, with one notification a day; another account is
// untouched.   node storage56_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 600) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8152, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd|debian/.test(e.stack || e.message || '')) errs.push(e.message) });
  await p.addInitScript(() => {
    window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0};
    const G = 1073741824, M = 1048576;
    window.T = {calls: [], n: 0, res: {ok: true, time: 0, took: 4, disk: {total: 125 * G, used: 23 * G, free: 102 * G},
      items: {files: 2 * M, trash: 4096, downloads: 650 * M, flatpak: 6 * G, steam: 8.6 * G, debian: 3 * G, games: M, ai: 1 * G, updates: 0.6 * G, rest: 3.5 * G},
      accounts: [{id: 'u-ann', files: 2 * M, trash: 4096, downloads: 640 * M}, {id: 'u-bob', files: 30 * M, trash: 0, downloads: 0}]}};
    const f0 = window.fetch;
    window.fetch = (u, o) => { u = String(u); const J = x => Promise.resolve(new Response(JSON.stringify(Object.assign({ok: true}, x)), {headers: {'content-type': 'application/json'}}));
      const q = new URL(u, location.href);
      if (u.includes('/api/')) T.calls.push(q.pathname.slice(5));
      // the first look: never measured; "measure" runs for two answers, then gives the result
      if (q.pathname == '/api/storage/measure') { T.n = 2; T.done = 1; return J({measuring: true, result: null}) }
      if (q.pathname == '/api/storage') { if (!T.done) return J({measuring: false, result: null}); if (T.n > 0) { T.n--; return J({measuring: true, result: null}) } T.res.time = Math.floor(Date.now() / 1000); return J({measuring: false, result: T.res}) }
      if (u.includes('/api/')) return J({items: [], apps: []});
      return f0(u, o) } });
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8152/'); await p.waitForFunction(() => window.__storage56 && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    ACCTS.length=0;ACCTS.push({n:'ann',h:await pwMake('a1'),r:'admin'});ACCTS.push({n:'bob',h:await pwMake('b1'),r:'user'});saveUser();
    window.IN=async n=>{if(USER)userLogout();USER=ACCTS.find(a=>a.n==n);await userLogin(USER,n[0]+'1');return 1};await IN('ann');if(typeof HD!='undefined')HD.acct='u-ann';return 1})()`);
  // the page
  await q("(()=>{wins.slice().forEach(w=>w.close());settings('stor');return 1})()");
  await p.waitForFunction(() => document.querySelectorAll('.sd56 .sdt tr').length > 0, null, {timeout: 15000}).catch(() => {});
  const rows = await q("[...document.querySelectorAll('.sd56 .sdt tr')].map(r=>({k:r.dataset.k,t:r.cells[2].textContent,a:r.cells[3].textContent}))");
  ok('1. "On this computer\'s disk" is measured when the page opens (it was never measured)', (await q("T.calls")).includes('storage/measure') && rows.length > 5, [await q("T.calls"), rows]);
  ok('2. largest first: Steam\'s games, Linux apps, the rest, Linux (Debian)...', JSON.stringify(rows.slice(0, 4).map(r => r.k)) == '["steam","flatpak","rest","debian"]', rows.map(r => r.k));
  ok('3. "Your files" is this account\'s (2 MB), and the other accounts\' are a line of their own (30 MB)', rows.some(r => r.k == 'files' && /^2(\.0+)? MB/.test(r.t)) && rows.some(r => r.k == 'others' && /30(\.0+)? MB/.test(r.t)), rows);
  ok('4. each line has its action: Downloads → Open, Linux apps → Manage, Debian → Manage', rows.some(r => r.k == 'downloads' && r.a == 'Open') && rows.some(r => r.k == 'flatpak' && r.a == 'Manage') && rows.some(r => r.k == 'debian' && r.a == 'Manage'), rows);
  const msg = await q("document.querySelector('.sd56 .sdm').textContent");
  ok('5. the disk: used, size and free', /23(\.0)? GB used of 125(\.0)? GB · 102(\.0)? GB free/.test(msg), msg);
  await q("T.calls.length=0;document.querySelector('.sd56 .sda').click()"); await wait(4500);
  ok('6. "Measure again" measures again', (await q("T.calls")).includes('storage/measure') && (await q("document.querySelectorAll('.sd56 .sdt tr').length")) > 5, await q("T.calls"));
  // the Trash: sizes, a large file at its size on the disk
  await q(`(()=>{const now=Date.now();TR.length=0;TR.push({n:'NOTE.TXT',from:homePath('DOCUMENTS'),v:'x'.repeat(2048),t:now-20*864e5},{n:'BIG.ISO',from:downloadsPath(),v:dlMkRef('/persist/accounts/u-ann/Downloads/BIG.ISO',734003200,'full'),t:now-2*864e5});trSave();return 1})()`);
  const tip = await q("(()=>{const d=document.querySelector('.ic.tr');return d?(d.dataset.t0||d.title):''})()");
  ok('7. the desktop\'s Trash says its size, with the large file at its size on the disk', /2 items in the Trash \(700(\.0+)? MB\)/.test(tip), tip);
  await q("(()=>{trash();return 1})()"); await wait(300);
  const st = await q("wins.at(-1).body.querySelector('.st').textContent");
  ok('8. ...and so does the Trash window', /2 item\(s\) · 700(\.0+)? MB/.test(st), st);
  // emptying old items: 14 days
  await q("(()=>{wins.slice().forEach(w=>w.close());settings('stor');return 1})()"); await wait(800);
  await q("(()=>{const s=document.querySelector('.tra56 .trad');s.value='14';s.onchange();return 1})()"); await wait(300);
  const left = await q("TR.map(x=>x.n)"), nt = await q("(__notifCentre.list?__notifCentre.list():[]).length");
  ok('9. set to 14 days: the item deleted 20 days ago goes, the one from 2 days ago stays', JSON.stringify(left) == '["BIG.ISO"]', left);
  const ns = await q("document.querySelector('.tra56 .tras').textContent");
  ok('10. the page says what happened', /1 old item was deleted/.test(ns), ns);
  // later: once a day at most for the notice
  await q(`(()=>{TR.push({n:'OLD2.TXT',from:homePath('DOCUMENTS'),v:'y',t:Date.now()-30*864e5});trSave();window.__nn=0;const o=notify;notify=function(t){if(/Old items/.test(t))__nn++;return o.apply(this,arguments)};trAutoEmpty();notify=o;return 1})()`);
  ok('11. the next deletion the same day removes the item but does not notify again', (await q("TR.map(x=>x.n).join()")) == 'BIG.ISO' && await q("__nn") == 0, [await q("TR.map(x=>x.n)"), await q("__nn")]);
  // another account
  await q("IN('bob')"); await wait(600);
  await q(`(()=>{TR.length=0;TR.push({n:'B.TXT',from:homePath('DOCUMENTS'),v:'b',t:Date.now()-90*864e5});trSave();return trAutoEmpty()})()`);
  ok('12. another account keeps its own setting (Never): its old item stays', (await q("TR.map(x=>x.n).join()")) == 'B.TXT' && await q("trAutoDays()") == 0);
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
