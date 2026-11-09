// 1.5.6 (1.5.5's plan item 6): File Explorer's "Everywhere": this folder first, then the rest of C: (not other
// accounts' folders), then plugged-in drives (rudolfd's fs/search, a stand-in here); filters by type and date;
// results stream in, 1,000 at a time with "Show more", and the search can be stopped.
//   node search56_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 600) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8153, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd|debian/.test(e.stack || e.message || '')) errs.push(e.message) });
  await p.addInitScript(() => {
    window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0};
    window.T = {calls: []};
    const f0 = window.fetch;
    window.fetch = (u, o) => { u = String(u); const J = x => Promise.resolve(new Response(JSON.stringify(Object.assign({ok: true}, x)), {headers: {'content-type': 'application/json'}}));
      const q = new URL(u, location.href);
      if (u.includes('/api/')) T.calls.push(q.pathname.slice(5) + q.search);
      if (q.pathname == '/api/disks') return J({disks: [{name: 'nvme0n1', removable: false, partitions: [{name: 'nvme0n1p3', label: 'RUDOLFDATA', fs: 'ext4', mount: '/persist'}]},
        {name: 'sda', removable: true, model: 'Stick', partitions: [{name: 'sda1', label: 'STICK', fs: 'vfat', bytes: 8e9, mount: '/media/sda1'}]}]});
      if (q.pathname == '/api/fs/search') return J({items: [{p: 'Photos/holiday-beach.JPG', dir: false, size: 2048, mtime: Date.now() - 2 * 864e5}, {p: 'old/holiday-1999.jpg', dir: false, size: 10, mtime: Date.now() - 900 * 864e5}], seen: 2000, truncated: true});
      if (u.includes('/api/')) return J({items: [], apps: []});
      return f0(u, o) } });
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8153/'); await p.waitForFunction(() => typeof explorer == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    ACCTS.length=0;ACCTS.push({n:'ann',h:await pwMake('a1'),r:'admin'});ACCTS.push({n:'bob',h:await pwMake('b1'),r:'user'});saveUser();
    USER=ACCTS[0];await userLogin(USER,'a1');const now=Date.now(),mt=(p,t)=>{META['/'+p.join('/')]={m:t}};
    const d=node(homePath('DOCUMENTS'));d['HOLIDAY PLAN.TXT']='packing list';d['NOTES.TXT']='the holiday is in May';const pic=node(homePath('PICTURES'));pic['HOLIDAY.PNG']='data:image/png;base64,AAAA';pic['OLD HOLIDAY.PNG']='data:image/png;base64,AAAA';
    mt([...homePath('PICTURES'),'HOLIDAY.PNG'],now-864e5);mt([...homePath('PICTURES'),'OLD HOLIDAY.PNG'],now-400*864e5);mt([...homePath('DOCUMENTS'),'HOLIDAY PLAN.TXT'],now);
    const big=node(homePath('MUSIC'));for(let i=0;i<1300;i++)big['HOLIDAY SONG '+i+'.MP3']='x';
    fs['C:'].USERS.BOB=fs['C:'].USERS.BOB||{};fs['C:'].USERS.BOB['HOLIDAY SECRET.TXT']='no';fsSave();return 1})()`);
  const Wb = "wins.at(-1).body";
  const search = async (term, ty, mo) => { await q(`(()=>{const B=${Wb},d=B.querySelector('.deep');d.checked=true;d.onchange({target:d});
      const t=B.querySelector('.dty'),m=B.querySelector('.dmo');t.value='${ty || ''}';t.onchange({target:t});m.value='${mo || ''}';m.onchange({target:m});
      const s=B.querySelector('.sr');s.value=${JSON.stringify(term)};s.oninput({target:s});return 1})()`) };
  const st = () => q(`(()=>{const s=${Wb}.querySelector('.xsrch');return s?s.textContent:''})()`);
  const names = () => q(`[...${Wb}.querySelectorAll('.xpanes [data-k]')].map(e=>e.dataset.k)`);
  const done = async () => { for (let i = 0; i < 60; i++) { if (/found|Nothing/.test(await st()) && !/Searching/.test(await st())) return; await wait(250) } };
  await q("(()=>{wins.slice().forEach(w=>w.close());explorer(homePath('DOCUMENTS'));return 1})()"); await wait(400);
  await search('holiday'); await done();
  let n = await names(), s = await st();
  ok('1. "Everywhere": this folder first (its file names and text inside), then the rest of C:', /DOCUMENTS\/HOLIDAY PLAN\.TXT$/.test(n[0]) && n.slice(0, 2).some(k => /NOTES\.TXT$/.test(k)) && n.some(k => /PICTURES\/HOLIDAY\.PNG$/.test(k)), n.slice(0, 5));
  ok('2. never another account\'s folder', !n.some(k => /BOB/.test(k)), n.filter(k => /BOB/.test(k)));
  ok('3. 1,000 at a time: "Show more" after the first 1,000', n.length == 1000 && /1000 found \(the first 1,000\)/.test(s) && await q(`[...${Wb}.querySelectorAll('.xsrch button')].some(b=>b.textContent=='Show more')`), [n.length, s]);
  await q(`[...${Wb}.querySelectorAll('.xsrch button')].find(b=>b.textContent=='Show more').click()`); await done();
  n = await names(); s = await st();
  ok('4. "Show more": the rest, and the drive\'s results with where they are', n.length > 1300 && n.some(k => /holiday-beach\.JPG$/.test(k)) && await q(`[...${Wb}.querySelectorAll('.xpanes small, .xpanes [data-k]')].some(e=>/STICK › Photos/.test(e.textContent||e.title))`), [n.length, s]);
  ok('5. a large drive: it says only part of it was looked at', /on STICK only the first 2,000 files and 3 folder levels/.test(s), s);
  ok('6. the drive was searched through rudolfd (fs/search), not the computer\'s own disk', (await q("T.calls")).some(c => /fs\/search\?path=%2Fmedia%2Fsda1&q=holiday/.test(c)) && !(await q("T.calls")).some(c => /fs\/search\?path=%2Fpersist/.test(c)), (await q("T.calls")).filter(c => /search/.test(c)));
  await search('holiday', 'pic', 'week'); await done(); n = await names();
  ok('7. filters: pictures changed this week (C: and the drive), nothing else', n.length == 2 && n.some(k => /PICTURES\/HOLIDAY\.PNG$/.test(k)) && n.some(k => /holiday-beach\.JPG$/.test(k)), n);
  // double-click a drive result: its folder on the drive
  await q(`(()=>{const e=[...${Wb}.querySelectorAll('.xpanes [data-k]')].find(x=>/holiday-beach/.test(x.dataset.k));e.ondblclick({stopPropagation(){}});return 1})()`); await wait(400);
  ok('8. a result on the drive opens its folder there', /\/media\/sda1\/Photos/.test(await q(`${Wb}.querySelector('.ad').value`)), await q(`${Wb}.querySelector('.ad').value`));
  // stopping
  await q("(()=>{const m=node(homePath('VIDEOS'));for(let i=0;i<40000;i++)m['CLIP '+i+'.MP4']='y';return 1})()");
  await q("(()=>{wins.slice().forEach(w=>w.close());explorer(homePath('VIDEOS'));return 1})()"); await wait(300);
  await search('nothing-like-this'); let running = '';
  for (let i = 0; i < 20 && !/Searching/.test(running); i++) { await wait(100); running = await st() }
  await q(`(()=>{const b=[...${Wb}.querySelectorAll('.xsrch button')].find(b=>b.textContent=='Stop');b&&b.click();return 1})()`); await wait(400);
  s = await st();
  ok('9. while it looks it says so, and Stop stops it', /Searching/.test(running) && /^Stopped: 0 found/.test(s) && await q(`[...${Wb}.querySelectorAll('.xsrch button')].some(b=>b.textContent=='Search again')`), [running, s]);
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
