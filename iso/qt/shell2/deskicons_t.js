// 1.5.5 (desk54.js, props25.js, shell25.js, apps25a.js, files25.js): the Trash's tooltip follows what is in it, also
// after hovering the desktop; app shortcuts come off the desktop from their menu, from File Explorer's Desktop folder,
// with Delete and by dropping them on the Trash, and back from the Start menu (built-in icons and apps without one) or
// Desktop icons…; each account keeps its own choice; File Explorer copies files to the desktop.
//   node deskicons_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 400) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8098, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto('http://127.0.0.1:8098/'); await p.waitForFunction(() => typeof deskApply == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    window.IN=async(n,pw)=>{const a=ACCTS.find(x=>x.n==n);if(!(await pwCheck(a,pw)))return false;if(USER)userLogout();USER=a;await userLogin(a,pw);try{__sessionRestore()}catch(e){}return true};
    ACCTS.length=0;ACCTS.push({n:'boss',h:await pwMake('b1'),r:'admin'});ACCTS.push({n:'kid',h:await pwMake('k1'),r:'user'});saveUser();return 1})()`);
  await q("IN('boss','b1')"); await wait(800);
  const vis = n => q(`(()=>{const d=[...DESK.querySelectorAll(':scope > .ic')].find(x=>x.querySelector('span').textContent==${JSON.stringify(n)});return !!d&&d.style.display!='none'})()`);
  const menuItems = () => q("CMENU?[...CMENU.el.querySelectorAll('.cmi')].map(x=>x.textContent):[]");
  // the Trash's tooltip
  const tr = () => q("document.querySelector('.ic.tr').title");
  await q("(()=>{const d=document.querySelector('#desk > .ic:not(.tr)');d.dispatchEvent(new PointerEvent('pointerover',{bubbles:true}))})()");
  const t0 = await tr();
  await q("(()=>{const d=node(homePath('DESKTOP'));d['NOTE.TXT']='hello';fsSave();EXPL.forEach(f=>f());trashPut([homePath('DESKTOP').concat('NOTE.TXT')]);return TR.length})()");
  await q("(()=>{const d=document.querySelector('#desk > .ic:not(.tr)');delete DESK.dataset.notes;d.dispatchEvent(new PointerEvent('pointerover',{bubbles:true}))})()"); await wait(300);
  const t1 = await tr();
  ok('1. the Trash says it is empty, then (after hovering the desktop icons) how many items it holds', t0 == 'The Trash is empty' && t1 == '1 item in the Trash', [t0, t1]);
  // a built-in icon off the desktop from its menu
  await q("(()=>{const d=[...DESK.querySelectorAll(':scope > .ic')].find(x=>x.querySelector('span').textContent=='Calculator');const r=d.getBoundingClientRect();iconMenu(d,r.left+5,r.top+5)})()");
  let m = await menuItems();
  ok('2. an app icon\'s menu has Remove from desktop', m.some(x => /Remove from desktop/.test(x)), m);
  await q("(()=>{[...CMENU.el.querySelectorAll('.cmi')].find(x=>/Remove from desktop/.test(x.textContent)).click()})()"); await wait(200);
  ok('3. ...and it is gone from the desktop', !(await vis('Calculator')));
  // the Start menu: Add to desktop brings it back
  await q("(()=>{$('#menu').style.display='flex';startMenuApps()})()"); await wait(200);
  await q("(()=>{const li=document.querySelector('#menu li[data-a=calc]')||[...document.querySelectorAll('#menu li')].find(l=>/Calculator/.test(l.textContent));const r=li.getBoundingClientRect();li.dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,cancelable:true,clientX:r.left+5,clientY:r.top+5}))})()"); await wait(200);
  m = await menuItems();
  ok('4. the Start menu offers Add to desktop for it (no longer "Already on the desktop")', m.some(x => /Add to desktop/.test(x)), m);
  await q("(()=>{[...CMENU.el.querySelectorAll('.cmi')].find(x=>/Add to desktop/.test(x.textContent)).click()})()"); await wait(200);
  ok('5. ...and it is back', await vis('Calculator'));
  await q("(()=>{const li=[...document.querySelectorAll('#menu li')].find(l=>/Calculator/.test(l.textContent));const r=li.getBoundingClientRect();li.dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,cancelable:true,clientX:r.left+5,clientY:r.top+5}))})()"); await wait(200);
  m = await menuItems(); ok('6. now the Start menu offers Remove from desktop', m.some(x => /Remove from desktop/.test(x)), m);
  await q("cmenuClose()");
  // an app without a built-in icon: added from the Start menu's added items
  const k = await q("(()=>{const li=[...document.querySelectorAll('#menu li[data-v25]')].find(l=>!deskHasApp(l.dataset.v25));return li&&li.dataset.v25})()");
  await q(`(()=>{const li=document.querySelector('#menu li[data-v25="${k}"]');const r=li.getBoundingClientRect();li.dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,cancelable:true,clientX:r.left+5,clientY:r.top+5}))})()`); await wait(200);
  await q("(()=>{[...CMENU.el.querySelectorAll('.cmi')].find(x=>/Add to desktop/.test(x.textContent)).click()})()"); await wait(300);
  const own = await q(`(()=>{const d=DESK.querySelector(':scope > .ic[data-app="${k}"]');return d?{name:d.querySelector('span').textContent,inIC:IC.includes(d),left:d.style.left}:null})()`);
  ok('7. an app without a desktop icon (' + k + ') gets a shortcut of its own, laid out with the others', own && own.inIC && own.left, own);
  const opened = await q(`(async()=>{const d=DESK.querySelector(':scope > .ic[data-app="${k}"]');const n=wins.length;d.onclick({});d.onclick({});await new Promise(r=>setTimeout(r,600));return wins.length>n})()`);
  ok('8. ...which opens the app', opened);
  await q("$('#menu').style.display='none';wins.slice().forEach(w=>w.close())");
  // File Explorer's Desktop folder: Remove from desktop on a link
  await q("explorer(homePath('DESKTOP'))"); await wait(800);
  const lk = await q("(()=>{const c=[...document.querySelectorAll('.xlnk')].find(x=>/Paint/.test(x.textContent));if(!c)return 'no link';const r=c.getBoundingClientRect();c.dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,cancelable:true,clientX:r.left+5,clientY:r.top+5}));return [...CMENU.el.querySelectorAll('.cmi')].map(x=>x.textContent)})()");
  ok('9. File Explorer\'s Desktop folder: a shortcut\'s menu has Remove from desktop', Array.isArray(lk) && lk.some(x => /Remove from desktop/.test(x)), lk);
  await q("(()=>{[...CMENU.el.querySelectorAll('.cmi')].find(x=>/Remove from desktop/.test(x.textContent)).click()})()"); await wait(500);
  ok('10. ...it goes from the desktop and from the folder', !(await vis('Paint')) && await q("![...document.querySelectorAll('.xlnk')].some(x=>/Paint/.test(x.textContent))"));
  const bg = await q("(()=>{const box=document.querySelector('.win .xc')||document.querySelector('.win .xg')?.parentNode;if(!box)return 'no box';const r=box.getBoundingClientRect();box.dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,cancelable:true,clientX:r.right-10,clientY:r.bottom-10}));return CMENU?[...CMENU.el.querySelectorAll('.cmi')].map(x=>x.textContent):[]})()");
  ok('11. ...and its background menu has Desktop icons…', Array.isArray(bg) && bg.some(x => /Desktop icons/.test(x)), bg);
  await q("cmenuClose();wins.slice().forEach(w=>w.close())");
  // copy a file to the desktop from another folder
  await q("(()=>{const d=node(homePath('DOCUMENTS'));d['REPORT.TXT']='r';fsSave();explorer(homePath('DOCUMENTS'))})()"); await wait(800);
  const cp = await q("(()=>{const r=[...document.querySelectorAll('.win [data-k]')].find(x=>/REPORT\\.TXT/.test(x.textContent));if(!r)return 'no row';r.click();const b=r.getBoundingClientRect();r.dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,cancelable:true,clientX:b.left+5,clientY:b.top+5}));const it=[...CMENU.el.querySelectorAll('.cmi')].find(x=>/Copy to desktop/.test(x.textContent));if(!it)return 'no item';it.click();return 'ok'})()"); await wait(500);
  ok('12. File Explorer: Copy to desktop puts the file on the desktop', cp == 'ok' && await q("!!node(homePath('DESKTOP'))['REPORT.TXT']&&!!DESK.querySelector(':scope > .ic.fic[data-fn=\"REPORT.TXT\"]')"), cp);
  await q("wins.slice().forEach(w=>w.close())");
  // Delete key and the Trash
  await q("(()=>{const d=[...DESK.querySelectorAll(':scope > .ic')].find(x=>x.querySelector('span').textContent=='Notepad');d.tabIndex=0;d.focus();d.dispatchEvent(new KeyboardEvent('keydown',{key:'Delete',bubbles:true}))})()"); await wait(200);
  ok('13. Delete on an app icon removes it from the desktop', !(await vis('Notepad')));
  const dropped = await q("(()=>{const d=[...DESK.querySelectorAll(':scope > .ic')].find(x=>x.querySelector('span').textContent=='Board Games');const t=document.querySelector('#desk > .ic.tr').getBoundingClientRect();d.dispatchEvent(new PointerEvent('pointerup',{bubbles:true,clientX:t.left+t.width/2,clientY:t.top+t.height/2,button:0}));return 1})()"); await wait(300);
  ok('14. an app icon dropped on the Trash is removed from the desktop (not put in the Trash)', !(await vis('Board Games')) && await q("TR.length") == 1);
  // each account keeps its own
  await q("IN('kid','k1')"); await wait(800);
  ok('15. another account still has all the standard icons', await vis('Calculator') && await vis('Paint') && await vis('Notepad') && await q(`!DESK.querySelector(':scope > .ic[data-app="${k}"]')`));
  await q("IN('boss','b1')"); await wait(800);
  ok('16. back in the first account: its choice is kept', !(await vis('Paint')) && !(await vis('Notepad')) && await q(`!!DESK.querySelector(':scope > .ic[data-app="${k}"]')`));
  // Desktop icons…: the list, and Restore the standard icons
  await q("deskIconsDlg()"); await wait(300);
  const dl = await q("(()=>{const W=wins.find(w=>/Desktop icons/.test(w.el.textContent));const rows=[...W.body.querySelectorAll('.dsr')];const paint=rows.find(r=>/Paint/.test(r.textContent));return {n:rows.length,paint:paint&&paint.querySelector('input').checked}})()");
  ok('17. Desktop icons… lists the apps, with what is on the desktop ticked', dl.n > 10 && dl.paint === false, dl);
  await q("(()=>{const W=wins.find(w=>/Desktop icons/.test(w.el.textContent));W.body.querySelector('.rst').click()})()"); await wait(300);
  ok('18. Restore the standard icons', await vis('Paint') && await vis('Notepad') && await vis('Board Games') && await q(`!DESK.querySelector(':scope > .ic[data-app="${k}"]')`));
  // 1.5.5: My Computer (found on the Vivobook: dropped on the Trash, then no obvious way back)
  await q("wins.slice().forEach(w=>w.close())");
  await q("(()=>{const d=[...DESK.querySelectorAll(':scope > .ic')].find(x=>x.querySelector('span').textContent=='My Computer');const t=document.querySelector('#desk > .ic.tr').getBoundingClientRect();d.dispatchEvent(new PointerEvent('pointerup',{bubbles:true,clientX:t.left+t.width/2,clientY:t.top+t.height/2,button:0}));return 1})()"); await wait(300);
  ok('19. My Computer dropped on the Trash stays on the desktop', await vis('My Computer'));
  await q("(()=>{const d=[...DESK.querySelectorAll(':scope > .ic')].find(x=>x.querySelector('span').textContent=='My Computer');deskRemove(d)})()"); await wait(200);
  const dlg2 = await q("(()=>{deskIconsDlg();const W=wins.find(w=>/Desktop icons/.test(w.el.textContent));const r=[...W.body.querySelectorAll('.dsr')].find(x=>x.children[2].textContent=='My Computer');return r?{on:r.querySelector('input').checked}:null})()");
  ok('20. hidden from its menu, it is listed by its own name in Desktop icons…', dlg2 && dlg2.on === false, dlg2);
  await q("(()=>{const W=wins.find(w=>/Desktop icons/.test(w.el.textContent));const c=[...W.body.querySelectorAll('.dsr')].find(x=>x.children[2].textContent=='My Computer').querySelector('input');c.click();return 1})()"); await wait(200);
  ok('21. ...and ticking it puts it back', await vis('My Computer'));
  await q("wins.slice().forEach(w=>w.close())");
  await q("(()=>{const d=[...DESK.querySelectorAll(':scope > .ic')].find(x=>x.querySelector('span').textContent=='My Computer');deskRemove(d)})()"); await wait(200);
  await q("(()=>{$('#menu').style.display='flex';const li=document.querySelector('#menu li[data-a=explorer]');const r=li.getBoundingClientRect();li.dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,cancelable:true,clientX:r.left+5,clientY:r.top+5}));[...CMENU.el.querySelectorAll('.cmi')].find(x=>/Add to desktop/.test(x.textContent)).click();$('#menu').style.display='none';return 1})()"); await wait(200);
  ok('22. the Start menu\'s File Explorer › Add to desktop brings My Computer back too', await vis('My Computer'));
  await q("(()=>{const d=[...DESK.querySelectorAll(':scope > .ic')].find(x=>x.querySelector('span').textContent=='Paint');deskRemove(d);deskRestore(d)})()"); await wait(200);
  ok('23. the removal notification\'s Undo puts the icon back', await vis('Paint'));
  ok('24. no page errors', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1)
})().catch(e => { console.log('FAIL ' + e.stack); process.exit(1) });
