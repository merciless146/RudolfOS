// 2026-11-21 (docs/plan-1.6.1.md Part 19): Settings › Display › Graphics driver: hidden with no NVIDIA card; an older card
// shown with NVIDIA's driver greyed out and why; a GeForce 16+ card with the choice, the change sent with an
// administrator's name and password (elevate {cred:1}) for rudolfd to check, "from the next start"; the extension missing
// said; a switch back by itself shown in Settings and once as a notification after sign-in (then acknowledged); names from
// rudolfd drawn as text.   node gpudrv61_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const M = {posts: [], acks: 0, st: null};
const base = (o) => Object.assign({ok: true, card: null, name: '', offered: false, why: 'this computer has no NVIDIA graphics card', extension: {installed: false, version: '', state: ''}, choice: 'open', inUse: 'open', version: ''}, o);
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0); let body = '';
  q.on('data', d => body += d); q.on('end', async () => {
    const J = (o, c) => { s.writeHead(c || 200, {'content-type': 'application/json'}); s.end(JSON.stringify(o)) };
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/events/wait') { await new Promise(r => setTimeout(r, 800)); s.writeHead(200); return s.end('{"ok":true,"events":""}') }
    if (u == '/api/gpu/driver') return J(M.st);
    if (u == '/api/gpu/driver/licence') { if (!M.st.extension.installed) return J({ok: false, error: 'not in use'}, 404); s.writeHead(200, {'content-type': 'text/plain'}); return s.end('License for Customer Use of NVIDIA Software <b>x</b>\n1. d. Distribute the SOFTWARE ...') }
    if (u == '/api/gpu/driver/ack') { M.acks++; delete M.st.fellBack; return J(M.st) }
    if (u == '/api/gpu/driver/set') { const to = new URLSearchParams(qs).get('to'); M.posts.push([to, body]); M.st.choice = to; return J(M.st) }
    if (u == '/api/display') return J({ok: true, setting: 'games', active: 'gpu', xorg: true, gpu: 'nouveaudrmfb'});
    if (u.startsWith('/api/')) return J({ok: true});
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8746, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  M.st = base({card: '10de:2684', name: 'NVIDIA GeForce RTX 4090', offered: true, why: '', extension: {installed: true, version: '595.112.07', state: 'in-use'},
    fellBack: {at: '2026-11-21T10:00:00Z', why: 'nvidia did not load: Key was rejected by service'}});
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  const p = await b.newPage({viewport: {width: 1280, height: 900}}); const errs = []; p.on('pageerror', e => errs.push(String(e.stack || e).slice(0, 300)));
  await p.goto('http://127.0.0.1:8746/'); await p.waitForFunction(() => typeof window.__gpudrv61 != 'undefined' && typeof settings == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  ok('1. before sign-in: no notification about the switch back yet', (await q("(window.__notifCentre&&__notifCentre.list?__notifCentre.list():[]).length")) == 0 || M.acks == 0);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});USER=USER||ACCTS[0];USER.r='admin';window.__asked=[];window.__nt=[];const n=window.notify;window.notify=(t,b,o)=>{__nt.push([t,b]);return n(t,b,o)};window.elevate=async(r,o)=>{__asked.push([r,!!(o&&o.cred)]);return o&&o.cred?{n:'Ann',p:'pw-ann'}:true};return 1})()");
  await wait(6500);
  const nt = await q('__nt.filter(x=>x[0]=="Graphics driver")');
  ok('2. after sign-in: one notification says it went back to the open driver, and why', nt.length == 1 && /could not be used \(nvidia did not load: Key was rejected by service\)/.test(nt[0][1]) && /open driver/.test(nt[0][1]), nt);
  ok('3. ...and the note was acknowledged (shown once)', M.acks == 1, M.acks);
  const open = async () => { await q("(()=>{wins.slice().forEach(w=>w.close());settings('disp');return 1})()"); await wait(1200) };
  const sec = () => q("(()=>{const s=document.querySelector('#gdsel');if(!s)return null;const d=s.closest('.sp25');return {txt:d.textContent,val:s.value,nvOff:s.querySelector('option[value=nvidia]').disabled}})()");
  await open(); let x = await sec();
  ok('4. GeForce RTX 4090: Graphics driver shown, Open (NVK) chosen, NVIDIA\'s driver offered', x && x.val == 'open' && !x.nvOff && /NVIDIA GeForce RTX 4090: now using the open driver \(NVK\)/.test(x.txt), x);
  const lic = await q("(()=>{const l=document.querySelector('.gdl');return l&&!l.hidden?l.querySelector('pre').textContent:null})()");
  ok('...NVIDIA\'s licence shown with it (the extension installed: its 1.d asks that each recipient get it), as text', lic && /License for Customer Use of NVIDIA Software <b>x<\/b>/.test(lic), lic);
  await q("(()=>{const s=document.querySelector('#gdsel');s.value='nvidia';s.onchange();return 1})()"); await wait(600); x = await sec();
  ok('5. choosing NVIDIA\'s driver: an administrator\'s password asked, sent to rudolfd with to=nvidia', M.posts.length == 1 && M.posts[0][0] == 'nvidia' && JSON.parse(M.posts[0][1]).n == 'Ann' && (await q('__asked'))[0][1] === true, M.posts);
  ok('6. ..."at the next start" said', /At the next start: NVIDIA's driver/.test(x.txt), x.txt);
  await q("(()=>{window.elevate=async()=>null;const s=document.querySelector('#gdsel');s.value='open';s.onchange();return 1})()"); await wait(500); x = await sec();
  ok('7. the password prompt cancelled: nothing sent, the choice shown as before', M.posts.length == 1 && x.val == 'nvidia', [M.posts, x.val]);
  M.st = base({card: '10de:1b80', name: '<img src=x onerror="window.__pwn=1">', offered: false, why: "NVIDIA's driver is offered for GeForce 16 and newer; this card (10de:1b80) is older or not on its list"});
  await open(); x = await sec();
  ok('8. an older card: NVIDIA\'s driver greyed out, and why', x && x.nvOff && /offered for GeForce 16 and newer/.test(x.txt), x);
  ok('9. names from rudolfd drawn as text', !(await q('window.__pwn')) && /<img src=x/.test(x.txt));
  M.st = base({card: '10de:2684', name: 'NVIDIA GeForce RTX 4090', offered: true, why: ''});
  await open(); x = await sec();
  ok('10. the extension not installed: says where to install it', x && /System extensions first/.test(x.txt), x && x.txt);
  M.st = base({}); await open();
  ok('11. no NVIDIA card: no Graphics driver section at all', (await sec()) === null && (await q("!!document.querySelector('#dgp')")));
  ok('12. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('FAIL', e); process.exit(1) });
