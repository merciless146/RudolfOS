// 1.5.7 (plan stage 7): apps in the background and memory. Task Manager lists the apps that run (also with no window)
// with "End"; the memory guard's closing of an app is said at once; a game started with little memory free offers to
// close background apps; System health › Memory.   node apps57_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 600) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm', '.json': 'application/json'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8167, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await (await b.newContext()).newPage();
  const errs = []; p.on('pageerror', e => errs.push(e.message + ' ' + String(e.stack).slice(0, 300)));
  await p.addInitScript(() => {
    window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0};
    const now = Math.floor(Date.now() / 1000);
    window.M = {calls: [], avail: 3e9, total: 3.6e9, zram: 'auto', ca: true,
      apps: [{id: 'flatpak:com.discordapp.Discord', app: 'com.discordapp.Discord', name: 'Discord', kind: 'flatpak', rss: 640 * 1048576, window: false},
             {id: 'wine:4242', name: 'StarportGE.exe', kind: 'wine', rss: 300 * 1048576, window: true},
             {id: 'debian:5151', name: 'gimp', kind: 'debian', rss: 90 * 1048576, window: false}]};
    const H = [{t: now - 3600, k: 'memory', m: 'memory ran out: Discord, which had no window open and used 612 MB, was closed'}, {t: now - 9 * 86400, k: 'memory', m: 'memory ran out: a web page using 1882 MB was closed'}];
    const f0 = window.fetch;
    window.fetch = (u, o) => { u = String(u); const q = new URL(u, location.href); const a = q.pathname.replace('/api/', ''); const J = (x, st) => Promise.resolve(new Response(JSON.stringify(Object.assign({ok: !st}, x)), {status: st || 200, headers: {'content-type': 'application/json'}}));
      if (u.includes('/api/')) M.calls.push(((o && o.method) || 'GET') + ' ' + a + q.search);
      if (a == 'apps/running') return J({apps: M.apps});
      if (a == 'apps/end') { const id = q.searchParams.get('id'); M.apps = M.apps.filter(x => x.id != id); return J({processes: 3, forced: false}) }
      if (a == 'apps/run') return J({});
      if (a == 'apps/installed') return J({apps: [{id: 'com.discordapp.Discord', name: 'Discord', cats: ['Network'], launchers: []}, {id: 'com.example.Game', name: 'Space Game', cats: ['Game', 'ActionGame'], launchers: []}]});
      if (a == 'stats') return J({cpu: 5, memTotal: M.total, memAvailable: M.avail, tempC: null, battery: null});
      if (a == 'memory/settings') { if (o && o.method == 'POST') { const z = q.searchParams.get('zram'), c = q.searchParams.get('closeApps'); if (z) M.zram = z; if (c) M.ca = c == '1' }
        return J({zram: M.zram, closeApps: M.ca, persistent: true, total: M.total, available: M.avail, swapTotal: 3.6e9, swapFree: 3e9, compressed: {size: 3.6e9, stored: 900 * 1048576, used: 300 * 1048576, on: true}, setup: ''}) }
      if (a == 'health') return J({events: H.slice().reverse(), now});
      if (a == 'processes') return J({items: [], pages: {pages: []}});
      if (u.includes('/api/')) return J({items: [], apps: [], windows: []});
      return f0(u, o) } });
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8167/'); await p.waitForFunction(() => typeof tmApps57 == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS.length=0;ACCTS.push({n:'ann',h:await pwMake('a1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'a1');
    LX.inst=[{id:'com.discordapp.Discord',name:'Discord',cats:['Network']},{id:'com.example.Game',name:'Space Game',cats:['Game','ActionGame']}];APPS['lx:com.discordapp.Discord']={name:'Discord',k:'lx:com.discordapp.Discord'};window.__elevTrust=Date.now();return 1})()`);
  await wait(1200);
  // 1. Task Manager
  await q("(()=>{taskmgr26();return 1})()"); await wait(2800);
  const rows = await q("[...document.querySelectorAll('.tma57 tbody tr')].map(r=>[...r.cells].slice(0,4).map(c=>c.textContent).join('|'))");
  ok('1. Task Manager lists the apps that run, also with no window: Discord (Linux app), StarportGE (Windows program), gimp (Linux program), largest first', rows.length == 3 && rows[0].startsWith('Discord|Linux app|None (in the background)|640') && rows[1].startsWith('StarportGE|Windows program|Open|') && rows[2].startsWith('gimp|Linux program|None'), rows);
  await q("(()=>{[...document.querySelectorAll('.tma57 tbody tr')].find(r=>/gimp/.test(r.textContent)).querySelector('.et').click();return 1})()"); await wait(2500);
  const c2 = await q("M.calls.filter(c=>/apps\\/end/.test(c))"), rows2 = await q("[...document.querySelectorAll('.tma57 tbody tr')].map(r=>r.cells[0].textContent)");
  ok('2. End: POST apps/end?id=debian:5151; gimp is gone from the list', JSON.stringify(c2) == '["POST apps/end?id=debian%3A5151"]' && !rows2.includes('gimp'), [c2, rows2]);
  await q("(()=>{wins.filter(w=>/Task Manager/.test(w.el.querySelector('.t').textContent)).forEach(w=>w.close());return 1})()");
  // 3. the memory guard closed an app
  await q("(()=>{document.querySelectorAll('.ntoast').forEach(x=>x.remove());evDispatch('memory closed Discord 612\\n');return 1})()"); await wait(300);
  const nt = await q("[...document.querySelectorAll('.ntoast')].map(t=>t.textContent).join('|')");
  ok('3. the memory guard closing Discord is said at once', /Discord was closed to free memory/.test(nt) && /had no window open and used 612 MB/.test(nt), nt);
  // 4. a game with little memory free
  await q("(()=>{M.apps=[{id:'flatpak:com.discordapp.Discord',app:'com.discordapp.Discord',name:'Discord',kind:'flatpak',rss:640*1048576,window:false}];return 1})()");
  await q("(()=>{M.avail=0.6e9;M.calls.length=0;window.__run=lxRun({app:'com.example.Game',desktop:'com.example.Game',name:'Space Game'});return 1})()"); await wait(800);
  const dl = await q("(()=>{const o=document.querySelector('.gm57');return o?o.textContent:null})()");
  ok('4. a game with little memory free: "Discord is running in the background and uses 640 MB… Close it for the game?"', dl && /Discord is running in the background and uses 640(\.00)? MB/.test(dl) && /Close Discord and play/.test(dl) && !(await q("M.calls.some(c=>/apps\\/run\\?/.test(c))")), dl);
  await q("(()=>{document.querySelector('.gm57 .ok').click();return 1})()"); await q("__run"); await wait(300);
  const c4 = await q("M.calls.filter(c=>/apps\\/(end|run)\\?/.test(c))");
  ok('5. ..."Close Discord and play" ends Discord, then starts the game', c4.length == 2 && /apps\/end\?id=flatpak%3Acom.discordapp.Discord/.test(c4[0]) && /apps\/run\?id=com.example.Game/.test(c4[1]), c4);
  await q("(()=>{M.avail=3e9;M.calls.length=0;M.apps=[{id:'flatpak:com.discordapp.Discord',app:'com.discordapp.Discord',name:'Discord',kind:'flatpak',rss:640*1048576,window:false}];return lxRun({app:'com.example.Game',desktop:'com.example.Game',name:'Space Game'})})()"); await wait(300);
  ok('6. with enough memory free the game starts without asking', !(await q("!!document.querySelector('.gm57')")) && (await q("M.calls.filter(c=>/apps\\/(end|run)\\?/.test(c)).length")) == 1);
  await q("(()=>{M.avail=0.6e9;M.calls.length=0;return lxRun({app:'com.discordapp.Discord',desktop:'com.discordapp.Discord',name:'Discord'})})()"); await wait(300);
  ok('7. an app that is not a game starts without asking', !(await q("!!document.querySelector('.gm57')")));
  // 8. System health › Memory
  await q("(()=>{M.avail=1.2e9;settings('health');return 1})()"); await wait(1500);
  const mp = await q("(()=>{const d=document.querySelector('.mem57');return d?{t:d.innerText,z:d.querySelector('.mzs').value,ca:d.querySelector('.mca').checked}:null})()");
  ok('8. System health › Memory: in use now, the compressed memory, the setting, the switch, what was closed in the last 7 days', mp && /2\.\d GB of 3\.\d GB/.test(mp.t) && /holding 900(\.00)? MB in 300(\.00)? MB/.test(mp.t) && mp.z == 'auto' && mp.ca === true && /Discord, which had no window open and used 612 MB, was closed/.test(mp.t) && !/1882 MB/.test(mp.t), mp);
  await q("(()=>{const s=document.querySelector('.mem57 .mzs');s.value='small';s.dispatchEvent(new Event('change'));return 1})()"); await wait(800);
  await q("(()=>{const c=document.querySelector('.mem57 .mca');c.click();return 1})()"); await wait(800);
  const c8 = await q("M.calls.filter(c=>/^POST memory\\/settings/.test(c))");
  ok('9. ...changing them saves them (zram=small, closeApps=0)', JSON.stringify(c8) == '["POST memory/settings?zram=small","POST memory/settings?closeApps=0"]' && await q("M.zram=='small'&&M.ca===false"), c8);
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
