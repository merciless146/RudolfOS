// 1.5.6: every app in the Start menu has the same right-click menu (Open, Pin to taskbar, Add to / Remove from desktop,
// Properties):
// Linux apps (Flathub, and Debian's such as Starport GE under Wine) had none, Emu only Open. Flathub apps also get
// Uninstall…; a Linux app pinned to the taskbar shows its own picture.   node startmenu56_t.js [CHROME] [SHOT-DIR]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 600) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8162, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
const PNG = 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==';
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd|debian/.test(e.stack || e.message || '')) errs.push(e.message) });
  await p.addInitScript(png => {
    window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0};
    window.T = {calls: []};
    const f0 = window.fetch;
    window.fetch = (u, o) => { u = String(u); const J = x => Promise.resolve(new Response(JSON.stringify(Object.assign({ok: true}, x)), {headers: {'content-type': 'application/json'}}));
      const q = new URL(u, location.href); if (u.includes('/api/')) T.calls.push(q.pathname.slice(5) + q.search);
      if (q.pathname == '/api/apps/installed') return J({apps: T.gone ? [] : [{id: 'com.discordapp.Discord', name: 'Discord', version: '1.0.169', size: '597.6 MB', origin: 'flathub', launchers: [{desktop: 'com.discordapp.Discord', name: 'Discord', icon: 'com.discordapp.Discord', comment: 'All-in-one voice and text chat', mime: 'x-scheme-handler/discord;', wmclass: 'discord', cats: 'Network;InstantMessaging;'}]}]});
      if (q.pathname == '/api/debian/status') return J({installed: 1, wine: 1});
      if (q.pathname == '/api/debian/apps') return J({apps: [{id: 'starport-ge', name: 'Starport GE', wmclass: 'launcher.exe', categories: 'Game;', icon: '/persist/debian/usr/share/pixmaps/starport-ge.png', menu: true}]});
      if (q.pathname == '/api/apps/icon' || q.pathname == '/api/debian/icon') return Promise.resolve(new Response(Uint8Array.from(atob(png), c => c.charCodeAt(0)), {headers: {'content-type': 'image/png'}}));
      if (q.pathname == '/api/job') return J({running: false, status: 0, log: 'done\n'});
      if (u.includes('/api/')) return J({items: [], apps: [], windows: []});
      return f0(u, o) } }, PNG);
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8162/'); await p.waitForFunction(() => typeof lxAfterSignIn == 'function' && IDB && IDB.db, null, {timeout: 30000});
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    ACCTS.length=0;ACCTS.push({n:'ann',h:await pwMake('a1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'a1');window.elevate=async()=>true;return 1})()`);
  await wait(6000);
  const menuOf = i => q(`(()=>{document.querySelectorAll('.cmenu').forEach(m=>m.remove());const li=[...document.querySelectorAll('#menu li')][${i}];const before=new Set([...document.body.children]);
    li.dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,cancelable:true,clientX:100,clientY:100}));const m=[...document.body.children].filter(e=>!before.has(e));const t=m.map(e=>e.textContent).join(' ');m.forEach(e=>e.remove());return t})()`);
  const items = await q("[...document.querySelectorAll('#menu li')].map(li=>({t:li.textContent.trim(),a:li.dataset.a||'',v:li.dataset.v25||'',lx:li.classList.contains('lxv'),head:li.classList.contains('offh')}))");
  const SYS = ['docs', 'run', 'off'];   // the Programs folder, Run…, Shut Down…: not apps
  const apps = items.map((x, i) => ({...x, i})).filter(x => !x.head && !SYS.includes(x.a));
  const lacking = [];
  for (const x of apps) { const m = await menuOf(x.i); if (!/Open/.test(m) || !/Pin to taskbar|Unpin from taskbar/.test(m) || !/Add to desktop|Remove from desktop/.test(m) || !/Properties/.test(m)) lacking.push(x.t + ' → ' + m.slice(0, 60)) }
  ok(`1. every app in the Start menu (${apps.length}) has Open, Pin to taskbar, Add to / Remove from desktop and Properties`, apps.length > 30 && !lacking.length, lacking);
  ok('2. Linux apps among them: Discord (Flathub) and Starport GE (Debian, Wine)', apps.some(x => /Discord/.test(x.t)) && apps.some(x => /Starport GE/.test(x.t)));
  ok('3. Emu too (gbplayer in the menu)', apps.some(x => x.a == 'gbplayer'));
  const iD = items.findIndex(x => /Discord/.test(x.t)), iS = items.findIndex(x => /Starport GE/.test(x.t));
  ok('4. a Flathub app also has Uninstall…; a Debian program not', /Uninstall/.test(await menuOf(iD)) && !/Uninstall/.test(await menuOf(iS)));
  // pin Discord from the menu: its own picture on the taskbar
  await q(`(()=>{const li=[...document.querySelectorAll('#menu li')][${iD}];li.dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,cancelable:true,clientX:100,clientY:100}));
    const it=[...document.querySelectorAll('.cmenu *')].find(e=>e.children.length<=2&&/^\\W*Pin to taskbar$/.test(e.textContent.trim()));it&&it.click();return 1})()`); await wait(800);
  ok('5. pinned from the Start menu: Discord on the taskbar with its own picture', await q("!!document.querySelector('#pins .pin[data-k=\"lx:com.discordapp.Discord\"] img.lxapp')"), await q("[...document.querySelectorAll('#pins .pin')].map(b=>b.dataset.k+':'+b.innerHTML.slice(0,40))"));
  // Uninstall…
  await q(`(()=>{const li=[...document.querySelectorAll('#menu li')][${iD}];li.dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,cancelable:true,clientX:100,clientY:100}));
    const it=[...document.querySelectorAll('.cmenu *')].find(e=>e.children.length<=2&&/^\\W*Uninstall…$/.test(e.textContent.trim()));it&&it.click();return 1})()`); await wait(2500);
  ok('6. Uninstall… (after the administrator\'s yes) uninstalls it through rudolfd, as the App Store does', (await q("T.calls")).some(c => /apps\/uninstall\?id=com\.discordapp\.Discord/.test(c)), (await q("T.calls")).filter(c => /uninstall/.test(c)));
  // Properties: that app's extended information
  const props = async i => { await q(`(()=>{wins.slice().forEach(w=>w.close());const li=[...document.querySelectorAll('#menu li')][${i}];li.dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,cancelable:true,clientX:100,clientY:100}));
    const it=[...document.querySelectorAll('.cmenu *')].find(e=>e.children.length<=2&&/^\\W*Properties$/.test(e.textContent.trim()));it&&it.click();return 1})()`); await wait(400);
    const g = await q("(()=>{const W=wins.at(-1);return W?{title:W.title||W.el.querySelector('.tb').textContent,body:W.body.textContent}:null})()");
    await q("(()=>{const W=wins.at(-1);[...W.body.querySelectorAll('.aptabs button')].find(b=>b.textContent=='Details').click();return 1})()"); await wait(100);
    const d = await q("wins.at(-1).body.textContent"); return {g, d} };
  const iN = items.findIndex(x => x.a == 'notepad' || x.v == 'notepad');
  let r = await props(iN);
  ok('7. Properties of a built-in app (Notepad): its type, description, version of Rudolf OS', r.g && /Notepad Properties/.test(r.g.title) && /Built into Rudolf OS/.test(r.g.body) && /plain text/.test(r.g.body) && /Rudolf OS \d+\.\d+/.test(r.d), r);
  r = await props(items.findIndex(x => /Discord/.test(x.t)));
  ok('8. ...of a Flathub app (Discord): Linux app (Flathub), its ID, version, size, publisher and what it opens', /Linux app \(Flathub\)/.test(r.g.body) && /com\.discordapp\.Discord/.test(r.d) && /1\.0\.169/.test(r.d) && /597\.6 MB/.test(r.d) && /Flathub/.test(r.d) && /x-scheme-handler\/discord/.test(r.d), r.d.slice(0, 400));
  if (process.argv[3]) { await q("(()=>{const W=wins.at(-1);[...W.body.querySelectorAll('.aptabs button')].find(b=>b.textContent=='General').click();return 1})()"); await wait(600);
    const bx = await q("(()=>{const r=wins.at(-1).el.getBoundingClientRect();return{x:r.x,y:r.y,width:r.width,height:r.height}})()"); await p.screenshot({path: path.join(process.argv[3], 'props-discord.png'), clip: bx}) }
  r = await props(items.findIndex(x => /Starport GE/.test(x.t)));
  ok('9. ...of a Windows program in Debian (Starport GE): Windows program (Debian, Wine), its ID and window class', /Windows program \(Debian, Wine\)/.test(r.g.body) && /starport-ge/.test(r.d) && /launcher\.exe/.test(r.d), [r.g.body.slice(0, 200), r.d.slice(0, 300)]);
  r = await props(items.findIndex(x => x.a == 'gbplayer'));
  ok('10. ...of Emu', /Emu Properties/.test(r.g.title) && /Game Boy/.test(r.g.body), r.g);
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
