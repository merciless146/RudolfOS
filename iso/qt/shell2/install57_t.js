// 1.5.7 (plan stage 9): the installation wizard for .AppImage, .bin and .run (Linux programs that need no installing):
// what it is, Run (rudolf-debian runfile through rudolfd's debian/runfile) and Keep it in the Start menu
// (debian/keepfile); Linux not set up; a file Linux cannot reach.   node install57_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 600) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8170, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.addInitScript(() => {
    window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0};
    window.T = {installed: 1, calls: [], apps: [{id: 'org.old.App', name: 'Old App', menu: true}]};
    const f0 = window.fetch;
    window.fetch = (u, o) => { u = String(u); const J = (x, st) => Promise.resolve(new Response(JSON.stringify(Object.assign({ok: !st}, x)), {status: st || 200, headers: {'content-type': 'application/json'}}));
      const q = new URL(u, location.href);
      if (u.includes('/api/')) T.calls.push(q.pathname.slice(5) + q.search);
      if (u.includes('/api/debian/status')) return J({installed: T.installed, version: '13.7', running: 1, wine: 1});
      if (u.includes('/api/debian/apps')) return J({apps: T.apps});
      if (q.pathname == '/api/debian/runfile') return J({message: 'started'});
      if (q.pathname == '/api/debian/keepfile') { T.apps = T.apps.concat([{id: 'u-72756e', name: q.searchParams.get('name'), menu: true}]); return J({message: 'kept'}) }
      if (u.includes('/api/')) return J({items: [], apps: []});
      return f0(u, o) } });
  await p.goto('http://127.0.0.1:8170/'); await p.waitForFunction(() => typeof wzLinuxFile == 'function' && FSLOADED && fs['C:'] && fs['C:'].USERS, null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>{w.__force=1;w.close()});USER=USER||ACCTS[0];USER.r='admin';if(!HOME){HOME={DOWNLOADS:{}};fs['C:'].USERS[uname()]=HOME}const d=node(downloadsPath());d['KRITA-5.2.6-X86_64.APPIMAGE']=dlMkRef('/home/rudolf/Downloads/krita-5.2.6-x86_64.appimage',3e8,'full');d['GAME_INSTALLER.BIN']=dlMkRef('/home/rudolf/Downloads/game_installer.bin',4e7,'full');d['DRIVER.RUN']=dlMkRef('/home/rudolf/Downloads/driver.run',1e6,'full');d['SMALL.APPIMAGE']='x';return 1})()");
  const W = () => q("(()=>{const w=wins.at(-1);return w?{txt:w.body.innerText,btn:[...w.body.querySelectorAll('.wzf button')].map(b=>b.textContent)}:null})()");
  const click = t => q(`(()=>{const b=[...wins.at(-1).body.querySelectorAll('.wzf button')].find(x=>x.textContent==${JSON.stringify(t)});b&&b.click();return !!b})()`);
  const open = n => q(`(()=>{wins.slice().forEach(w=>{w.__force=1;w.close()});openPath([...downloadsPath(),${JSON.stringify(n)}]);return 1})()`);
  await q("T.installed=0"); await open('KRITA-5.2.6-X86_64.APPIMAGE'); await wait(800);
  let w = await W();
  ok('1. Linux not set up: the wizard says so and offers to set it up', w && /not set up/.test(w.txt) && w.btn.includes('Set up Linux…'), w);
  await q("T.installed=1;DEB.st=null"); await open('KRITA-5.2.6-X86_64.APPIMAGE'); await wait(1000); w = await W();
  ok('2. an AppImage: what it is (runs without installing), its size, Run and Keep it in the Start menu', w && /A Linux program \(AppImage\): it runs without installing/.test(w.txt) && /286\.\d+ MB/.test(w.txt) && JSON.stringify(w.btn) == '["Run","Keep it in the Start menu","Cancel"]', w);
  await q("T.calls.length=0"); await click('Run'); await wait(500);
  let c = await q("T.calls.filter(x=>/runfile/.test(x))");
  ok('3. Run: debian/runfile with kind appimage and the file as Linux sees it', c.length == 1 && /kind=appimage&file=%2Fhome%2Frudolf%2FDownloads%2Fkrita-5.2.6-x86_64.appimage/.test(c[0]), c);
  await open('KRITA-5.2.6-X86_64.APPIMAGE'); await wait(1000); await q("T.calls.length=0"); await click('Keep it in the Start menu'); await wait(1200);
  c = await q("T.calls.filter(x=>/keepfile/.test(x))"); w = await W();
  ok('4. Keep it in the Start menu: debian/keepfile with a readable name ("krita 5.2.6"), and it says so', c.length == 1 && /name=krita%205.2.6/.test(c[0]) && /is in the Start menu now/.test(w.txt), [c, w && w.txt]);
  ok('5. ...and it is in the Start menu', await q("Object.values(APPS).some(a=>a&&a.name=='krita 5.2.6')"));
  await open('GAME_INSTALLER.BIN'); await wait(1000); w = await W(); await q("T.calls.length=0"); await click('Run'); await wait(400); c = await q("T.calls.filter(x=>/runfile/.test(x))");
  ok('6. a .bin: a program or installer; Run with kind bin', /A Linux program or installer \(\.bin\)/.test(w.txt) && c.length == 1 && /kind=bin&/.test(c[0]), [w && w.txt, c]);
  await open('DRIVER.RUN'); await wait(1000); w = await W();
  ok('7. a .run too', /\(\.run\)/.test(w.txt), w);
  await open('SMALL.APPIMAGE'); await wait(800); w = await W();
  ok('8. a file Linux cannot reach: move it into Downloads or Documents', /Linux cannot reach this file/.test(w.txt), w);
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
