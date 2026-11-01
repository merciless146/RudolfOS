// 1.4.5 the Office apps together (office45.js): autosave and recovery after a crash (SpreadsheetPro and PresentPro),
// the Start menu's Office group, Settings › Apps' default apps, the App Store's built-in apps, and the look they share.
// node office_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
const errors = [];
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]); if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'))) } s.writeHead(404); s.end() }).listen(8093, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const ctx = await b.newContext({viewport: {width: 1280, height: 760}});
  const open = async () => { const p = await ctx.newPage(); p.on('pageerror', e => { errors.push(e.message); console.log('page error:', e.stack.slice(0, 300)) });
    await p.goto('http://127.0.0.1:8093/'); await p.waitForFunction(() => typeof sheetApp == 'function' && typeof IDB != 'undefined', null, {timeout: 30000}); await p.evaluate('IDB.ready');
    await p.evaluate("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>{w.__force=1;w.close()});USER=USER||ACCTS[0];USER.r='admin';const h=homePath();let n=fs;for(const x of h){if(!n[x])n[x]={};n=n[x]}return 1})()"); return p };
  let p = await open(); const q = s => p.evaluate(s);
  // autosave: changes are copied every minute (the test runs the minute's work at once)
  await q("window.XS=sheetApp();window.XP=deckApp()"); await wait(800);
  await q("XS.sp.type('A1','Unsaved budget');XS.sp.type('B1','=21*2');XP.pp.change('t',()=>{XP.pp.deck.slides[0].objs[0].html='<p>Unsaved talk</p>'})");
  ok('1. both apps are open with unsaved work', await q("XS.sp.dirty&&XP.pp.dirty"));
  await wait(61000);
  const idx = await q("kvGet(offRecKey()).then(x=>Object.values(x||{}).map(v=>v.app+':'+v.name))");
  ok('2. after a minute, recovery copies are kept for both', idx.includes('SpreadsheetPro:Book1') && idx.includes('PresentPro:Presentation1'), idx);
  // a crash: the page goes away without the windows being closed
  await p.close(); p = await open();
  await q("window.XS2=sheetApp()"); await wait(1200);
  ok('3. after a crash, SpreadsheetPro offers the copy it kept', await q("/Recover your work/.test(XS2.el.querySelector('.wp-dlg')?.textContent||'')&&/Book1/.test(XS2.el.querySelector('.wp-dlg').textContent)"));
  await q("[...XS2.el.querySelectorAll('.wp-dlg button')].find(b=>b.textContent=='Open it').click()"); await wait(300);
  ok('...Open it: the unsaved work is back (A1, and B1 worked out: 42)', await q("XS2.sp.value('A1')=='Unsaved budget'&&XS2.sp.value('B1')==42&&XS2.sp.dirty"));
  await q("window.XP2=deckApp()"); await wait(1200);
  ok('...and PresentPro too', await q("(()=>{const d=XP2.el.querySelector('.wp-dlg');if(!d)return false;[...d.querySelectorAll('button')].find(b=>b.textContent=='Open it').click();return true})()") && await q("ppText(XP2.pp.deck.slides[0].objs[0].html)=='Unsaved talk'"));
  await wait(300);
  ok('...once taken, the copy is not offered again', await q("kvGet(offRecKey()).then(x=>Object.values(x||{}).filter(v=>v.app=='SpreadsheetPro'||v.app=='PresentPro').length)") == 0);
  // saving removes the copy
  await q("XS2.__force=1;XS2.close();XP2.__force=1;XP2.close()");
  // the Office group, default apps, the App Store
  await q("startMenuApps()");
  ok('4. the Start menu: an Office group with the three apps, and WordPro is not listed twice', await q("(()=>{const it=[...document.querySelectorAll('#menu li.offi')].map(l=>l.textContent);const w=document.querySelector('#menu li[data-a=wordpro]');return it.length==3&&(!w||w.style.display=='none')})()"));
  await q("settings('apps')"); await wait(800);
  ok('5. Settings › Apps lists .rsheet, .xlsx, .rdeck and .pptx with SpreadsheetPro and PresentPro', await q("(()=>{const t=document.querySelector('.da');if(!t)return false;const rows=[...t.rows].map(r=>r.textContent);return['.rsheet','.xlsx','.rdeck','.pptx'].every(e=>rows.some(r=>r.startsWith(e)))&&/SpreadsheetPro/.test(t.textContent)&&/PresentPro/.test(t.textContent)})()"));
  await q("wins.slice().forEach(w=>{w.__force=1;w.close()});launch('store')"); await wait(800);
  await q("(()=>{const b=[...document.querySelectorAll('.tmtabs button')].find(b=>b.dataset.t=='inst');b&&b.click()})()"); await wait(400);
  ok('6. the App Store\'s Installed list shows the three Office apps as built in', await q("(()=>{const t=wins.at(-1).el.textContent;return /WordPro · built in/.test(t)&&/SpreadsheetPro · built in/.test(t)&&/PresentPro · built in/.test(t)})()"));
  // one look: the same menu bar, toolbar, dialogs and status bar classes as WordPro
  await q("wins.slice().forEach(w=>w.close());wordpro();window.XW=wins.at(-1);window.XB=sheetApp();window.XC=deckApp()"); await wait(800);
  ok('7. one look: WordPro, SpreadsheetPro and PresentPro share the menu bar, toolbar and status bar', await q("[XW,XB,XC].every(w=>!!w.el.querySelector('.wp-mb')&&!!w.el.querySelector('.wp-tb')&&!!w.el.querySelector('.wp-sb'))"));
  ok('...and Notey', await q("[XW,XB,XC].every(w=>!!w.el.querySelector('.wn'))"));
  ok('no page error', !errors.length, errors);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
