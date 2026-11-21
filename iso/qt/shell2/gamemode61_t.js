// 2026-11-21 (docs/plan-1.6.1.md Part 19): game mode in the page (gamemode61.js) with a stand-in rudolfd: a Linux game full
// screen in front (a stand-in window) is said to root every 2 s (game/on?src=page&kind=linux), the page's own animations and
// the dynamic background stop, notifications are held and one says how many after, game/off when it ends and everything is
// back; a page's own full screen is kind=page and stops nothing of the page; a full screen only the native desktop sees
// (rudolfd's status: src=native) holds this page's notifications and background work too; a GameMode D-Bus request alone
// does not; Settings › System health › Memory › Game mode turns it off (and tells root: game/set).   node gamemode61_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const M = {calls: [], src: [], games: [], enabled: true};
const status = () => ({ok: true, enabled: M.enabled, on: M.enabled && (M.src.length > 0 || M.games.length > 0), throttled: M.src.some(s => s.kind == 'linux'), sources: M.src, games: M.games, cpu: ''});
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0); let body = '';
  q.on('data', d => body += d); q.on('end', async () => {
    const J = (o, c) => { s.writeHead(c || 200, {'content-type': 'application/json'}); s.end(JSON.stringify(o)) };
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/events/wait') { await new Promise(r => setTimeout(r, 800)); s.writeHead(200); return s.end('{"ok":true,"events":""}') }
    if (u.startsWith('/api/game/')) { const a = new URLSearchParams(qs || ''); M.calls.push(u.slice(10) + (qs ? '?' + qs : ''));
      if (u == '/api/game/on') { M.src = M.src.filter(x => x.src != a.get('src')); M.src.push({src: a.get('src'), kind: a.get('kind'), pid: null}) }
      if (u == '/api/game/off') M.src = M.src.filter(x => x.src != a.get('src'));
      if (u == '/api/game/set') M.enabled = a.get('enabled') == '1';
      return J(status()) }
    if (u == '/api/apps/windows') return J({ok: true, mode: M.lx ? 'win:1' : 'desktop', windows: M.lx ? [{win: 1, shown: true, fullscreen: true, title: 'Stand-in game'}] : []});
    if (u.startsWith('/api/')) return J({ok: true});
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8747, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  const p = await b.newPage({viewport: {width: 1280, height: 800}}); const errs = []; p.on('pageerror', e => errs.push(String(e.stack || e).slice(0, 300)));
  await p.goto('http://127.0.0.1:8747/'); await p.waitForFunction(() => typeof window.__gamemode61 != 'undefined' && typeof healthPanel == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});USER=USER||ACCTS[0];USER.r='admin';window.__pops=0;const nc=__notifCentre,a0=nc.add;return 1})()");
  const held = () => q('GAME_HELD'); const gm = () => q('gameMode()'); const cls = () => q("document.documentElement.classList.contains('gm61')");
  await wait(2500);
  ok('1. before: game mode off, nothing said to root', !(await gm()) && !M.calls.some(c => c.startsWith('on')), M.calls);
  // a Linux game full screen in front (as rudolfd's apps/windows says)
  M.lx = true; await q('lxWinPoll()'); await wait(4500);
  ok('2. a Linux game full screen: game mode on, said to root as src=page kind=linux, again every 2 s', (await gm()) && M.calls.filter(c => c == 'on?src=page&kind=linux').length >= 2, M.calls);
  ok('3. ...the page\'s own animations stopped (html.gm61), the dynamic background\'s too', (await cls()) && (await q("(()=>{dynOvCheck();return DYNOV.vis===false})()")));
  const h0 = await held(); await q("notify('Mail','A new message')"); await q("notify('Calendar','Lunch in 10 minutes')");
  ok('4. ...notifications held (kept in the centre, no pop-up)', (await held()) == h0 + 2, [h0, await held()]);
  await q("(()=>{window.__nt=[];const n=__notifCentre.add.bind(__notifCentre);__notifCentre.add=(t,bb,o)=>{__nt.push(t);return n(t,bb,o)};return 1})()");
  M.lx = false; await q('lxWinPoll()'); await wait(3000);
  ok('5. the game leaves full screen: game/off said, game mode off, the animations back', M.calls.includes('off?src=page') && !(await gm()) && !(await cls()), M.calls.slice(-4));
  ok('6. ...one notification says how many came while playing', (await q('__nt')).includes('While you were playing'), await q('__nt'));
  // a page's own full screen: kind=page, nothing of the page stops
  M.calls = []; await q("(()=>{GAME_FS.add(9);gameModeTick();return 1})()"); await wait(2500);
  ok('7. a page full screen (a game in an Oracle tab): kind=page, the page\'s animations not stopped', M.calls.includes('on?src=page&kind=page') && !(await cls()), M.calls);
  await q("(()=>{GAME_FS.delete(9);gameModeTick();return 1})()"); await wait(2500);
  // the native desktop's full screen only (rudolfd says so)
  M.calls = []; M.src = [{src: 'native', kind: 'linux', pid: 77}]; await wait(3500);
  ok('8. a full screen only the native desktop sees (rudolfd: src=native): this page\'s game mode on too (notifications, background work)', (await gm()) && (await cls()), [await gm(), M.src]);
  ok('...and the page does not claim it as its own', !M.calls.some(c => c.startsWith('on')), M.calls);
  M.src = []; await wait(3500); ok('9. ...over: off again', !(await gm()));
  // a GameMode D-Bus request alone (a game windowed): the processor only
  M.games = [{pid: 88, name: 'supertux2'}]; await wait(3500);
  ok('10. a game that only asked through GameMode (not full screen): no notifications held, nothing of the page stopped', !(await gm()) && !(await cls()));
  M.games = [];
  // the setting
  await q("(()=>{wins.slice().forEach(w=>w.close());settings('health');return 1})()"); await wait(1500);
  const box = await q("(()=>{const c=document.querySelector('#gm61ck');return c?{on:c.checked,txt:c.closest('.gm61s').textContent}:null})()");
  ok('11. Settings › System health › Memory › Game mode: the switch, on, what it does', box && box.on && /processor runs at full speed/.test(box.txt), box);
  await q("(()=>{const c=document.querySelector('#gm61ck');c.checked=false;c.onchange({target:c});return 1})()"); await wait(800);
  ok('12. turned off: root told (game/set?enabled=0), the setting kept', M.calls.includes('set?enabled=0') && (await q('S.gamemode')) === false, M.calls);
  M.lx = true; await q('lxWinPoll()'); await wait(3000);
  ok('13. ...a Linux game full screen now changes nothing', !(await gm()) && !(await cls()) && !M.calls.some(c => c.startsWith('on')), M.calls);
  M.lx = false; await q('lxWinPoll()');
  ok('14. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('FAIL', e); process.exit(1) });
