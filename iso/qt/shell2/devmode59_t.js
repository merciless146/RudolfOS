// 1.5.9 (plan 8.3): Settings › Update and recovery › Developer mode (administrators: a strong warning, a typed
// confirmation, the password every time), and Official / Custom in About. A stand-in rudolfd.   node devmode59_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const KEY = 'kRBDN3Ys6i49gblu504quUt7NZHGZj/kexNTb6uoenY=';   // (a public key: the Library's, as a stand-in developer key)
let dev = false, signer = 'official'; const posts = [];
const status = () => ({ok: true, persistent: true, booted: 'a', current: {serial: 2026111301, version: '1.5.9'}, state: {good: 'a'}, slots: {a: {complete: true}, b: {}, r: {complete: true}}, canUpdate: true, signer, signerKey: signer == 'official' ? 'UqTqXIW8vtITjwoMix+MkFVZ0GAee9+kOS1WBIs/3mo=' : KEY, developer: dev, recoveryOfficial: true, url: dev ? 'https://dev.example.com/beta' : 'https://merciless146.github.io/RudolfOS/stable', auto: true, checked: 0, available: null});
const api = (a, q, m) => { const sp = new URLSearchParams(q || '');
  if (a == 'sys/status') return status();
  if (a == 'sys/developer' && m == 'POST') { posts.push(sp.get('on') == '0' ? 'off' : 'on ' + sp.get('key') + ' ' + sp.get('url')); dev = sp.get('on') != '0'; return {ok: true, message: dev ? 'Developer mode is on: …' : 'Developer mode is off: …'} }
  if (a == 'sys/return' && m == 'POST') { posts.push('return ' + sp.get('erase')); return {ok: true, message: 'started'} }
  if (a == 'job') return {ok: true, running: false, status: 0, log: 'Returning to the official Rudolf OS 1.5.9: it starts the next time the computer restarts, with everything erased (a factory reset).\n'};
  if (a == 'info') return {ok: true, vendor: 'ASUS', product: 'Vivobook', cpu: 'x', threads: 4, memTotal: 3.4e9, kernel: '6.18', boot: 'uefi', live: 0};
  return {ok: true} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/a/')) { const f = path.join(WWW, u); if (fs.existsSync(f)) { s.writeHead(200); return s.end(fs.readFileSync(f)) } }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs, q.method))) }
    s.writeHead(404); s.end() }) }).listen(8275, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8275/'); await p.waitForFunction(() => typeof devmOn == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x'},{n:'Ben',r:'user',h:'y'}];USER=ACCTS[1];pwCheck=async(a,pw)=>a.n=='Ann'&&pw=='sesame';return 1})()`);
  const panel = async () => { await q("(()=>{const o=document.getElementById('dmt');o&&o.remove();const d=document.createElement('div');d.id='dmt';document.body.appendChild(d);updPanel(d);return 1})()"); await wait(1200); return q("(()=>{const b=document.querySelector('#dmt .devm');return b?[b.textContent.replace(/\\s+/g,' '),b.querySelector('.dvb').disabled]:null})()") };
  let d = await panel();
  ok('1. a standard user sees Developer mode off and the official system, and cannot change it', d && /Developer mode\s*Off · this system: Official Rudolf OS/.test(d[0]) && d[1] && /Only an administrator/.test(d[0]), d);
  await q('USER=ACCTS[0];window.__elevTrust=Date.now()'); d = await panel();
  ok('2. an administrator can', d && !d[1], d);
  await q("document.querySelector('#dmt .dvb').click()"); await wait(300);
  const warn = await q("document.querySelector('.rmodal h3')?.textContent+' '+document.querySelector('.rmodal p')?.textContent");
  ok('3. a strong warning first (a build from someone else can do anything; recovery stays official)', /Turn on Developer mode/.test(warn) && /can do anything on this computer/.test(warn), warn);
  const fill = (k, u, c) => q(`(()=>{const o=document.querySelector('.rmodal');o.querySelector('.dk').value=${JSON.stringify(k)};o.querySelector('.du').value=${JSON.stringify(u)};o.querySelector('.dc').value=${JSON.stringify(c)};o.querySelector('.y').click();return o.querySelector('.rmmsg').textContent})()`);
  const m1 = await fill('short', 'https://dev.example.com/beta', 'I TRUST THIS DEVELOPER'), m2 = await fill(KEY, 'http://dev.example.com/beta', 'I TRUST THIS DEVELOPER'), m3 = await fill(KEY, 'https://dev.example.com/beta', 'yes');
  ok('4. a key that is not one, an address without https, and no typed confirmation: refused, nothing sent', /44 characters/.test(m1) && /https/.test(m2) && /Type I TRUST THIS DEVELOPER/.test(m3) && posts.length == 0, [m1, m2, m3]);
  await fill(KEY, 'https://dev.example.com/beta', 'I trust this developer'); await wait(400);
  const el = await q("document.querySelector('.rmodal .rmr')?.textContent||''");
  ok('5. then the administrator\'s password, even right after another approval', /Turn on Developer mode: install system updates signed by key kRBDN3Ys…/.test(el) && posts.length == 0, el);
  await p.type('.rmodal .ep', 'sesame'); await p.keyboard.press('Enter'); await wait(1500);
  ok('6. rudolfd turns it on with the key and the address', posts.at(-1) == 'on ' + KEY + ' https://dev.example.com/beta', posts);
  signer = 'custom'; d = await panel();
  ok('7. On: the developer\'s address, recovery still official, and this system a Custom build with its key', d && /Developer mode\s*On · this system: Custom build \(signed by key kRBDN3Ys…/.test(d[0]) && /dev\.example\.com/.test(d[0]) && /Recovery mode still starts the official Rudolf OS/.test(d[0]), d);
  await q("(()=>{const o=document.getElementById('abt');o&&o.remove();const d=document.createElement('div');d.id='abt';document.body.appendChild(d);aboutPanel(d);return 1})()"); await wait(800);
  ok('8. About says Custom build', /⚠️ Custom build \(signed by key kRBDN3Ys…/.test(await q("document.querySelector('#abt .devmab')?.textContent||''")));
  await q("document.querySelector('#dmt .dvb').click()"); await wait(400);
  ok('9. turning it off asks for the password too', /Turn off Developer mode/.test(await q("document.querySelector('.rmodal .rmr')?.textContent||''")));
  await p.type('.rmodal .ep', 'sesame'); await p.keyboard.press('Enter'); await wait(1200);
  signer = 'official'; await q("(()=>{const o=document.getElementById('abt');o&&o.remove();const d=document.createElement('div');d.id='abt';document.body.appendChild(d);aboutPanel(d);return 1})()"); await wait(800);
  ok('10. off: rudolfd told; About says Official Rudolf OS again', posts.at(-1) == 'off' && /✅ Official Rudolf OS/.test(await q("document.querySelector('#abt .devmab')?.textContent||''")), posts);
  dev = true; signer = 'custom'; d = await panel();
  ok('12. a custom build: "Return to official Rudolf OS" is offered', await q("!!document.querySelector('#dmt .dvr')"));
  await q("document.querySelector('#dmt .dvr').click()"); await wait(300);
  await q("(()=>{document.querySelector('.rmodal input[name=dre][value=\"1\"]').checked=true;document.querySelector('.rmodal .y').click();return 1})()"); await wait(400);
  ok('13. keep or erase, then the administrator\'s password', /Return to the official Rudolf OS, erasing every account/.test(await q("document.querySelector('.rmodal .rmr')?.textContent||''")));
  await p.type('.rmodal .ep', 'sesame'); await p.keyboard.press('Enter'); await wait(2000);
  ok('14. rudolfd returns to official, erasing (as chosen)', posts.at(-1) == 'return 1', posts);
  signer = 'custom'; await q("(async()=>{const s=await rd('sys/status');const _d=osverDraw;osverDraw=function(){_d.apply(this,arguments);const v=document.querySelector('#start .stv');if(v&&!/custom$/.test(v.textContent))v.textContent+=' · custom'};osverDraw();return 1})()");
  ok('15. a custom build: the Start button says so', / · custom$/.test(await q("document.querySelector('#start .stv')?.textContent||''")));
  ok('16. Blitzen\'s facts say which build this is', /Custom build/.test(await q("(async()=>{const f=await bzFacts(1);return f.build||''})()")));
  ok('11. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
