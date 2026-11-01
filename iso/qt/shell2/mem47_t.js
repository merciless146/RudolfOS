// 1.4.7: a tab the memory guard closed (mem47.js): Oracle's own page, the notification, Reload; other stops keep
// the plain message. node mem47_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path'), os = require('os');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www'), SHOT = process.env.SHOTS || path.join(os.tmpdir(), 'mem47-shots'); fs.mkdirSync(SHOT, {recursive: true});
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 300) : '')) };
const srv = http.createServer((q, s) => { const u = q.url.split('?')[0];
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }
  s.writeHead(404); s.end() }).listen(8093, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 760}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  // the installed system's rudolfd: /api/memory answers from MEMR (set by the test), other requests fail quietly
  await p.addInitScript(() => { window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0}; window.MEMR = {now: 0, events: []};
    const f0 = window.fetch; window.fetch = (u, o) => String(u).includes('/api/memory') ? Promise.resolve(new Response(JSON.stringify(Object.assign({ok: true}, window.MEMR)), {headers: {'content-type': 'application/json'}})) : String(u).includes('/api/') ? Promise.resolve(new Response('{"ok":false,"error":"test"}', {status: 500, headers: {'content-type': 'application/json'}})) : f0(u, o) });
  await p.goto('http://127.0.0.1:8093/'); await p.waitForFunction(() => typeof memGone == 'function' && typeof browser == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>{w.__force=1;w.close()});USER=USER||ACCTS[0];return 1})()");
  // a stand-in tab and Oracle window parts; rudolfd answers /api/memory
  await q(`(()=>{window.NOTES=[];const n0=notify;window.notify=(a,b,o)=>{NOTES.push([a,b,o&&o.kind]);return n0(a,b,o)};
    window.MEMR={now:1000,critical:false,events:[{time:990,pid:4242,rss:1700000000}]};
    const fr=document.createElement('iframe');fr.style.cssText='position:fixed;left:40px;top:40px;width:900px;height:600px;border:1px solid #999;background:#fff';document.body.appendChild(fr);
    window.FT={h:['https://www.newgrounds.com/portal/view/770371'],i:0,fr,title:'Friday Night Funkin\\'',real:true};window.SYNCED=0;window.RELOADED=0;
    window.ST={textContent:''};window.FO={T:()=>FT,sync:()=>{SYNCED++},reload:()=>{RELOADED++},st:ST};return 1})()`);
  await q("memGone(FT,'killed',FO)"); await wait(800);
  ok('1. a tab ended by the memory guard (reason killed, an event 10 s ago) is no longer shown as a real tab', await q("FT.real===false&&SYNCED>0"));
  ok('2. it shows Oracle\'s page: closed because it used too much memory, with the page\'s name and Reload', await (async () => { const t = await p.frameLocator('iframe >> nth=-1').locator('body').textContent({timeout: 5000}).catch(() => ''); return /used too much memory/.test(t) && /Friday Night Funkin/.test(t) && await p.frameLocator('iframe >> nth=-1').locator('#r').count() == 1 })(), await q("FT.fr.srcdoc.slice(0,200)"));
  ok('3. a notification names the page', await q("NOTES.some(n=>n[0]=='Page closed to free memory'&&/Friday Night Funkin/.test(n[1]))"), await q("NOTES"));
  ok('4. the status bar says how to open it again', await q("/Reload \\(F5\\)/.test(ST.textContent)"));
  await wait(1000); await p.locator('iframe').last().screenshot({path: path.join(SHOT, 'mem-page.png')});
  await p.frameLocator('iframe >> nth=-1').locator('#r').click(); await wait(300);
  ok('5. its Reload button opens the page again (Oracle\'s reload)', await q("RELOADED==1"), await q("RELOADED"));
  // a tab that stopped for another reason: no memory page
  await q("FT.real=true;SYNCED=0;NOTES.length=0;MEMR={now:5000,critical:false,events:[{time:990,pid:4242,rss:1700000000}]}");
  await q("memGone(FT,'crashed',FO)"); await wait(500);
  ok('6. a page that crashed with no recent memory-guard event keeps the plain message (no memory page, no notification)', await q("FT.real===true&&SYNCED==0&&!NOTES.length"));
  await q("memGone(FT,'unresponsive',FO)"); await wait(300);
  ok('7. a page that is not responding is not called a memory problem', await q("FT.real===true&&!NOTES.length"));
  await q("memGone(FT,'oom',FO)"); await wait(500);
  ok('8. a page the kernel ended for memory (reason oom) gets the memory page', await q("FT.real===false&&NOTES.length==1"));
  await q("FT.real=true;NOTES.length=0;MEMR={now:5000,critical:true,events:[]}"); await q("memGone(FT,'killed',FO)"); await wait(500);
  ok('9. ...and one ended while memory is critical', await q("FT.real===false&&NOTES.length==1"));
  await q("WEB_DARK=true;FT.real=true;NOTES.length=0;MEMR={now:1000,critical:false,events:[{time:999,pid:1,rss:1}]}"); await q("memGone(FT,'killed',FO)"); await wait(500);
  ok('10. with Oracle\'s dark pages on, the memory page is dark too', await q("/rudolf-dark/.test(FT.fr.srcdoc)"));
  ok('11. no page errors', !errs.length, errs);
  console.log(`${R.filter(x => x).length}/${R.length} passed`); await b.close(); srv.close(); process.exit(R.every(x => x) ? 0 : 1);
})();
