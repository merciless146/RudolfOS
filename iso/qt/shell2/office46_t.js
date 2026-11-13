// 1.4.6: Notey in SpreadsheetPro and PresentPro as in WordPro (8 frames a second, his poses and tricks, checks with
// fixes, tips as you work, explaining a cell, How do I…?), and Oracle (its ☰ menu readable in the dark theme, dark pages).
// node office46_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path'), os = require('os');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www'), SHOT = process.env.SHOTS || path.join(os.tmpdir(), 'office46-shots'); fs.mkdirSync(SHOT, {recursive: true});
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
const errors = [];
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }
  if (u == '/light.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end('<body style="background:#fff;color:#111"><h1>Light</h1><img src="data:image/gif;base64,R0lGODlhAQABAAAAACw=" width=10></body>') }
  if (u == '/dark.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end('<body style="background:#111;color:#eee"><h1>Dark</h1></body>') }
  if (u.startsWith('/a/')) { const f = path.join(WWW, path.normalize(u)); if (f.startsWith(WWW) && fs.existsSync(f)) { s.writeHead(200, {'content-type': u.endsWith('.js') ? 'text/javascript' : u.endsWith('.wasm') ? 'application/wasm' : 'application/octet-stream'}); return s.end(fs.readFileSync(f)) } }   /* (1.5.9: files beside the page) */ s.writeHead(404); s.end() }).listen(8092, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const ctx = await b.newContext({viewport: {width: 1280, height: 760}});
  const p = await ctx.newPage(); p.on('pageerror', e => { errors.push(e.message); console.log('page error:', e.stack.slice(0, 400)) });
  await p.goto('http://127.0.0.1:8092/'); await p.waitForFunction(() => typeof sheetApp == 'function' && typeof offNotey == 'function', null, {timeout: 30000});
  // (1.5.9: SpreadsheetPro's and PresentPro's code loads when first needed, plan 10.1)
  await p.evaluate("typeof lazyApp=='function'?Promise.all([lazyApp('sheet'),lazyApp('deck')]):0");
  await p.evaluate("(()=>{try{svj('bd95setup',1)}catch(e){}localStorage.removeItem('bd95wp');['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>{w.__force=1;w.close()});USER=USER||ACCTS[0];USER.r='admin';return 1})()");
  const q = s => p.evaluate(s); const shot = async n => p.screenshot({path: path.join(SHOT, n + '.png')});
  // ---------------- SpreadsheetPro ----------------
  await q("window.XS=sheetApp();XS.el.querySelector('.mx').click()"); await wait(1500);
  ok('1. SpreadsheetPro: Notey is drawn by WordPro\'s limited animation (his poses, not the old smooth one)', await q("!!XS.el.querySelector('.wn .wn-m svg.wn2')&&typeof XS.notey.play=='function'&&typeof XS.notey.react=='function'"));
  const fr = await q("(async()=>{const sv=XS.el.querySelector('.wn .wn-m svg');let n=0,last=sv.innerHTML;const mo=new MutationObserver(()=>{n++});mo.observe(sv,{childList:true,subtree:true,characterData:true});XS.notey.play('hum');await new Promise(r=>setTimeout(r,2000));mo.disconnect();return n})()");
  console.log(`   Notey's drawing changed ${fr} times in 2 seconds`);
  ok('...at most 8 frames a second (no more than 17 changes in 2 seconds while humming)', fr > 0 && fr <= 17, fr);
  ok('...he greets you, and offers a template for an empty sheet', await q("/template/i.test(XS.notey.bubble.textContent)"));
  await q("XS.notey.hide();XS.el.querySelector('.wn-m').click()"); await wait(200);
  ok('2. clicking him: the menu, with Check my spreadsheet, Explain this cell, a trick and How do I…?', await q("(()=>{const t=XS.notey.bubble.textContent;return /What can I help you with/.test(t)&&/Check my spreadsheet/.test(t)&&/Explain this cell/.test(t)&&/trick/.test(t)&&/How do I/.test(t)})()"), await q("XS.notey.bubble.textContent"));
  await q("XS.notey.hide()");
  // a sheet with problems
  await q("(()=>{const s=XS.sp;['A1','Item','B1','Amount'].reduce(()=>0);s.type('A1','Item');s.type('B1','Amount');for(let i=2;i<=8;i++){s.type('A'+i,'Thing '+i);s.type('B'+i,String(i*10))}s.type('C2','=B2*2');s.type('C3','=B3*2');s.type('C4','=B4*3');s.type('C5','=B5*2');s.type('C6','=B6*2');s.type('B9','=SUM(B2:B7)');s.type('D2','=B2/0');return 1})()");
  await q("(()=>{const s=XS.sp,b=s.book.sheets[0];b.cells.set(spK(9,1),{v:'1,250'});b.fk=null;s.type('E1','12345678901');return 1})()"); await wait(300);
  const F = await q("XS.notey.hide(),XS.notey.check(),new Promise(r=>setTimeout(()=>r([...XS.notey.bubble.querySelectorAll('.wn-fi')].map(d=>d.textContent)),700))");
  ok('3. Check my spreadsheet finds the error, the number stored as text, the odd formula, the short total and #####', F.length >= 5 && F.some(t => /#DIV\/0!.*divided by zero/.test(t)) && F.some(t => /stored as text/.test(t)) && F.some(t => /C4.*different/.test(t)) && F.some(t => /total in B9.*row 7/.test(t)) && F.some(t => /#####/.test(t)), F);
  await shot('01-sheet-check');
  await q("[...XS.notey.bubble.querySelectorAll('.wn-fi')].forEach(d=>{const b=d.querySelector('button.p');if(b&&!/blank/.test(b.textContent))b.click()})"); await wait(200);
  ok('...its fixes: the formula matches, the total includes row 8, the text is a number, the column is wider', await q("(()=>{const s=XS.sp;return s.cell('C4').f=='=B4*2'&&s.cell('B9').f=='=SUM(B2:B8)'&&s.value('B10')===1250&&(s.sheet.cw[4]||80)>90})()"), await q("[XS.sp.cell('C4'),XS.sp.cell('B9'),XS.sp.value('B10'),XS.sp.sheet.cw]"));
  await q("XS.notey.hide();XS.sp.select('B9')"); await q("XS.notey.hide();(()=>{const it=XS.notey.menu;XS.el.querySelector('.wn-m').click();[...XS.notey.bubble.querySelectorAll('button')].find(b=>/Explain/.test(b.textContent)).click()})()"); await wait(200);
  ok('4. Explain this cell: the formula in words (SUM, its range, the result)', await q("(()=>{const t=XS.notey.bubble.textContent;return /=SUM\\(B2:B8\\)/.test(t)&&/SUM: .*adds/i.test(t)&&/B2:B8: 7 cells, 7 with numbers/.test(t)&&/The result is/.test(t)})()"), await q("XS.notey.bubble.textContent"));
  await q("XS.notey.hide();XS.notey.ask()"); await wait(100);
  await q("(()=>{const i=XS.notey.bubble.querySelector('.wn-q');i.value='how do I work out an average';i.dispatchEvent(new KeyboardEvent('keydown',{key:'Enter'}))})()");
  ok('5. How do I…? finds the function, with a button that starts it in the cell', await q("(()=>{const t=XS.notey.bubble.textContent;return /AVERAGE/.test(t)&&[...XS.notey.bubble.querySelectorAll('.wn-fi button')].some(b=>b.textContent=='Use it')})()"), await q("XS.notey.bubble.textContent"));
  await q("XS.notey.hide()");
  // a tip as you work: a column of numbers wants a total
  await q("(()=>{const s=XS.sp;for(let i=1;i<=6;i++)s.type('G'+i,String(i));s.select('G7');XS.notey.hide();XS.notey.snooze.err1=Date.now()+1e6;return 1})()"); await q("XS.notey.edited('',1)"); await wait(2700);
  ok('6. after typing six numbers in a column, Notey offers to add a total (by himself)', await q("/total under them/.test(XS.notey.bubble.textContent)"), await q("XS.notey.bubble.textContent"));
  await q("[...XS.notey.bubble.querySelectorAll('button')].find(b=>b.textContent=='Add a total').click()");
  ok('...and adds it', await q("XS.sp.cell('G7').f=='=SUM(G1:G6)'&&XS.sp.value('G7')==21"), await q("XS.sp.cell('G7')"));
  const sp = await q("(()=>{let got=null;const o=XS.notey.react;XS.notey.react=(k,d)=>{got=[k,d]};XS.sp.select('A1:B9');XS.sp.key('Delete');XS.notey.react=o;return got})()");
  ok('7. he reacts to a big deletion (as he does in WordPro when a lot of text goes)', sp && sp[1] < -250, sp);
  await q("XS.__force=1;XS.close()");
  // ---------------- PresentPro ----------------
  await q("window.XP=deckApp();XP.el.querySelector('.mx').click()"); await wait(1500);
  ok('8. PresentPro: the same Notey, with Check my presentation, Design and Rehearse', await q("(()=>{XP.notey.hide();XP.el.querySelector('.wn-m').click();const t=XP.notey.bubble.textContent;return!!XP.el.querySelector('.wn-m svg.wn2')&&/Check my presentation/.test(t)&&/Design/.test(t)&&/Rehearse/.test(t)})()"));
  await q("XP.notey.hide()");
  const red = await q("(()=>{const c=document.createElement('canvas');c.width=400;c.height=200;const g=c.getContext('2d');g.fillStyle='#e11d48';g.fillRect(0,0,400,200);return c.toDataURL('image/png')})()");
  await q(`(()=>{const pp=XP.pp,D=pp.deck;pp.change('t',()=>{D.slides[0].objs[0].html='<p>My talk</p>';});pp.addSlide('content');pp.change('b',()=>{const s=D.slides[1];s.objs[1].html=ppBullets(Array.from({length:10},(_,i)=>'Point '+(i+1)).join('\\n'));});pp.addSlide('content');pp.change('s',()=>{const s=D.slides[2];s.objs[0].html='<p>Small</p>';s.objs[1].html='<p>tiny words</p>';s.objs[1].fs=10;s.objs[1].color='#f8f8f8';s.objs.push({id:'pic1',t:'img',x:500,y:200,w:300,h:300,img:'${red}'})});pp.addSlide('blank');return 1})()`);
  await q("XP.pp.change('x',()=>{})"); await wait(400);
  const P = await q("XP.notey.hide(),XP.notey.check(),new Promise(r=>setTimeout(()=>r([...XP.notey.bubble.querySelectorAll('.wn-fi')].map(d=>d.textContent)),700))");
  ok('9. Check my presentation: too much text, small text, text too faint, a stretched picture, an empty slide', P.some(t => /Slide 2 has a lot of text \(10 points\)/.test(t)) && P.some(t => /smaller than 14 points/.test(t)) && P.some(t => /too close to the background/.test(t)) && P.some(t => /stretched/.test(t)) && P.some(t => /Slide 4 is empty/.test(t)), P);
  await shot('02-deck-check');
  await q("[...XP.notey.bubble.querySelectorAll('.wn-fi')].forEach(d=>{const b=d.querySelector('button.p');if(b)b.click()})"); await wait(200);
  ok('...the fixes: split into two slides (5 points each, "continued"), 18-point text, the theme\'s colour, the picture\'s shape, the empty slide gone',
     await q("(()=>{const S=XP.pp.deck.slides;const n=x=>(String(x.html).match(/<li/g)||[]).length;const pic=S.flatMap(s=>s.objs).find(o=>o.id=='pic1');const tiny=S.flatMap(s=>s.objs).find(o=>/tiny words/.test(o.html||''));return S.length==4&&n(S[1].objs[1])==5&&n(S[2].objs[1])==5&&/continued/.test(S[2].objs[0].html)===false&&tiny.fs>=18&&!tiny.color&&Math.abs(pic.w/pic.h-2)<.05})()"),
     await q("(()=>{const S=XP.pp.deck.slides;return S.map(s=>s.objs.map(o=>[o.t,(o.html||'').slice(0,40),o.fs,o.color,o.w,o.h]))})()"));
  await q("XP.notey.hide();XP.notey.snooze.toomuch=Date.now()+1e6;XP.pp.change('y',()=>{})"); await wait(2700);
  ok('10. with four slides and no transitions, Notey offers one (by himself)', await q("/transition/i.test(XP.notey.bubble.textContent)"), await q("XP.notey.bubble.textContent"));
  await q("[...XP.notey.bubble.querySelectorAll('button')].find(b=>/Fade/.test(b.textContent)).click()");
  ok('...and adds a fade between the slides', await q("XP.pp.deck.slides.slice(1).every(s=>s.tr.t=='fade')"));
  await q("XP.notey.hide();XP.notey.ask()"); await wait(100);
  await q("(()=>{const i=XP.notey.bubble.querySelector('.wn-q');i.value='how do I see my notes while presenting';i.dispatchEvent(new KeyboardEvent('keydown',{key:'Enter'}))})()");
  ok('11. How do I…? answers about presenting with notes', await q("/presenter view/i.test(XP.notey.bubble.textContent)"), await q("XP.notey.bubble.textContent"));
  await q("XP.notey.hide();XP.__force=1;XP.close()");
  ok('12. Notey can be hidden in one app and stays hidden in the others (as in WordPro)', await q("(()=>{const w=sheetApp();w.notey.show(false);const v=deckApp();const r=!v.notey.visible();v.notey.show(true);v.notey.hide();w.__force=1;w.close();v.__force=1;v.close();return r})()"));
  // ---------------- Oracle ----------------
  await q("document.documentElement.classList.add('dark');browser('about:home');window.XO=wins.at(-1)"); await wait(800);
  await q("XO.body.querySelector('.ob .mn').click()"); await wait(150);
  const mc = await q("(()=>{const d=XO.body.querySelector('.odd');const c=getComputedStyle(d).color,bg=getComputedStyle(d).backgroundColor;const L=s=>{const v=s.match(/\\d+/g).slice(0,3).map(x=>x/255).map(x=>x<=.03928?x/12.92:Math.pow((x+.055)/1.055,2.4));return .2126*v[0]+.7152*v[1]+.0722*v[2]};const a=L(c),b=L(bg);return [(Math.max(a,b)+.05)/(Math.min(a,b)+.05),c,bg]})()");
  ok('13. Oracle\'s ☰ menu in the dark theme: its text stands out from its background (contrast at least 7:1)', mc[0] >= 7, mc);
  await shot('03-oracle-menu');
  ok('...it has Dark pages', await q("[...XO.body.querySelectorAll('.odd div')].some(d=>/Dark pages/.test(d.textContent))"));
  await q("[...XO.body.querySelectorAll('.odd div')].find(d=>/Dark pages/.test(d.textContent)).click()"); await wait(400);
  ok('14. Dark pages: Oracle\'s bar turns dark and its own pages are darkened', await q("XO.el.classList.contains('odark')&&/rudolf-dark/.test(XO.body.querySelector('.fh iframe').srcdoc)&&localStorage.getItem('bd95webdark')=='1'"));
  await shot('04-oracle-dark');
  await q("XO.body.querySelector('.ob .mn').click()"); await wait(100);
  ok('...the menu shows it ticked', await q("[...XO.body.querySelectorAll('.odd div')].some(d=>/✓ Dark pages/.test(d.textContent))"));
  await q("XO.body.querySelector('.ob .mn').click()");
  ok('...a website shown in a frame (outside the shell) is darkened too', await q("(()=>{const fr=document.createElement('iframe');fr.setAttribute('src','about:blank');webDarkFrame(fr,true);return /invert/.test(fr.style.filter)})()"));
  const sent = await q("(()=>{const old=shellSend,oS=SHELL;const got=[];SHELL={};shellSend=m=>got.push(m);RT.set('x',{});webDarkSet(false);webDarkSet(true);RT.delete('x');shellSend=old;SHELL=oS;return got})()");
  ok('...real tabs: the shell is told (off, then on)', sent.length == 2 && sent[0].op == 'dark' && sent[0].on === false && sent[1].on === true, sent);
  // the shell's own script, on a light and a dark page
  const shellSrc = fs.readFileSync(path.join(ROOT, 'iso/shell/shell.js'), 'utf8');
  const part = shellSrc.slice(shellSrc.indexOf('const DARK_CSS'), shellSrc.indexOf('const darken'));
  const pg = await ctx.newPage();
  const res = {};
  for (const u of ['light', 'dark']) { await pg.goto(`http://127.0.0.1:8092/${u}.html`); res[u] = await pg.evaluate(part + ';eval(darkJS(true));new Promise(r=>setTimeout(()=>r([!!document.getElementById("rudolf-dark"),getComputedStyle(document.documentElement).filter,getComputedStyle(document.querySelector("img")||document.body).filter]),200))') }
  ok('15. the shell\'s dark pages: a light page is inverted, its pictures turned back', res.light[0] && /invert/.test(res.light[1]) && /invert\(1\)/.test(res.light[2]), res.light);
  ok('...a page that is dark already is left as it is', !res.dark[0], res.dark);
  await pg.evaluate(part + ';eval(darkJS(false))');
  await q("webDarkSet(false);document.documentElement.classList.remove('dark')");
  ok('no page error', !errors.length, errors);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
