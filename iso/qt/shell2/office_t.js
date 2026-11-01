// 1.4.5 the Office apps together (office45.js): autosave and recovery after a crash (SpreadsheetPro and PresentPro),
// the Start menu's Office group, Settings › Apps' default apps, the App Store's built-in apps, and the look they share.
// node office_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path'), os = require('os'), cp = require('child_process');
const SHOT = process.env.SHOTS || path.join(os.tmpdir(), 'office-shots'); fs.mkdirSync(SHOT, {recursive: true});
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

  // ---- the common tasks, start to finish (round 2 of the plan), with a picture of each step ----
  const shot = async n => p.screenshot({path: path.join(SHOT, n + '.png')});
  await q("wins.slice().forEach(w=>{w.__force=1;w.close()})");
  // a grade book: conditional formatting, then sorted by the average
  await q("window.G=sheetApp(null,spTemplate('grades'));G.el.querySelector('.mx').click()"); await wait(800);
  await q("G.sp.sortRange('A3:G9',[[5,-1]],true)"); await wait(200); await shot('u1-gradebook-sorted');
  ok('8. a grade book sorted by the average, best first, the grades and colours following', await q("G.sp.value('A4')=='Casey'&&G.sp.value('G4')=='A'&&G.sp.value('A9')=='Devon'&&G.sp.value('F11')==82"), await q("[G.sp.value('A4'),G.sp.value('G4'),G.sp.value('A9'),G.sp.value('F11')]"));
  await q("G.__force=1;G.close()");
  // a monthly budget with its chart, copied with its data into a 10-slide talk
  await q("window.B=sheetApp(null,spTemplate('budget'));B.el.querySelector('.mx').click()"); await wait(800);
  await q("B.sp.type('C5','520')"); await wait(200); await shot('u2-budget');
  ok('9. a monthly budget: a change flows through the totals and the chart', await q("B.sp.value('C13')==2810&&B.sp.chartSpec(B.sp.sheet.charts[0]).series[1].vals[1]==520"));
  await q("(async()=>{const ch=B.sp.sheet.charts[0];const spec=B.sp.chartSpec(ch);OFFCLIP={kind:'chart',spec,png:await offSvgPng(offChartSVG(spec),ch.w,ch.h),live:{rg:ch.rg,sheet:B.sp.sheet.name}}})()"); await wait(300);
  await q("window.T=deckApp(null,ppTemplate('talk'));T.el.querySelector('.mx').click()"); await wait(800);
  await q("(()=>{const pp=T.pp;pp.go(1);for(let i=0;i<4;i++)pp.addSlide('content');pp.go(2);pp.pasteChart(OFFCLIP);pp.change('n',()=>{pp.deck.slides.forEach((s,i)=>{s.notes='Notes for slide '+(i+1)})});pp.change('th',()=>{pp.deck.theme='ocean'})})()");
  const red = await q("(()=>{const c=document.createElement('canvas');c.width=800;c.height=500;const g=c.getContext('2d');g.fillStyle='#0ea5e9';g.fillRect(0,0,800,500);g.fillStyle='#fde047';g.beginPath();g.arc(400,250,120,0,7);g.fill();return c.toDataURL('image/png')})()");
  await q(`T.pp.go(3);T.pp.placePicture('${red}')`); await wait(400); await q("T.pp.go(2)"); await wait(300); await shot('u3-talk-chart');
  ok('10. a 10-slide talk with a theme, a picture, the budget chart, notes and transitions', await q("(()=>{const d=T.pp.deck;return d.slides.length==10&&d.theme=='ocean'&&d.slides.some(s=>s.objs.some(o=>o.t=='img'))&&d.slides[2].objs.some(o=>o.t=='chart'&&o.chart.series.length==2)&&d.slides.every(s=>s.notes)&&d.slides.slice(1).every(s=>s.tr.t=='fade')})()"));
  await q("T.pp.present(0)"); await wait(300); await p.keyboard.press('ArrowRight'); await wait(300); await p.keyboard.press('ArrowRight'); await wait(800); await p.keyboard.press('p'); await wait(300); await shot('u4-presenter-view');
  ok('...presented, with the presenter view showing the notes', await q("/Notes for slide 3/.test(document.querySelector('.pp-pvnotes').textContent)&&/Slide 3 of 10/.test(document.querySelector('.pp-pvh .n').textContent)"));
  await p.keyboard.press('Escape'); await wait(200);
  const pdfn = await q("(async()=>{let got=null;window.savePdf=(n,b)=>{got=b};await T.pp.exportPdf('slides');const t=new TextDecoder('latin1').decode(await new Response(got).arrayBuffer());return (t.match(/\\/Type\\s*\\/Page[^s]/g)||[]).length})()");
  ok('...exported as a PDF of 10 pages', pdfn == 10, pdfn);
  const px = await q("(async()=>{const b=await ppToPptx(T.pp.deck);const d=await offDataUrl(b);return d.split(',')[1]})()"); const f = path.join(os.tmpdir(), 'office-talk.pptx'); fs.writeFileSync(f, Buffer.from(px, 'base64'));
  let n = ''; try { n = cp.execFileSync('python3', ['-c', 'import pptx,sys;p=pptx.Presentation(sys.argv[1]);print(len(p.slides),sum(1 for s in p.slides for sh in s.shapes if sh.shape_type==13),sum(1 for s in p.slides if s.has_notes_slide))', f], {encoding: 'utf8'}).trim() } catch (e) { n = String(e) }
  ok('...and as a .pptx that python-pptx reads: 10 slides, the picture and the chart as pictures, 10 notes pages', n == '10 2 10', n);
  ok('no page error', !errors.length, errors);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
