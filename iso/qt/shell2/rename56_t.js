// 1.5.6 (1.5.5's plan item 7): Rename… with several files (a pattern, or find and replace; a preview; clashes in red
// and nothing done; one Undo), and Find duplicates (same size, then SHA-256; the newest kept; the others to the Trash
// only when asked).   node rename56_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 600) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8154, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd|debian/.test(e.stack || e.message || '')) errs.push(e.message) });
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8154/'); await p.waitForFunction(() => window.__rename56 && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    ACCTS.length=0;ACCTS.push({n:'ann',h:await pwMake('a1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'a1');
    const pic=node(homePath('PICTURES'));Object.keys(pic).forEach(k=>delete pic[k]);pic['IMG_0041.JPG']='data:image/jpeg;base64,AAAA';pic['IMG_0042.JPG']='data:image/jpeg;base64,AAAB';pic['IMG_0043.PNG']='data:image/png;base64,AAAC';pic['KEEP.TXT']='k';
    const doc=node(homePath('DOCUMENTS'));Object.keys(doc).forEach(k=>delete doc[k]);doc['A.TXT']='a';doc['B.TXT']='b';doc['C.TXT']='c';fsSave();return 1})()`);
  const Wb = "wins.at(-1).body";
  const openX = async pth => { await q(`(()=>{wins.slice().forEach(w=>w.close());explorer(${pth});return 1})()`); await wait(400) };
  const selAll = async names => q(`(()=>{const B=${Wb},rows=[...B.querySelectorAll('.xpanes [data-k]')].filter(e=>${JSON.stringify(names)}.includes(e.dataset.k.split('/').pop()));rows[0].click();rows.slice(1).forEach(r=>r.dispatchEvent(new MouseEvent('click',{bubbles:true,ctrlKey:true})));return rows.length})()`);
  const f2 = () => q(`(()=>{${Wb}.querySelector('.xc').dispatchEvent(new KeyboardEvent('keydown',{key:'F2',bubbles:true}));return 1})()`);
  await openX("homePath('PICTURES')");
  ok('0. three files selected', await selAll(['IMG_0041.JPG', 'IMG_0042.JPG', 'IMG_0043.PNG']) == 3);
  await f2(); await wait(300);
  const RW = "wins.find(w=>/Rename 3 items/.test(w.title||w.el.textContent))";
  ok('1. Rename with several files opens the batch window', await q(`!!${RW}`));
  await q(`(()=>{const B=${RW}.body,i=B.querySelector('.bpat');i.value='holiday ###';i.oninput();return 1})()`); await wait(100);
  const pv = await q(`[...${RW}.body.querySelectorAll('.brt tbody tr')].map(r=>r.cells[1].textContent)`);
  ok('2. a preview of every new name: numbered, in capitals, each keeping its extension', JSON.stringify(pv) == '["HOLIDAY 001.JPG","HOLIDAY 002.JPG","HOLIDAY 003.PNG"]', pv);
  ok('3. nothing has changed yet', await q("Object.keys(node(homePath('PICTURES'))).join()") == 'IMG_0041.JPG,IMG_0042.JPG,IMG_0043.PNG,KEEP.TXT');
  await q(`${RW}.body.querySelector('.ba').click()`); await wait(400);
  ok('4. Rename: all three are renamed', await q("Object.keys(node(homePath('PICTURES'))).sort().join()") == 'HOLIDAY 001.JPG,HOLIDAY 002.JPG,HOLIDAY 003.PNG,KEEP.TXT', await q("Object.keys(node(homePath('PICTURES')))"));
  await q(`(()=>{${Wb}.querySelector('.xc').dispatchEvent(new KeyboardEvent('keydown',{key:'z',ctrlKey:true,bubbles:true}));return 1})()`); await wait(300);
  ok('5. one Undo puts all the names back', await q("Object.keys(node(homePath('PICTURES'))).sort().join()") == 'IMG_0041.JPG,IMG_0042.JPG,IMG_0043.PNG,KEEP.TXT', await q("Object.keys(node(homePath('PICTURES')))"));
  // a clash: find and replace makes A.TXT into C.TXT, which is already there and not being renamed
  await openX("homePath('DOCUMENTS')"); await selAll(['A.TXT', 'B.TXT']); await f2(); await wait(300);
  const RW2 = "wins.find(w=>/Rename 2 items/.test(w.title||w.el.textContent))";
  await q(`(()=>{const B=${RW2}.body;B.querySelector("input[value='replace']").click();const f=B.querySelector('.bf');f.value='a';f.oninput();const r=B.querySelector('.br');r.value='c';r.oninput();return 1})()`); await wait(100);
  const cl = await q(`(()=>{const B=${RW2}.body;return{red:[...B.querySelectorAll('.brt tbody tr.brerr')].map(r=>r.cells[1].textContent),dis:B.querySelector('.ba').disabled,msg:B.querySelector('.bre').textContent}})()`);
  ok('6. a clash is shown in red, with why, and Rename cannot be pressed', JSON.stringify(cl.red) == '["C.TXT"]' && cl.dis && /already called C\.TXT/.test(cl.msg), cl);
  await q(`(()=>{const B=${RW2}.body,r=B.querySelector('.br');r.value='d';r.oninput();B.querySelector('.ba').click();return 1})()`); await wait(300);
  ok('7. fixed (A → D): renamed', await q("Object.keys(node(homePath('DOCUMENTS'))).sort().join()") == 'B.TXT,C.TXT,D.TXT', await q("Object.keys(node(homePath('DOCUMENTS')))"));
  // duplicates
  await q(`(()=>{wins.slice().forEach(w=>w.close());const now=Date.now(),D=homePath('DOCUMENTS'),P=homePath('PICTURES');node(D)['REPORT.TXT']='the same words';node(P)['REPORT COPY.TXT']='the same words';node(P)['OTHER.TXT']='the same wordz';
    META['/'+[...D,'REPORT.TXT'].join('/')]={m:now-864e5};META['/'+[...P,'REPORT COPY.TXT'].join('/')]={m:now};fsSave();TR.length=0;trSave();dupWindow(homePath());return 1})()`);
  await p.waitForFunction(() => /group|No duplicates/.test((wins.at(-1).body.querySelector('.dps') || {}).textContent || ''), null, {timeout: 10000}).catch(() => {});
  const dp = await q(`(()=>{const B=${Wb};return{msg:B.querySelector('.dps').textContent,rows:[...B.querySelectorAll('.dpl label')].map(l=>({t:l.querySelector('span').textContent,c:l.querySelector('input').checked,k:l.querySelector('em').textContent}))}})()`);
  ok('8. one group of identical files (the same size and SHA-256; a file of the same size with other words is not in it)', /^1 group/.test(dp.msg) && dp.rows.length == 2 && !dp.rows.some(r => /OTHER/.test(r.t)), dp);
  ok('9. the newest is kept; the other is ticked for the Trash', /REPORT COPY\.TXT$/.test(dp.rows[0].t) && !dp.rows[0].c && /keep/.test(dp.rows[0].k) && dp.rows[1].c, dp.rows);
  await q(`(()=>{const c=${Wb}.querySelector('.dpl label input');c.checked=true;c.onchange();return 1})()`);
  ok('10. one copy of each file is always kept', !(await q(`${Wb}.querySelector('.dpl label input').checked`)) && /always kept/.test(await q(`${Wb}.querySelector('.dps').textContent`)));
  ok('11. nothing went to the Trash before asking', await q("TR.length") == 0);
  await q(`${Wb}.querySelector('.dpt').click()`); await wait(300);
  ok('12. "Move the others to the Trash": the older copy is in the Trash (and can be restored)', await q("TR.map(x=>x.n).join()") == 'REPORT.TXT' && await q("!('REPORT.TXT' in node(homePath('DOCUMENTS')))"), await q("TR.map(x=>x.n)"));
  await q("(()=>{wins.slice().forEach(w=>w.close());settings('stor');return 1})()"); await wait(500);
  ok('13. Settings › Storage and backup has "Find duplicates…"', await q("!!document.querySelector('.dup56')"));
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
