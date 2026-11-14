// 1.5.9: the sign-in page's background ("the desktop's") is remembered as a reference: a picture of your own by its id,
// a drawn background by its name, otherwise the colours. Before, the pixels were kept, and while a picture was still
// being read after a start-up that was the teal under it (the Vivobook, 2026-11-13). A stand-in rudolfd.
//   node signinbg59_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www'), J = {'content-type': 'application/json'};
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
const srv = http.createServer((q, s) => { const u = q.url.split('?')[0]; q.on('data', () => {}); q.on('end', () => {
  if (u == '/') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
  if (u.startsWith('/a/')) { const f = path.join(WWW, u); if (fs.existsSync(f)) { s.writeHead(200); return s.end(fs.readFileSync(f)) } }
  if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200, J); s.end('{"ok":true,"events":[]}') }, 20000); s.writeHead(200, J); return s.end('{"ok":true}') }
  s.writeHead(404); s.end() }) }).listen(8290, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => { if (/lk[A-Z]|lock58|applySet/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8290/'); await p.waitForFunction(() => typeof lkRef == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.style&&(o.style.display='none')});ACCTS=[{n:'Ann',r:'admin',h:'x'}];USER=ACCTS[0];return 1})()`);
  // a picture of your own, chosen but not yet read (as just after a start-up): the colours show under it meanwhile
  const pic = await q(`(()=>{const c=document.createElement('canvas');c.width=40;c.height=20;const g=c.getContext('2d');g.fillStyle='#c0392b';g.fillRect(0,0,40,20);return c.toDataURL('image/jpeg')})()`);
  await q(`kvSet('wp:pic1',${JSON.stringify(pic)})`); await wait(200);
  await q(`(()=>{S.dyn='';S.b1='#008080';S.bs='solid';S.wpimg='pic1';S.wpfit='fill';WPID='';WPURL='';return 1})()`);
  await q('lkRemember()');
  ok('1. a picture not yet read: its id is remembered, not the teal under it', JSON.stringify(await q("ld(LK_LAST,null)")) == '{"wp":"pic1","fit":"fill"}', await q("ld(LK_LAST,null)"));
  await q("(()=>{const L=document.getElementById('login');L.style.display='';lkApply(L,'login');return 1})()"); await wait(600);
  const lb = await q("(()=>{const l=[...document.querySelectorAll('#login > .lkbgl')].find(x=>x.style.zIndex=='-2');return l?l.style.backgroundImage:''})()");
  ok('2. the sign-in page shows the picture (read from the store)', /^url\("data:image\/jpeg/.test(lb), lb.slice(0, 80));
  // a drawn background: its name, and the picture for the time of day when shown
  await q(`(()=>{S.wpimg='';S.dyn='stag';return 1})()`); await q('applySet()'); await wait(200);
  ok('3. a drawn background (Moonlit Stag): remembered by its name whenever the background is set', JSON.stringify(await q("ld(LK_LAST,null)")) == '{"dyn":"stag"}', await q("ld(LK_LAST,null)"));
  await q("(()=>{const L=document.getElementById('login');lkApply(L,'login');return 1})()"); await wait(300);
  const lb2 = await q("[...document.querySelectorAll('#login > .lkbgl')].find(x=>x.style.zIndex=='-2').style.backgroundImage");
  const want = await q("(()=>{const w=DYN.stag;return dynResolve().mode=='light'?w.l:w.d})()");
  ok('4. ...the sign-in page shows its light or dark picture, as the desktop would now', lb2.includes(want), [lb2.slice(0, 80), want]);
  // plain colours
  await q(`(()=>{S.dyn='';S.bs='solid';S.b1='#336699';return 1})()`); await q('applySet()'); await wait(200);
  const c3 = await q("ld(LK_LAST,null)");
  ok('5. colours: remembered as the colours', c3 && (c3.col == 'rgb(51, 102, 153)' || /51, 102, 153/.test(c3.img || '')), c3);
  // signed out: nothing is remembered (the page has no account's background then)
  await q("USER=null;S.b1='#000000';applySet()"); await wait(100);
  ok('6. nobody signed in: what was remembered stays', JSON.stringify(await q("ld(LK_LAST,null)")) == JSON.stringify(c3));
  ok('7. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
