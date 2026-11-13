// 1.5.9 (plan 8.2): a Library web app runs in a sandbox (no origin: not the desktop's page, storage or rudolfd) and
// reaches the desktop only through the game kit's messages (games58.js); an outside developer's app gets only what its
// PKGINFO asks and the person allowed; the project's own apps as before.
// A stand-in rudolfd (it asks for the token as rudolfd does, and sends the package's page with its sandbox policy).
//   node pkgbox59_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 600) : '')) };
const RUDC = fs.readFileSync(path.join(__dirname, '../../sys/rudolfd.c'), 'utf8');
const CSP = (RUDC.match(/Content-Security-Policy: (sandbox allow-scripts[^\\]*)\\r\\n/) || [])[1];
const APP = `<!doctype html><script>
const ask=(op,o)=>new Promise(r=>{const id=Math.random();addEventListener('message',function f(e){if(e.data&&e.data.rudolfGame===1&&e.data.id===id){removeEventListener('message',f);r(e.data)}});parent.postMessage(Object.assign({rudolfGame:1,op,id,game:'demo'},o||{}),'*');setTimeout(()=>r(null),3000)});
(async()=>{const r={};const t=async(k,f)=>{try{r[k]=await f()}catch(e){r[k]='ERR '+e.message}};
 await t('parent',()=>parent.document.title);await t('storage',()=>localStorage.length);await t('origin',()=>self.origin);
 await t('api',async()=>(await fetch('http://127.0.0.1:8274/api/pkg/status')).status);
 await t('desk',async()=>(await (await fetch('http://127.0.0.1:8274/')).text()).slice(0,20));
 await t('hello',()=>ask('hello'));await t('save',()=>ask('save',{data:{lv:2}}));await t('load',()=>ask('load'));
 parent.postMessage({test:r,pkg:location.pathname.split('/')[2]},'*')})()</script>`;
const api = (a, q, m, h) => {
  if (h['x-rudolf-token'] != 't') return [403, {ok: false, error: 'token'}];
  if (a == 'pkg/status') return [200, {ok: true, installed: [
    {name: 'alex-demo', kind: 'webapp', title: 'Demo Game', icon: '🎮', version: '1', mounted: true, signed: 'developer', developer: 'Alex Doe', github: 'alexdoe', permissions: 'saves gamepad fullscreen'},
    {name: 'rudolf-lib', kind: 'webapp', title: 'Library Game', icon: '🎲', version: '1', mounted: true, signed: 'library'}], available: [], sources: []}];
  return [200, {ok: true}] };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/a/') || u.startsWith('/lib/')) { const f = path.join(WWW, u); if (fs.existsSync(f)) { s.writeHead(200, {'access-control-allow-origin': '*'}); return s.end(fs.readFileSync(f)) } }
    if (/^\/pkg\/(alex-demo|rudolf-lib)\/www\/index\.html$/.test(u)) { s.writeHead(200, {'content-type': 'text/html', 'access-control-allow-origin': '*', 'content-security-policy': CSP}); return s.end(APP) }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      const [c, b] = api(u.slice(5), qs, q.method, q.headers); s.writeHead(c, {'content-type': 'application/json'}); return s.end(JSON.stringify(b)) }
    s.writeHead(404); s.end() }) }).listen(8274, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  ok('0. rudolfd sends a Library app\'s pages with a sandbox policy (no allow-same-origin)', CSP && !/allow-same-origin/.test(CSP), CSP);
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8274/'); await p.waitForFunction(() => typeof pkgOpen == 'function' && typeof pkgBoxFrame == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x'}];USER=ACCTS[0];window.__res={};addEventListener('message',e=>{if(e.data&&e.data.test)__res[e.data.pkg]=e.data.test});return 1})()`);
  await q("(pkgOpen('alex-demo'),1)"); await wait(800);
  const dlg = await q("(()=>{const o=document.querySelector('.rmodal');return o?o.textContent:''})()");
  ok('1. an outside developer\'s app, first opened: who made it, and what its PKGINFO asks for', /Made by Alex Doe \(github\.com\/alexdoe\)/.test(dlg) && /Keep its saves/.test(dlg) && /game controllers/.test(dlg) && /full screen/.test(dlg), dlg);
  await q("(()=>{const i=[...document.querySelectorAll('.rmodal .pkbp input')].find(x=>x.value=='saves');i.checked=false;document.querySelector('.rmodal .y').click();return 1})()");
  await p.waitForFunction(() => window.__res['alex-demo'], null, {timeout: 20000}).catch(() => {});
  const r = await q("__res['alex-demo']"), fr = await q("(()=>{const f=[...document.querySelectorAll('iframe')].find(x=>/alex-demo/.test(x.src));return f?[f.getAttribute('sandbox'),f.getAttribute('allow')]:null})()");
  ok('2. its frame: sandboxed without its own origin; full screen and controllers allowed by the frame\'s policy', fr && !/allow-same-origin/.test(fr[0]) && /fullscreen/.test(fr[1]) && /gamepad/.test(fr[1]), fr);
  ok('3. it cannot reach the desktop\'s page or its storage, and has no origin', r && /^ERR/.test(String(r.parent)) && /^ERR/.test(String(r.storage)) && r.origin == 'null', r);
  ok('4. it cannot use rudolfd (no token) nor read the desktop page (which carries the token)', r && (r.api == 403 || /^ERR/.test(String(r.api))) && /^ERR/.test(String(r.desk)), r && {api: r.api, desk: r.desk});
  ok('5. the game kit\'s hello answers', r && r.hello && r.hello.ok === true, r && r.hello);
  ok('6. saves, which the person unticked, are refused (the game kit\'s save and load)', r && r.save && r.save.ok === false && /not allowed/.test(r.save.error) && r.load && r.load.ok === false, r && [r.save, r.load]);
  await q("(pkgOpen('rudolf-lib'),1)"); await wait(800);
  ok('7. the project\'s own Library app opens without asking', !(await q("!!document.querySelector('.rmodal .pkbp')")));
  await p.waitForFunction(() => window.__res['rudolf-lib'], null, {timeout: 20000}).catch(() => {});
  const r2 = await q("__res['rudolf-lib']"), fr2 = await q("(()=>{const f=[...document.querySelectorAll('iframe')].find(x=>/rudolf-lib/.test(x.src));return f?f.getAttribute('allow'):null})()");
  ok('8. ...with saves, full screen and controllers, as in 1.5.8 (still sandboxed)', r2 && r2.save && r2.save.ok === true && r2.load && r2.load.data && r2.load.data.lv == 2 && /fullscreen/.test(fr2) && /gamepad/.test(fr2) && r2.origin == 'null', {r2, fr2});
  const fake = await q("(async()=>{let got=null;const f=e=>{if(e.data&&e.data.op=='reply')got=e.data};addEventListener('message',f);window.postMessage({rudolfGame:1,op:'save',id:9,game:'demo',data:{x:1}},'*');await new Promise(r=>setTimeout(r,400));removeEventListener('message',f);return got})()");
  ok('9. messages from anything but an open app\'s frame are ignored', fake === null, fake);
  await q("window.__res={};pkgOpen('alex-demo');1"); await wait(800);
  ok('10. opened again: not asked again (remembered for this account)', !(await q("!!document.querySelector('.rmodal .pkbp')")) && await q("[...document.querySelectorAll('iframe')].filter(x=>/alex-demo/.test(x.src)).length==2"));
  await q("(()=>{const s0=PKGS;PKGS={...s0,installed:[{...s0.installed[0],disabled:'the Rudolf OS Library revoked its developer\\u2019s key'}]};PKGT=Date.now();window.__n=[];const _n=notify;notify=(t,m,o)=>{__n.push(m);return _n(t,m,o)};pkgOpen('alex-demo');return 1})()"); await wait(400);
  ok('12. a revoked app does not open, and says why', /turned off: the Rudolf OS Library revoked/.test(await q("__n.join('|')")) && await q("[...document.querySelectorAll('iframe')].filter(x=>/alex-demo/.test(x.src)).length==2"));
  ok('11. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
