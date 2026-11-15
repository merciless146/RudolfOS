// 2026-11-14: Notepad and WordLite: the Save dialog (folders, a name that can be typed, the file type), the Open
// dialog, document tabs under the buttons, a file opened from elsewhere going into the open window as a tab, unsaved
// tabs asked about; and the old Save As box's typing (nameDlg: its Enter handler cancelled every other key).
//   node editors59_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  q.on('data', () => {}); q.on('end', async () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8')) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8341, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  const p = await b.newPage({viewport: {width: 1280, height: 900}}); const errs = []; p.on('pageerror', e => errs.push(String(e.stack || e).slice(0, 300)));
  await p.goto('http://127.0.0.1:8341/'); await p.waitForFunction(() => typeof fileDlg == 'function' && typeof docTabs == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());node(['C:']).DOCS={};save();return 1})()");
  // the old box: typing works
  await q("nameDlg('NOTE.TXT',n=>window.__nd=n)"); await wait(200);
  const nd = p.locator('.win input.sunk').last(); await nd.click(); await p.keyboard.press('End'); for (let i = 0; i < 8; i++) await p.keyboard.press('Backspace'); await p.keyboard.type('typed.txt'); await p.keyboard.press('Enter'); await wait(200);
  ok('1. the Save As box takes typing again (its Enter handler cancelled every other key)', (await q('window.__nd')) == 'TYPED.TXT', await q('window.__nd'));
  // Notepad: tabs, the Save dialog
  await q("notepad()"); await wait(400);
  const np = "wins.find(w=>w.dtabs&&w.dtabs.app=='notepad')";
  ok('2. Notepad: New, Open, Save, Save As; a tab row under the buttons with one Untitled tab', await q(`(()=>{const W=${np};const mb=W.body.querySelector('.menubar'),tb=W.body.querySelector('.dtabs');return !!mb&&mb.nextElementSibling===tb&&[...mb.querySelectorAll('button')].slice(0,4).map(b=>b.textContent).join()=='New,Open,Save,Save As'&&tb.querySelectorAll('.dtab').length==1&&/Untitled/.test(tb.textContent)})()`));
  await p.locator('.win textarea').last().click(); await p.keyboard.type('first doc');
  await q(`${np}.body.querySelector('.sv').click()`); await wait(300);
  ok('3. Save on a new document: the Save dialog (folders, a name to type, the type list)', await q("(()=>{const o=document.querySelector('.fdlg');return !!o&&!!o.querySelector('.fdname')&&o.querySelector('.fdtype').options.length>=7&&/Text/.test(o.querySelector('.fdtype').options[0].textContent)})()"));
  await q("(()=>{for(let i=0;i<6;i++)document.querySelector('.fdlg .fdup').click();const r=[...document.querySelectorAll('.fdlg .fdr')].find(x=>x.dataset.n=='DOCS');r?r.click():0;return 1})()"); await wait(150);
  const nm = p.locator('.fdlg .fdname'); await nm.click(); await nm.press('Control+a'); await p.keyboard.type('plan');
  await q("(()=>{const s=document.querySelector('.fdlg .fdtype');s.value=[...s.options].findIndex(o=>/^Markdown/.test(o.textContent));s.dispatchEvent(new Event('change'));return 1})()");
  ok('...typing works in it, and the type puts its extension on the name', (await nm.inputValue()).toLowerCase() == 'plan.md', await nm.inputValue());
  await p.keyboard.press('Enter'); await wait(300);
  ok('4. saved where chosen, with the type chosen; the tab and title named', await q("node(['C:','DOCS','PLAN.MD'])=='first doc'") && /PLAN\.MD - Notepad/.test(await q(`${np}.el.querySelector('.t').textContent`)), [await q("JSON.stringify(node(['C:','DOCS']))"), await q(`${np}.el.querySelector('.t').textContent`)]);
  // a second tab, Open
  await q("node(['C:','DOCS'])['OTHER.TXT']='other text';save()");
  await q(`${np}.body.querySelector('.op').click()`); await wait(300);
  await q("(()=>{const s=document.querySelector('.fdlg .fdtype');s.value=[...s.options].findIndex(o=>/^All files/.test(o.textContent));s.dispatchEvent(new Event('change'));for(let i=0;i<6;i++)document.querySelector('.fdlg .fdup').click();const r=[...document.querySelectorAll('.fdlg .fdr')].find(x=>x.dataset.n=='DOCS');r&&r.click();return 1})()"); await wait(150);
  await q("(()=>{const r=[...document.querySelectorAll('.fdlg .fdr')].find(x=>x.dataset.n=='OTHER.TXT');r.dispatchEvent(new MouseEvent('dblclick',{bubbles:true}));return 1})()"); await wait(300);
  ok('5. Open: the file in a second tab, the first kept', await q(`(()=>{const W=${np};return W.body.querySelectorAll('.dtab').length==2&&W.body.querySelector('textarea').value=='other text'})()`));
  await q(`${np}.body.querySelectorAll('.dtab')[0].click()`); await wait(100);
  ok('...switching back shows the first document', (await q(`${np}.body.querySelector('textarea').value`)) == 'first doc');
  await q("notepad(['C:','DOCS','OTHER.TXT'])"); await wait(200);
  ok('6. a file opened from elsewhere while Notepad is open: switched to in the same window, not a second window or tab', (await q("wins.filter(w=>w.dtabs&&w.dtabs.app=='notepad').length")) == 1 && (await q(`${np}.body.querySelectorAll('.dtab').length`)) == 2 && (await q(`${np}.body.querySelector('textarea').value`)) == 'other text');
  // unsaved, then close the tab: asked
  await p.locator('.win textarea').last().click(); await p.keyboard.type(' changed'); await wait(400);
  ok('7. a changed tab shows •', /• OTHER\.TXT/.test(await q(`${np}.body.querySelector('.dtabs').textContent`)), await q(`${np}.body.querySelector('.dtabs').textContent`));
  await q(`${np}.body.querySelector('.dtab.on u').click()`); await wait(200);
  ok('...closing it asks to save', /changes that are not saved/.test(await q("(document.querySelector('.dqa')||{}).textContent||''")));
  await q("[...document.querySelectorAll('.rmodal button')].find(b=>b.textContent=='Save').click()"); await wait(300);
  ok('...Save: saved, the tab closed', (await q("node(['C:','DOCS','OTHER.TXT'])")) == 'other text changed' && (await q(`${np}.body.querySelectorAll('.dtab').length`)) == 1);
  // an existing file: asked before replacing
  await q(`${np}.dtabs.newTab()`); await wait(100); await p.locator('.win textarea').last().click(); await p.keyboard.type('x');
  await q(`${np}.body.querySelector('.sv').click()`); await wait(300);
  await q("(()=>{for(let i=0;i<6;i++)document.querySelector('.fdlg .fdup').click();const r=[...document.querySelectorAll('.fdlg .fdr')].find(x=>x.dataset.n=='DOCS');r&&r.click();const n=document.querySelector('.fdlg .fdname');n.value='PLAN.MD';document.querySelector('.fdlg .fdok').click();return 1})()"); await wait(150);
  ok('8. saving over an existing file asks first', /already exists/.test(await q("document.querySelector('.fdlg .fderr').textContent")) && (await q("node(['C:','DOCS','PLAN.MD'])")) == 'first doc');
  await q("document.querySelector('.fdlg .fdx').click()"); await wait(100);
  // WordLite
  await q("wordpad()"); await wait(400);
  const wl = "wins.find(w=>w.dtabs&&w.dtabs.app=='wordlite')";
  await p.locator('.win .ed').last().click(); await p.keyboard.type('Hello world');
  await q(`${wl}.body.querySelector('.sv').click()`); await wait(300);
  const wt = await q("[...document.querySelectorAll('.fdlg .fdtype option')].map(o=>o.textContent)");
  ok('9. WordLite: the same dialog, its types (rich document, web page, text)', /Rich document/.test(wt[0]) && wt.some(t => /Web page/.test(t)) && wt.some(t => /^Text/.test(t)), wt);
  await q("(()=>{for(let i=0;i<6;i++)document.querySelector('.fdlg .fdup').click();const r=[...document.querySelectorAll('.fdlg .fdr')].find(x=>x.dataset.n=='DOCS');r&&r.click();const n=document.querySelector('.fdlg .fdname');n.value='letter';const s=document.querySelector('.fdlg .fdtype');s.value=[...s.options].findIndex(o=>/^Text/.test(o.textContent));s.dispatchEvent(new Event('change'));document.querySelector('.fdlg .fdok').click();return 1})()"); await wait(300);
  ok('...saved as text: no markup in the file', (await q("node(['C:','DOCS','LETTER.TXT'])")) == 'Hello world', await q("JSON.stringify(node(['C:','DOCS']))"));
  await q("node(['C:','DOCS'])['EVIL.DOC']='<p>ok</p><img src=x onerror=\"window.__pwn=1\">';save()"); await q("wordpad(['C:','DOCS','EVIL.DOC'])"); await wait(500);
  ok('10. a document opened in WordLite: a new tab, its script never runs', (await q(`${wl}.body.querySelectorAll('.dtab').length`)) == 2 && !(await q('window.__pwn')));
  ok('11. no page errors', !errs.length, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
