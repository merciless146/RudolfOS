// 1.6.1 (the owner, 2026-11-20): Oracle's Downloads page (about:downloads): a progress bar while a file arrives, and
// Remove beside Open. A file in Downloads goes to the Trash; otherwise the notification "File removed from list" says it
// was not in Downloads, with Blitzen's button to help find it (Blitzen reads the record, the Trash and where files of that
// name are). rudolfd stood in.   node dlpage61_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 600) : '')) };
const api = a => a == 'events' ? {ok: true, events: ''} : a == 'info' ? {ok: true, backlight: false, live: 0, boot: 'uefi', kernel: 'test'} : a == 'dl/incoming' ? {ok: true, items: []} : {ok: true};
const srv = http.createServer((q, s) => { const [u0] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', async () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/events/wait') await new Promise(r => setTimeout(r, 800));
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5) == 'events/wait' ? 'events' : u.slice(5)))) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8612, '127.0.0.1');
const prep = "(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';window.__net=false;window.__dl27=false;return 1})()";
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const p = await (await b.newContext({viewport: {width: 1280, height: 860}})).newPage(); const errs = []; p.on('pageerror', e => errs.push(e.stack.slice(0, 300)));
  await p.goto('http://127.0.0.1:8612/'); await p.waitForFunction(() => typeof browser == 'function' && typeof dlRemove == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s); await q(prep);
  // the list: one saved file that is in Downloads, one saved but moved away, one failed, one arriving (size unknown), one arriving (size known), one being checked
  await q(`(()=>{const DP=downloadsPath();let d=fs;for(const x of DP){if(!d[x]||typeof d[x]!='object')d[x]={};d=d[x]}fsSave();d['HELLO.TXT']='hi';d['O\\'K & <X>.TXT']='x';const t=Date.now();DLS.length=0;
    DLS.push({n:'HELLO.TXT',u:'https://example.com/hello.txt',t:t-50000,sz:2,st:'saved',why:'',path:'HELLO.TXT'},
     {n:'GONE.ZIP',u:'https://example.com/gone.zip',t:t-40000,sz:2000,st:'saved',why:'',path:'GONE.ZIP'},
     {n:'BAD.EXE',u:'https://example.com/bad.exe',t:t-30000,sz:0,st:'blocked',why:'Program files are blocked.'},
     {n:'BIG.ISO',u:'https://example.com/big.iso',t:t-20000,sz:52428800,sp:2097152,st:'wait',why:'',nat:1},
     {n:'KNOWN.BIN',u:'https://example.com/known.bin',t:t-10000,sz:25*1048576,tot:100*1048576,sp:5*1048576,st:'wait',why:''},
     {n:'SCAN.PDF',u:'https://example.com/scan.pdf',t:t-5000,sz:1000,st:'check',why:''},
     {n:"O'K & <X>.TXT",u:'https://example.com/ok',t:t-1000,sz:1,st:'saved',why:'',path:"O'K & <X>.TXT"});saveDl();
    window.__asked=null;blzAskFrom=(app,qn,ctx,o)=>{__asked={app,qn,ctx}};return 1})()`);
  await q("browser('about:downloads')"); await p.waitForTimeout(900);
  const F = p.frameLocator('.win.act iframe:visible');
  const rowOf = n => F.locator('tr', {hasText: n});
  const txt = async n => (await rowOf(n).innerText()).replace(/\s+/g, ' ');
  // progress
  ok('1. arriving, size not known: a moving bar, how much so far and how fast', await rowOf('BIG.ISO').locator('[role=progressbar]').count() == 1 && /50 MB so far · 2\.0 MB\/s/.test(await txt('BIG.ISO')), await txt('BIG.ISO'));
  const kn = await txt('KNOWN.BIN');
  ok('2. arriving, size known: the bar at 25%, "25 MB of 100 MB (25%) · 5.0 MB/s · 15 s left"', /25 MB of 100 MB \(25%\) · 5\.0 MB\/s · 15 s left/.test(kn) && await rowOf('KNOWN.BIN').locator('[role=progressbar][aria-valuenow="25"]').count() == 1, kn);
  ok('3. being checked: a bar and "Checking the file…"', /Checking the file/.test(await txt('SCAN.PDF')) && await rowOf('SCAN.PDF').locator('[role=progressbar]').count() == 1);
  ok('4. finished ones have no bar; arriving ones have no Remove', await rowOf('HELLO.TXT').locator('[role=progressbar]').count() == 0 && await rowOf('BIG.ISO').locator('a', {hasText: 'Remove'}).count() == 0);
  ok('5. Remove beside Open (saved), and on a blocked one', /Open · Remove/.test(await txt('HELLO.TXT')) && await rowOf('BAD.EXE').locator('a', {hasText: 'Remove'}).count() == 1, await txt('HELLO.TXT'));
  // Remove: in Downloads -> Trash
  const tr0 = await q('TR.length');
  await rowOf('HELLO.TXT').locator('a', {hasText: 'Remove'}).click(); await p.waitForTimeout(700);
  const s1 = await q("({inDl:'HELLO.TXT' in node(downloadsPath()),tr:TR.slice(-1)[0]&&TR.slice(-1)[0].n,list:DLS.some(d=>d.n=='HELLO.TXT'),toast:[...document.querySelectorAll('.ntoast')].map(t=>t.textContent).join('|')})");
  ok('6. Remove on a file in Downloads: off the list, moved to the Trash (restorable), "Moved to the Trash"', !s1.inDl && s1.tr == 'HELLO.TXT' && !s1.list && /Moved to the Trash/.test(s1.toast) && await q('TR.length') == tr0 + 1, s1);
  ok('...and the page no longer lists it', await rowOf('HELLO.TXT').count() == 0);
  // a name with ' & <: the right one
  await rowOf("O'K & <X>.TXT").locator('a', {hasText: 'Remove'}).click(); await p.waitForTimeout(500);
  ok("7. a name with ', & and <: that one removed and in the Trash", await q("!(\"O'K & <X>.TXT\" in node(downloadsPath()))&&TR.some(e=>e.n==\"O'K & <X>.TXT\")"));
  // Remove: not in Downloads
  await q("document.querySelectorAll('.ntoast').forEach(t=>t.remove())");
  await q(`(()=>{fs['C:'].GAMES=fs['C:'].GAMES||{};fs['C:'].GAMES['GONE.ZIP']='moved here';return 1})()`);
  await rowOf('GONE.ZIP').locator('a', {hasText: 'Remove'}).click(); await p.waitForTimeout(600);
  const t2 = await q("[...document.querySelectorAll('.ntoast')].map(t=>({t:t.textContent,b:!!t.querySelector('.bzwhy'),l:t.querySelector('.bzwhy')&&t.querySelector('.bzwhy').textContent}))");
  ok('8. Remove on a file not in Downloads: "File removed from list" "Not in Downloads. Ask Blitzen if you need help finding this file."', t2.some(x => /File removed from list/.test(x.t) && /Not in Downloads\. Ask Blitzen if you need help finding this file\./.test(x.t)), t2);
  ok('...with Blitzen\'s button ("⚡ Ask Blitzen", the same button as ⚡ Why?)', t2.some(x => x.b && x.l == '⚡ Ask Blitzen'), t2);
  ok('...nothing was moved or deleted (the moved file stays where it is)', await q("fs['C:'].GAMES['GONE.ZIP']=='moved here'&&!DLS.some(d=>d.n=='GONE.ZIP')"));
  const nc = await q("NC.find(n=>/File removed from list/.test(n.t))");
  ok('9. kept in the notifications\' centre with its button (bz, wl "Ask Blitzen": the native centre draws the same)', nc && nc.bz && nc.bz.n == 'GONE.ZIP' && nc.wl == 'Ask Blitzen', nc);
  await p.click('.ntoast .bzwhy'); await p.waitForTimeout(500);
  const rev = await q("[...document.querySelectorAll('.bzrev .bzparts pre')].map(x=>x.textContent).join('\\n---\\n')");
  ok('10. Blitzen\'s button: what Blitzen will read is shown first: the download, the Trash, where files of that name are', /Name: GONE\.ZIP/.test(rev) && /No file of that name is in the Trash/.test(rev) && /C:\\GAMES\\GONE\.ZIP/.test(rev), rev);
  await q("document.querySelector('.bzrev .ok').click()"); await p.waitForTimeout(300);
  const asked = await q('__asked');
  ok('...then Blitzen is asked where it is (moved, in the Trash, deleted or never downloaded)', asked && /moved somewhere else, put in the Trash, deleted, or never downloaded/.test(asked.qn) && /GAMES/.test(asked.ctx), asked);
  // a blocked one: never saved
  await rowOf('BAD.EXE').locator('a', {hasText: 'Remove'}).click(); await p.waitForTimeout(400);
  const nb = await q("NC.find(n=>n.bz&&n.bz.n=='BAD.EXE')");
  ok('11. a blocked download: removed with the same notification; Blitzen is told it was never saved', nb && /File removed from list/.test(nb.t) && await q("/never saved/.test(dlWhere(NC.find(n=>n.bz&&n.bz.n=='BAD.EXE').bz).rec)"), nb);
  // speed
  const sp = await q("(()=>{const r={};const t0=Date.now;let now=1000;Date.now=()=>now;dlSpeed(r,0);now=2000;dlSpeed(r,1048576);const a=r.sp;now=3000;dlSpeed(r,3*1048576);Date.now=t0;return [a,r.sp]})()");
  ok('12. the speed from the growing file (1 MB/s, then smoothed toward 2 MB/s)', Math.round(sp[0]) == 1048576 && sp[1] > 1048576 && sp[1] < 2097152, sp);
  // the list without Blitzen (browser-only): no button
  ok('no page errors', !errs.length, errs);
  await b.close(); srv.close();
  const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
