// 1.3.0 stage B in the desktop page, with a stand-in rudolfd, in Chromium: a crashed job is reported as a crash
// (item 4: the App Store asked "Is this computer connected to the internet?"), the system report's Crashes section,
// and Settings › Apps › Linux › Change the Linux password (item 6). node b130_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../..', 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.mjs': 'text/javascript', '.html': 'text/html'};
const CRASH = {program: 'rudolf-apps', at: 913, kind: 'segmentation fault', where: 'libc.so.6+18badd'};
const M = {calls: [], body: {}, job: null};
const api = (a, q, body) => { M.calls.push(a + (q ? '?' + q : '')); if (body && body.length) M.body[a] = body.toString();
  if (a == 'apps/status') return {ok: true, flatpak: true, persistent: true};
  if (a == 'apps/catalog') return {ok: false, error: 'the app catalogue has not been downloaded yet', __status: 404};
  if (a == 'apps/installed') return {ok: true, apps: []};
  if (a == 'apps/refresh') { M.job = {ok: true, running: false, status: 139, name: 'catalog', log: 'Adding the Flathub app source...\nDownloading the app catalogue...\nSegmentation fault\n', crashed: true, crashes: [CRASH]}; return {ok: true, message: 'started'} }
  if (a == 'debian/passwd') { M.job = {ok: true, running: false, status: 0, name: 'debian-passwd', log: 'done the Linux password is changed\n'}; return {ok: true, message: 'started'} }
  if (a == 'debian/status') return {ok: true, installed: '1', version: '13.7', running: '0', persist: '1', free: 1e11, size: 1.3e8};
  if (a == 'debian/apps') return {ok: true, apps: []};
  if (a == 'job') return M.job || {ok: true, running: false, status: 0, log: ''};
  if (a == 'crashes') return {ok: true, crashes: [CRASH]};
  if (a == 'system') return {ok: true, renderer: 'AMD', display: 'gpu', audio: '', net: '', services: {sound: true}, machine: 'Test', firmware: 'Test', kernel: {boot: 'uefi', messages: '', release: '6.18'}, cpu: {}, memory: {total: 8e9}, gpu: '', storage: '', input: '', uptime: 100, load: [0, 0, 0]};
  if (a == 'stats') return {ok: true, battery: [], cpu: 1, memTotal: 8e9, memAvailable: 4e9, load: 0};
  if (a == 'info') return {ok: true, live: false, boot: 'uefi', media: 'disk', uptime: 100, load: [0, 0, 0], memTotal: 8e9, memAvailable: 4e9, kernel: '6.18'};
  return {ok: true} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0); const ch = []; q.on('data', c => ch.push(c)); q.on('end', () => {
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
  if (u.startsWith('/api/')) { const r = api(u.slice(5), qs, Buffer.concat(ch)); s.writeHead(r.__status || 200, {'content-type': 'application/json'}); return s.end(JSON.stringify(r)) }
  const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(8098, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage(); p.on('pageerror', e => /toUpperCase/.test(e.message) || console.log('page error:', e.message));
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8098/'); await p.waitForFunction(() => typeof flatStoreTab == 'function' && typeof debPasswd == 'function', null, {timeout: 30000});
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';window.elevate=async()=>true;return 1})()");
  // item 4: the app list download crashing
  await q("(()=>{const W=appStore();[...W.body.querySelectorAll('.tmtabs button')].find(x=>x.dataset.t=='linux').click();return 1})()");
  await p.waitForSelector('.lxdl', {timeout: 8000}).catch(() => 0); await q("document.querySelector('.lxdl').click()"); await wait(2500);
  const t = await q("document.querySelector('.lxempty pre').textContent");
  ok('4. a crashed app list download says which program crashed, not "Is this computer connected"', /rudolf-apps stopped unexpectedly \(in libc\.so\.6\+18badd\)\. That is a bug in Rudolf OS, not a problem with this computer or its connection/.test(t) && !/connected to the internet/.test(t), t.slice(-250));
  ok('...jobCrashText for a crash with no kernel line', await q("jobCrashText({crashed:true,crashes:[]})").then(x => /^A part of Rudolf OS stopped unexpectedly\./.test(x)) && await q("jobCrashText({status:1})") === '');
  const rep = await q("sysReport('').then(r=>r.text)");
  ok('...and the system report has a Crashes section', /== Crashes \(Rudolf OS programs since start-up\)\nrudolf-apps: segmentation fault in libc\.so\.6\+18badd \(15 min after start-up\)/.test(rep), (rep.match(/== Crashes[^]*?\n\n/) || [''])[0]);
  // item 6: Change the Linux password
  await q("(()=>{wins.slice().forEach(w=>w.close());settings('apps');return 1})()"); await p.waitForSelector('.dpw', {timeout: 8000}).catch(() => 0);
  ok('6. Settings › Apps › Linux has Change the Linux password…', await q("document.querySelector('.dpw')?.textContent") == 'Change the Linux password…');
  await q("document.querySelector('.dpw').click()"); await p.waitForSelector('.debpw .dp1', {timeout: 5000}).catch(() => 0);
  await q("(()=>{const o=document.querySelector('.debpw');o.querySelector('.dp1').value='abc';o.querySelector('.dp2').value='abc';o.querySelector('.y').click();return 1})()"); await wait(300);
  ok('...too short: refused before asking rudolfd', /at least 4 characters/.test(await q("document.querySelector('.debpw .dperr').textContent")) && !M.body['debian/passwd']);
  await q("(()=>{const o=document.querySelector('.debpw');o.querySelector('.dp1').value='newpass1';o.querySelector('.dp2').value='newpass2';o.querySelector('.y').click();return 1})()"); await wait(300);
  ok('...two different ones: refused', /different/.test(await q("document.querySelector('.debpw .dperr').textContent")) && !M.body['debian/passwd']);
  await q("(()=>{const o=document.querySelector('.debpw');o.querySelector('.dp1').value='newpass1';o.querySelector('.dp2').value='newpass1';o.querySelector('.y').click();return 1})()"); await wait(2500);
  ok('...a good one goes to rudolfd (debian/passwd, in the body, not the address) and the result is shown', M.body['debian/passwd'] == 'newpass1' && M.calls.includes('debian/passwd') && !(await q("!!document.querySelector('.debpw')")) && /The Linux password is changed/.test(await q("document.querySelector('.dmsg').textContent")), [M.calls.filter(c => /passwd/.test(c)), await q("document.querySelector('.dmsg')?.textContent")]);
  await b.close(); srv.close();
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
