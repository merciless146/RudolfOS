// 1.4.9: Install Wine from Settings › Apps › Linux and from a Windows program's message; Debian's apps include the
// programs Wine installs (the person's own launchers). node wine49_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 400) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8100, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.addInitScript(() => {
    window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0};
    window.T = {wine: 0, calls: [], job: null,
      apps: [{id: 'u-77696e652f50726f6772616d732f53746172706f727447452f53746172706f7274204745', name: 'Starport GE', wmclass: 'launcher.exe', mime: '', categories: '', icon: '/home/rudolf/.local/share/icons/hicolor/256x256/apps/E45A_Launcher.0.png', menu: true}]};
    const f0 = window.fetch;
    window.fetch = (u, o) => { u = String(u); const J = x => Promise.resolve(new Response(JSON.stringify(Object.assign({ok: true}, x)), {headers: {'content-type': 'application/json'}}));
      if (u.includes('/api/')) T.calls.push(u.replace(/^.*\/api\//, ''));
      if (u.includes('/api/debian/status')) return J({installed: 1, version: '13.7', running: 1, size: 3e9, free: 1e11, wine: T.wine});
      if (u.includes('/api/debian/apps')) return J({apps: T.apps});
      if (u.includes('/api/debian/wine')) { T.job = {running: true, status: -1, log: ''}; setTimeout(() => { T.job = {running: false, status: 0, log: 'Get:1 wine\nUnpacking wine\nSetting up wine\ndone Wine is installed\n'}; T.wine = 1 }, 1500); return J({}) }
      if (u.includes('/api/job')) return J(T.job || {running: false, status: 0, log: ''});
      if (u.includes('/api/debian/icon')) return Promise.resolve(new Response(new Blob([new Uint8Array([137, 80, 78, 71])], {type: 'image/png'})));
      if (u.includes('/api/dl/link')) return J({url: '/x'});
      if (u.includes('/api/')) return J({items: [], apps: []});
      return f0(u, o) } });
  await p.goto('http://127.0.0.1:8100/'); await p.waitForFunction(() => typeof debInstallWine == 'function' && FSLOADED && fs['C:'] && fs['C:'].USERS && typeof settings == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>{w.__force=1;w.close()});USER=USER||ACCTS[0];USER.r='admin';if(!HOME){HOME={DOWNLOADS:{}};fs['C:'].USERS[uname()]=HOME}return 1})()");
  await q("debApps()"); await wait(800);
  const menu = await q("(startMenuApps(),[...document.querySelectorAll('#menu li.lxv')].map(l=>l.textContent.trim()))");
  ok('1. a program Wine installed (the person\'s own launcher) is in the Start menu', menu.some(t => /Starport GE/.test(t)), menu);
  // Settings › Apps › Linux
  await q("settings('apps');1"); await wait(2500);
  const cell = await q("(document.querySelector('.dwine')||{}).textContent||''");
  ok('2. Settings › Apps › Linux says Wine is not installed, with Install Wine…', /not installed/.test(cell) && !!(await q("!!document.querySelector('.dwine .dwi')")), cell);
  await q("document.querySelector('.dwine .dwi').click();1"); await wait(600);
  ok('3. Install Wine… asks rudolfd to install it (one job)', (await q("T.calls.filter(c=>c.startsWith('debian/wine')).length")) == 1, await q("T.calls"));
  await wait(4500);
  const cell2 = await q("(document.querySelector('.dwine')||{}).textContent||''"), msg = await q("(document.querySelector('.dmsg')||{}).textContent||''");
  ok('4. when it is done, the panel says Wine is installed', /Wine is installed/.test(cell2) || /Wine is installed/.test(msg), [cell2, msg]);
  ok('5. ...and a notification says so', await q("NC.some(n=>n.t=='Wine is installed')"));
  // a Windows program without Wine: the message has Install Wine…
  await q("T.wine=0;T.calls.length=0;wins.slice().forEach(w=>{w.__force=1;w.close()});DEB.st=null;1"); await q("debStatus()");
  await q("(()=>{const C=dlCDir();C['GAME_SETUP.EXE']=dlMkRef('/home/rudolf/Downloads/Game_Setup.exe',30e6,'ok');return 1})()");
  await q("openPath([...downloadsPath(),'GAME_SETUP.EXE']);1"); await wait(1200);
  const w = await q("[(wins.at(-1)&&wins.at(-1).el.textContent)||'',[...wins.at(-1).el.querySelectorAll('button')].map(b=>b.textContent)]");
  ok('6. a Windows program without Wine offers Install Wine… right there', /Windows program/.test(w[0]) && w[1].includes('Install Wine…'), w);
  await q("[...wins.at(-1).el.querySelectorAll('button')].find(b=>b.textContent=='Install Wine…').click();1"); await wait(5000);
  const w2 = await q("[...wins.at(-1).el.querySelectorAll('button')].map(b=>b.textContent)");
  ok('7. ...which installs it and then offers to open the program with Wine', (await q("T.calls.filter(c=>c.startsWith('debian/wine')).length")) == 1 && w2.includes('Open it with Wine'), [w2, await q("T.calls")]);
  ok('8. no page errors', !errs.length, errs);
  console.log(`${R.filter(x => x).length}/${R.length} passed`); await b.close(); srv.close(); process.exit(R.every(x => x) ? 0 : 1);
})();
