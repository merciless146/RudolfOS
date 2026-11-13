// 1.4.5 PresentPro (ppro45.js, pprof45.js) in the desktop page: slides and layouts, typing, objects (shapes, pictures,
// tables, charts, icons) moved, resized, rotated, aligned and grouped, themes and the master, transitions and
// animations, presenting (keys, presenter view, black screen, pen), files (.RDECK 1 and 2, .pptx both ways, checked
// with python-pptx, PDF, PNG), working with WordPro and SpreadsheetPro, the Office group, a 50-slide deck.
// node ppro_t.js [CHROME]   (build iso/www/index.html first; python3 with python-pptx for the PowerPoint checks)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path'), os = require('os'), cp = require('child_process');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www'), SHOT = process.env.SHOTS || path.join(os.tmpdir(), 'ppro-shots');
fs.mkdirSync(SHOT, {recursive: true});
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
const errors = [];
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }
  const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200); fs.createReadStream(f).pipe(s) }).listen(8096, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
const py = (code, ...args) => cp.execFileSync('python3', ['-c', code, ...args], {encoding: 'utf8'});
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const ctx = await b.newContext({viewport: {width: 1280, height: 760}});
  const p = await ctx.newPage(); p.on('pageerror', e => { errors.push(e.message); console.log('page error:', e.stack.slice(0, 400)) });
  await p.goto('http://127.0.0.1:8096/'); await p.waitForFunction(() => typeof deckApp == 'function' && typeof launch == 'function', null, {timeout: 30000});
  // (1.5.9: SpreadsheetPro's and PresentPro's code loads when first needed, plan 10.1)
  await p.evaluate("typeof lazyApp=='function'?Promise.all([lazyApp('sheet'),lazyApp('deck')]):0");
  await p.evaluate("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';for(const sub of [[],['DOCUMENTS'],['PICTURES']]){const h=homePath(...sub);let n=fs;for(const x of h){if(!n[x])n[x]={};n=n[x]}}return 1})()");
  const q = s => p.evaluate(s); const shot = async n => p.screenshot({path: path.join(SHOT, n + '.png')});
  const key = async (k, n = 1) => { for (let i = 0; i < n; i++) await p.keyboard.press(k) };
  const red = await q("(()=>{const c=document.createElement('canvas');c.width=3200;c.height=2000;const g=c.getContext('2d');g.fillStyle='#e11d48';g.fillRect(0,0,3200,2000);for(let i=0;i<400;i++){g.fillStyle=`hsl(${i*7},80%,60%)`;g.fillRect((i*97)%3100,(i*61)%1900,90,90)}return c.toDataURL('image/png')})()");
  await q(`node(homePath('PICTURES'))['RED.PNG']='${red}'`);
  // ---------------- 1. the app ----------------
  await q("launch('deck')"); await wait(1200); await q("window.W=wins.at(-1);W.el.querySelector('.mx').click()"); await wait(500);
  ok('1. Start: PresentPro opens (the old id `deck`), with the menus, toolbar, slide list, slide and notes', await q("W.app=='deck'&&W.el.textContent.includes('PresentPro')&&[...W.el.querySelectorAll('.wp-mb button')].map(b=>b.textContent).join()=='File,Home,Insert,Design,Arrange,Slide Show,View,Help'&&!!W.el.querySelector('.pp-list .pp-th')&&!!W.el.querySelector('.pp-stage .pp-slide')&&!!W.el.querySelector('.pp-notes')"));
  ok('....RDECK and .PPTX open in PresentPro', await q("OPENERS.find(o=>o[0]=='deck')[1]=='PresentPro'&&OPENERS.find(o=>o[0]=='deck')[3]('PPTX')"));
  // ---------------- 2. typing on a slide ----------------
  const objPt = async (i, dx = .5, dy = .5) => q(`(()=>{const o=W.pp.deck.slides[W.pp.cur].objs[${i}];const r=W.pp.ov().getBoundingClientRect();return[r.left+(o.x+o.w*${dx})*W.pp.K,r.top+(o.y+o.h*${dy})*W.pp.K]})()`);
  let pt = await objPt(0); await p.mouse.dblclick(pt[0], pt[1]); await wait(150); await p.keyboard.type('Our year in review'); await key('Escape'); await wait(150);
  ok('2. double-click a title, type, Esc: the text is on the slide and in its thumbnail', await q("ppText(W.pp.deck.slides[0].objs[0].html)=='Our year in review'&&/Our year in review/.test(W.el.querySelector('.pp-list .pp-th').textContent)"), await q("W.pp.deck.slides[0].objs[0].html"));
  pt = await objPt(1); await p.mouse.dblclick(pt[0], pt[1]); await wait(100); await p.keyboard.type('Avery Smith'); await p.keyboard.down('Shift'); await key('Home'); await p.keyboard.up('Shift'); await key('Control+b'); await key('Escape');
  ok('...Ctrl+B bolds the selected words', /<b>Avery Smith<\/b>|font-weight: ?bold/.test(await q("W.pp.deck.slides[0].objs[1].html")), await q("W.pp.deck.slides[0].objs[1].html"));
  await q("W.el.querySelector('.pp-wrap').focus()"); await key('Control+m'); await wait(300);
  ok('3. Ctrl+M adds a "Title and content" slide after the title slide', await q("W.pp.deck.slides.length==2&&W.pp.cur==1&&W.pp.deck.slides[1].layout=='content'"));
  pt = await objPt(1); await p.mouse.dblclick(pt[0], pt[1]); await wait(100); await p.keyboard.type('Sales up 12%'); await key('Enter'); await p.keyboard.type('Two new shops'); await key('Escape');
  ok('...its text box makes bullets', await q("(()=>{const h=W.pp.deck.slides[1].objs[1].html;return /<ul>/.test(h)&&(h.match(/<li>/g)||[]).length==2})()"), await q("W.pp.deck.slides[1].objs[1].html"));
  await q("W.el.querySelector('.pp-notes').value='Mention the new shops in Leeds and York.';W.el.querySelector('.pp-notes').dispatchEvent(new Event('input'))");
  ok('...speaker notes are kept with the slide', await q("W.pp.deck.slides[1].notes.includes('Leeds')"));
  // ---------------- 4. objects ----------------
  await q("W.pp.setLayout('titleonly')");
  ok('4. changing the layout keeps the text already typed', await q("W.pp.deck.slides[1].layout=='titleonly'&&W.pp.deck.slides[1].objs.some(o=>ppText(o.html||'').includes('Sales up 12%'))"));
  await q("W.pp.addShape('round')"); const sh0 = await q("JSON.stringify(W.pp.deck.slides[1].objs.at(-1))");
  ok('...a rounded rectangle in the middle of the slide, selected', await q("(()=>{const o=W.pp.deck.slides[1].objs.at(-1);return o.t=='shape'&&o.shape=='round'&&W.pp.sel[0]==o.id&&!!W.el.querySelector('.pp-sel .pp-h.se')})()"), sh0);
  // drag it: it snaps to the slide's centre
  const si = await q("W.pp.deck.slides[1].objs.length-1"); pt = await objPt(si);
  await p.mouse.move(pt[0], pt[1]); await p.mouse.down(); await p.mouse.move(pt[0] - 120, pt[1] + 3, {steps: 6}); await p.mouse.move(pt[0] - 150, pt[1] + 2, {steps: 3});
  ok('5. dragging shows guides, and snaps to the middle of the slide', await q("!!W.el.querySelector('.pp-g')"));
  await p.mouse.up(); const o1 = JSON.parse(await q("JSON.stringify(W.pp.deck.slides[1].objs.at(-1))"));
  ok('...it moved left, and snapped to the slide\'s middle line (centre at 270)', o1.x < 390 && Math.abs(o1.y + o1.h / 2 - 270) < 1, o1);
  const hd = await q("(()=>{const r=W.el.querySelector('.pp-sel .pp-h.se').getBoundingClientRect();return[r.left+4,r.top+4]})()"); await p.mouse.move(hd[0], hd[1]); await p.mouse.down(); await p.mouse.move(hd[0] + 60, hd[1] + 40, {steps: 5}); await p.mouse.up();
  const o2 = JSON.parse(await q("JSON.stringify(W.pp.deck.slides[1].objs.at(-1))"));
  ok('...resized from its corner', o2.w > o1.w + 40 && o2.h > o1.h + 20, [o1, o2]);
  const rh = await q("(()=>{const r=W.el.querySelector('.pp-sel .pp-h.rot').getBoundingClientRect();return[r.left+5,r.top+5]})()"); const c0 = await objPt(si); await p.mouse.move(rh[0], rh[1]); await p.mouse.down(); await p.mouse.move(c0[0] + 200, c0[1], {steps: 6}); await p.mouse.up();
  ok('...and rotated with the round handle (90°, snapping to 15° steps)', await q("W.pp.deck.slides[1].objs.at(-1).rot") == 90, await q("W.pp.deck.slides[1].objs.at(-1).rot"));
  await key('Control+z'); ok('...Ctrl+Z undoes the rotation', await q("!W.pp.deck.slides[1].objs.at(-1).rot"));
  await key('Control+y'); await key('Control+z');
  // picture, table, chart, icon
  await q(`W.pp.placePicture(node([...homePath('PICTURES'),'RED.PNG']))`); await wait(500);
  ok('6. a picture: kept with its shape, shrunk to the slide size (3200 pixels wide becomes 1920)', await q("(async()=>{const o=W.pp.deck.slides[1].objs.at(-1);const im=new Image();im.src=o.img;await im.decode();return o.t=='img'&&Math.abs(o.w/o.h-1.6)<.02&&im.naturalWidth==1920})()"), await q("(()=>{const o=W.pp.deck.slides[1].objs.at(-1);return[o.w,o.h,o.img.length]})()"));
  await q("W.pp.change('crop',()=>{const o=W.pp.deck.slides[1].objs.at(-1);o.crop={l:.25,t:0,r:.25,b:0};o.br=130;o.line='#ffffff';o.lw=4})");
  ok('...cropped, brighter, with a border', await q("(()=>{const e=W.pp.stage().querySelector('.pp-img img');return /brightness\\(1.3/.test(e.style.filter)&&e.style.width.startsWith('200')})()"), await q("W.pp.stage().querySelector('.pp-img img').style.cssText"));
  await q("W.pp.addObj({t:'table',w:400,h:150,rows:[['Shop','Sales'],['Leeds','120'],['York','95']],hdr:true,fs:16})");
  await q("W.pp.addObj({t:'chart',w:420,h:260,chart:{type:'pie',title:'Share',cats:['A','B','C'],series:[{name:'S',vals:[5,3,2]}],legend:true}})");
  await q("W.pp.addObj({t:'icon',icon:'🚀',w:80,h:80})"); await wait(300);
  ok('...a table, a chart and an icon are drawn', await q("!!W.pp.stage().querySelector('.pp-table th')&&!!W.pp.stage().querySelector('.pp-chart svg path')&&W.pp.stage().querySelector('.pp-icon').textContent.includes('🚀')"));
  await shot('01-objects');
  // align, distribute, group
  await q("(()=>{const os=W.pp.deck.slides[1].objs.filter(o=>o.t=='table'||o.t=='chart'||o.t=='icon');W.pp.sel=os.map(o=>o.id)})()"); await q("W.pp.align('t')");
  ok('7. align top lines up the selected objects', await q("new Set(W.pp.deck.slides[1].objs.filter(o=>o.t=='table'||o.t=='chart'||o.t=='icon').map(o=>o.y)).size==1"));
  await q("W.pp.group()"); ok('...group: they share a group', await q("new Set(W.pp.deck.slides[1].objs.filter(o=>o.t=='table'||o.t=='chart'||o.t=='icon').map(o=>o.grp)).size==1&&!!W.pp.deck.slides[1].objs.find(o=>o.t=='icon').grp"));
  const ic = await q("W.pp.deck.slides[1].objs.findIndex(o=>o.t=='icon')"); pt = await objPt(ic); await p.mouse.click(pt[0], pt[1]);
  ok('...clicking one selects the whole group', await q("W.pp.sel.length==3"), await q("W.pp.sel"));
  // copy and paste
  await key('Control+c'); await p.keyboard.press('Control+v'); await wait(200);
  ok('8. Ctrl+C, Ctrl+V: a copy of the group', await q("W.pp.deck.slides[1].objs.filter(o=>o.t=='icon').length==2"), await q("W.pp.deck.slides[1].objs.map(o=>o.t)"));
  await key('Delete'); ok('...Delete removes it', await q("W.pp.deck.slides[1].objs.filter(o=>o.t=='icon').length==1"));
  // from SpreadsheetPro: a range becomes a table, a chart comes with its data
  await q("window.S1=sheetApp(null,spTemplate('budget'))"); await wait(800);
  await q("S1.sp.select('A3:C6');S1.sp.copy()"); await q("W.pp.go(1);W.el.querySelector('.pp-wrap').focus()"); await p.keyboard.press('Control+v'); await wait(200);
  ok('9. a range copied in SpreadsheetPro pastes as a table', await q("(()=>{const o=W.pp.deck.slides[1].objs.at(-1);return o.t=='table'&&ppText(o.rows[1][0])=='Rent'&&o.rows.length==4})()"), await q("W.pp.deck.slides[1].objs.at(-1)"));
  await q("(async()=>{const S=S1.sp;const ch=S.sheet.charts[0];await (async()=>{const spec=S.chartSpec(ch);OFFCLIP={kind:'chart',spec,png:await offSvgPng(offChartSVG(spec),ch.w,ch.h),live:{rg:ch.rg,sheet:S.sheet.name}}})()})()"); await wait(300);
  await q("W.pp.pasteChart(OFFCLIP)");
  ok('...a SpreadsheetPro chart pastes with its data (it stays a chart you can change)', await q("(()=>{const o=W.pp.deck.slides[1].objs.at(-1);return o.t=='chart'&&o.chart.series.length==2&&o.chart.cats[0]=='Rent'})()"));
  await q("S1.__force=1;S1.close()");
  // ---------------- 10. design ----------------
  await q("W.pp.change('Theme',()=>{W.pp.deck.theme='ocean'})"); await wait(200);
  ok('10. at least 10 themes; the Ocean theme gives a gradient background', await q("Object.keys(PP_THEMES).length>=10&&/gradient/.test(W.pp.stage().querySelector('.pp-slide').style.background)"));
  await q("W.pp.change('bg',()=>{W.pp.deck.slides[0].bg={t:'grad',c:'#111111',c2:'#4444ff',a:90}});W.pp.change('m',()=>{Object.assign(W.pp.deck.master,{footer:'Rudolf & Co.',num:true,numSkip:true})})"); await q("W.pp.go(1)");
  ok('...the master puts the footer and slide numbers on the slides (not the title slide)', await q("(()=>{const s=W.pp.stage();return s.querySelector('.pp-ff')?.textContent=='Rudolf & Co.'&&s.querySelector('.pp-fn')?.textContent=='2'})()"));
  // ---------------- 11. transitions and animations ----------------
  await q("W.pp.change('t',()=>{W.pp.deck.slides.forEach(s=>s.tr={t:'push',d:.4})});W.pp.change('a',()=>{const os=W.pp.deck.slides[1].objs;os.find(o=>o.t=='table').anim={t:'fade',on:'click',d:.3};os.find(o=>o.t=='icon').anim={t:'fly',on:'after',d:.3};os.find(o=>o.t=='img').anim={t:'zoom',on:'click',d:.3}})");
  ok('11. animations make two clicks on slide 2: the picture zooms; then the table fades with the icon flying in after it', await q("(()=>{const st=W.pp.steps(W.pp.deck.slides[1]);return st.length==2&&st[0].length==1&&st[1].length==2&&st[1][1].at>0})()"), await q("W.pp.steps(W.pp.deck.slides[1])"));
  // add more slides for the show
  await q("W.pp.go(1);W.pp.addSlide('section');W.pp.addSlide('two');W.pp.change('h',()=>{W.pp.deck.slides[2].hidden=true})");
  // ---------------- 12. presenting ----------------
  await q("W.pp.present(0)"); await wait(400);
  const st = () => q("JSON.stringify(W.show.state)");
  ok('12. F5: the show fills the screen with the first slide', await q("!!document.querySelector('.pp-show .pp-sc .pp-slide')&&W.show.state.slide==0"));
  await key('ArrowRight'); await wait(500);
  ok('...→ goes to slide 2 with a push transition; its animated objects wait', await q("W.show.state.slide==1&&W.show.state.step==0&&getComputedStyle(document.querySelector('.pp-show .pp-sc .pp-slide:last-child .pp-table')).visibility=='hidden'"), await st());
  await key('PageDown'); await wait(500);
  ok('...a presenter remote\'s Page Down plays the first animation (the picture)', await q("W.show.state.step==1&&getComputedStyle(document.querySelector('.pp-show .pp-sc .pp-slide:last-child .pp-img')).visibility=='visible'&&getComputedStyle(document.querySelector('.pp-show .pp-sc .pp-slide:last-child .pp-table')).visibility=='hidden'"), await st());
  await key('PageDown'); await wait(800);
  ok('...and the next: the table, then the icon after it', await q("W.show.state.step==2&&getComputedStyle(document.querySelector('.pp-show .pp-sc .pp-slide:last-child .pp-table')).visibility=='visible'&&getComputedStyle(document.querySelector('.pp-show .pp-sc .pp-slide:last-child .pp-icon')).visibility=='visible'"), await st());
  await key(' '); await wait(500);
  ok('...the hidden slide 3 is skipped (slide 4 comes next)', await q("W.show.state.slide==3"), await st());
  await key('b'); ok('...B makes the screen black, and again shows it', await q("W.show.state.blank=='#000'"));await key('b');
  await key('p'); await wait(300); await shot('02-presenter-view');
  ok('...P shows the presenter view: the slide, the next one, the notes, the clock and the time', await q("(()=>{const o=document.querySelector('.pp-show');return o.classList.contains('pv')&&!!o.querySelector('.pp-pvn')&&/Slide \\d of/.test(o.querySelector('.pp-pvh .n').textContent)&&/\\d:\\d\\d/.test(o.querySelector('.pp-pvh .e').textContent)})()"));
  await key('1'); await key('Enter'); await wait(500);
  ok('...a number then Enter goes to that slide', await q("W.show.state.slide==0"), await st());
  await key('ArrowRight'); await wait(300);
  ok('...the presenter view shows this slide\'s notes', await q("document.querySelector('.pp-pvnotes').textContent.includes('Leeds')"));
  await key('p'); await q('W.show.togglePen()');  const sc = await q("(()=>{const r=document.querySelector('.pp-show').getBoundingClientRect();return[r.width/2,r.height/2]})()");
  await p.mouse.move(sc[0], sc[1]); await p.mouse.down(); await p.mouse.move(sc[0] + 80, sc[1] + 40, {steps: 5}); await p.mouse.up();
  ok('...the pen (Ctrl+P, or ✎ in the presenter view) draws on the slide', await q("(()=>{const c=document.querySelector('.pp-ink');const d=c.getContext('2d').getImageData(0,0,c.width,c.height).data;for(let i=3;i<d.length;i+=4)if(d[i])return true;return false})()"));
  await key('Escape'); await wait(300);
  ok('...Esc ends the show', await q("!document.querySelector('.pp-show')"));
  // smoothness of a transition
  await q("W.pp.present(0)"); await wait(300);
  const fps = await q("(async()=>{let n=0;const t0=performance.now();W.show.next();await new Promise(r=>{const f=()=>{n++;if(performance.now()-t0<400)requestAnimationFrame(f);else r()};requestAnimationFrame(f)});return n/((performance.now()-t0)/1000)})()");
  console.log(`   a transition: ${fps.toFixed(0)} frames a second`); ok('...transitions are smooth (at least 50 frames a second here)', fps >= 50, fps);
  await key('Escape');
  // ---------------- 13. files ----------------
  await q(`(()=>{const d=homePath('DOCUMENTS');saveVfsDlg=async(nm)=>[...d,window.__saveAs||nm];return 1})()`);
  await q("window.__saveAs='TALK.RDECK';W.pp.doSave(true)"); await wait(300);
  ok('13. Save as .RDECK (version 2)', await q("(()=>{const o=JSON.parse(node([...homePath('DOCUMENTS'),'TALK.RDECK']));return o.v==2&&o.slides.length==4&&o.theme=='ocean'})()"));
  await q("window.W2=deckApp([...homePath('DOCUMENTS'),'TALK.RDECK'])"); await wait(700);
  ok('...it opens again with its slides, objects, notes, transitions and animations', await q("(()=>{const d=W2.pp.deck,s=d.slides[1];return d.slides.length==4&&s.notes.includes('Leeds')&&s.tr.t=='push'&&s.objs.some(o=>o.t=='chart')&&s.objs.find(o=>o.t=='table').anim.t=='fade'&&d.slides[2].hidden})()"));
  await q("W2.close()");
  await q(`node(homePath('DOCUMENTS'))['OLD.RDECK']=JSON.stringify({slides:[{title:'Old talk',body:'Point one\\nPoint two',bg:'#1e3a8a',fg:'#ffffff',img:null},{title:'Second',body:'',bg:'#000000',fg:'#ffff00',img:null}]})`);
  await q("window.W3=deckApp([...homePath('DOCUMENTS'),'OLD.RDECK'])"); await wait(600);
  ok('14. a file from the old Presentations app (version 1) opens: titles, bullets, colours', await q("(()=>{const d=W3.pp.deck;return d.slides.length==2&&ppText(d.slides[0].objs[0].html)=='Old talk'&&/<li>Point two/.test(d.slides[0].objs[1].html)&&d.slides[0].bg.c=='#1e3a8a'&&d.slides[1].objs[0].color=='#ffff00'})()"), await q("W3.pp.deck.slides[0]"));
  await q("W3.close()");
  // PowerPoint: PresentPro writes, python-pptx reads
  await q("window.__saveAs='TALK.PPTX';W.pp.doSave(true)"); await wait(1500);
  const x64 = await q("node([...homePath('DOCUMENTS'),'TALK.PPTX'])"); const xf = path.join(os.tmpdir(), 'ppro-talk.pptx'); fs.writeFileSync(xf, Buffer.from(x64.split(',')[1], 'base64'));
  let rd = ''; try { rd = py(`import pptx,sys,json\nfrom pptx.util import Emu\np=pptx.Presentation(sys.argv[1]);out={'n':len(p.slides),'w':p.slide_width,'h':p.slide_height,'s':[]}\nfor s in p.slides:\n  d={'shapes':[],'notes':s.notes_slide.notes_text_frame.text if s.has_notes_slide else ''}\n  for sh in s.shapes:\n    d['shapes'].append([sh.shape_type and str(sh.shape_type),sh.text_frame.text if sh.has_text_frame else '',sh.has_table and sh.table.cell(1,0).text or ''])\n  out['s'].append(d)\nprint(json.dumps(out))`, xf) } catch (e) { rd = String(e.stdout || '') + String(e.stderr || e) }
  let P = {}; try { P = JSON.parse(rd) } catch (e) {}
  const s1 = P.s && P.s[1] || {shapes: []}; const kinds = s1.shapes.map(x => x[0]).join(',');
  ok('15. Save as .pptx: python-pptx reads 4 slides, 16:9, the title, the bullets, the shape, the picture, the table and the notes',
     P.n == 4 && P.w == 12192000 && P.h == 6858000 && P.s[0].shapes.some(x => x[1] == 'Our year in review') && /PICTURE/.test(kinds) && /AUTO_SHAPE/.test(kinds) && /TABLE/.test(kinds) && s1.shapes.some(x => x[2] == 'Leeds') && s1.notes.includes('Leeds'), rd.slice(0, 600));
  // python-pptx writes, PresentPro reads
  const xin = path.join(os.tmpdir(), 'ppro-in.pptx'); const png = path.join(os.tmpdir(), 'ppro-red.png'); fs.writeFileSync(png, Buffer.from(red.split(',')[1], 'base64'));
  py(`import pptx,sys\nfrom pptx.util import Inches,Pt\nfrom pptx.dml.color import RGBColor\nfrom pptx.enum.shapes import MSO_SHAPE\np=pptx.Presentation()\ns=p.slides.add_slide(p.slide_layouts[0]);s.shapes.title.text='Imported talk';s.placeholders[1].text='From python-pptx'\ns=p.slides.add_slide(p.slide_layouts[1]);s.shapes.title.text='Points';tf=s.placeholders[1].text_frame;tf.text='First point';tf.add_paragraph().text='Second point'\nr=s.shapes.add_shape(MSO_SHAPE.OVAL,Inches(6),Inches(4),Inches(2),Inches(1.5));r.fill.solid();r.fill.fore_color.rgb=RGBColor(0x22,0x88,0x44);r.text='Go'\ns.notes_slide.notes_text_frame.text='Say hello'\ns=p.slides.add_slide(p.slide_layouts[6]);s.shapes.add_picture(sys.argv[2],Inches(1),Inches(1),width=Inches(4))\nt=s.shapes.add_table(2,2,Inches(5),Inches(1),Inches(4),Inches(1)).table;t.cell(0,0).text='K';t.cell(1,1).text='V'\nb=s.shapes.add_textbox(Inches(1),Inches(5),Inches(4),Inches(1));b.text_frame.text='Big red';rr=b.text_frame.paragraphs[0].runs[0];rr.font.size=Pt(40);rr.font.bold=True;rr.font.color.rgb=RGBColor(255,0,0)\np.save(sys.argv[1])`, xin, png);
  await q(`node(homePath('DOCUMENTS'))['IN.PPTX']='data:application/vnd.openxmlformats-officedocument.presentationml.presentation;base64,${fs.readFileSync(xin).toString('base64')}'`);
  await q("window.W4=deckApp([...homePath('DOCUMENTS'),'IN.PPTX'])"); await wait(1500);
  ok('16. a .pptx made by python-pptx opens: titles, bullets, a shape with its colour, notes, a picture, a table, formatted text',
     await q("(()=>{const d=W4.pp.deck,s=d.slides;const t=x=>ppText(x.html||'');return d.size=='4:3'&&s.length==3&&s[0].objs.some(o=>t(o)=='Imported talk')&&s[1].objs.some(o=>/<li>/.test(o.html||'')&&t(o).includes('Second point'))&&s[1].objs.some(o=>o.t=='shape'&&o.shape=='ellipse'&&o.fill=='#228844'&&t(o)=='Go')&&s[1].notes=='Say hello'&&s[2].objs.some(o=>o.t=='img'&&o.img.startsWith('data:image/'))&&s[2].objs.some(o=>o.t=='table'&&ppText(o.rows[1][1])=='V')&&s[2].objs.some(o=>/font-size:40px/.test(o.html)&&/color:#ff0000|color: ?rgb\\(255, 0, 0\\)/.test(o.html)&&/<b>/.test(o.html))})()"),
     await q("W4.pp.deck.slides.map(s=>s.objs.map(o=>[o.t,o.shape,o.fill,(o.html||'').slice(0,80),Math.round(o.x),Math.round(o.y)]))"));
  await q("W4.pp.go(1)"); await wait(200); await shot('03-imported-pptx'); await q("W4.close()");
  // PDF and pictures
  const pdf = await q("(async()=>{let got=null;window.savePdf=(n,b)=>{got=[n,b]};await W.pp.exportPdf('slides');const s=got;await W.pp.exportPdf('6');const h=got;await W.pp.exportPdf('notes');const n=got;const txt=async b=>typeof b=='string'?b:new TextDecoder('latin1').decode(await (b.arrayBuffer?b.arrayBuffer():new Response(b).arrayBuffer()));const cnt=async b=>((await txt(b)).match(/\\/Type\\s*\\/Page[^s]/g)||[]).length;return[s[0],await cnt(s[1]),h[0],await cnt(h[1]),n[0],await cnt(n[1])]})()");
  ok('17. PDF: the slides (3, the hidden one left out), handouts with 6 a page (1 page), notes pages (3)', pdf[1] == 3 && pdf[3] == 1 && pdf[5] == 3 && /HANDOUT/.test(pdf[2]), pdf);
  const cv = await q("W.pp.slideCanvases(1).then(cs=>{const g=cs[0].getContext('2d');const d=g.getImageData(cs[0].width/2,cs[0].height/2,1,1).data;return[cs.length,cs[0].width,[...d]]})");
  ok('...pictures of the slides are drawn (not blank)', cv[0] == 3 && cv[1] == 960 && !(cv[2][0] == 255 && cv[2][1] == 255 && cv[2][2] == 255), cv);
  // ---------------- 18. WordPro ----------------
  await q("W.pp.sendToWordPro()"); await wait(800);
  ok('18. Send to WordPro makes a handout: the slide titles as headings, the bullets and the notes', await q("(()=>{const w=wins.at(-1);const ed=w.el.querySelector('.wp-ed');return!!ed&&/Our year in review/.test(ed.querySelector('h1').textContent)&&ed.querySelectorAll('h2').length==4&&/Leeds/.test(ed.textContent)})()"));
  await q("(()=>{const w=wins.at(-1);w.el.querySelector('.wp-ed').innerHTML='<h1>Plan</h1><p>For the team</p><h2>Goals</h2><ul><li>Grow</li><li>Hire</li></ul><h2>Budget</h2><p>Within limits</p>';const b=[...w.el.querySelectorAll('.wp-mb button')].find(b=>b.textContent=='File');b.click();const it=[...w.el.querySelectorAll('.wp-di')].find(d=>/Send to PresentPro/.test(d.textContent));it.click()})()"); await wait(800);
  ok('...WordPro\'s File › Send to PresentPro makes slides from the headings', await q("(()=>{const w=wins.at(-1);const d=w.pp&&w.pp.deck;return!!d&&d.slides.length==3&&d.slides[0].layout=='title'&&/<li>Grow<\\/li><li>Hire<\\/li>/.test(d.slides[1].objs[1].html)})()"), await q("wins.at(-1).pp&&wins.at(-1).pp.deck.slides.map(s=>s.objs.map(o=>o.html))"));
  await q("wins.at(-1).__force=1;wins.at(-1).close()");
  await q("OFFCLIP={kind:'range',rows:[['A','B'],['1','2']],html:'<table><tr><td>A</td><td>B</td></tr><tr><td>1</td><td>2</td></tr></table>'};(()=>{const w=wins.at(-1);const b=[...w.el.querySelectorAll('.wp-mb button')].find(b=>b.textContent=='Insert');b.click();[...w.el.querySelectorAll('.wp-di')].find(d=>/Spreadsheet table/.test(d.textContent)).click()})()"); await wait(300);
  ok('...WordPro\'s Insert › Spreadsheet table puts in the range copied in SpreadsheetPro', await q("(()=>{const w=wins.at(-1);const t=w.el.querySelector('.wp-ed table');return!!t&&t.rows.length==2})()"));
  await q("(()=>{const w=wins.at(-1);w.__force=1;w.close()})()");
  // ---------------- 19. the Office group, a big deck ----------------
  await q("startMenuApps()");
  ok('19. the Start menu has an Office group with WordPro, SpreadsheetPro and PresentPro', await q("(()=>{const h=[...document.querySelectorAll('#menu li.offh')];const it=[...document.querySelectorAll('#menu li.offi')].map(l=>l.textContent);return h.length==1&&it.join('|')=='📝 WordPro|📊 SpreadsheetPro|📽️ PresentPro'})()"), await q("[...document.querySelectorAll('#menu li.offi')].map(l=>l.textContent)"));
  const t50 = await q(`(async()=>{const D=ppDeckNew('slate');const img=await offShrink(node([...homePath('PICTURES'),'RED.PNG']),1920);D.slides=Array.from({length:50},(_,i)=>{const s=ppSlide(D,'picture',{title:'Photo '+(i+1),sub:'Caption'});s.objs.find(o=>o.t=='img').img=img;s.notes='n'+i;return s});node(homePath('DOCUMENTS'))['BIG.RDECK']=ppToJSON(D);const t=performance.now();window.W5=deckApp([...homePath('DOCUMENTS'),'BIG.RDECK']);await new Promise(r=>{const f=()=>{if(W5.pp.deck.slides.length==50&&W5.el.querySelectorAll('.pp-th').length==50)r();else setTimeout(f,20)};f()});return[performance.now()-t,node(homePath('DOCUMENTS'))['BIG.RDECK'].length]})()`);
  console.log(`   a 50-slide deck with pictures: opens in ${t50[0].toFixed(0)} ms, ${(t50[1] / 1048576).toFixed(1)} MB on disk`);
  ok('20. a 50-slide deck with pictures opens within 2 seconds', t50[0] < 2000, t50);
  await q("W5.close()");
  await q("W.pp.setView('sorter')"); await wait(300); await shot('04-sorter');
  ok('21. the slide sorter shows every slide', await q("W.el.querySelectorAll('.pp-sorter .pp-th').length==4"));
  await q("W.pp.setView('normal')");
  ok('22. templates: talk, lesson, report, photo album, pitch', await q("['talk','lesson','report','album','pitch'].every(k=>ppTemplate(k).slides.length>=5)"));
  // menus open
  const menus = await q("(async()=>{const out=[];for(const b of W.el.querySelectorAll('.wp-mb button')){b.click();await new Promise(r=>setTimeout(r,30));const d=W.el.querySelector('.wp-dd');out.push(b.textContent+':'+(d?d.querySelectorAll('.wp-di').length:0));b.click()}return out.join(' ')})()");
  ok('23. every menu opens with its items', !/:0\b/.test(menus), menus);
  await q("W.pp.themeDlg()"); await wait(200); await shot('05-themes'); await q("W.el.querySelector('.wp-dlg button').click()");
  ok('24. text from a file cannot carry scripts or event handlers', await q("ppClean('<p onclick=\"x()\">a<script>alert(1)</script><img src=x onerror=alert(1)><span style=\"color:red;background:url(javascript:1)\">b</span></p>')") == '<p>a<span style="color:red">b</span></p>', await q("ppClean('<p onclick=\"x()\">a<script>alert(1)</script><img src=x onerror=alert(1)><span style=\"color:red;background:url(javascript:1)\">b</span></p>')"));
  ok('no page error', !errors.length, errors);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
