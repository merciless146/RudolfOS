// 1.5.9 (plan 15.2): a file opened in an app cannot run anything in the desktop's page. Hostile files (script in every
// place each format allows it, and in file names and extensions) are opened in each app that opens them, and shown in
// File Explorer, Open with, Properties and the recent list; a canary in the page (top.__pwn) must never be called.
// A stand-in rudolfd.   node hostile59_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 700) : '')) };
const srv = http.createServer((q, s) => { const [u0] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};window.__hit=[];window.__pwn=function(id){window.__hit.push(String(id))};</script>')) }
    if (u.startsWith('/a/') || u.startsWith('/lib/')) { const f = path.join(WWW, path.normalize(u)); if (f.startsWith(WWW) && fs.existsSync(f) && !fs.statSync(f).isDirectory()) { s.writeHead(200); return s.end(fs.readFileSync(f)) } }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      s.writeHead(200, {'content-type': 'application/json'}); return s.end('{"ok":true}') }
    s.writeHead(404); s.end() }) }).listen(8303, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
// the payloads: each calls the canary with its own name if it runs in (or can reach) the desktop's page
const P = id => `<img src=x onerror="top.__pwn('${id}-img')"><svg onload="top.__pwn('${id}-svg')"></svg><script>top.__pwn('${id}-script')</script><iframe srcdoc="<script>top.__pwn('${id}-srcdoc')</script>"></iframe><a href="javascript:top.__pwn('${id}-href')">x</a><details open ontoggle="top.__pwn('${id}-toggle')"></details>`;
const NAME = `<img src=x onerror="top.__pwn('name')">.txt`, EXT = `x.<img src=x onerror="top.__pwn('ext')">`;
const SVG = `<svg xmlns="http://www.w3.org/2000/svg" onload="top.__pwn('svg-onload')"><script>top.__pwn('svg-script')</script><foreignObject><img xmlns="http://www.w3.org/1999/xhtml" src="x" onerror="top.__pwn('svg-fo')"/></foreignObject></svg>`;
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1400, height: 900}});
  await p.goto('http://127.0.0.1:8303/'); await p.waitForFunction(() => typeof openPath == 'function' && typeof OPENERS == 'object', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x'}];USER=ACCTS[0];return 1})()`);
  const files = {
    [NAME]: 'hello', [EXT]: 'hello',
    'note.txt': P('txt'), 'page.html': P('html'), 'page.htm': P('htm'), 'doc.rtf': '{\\rtf1 ' + P('rtf') + '}', 'doc.doc': P('doc'),
    'data.csv': `a,b\n"${P('csv').replace(/"/g, '""')}",2\n`, 'data.tsv': `a\tb\n${P('tsv')}\t2\n`,
    'pic.svg': 'data:image/svg+xml;base64,' + Buffer.from(SVG).toString('base64'), 'raw.svg': SVG,
    'readme.md': '# t\n\n' + P('md') + '\n\n[x](javascript:top.__pwn("md-link"))', 'script.lua': '-- ' + P('lua'),
    'page.xml': '<?xml version="1.0"?><r>' + P('xml') + '</r>', 'mail.eml': 'Subject: ' + P('eml-subj') + '\n\n' + P('eml'),
  };
  files['slash.doc'] = `<p>x</p><img src=x/onerror=top.__pwn('wp-slash')><svg><a><animate attributeName=href values="javascript:top.__pwn('wp-anim')"/><text y=20>t</text></a></svg>`;
  // zip-based files, made here with the page's own JSZip: an archive with hostile names, a workbook and a deck with
  // hostile text, sheet and slide names (minimal Office files: enough for the importers)
  const zips = await q(`(async()=>{const P=${JSON.stringify(P('ZID'))},N=${JSON.stringify(NAME)};const out={};
    const du=async z=>'data:application/zip;base64,'+await z.generateAsync({type:'base64'});
    let z=new JSZip();z.file(N,'x');z.file('d/'+N+'/y.txt','y');z.file('n.txt',P.replace(/ZID/g,'zipc'));out['names.zip']=await du(z);
    const ss='<?xml version="1.0"?><sst xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" count="1" uniqueCount="1"><si><t>'+P.replace(/ZID/g,'xlsx').replace(/&/g,'&amp;').replace(/</g,'&lt;')+'</t></si></sst>';
    z=new JSZip();z.file('[Content_Types].xml','<?xml version="1.0"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="xml" ContentType="application/xml"/><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/><Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/><Override PartName="/xl/sharedStrings.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml"/></Types>');
    z.file('_rels/.rels','<?xml version="1.0"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>');
    z.file('xl/workbook.xml','<?xml version="1.0"?><workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets><sheet name="'+N.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/"/g,'&quot;')+'" sheetId="1" r:id="rId1"/></sheets></workbook>');
    z.file('xl/_rels/workbook.xml.rels','<?xml version="1.0"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/><Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/sharedStrings" Target="sharedStrings.xml"/></Relationships>');
    z.file('xl/sharedStrings.xml',ss);
    z.file('xl/worksheets/sheet1.xml','<?xml version="1.0"?><worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData><row r="1"><c r="A1" t="s"><v>0</v></c><c r="B1" t="inlineStr"><is><t>'+P.replace(/ZID/g,'xlsx-inline').replace(/&/g,'&amp;').replace(/</g,'&lt;')+'</t></is></c></row></sheetData></worksheet>');
    out['book.xlsx']=await du(z);
    const tx=P.replace(/ZID/g,'pptx').replace(/&/g,'&amp;').replace(/</g,'&lt;');
    z=new JSZip();z.file('[Content_Types].xml','<?xml version="1.0"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="xml" ContentType="application/xml"/><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Override PartName="/ppt/presentation.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml"/><Override PartName="/ppt/slides/slide1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slide+xml"/></Types>');
    z.file('_rels/.rels','<?xml version="1.0"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="ppt/presentation.xml"/></Relationships>');
    z.file('ppt/presentation.xml','<?xml version="1.0"?><p:presentation xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main"><p:sldIdLst><p:sldId id="256" r:id="rId1"/></p:sldIdLst><p:sldSz cx="9144000" cy="6858000"/></p:presentation>');
    z.file('ppt/_rels/presentation.xml.rels','<?xml version="1.0"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide" Target="slides/slide1.xml"/></Relationships>');
    z.file('ppt/slides/slide1.xml','<?xml version="1.0"?><p:sld xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main" xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><p:cSld><p:spTree><p:sp><p:spPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="4000000" cy="1000000"/></a:xfrm></p:spPr><p:txBody><a:bodyPr/><a:p><a:r><a:t>'+tx+'</a:t></a:r></a:p></p:txBody></p:sp></p:spTree></p:cSld><p:notes>'+tx+'</p:notes></p:sld>');
    out['deck.pptx']=await du(z);return out})()`);
  Object.assign(files, zips);
  await q(`(()=>{const F=${JSON.stringify(files)};const d=dirOf(['C:','h','x']);if(!d||!isDir(node(['C:','h']))){node(['C:']).h={};}const h=node(['C:','h']);for(const k in F)h[k]=F[k];fsSave();return Object.keys(h).length})()`);
  const hits = async () => q('window.__hit.splice(0)');
  const closeAll = async () => { await q("(()=>{try{wins.slice().forEach(w=>{try{w.close()}catch(e){}});document.querySelectorAll('.rmodal,.cmenu,.sdlg').forEach(x=>x.remove())}catch(e){}return 1})()"); await wait(150) };
  const opened = [];
  // each file in each app that opens it, and by its default (openPath)
  for (const f of Object.keys(files)) {
    const ops = await q(`openersFor(['C:','h',${JSON.stringify(f)}]).filter(o=>o[4]).map(o=>o[0])`);
    for (const o of ops) {
      await q(`(()=>{try{OPENERS.find(x=>x[0]==${JSON.stringify(o)})[4](['C:','h',${JSON.stringify(f)}])}catch(e){window.__operr=(window.__operr||[]).concat(String(e))}return 1})()`); await wait(900);
      const js = await q("[...document.querySelectorAll('a[href]')].filter(a=>/^\\s*javascript:/i.test(a.getAttribute('href'))&&a.isConnected).map(a=>a.getAttribute('href')).slice(0,3)");
      const shown = await q("[...document.querySelectorAll('.win')].map(w=>w.innerText+' '+[...w.querySelectorAll('input,textarea')].map(x=>x.value).join(' ')).join(' ')");
      opened.push([f, o, await hits(), js, /(xlsx|pptx|zipc|name)-img|top\.__pwn|<img|onerror/.test(shown) || f == 'names.zip' && /img src=x/.test(shown)]); await closeAll() }
    await q(`(()=>{try{openPath(['C:','h',${JSON.stringify(f)}])}catch(e){}return 1})()`); await wait(900);
    opened.push([f, 'default', await hits(), []]); await closeAll() }
  const bad = opened.filter(x => x[2].length || x[3].length);
  ok('...the archive, the workbook and the deck were opened by their apps, their hostile text shown as text', ['names.zip', 'book.xlsx', 'deck.pptx'].every(f => opened.some(x => x[0] == f && x[1] != 'default' && x[1] != 'notepad' && x[1] != 'code' && x[4])), opened.filter(x => /zip|xlsx|pptx/.test(x[0])).map(x => x[0] + ':' + x[1] + ':' + x[4]));
  ok('1. every hostile file, in every app that opens it: nothing runs in the desktop\'s page', !bad.length, bad.map(x => `${x[0].slice(0, 40)} in ${x[1]}: ${x[2].join(' ')} ${x[3].join(' ')}`));
  ok('...(files opened: ' + opened.length + ')', opened.length > 25, opened.length);
  // File Explorer on the folder (list and icons), its status, Open with, Properties, Rename
  await q("explorer(['C:','h'])"); await wait(800);
  await q("(()=>{const W=wins.at(-1);const b=W.body.querySelector('.vw');b&&b.click();return 1})()"); await wait(500);
  ok('2. File Explorer showing them (list and icons): nothing runs', !(await hits()).length, await hits());
  for (const f of [NAME, EXT]) {
    await q(`openWith(['C:','h',${JSON.stringify(f)}])`); await wait(400);
    const h = await hits(); ok('3. Open with for ' + (f == NAME ? 'a hostile name' : 'a hostile extension') + ': nothing runs', !h.length, h); await closeAll() }
  for (const f of [NAME, EXT]) {
    await q(`(()=>{try{if(typeof fileProps=='function')fileProps(['C:','h',${JSON.stringify(f)}]);else if(typeof propsOf=='function')propsOf(['C:','h',${JSON.stringify(f)}])}catch(e){}return 1})()`); await wait(400);
    const h = await hits(); ok('4. Properties of ' + (f == NAME ? 'a hostile name' : 'a hostile extension') + ': nothing runs', !h.length, h); await closeAll() }
  await q("(()=>{try{document.querySelector('#startb, #start').click()}catch(e){}return 1})()"); await wait(500);
  ok('5. the Start menu with them in Recent: nothing runs', !(await hits()).length, await hits()); await closeAll();
  await q("(()=>{try{wgDraw&&wgDraw();notify('Downloads',"+JSON.stringify(NAME + ' downloaded')+")}catch(e){}return 1})()"); await wait(600);
  ok('6. a notification naming one: nothing runs', !(await hits()).length, await hits());
  ok('7. the canary itself works (a check of the test)', await q("(()=>{const d=document.createElement('div');d.innerHTML=`<img src=x onerror=\"top.__pwn('self')\">`;document.body.appendChild(d);return new Promise(r=>setTimeout(()=>{d.remove();r(window.__hit.splice(0).includes('self'))},300))})()"));
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
