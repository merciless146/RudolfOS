// 1.5.6: a prompt that needs the person comes in front of the app or game on the screen, and the app comes back once
// it is answered (the development link's "Allow this command?" waited behind Among Us on the Vivobook; a remote
// connection's prompt is the same).   node front56_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8148, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd|debian/.test(e.stack || e.message || '')) errs.push(e.message) });
  await p.addInitScript(() => {
    window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0};
    window.T = {calls: [], mode: 'win 3a00001'};
    const f0 = window.fetch;
    window.fetch = (u, o) => { u = String(u); const J = x => Promise.resolve(new Response(JSON.stringify(Object.assign({ok: true}, x)), {headers: {'content-type': 'application/json'}}));
      const q = new URL(u, location.href);
      if (/\/api\/(apps|devlink\/answer|remote\/answer)/.test(q.pathname)) T.calls.push(q.pathname.slice(5) + q.search);
      if (q.pathname == '/api/apps/windows') return J({windows: [], mode: T.mode});
      if (q.pathname == '/api/apps/desktop') { T.mode = 'desktop'; return J({}) }
      if (q.pathname == '/api/apps/show') { T.mode = 'win ' + q.searchParams.get('win'); return J({}) }
      if (q.pathname == '/api/devlink') return J({available: true, on: false});
      if (u.includes('/api/')) return J({items: [], apps: []});
      return f0(u, o) } });
  await p.goto('http://127.0.0.1:8148/'); await p.waitForFunction(() => typeof devlAsk == 'function' && typeof rmAsk == 'function' && typeof fs != 'undefined' && fs['C:'], null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});return 1})()");
  // the development link asks while a game is in front
  await q("T.calls.length=0;devlAsk({on:true,repo:'me/link',pending:{id:'000300',kind:'sh',what:'ls'}})"); await wait(600);
  ok('1. "Allow this command?" brings the desktop (with the prompt) in front of the game', (await q("T.calls")).includes('apps/desktop') && await q("!!document.getElementById('devlinkAsk')"), await q("T.calls"));
  await q("document.querySelector('#devlinkAsk .dly').click()"); await wait(600);
  const c1 = await q("T.calls");
  ok('2. answered: the game comes back in front', c1.some(c => /^devlink\/answer\?id=000300&a=allow/.test(c)) && c1.includes('apps/show?win=3a00001'), c1);
  // a remote connection's prompt
  await q("T.calls.length=0;T.mode='win 4b00002';rmAsk({id:'r1',user:'ann',from:'10.0.0.5',time:Date.now()/1000})"); await wait(600);
  ok('3. "Allow a remote connection?" comes in front too', (await q("T.calls")).includes('apps/desktop'), await q("T.calls"));
  await q("document.querySelector('.rmodal .n').click()"); await wait(600);
  ok('4. ...and the app comes back once it is answered', (await q("T.calls")).includes('apps/show?win=4b00002'), await q("T.calls"));
  // with the desktop already in front nothing moves
  await q("T.calls.length=0;T.mode='desktop';devlAsk({on:true,repo:'me/link',pending:{id:'000301',kind:'sh',what:'ls'}})"); await wait(500);
  await q("document.querySelector('#devlinkAsk .dln').click()"); await wait(500);
  ok('5. the desktop in front already: no window is switched', !(await q("T.calls")).some(c => /apps\/(desktop|show)/.test(c)), await q("T.calls"));
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
