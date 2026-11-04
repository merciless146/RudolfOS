// 1.5.2 (lib52.js): the Rudolf OS Library on the desktop: the App Store tab, downloading an app's package when it is
// first needed (Emu's DS player, Quake's free data), and Emu coming back by itself on computers that played DS games.
// The desktop page with a stand-in rudolfd (rpk's status as rpk prints it) and the Library's catalog, in Chromium.
//   node lib52_t.js [CHROME]   (build iso/www/index.html first; the DS core is read from a rudolfos-library checkout
//   next to this repository, ../rudolfos-library/apps/rudolf-emu/www, when there is one)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www'), LIBW = path.join(ROOT, '../rudolfos-library/apps/rudolf-emu/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
const LIB = 'https://merciless146.github.io/rudolfos-library';
const AV = {
  'rudolf-quake': {name: 'rudolf-quake', version: '1.0.0', kind: 'addon', size: 21090376, title: 'Quake', summary: 'The classic first-person shooter', icon: '🎯', license: 'GPL-2.0-or-later (engine), BSD-3-Clause (LibreQuake data)', source: LIB, library: true, installed: null},
  'rudolf-emu': {name: 'rudolf-emu', version: '1.0.0', kind: 'addon', size: 364616, title: 'Emu', summary: 'Plays Nintendo DS and Super Nintendo games you own', icon: '🕹️', license: 'GPL-2.0-or-later (DeSmuME), MIT (LakeSnes)', source: LIB, library: true, installed: null},
  'jq': {name: 'jq', version: '1.7', kind: 'cli', size: 300000, title: 'jq', summary: 'JSON', icon: '', license: 'MIT', source: '/live/media/rudolf/pkg', library: false, installed: null}};
const M = {inst: {}, calls: [], dsGames: ['Mario.nds'], job: {running: false, status: 0, log: ''}};
const status = () => ({ok: true, persistent: true, sources: [{loc: '/live/media/rudolf/pkg', type: 'disc', ok: true, library: false}, {loc: LIB, type: 'web', ok: true, library: true, count: 2}],
  installed: Object.values(M.inst), available: Object.values(AV).map(a => ({...a, installed: M.inst[a.name] ? M.inst[a.name].version : null}))});
const api = (a, q) => {
  M.calls.push(a + (q ? '?' + q : ''));
  const n = new URLSearchParams(q || '').get('name');
  if (a == 'pkg/status') return status();
  if (a == 'pkg/install') { const x = AV[n]; M.inst[n] = {name: n, version: x.version, kind: x.kind, title: x.title, summary: x.summary, icon: x.icon, license: x.license, signed: x.library ? 'library' : 'system', mounted: true, update: null}; M.job = {running: false, status: 0, log: 'Installed ' + n + ' ' + x.version}; return {ok: true} }
  if (a == 'pkg/remove') { delete M.inst[n]; M.job = {running: false, status: 0, log: 'Removed ' + n}; return {ok: true} }
  if (a == 'job') return {ok: true, ...M.job};
  if (a == 'fs/list') return {ok: true, items: M.dsGames.map(x => ({name: x, dir: false, size: 1, mtime: 1}))};
  if (a == 'apps/status') return {ok: true, flatpak: true, persistent: true};
  if (a == 'apps/installed') return {ok: true, apps: []};
  if (a == 'system') return {ok: true, renderer: 'AMD'};
  if (a == 'events/wait') return {ok: true, events: []};
  return {ok: true};
};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200, {'content-type': 'application/json'}); s.end('{"ok":true,"events":[]}') }, 20000);
      s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs))) }
    // installed packages are served at /pkg/<name>/www/
    let m = u.match(/^\/pkg\/(rudolf-emu|rudolf-quake)\/www\/(.+)$/);
    if (m && M.inst[m[1]]) {
      if (m[1] == 'rudolf-emu' && fs.existsSync(path.join(LIBW, m[2]))) { s.writeHead(200, {'content-type': 'application/wasm'}); return fs.createReadStream(path.join(LIBW, m[2])).pipe(s) }
      if (m[1] == 'rudolf-quake' && m[2] == 'manifest.json') { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify({title: 'LibreQuake demo data', files: [{name: 'TEST.TXT', dir: '', path: 't.txt'}]})) }
      if (m[1] == 'rudolf-quake' && m[2] == 't.txt') { s.writeHead(200); return s.end('hello') }
    }
    s.writeHead(404); s.end() }) }).listen(8097, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.route(LIB + '/catalog.json', r => r.fulfill({contentType: 'application/json', body: JSON.stringify({catalog: 1, apps: [
    {name: 'rudolf-quake', description: 'Quake’s engine with LibreQuake: a complete, free replacement for Quake’s game data.', download: 21090376},
    {name: 'rudolf-emu', description: 'The emulator cores of Emu. No games, BIOS files or keys are included.', download: 364616}]})}));
  await p.goto('http://127.0.0.1:8097/'); await p.waitForFunction(() => typeof appStore == 'function' && typeof libTab == 'function' && typeof fs != 'undefined' && fs['C:'], null, {timeout: 30000});
  await p.evaluate(() => { try { svj('bd95setup', 1) } catch (e) {} ['oobe', 'login'].forEach(i => { const o = document.getElementById(i); o && o.remove() }); USER = USER || ACCTS[0]; wins.slice().forEach(w => w.close()); localStorage.setItem('bd95lib52', JSON.stringify({done: 1})) });
  const dlgBtn = async re => { await p.waitForSelector('.natdlg', {timeout: 5000}).catch(() => {}); return p.evaluate(re => { const d = document.querySelector('.natdlg'); if (!d) return null; const t = d.innerText; [...d.querySelectorAll('button')].find(x => new RegExp(re).test(x.textContent)).click(); return t }, re) };
  // the tab
  await p.evaluate(() => { const W = appStore(); W.showLibrary() }); await wait(1200);
  const tab = await p.evaluate(() => { const W = wins.at(-1), tabs = [...W.body.querySelectorAll('.tmtabs button')].map(x => x.textContent); return {tabs, cards: [...W.body.querySelectorAll('.lib52c')].map(c => c.innerText.replace(/\s+/g, ' '))} });
  ok('1. App Store has a Rudolf OS Library tab, after Games', tab.tabs.indexOf('Rudolf OS Library') == tab.tabs.indexOf('Games') + 1, tab.tabs);
  ok('2. it lists the Library\'s apps only (not the disc\'s jq), with the catalog\'s description, size and licence', tab.cards.length == 2 && /Quake.*21 MB download.*free replacement.*BSD-3-Clause.*version 1\.0\.0.*Get/.test(tab.cards.find(c => /Quake/.test(c))) && /Emu.*365 KB download/.test(tab.cards.find(c => /Emu/.test(c))), tab.cards);
  await p.evaluate(() => [...wins.at(-1).body.querySelectorAll('.lib52c')].find(c => /Quake/.test(c.innerText)).querySelector('button').click()); await wait(2500);
  const t3 = await p.evaluate(() => [...wins.at(-1).body.querySelectorAll('.lib52c')].find(c => /Quake/.test(c.innerText)).innerText.replace(/\s+/g, ' '));
  ok('3. Get installs it (rpk install), and it then shows Open and Remove', M.calls.includes('pkg/install?name=rudolf-quake') && /installed/.test(t3) && /Open/.test(t3) && /Remove/.test(t3), t3);
  M.inst['rudolf-quake'].update = '1.1.0'; await p.evaluate(() => libTab(wins.at(-1).body.querySelector('.stb'))); await wait(800);
  const t4 = await p.evaluate(() => [...wins.at(-1).body.querySelectorAll('.lib52c')].find(c => /Quake/.test(c.innerText)).innerText.replace(/\s+/g, ' '));
  ok('4. a newer version in the Library shows Update', /update to 1\.1\.0/.test(t4) && /Update/.test(t4), t4);
  M.inst['rudolf-quake'].update = null;
  await p.evaluate(() => libTab(wins.at(-1).body.querySelector('.stb'))); await wait(800);
  p.evaluate(() => [...[...wins.at(-1).body.querySelectorAll('.lib52c')].find(c => /Quake/.test(c.innerText)).querySelectorAll('button')].find(x => x.textContent == 'Remove').click());
  const q5 = await dlgBtn('^Remove$'); await wait(2500);
  ok('5. Remove asks first (games and saves stay), then removes it', /Games and saves .* stay/.test(q5 || '') && M.calls.includes('pkg/remove?name=rudolf-quake') && !M.inst['rudolf-quake'], q5);
  await p.evaluate(() => wins.slice().forEach(w => w.close()));
  // Emu: SNES plays without the package; DS asks for it
  M.calls.length = 0;
  const snes = await p.evaluate(async () => { try { const m = await emuModule('snes'); return m instanceof WebAssembly.Module } catch (e) { return e.message } });
  ok('6. SNES games play without the package (the inlined core), and nothing is downloaded', snes === true && !M.calls.some(c => /pkg\/install/.test(c)) && !(await p.evaluate(() => !!document.querySelector('.natdlg'))), snes);
  const dsP = p.evaluate(async () => { try { const m = await emuModule('ds'); return m instanceof WebAssembly.Module ? 'module' : 'no' } catch (e) { return 'ERR ' + e.message } });
  const q7 = await dlgBtn('^Download$');
  ok('7. a DS game asks to download Emu\'s DS player from the Library, with its size', /Emu needs a download/.test(q7 || '') && /Nintendo DS player is in the Rudolf OS Library \(365 KB\)/.test(q7 || ''), q7);
  const ds = await dsP;
  ok('8. ...Download installs rudolf-emu, and the DS core is loaded from /pkg/rudolf-emu/www', M.calls.includes('pkg/install?name=rudolf-emu') && (fs.existsSync(LIBW) ? ds == 'module' : /ERR|module/.test(ds)), ds);
  // Quake's free data: asks; Not now leaves a message that says where to get it
  delete M.inst['rudolf-quake'];
  const qP = p.evaluate(async () => { try { await qGetFree(() => {}); return 'got it' } catch (e) { return e.message } });
  await dlgBtn('^Not now$'); const q9 = await qP;
  ok('9. Quake\'s free data: Not now says where to get it, and downloads nothing', /App Store › Rudolf OS Library/.test(q9) && !M.calls.includes('pkg/install?name=rudolf-quake'), q9);
  const qP2 = p.evaluate(async () => { try { const m = await qGetFree(() => {}); return m.title } catch (e) { return 'ERR ' + e.message } });
  await dlgBtn('^Download$'); const q10 = await qP2;
  ok('10. ...Download installs rudolf-quake, and the data is read from /pkg/rudolf-quake/www', q10 == 'LibreQuake demo data' && M.calls.includes('pkg/install?name=rudolf-quake'), q10);
  // nobody loses an app
  delete M.inst['rudolf-emu']; M.calls.length = 0;
  await p.evaluate(async () => { localStorage.removeItem('bd95lib52'); await libMigrate() }); await wait(1500);
  const n11 = await p.evaluate(() => NC.map(n => n.t + ' | ' + n.b).join('\n'));
  ok('11. a computer with DS games gets Emu\'s DS player back by itself, and is told', M.calls.includes('pkg/install?name=rudolf-emu') && /DS player now comes from the Rudolf OS Library/.test(n11), n11);
  M.calls.length = 0; await p.evaluate(() => libMigrate()); await wait(500);
  ok('12. ...only once', !M.calls.some(c => /pkg\/install/.test(c)), M.calls);
  delete M.inst['rudolf-emu']; M.dsGames = []; M.calls.length = 0;
  await p.evaluate(async () => { localStorage.removeItem('bd95lib52'); await libMigrate() }); await wait(500);
  ok('13. a computer without DS games downloads nothing', !M.calls.some(c => /pkg\/install/.test(c)) && /"done":1/.test(await p.evaluate(() => localStorage.getItem('bd95lib52'))), M.calls);
  ok('14. no page errors', !errs.length, errs);
  console.log(`${R.filter(x => x).length}/${R.length} passed`); await b.close(); srv.close(); process.exit(R.every(x => x) ? 0 : 1);
})();
