// 2026-11-14: WordPro's hyperlinks (desktop/wplinks59.js): the dialog's four kinds (web, a place in the document,
// a file, e-mail), the text and ScreenTip, editing, the right-click menu, bookmarks, typed addresses becoming links,
// the Links list, opening each kind, and addresses that are refused.   node wplinks59_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]); q.on('data', () => {}); q.on('end', () => {
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8')) }
  const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8342, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  const ctx = await b.newContext({viewport: {width: 1280, height: 900}}); await ctx.grantPermissions(['clipboard-read', 'clipboard-write'], {origin: 'http://127.0.0.1:8342'});
  const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(String(e.stack || e).slice(0, 300)));
  await p.goto('http://127.0.0.1:8342/'); await p.waitForFunction(() => typeof wplDialog == 'function' && typeof wordpro == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());node(['C:']).DOCS={'GUIDE.PDF':'data:application/pdf;base64,JVBERi0='};save();window.__opened=[];window.browser=u=>__opened.push('web '+u);window.openPath=pp=>__opened.push('file '+pp.join('\\\\'));window.emailCompose=o=>__opened.push('mail '+o.to+' '+o.subject);return 1})()");
  await q("wordpro(null,'<h1>Intro</h1><p>Read the docs here.</p><h2>Details</h2><p>More.</p>')"); await wait(800);
  const W = "wins.find(w=>w.X&&w.X.__wpl)";
  ok('1. WordPro gets the link features', !!(await q(`!!${W}`)));
  // select "docs" and insert a web link
  await q(`(()=>{const X=${W}.X,t=[...X.ed.querySelectorAll('p')][0].firstChild,r=document.createRange();r.setStart(t,9);r.setEnd(t,13);const s=getSelection();s.removeAllRanges();s.addRange(r);X.ed.focus();return 1})()`);
  await q(`${W}.X.linkDialog()`); await wait(150);
  const kinds = await q("[...document.querySelectorAll('.wpldlg .wplk button')].map(b=>b.textContent)");
  ok('2. the dialog: web address, place in this document, file on this computer, e-mail address', kinds.length == 4 && /Web address/.test(kinds[0]) && /Place/.test(kinds[1]) && /File/.test(kinds[2]) && /E-mail/.test(kinds[3]), kinds);
  ok('...the selected text is the text to show', (await q("document.querySelector('.wpldlg .wpltx').value")) == 'docs');
  await q("(()=>{const o=document.querySelector('.wpldlg');o.querySelector('.wplu').value='example.com/docs';o.querySelector('.wpltip').value='The manual';o.querySelector('.wplok').click();return 1})()"); await wait(150);
  let a = await q(`(()=>{const a=${W}.X.ed.querySelector('a[href^="https"]');return a&&[a.getAttribute('href'),a.textContent,a.title]})()`);
  ok('3. a web link: https:// added, its text and ScreenTip', a && a[0] == 'https://example.com/docs' && a[1] == 'docs' && a[2] == 'The manual', a);
  // refused addresses
  await q(`${W}.X.ed.focus();${W}.X.linkDialog()`); await wait(100);
  await q("(()=>{const o=document.querySelector('.wpldlg');o.querySelector('.wpltx').value='x';o.querySelector('.wplu').value='javascript:alert(1)';o.querySelector('.wplok').click();return 1})()"); await wait(100);
  ok('4. a javascript: address is refused', /not a web address|cannot be linked/.test(await q("(document.querySelector('.wpldlg .wplerr')||{}).textContent||''")) && !(await q(`!!${W}.X.ed.querySelector('a[href^="javascript"]')`)));
  await q("document.querySelector('.wpldlg .wplx').click()");
  // a place: the headings are offered (ids given)
  await q(`(()=>{const X=${W}.X,t=[...X.ed.querySelectorAll('p')][1].firstChild,r=document.createRange();r.setStart(t,0);r.setEnd(t,4);const s=getSelection();s.removeAllRanges();s.addRange(r);return 1})()`);
  await q(`${W}.X.linkDialog()`); await wait(100); await q("document.querySelector('.wpldlg .wplk button[data-k=place]').click()");
  const pl = await q("[...document.querySelectorAll('.wpldlg .wplp option')].map(o=>[o.value,o.textContent])");
  ok('5. place in this document: the headings', pl.length == 2 && /Intro/.test(pl[0][1]) && /Details/.test(pl[1][1]), pl);
  await q("(()=>{const o=document.querySelector('.wpldlg');o.querySelector('.wplp').value=o.querySelector('.wplp').options[1].value;o.querySelector('.wplok').click();return 1})()"); await wait(100);
  const ph = await q(`(()=>{const a=[...${W}.X.ed.querySelectorAll('a[href^="#"]')][0];return a&&[a.getAttribute('href'),!!${W}.X.ed.querySelector(a.getAttribute('href'))]})()`);
  ok('...linked to the heading (which got an id)', ph && /^#h-details/.test(ph[0]) && ph[1], ph);
  // bookmarks
  await q(`(()=>{const X=${W}.X,t=[...X.ed.querySelectorAll('p')][1].lastChild,r=document.createRange();r.setStart(t,t.length);r.collapse(true);const s=getSelection();s.removeAllRanges();s.addRange(r);return 1})()`);
  await q(`${W}.X.bookmarkDialog()`); await wait(100); await q("(()=>{const o=document.querySelector('.wplbm');o.querySelector('.wbn').value='end note';o.querySelector('.wbadd').click();return 1})()"); await wait(100);
  ok('6. a bookmark is added and then offered as a place', (await q(`!!${W}.X.ed.querySelector('a.wp-bm#bm-end-note')`)) && (await q(`wplPlaces(${W}.X.ed).some(p=>p.id=='bm-end-note'&&/end note/.test(p.label))`)));
  // a file and an e-mail link
  await q(`${W}.X.ed.focus();${W}.X.linkDialog()`); await wait(100); await q("document.querySelector('.wpldlg .wplk button[data-k=file]').click();document.querySelector('.wpldlg .wplfc').click()"); await wait(300);
  await q("(()=>{const o=document.querySelector('.fdlg');for(let i=0;i<6;i++)o.querySelector('.fdup').click();[...o.querySelectorAll('.fdr')].find(x=>x.dataset.n=='DOCS').click();return 1})()"); await wait(100);
  await q("(()=>{const r=[...document.querySelectorAll('.fdlg .fdr')].find(x=>x.dataset.n=='GUIDE.PDF');r.dispatchEvent(new MouseEvent('dblclick',{bubbles:true}));return 1})()"); await wait(200);
  await q("document.querySelector('.wpldlg .wplok').click()"); await wait(100);
  await q(`${W}.X.ed.focus();${W}.X.linkDialog()`); await wait(100); await q("(()=>{const o=document.querySelector('.wpldlg');o.querySelector('.wplk button[data-k=mail]').click();o.querySelector('.wpltx').value='Write to us';o.querySelector('.wplm').value='hi@example.com';o.querySelector('.wpls').value='Hello there';o.querySelector('.wplok').click();return 1})()"); await wait(100);
  const fm = await q(`[...${W}.X.ed.querySelectorAll('a[href^="file"],a[href^="mailto"]')].map(a=>[a.getAttribute('href'),a.textContent])`);
  ok('7. a file link (file:///C:/…) and an e-mail link with its subject', fm.some(x => x[0] == 'file:///C:/DOCS/GUIDE.PDF' && x[1] == 'GUIDE.PDF') && fm.some(x => x[0] == 'mailto:hi@example.com?subject=Hello%20there' && x[1] == 'Write to us'), fm);
  // Ctrl+click opens each kind
  for (const sel of ['a[href^="https"]', 'a[href^="file"]', 'a[href^="mailto"]']) await q(`${W}.X.ed.querySelector('${sel}').dispatchEvent(new MouseEvent('click',{bubbles:true,cancelable:true,ctrlKey:true}))`);
  const op = await q('__opened');
  ok('8. Ctrl+click: the web link in Oracle, the file in its app, the e-mail in the e-mail window', op.includes('web https://example.com/docs') && op.includes('file C:\\DOCS\\GUIDE.PDF') && op.includes('mail hi@example.com Hello there'), op);
  // right-click menu
  const r = await q(`(()=>{const a=${W}.X.ed.querySelector('a[href^="https"]').getBoundingClientRect();return [a.left+3,a.top+3]})()`);
  await p.mouse.click(r[0], r[1], {button: 'right'}); await wait(200);
  const cm = await q("[...document.querySelectorAll('.cmenu .cmi span')].map(s=>s.textContent)");
  ok('9. right-click a link: Open link, Edit link…, Copy link address, Remove link', ['Open link', 'Edit link…', 'Copy link address', 'Remove link'].every(x => cm.includes(x)), cm);
  await q("[...document.querySelectorAll('.cmenu .cmi')].find(d=>/Remove link/.test(d.textContent)).click()"); await wait(100);
  ok('...Remove link keeps the text', !(await q(`!!${W}.X.ed.querySelector('a[href^="https"]')`)) && /Read the docs here/.test(await q(`${W}.X.ed.textContent`)));
  // typed addresses
  await q(`(()=>{const X=${W}.X,p=document.createElement('p');p.textContent='see\u00a0';X.ed.appendChild(p);const r=document.createRange();r.setStart(p.firstChild,4);r.collapse(true);const s=getSelection();s.removeAllRanges();s.addRange(r);X.ed.focus();return 1})()`);
  await p.keyboard.type('www.rudolf.example'); await p.keyboard.press(' ');
  ok('10. a typed web address followed by a space becomes a link', await q(`!!${W}.X.ed.querySelector('a[href="https://www.rudolf.example"]')`), await q(`${W}.X.ed.lastElementChild.innerHTML`));
  // the Links list
  await q(`${W}.X.linksList()`); await wait(150);
  const ll = await q("[...document.querySelectorAll('.wplls .wlr')].map(r=>r.textContent)");
  ok('11. Links in this document: each link with Go to, Edit, Remove', ll.length >= 4 && ll.every(t => /Go to/.test(t) && /Edit/.test(t) && /Remove/.test(t)), ll);
  await q(`${W}.X.ed.querySelector('#bm-end-note').remove()`); await q("document.querySelector('.wplls .wlx').click()");
  await q(`(()=>{const X=${W}.X,a=document.createElement('a');a.href='#bm-gone';a.textContent='gone';X.ed.appendChild(a);return 1})()`); await q(`${W}.X.linksList()`); await wait(100);
  ok('...a link to a place that is not there is marked ⚠', /⚠ not found: 📍 #bm-gone/.test(await q("document.querySelector('.wplls .wll').textContent")));
  await q("document.querySelector('.wplls .wlrmall').click()"); await wait(100);
  ok('...Remove all links: none left, the text kept', (await q(`${W}.X.ed.querySelectorAll('a[href]').length`)) == 0 && /Read the docs here/.test(await q(`${W}.X.ed.textContent`)));
  ok('12. the Insert menu has Bookmark… and Links in this document…', /Bookmark…/.test(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8')) && /Links in this document…/.test(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8')));
  ok('13. no page errors', !errs.length, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
