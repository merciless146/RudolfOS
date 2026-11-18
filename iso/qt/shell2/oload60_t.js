// 1.6.0 (the owner, 2026-11-18): Oracle's loading line. An Oracle window: "Loading…" on its status line starts LitePoint's
// line and point along the bottom of the bookmarks bar, moving left to right; the page's own progress (the shell's) moves
// it; "Done" runs it to the end and it fades; a failed load fades where it stopped; Reduce motion: a plain line; the
// bookmarks bar hidden: the address bar's row.   node oload60_t.js [CHROME] [SHOT.png]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 500) : '')) };
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }
  const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200); fs.createReadStream(f).pipe(s) }).listen(8348, '127.0.0.1');
let p; const q = s => p.evaluate(s), sleep = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  p = await b.newPage({viewport: {width: 1100, height: 700}}); const errs = []; p.on('pageerror', e => errs.push(String(e.stack || e).slice(0, 300)));
  await p.goto('http://127.0.0.1:8348/'); await p.waitForFunction(() => typeof olpHook == 'function' && typeof browser == 'function', null, {timeout: 30000});
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());S.rmotion=false;return 1})()");
  await q("(()=>{browser('about:home');window.__O=wins.find(w=>w.el.querySelector('.obm'));return !!__O})()"); await sleep(1500);
  ok('1. an Oracle window is watched', await q("!!(__O&&__O.__olpHook)"));
  const set = t => q(`(()=>{__O.body.querySelector('.stat').textContent=${JSON.stringify(t)};return 1})()`);
  await set('Done'); await sleep(900);
  await set('Loading…'); await sleep(250);
  const at = await q("(()=>{const c=__O.body.querySelector('.olp');return c?{in:c.parentElement.className,b:c.getBoundingClientRect().bottom,bar:__O.body.querySelector('.obm').getBoundingClientRect().bottom,w:c.getBoundingClientRect().width,bw:__O.body.querySelector('.obm').clientWidth}:null})()");
  ok('2. Loading: the line along the bottom of the bookmarks bar, its whole width', at && at.in == 'obm' && Math.abs(at.b - at.bar - 8) < 1.5 && Math.abs(at.w - at.bw) < 1, at);
  const s1 = await q('__O.__olp.shown'); await sleep(1200); const s2 = await q('__O.__olp.shown'); await sleep(2500); const s3 = await q('__O.__olp.shown');
  ok('3. the point moves left to right, slowing (an estimate, under 90 %)', s1 < s2 && s2 < s3 && s3 < .9 && (s3 - s2) / 2.5 < (s2 - s1) / 1.2, [s1, s2, s3]);
  const lit = await q("(()=>{const c=__O.body.querySelector('.olp'),x=c.getContext('2d'),d=c.width/c.clientWidth,X=Math.floor(c.clientWidth*__O.__olp.shown*d),y=Math.floor(c.height/2);const px=(i)=>x.getImageData(i,y,1,1).data[3];return{behind:px(Math.max(1,X-40)),head:px(Math.max(1,X-1)),ahead:px(Math.min(c.width-2,X+40))}})()");
  ok('...lit behind the point, dark ahead of it', lit.behind > 60 && lit.head > 60 && lit.ahead < 20, lit);
  await q('olpProgress(__O,.95)'); await sleep(900);
  ok('4. the page\'s own progress moves it (95 %)', await q('__O.__olp.shown') > .9, await q('__O.__olp.shown'));
  if (process.argv[3]) { await q("(()=>{__O.el.style.left='40px';__O.el.style.top='30px';return 1})()"); await set('Done'); await set('Loading…'); await q('olpProgress(__O,.55)'); await sleep(1400);
    const r = await q("(()=>{const r=__O.el.getBoundingClientRect();return{x:r.x,y:r.y,width:r.width,height:Math.min(200,r.height)}})()"); await p.screenshot({path: process.argv[3], clip: r}); }
  await set('Done'); await sleep(250);
  ok('5. Done: the point runs to the right end', await q('__O.__olp.shown') > .98, await q('__O.__olp.shown'));
  await sleep(1500);
  ok('...and the line fades away', await q("!__O.body.querySelector('.olp')&&__O.__olp.state=='idle'"));
  await set('Loading…'); await sleep(700); const sf = await q('__O.__olp.shown');
  await set('This page could not be loaded (ERR_NAME_NOT_RESOLVED).'); await sleep(150);
  ok('6. a load that failed: it fades where it stopped (not run to the end)', await q('__O.__olp.state') == 'failed' && await q('__O.__olp.shown') < sf + .05, [sf, await q('__O.__olp.shown')]);
  await sleep(1500); ok('...and is gone', await q("!__O.body.querySelector('.olp')"));
  await q("(()=>{S.rmotion=true;return 1})()"); await set('Loading…'); await sleep(300);
  ok('7. Reduce motion: a plain line that only fills (no moving point)', await q("(()=>{const c=__O.body.querySelector('.olp'),x=c.getContext('2d');const d=c.width/c.clientWidth;return !!c&&x.getImageData(1,Math.floor(c.height/2),1,1).data[3]>0&&x.getImageData(1,0,1,1).data[3]==0})()"));
  await set('Done'); await sleep(800); await q("(()=>{S.rmotion=false;__O.body.querySelector('.obm').style.display='none';return 1})()");
  await set('Loading…'); await sleep(300);
  ok('8. the bookmarks bar hidden: along the address bar\'s row', await q("(()=>{const c=__O.body.querySelector('.olp');return !!c&&c.parentElement.classList.contains('ob')})()"));
  await set('Done'); await sleep(1500);
  await q("(()=>{__O.close();return 1})()"); await set('Loading…').catch(() => {}); await sleep(300);
  ok('9. a closed window: nothing left running', await q("OLP.on.size==0"));
  ok('10. no page errors', errs.length == 0, errs);
  console.log(`${R.filter(x => x).length}/${R.length} passed`); await b.close(); srv.close(); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('FAIL', e); process.exit(1) });
