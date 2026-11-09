// 1.5.6: Flathub's website Install links (flatpak+https://…/<id>.flatpakref, appstream://<id>) and downloaded
// .flatpakref files open that app's page in the App Store (found on the Vivobook: ERR_UNKNOWN_URL_SCHEME for Steam).
// node flatlink_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 500) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8145, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.addInitScript(() => {
    window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0};
    const f0 = window.fetch;
    window.fetch = (u, o) => { u = String(u); const J = x => Promise.resolve(new Response(JSON.stringify(Object.assign({ok: true}, x)), {headers: {'content-type': 'application/json'}}));
      if (u.includes('/api/apps/status')) return J({flatpak: 1, persistent: 1});
      if (u.includes('/api/apps/catalog')) return J({apps: [{id: 'com.valvesoftware.Steam', name: 'Steam', summary: 'Launcher for the Steam software distribution service', dev: 'Valve', verified: true, cats: ['Game']}, {id: 'org.gimp.GIMP', name: 'GIMP', summary: 'x', verified: true}]});
      if (u.includes('/api/apps/info')) return J({id: 'com.valvesoftware.Steam', description: 'Steam is a video game digital distribution service', size: 1000});
      if (u.includes('/api/')) return J({apps: [], items: []});
      return f0(u, o) } });
  await p.goto('http://127.0.0.1:8145/'); await p.waitForFunction(() => typeof lxStoreOpen == 'function' && FSLOADED && fs['C:'] && fs['C:'].USERS, null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>{w.__force=1;w.close()});USER=USER||ACCTS[0];if(!HOME){HOME={DOWNLOADS:{}};fs['C:'].USERS[uname()]=HOME}return 1})()");
  const ids = await q("['flatpak+https://dl.flathub.org/repo/appstream/com.valvesoftware.Steam.flatpakref','appstream://org.gimp.GIMP','appstream:org.gimp.GIMP.desktop','https://flathub.org/apps/x','flatpak+https://evil/../x.flatpakref','javascript:alert(1)'].map(lxStoreLinkId)");
  ok('1. the app is read from Flathub\'s links, and nothing else is taken for one', JSON.stringify(ids) == JSON.stringify(['com.valvesoftware.Steam', 'org.gimp.GIMP', 'org.gimp.GIMP', null, null, null]), ids);
  await q("lxStoreOpen('com.valvesoftware.Steam')"); await wait(1200);
  let t = await q("(()=>{const w=wins.at(-1);return {title:w.el.querySelector('.t').textContent,on:(w.body.querySelector('.tmtabs button.on')||{}).dataset?.t,txt:w.body.innerText.slice(0,400)}})()");
  ok('2. the link opens the App Store\'s Linux apps on Steam\'s page', /App Store/.test(t.title) && t.on == 'linux' && /Steam/.test(t.txt) && /Valve/.test(t.txt), t);
  await q("(()=>{wins.slice().forEach(w=>{w.__force=1;w.close()});node(downloadsPath())['COM.VALVESOFTWARE.STEAM.FLATPAKREF']='[Flatpak Ref]\\nName=com.valvesoftware.Steam\\nBranch=stable\\nUrl=https://dl.flathub.org/repo/\\n';openPath([...downloadsPath(),'COM.VALVESOFTWARE.STEAM.FLATPAKREF']);return 1})()"); await wait(1200);
  t = await q("(()=>{const w=wins.at(-1);return {title:w.el.querySelector('.t').textContent,txt:w.body.innerText.slice(0,300)}})()");
  ok('3. a downloaded .flatpakref opens the same page', /App Store/.test(t.title) && /Steam/.test(t.txt), t);
  ok('4. no page errors', !errs.length, errs);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
