// 1.5.9: Oracle's "+" on the bookmarks bar (desktop/bmadd59.js): the bar's rightmost button; its prompt offers three
// ways: by address, from the browsing history, a file on this computer (a document or a PDF); each adds a bookmark
// saved as the ☆'s (bd95bm) and shown on the bar; a private window offers the account's history (and adds none to it); names never become HTML.
// The shell is rudolf-webkit, stood in (as private59_t.js).   node bmadd59_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const srv = http.createServer((q, s) => { const [u0] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', async () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0,engine:"webkit"};window.__out=[];window.webkit={messageHandlers:{rudolfShell:{postMessage:m=>__out.push(m)}}};</script>')) }
    if (u == '/api/events/wait') await new Promise(r => setTimeout(r, 800));
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(u == '/api/events/wait' ? '{"ok":true,"events":""}' : '{"ok":true}') }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8336, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const p = await (await b.newContext({viewport: {width: 1280, height: 800}})).newPage(); const errs = [];
  p.on('pageerror', e => errs.push(String(e.stack || e).slice(0, 300)));
  await p.goto('http://127.0.0.1:8336/'); await p.waitForFunction(() => typeof oBmAdd == 'function' && typeof browser == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';window.__net=false;window.__sys=false;localStorage.removeItem('bd95bm');localStorage.setItem('bd95hist',JSON.stringify([['Old page','https://old.example/'],['Example','https://example.com/a'],['Example','https://example.com/a'],['News','https://news.example/today']]));pinDraw();return 1})()");
  await q("__rwkIn({rudolfShell:'hello',version:'webkit-1.5.9',caps:['tabs','dark']})"); await wait(200);
  await q("browser('about:home')"); await wait(600);
  const bar = await q("(()=>{const b=document.querySelector('.win .obm');const k=[...b.children];return k.map(x=>x.textContent+'|'+(x.title||''))})()");
  ok('1. the bookmarks bar ends with "+", its rightmost button, titled "Add a bookmark"', bar.at(-1) == '+|Add a bookmark' && bar.length > 1, bar);
  const plus = "document.querySelector('.win .obm .obmadd').click()";
  await q(plus); await wait(200);
  const dlg = await q("(()=>{const o=document.querySelector('.obma');return o?[...o.querySelectorAll('.bmaw button')].map(b=>b.textContent):null})()");
  ok('2. it asks how: by address, from the history, or a file on this computer (a document or a PDF)', dlg && dlg.length == 3 && /address \(URL\)/.test(dlg[0]) && /history/.test(dlg[1]) && /file on this computer \(a document or a PDF\)/.test(dlg[2]), dlg);
  // by address: "example.org/x" gets https://
  await q("document.querySelector('.obma .bma-url').click()"); await wait(100);
  await q("(()=>{const o=document.querySelector('.obma');o.querySelector('.bmau').value='example.org/docs';o.querySelector('.bman').value='Docs';o.querySelector('.bmaok').click();return 1})()"); await wait(300);
  let bms = await q("JSON.parse(localStorage.getItem('bd95bm'))");
  ok('3. by address: added with https://, saved, the prompt closed', bms.some(b => b[0] == 'Docs' && b[1] == 'https://example.org/docs') && !(await q("!!document.querySelector('.obma')")), bms);
  ok('...on the bar, before the "+"', (await q("[...document.querySelector('.win .obm').children].map(x=>x.textContent).slice(-2).join('|')")) == 'Docs|+');
  await q(plus); await wait(100); await q("document.querySelector('.obma .bma-url').click()"); await wait(100);
  await q("(()=>{const o=document.querySelector('.obma');o.querySelector('.bmau').value='not an address';o.querySelector('.bmaok').click();return 1})()"); await wait(100);
  ok('...something that is not an address is refused, said', /not an address/.test(await q("document.querySelector('.obma .bmaerr').textContent")));
  await q("(()=>{const o=document.querySelector('.obma');o.querySelector('.bmau').value='https://example.org/docs';o.querySelector('.bmaok').click();return 1})()"); await wait(100);
  ok('...the same page twice is refused, said', /already a bookmark/.test(await q("document.querySelector('.obma .bmaerr').textContent")));
  await q("document.querySelector('.obma .bma-x').click()");
  // from the history: newest first, no repeats, searchable
  await q(plus); await wait(100); await q("document.querySelector('.obma .bma-hist').click()"); await wait(150);
  const rows = await q("[...document.querySelectorAll('.obma .bmah')].map(r=>r.title)");
  ok('4. from the history: newest first, each page once', JSON.stringify(rows) == JSON.stringify(['https://news.example/today', 'https://example.com/a', 'https://old.example/']), rows);
  await q("(()=>{const s=document.querySelector('.obma .bmas');s.value='old';s.dispatchEvent(new Event('input'));return 1})()"); await wait(100);
  ok('...searchable', (await q("document.querySelectorAll('.obma .bmah').length")) == 1);
  await q("document.querySelector('.obma .bmah').click()"); await wait(200);
  bms = await q("JSON.parse(localStorage.getItem('bd95bm'))");
  ok('...a click adds it with its title', bms.some(b => b[0] == 'Old page' && b[1] == 'https://old.example/'), bms);
  // a file on this computer
  const docs = await q("(()=>{let h=USER?homePath('DOCUMENTS'):['C:'];if(!isDir(node(h)))h=['C:'];node(h)['GUIDE.PDF']='data:application/pdf;base64,JVBERi0=';try{fsSave()}catch(e){}return h})()");
  await q(plus); await wait(100); await q("document.querySelector('.obma .bma-file').click()"); await wait(400);
  const pk = await q("(()=>{const w=wins.find(w=>/Bookmark a file/.test(w.tb.title||''));return !!w})()");
  ok('5. a file: the desktop\'s file picker opens', pk);
  await q("(()=>{const w=wins.find(w=>/Bookmark a file/.test(w.tb.title||''));const r=[...w.body.querySelectorAll('.pkr')].find(e=>e.textContent.includes('GUIDE.PDF'));r.dispatchEvent(new MouseEvent('dblclick',{bubbles:true}));return 1})()"); await wait(400);
  bms = await q("JSON.parse(localStorage.getItem('bd95bm'))");
  ok('...the PDF chosen is a bookmark of its own, opened in Oracle as file:///C:/...', bms.some(b => b[0] == 'GUIDE' && b[1] == 'file:///' + docs.concat('GUIDE.PDF').join('/')), [docs, bms]);
  // names never become HTML
  await q(plus); await wait(100); await q("document.querySelector('.obma .bma-url').click()"); await wait(100);
  await q("(()=>{const o=document.querySelector('.obma');o.querySelector('.bmau').value='https://x.example/';o.querySelector('.bman').value='<img src=x onerror=top.__pwn=1>';o.querySelector('.bmaok').click();return 1})()"); await wait(300);
  ok('6. a name with markup stays text', !(await q("!!window.__pwn||!!document.querySelector('.win .obm img')")));
  // a private window: no history to offer
  await q("wins.slice().forEach(w=>w.close())"); await wait(200); await q("oraclePrivate()"); await wait(600);
  await q("document.querySelector('.win .obm .obmadd').click()"); await wait(100); await q("document.querySelector('.obma .bma-hist').click()"); await wait(100);
  ok('7. a private window: the account\'s history is offered (the private window adds none to it)', (await q("document.querySelectorAll('.obma .bmah').length")) == 3, await q("document.querySelector('.obma .bmaf').textContent"));
  ok('8. no page errors', !errs.length, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
