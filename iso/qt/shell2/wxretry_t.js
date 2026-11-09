// 1.5.6: the Weather widget tries again soon after a failed look-up (at start-up the desktop is often ready before
// the Wi-Fi: on the Vivobook it then said "Weather needs an internet connection" for 15 minutes), and at once when
// the computer comes online.   node wxretry_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8147, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd|debian/.test(e.stack || e.message || '')) errs.push(e.message) });
  // Open-Meteo stand-in: offline until NET is set
  let NET = false, CALLS = 0;
  await p.route(/open-meteo\.com/, r => { CALLS++; if (!NET) return r.abort('internetdisconnected');
    r.fulfill({contentType: 'application/json', body: JSON.stringify({current: {temperature_2m: 7, weather_code: 0, wind_speed_10m: 5}})}) });
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8147/'); await p.waitForFunction(() => typeof wgDraw == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    ACCTS.length=0;ACCTS.push({n:'boss',h:await pwMake('b1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'b1');
    svj('bd95wloc',{lat:41.88,lon:-87.63,name:'Chicago',cc:'US'});WG=[{t:'weather',x:500,y:40}];wgSave();wgDraw();return 1})()`);
  const txt = () => q("(()=>{const e=document.querySelector('[data-wx]');return e?e.textContent:''})()");
  await wait(1500);
  ok('1. offline at first: it says so, and that it will try again', /needs an internet connection\. Trying again/.test(await txt()), await txt());
  NET = true; const c0 = CALLS; await wait(21000);
  ok('2. it tries again by itself within about 20 s and shows the weather', CALLS > c0 && /7|45°F/.test(await txt()) && !/internet/.test(await txt()), [CALLS, await txt()]);
  // offline again, then the computer comes online: at once
  NET = false; await q("document.querySelector('[data-wx]')._wx()"); await wait(800);
  ok('3. offline again: the message comes back', /internet connection/.test(await txt()), await txt());
  NET = true; await q("dispatchEvent(new Event('online'))"); await wait(1200);
  ok('4. "online": the weather is shown at once, not after the next try', /°F/.test(await txt()), await txt());
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
