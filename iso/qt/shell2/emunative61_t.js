// 1.6.1 (emu26.js): Nintendo 64 and Dreamcast games in Emu, played by native emulators (Linux apps from Flathub:
// Rosalie's Mupen GUI, Flycast). The library shows them under their tabs (a Dreamcast .gdi disc by its folder's name),
// "Add games" lists their files, and playing one asks to download its emulator once (Flathub, through rudolfd's
// native/install job), then starts it with the game (native/run?id=…&file=…); something else open full screen is
// closed first when the player says so. The desktop page with a stand-in rudolfd, in Chromium. Made-up files only.
//   node emunative61_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
const G = '/home/rudolf/Games';
const DIRS = {[G + '/N64']: [{name: 'Made_Up  Racer (USA).z64', dir: false, size: 8388608, mtime: 1}, {name: 'readme.txt', dir: false, size: 3, mtime: 1}],
  [G + '/DC']: [{name: 'Made Up Adventure', dir: true, size: 0, mtime: 1}, {name: 'Crazy Made Up.cdi', dir: false, size: 700000000, mtime: 1}, {name: 'Empty Folder', dir: true, size: 0, mtime: 1}],
  [G + '/DC/Made Up Adventure']: [{name: 'Made Up Adventure.gdi', dir: false, size: 120, mtime: 1}, {name: 'track01.bin', dir: false, size: 1000000, mtime: 1}, {name: 'track03.bin', dir: false, size: 900000000, mtime: 1}],
  [G + '/DC/Empty Folder']: [{name: 'notes.txt', dir: false, size: 1, mtime: 1}],
  '/home/rudolf/Downloads': [{name: 'a.z64', dir: false, size: 9, mtime: 1}, {name: 'b.gdi', dir: false, size: 9, mtime: 1}, {name: 'c.chd', dir: false, size: 9, mtime: 1}, {name: 'd.cdi', dir: false, size: 9, mtime: 1}, {name: 'track01.bin', dir: false, size: 9, mtime: 1}, {name: 'e.n64', dir: false, size: 9, mtime: 1}]};
const CALLS = []; const NAT = {'org.flycast.Flycast': false, 'com.github.Rosalie241.RMG': true}; let running = false;
const api = (a, q, method) => { const sp = new URLSearchParams(q || '');
  if (a.startsWith('native/')) CALLS.push({a, id: sp.get('id'), file: sp.get('file'), method});
  if (a == 'fs/list') return {ok: true, items: DIRS[sp.get('path')] || []};
  if (a == 'games/info') return {ok: true, used: 1000, free: 5e10, persistent: true};
  if (a == 'native/status') return {ok: true, flatpak: true, installed: NAT[sp.get('id')], running, persistent: true, free: 5e10, info: ''};
  if (a == 'native/install') { NAT[sp.get('id')] = true; return {ok: true, msg: 'started'} }
  if (a == 'native/run') { running = true; return {ok: true, msg: 'started'} }
  if (a == 'native/stop') { running = false; return {ok: true, msg: 'stopped'} }
  if (a == 'job') return {ok: true, running: false, status: 0, log: 'Adding the Flathub app source...\nDownloading org.flycast.Flycast and its runtime...\nDone.\n'};
  if (a == 'disks') return {ok: true, disks: []};
  if (a == 'pkg/status') return {ok: true, installed: [], available: [], sources: []};
  return {ok: true} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs, q.method))) }
    s.writeHead(404); s.end() }) }).listen(8099, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8099/'); await p.waitForFunction(() => typeof emuLibrary == 'function' && typeof fs != 'undefined' && fs['C:'], null, {timeout: 30000});
  await p.evaluate(() => { try { svj('bd95setup', 1); svj('bd95emutab', 'all') } catch (e) {} ['oobe', 'login'].forEach(i => { const o = document.getElementById(i); o && o.remove() }); USER = USER || ACCTS[0]; wins.slice().forEach(w => w.close && w.close());
    window.__notes = []; const n0 = notify; notify = (t, m, o) => { window.__notes.push(t + ' | ' + m); return n0(t, m, o) }; elevate = async () => true; appDownloadOk = async () => true });
  await p.evaluate(() => emuLibrary()); await wait(1500);
  const tab = async t => { await p.evaluate(t => wins.at(-1).body.querySelector(`.eltabs [data-t='${t}']`).click(), t); await wait(300); return p.evaluate(() => [...wins.at(-1).body.querySelectorAll('.eltile')].map(x => x.innerText.replace(/\s+/g, ' '))) };
  const tabs = await p.evaluate(() => [...wins.at(-1).body.querySelectorAll('.eltabs button')].map(x => x.textContent));
  ok('1. the library has Nintendo 64 and Dreamcast tabs', tabs.includes('Nintendo 64') && tabs.includes('Dreamcast'), tabs);
  const n64 = await tab('n64'); ok('2. the Nintendo 64 tab shows its game with a tidy name (not the text file)', n64.length == 1 && /Made Up Racer \(USA\).*N64/.test(n64[0]), n64);
  const dc = await tab('dc'); ok('3. the Dreamcast tab shows a .cdi and a .gdi disc in its folder (by the folder\'s name, its tracks counted), not an empty folder', dc.length == 2 && dc.some(t => /Made Up Adventure Dreamcast · 859/.test(t)) && dc.some(t => /Crazy Made Up/.test(t)), dc);
  // a Dreamcast game: Flycast is not there yet; asked, downloaded, then started with the .gdi
  await p.evaluate(() => [...wins.at(-1).body.querySelectorAll('.eltile')].find(t => /Made Up Adventure/.test(t.innerText)).click()); await wait(800);
  const ask = await p.evaluate(() => { const d = document.querySelector('.natdlg'); return d ? d.innerText.replace(/\s+/g, ' ') : '' });
  ok('4. playing a Dreamcast game the first time asks to download Flycast (free, Flathub, the graphics card)', /Play Sega Dreamcast games\?/.test(ask) && /Flycast/.test(ask) && /Flathub/.test(ask), ask);
  await p.evaluate(() => document.querySelector('.natdlg .g').click()); await wait(3500);
  const c1 = CALLS.filter(c => c.a != 'native/status');
  ok('5. ...then rudolfd installs it from Flathub and starts it with the game\'s .gdi', c1.length == 2 && c1[0].a == 'native/install' && c1[0].id == 'org.flycast.Flycast' && c1[1].a == 'native/run' && c1[1].id == 'org.flycast.Flycast' && c1[1].file == G + '/DC/Made Up Adventure/Made Up Adventure.gdi', c1);
  const notes = await p.evaluate(() => window.__notes.join(' / '));
  ok('6. ...saying it opens full screen, how to use it, and Super+Esc', /Starting Made Up Adventure/.test(notes) && /Tab opens Flycast/.test(notes) && /Super\+Esc/.test(notes), notes);
  // an N64 game with something already open full screen: asked, closed, then started
  CALLS.length = 0; await tab('n64');
  await p.evaluate(() => wins.filter(w => w.body.classList.contains('emulib')).at(-1).body.querySelector('.eltile').click()); await wait(800);
  const ask2 = await p.evaluate(() => { const d = document.querySelector('.natdlg'); return d ? d.innerText.replace(/\s+/g, ' ') : '' });
  ok('7. with something open full screen already, it asks to close it first (Rosalie\'s Mupen GUI is installed: no download)', /Close what is open full screen\?/.test(ask2) && !/Download/.test(ask2), ask2);
  await p.evaluate(() => document.querySelector('.natdlg .g').click()); await wait(2500);
  const c2 = CALLS.filter(c => c.a != 'native/status');
  ok('8. ...then it is closed and Rosalie\'s Mupen GUI starts with the .z64', c2.length == 2 && c2[0].a == 'native/stop' && c2[1].a == 'native/run' && c2[1].id == 'com.github.Rosalie241.RMG' && c2[1].file == G + '/N64/Made_Up  Racer (USA).z64', c2);
  const rec = await tab('recent'); ok('9. both are in Recently played', rec.length == 2, rec);
  await p.evaluate(() => emuAddGames()); await wait(1200);
  const add = await p.evaluate(() => [...wins.at(-1).body.querySelectorAll('.earow')].map(r => r.innerText.trim()));
  ok('10. "Add games" lists Nintendo 64 (.z64, .n64) and Dreamcast (.gdi, .cdi, .chd) files, not loose tracks', add.length == 5 && ['a.z64', 'b.gdi', 'c.chd', 'd.cdi', 'e.n64'].every(n => add.some(r => r.includes(n))) && !add.some(r => /track01/.test(r)), add);
  ok('11. no page errors', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1)
})().catch(e => { console.log('FAIL ' + e.stack); process.exit(1) });
