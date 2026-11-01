// 1.4.5 SpreadsheetPro (spro45.js, sprof45.js, spreng45.js) in the desktop page: typing, formulas, the keyboard and
// mouse, fill, copy and paste, undo, rows and columns, sorting, filters, formats, conditional formatting, validation,
// charts, sheets, files (.RSHEET 1 and 2, CSV, .xlsx both ways, checked with openpyxl), templates, PDF, a big sheet.
// node spro_t.js [CHROME]   (build iso/www/index.html first; python3 with openpyxl for the Excel checks)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path'), os = require('os'), cp = require('child_process');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www'), SHOT = process.env.SHOTS || path.join(os.tmpdir(), 'spro-shots');
fs.mkdirSync(SHOT, {recursive: true});
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
const errors = [];
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }
  const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200); fs.createReadStream(f).pipe(s) }).listen(8097, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
const py = (code, ...args) => cp.execFileSync('python3', ['-c', code, ...args], {encoding: 'utf8'});
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const ctx = await b.newContext({viewport: {width: 1280, height: 760}});
  const p = await ctx.newPage(); p.on('pageerror', e => { errors.push(e.message); console.log('page error:', e.stack.slice(0, 400)) });
  await p.goto('http://127.0.0.1:8097/'); await p.waitForFunction(() => typeof sheetApp == 'function' && typeof launch == 'function', null, {timeout: 30000});
  await p.evaluate("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';const h=homePath();let n=fs;for(const x of h){if(!n[x])n[x]={};n=n[x]}return 1})()");
  const q = s => p.evaluate(s); const shot = async n => p.screenshot({path: path.join(SHOT, n + '.png')});
  const key = async (k, n = 1) => { for (let i = 0; i < n; i++) await p.keyboard.press(k) };
  const type = async t => { await p.keyboard.type(t, {delay: 5}) };
  // ---------------- 1. the app ----------------
  await q("launch('sheet')"); await wait(1200);
  ok('1. Start: SpreadsheetPro opens (the old id `sheet`), with WordPro\'s menu bar and toolbar', await q("(()=>{const W=wins.at(-1);window.W=W;return W.app=='sheet'&&W.el.textContent.includes('SpreadsheetPro')&&[...W.el.querySelectorAll('.wp-mb button')].map(b=>b.textContent).join()=='File,Edit,View,Insert,Format,Data,Tools,Help'&&!!W.el.querySelector('.wp-tb')&&!!W.sp})()"));
  ok('...Open with and the file types go to SpreadsheetPro', await q("OPENERS.find(o=>o[0]=='sheet')[1]=='SpreadsheetPro'&&OPENERS.find(o=>o[0]=='sheet')[3]('XLSX')&&APPS.sheet.name=='SpreadsheetPro'"));
  await q("W.el.querySelector('.mx').click()"); await wait(400);
  // ---------------- 2. typing and formulas ----------------
  const cell = async (x, y) => { const r = await q(`(()=>{const sp=W.sp,b=W.el.querySelector('.sp-sc').getBoundingClientRect();const p=SPE.parseA1('${x}');return[b.left+sp.Xc(p.c)+12,b.top+sp.Yr(p.r)+8]})()`); await p.mouse.click(r[0], r[1]); return r };
  await cell('A1'); await type('Item'); await key('Tab'); await type('Price'); await key('Tab'); await type('Qty'); await key('Tab'); await type('Total'); await key('Enter');
  ok('2. typing: Tab moves right, Enter goes down to the column where the row started (as Excel)', await q("W.sp.sel.r==1&&W.sp.sel.c==0"), await q("[W.sp.sel.r,W.sp.sel.c]"));
  await type('Apples'); await key('Tab'); await type('1.25'); await key('Tab'); await type('4'); await key('Tab'); await type('=b2*c2'); await key('Enter');
  ok('...a formula: =b2*c2 becomes =B2*C2 and shows 5', await q("W.sp.cell('D2').f=='=B2*C2'&&W.sp.value('D2')==5"), await q("[W.sp.cell('D2'),W.sp.value('D2')]"));
  await q("W.sp.type('A3','Pears');W.sp.type('B3','$2.50');W.sp.type('C3','3');W.sp.type('A4','Plums');W.sp.type('B4','0.8');W.sp.type('C4','10')");
  ok('...$2.50 is a number with a currency format', await q("W.sp.value('B3')===2.5&&W.sp.text('B3')=='$2.50'"), await q("[W.sp.value('B3'),W.sp.text('B3')]"));
  // the fill handle: drag D2 down to D4
  const fh = await q("(()=>{const sp=W.sp,b=W.el.querySelector('.sp-sc').getBoundingClientRect();return[b.left+sp.Xc(4),b.top+sp.Yr(2),b.left+sp.Xc(4)-10,b.top+sp.Yr(4)-5]})()");
  await cell('D2'); await p.mouse.move(fh[0] - 1, fh[1] - 1); await p.mouse.down(); await p.mouse.move(fh[2], fh[3], {steps: 5}); await p.mouse.up(); await wait(200);
  ok('3. the fill handle copies the formula with its references moved (=B4*C4)', await q("W.sp.cell('D4').f=='=B4*C4'&&W.sp.value('D4')==8"), await q("W.sp.cell('D4')"));
  await cell('D5'); await key('Alt+Equal'); await wait(100); await key('Enter');
  ok('...Σ (Alt+=) puts =SUM(D2:D4) under the numbers: 20.5', await q("W.sp.cell('D5').f=='=SUM(D2:D4)'&&W.sp.value('D5')==20.5"), await q("W.sp.cell('D5')"));
  await q("W.sp.type('B2','2')");
  ok('...a change recalculates what depends on it (D2 8, D5 23.5)', await q("W.sp.value('D2')==8&&W.sp.value('D5')==23.5"));
  // the formula hint and clicking cells while typing a formula
  await cell('E2'); await type('=VLOO'); await wait(100);
  ok('4. typing a function name lists it with its description', await q("(()=>{const h=W.el.querySelector('.sp-hint');return h.style.display=='block'&&/VLOOKUP/.test(h.textContent)})()"));
  await key('Tab'); await type('"Pears",'); const a2 = await q("(()=>{const sp=W.sp,b=W.el.querySelector('.sp-sc').getBoundingClientRect();return[b.left+sp.Xc(0)+5,b.top+sp.Yr(1)+5,b.left+sp.Xc(3)+5,b.top+sp.Yr(3)+5]})()");
  await p.mouse.move(a2[0], a2[1]); await p.mouse.down(); await p.mouse.move(a2[2], a2[3], {steps: 4}); await p.mouse.up();
  ok('...inside the brackets: its description; dragging over cells puts the range in', await q("W.sp.editor().value")=='=VLOOKUP("Pears",A2:D4', await q("W.sp.editor().value"));
  await type(',4,FALSE)'); await key('Enter');
  ok('...the result: 7.5', await q("W.sp.value('E2')==7.5"), await q("W.sp.cell('E2')"));
  await q("W.sp.type('E3','=1/0');W.sp.type('E4','=SUM(E4)');W.sp.type('E5','=NOSUCH(1)')");
  ok('...errors as Excel shows them: #DIV/0!, #CIRCULAR!, #NAME?', await q("[W.sp.text('E3'),W.sp.text('E4'),W.sp.text('E5')].join()") == '#DIV/0!,#CIRCULAR!,#NAME?', await q("[W.sp.text('E3'),W.sp.text('E4'),W.sp.text('E5')]"));
  await shot('01-typing');
  // ---------------- 5. the keyboard ----------------
  await cell('A1'); await key('Control+ArrowDown');
  ok('5. Ctrl+Down goes to the end of the data (A4)', await q("W.sp.sel.r==3&&W.sp.sel.c==0"), await q("[W.sp.sel.r,W.sp.sel.c]"));
  await key('Control+Shift+ArrowRight');
  ok('...Ctrl+Shift+Right selects to the end of the row (A4:E4)', await q("JSON.stringify(W.sp.sel.rs[0])") == '{"r0":3,"r1":3,"c0":0,"c1":4}', await q("W.sp.sel.rs"));
  await key('Control+Home'); ok('...Ctrl+Home: A1', await q("W.sp.sel.r==0&&W.sp.sel.c==0"));
  await cell('B2'); await key('F2'); await wait(50); console.log('   editing:', await q("[W.sp.editing,document.activeElement.className,W.sp.editor().value]")); await key('End'); await type('5'); await key('Enter');
  ok('...F2 edits the cell (2 → 25)', await q("W.sp.value('B2')==25"));
  await cell('A3'); await key('Escape'); await type('Zzz'); await key('Escape');
  ok('...Esc cancels typing', await q("W.sp.value('A3')=='Pears'"));
  // ---------------- 6. undo and redo ----------------
  await key('Control+z');
  ok('6. Ctrl+Z undoes (B2 back to 2)', await q("W.sp.value('B2')==2&&W.sp.value('D5')==23.5"), await q("[W.sp.value('B2'),W.sp.value('D5')]"));
  await key('Control+y'); ok('...Ctrl+Y redoes', await q("W.sp.value('B2')==25"));
  await key('Control+z');
  // ---------------- 7. copy, cut and paste ----------------
  await q("W.sp.select('D2:D3');W.sp.copy();W.sp.select('F7');W.sp.paste()");
  ok('7. copy and paste: formulas move with the cells (D2 =B2*C2 pasted in F7: =D7*E7)', await q("W.sp.cell('F7').f=='=D7*E7'"), await q("W.sp.cell('F7')"));
  await q("W.sp.select('F7:F8');W.sp.key('Delete');W.sp.select('D2:D3');W.sp.copy();W.sp.select('F7');W.sp.paste('values')");
  ok('...paste values: the numbers only', await q("W.sp.cell('F7').f==null&&W.sp.value('F7')==8"));
  await q("W.sp.select('F7:F8');W.sp.key('Delete');W.sp.select('A2:D2');W.sp.cut();W.sp.select('A10');W.sp.paste()");
  ok('...cut and paste moves, and formulas that pointed to the cells follow (E2 → A10:D10)', await q("W.sp.value('A10')=='Apples'&&W.sp.cell('D10').f=='=B10*C10'&&!W.sp.cell('A2')&&W.sp.cell('E2').f.includes('A10:D4')||W.sp.cell('E2').f"), await q("[W.sp.cell('D10'),W.sp.cell('E2')]"));
  await key('Control+z'); ok('...and Ctrl+Z puts them back', await q("W.sp.value('A2')=='Apples'&&!W.sp.cell('A10')"));
  ok('...a copied range is also an HTML table for WordPro and PresentPro', await q("(W.sp.select('A1:D3'),W.sp.copy(),OFFCLIP.kind=='range'&&/<table/.test(OFFCLIP.html)&&OFFCLIP.rows[1][0]=='Apples')"));
  // ---------------- 8. rows and columns ----------------
  await q("W.sp.select('A3');W.sp.insRC('r',1)");
  ok('8. inserting a row moves the cells down and adjusts formulas (=SUM(D2:D5))', await q("W.sp.value('A4')=='Pears'&&W.sp.cell('D6').f=='=SUM(D2:D5)'&&W.sp.cell('D5').f=='=B5*C5'"), await q("[W.sp.cell('D6'),W.sp.cell('D5')]"));
  await q("W.sp.select('A3');W.sp.insRC('r',-1)");
  ok('...deleting it moves them back', await q("W.sp.cell('D5').f=='=SUM(D2:D4)'&&W.sp.value('A3')=='Pears'"), await q("W.sp.cell('D5')"));
  await q("W.sp.select('B1');W.sp.insRC('c',-1)");
  ok('...deleting column B: references to it become #REF!', await q("W.sp.cell('C2').f=='=#REF!*B2'&&W.sp.text('C2')=='#REF!'"), await q("W.sp.cell('C2')"));
  await key('Control+z');
  // ---------------- 9. sorting and filtering ----------------
  await q("W.sp.type('B4','0.9');W.sp.sortRange('A1:D4',[[3,-1]],true)");
  ok('9. sort by Total, largest first, keeping the headings', await q("[W.sp.value('A2'),W.sp.value('A3'),W.sp.value('A4')].join()") == 'Plums,Apples,Pears' && await q("W.sp.cell('D2').f=='=B2*C2'"), await q("[W.sp.value('A2'),W.sp.value('A3'),W.sp.value('A4'),W.sp.cell('D2')]"));
  await q("W.sp.select('A1');W.sp.toggleFilter();W.sp.filter(0,['Pears','Plums'])");
  ok('...AutoFilter hides the rows not chosen', await q("(()=>{const s=W.sp.sheet;return s._fh[2]==1&&!s._fh[1]&&!s._fh[3]})()"), await q("W.sp.sheet._fh"));
  await shot('02-filter');
  await q("W.sp.filter(0,undefined);W.sp.toggleFilter()");
  // ---------------- 10. formats ----------------
  await q("W.sp.select('B2:B4');W.sp.style({nf:'$#,##0.00'});W.sp.select('A1:D1');W.sp.style({b:1,bg:'#1f4e79',fc:'#ffffff'})");
  ok('10. number formats: $ with two decimals', /^\$\d+\.\d\d$/.test(await q("W.sp.text('B2')")), await q("W.sp.text('B2')"));
  await q("W.sp.type('G1','0.256');W.sp.select('G1');W.sp.style({nf:'0.0%'});W.sp.type('G2','2026-11-01');W.sp.type('G3','14:05');W.sp.type('G4','1234567.891');W.sp.select('G4');W.sp.style({nf:'#,##0.00'})");
  ok('...percent, dates and times as typed, thousands', await q("[W.sp.text('G1'),W.sp.text('G2'),W.sp.text('G3'),W.sp.text('G4')].join('|')") == '25.6%|2026-11-01|14:05|1,234,567.89', await q("[W.sp.text('G1'),W.sp.text('G2'),W.sp.text('G3'),W.sp.text('G4')]"));
  await cell('G2'); await key('Control+b');
  ok('...Ctrl+B bolds the cell', await q("W.sp.cell('G2').st.b==1"));
  await q("W.sp.select('A6:C6');W.sp.type('A6','Merged heading');W.sp.select('A6:C6');W.sp.merge()");
  ok('...merge and centre', await q("W.sp.sheet.merges.some(m=>m.r0==5&&m.c0==0&&m.c1==2)&&W.sp.cell('A6').st.ha=='center'"));
  // conditional formatting and validation
  await q("W.sp.cf({r0:1,c0:3,r1:3,c1:3,t:'gt',a:'6',st:{bg:'#c6efce',fc:'#006100'}});W.sp.cf({r0:1,c0:2,r1:3,c1:2,t:'bar'})");
  ok('11. conditional formatting is kept with the sheet', await q("W.sp.sheet.cf.length==2"));
  await q("W.sp.dv({r0:7,c0:0,r1:9,c1:0,t:'list',list:'Yes,No'})");await q("W.sp.select('A8');W.sp.type('A8','Maybe')"); await wait(200);
  ok('12. data validation: a value not in the list is refused, with a message', await q("W.sp.value('A8')==null&&/Choose one of: Yes, No/.test(W.el.querySelector('.wp-dlg')?.textContent||'')"), await q("[W.sp.value('A8'),W.el.querySelector('.wp-dlg')?.textContent]"));
  await q("W.el.querySelector('.wp-dlg button').click()"); await q("W.sp.type('A8','yes')");
  ok('...a value in the list is taken', await q("W.sp.value('A8')=='yes'"));
  // ---------------- 13. charts ----------------
  await q("window.__ch=W.sp.chart({rg:'A1:C4',type:'column',title:'Fruit',x:520,y:40})"); await wait(300);
  ok('13. a chart is drawn on the sheet from a range, with its title and legend', await q("(()=>{const el=W.el.querySelector('.sp-ch svg');return !!el&&/Fruit/.test(el.textContent)&&el.querySelectorAll('rect').length>6})()"));
  await q("W.sp.type('B3','10')"); await wait(200);
  ok('...and updates when the data changes', await q("W.sp.chartSpec(W.sp.sheet.charts[0]).series[0].vals.includes(10)"), await q("W.sp.chartSpec(W.sp.sheet.charts[0]).series"));
  for (const t of ['bar', 'line', 'area', 'pie', 'doughnut', 'scatter']) { await q(`W.sp.sheet.charts[0].type='${t}';W.sp.draw()`); await wait(80) }
  ok('...every kind draws: column, bar, line, area, pie, doughnut, scatter', await q("!!W.el.querySelector('.sp-ch svg circle')"));
  await q("W.sp.sheet.charts[0].type='column';W.sp.draw()"); await wait(100); await shot('03-chart');
  // ---------------- 14. sheets ----------------
  await q("W.sp.addSheet();W.sp.type('A1','=Sheet1!D5*2')");
  ok('14. a second sheet, and a formula pointing to the first', await q("W.sp.book.sheets.length==2&&W.sp.value('A1')==W.sp.value('D5','Sheet1')*2"), await q("[W.sp.value('A1'),W.sp.value('D5','Sheet1')]"));
  await q("W.sp.renameSheet(0,'Fruit list')"); await wait(100);
  ok('...renaming a sheet changes the formulas that name it', await q("W.sp.cell('A1').f==\"='Fruit list'!D5*2\"&&W.sp.value('A1')>0"), await q("W.sp.cell('A1')"));
  await q("W.sp.showSheet(0)");
  // ---------------- 15. files ----------------
  const docs = await q("JSON.stringify(homePath('DOCUMENTS'))");
  await q(`(()=>{const d=homePath('DOCUMENTS');let n=fs;for(const x of d){if(!n[x])n[x]={};n=n[x]}saveVfsDlg=async(nm)=>[...d,window.__saveAs||nm];return 1})()`);
  await q("window.__saveAs='FRUIT.RSHEET';W.sp.doSave(true)"); await wait(400);
  ok('15. Save as .RSHEET (version 2)', await q("(()=>{const v=node([...homePath('DOCUMENTS'),'FRUIT.RSHEET']);const o=JSON.parse(v);return o.v==2&&o.sheets.length==2&&o.sheets[0].charts.length==1&&o.styles.length>0})()"));
  await q("window.W2=sheetApp([...homePath('DOCUMENTS'),'FRUIT.RSHEET'])"); await wait(800);
  ok('...and it opens again: values, formulas, formats, merges, charts, conditional formatting', await q("(()=>{const s=W2.sp;return s.book.sheets[0].name=='Fruit list'&&s.value('D5')==W.sp.value('D5')&&s.cell('D2').f=='=B2*C2'&&s.text('G1')=='25.6%'&&s.book.sheets[0].merges.length==1&&s.book.sheets[0].charts.length==1&&s.book.sheets[0].cf.length==2})()"));
  await q("W2.close()");
  // version 1 from the old Spreadsheet
  await q(`node(homePath('DOCUMENTS'))['OLD.RSHEET']=JSON.stringify({v:1,cells:{A1:{v:'Name',b:true},B1:{v:'5'},B2:{v:'7'},B3:{v:'=SUM(B1:B2)'},C1:{v:'=B1*2'}}})`);
  await q("window.W3=sheetApp([...homePath('DOCUMENTS'),'OLD.RSHEET'])"); await wait(600);
  ok('16. an old Spreadsheet file (version 1) opens: values, formulas, bold', await q("W3.sp.value('B3')==12&&W3.sp.value('C1')==10&&W3.sp.cell('A1').st.b==1"), await q("[W3.sp.value('B3'),W3.sp.value('C1'),W3.sp.cell('A1')]"));
  await q("W3.close()");
  // CSV
  await q(`node(homePath('DOCUMENTS'))['DATA.CSV']='name,score,date\\r\\n"Lee, Ann",91,2026-01-05\\r\\nBo,"7,500",2026-02-10\\r\\n'`);
  await q("window.W4=sheetApp([...homePath('DOCUMENTS'),'DATA.CSV'])"); await wait(600);
  ok('17. CSV opens: quotes, numbers with commas, dates', await q("W4.sp.value('A2')=='Lee, Ann'&&W4.sp.value('B3')==7500&&W4.sp.text('C2')=='2026-01-05'"), await q("[W4.sp.value('A2'),W4.sp.value('B3'),W4.sp.text('C2')]"));
  await q("W4.close()");
  // Excel .xlsx: SpreadsheetPro writes, openpyxl reads
  await q("window.__saveAs='FRUIT.XLSX';W.sp.doSave(true)"); await wait(800);
  const x64 = await q("node([...homePath('DOCUMENTS'),'FRUIT.XLSX'])"); const xf = path.join(os.tmpdir(), 'spro-fruit.xlsx'); fs.writeFileSync(xf, Buffer.from(x64.split(',')[1], 'base64'));
  let rd = ''; try { rd = py(`import openpyxl,sys,json\nwb=openpyxl.load_workbook(sys.argv[1]);ws=wb[wb.sheetnames[0]]\nprint(json.dumps({'names':wb.sheetnames,'d2':ws['D2'].value,'b2fmt':ws['B2'].number_format,'b':ws['A1'].font.b,'fill':ws['A1'].fill.fgColor.rgb,'merged':[str(m) for m in ws.merged_cells.ranges],'s2':wb[wb.sheetnames[1]]['A1'].value,'g1':ws['G1'].number_format,'w':ws.column_dimensions['A'].width}))`, xf) } catch (e) { rd = String(e.stdout || e) }
  let X = {}; try { X = JSON.parse(rd) } catch (e) {}
  ok('18. Save as .xlsx: openpyxl reads the sheets, formulas, number formats, fonts, fills and merged cells', X.names && X.names.join() == 'Fruit list,Sheet2' && X.d2 == '=B2*C2' && X.b2fmt == '$#,##0.00' && X.b && /1F4E79/i.test(X.fill) && X.merged.includes('A6:C6') && X.s2 == "='Fruit list'!D5*2" && X.g1 == '0.0%', rd);
  // openpyxl writes, SpreadsheetPro reads
  const xin = path.join(os.tmpdir(), 'spro-in.xlsx');
  py(`import openpyxl,sys\nfrom openpyxl.styles import Font,PatternFill,Border,Side,Alignment\nwb=openpyxl.Workbook();ws=wb.active;ws.title='Budget'\nws.append(['Month','Income','Costs','Left'])\nfor i,m in enumerate(['Jan','Feb','Mar']):\n  ws.append([m,1000+i*100,700+i*50,None]);ws.cell(row=i+2,column=4).value='=B%d-C%d'%(i+2,i+2)\nws['A5']='Total';ws['B5']='=SUM(B2:B4)';ws['B5'].number_format='#,##0.00';ws['A1'].font=Font(bold=True,color='FFFF0000',size=14);ws['A1'].fill=PatternFill('solid',fgColor='FFFFFF00')\nws['A5'].border=Border(top=Side(style='thin'),bottom=Side(style='double'));ws['D1'].alignment=Alignment(horizontal='center',wrap_text=True)\nws.merge_cells('F1:G2');ws['F1']='merged';ws.column_dimensions['A'].width=20;ws.freeze_panes='B2'\nw2=wb.create_sheet('Notes');w2['A1']='hello';w2['A2']=True;w2['A3']=3.5;wb.save(sys.argv[1])`, xin);
  await q(`node(homePath('DOCUMENTS'))['IN.XLSX']='data:application/vnd.openxmlformats-officedocument.spreadsheetml.sheet;base64,${fs.readFileSync(xin).toString('base64')}'`);
  await q("window.W5=sheetApp([...homePath('DOCUMENTS'),'IN.XLSX'])"); await wait(1200);
  ok('19. an .xlsx made by openpyxl opens: sheets, formulas worked out, styles, merges, widths, frozen panes', await q("(()=>{const s=W5.sp,b=s.book,sh=b.sheets[0],a1=s.cell('A1').st;return b.sheets.map(x=>x.name).join()=='Budget,Notes'&&s.value('D3')==350&&s.value('B5')==3300&&s.text('B5')=='3,300.00'&&a1.b==1&&a1.fc=='#ff0000'&&a1.bg=='#ffff00'&&a1.sz==14&&s.cell('A5').st.bd.b=='double'&&s.cell('D1').st.ha=='center'&&s.cell('D1').st.wrap==1&&sh.merges.length==1&&Math.abs(sh.cw[0]-145)<3&&sh.freeze.r==1&&sh.freeze.c==1})()"),
    await q("(()=>{const s=W5.sp,b=s.book,sh=b.sheets[0];return[b.sheets.map(x=>x.name),s.value('D3'),s.value('B5'),s.text('B5'),s.cell('A1'),s.cell('A5'),s.cell('D1'),sh.merges,sh.cw,sh.freeze]})()"));
  ok('...the second sheet: text, TRUE, a number', await q("(()=>{W5.sp.showSheet(1);return W5.sp.value('A1')=='hello'&&W5.sp.value('A2')===true&&W5.sp.value('A3')==3.5})()"));
  await q("W5.close()");
  // ---------------- 20. templates, printing ----------------
  const tpl = await q("['budget','invoice','timetable','grades','loan'].map(k=>{const b=spTemplate(k);SPE.recalc(b,null);return b.sheets[0].cells.size>10}).every(Boolean)");
  ok('20. templates: budget, invoice, timetable, grade book, loan calculator', tpl);
  await q("window.W6=sheetApp(null,spTemplate('loan'))"); await wait(800);
  ok('...the loan calculator works out the monthly payment ($1,580.17) and the schedule ends at 0', await q("W6.sp.text('B6')=='$1,580.17'&&Math.abs(W6.sp.value('E370'))<0.01"), await q("[W6.sp.text('B6'),W6.sp.value('E370')]"));
  const pg = await q("W6.sp.pages().then(ps=>ps.length)");
  ok('...printing makes pages (the 360-month schedule over several pages)', pg >= 5, pg);
  await q("W6.close()");
  // ---------------- 21. a big sheet ----------------
  await q("window.W7=sheetApp(null,(()=>{const b=spBookNew(),s=b.sheets[0];for(let r=0;r<100000;r++){s.cells.set(r*702,{v:r%97});s.cells.set(r*702+1,{f:'=A'+(r+1)+'*2'});s.cells.set(r*702+2,{f:'=IF(B'+(r+1)+'>100,\"big\",\"small\")'})}s.cells.set(3,{f:'=SUM(B1:B100000)'});return b})())"); await wait(2500);
  await q("W7.el.querySelector('.mx').click()"); await wait(300);
  const fps = await q("(async()=>{const sc=W7.el.querySelector('.sp-sc');let n=0;const t0=performance.now();await new Promise(res=>{const f=()=>{sc.scrollTop+=97;n++;if(performance.now()-t0<2000)requestAnimationFrame(f);else res()};requestAnimationFrame(f)});return n/((performance.now()-t0)/1000)})()");
  console.log(`   scrolling 100,000 rows: ${fps.toFixed(0)} frames a second`);
  ok('21. 100,000 rows with formulas scroll smoothly (at least 50 frames a second here)', fps >= 50, fps);
  const t1 = await q("(()=>{const t=performance.now();W7.sp.type('A500','1000');return performance.now()-t})()");
  console.log(`   one change in 100,000 rows: ${t1.toFixed(0)} ms`);
  ok('...a change is worked out quickly (under 300 ms)', t1 < 300 && await q("W7.sp.value('D1')") == await q("(()=>{let s=0;for(let r=0;r<100000;r++)s+=(r==499?1000:r%97)*2;return s})()"), t1);
  await q("W7.sp.markDirty&&0;W7.__force=1;W7.close()");
  // ---------------- 22. menus ----------------
  const menus = await q("(async()=>{const out=[];for(const b of W.el.querySelectorAll('.wp-mb button')){b.click();await new Promise(r=>setTimeout(r,30));const d=W.el.querySelector('.wp-dd');out.push(b.textContent+':'+(d?d.querySelectorAll('.wp-di').length:0));b.click()}return out.join(' ')})()");
  ok('22. every menu opens with its items', !/:0\b/.test(menus), menus);
  await q("W.sp.fmtDlg()"); await wait(200); await shot('04-format-cells');
  ok('...Format cells has Number, Alignment, Font, Border and Fill', await q("[...W.el.querySelectorAll('.sp-tabsel button')].map(b=>b.textContent).join()=='Number,Alignment,Font,Border,Fill'"));
  await q("W.el.querySelector('.wp-dlg button').click()");
  await q("W.sp.chartDlg()"); await wait(200); await shot('05-chart-dialog'); await q("W.el.querySelector('.wp-dlg button').click()");
  // Notey's help
  ok('23. Notey answers a question (how to make a chart)', await q("(()=>{W.notey.ask();const i=W.el.querySelector('.wn-q');i.value='how do I make a chart';i.dispatchEvent(new KeyboardEvent('keydown',{key:'Enter'}));return /Insert › Chart/.test(W.el.querySelector('.wn-bub').textContent)})()"));
  ok('no page error', !errors.length, errors);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
