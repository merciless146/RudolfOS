// 1.5.0 (sec50.js): the firewall in Security Settings (the old URL rules renamed Website blocking), System health in
// Settings and in the system report, the desktop's own health events. A stand-in for rudolfd answers /api/*.
// node sec50_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 300) : '')) };
const FW = {on: true, blocked: 7, remote: false}, POSTS = [], now = Math.floor(Date.now() / 1000);
const EV = [{t: now - 60, k: 'ready', m: 'the desktop was ready 41 s after start-up'}, {t: now - 100, k: 'start', m: 'Rudolf OS 1.5.0 started (A)'},
  {t: now - 4000, k: 'memory', m: 'memory ran out: a web page using 1700 MB was closed'}, {t: now - 5000, k: 'crash', m: 'wine64-preloader stopped (segmentation fault)'},
  {t: now - 9 * 86400, k: 'crash', m: 'an old crash'}];
const api = (q, s, body) => {
  const u = new URL(q.url, 'http://x'), p = u.pathname.slice(5); let j = q.method == 'POST' ? {ok: true} : {ok: false, error: 'not in the stand-in'};
  if (q.method == 'POST') POSTS.push([p + u.search, body]);
  if (p == 'firewall') { if (q.method == 'POST') FW.on = u.searchParams.get('on') == '1'; j = {ok: true, ...FW} }
  else if (p == 'health' && q.method == 'GET') j = {ok: true, now, uptime: 200, events: EV.slice(0, +u.searchParams.get('n') || 200)};
  else if (p == 'events/wait') return setTimeout(() => { s.writeHead(200, {'content-type': 'application/json'}); s.end('{"ok":true,"events":[]}') }, 20000);
  s.writeHead(200, {'content-type': 'application/json'}); s.end(JSON.stringify(j));
};
const srv = http.createServer((q, s) => { let b = ''; q.on('data', d => b += d); q.on('end', () => {
  if (q.url.startsWith('/api/')) return api(q, s, b);
  s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', "<head><script>window.RUDOLFD={token:'t',base:'',boot:'uefi',media:'disk',live:0,recovery:0}</script>")) }) }).listen(8099, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto('http://127.0.0.1:8099/'); await p.waitForFunction(() => typeof fwSection == 'function' && typeof fs != 'undefined' && fs['C:'], null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});USER=ACCTS.find(a=>a.r=='admin')||ACCTS[0];window.elevate=async()=>true;return 1})()");
  await q("settings('sec')"); await wait(800);
  const sec = await q("(()=>{const c=SW.body.querySelector('.sc');return{h4:[...c.querySelectorAll('h4')].map(h=>h.textContent.trim()),stat:c.querySelector('.stat b').textContent,st:c.querySelector('.fws').textContent,ck:c.querySelector('.fwn').checked,txt:c.textContent}})()");
  ok('1. Security Settings has a Firewall section first, and the URL rules are called Website blocking', sec.h4[0] == 'Firewall' && sec.h4.includes('Website blocking') && sec.h4.filter(h => h == 'Firewall').length == 1 && sec.stat == 'Website blocking', sec.h4);
  ok('2. it shows the state and the blocked count from rudolfd', sec.ck && /On · 7 incoming connections blocked/.test(sec.st), sec.st);
  ok('3. the old check box says website blocking', /Turn on website blocking/.test(sec.txt) && !/Turn on the firewall/.test(sec.txt));
  await q("(()=>{const c=SW.body.querySelector('.fwn');c.checked=false;c.dispatchEvent(new Event('change'))})()"); await wait(600);
  ok('4. turning it off asks rudolfd (POST firewall?on=0) after an administrator allows it', POSTS.some(x => x[0] == 'firewall?on=0') && FW.on === false, POSTS);
  ok('5. ...and the page shows Off', await q("SW.body.querySelector('.fws').textContent.startsWith('Off')"));
  await q("settings('health')"); await wait(800);
  const h = await q("(()=>{const c=SW.body.querySelector('.sc');return{t:c.querySelector('h3').textContent,sum:c.querySelector('.hls').textContent,rows:c.querySelectorAll('.hll .acr').length,first:c.querySelector('.hll .acr')&&c.querySelector('.hll .acr').textContent}})()");
  ok('6. Settings has System health, with the events newest first', h.t == 'System health' && h.rows == 5 && /ready 41 s/.test(h.first), h);
  ok('7. the summary: start-up time, crashes this week (not the old one), pages closed to free memory', /41 s to the desktop/.test(h.sum) && /Crashes this week1/.test(h.sum) && /Pages closed to free memory1/.test(h.sum), h.sum);
  await q("(()=>{const s=SW.body.querySelector('.hlk');s.value='crash';s.dispatchEvent(new Event('change'))})()"); await wait(500);
  ok('8. the filter shows one kind', await q("SW.body.querySelectorAll('.hll .acr').length") == 2);
  const rep = await q("sysReport('x').then(r=>r.text)");
  ok('9. the system report has the health log and the firewall line', /== Health log \(latest 50/.test(rep) && /Crash: wine64-preloader stopped/.test(rep) && /^Firewall: off/m.test(rep), rep.slice(0, 1500));
  await q("hlNote('signin','wrong password for the account x')"); await wait(300);
  ok('10. the desktop adds its own events (POST health)', POSTS.some(x => x[0] == 'health' && /"k":"signin"/.test(x[1])));
  ok('11. no page errors', !errs.length, errs);
  console.log(`${R.filter(x => x).length}/${R.length} passed`); await b.close(); srv.close(); process.exit(R.every(x => x) ? 0 : 1);
})();
