// 1.5.6: the installation wizard (install56.js), with a stand-in rudolfd: a .deb is read (debian/debinfo), installed
// as a job (debian/deb) with progress, and its new app is offered (Open, Put on the desktop); Linux not set up, a
// package for another processor, a failed install, a file Linux cannot reach, and an .ipa (explained, not installed).
// node install56_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 500) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8143, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.addInitScript(() => {
    window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0};
    window.T = {installed: 1, wine: 1, arch: 'amd64', fail: 0, calls: [], launched: [], apps: [{id: 'org.old.App', name: 'Old App', menu: true}, {id: 'wine', name: 'Wine Windows Program Loader', menu: false, mime: 'application/x-ms-dos-executable;application/x-msi;'}], job: null};
    const f0 = window.fetch;
    window.fetch = (u, o) => { u = String(u); const J = (x, st) => Promise.resolve(new Response(JSON.stringify(Object.assign({ok: !st}, x)), {status: st || 200, headers: {'content-type': 'application/json'}}));
      const q = new URL(u, location.href);
      if (u.includes('/api/')) T.calls.push(q.pathname.slice(5) + q.search);
      if (u.includes('/api/debian/status')) return J({installed: T.installed, version: '13.7', running: 1, wine: T.wine});
      if (u.includes('/api/debian/launch')) { T.launched.push([q.searchParams.get('id'), q.searchParams.get('file')]); setTimeout(() => { T.apps = T.apps.concat([{id: 'u-77696e65', name: 'Starport GE', menu: true}]) }, 1500); return J({message: 'started'}) }
      if (u.includes('/api/debian/apps')) return J({apps: T.apps});
      if (u.includes('/api/debian/debinfo')) return J({package: 'discord', version: '0.0.120', arch: T.arch, maintainer: 'Discord Inc. <dev@discord.com>', size: '252000', description: 'Chat for Communities and Friends'});
      if (u.includes('/api/debian/deb')) { T.job = {n: 0}; return J({message: 'started'}) }
      if (q.pathname == '/api/job') { const j = T.job; if (!j) return J({running: 0, status: 0, log: ''}); j.n++;
        const log = 'step Getting the list of packages\nGet:1 http://deb.debian.org trixie InRelease\nstep Installing\nUnpacking discord (0.0.120) ...\nSetting up discord (0.0.120) ...\n' + (T.fail ? 'E: Unable to locate package libappindicator1\n' : 'done installed\n');
        if (j.n < 3) return J({running: 1, status: -1, log: log.split('\n').slice(0, 2 * j.n).join('\n') + '\n'});
        if (!T.fail) T.apps = T.apps.concat([{id: 'discord', name: 'Discord', menu: true, icon: ''}]);
        return J({running: 0, status: T.fail ? 100 : 0, log}) }
      if (u.includes('/api/')) return J({items: [], apps: []});
      return f0(u, o) } });
  await p.goto('http://127.0.0.1:8143/'); await p.waitForFunction(() => typeof installWizard == 'function' && FSLOADED && fs['C:'] && fs['C:'].USERS, null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>{w.__force=1;w.close()});USER=USER||ACCTS[0];USER.r='admin';if(!HOME){HOME={DOWNLOADS:{}};fs['C:'].USERS[uname()]=HOME}const d=node(downloadsPath());d['DISCORD.DEB']=dlMkRef('/home/rudolf/Downloads/DISCORD.DEB',2387236,'full');d['APP.IPA']='x';d['STARPORTGE_INSTALL.EXE']=dlMkRef('/home/rudolf/Downloads/StarportGE_install.exe',5e6,'full');d['GAME.EXE']=dlMkRef('/home/rudolf/Downloads/game.exe',2e6,'full');d['SMALL.DEB']='!<arch>';return 1})()");
  const W = () => q("(()=>{const w=wins.at(-1);return w?{t:w.title||(w.el&&w.el.querySelector('.tb span:nth-child(2)')||{}).textContent,txt:w.body.innerText,btn:[...w.body.querySelectorAll('.wzf button')].map(b=>b.textContent)}:null})()");
  const click = t => q(`(()=>{const b=[...wins.at(-1).body.querySelectorAll('.wzf button')].find(x=>x.textContent==${JSON.stringify(t)});b&&b.click();return !!b})()`);
  const open = n => q(`(()=>{wins.slice().forEach(w=>{w.__force=1;w.close()});openPath([...downloadsPath(),${JSON.stringify(n)}]);return 1})()`);
  await q("T.installed=0"); await open('DISCORD.DEB'); await wait(800);
  let w = await W();
  ok('1. Linux not set up: the wizard says so and offers to set it up', /not set up/.test(w.txt) && w.btn.includes('Set up Linux…'), w);
  await q("T.installed=1;DEB.st=null"); await open('DISCORD.DEB'); await wait(1000); w = await W();
  ok('2. what the package is: name, version, description, from, size', /discord 0\.0\.120/.test(w.txt) && /Chat for Communities/.test(w.txt) && /Discord Inc/.test(w.txt) && /246\.1 MB/.test(w.txt) && w.btn.includes('Install'), w);
  ok('...asked rudolfd with the file\'s path for Linux', (await q("T.calls")).some(c => c.startsWith('debian/debinfo?file=%2Fhome%2Frudolf%2FDownloads%2FDISCORD.DEB')), await q("T.calls"));
  await click('Install'); await wait(4500); w = await W();
  ok('3. installing: the job with the file, then "installed" and the new app with Open and Put on the desktop', /discord is installed/.test(w.txt) && /Discord/.test(w.txt) && !/Old App/.test(w.txt) && (await q("T.calls")).some(c => c.startsWith('debian/deb?file=')) && await q("!!wins.at(-1).body.querySelector('.wzapp .o')&&!!wins.at(-1).body.querySelector('.wzapp .d')"), w);
  const put = await q("(()=>{wins.at(-1).body.querySelector('.wzapp .d').click();return deskHasApp('lx:deb-discord')})()");
  ok('...Put on the desktop puts it there', put);
  await q("T.fail=1;T.apps=T.apps.filter(a=>a.id!='discord')"); await open('DISCORD.DEB'); await wait(1000); await click('Install'); await wait(4500); w = await W();
  ok('4. a failed install says why, from the log', /could not be installed: E: Unable to locate package libappindicator1/.test(w.txt), w);
  await q("T.fail=0;T.arch='arm64'"); await open('DISCORD.DEB'); await wait(1000); w = await W();
  ok('5. a package for another processor is not offered', /another kind of processor \(arm64\)/.test(w.txt) && !w.btn.includes('Install'), w);
  await q("T.arch='amd64'"); await open('SMALL.DEB'); await wait(800); w = await W();
  ok('6. a file Linux cannot reach (kept only in the desktop) says to move it', /cannot reach this file/.test(w.txt), w);
  await open('APP.IPA'); await wait(600); w = await W();
  ok('7. an .ipa is explained, not installed', /cannot be installed/.test(w.txt) && /Apple/.test(w.txt) && w.btn.includes('Look in the App Store'), w);
  // Windows programs
  await q("T.wine=0;T.apps=T.apps.filter(a=>a.id!='wine');DEB.apps=T.apps;DEB.st=null"); await open('STARPORTGE_INSTALL.EXE'); await wait(1000); w = await W();
  ok('8. a Windows installer without Wine: offers to install Wine', /Wine, which is not installed yet/.test(w.txt) && w.btn.includes('Install Wine'), w);
  await q("T.wine=1;T.apps=T.apps.concat([{id:'wine',name:'Wine Windows Program Loader',menu:false,mime:'application/x-ms-dos-executable;application/x-msi;'}]);DEB.appsAt=0;DEB.st=null;debApps()"); await wait(500);
  await open('STARPORTGE_INSTALL.EXE'); await wait(1000); w = await W();
  ok('9. an installer (by its name): Install first, with its size', /A Windows installer/.test(w.txt) && w.btn[0] == 'Install' && /4\.77 MB/.test(w.txt), w);
  await click('Install'); await wait(7500); w = await W();
  const L = await q("T.launched");
  ok('10. Install runs it with Wine (the file\'s path for Linux) and offers the program it installed', L.some(x => x[0] == 'wine' && x[1] == '/home/rudolf/Downloads/StarportGE_install.exe') && /It installed this program/.test(w.txt) && /Starport GE/.test(w.txt), [L, w]);
  await open('GAME.EXE'); await wait(1000); w = await W();
  ok('11. another .exe is a program: Run first', /A Windows program/.test(w.txt) && w.btn[0] == 'Run', w);
  ok('12. no page errors', !errs.length, errs);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
