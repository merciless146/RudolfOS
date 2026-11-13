// 1.5.9 (plan 8.2): a Library web app runs in a sandbox (no origin: not the desktop's page, storage or rudolfd) and
// reaches the desktop only through the message API (rudolf-gamekit), for what its PKGINFO asks and the person allowed.
// A stand-in rudolfd (it asks for the token as rudolfd does, and sends the package's page with its sandbox policy).
//   node pkgbox59_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 600) : '')) };
const RUDC = fs.readFileSync(path.join(__dirname, '../../sys/rudolfd.c'), 'utf8');
const CSP = (RUDC.match(/Content-Security-Policy: (sandbox allow-scripts[^\\]*)\\r\\n/) || [])[1];
const APP = `<!doctype html><script src="http://127.0.0.1:8274/lib/rudolf-gamekit.js"></script><script>
(async()=>{const r={};const t=async(k,f)=>{try{r[k]=await f()}catch(e){r[k]='ERR '+e.message}};
 await t('parent',()=>parent.document.title);await t('storage',()=>localStorage.length);await t('origin',()=>self.origin);
 await t('api',async()=>(await fetch('http://127.0.0.1:8274/api/pkg/status')).status);
 await t('desk',async()=>(await (await fetch('http://127.0.0.1:8274/')).text()).slice(0,20));
 await t('hello',()=>RudolfGamekit.hello());await t('set',()=>RudolfGamekit.save('slot1','{"lv":2}'));await t('get',()=>RudolfGamekit.load('slot1'));
 await t('list',()=>RudolfGamekit.saves());await t('badkey',()=>RudolfGamekit.save('../x','1'));await t('note',()=>RudolfGamekit.notify('hi'));
 await t('big',()=>RudolfGamekit.save('big','x'.repeat(1048577)));
 parent.postMessage({test:r},'*')})()</script>`;
const api = (a, q, m, h) => {
  if (h['x-rudolf-token'] != 't') return [403, {ok: false, error: 'token'}];
  if (a == 'pkg/status') return [200, {ok: true, installed: [{name: 'demo', kind: 'webapp', title: 'Demo Game', icon: '🎮', version: '1', mounted: true, permissions: 'saves gamepad fullscreen notifications'}], available: [], sources: []}];
  return [200, {ok: true}] };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/a/') || u.startsWith('/lib/')) { const f = path.join(WWW, u); if (fs.existsSync(f)) { s.writeHead(200, {'access-control-allow-origin': '*'}); return s.end(fs.readFileSync(f)) } }
    if (u == '/pkg/demo/www/index.html') { s.writeHead(200, {'content-type': 'text/html', 'access-control-allow-origin': '*', 'content-security-policy': CSP}); return s.end(APP) }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      const [c, b] = api(u.slice(5), qs, q.method, q.headers); s.writeHead(c, {'content-type': 'application/json'}); return s.end(JSON.stringify(b)) }
    s.writeHead(404); s.end() }) }).listen(8274, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  ok('0. rudolfd sends a Library app\'s pages with a sandbox policy (no allow-same-origin)', CSP && !/allow-same-origin/.test(CSP), CSP);
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8274/'); await p.waitForFunction(() => typeof pkgOpen == 'function' && typeof pkgBoxCall == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x'}];USER=ACCTS[0];window.__res=null;addEventListener('message',e=>{if(e.data&&e.data.test)__res=e.data.test});return 1})()`);
  await q("(pkgOpen('demo'),1)"); await wait(800);
  const dlg = await q("(()=>{const o=document.querySelector('.rmodal .pkbp');return o?o.textContent:''})()");
  ok('1. first opened: it asks for what the PKGINFO lists (saves, controllers, full screen, notifications)', /saves/.test(dlg) && /game controllers/.test(dlg) && /full screen/.test(dlg) && /notifications/.test(dlg), dlg);
  await q("(()=>{const i=[...document.querySelectorAll('.rmodal .pkbp input')].find(x=>x.value=='notifications');i.checked=false;document.querySelector('.rmodal .y').click();return 1})()");
  await p.waitForFunction(() => window.__res, null, {timeout: 20000}).catch(() => {});
  const r = await q('__res'), fr = await q("(()=>{const f=[...document.querySelectorAll('iframe')].find(x=>/pkg\\/demo/.test(x.src));return f?[f.getAttribute('sandbox'),f.getAttribute('allow')]:null})()");
  ok('2. its frame: sandboxed without its own origin; full screen and controllers allowed by the frame\'s policy', fr && !/allow-same-origin/.test(fr[0]) && /fullscreen/.test(fr[1]) && /gamepad/.test(fr[1]), fr);
  ok('3. it cannot reach the desktop\'s page or its storage, and has no origin', r && /^ERR/.test(String(r.parent)) && /^ERR/.test(String(r.storage)) && r.origin == 'null', r);
  ok('4. it cannot use rudolfd (no token) nor read the desktop page (which carries the token)', r && (r.api == 403 || /^ERR/.test(String(r.api))) && /^ERR/.test(String(r.desk)), r && {api: r.api, desk: r.desk});
  ok('5. hello: what was allowed (not notifications, which the person unticked)', r && r.hello && r.hello.name == 'demo' && r.hello.granted.includes('saves') && !r.hello.granted.includes('notifications'), r && r.hello);
  ok('6. a save kept, read back and listed', r && r.set === true && r.get == '{"lv":2}' && JSON.stringify(r.list) == '["slot1"]', r && [r.set, r.get, r.list]);
  ok('7. ...in its own folder of the account\'s files (Saved Games\\DEMO)', await q("node(['C:','USERS','ANN','SAVED GAMES','DEMO'])?.slot1==='{\"lv\":2}'"));
  ok('8. a save name with a path in it, a save over 1 MB, and a notification not allowed: refused', r && /^ERR a save name/.test(r.badkey) && /^ERR a save is 1 MB/.test(r.big) && /^ERR notifications were not allowed/.test(r.note), r && [r.badkey, r.big, r.note]);
  const fake = await q("(async()=>{const before=JSON.stringify(node(['C:','USERS','ANN','SAVED GAMES','DEMO']));window.postMessage({rudolf:'v1',id:'x',call:'save.set',args:{key:'evil',value:'1'}},'*');await new Promise(r=>setTimeout(r,300));return JSON.stringify(node(['C:','USERS','ANN','SAVED GAMES','DEMO']))==before})()");
  ok('9. messages from anything but an open Library app\'s frame are ignored', fake);
  await q("window.__res=null;pkgOpen('demo');1"); await wait(800);
  ok('10. opened again: not asked again (remembered for this account)', !(await q("!!document.querySelector('.rmodal .pkbp')")) && await q("[...document.querySelectorAll('iframe')].filter(x=>/pkg\\/demo/.test(x.src)).length==2"));
  ok('11. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
