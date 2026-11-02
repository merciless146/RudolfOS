// 1.4.8: Windows programs open with Wine (a Debian launcher kept out of menus still opens its file types, also files
// kept on the disk), and the Start menu picks up Debian apps added outside the Terminal. node lx48_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 400) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8098, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 760}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.addInitScript(() => {
    window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0};
    window.T = {apps: [{id: 'starport-ge', name: 'Starport GE', wmclass: 'launcher.exe', mime: '', categories: 'Game;', icon: '', menu: true},
                       {id: 'wine', name: 'Wine Windows Program Loader', wmclass: '', mime: 'application/x-ms-dos-executable;application/x-msi;application/x-ms-shortcut;', categories: '', icon: '', menu: false}],
                launched: [], appsCalls: 0};
    const f0 = window.fetch;
    window.fetch = (u, o) => { u = String(u); const J = x => Promise.resolve(new Response(JSON.stringify(Object.assign({ok: true}, x)), {headers: {'content-type': 'application/json'}}));
      if (u.includes('/api/debian/status')) return J({installed: 1, version: '13.7', running: 1});
      if (u.includes('/api/debian/apps')) { T.appsCalls++; return J({apps: T.apps}) }
      if (u.includes('/api/debian/launch')) { const q = new URL(u, location.href).searchParams; T.launched.push([q.get('id'), q.get('file')]); return J({}) }
      if (u.includes('/api/dl/link')) return J({url: '/x'});
      if (u.includes('/api/')) return J({items: [], apps: []});
      return f0(u, o) } });
  await p.goto('http://127.0.0.1:8098/'); await p.waitForFunction(() => typeof debApps == 'function' && typeof dlLxOpen == 'function' && typeof openersFor == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>{w.__force=1;w.close()});USER=USER||ACCTS[0];USER.r='admin';return 1})()");
  await q("debApps()"); await wait(800);
  const menu = await q("(startMenuApps(),[...document.querySelectorAll('#menu li.lxv')].map(l=>l.textContent.trim()))");
  ok('1. the Start menu lists Starport GE, not Wine\'s program loader (a launcher kept out of menus)', menu.some(t => /Starport GE/.test(t)) && !menu.some(t => /Wine/.test(t)), menu);
  // a small .exe kept by the desktop itself
  await q("(()=>{fs['C:']['TINY.EXE']='data:application/octet-stream;base64,TVo=';return 1})()");
  const op = await q("openersFor(['C:','TINY.EXE']).map(o=>o[1])");
  ok('2. a Windows program (.exe) can be opened with Wine (Open with)', op.some(t => /Wine/.test(t)), op);
  // a 60 MB installer kept on the disk (Downloads larger than 1.5 MB)
  await q("(()=>{const C=dlCDir();C['STARPORTGE_INSTALL.EXE']=dlMkRef('/home/rudolf/Downloads/StarportGE_install.exe',60018940,'ok');return 1})()");
  const op2 = await q("openersFor(['C:','DOWNLOADS','STARPORTGE_INSTALL.EXE']).map(o=>o[1])");
  ok('3. an installer kept on the disk offers Wine too (not only the built-in viewer)', op2.some(t => /Wine/.test(t)), op2);
  await q("openPath(['C:','DOWNLOADS','STARPORTGE_INSTALL.EXE']);1"); await wait(800);
  const L = await q("T.launched");
  ok('4. opening it starts Wine with the file\'s own path on the disk (no copy)', L.some(x => x[0] == 'wine' && x[1] == '/home/rudolf/Downloads/StarportGE_install.exe'), L);
  // without Wine: say how to get it
  await q("T.apps=T.apps.filter(a=>a.id!='wine');DEB.appsAt=0;debApps()"); await wait(800);
  await q("wins.slice().forEach(w=>{w.__force=1;w.close()});openPath(['C:','DOWNLOADS','STARPORTGE_INSTALL.EXE']);1"); await wait(800);
  const msg = await q("(wins.at(-1)&&wins.at(-1).el.textContent)||''");
  ok('5. without Wine, a Windows program says how to get it (Debian, then apt install wine)', /Windows program/.test(msg) && /apt install wine/.test(msg), msg);
  // a Debian app added outside the Terminal shows up when the Start menu opens
  await q("T.apps.push({id:'newgame',name:'New Game',wmclass:'',mime:'',categories:'Game;',icon:'',menu:true});DEB.appsAt=Date.now()-60000;1");
  const n0 = await q("T.appsCalls");
  await q("(()=>{const m=document.getElementById('menu');m.style.display='none';return 1})()"); await wait(100);
  await q("(()=>{const m=document.getElementById('menu');m.style.display='block';return 1})()"); await wait(1500);
  const menu2 = await q("[...document.querySelectorAll('#menu li.lxv')].map(l=>l.textContent.trim())");
  ok('6. opening the Start menu picks up a Debian app added outside the Terminal', (await q("T.appsCalls")) > n0 && menu2.some(t => /New Game/.test(t)), [menu2, n0]);
  await q("(()=>{const m=document.getElementById('menu');m.style.display='none';return 1})()"); const n1 = await q("T.appsCalls");
  await q("(()=>{const m=document.getElementById('menu');m.style.display='block';return 1})()"); await wait(800);
  ok('7. ...but not again within 10 s (the menu opens instantly)', (await q("T.appsCalls")) == n1);
  ok('8. no page errors', !errs.length, errs);
  console.log(`${R.filter(x => x).length}/${R.length} passed`); await b.close(); srv.close(); process.exit(R.every(x => x) ? 0 : 1);
})();
