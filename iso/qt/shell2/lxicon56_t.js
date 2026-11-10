// 1.5.6: a Linux app's icon that could not be had once is asked for again. On the Vivobook Discord's and Steam's
// icons stayed the penguin for the whole session: the first request failed (start-up, sign-in) and the failure was
// kept.   node lxicon56_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8156, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
const PNG = 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==';
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd|debian/.test(e.stack || e.message || '')) errs.push(e.message) });
  await p.addInitScript(png => {
    window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0};
    window.T = {icon: 0, fail: 2, ids: {}};
    const f0 = window.fetch;
    window.fetch = (u, o) => { u = String(u); const J = (x, st) => Promise.resolve(new Response(JSON.stringify(Object.assign({ok: !st}, x)), {status: st || 200, headers: {'content-type': 'application/json'}}));
      const q = new URL(u, location.href);
      if (q.pathname == '/api/apps/icon') { const id = q.searchParams.get('id'); T.ids[id] = (T.ids[id] || 0) + 1; if (!/^org\./.test(id) && id != T.only) return J({error: 'no icon'}, 404); T.icon++; if (T.fail > 0) { T.fail--; return J({error: 'no icon'}, 404) }
        return Promise.resolve(new Response(Uint8Array.from(atob(png), c => c.charCodeAt(0)), {headers: {'content-type': 'image/png'}})) }
      if (q.pathname == '/api/apps/installed') return J({apps: [{id: 'com.discordapp.Discord', name: 'Discord', launchers: [{desktop: 'com.discordapp.Discord', name: 'Discord', icon: 'com.discordapp.Discord'}]}]});
      if (u.includes('/api/')) return J({items: [], apps: []});
      return f0(u, o) } }, PNG);
  await p.goto('http://127.0.0.1:8156/'); await p.waitForFunction(() => typeof lxImg == 'function' && typeof LX_ICON_RETRY != 'undefined', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{LX_ICON_RETRY[0]=150;LX_ICON_RETRY[1]=300;LX_ICON_RETRY[2]=300;T.only='com.discordapp.Discord';LX.icons.clear();T.icon=0;T.fail=2;const d=document.createElement('div');d.id='ic1';d.textContent='🐧';document.body.appendChild(d);lxImg(d,'com.discordapp.Discord','com.discordapp.Discord');return 1})()");
  await wait(200);
  ok('1. the first request fails: the penguin for now', await q("!document.querySelector('#ic1 img')&&document.querySelector('#ic1').textContent=='🐧'"));
  ok('2. the failure is not kept', await q("LX.icons.size") == 0, await q("LX.icons.size"));
  await wait(1200);
  ok('3. asked for again, the icon replaces the penguin', await q("!!document.querySelector('#ic1 img.lxapp')") && await q("T.icon") == 3, [await q("T.icon"), await q("document.querySelector('#ic1').innerHTML")]);
  // an icon that is never there: tried a few times, then left (no endless requests)
  await q("(()=>{T.only='';LX.icons.clear();T.icon=0;T.fail=99;LX_ICON_RETRY[3]=300;const d=document.createElement('div');d.id='ic2';d.textContent='🐧';document.body.appendChild(d);lxImg(d,'org.none','org.none');return 1})()");
  await wait(2000);
  ok('4. an icon that is never there is asked for 5 times in all, then left as the penguin', await q("T.ids['org.none']") == 5 && await q("document.querySelector('#ic2').textContent=='🐧'"), await q("T.icon"));
  // a drawing taken off the screen stops asking
  await q("(()=>{LX.icons.clear();T.icon=0;T.fail=99;const d=document.createElement('div');d.id='ic3';document.body.appendChild(d);lxImg(d,'org.gone','org.gone');setTimeout(()=>d.remove(),50);return 1})()");
  await wait(1500);
  ok('5. an icon no longer on the screen is not asked for again', await q("T.ids['org.gone']") == 1, await q("T.ids"));
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
