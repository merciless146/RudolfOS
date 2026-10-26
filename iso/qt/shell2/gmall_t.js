// App Store › Games 1.2.10: every game in Flathub's app list (a game is an app with "Game" or a game genre), with a
// search box, genres and "Verified only", and Linux apps' Games category finding the same games. Stand-in rudolfd.
// node gmall_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../..', 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.mjs': 'text/javascript', '.html': 'text/html'};
const app = (id, name, cats, verified, summary) => ({id, name, summary: summary || name + ' summary', cats, verified: !!verified, dev: 'Dev'});
const CAT = [app('org.libreoffice.LibreOffice', 'LibreOffice', ['Office'], 1),
  app('space.oolite.Oolite', 'Oolite', ['Game', 'Simulation'], 1, 'Open-world space opera'),
  app('net.supertuxkart.SuperTuxKart', 'SuperTuxKart', ['Game', 'ArcadeGame', 'SportsGame'], 1, 'Kart racing'),
  app('org.example.OnlyGenre', 'Genre Only', ['ActionGame'], 1, 'Lists a genre but not Game'),
  app('org.example.Unverified', 'Unverified Chess', ['Game', 'BoardGame'], 0, 'A chess game'),
  app('org.gimp.GIMP', 'GIMP', ['Graphics'], 1),
  app('org.libretro.RetroArch', 'RetroArch', ['Game', 'Emulator'], 1)];
const M = {calls: []};
const api = (a, q) => { M.calls.push(a + (q ? '?' + q : ''));
  if (a == 'apps/status') return {ok: true, flatpak: true, persistent: true};
  if (a == 'apps/catalog') return {ok: true, apps: CAT};
  if (a == 'apps/installed') return {ok: true, apps: [{id: 'space.oolite.Oolite', name: 'Oolite', version: '1.92.1', launchers: []}]};
  if (a == 'apps/info') return {ok: true, id: new URLSearchParams(q).get('id'), description: 'About it.', screenshots: [], permissions: {}};
  if (a == 'system') return {ok: true, renderer: 'AMD Radeon'};
  return {ok: true} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0); q.on('data', () => {}); q.on('end', () => {
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
  if (u == '/api/apps/icon') { s.writeHead(404); return s.end() }
  if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs))) }
  const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(8097, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage(); p.on('pageerror', e => console.log('page error:', e.message));
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8097/'); await p.waitForFunction(() => typeof appStore == 'function' && typeof gamesAll == 'function', null, {timeout: 30000});
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';return 1})()");
  await q("(()=>{const W=appStore();W.showGames();return 1})()");
  await p.waitForSelector('.gmall .gmq', {timeout: 10000}).catch(() => 0); await wait(400);
  const names = () => q("[...document.querySelectorAll('.gmall .gml .lxrow b')].map(x=>x.textContent)");
  let n = await names();
  ok('Games has "All games on Flathub" with a search box and genres', await q("!!document.querySelector('.gmall .gmq')&&[...document.querySelectorAll('.gmall .gmg option')].map(o=>o.textContent).join('|')") == 'All genres|Action|Arcade|Board|Simulation|Sports|Emulators', await q("[...document.querySelectorAll('.gmall .gmg option')].map(o=>o.textContent).join('|')"));
  ok('...every verified game, the installed one first, no other apps (one listing only a genre counts as a game)', JSON.stringify(n) == JSON.stringify(['Oolite', 'Genre Only', 'RetroArch', 'SuperTuxKart']), n);
  ok('...marked installed, with their genres', /Installed/.test(await q("document.querySelector('.gmall .gml .lxrow').innerText")) && /Arcade, Sports/.test(await q("[...document.querySelectorAll('.gmall .lxrow')].find(r=>/SuperTux/.test(r.innerText)).innerText")));
  await q("(()=>{const i=document.querySelector('.gmall .gmq');i.value='kart';i.oninput();return 1})()"); await wait(400); n = await names();
  ok('Search narrows them (by name or description)', JSON.stringify(n) == '["SuperTuxKart"]', n);
  await q("(()=>{const i=document.querySelector('.gmall .gmq');i.value='';i.oninput();const g=document.querySelector('.gmall .gmg');g.value='Emulator';g.onchange();return 1})()"); await wait(400); n = await names();
  ok('A genre narrows them', JSON.stringify(n) == '["RetroArch"]', n);
  await q("(()=>{const g=document.querySelector('.gmall .gmg');g.value='';const v=document.querySelector('.gmall .gmv');v.checked=false;v.onchange();return 1})()"); await wait(400); n = await names();
  ok('Verified only can be turned off', n.includes('Unverified Chess') && n.length == 5, n);
  ok('...and the footer counts them', /5 games from Flathub/.test(await q("document.querySelector('.gmall .gmfoot').textContent")));
  await q("[...document.querySelectorAll('.gmall .lxrow')].find(r=>/SuperTux/.test(r.innerText)).click()"); await wait(800);
  ok('A game opens its page, with "‹ Games" to go back', /‹ Games/.test(await q("document.querySelector('.lxback').textContent")) && /SuperTuxKart/.test(await q("document.querySelector('.lxhead').innerText")));
  await q("document.querySelector('.lxback').click()"); await p.waitForSelector('.gmall .gmq', {timeout: 8000}).catch(() => 0);
  ok('...which goes back to Games', !!(await q("!!document.querySelector('.gmall .gmq')")));
  await q("(()=>{const W=wins.find(w=>w.app=='store');[...W.body.querySelectorAll('.tmtabs button')].find(x=>x.dataset.t=='linux').click();return 1})()");
  await p.waitForSelector('.lxc', {timeout: 8000}).catch(() => 0);
  await q("(()=>{const c=document.querySelector('.lxc');c.value='Game';c.onchange();return 1})()"); await wait(400);
  const ln = await q("[...document.querySelectorAll('.lxlist .lxrow b')].map(x=>x.textContent)");
  ok('Linux apps › Games finds the same games (a genre alone counts too)', JSON.stringify([...ln].sort()) == JSON.stringify(['Genre Only', 'Oolite', 'RetroArch', 'SuperTuxKart']), ln);
  await b.close(); srv.close();
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
