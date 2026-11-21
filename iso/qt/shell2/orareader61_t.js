// 1.6.1 (docs/plan-1.6.1.md Part 10, docs/plan-oracle.md item 7): Oracle's reader mode (desktop/orareader61.js). A. the tabs' check for an
// article (shell.js READABLE_JS, and rudolf-webkit's copy the same): an article page yes, a short page no. B. the desktop with a stood-in
// shell (rudolf-webkit's protocol, the same tab messages as the Chromium shell): the 📖 button only when the tab says its page has an
// article; pressed, the tab is asked for its HTML (readhtml) and the article is shown in the tab's frame: its title, author and text,
// nothing of the page's that runs (a <script>, an onerror, a javascript: link, a form) and a Content-Security-Policy that lets only
// Oracle's own script run; the website's tab hidden meanwhile, the address unchanged; A+ and the colours (kept in the settings; dark with
// Rudolf's dark theme); read aloud sentence by sentence with the sentence highlighted, and Stop; Print (the article alone); a link opens
// in the tab and leaves reader mode; ✕ Exit reader and F9 (from the page too) give the page back; ☰ › Reader mode; About Oracle credits
// Readability; an .HTM file on C: too; a website in a plain frame (no shell) has no button.   node orareader61_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const api = a => a == 'events' ? {ok: true, events: ''} : a == 'info' ? {ok: true, backlight: false, live: 0, boot: 'uefi', kernel: 'test'} : {ok: true};
let WK = true;
const PARA = 'Reindeer are well adapted to the cold: their fur traps air, their noses warm the air they breathe, and their hooves change with the seasons, soft in summer and hard in winter. ';
const ARTICLE = `<!doctype html><html lang="en"><head><meta charset=utf-8><title>The reindeer's year - Test News</title><meta name="author" content="Ann Writer"></head><body>
<header class="site-header"><nav class="menu"><a href="/">Home</a> <a href="/news">News</a></nav></header>
<div class="sidebar">Subscribe now! Ads here.</div>
<article><h1>The reindeer's year</h1><p class="byline">By Ann Writer</p>
${Array.from({length: 8}, (_, i) => `<p>${i}. ${PARA.repeat(2)}<a href="/more${i}">more ${i}</a></p>`).join('\n')}
<p>An image: <img src="/pic.png" onerror="parent.postMessage({evil:'onerror'},'*')" alt="pic"></p>
<p>A bad link: <a href="javascript:parent.postMessage({evil:'js'},'*')">click</a>. ${PARA}</p>
<form action="/steal"><input name=x value=secret><button>Send</button></form>
<script>parent.postMessage({evil:'script'},'*')</script>
</article><footer class="footer">Copyright footer text.</footer></body></html>`;
const SHORT = '<!doctype html><title>Short</title><p>Just a line.</p>';
const srv = http.createServer((q, s) => { const [u0] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', async () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0,engine:"webkit"};window.__out=[];' + (WK ? 'window.webkit={messageHandlers:{rudolfShell:{postMessage:m=>__out.push(m)}}};' : '') + '</script>')) }
    if (u == '/article') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(ARTICLE) }
    if (u == '/short') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(SHORT) }
    if (u == '/api/events/wait') await new Promise(r => setTimeout(r, 800));
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5) == 'events/wait' ? 'events' : u.slice(5)))) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8614, '127.0.0.1');
const prep = "(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';window.__net=false;window.__sys=false;return 1})()";
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const ctx = await b.newContext({viewport: {width: 1280, height: 800}});
  // A. the tabs' check
  {const src = fs.readFileSync(path.join(ROOT, 'iso/shell/shell.js'), 'utf8'), wk = fs.readFileSync(path.join(ROOT, 'iso/sys/rudolf-webkit.c'), 'utf8');
   const m = /^const READABLE_JS = (`[\s\S]*?`);\n/m.exec(src); const code = m ? eval(m[1]) : 'false';
   const cs = /static const char READABLE_JS\[\] = ((?:"(?:[^"\\]|\\.)*"\s*)+);/.exec(wk); const wcode = cs ? cs[1].match(/"((?:[^"\\]|\\.)*)"/g).map(x => JSON.parse(x)).join('') : '';
   ok('A0. shell.js and rudolf-webkit.c have the same check (READABLE_JS), and the ops readhtml', !!m && code.replace(/\s+/g, '') == wcode.replace(/\s+/g, '') && /readhtml: \(m, t\)/.test(src) && /tab_reader_op/.test(wk));
   const p = await ctx.newPage(); await p.goto('http://127.0.0.1:8614/article');
   ok('A1. an article page: yes', await p.evaluate(code) === true);
   await p.goto('http://127.0.0.1:8614/short'); ok('A2. a short page: no', await p.evaluate(code) === false);
   await p.close()}
  // B. the desktop
  const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.stack.slice(0, 300)));
  await p.goto('http://127.0.0.1:8614/'); await p.waitForFunction(() => typeof browser == 'function' && typeof rtOn == 'function' && typeof oraReader == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s); await q(prep);
  await q("window.__evil=[];addEventListener('message',e=>{if(e.data&&e.data.evil)__evil.push(e.data.evil)});window.__said=[];ttsSpeak=(t,o)=>{__said.push(t);setTimeout(()=>o&&o.onend&&o.onend(true),30);return true};ttsStop=()=>{__said.push('<stop>')};window.__printed=null;wpPrint=(W,h,o)=>{__printed={h,o};return true}");
  await q("__rwkIn({rudolfShell:'hello',version:'webkit-1.6.1',caps:['tabs']})"); await p.waitForTimeout(200);
  await q("browser('http://127.0.0.1:8614/article')"); await p.waitForTimeout(600);
  const tid = await q("(__out.find(m=>m.rudolfShell=='tab'&&m.op=='open')||{}).tid");
  const ev = o => q(`__rwkIn(Object.assign({rudolfShell:'tabev',tid:${JSON.stringify(tid)}},${JSON.stringify(o)}))`);
  const W = "wins.find(w=>w.app=='browser'&&w.el.isConnected)";
  const btn = () => q(`(()=>{const b=${W}.body.querySelector('.ord');return b?{shown:!b.hidden,on:b.getAttribute('aria-pressed')=='true',after:b.previousElementSibling&&b.previousElementSibling.className}:null})()`);
  await ev({ev: 'load', loading: false}); await ev({ev: 'title', title: "The reindeer's year"});
  ok('B0. no article said yet: no 📖 button (it sits after the address box)', JSON.stringify(await btn()) == JSON.stringify({shown: false, on: false, after: 'ad'}), await btn());
  await ev({ev: 'readable', on: true}); await p.waitForTimeout(100);
  ok('B1. the tab says its page has an article: 📖 shows', (await btn()).shown);
  await q(`${W}.body.querySelector('.ord').click()`); await p.waitForTimeout(150);
  const rq = await q("__out.filter(m=>m.op=='readhtml').pop()||null");
  ok('B2. pressed: the tab is asked for its page (readhtml)', rq && rq.tid == tid, rq);
  await ev({ev: 'readhtml', id: rq.id, html: ARTICLE, url: 'http://127.0.0.1:8614/article', title: "The reindeer's year"}); await p.waitForTimeout(1500);
  const fr = p.frameLocator('.win.act .fh iframe[srcdoc]');
  const art = await fr.locator('body').evaluate(() => ({t: document.getElementById('t').textContent, m: (document.getElementById('m') || {}).textContent || '', n: document.querySelectorAll('main p').length, text: document.querySelector('main').textContent,
    scripts: [...document.querySelectorAll('script')].map(s => s.hasAttribute('nonce')), on: document.querySelector('main').innerHTML.match(/\son[a-z]+=/gi) || [], js: !!document.querySelector('a[href^="javascript"]'), form: !!document.querySelector('main form,main input,main button'),
    csp: (document.querySelector('meta[http-equiv="Content-Security-Policy"]') || {}).content || '', img: (document.querySelector('main img') || {}).src || '', link: (document.querySelector('main a[href*="more1"]') || {}).href || ''})).catch(e => ({err: String(e)}));
  ok('B3. the article: its title and author', /^The reindeer's year/.test(art.t) && /Ann Writer/.test(art.m), art);
  ok('B4. ...its text, without the menu, the sidebar or the footer', art.n >= 8 && /hooves change/.test(art.text) && !/Subscribe now/.test(art.text) && !/Copyright footer/.test(art.text), art.text && art.text.slice(0, 200));
  ok('B5. nothing of the page runs: no script but Oracle\'s own (with its nonce), no on…= attribute, no javascript: link, no form', art.scripts && art.scripts.length == 1 && art.scripts[0] && !art.on.length && !art.js && !art.form, art);
  ok('B6. a Content-Security-Policy: only the nonce\'s script, pictures, nothing else', /default-src 'none'/.test(art.csp) && /script-src 'nonce-[0-9a-f]{24}'/.test(art.csp) && /img-src https: http: data:/.test(art.csp), art.csp);
  ok('B7. pictures and links made whole (the site\'s addresses)', art.img == 'http://127.0.0.1:8614/pic.png' && art.link == 'http://127.0.0.1:8614/more1', [art.img, art.link]);
  await p.waitForTimeout(300); ok('B8. nothing the page had tried to run did (script, onerror, javascript:)', (await q('__evil')).length == 0, await q('__evil'));
  const pl = await q(`__out.filter(m=>m.op=='place'&&m.tid==${JSON.stringify(tid)}).pop()`);
  ok('B9. the website\'s tab is hidden meanwhile; the address is still the page\'s; 📖 pressed', pl && pl.r === null && (await q(`${W}.body.querySelector('.ad').value`)) == 'http://127.0.0.1:8614/article' && (await btn()).on, [pl, await btn()]);
  // A+ and the colours
  const sz0 = await fr.locator('body').evaluate(() => getComputedStyle(document.body).fontSize);
  await fr.locator('#lg').click(); await p.waitForTimeout(150);
  const sz1 = await fr.locator('body').evaluate(() => getComputedStyle(document.body).fontSize);
  ok('B10. A+: larger text, kept in the account\'s settings', parseFloat(sz1) == parseFloat(sz0) + 2 && (await q('S.oreader&&S.oreader.sz')) == parseFloat(sz1), [sz0, sz1, await q('S.oreader')]);
  await fr.locator('#th').selectOption('sepia'); await p.waitForTimeout(150);
  ok('B11. Sepia', (await fr.locator('body').evaluate(() => document.documentElement.dataset.th)) == 'sepia' && (await q('S.oreader.th')) == 'sepia');
  // read aloud
  await fr.locator('#rd').click(); await p.waitForTimeout(400);
  const said = await q('__said.slice()'); const hl = await fr.locator('body').evaluate(() => CSS.highlights.has('rd-say'));
  ok('B12. Read aloud: sentence by sentence in Rudolf\'s voice (ttsSpeak), the title first', said.length >= 3 && /The reindeer/.test(said[0]) && said.every(x => x.length < 400), said.slice(0, 4));
  ok('B13. ...the sentence being read highlighted; the button says Stop reading', hl && /Stop reading/.test(await fr.locator('#rd').textContent()), hl);
  await fr.locator('#rd').click(); await p.waitForTimeout(200);
  ok('B14. Stop: the voice stops, the highlight goes', (await q('__said.includes("<stop>")')) && !(await fr.locator('body').evaluate(() => CSS.highlights.has('rd-say'))) && /Read aloud/.test(await fr.locator('#rd').textContent()));
  await fr.locator('#pr').click(); await p.waitForTimeout(150); const pr = await q('__printed');
  ok('B15. Print: the article alone (its title and text, not the page)', pr && /<h1>The reindeer's year[^<]*<\/h1>/.test(pr.h.replace(/&#39;/g, "'")) && /hooves change/.test(pr.h) && !/Subscribe now/.test(pr.h), pr && pr.h.slice(0, 120));
  await q(`(()=>{const b=${W}.body.querySelector('.mn');b.click();return 1})()`); await p.waitForTimeout(100);
  const items = await q(`[...${W}.body.querySelectorAll('.odd>div')].map(d=>d.textContent)`); await q(`${W}.body.querySelector('.mn').click()`);
  ok('B16. ☰ says Exit reader mode (F9)', items.includes('Exit reader mode (F9)'), items);
  await fr.locator('#x').click(); await p.waitForTimeout(300);
  const pl2 = await q(`__out.filter(m=>m.op=='place'&&m.tid==${JSON.stringify(tid)}).pop()`);
  ok('B17. ✕ Exit reader: the website\'s tab shown again (the page as it was: not loaded again)', pl2 && Array.isArray(pl2.r) && !(await btn()).on && !(await q("__out.some(m=>m.op=='reload'||m.op=='go')")), pl2);
  // F9 from the page, and a link in the article
  await ev({ev: 'keys', k: 'f9'}); await p.waitForTimeout(150); const rq2 = await q("__out.filter(m=>m.op=='readhtml').pop()");
  ok('B18. F9 inside the website: reader mode asked for again', rq2 && rq2.id != rq.id, rq2);
  await ev({ev: 'readhtml', id: rq2.id, html: ARTICLE, url: 'http://127.0.0.1:8614/article', title: 'x'}); await p.waitForTimeout(1200);
  await fr.locator('main a[href*="more2"]').click(); await p.waitForTimeout(300);
  const go = await q("__out.filter(m=>m.op=='go').pop()||null");
  ok('B19. a link in the article: opens in the tab, reader mode left', go && go.url == 'http://127.0.0.1:8614/more2' && !(await btn()).on, [go, await btn()]);
  await ev({ev: 'nav', url: 'http://127.0.0.1:8614/more2', back: true, fwd: false}); await ev({ev: 'readable', on: false}); await p.waitForTimeout(150);
  ok('B20. a page without an article: no button', !(await btn()).shown);
  // dark by default with Rudolf's dark theme
  await q("S.dm='dark';delete S.oreader"); await ev({ev: 'readable', on: true}); await q(`${W}.body.querySelector('.ord').click()`); await p.waitForTimeout(100);
  const rq3 = await q("__out.filter(m=>m.op=='readhtml').pop()"); await ev({ev: 'readhtml', id: rq3.id, html: ARTICLE, url: 'http://127.0.0.1:8614/more2', title: 'x'}); await p.waitForTimeout(1200);
  ok('B21. Rudolf\'s dark theme: the reader is dark', (await fr.locator('body').evaluate(() => document.documentElement.dataset.th)) == 'dark');
  await q("S.dm='light'"); await p.keyboard.press('F9').catch(() => {}); await q(`${W}.body.querySelector('.ord').click()`); await p.waitForTimeout(200);
  // Oracle's own pages: About credits Readability; an .HTM file on C:
  await q("browser('about:oracle')"); await p.waitForTimeout(500);
  const about = await p.frameLocator('.win.act iframe:visible').locator('body').textContent();
  ok('B22. About Oracle credits Readability.js (Mozilla, Apache License 2.0)', /Readability\.js by Mozilla \(Apache License 2\.0\)/.test(about), about.slice(-200));
  await q(`fs['C:']['ARTICLE.HTM']=${JSON.stringify(ARTICLE.replace(/<script>[\s\S]*?<\/script>/, ''))};wins.slice().forEach(w=>w.close());browser('file:///C:/ARTICLE.HTM')`); await p.waitForTimeout(700);
  ok('B23. an .HTM file on C: with an article: 📖 shows', (await btn()).shown, await btn());
  await q(`${W}.body.querySelector('.ord').click()`); await p.waitForTimeout(1200);
  const t2 = await p.frameLocator('.win.act .fh iframe[srcdoc]').locator('#t').textContent().catch(() => '');
  ok('B24. ...its article in reader mode', /^The reindeer's year/.test(t2), t2);
  await q(`${W}.body.querySelector('.ord').click()`); await p.waitForTimeout(500);
  ok('B25. ...📖 again: the file as it was', !(await btn()).on && (await p.frameLocator('.win.act iframe:visible').locator('.sidebar').count()) == 1);
  ok('no page errors', !errs.length, errs);
  // C. a website in a plain frame (no shell)
  WK = false; const p2 = await ctx.newPage(); const errs2 = []; p2.on('pageerror', e => errs2.push(e.stack.slice(0, 300)));
  await p2.goto('http://127.0.0.1:8614/'); await p2.waitForFunction(() => typeof browser == 'function' && typeof oraReader == 'function', null, {timeout: 30000});
  await p2.evaluate(prep); await p2.evaluate("browser('http://127.0.0.1:8614/article')"); await p2.waitForTimeout(1000);
  ok('C1. a website in a plain frame (no shell): no 📖 button', await p2.evaluate(`${W}.body.querySelector('.ord').hidden`));
  ok('no page errors (frames)', !errs2.length, errs2);
  await b.close(); srv.close();
  const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
