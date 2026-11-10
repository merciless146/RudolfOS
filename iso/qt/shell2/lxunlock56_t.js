// 1.5.6: with Encrypt my data the Linux apps are in the encrypted folder, which opens at sign-in. Asked at start-up,
// the list of apps was empty and the desktop kept (and saved) that: on the Vivobook Discord and Steam were gone from the
// desktop and the Start menu, and their icons were the penguin. Now rudolfd says "locked" (423), the desktop keeps the
// list it had, and asks again once signed in.   node lxunlock56_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8160, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
const PNG = 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==';
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const ctx = await b.newContext(); const p = await ctx.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd|debian/.test(e.stack || e.message || '')) errs.push(e.message) });
  await p.addInitScript(png => {
    window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0};
    window.T = {open: false, inst: 0};
    const APPS_ = [{id: 'com.discordapp.Discord', name: 'Discord', launchers: [{desktop: 'com.discordapp.Discord', name: 'Discord', icon: 'com.discordapp.Discord'}]}, {id: 'com.valvesoftware.Steam', name: 'Steam', launchers: [{desktop: 'com.valvesoftware.Steam', name: 'Steam', icon: 'com.valvesoftware.Steam'}]}];
    const f0 = window.fetch;
    window.fetch = (u, o) => { u = String(u); const J = (x, st) => Promise.resolve(new Response(JSON.stringify(Object.assign({ok: !st}, x)), {status: st || 200, headers: {'content-type': 'application/json'}}));
      const q = new URL(u, location.href);
      if (q.pathname == '/api/apps/installed') { T.inst++; return T.open ? J({apps: APPS_}) : J({error: 'the apps are in the encrypted folder, which opens at sign-in'}, 423) }
      if (q.pathname == '/api/apps/icon') return T.open ? Promise.resolve(new Response(Uint8Array.from(atob(png), c => c.charCodeAt(0)), {headers: {'content-type': 'image/png'}})) : J({error: 'locked'}, 423);
      if (u.includes('/api/')) return J({items: [], apps: [], windows: []});
      return f0(u, o) } }, PNG);
  const q = s => p.evaluate(s);
  const load = async () => { await p.goto('http://127.0.0.1:8160/'); await p.waitForFunction(() => typeof lxAfterSignIn == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000}) };
  // a first start with the folder open: the list is had and saved, and Discord is put on the desktop
  await load();
  await q(`(async()=>{T.open=true;try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    ACCTS.length=0;ACCTS.push({n:'ann',h:await pwMake('a1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'a1');return 1})()`);
  await wait(2500);
  await q("(()=>{deskAdd('lx:com.discordapp.Discord');return 1})()"); await wait(300);
  ok('0. set up: Discord is on the desktop, the list is saved', await q("!!document.querySelector('#desk .ic[data-app=\"lx:com.discordapp.Discord\"]')") && (await q("ld(LXK,[]).length")) == 2);
  // the next start: the folder is locked until sign-in
  await q("(()=>{T.open=false;return 1})()");
  await load(); await q("(()=>{T.open=false;return 1})()");
  await wait(5000);   // the start-up's own request (4 s) is refused
  ok('1. locked at start-up: the list saved last time is kept (not emptied, not saved empty)', (await q("ld(LXK,[]).length")) == 2 && (await q("(LX.inst||[]).length")) == 2 && await q("T.inst") >= 1, [await q("ld(LXK,[]).length"), await q("(LX.inst||[]).length"), await q("T.inst")]);
  ok('2. ...so the apps stay in the Start menu meanwhile', await q("!!APPS['lx:com.discordapp.Discord']&&!!APPS['lx:com.valvesoftware.Steam']"));
  // sign in; the folder opens a few seconds later
  await q(`(async()=>{const o=document.getElementById('oobe');o&&o.remove();USER=ACCTS[0];await userLogin(USER,'a1');const L=document.getElementById('login');if(L)L.style.display='none';return 1})()`);
  const n0 = await q("T.inst"); await wait(4000); await q("(()=>{T.open=true;return 1})()"); await wait(4500);
  ok('3. signed in: the list is asked for again until it comes', (await q("T.inst")) > n0 + 1 && await q("LX.ok==1"), [n0, await q("T.inst")]);
  const di = await q("(()=>{const e=document.querySelector('#desk .ic[data-app=\"lx:com.discordapp.Discord\"]');return e?{img:!!e.querySelector('b img.lxapp'),txt:e.querySelector('b').textContent}:null})()");
  ok('4. Discord is on the desktop with its own picture', di && di.img, di);
  ok('5. ...and the Start menu has both, with their pictures', await q("(()=>{startMenuApps();return 1})()") && await wait(800) === undefined && await q("[...document.querySelectorAll('#menu li.lxv')].filter(li=>/Discord|Steam/.test(li.textContent)&&li.querySelector('img.lxapp')).length") == 2, await q("[...document.querySelectorAll('#menu li.lxv')].map(li=>li.textContent)"));
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
