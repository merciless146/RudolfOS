// 1.5.6 (1.5.5's plan items 2 and 3): File Explorer sorts in every view and remembers each folder's order for each
// account, Downloads opens newest first, "Folders first" can be switched off; favourites take files too, follow a
// rename, stay greyed once deleted, and each account has its own.   node fe56_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8151, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd|debian/.test(e.stack || e.message || '')) errs.push(e.message) });
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8151/'); await p.waitForFunction(() => typeof explorer == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    ACCTS.length=0;ACCTS.push({n:'ann',h:await pwMake('a1'),r:'admin'});ACCTS.push({n:'bob',h:await pwMake('b1'),r:'user'});saveUser();
    window.IN=async n=>{if(USER)userLogout();USER=ACCTS.find(a=>a.n==n);await userLogin(USER,n[0]+'1');return 1};await IN('ann');
    const d=node(homePath('DOCUMENTS'));Object.keys(d).forEach(k=>delete d[k]);d['B.TXT']='bb';d['A.TXT']='a';d['C.TXT']='cccc';d['ZDIR']={};const now=Date.now();
    const mt=(n,t)=>{META['/'+[...homePath('DOCUMENTS'),n].join('/')]={c:t,m:t,o:'ann'}};mt('A.TXT',now-3e6);mt('B.TXT',now-1e6);mt('C.TXT',now-2e6);
    const dl=node(downloadsPath());dl['OLD.ZIP']='x';dl['NEW.ZIP']='y';META['/'+[...downloadsPath(),'OLD.ZIP'].join('/')]={m:now-9e6};META['/'+[...downloadsPath(),'NEW.ZIP'].join('/')]={m:now-1e3};fsSave();return 1})()`);
  // the names in the pane, in their order (any view)
  const order = () => q("[...wins.at(-1).body.querySelectorAll('.xpanes [data-k]')].map(e=>e.dataset.k.split('/').pop())");
  const W = () => "wins.at(-1).body";
  const open = async pth => { await q(`(()=>{wins.slice().forEach(w=>w.close());explorer(${pth});return 1})()`); await wait(400) };
  await open("homePath('DOCUMENTS')");
  ok('1. Details sorts by name, folders first', JSON.stringify(await order()) == '["ZDIR","A.TXT","B.TXT","C.TXT"]', await order());
  await q(`(()=>{const v=${W()}.querySelector('.vw');v.value='icons';v.onchange({target:v});return 1})()`); await wait(200);
  await q(`(()=>{const s=${W()}.querySelector('.so');s.value='size';s.onchange({target:s});return 1})()`); await wait(200);
  ok('2. the Icons view sorts too: Sort by Size (largest first)', JSON.stringify(await order()) == '["ZDIR","C.TXT","B.TXT","A.TXT"]', await order());
  await q(`${W()}.querySelector('.sod').click()`); await wait(200);
  ok('3. the direction button turns it around (smallest first)', JSON.stringify(await order()) == '["ZDIR","A.TXT","B.TXT","C.TXT"]', await order());
  await q(`(()=>{const s=${W()}.querySelector('.so');s.value='date';s.onchange({target:s});return 1})()`); await wait(200);
  await open("homePath('DOCUMENTS')");
  ok('4. closed and opened again: the folder keeps its order (Date modified, newest first)', JSON.stringify(await order()) == '["ZDIR","B.TXT","C.TXT","A.TXT"]' && await q(`${W()}.querySelector('.so').value`) == 'date', await order());
  await open("downloadsPath()");
  ok('5. Downloads opens newest first', JSON.stringify(await order()) == '["NEW.ZIP","OLD.ZIP"]', await order());
  await open("homePath('DOCUMENTS')");
  await q("svj('bd95xfold:'+uid(),false)"); await open("homePath('DOCUMENTS')");
  ok('6. "Folders first" off: the folder sorts among the files', (await order())[0] != 'ZDIR', await order()); await q("svj('bd95xfold:'+uid(),true)");
  await open("homePath('PICTURES')");
  ok('7. another folder still has the default order (Name)', await q(`${W()}.querySelector('.so').value`) == 'name');
  // favourites: a file
  await q("favToggle([...homePath('DOCUMENTS'),'B.TXT']);1"); await open("homePath('PICTURES')");
  const side = () => q(`[...${W()}.querySelectorAll('.xside .xsi')].map(e=>({t:e.textContent,gone:e.classList.contains('xgone')}))`);
  let sd = await side();
  ok('8. a favourite file is listed in the sidebar with its icon', sd.some(x => /B\.TXT/.test(x.t) && !x.gone), sd);
  await q("window.__op=[];window.__opO=openPath;openPath=function(p){__op.push(p.join('\\\\'))};1");
  await q(`[...${W()}.querySelectorAll('.xside .xsi')].find(e=>/B\\.TXT/.test(e.textContent)).click()`); await wait(200);
  ok('9. clicking it opens the file in its app', (await q("__op")).some(x => /DOCUMENTS\\B\.TXT$/.test(x)), await q("__op")); await q("openPath=__opO;1");
  // rename it in File Explorer (F2)
  await open("homePath('DOCUMENTS')");
  await q(`(()=>{const r=${W()}.querySelector('[data-k$="/B.TXT"]');r.click();return 1})()`); await wait(100);
  await q(`(()=>{const c=${W()}.querySelector('.xc');c.dispatchEvent(new KeyboardEvent('keydown',{key:'F2',bubbles:true}));return 1})()`); await wait(200);
  await q(`(()=>{const i=${W()}.querySelector('.xpanes input');i.value='RENAMED.TXT';i.dispatchEvent(new KeyboardEvent('keydown',{key:'Enter',bubbles:true}));return 1})()`); await wait(400);
  sd = await side();
  ok('10. renamed in File Explorer: the favourite follows it', sd.some(x => /RENAMED\.TXT/.test(x.t) && !x.gone) && !sd.some(x => /B\.TXT/.test(x.t)), sd);
  await q("trashPut([[...homePath('DOCUMENTS'),'RENAMED.TXT']]);EXPL.forEach(f=>f());1"); await wait(300); sd = await side();
  ok('11. deleted: the favourite stays, greyed', sd.some(x => /RENAMED\.TXT/.test(x.t) && x.gone), sd);
  // the folder favourite still works, and bob has his own
  await q("favToggle(homePath('MUSIC'));1");
  await q("IN('bob')"); await wait(500); await open("homePath()"); sd = await side();
  ok('12. another account has its own favourites (none of ann\'s)', !sd.some(x => /RENAMED|MUSIC/.test(x.t)), sd);
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
