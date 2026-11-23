// 1.6.2 (docs/plan-1.6.2.md Part 34, step 2): the Scan app (desktop/scan62.js) in Playwright's Chromium, end to end: a
// stand-in rudolfd here answers scan/* by running this tree's rudolf-scan (RS_BIN) against the stand-in eSCL scanner
// (qt/host/fixtures/esclsrv.py, its known picture) on 127.0.0.1:8641, as rudolfd-scan.inc does (scan/find answers that
// scanner: multicast DNS is the host test's, scan162_t.sh). In the Start menu; the scanner found and asked what it can do;
// a preview; an area drawn on it and scanned (the area asked of the scanner); the feeder's three pages; pages put in order
// and taken out; saved as one PDF in Documents through the Save dialog (its pages checked by their pixels after pdftoppm),
// each page as PNG; a paper jam, the cover open and busy said plainly; no scanner said; File Explorer's Scan into this
// folder, the Print dialog's Scan… and Settings › Devices' Scanners; a plain browser told it needs Rudolf OS.
//   node scan62_t.js [CHROME]    (build iso/www/index.html first; RS_BIN: a rudolf-scan built from this tree; libsane1 and
//                                 sane-airscan installed; python3 with PIL and poppler-utils for the checks)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path'), os = require('os'), cp = require('child_process');
const WWW = path.join(__dirname, '../../../iso/www'), FX = path.join(__dirname, '../host/fixtures');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 700) : '')) };
const BIN = process.env.RS_BIN; if (!BIN || !fs.existsSync(BIN)) { console.log('SKIP: RS_BIN (a rudolf-scan built from this tree) is needed'); process.exit(0) }
const T = fs.mkdtempSync(path.join(os.tmpdir(), 'scan62-')), SESS = path.join(T, 'sess'); fs.mkdirSync(SESS);
const ES = cp.spawn('python3', [path.join(FX, 'esclsrv.py'), '127.0.0.1', '8641', path.join(T, 'ready'), '--name', 'Office Scanner'], {stdio: 'ignore'});
const ctl = q => new Promise(r => http.get('http://127.0.0.1:8641/_test/' + q, s => { let b = ''; s.on('data', d => b += d); s.on('end', () => r(b)) }).on('error', () => r('')));
const ID = 'escl:http://127.0.0.1:8641/eSCL';
const M = {found: true, asked: [], starts: [], saves: []};
const run = args => new Promise(res => { cp.execFile(BIN, args, {maxBuffer: 1 << 28, encoding: 'buffer'}, (e, out) => res(out)) });
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0); const P = new URLSearchParams(qs || ''); const b = [];
  q.on('data', d => b.push(d)); q.on('end', async () => {
    const J = (o, c) => { s.writeHead(c || 200, {'content-type': 'application/json'}); s.end(typeof o == 'string' ? o : JSON.stringify(o)) };
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0,engine:"chromium"};</script>')) }
    if (u == '/api/events/wait') { await new Promise(r => setTimeout(r, 800)); s.writeHead(200); return s.end('{"ok":true,"events":""}') }
    if (u.startsWith('/api/scan/')) {
      const w = u.slice(10); M.asked.push(w); if (q.headers['x-rudolf-token'] != 't') return J({ok: false, error: 'no token'}, 403);
      const dir = path.join(SESS, P.get('session') || 'none');
      if (w == 'find') return J({ok: true, scanners: M.found ? [{id: ID, name: 'Office Scanner', model: 'Office Scanner', via: 'network', proto: 'eSCL'}] : []});
      if (w == 'caps') return J((await run(['caps', P.get('id')])).toString());
      if (w == 'start') { fs.mkdirSync(dir, {recursive: true}); M.starts.push(Object.fromEntries(P)); const a = ['scan', P.get('id'), dir];
        for (const k of ['source', 'mode', 'res', 'area', 'first', 'preview']) if (P.get(k)) a.push(k + '=' + P.get(k)); return J((await run(a)).toString()) }
      if (w == 'page') { const f = path.join(dir, path.basename(P.get('name'))); if (!fs.existsSync(f)) return J({ok: false, error: 'no such page'}, 404); s.writeHead(200, {'content-type': 'image/png'}); return s.end(fs.readFileSync(f)) }
      if (w == 'save') { M.saves.push(Object.fromEntries(P)); const fmt = P.get('format'), out = path.join(dir, 'out.' + fmt); try { fs.unlinkSync(out) } catch (e) {}
        const o = (await run(['save', fmt, out, ...P.get('pages').split(',').map(k => path.join(dir, 'page-' + k + '.png'))])).toString();
        if (!/^\{"ok":true/.test(o)) return J(o); s.writeHead(200, {'content-type': fmt == 'pdf' ? 'application/pdf' : fmt == 'png' ? 'image/png' : 'image/jpeg'}); return s.end(fs.readFileSync(out)) }
      if (w == 'cancel' || w == 'drop') return J({ok: true});
      return J({ok: false, error: 'unknown scan action'}, 404) }
    if (u == '/api/devices') return J({ok: true, drives: [], usb: [], inputs: []});
    if (u == '/api/print/usb') return J({ok: true, printers: []});
    if (u == '/api/print/jobs') return J({ok: true, printers: [], jobs: []});
    if (u == '/api/print/find') return J({ok: true, printers: []});
    if (u == '/api/print/pdf') { s.writeHead(200, {'content-type': 'application/pdf'}); return s.end('%PDF-1.4\n1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj\n2 0 obj << /Type /Pages /Kids [3 0 R] /Count 1 >> endobj\n3 0 obj << /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >> endobj\ntrailer << /Root 1 0 R >>\n%%EOF\n') }
    if (u.startsWith('/api/')) return J({ok: true});
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8642, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
const check = (file, ...a) => { try { return cp.execFileSync('python3', [path.join(FX, 'scancheck.py'), file, ...a]).toString().trim() } catch (e) { return String(e.stdout || e) } };
(async () => {
  for (let i = 0; i < 50 && !fs.existsSync(path.join(T, 'ready')); i++) await wait(100);
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  const p = await b.newPage({viewport: {width: 1366, height: 900}}); const errs = []; p.on('pageerror', e => errs.push(String(e.stack || e).slice(0, 300)));
  await p.goto('http://127.0.0.1:8642/'); await p.waitForFunction(() => typeof scanApp == 'function' && typeof settings == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});USER=USER||ACCTS[0];fs['C:'].USERS=fs['C:'].USERS||{};HOME=defaultHome();fs['C:'].USERS[uname()]=HOME;return 1})()");
  ok('1. Scan is in the Start menu\'s apps (found by "scanner" too)', await q("!!APPS.scan&&APPS.scan.name=='Scan'&&/scanner/.test(APPS.scan.kw)"));
  const W = "wins.find(w=>w.app=='scan')", sel = s => `${W}.body.querySelector(${JSON.stringify(s)})`;
  const until = async (js, ms) => { for (let i = 0; i < (ms || 30000) / 200; i++) { if (await q(js)) return true; await wait(200) } return false };
  await q("(()=>{wins.slice().forEach(w=>w.close());launch('scan');return 1})()");
  await until(`${W}&&${sel('.scres')}.options.length>0`);
  const st = await q(`(()=>{const B=${W}.body;return{dev:[...B.querySelector('.scdev').options].map(o=>o.textContent),src:[...B.querySelector('.scsrc').options].map(o=>o.textContent),res:[...B.querySelector('.scres').options].map(o=>+o.value),mode:[...B.querySelector('.scmode').options].filter(o=>!o.disabled).map(o=>o.value),title:${W}.el.querySelector('.tb').textContent}})()`);
  ok('2. opened: the scanner found (scan/find), and what it can do (scan/caps): the glass and the feeder, 75-300 dpi, colour, grey, black and white', st.dev.join() == 'Office Scanner (eSCL)' && st.src.join('|') == 'Glass|Document feeder' && st.res.join() == '75,100,150,200,300' && st.mode.join() == 'color,grey,bw' && /Scan/.test(st.title), st);
  await q(`${sel('.scprev')}.click()`); await until(`${sel('.scpi')}.naturalWidth>0`);
  ok('3. Preview: the whole glass at 75 dpi in colour shown (637 × 877)', await q(`${sel('.scpi')}.naturalWidth==637&&${sel('.scpi')}.naturalHeight==877&&${sel('.scpw')}.style.display==''`), await q(`[${sel('.scpi')}.naturalWidth,${sel('.scm')}.textContent]`));
  const bx = await (await p.$(`.win .scpi`)).boundingBox();
  await p.mouse.move(bx.x + bx.width * 0.1, bx.y + bx.height * 0.1); await p.mouse.down(); await p.mouse.move(bx.x + bx.width * 0.6, bx.y + bx.height * 0.3, {steps: 5}); await p.mouse.up();
  const sv = await q(`(()=>{const s=${sel('.scsel')};return{d:s.style.display,a:${sel('.scarea')}.value,sel:${W}.scan.sel}})()`);
  ok('4. an area drawn on the preview: shown, the Area list says so', sv.d == 'block' && sv.a == 'drawn' && sv.sel.w > 90 && sv.sel.w < 120 && sv.sel.h > 50, sv);
  await q(`(()=>{${sel('.scres')}.value='150';${sel('.scres')}.dispatchEvent(new Event('change'));${sel('.scgo')}.click()})()`); await until(`${W}.scan.pages.length==1&&!${W}.scan.busy`);
  const s1 = M.starts[M.starts.length - 1], [l, t, w, h] = (s1.area || '').split(',').map(Number);
  ok('5. Scan: the drawn area asked of the scanner in millimetres from the corner, at 150 dpi in colour; the page listed', s1.res == '150' && s1.mode == 'color' && s1.source == 'flatbed' && Math.abs(l - 21.6) < 1.5 && Math.abs(t - 29.7) < 1.5 && Math.abs(w - 108) < 2 && Math.abs(h - 59.4) < 2 && await q(`${sel('.scl')}.querySelectorAll('img').length==1`), s1);
  await q(`(()=>{const a=${sel('.scarea')};a.value='whole';a.dispatchEvent(new Event('change'));const s=${sel('.scsrc')};s.value='adf';s.dispatchEvent(new Event('change'));${sel('.scres')}.value='75';${sel('.scgo')}.click()})()`);
  await until(`${W}.scan.pages.length==4&&!${W}.scan.busy`);
  const s2 = M.starts[M.starts.length - 1];
  ok('6. the document feeder: its three pages added after the first (page numbers 2, 3, 4), the whole glass', s2.source == 'adf' && !s2.area && s2.first == '2' && await q(`${W}.scan.pages.map(p=>p.k).join()=='1,2,3,4'`) && /3 pages scanned/.test(await q(`${sel('.scm')}.textContent`)), [s2, await q(`${sel('.scm')}.textContent`)]);
  await q(`${sel('.scl')}.querySelectorAll('button[title="Remove"]')[0].click()`); await q(`${sel('.scl')}.querySelectorAll('button[title="Move later"]')[0].click()`);
  ok('7. pages taken out and put in order: the first removed, the next two swapped', await q(`${W}.scan.pages.map(p=>p.k).join()=='3,2,4'&&${sel('.scn2')}.textContent=='3 pages scanned.'`), await q(`${W}.scan.pages.map(p=>p.k).join()`));
  if (process.env.SCAN_SHOT) await p.screenshot({path: process.env.SCAN_SHOT});
  await q(`${sel('.scsave')}.click()`); await until("!!document.querySelector('.fdlg .fdok')");
  const dlg = await q("(()=>{const d=document.querySelector('.fdlg');return{name:d.querySelector('.fdname').value,path:d.querySelector('.fdpath').value}})()");
  await q("(()=>{const d=document.querySelector('.fdlg');d.querySelector('.fdname').value='Three pages.pdf';d.querySelector('.fdname').dispatchEvent(new Event('input'));d.querySelector('.fdok').click()})()");
  await until(`!${W}.scan.busy&&/saved/.test(${sel('.scm')}.textContent)`);
  const pdf = await q("(()=>{const d=node(homePath('DOCUMENTS'));const k=Object.keys(d).find(k=>/^Three pages\\.pdf$/i.test(k));return k?[k,d[k].slice(0,40),d[k]]:null})()");
  let pi = '', cs = [];
  if (pdf) { const f = path.join(T, 'three.pdf'); fs.writeFileSync(f, Buffer.from(pdf[2].split(',')[1], 'base64')); pi = cp.execFileSync('pdfinfo', [f]).toString();
    cs = [[1, 3], [2, 2], [3, 4]].map(([k, pg]) => check(f, 'pdfpage=' + k, 'page=' + (pg - 1), 'dpi=75', 'tol=10')) }
  ok('8. Save as PDF: the Save dialog starts in Documents with "Scan DATE TIME.pdf"; the three pages in their order in one PDF in Documents, each page\'s picture checked', /^Scan \d{4}-\d\d-\d\d \d\d\.\d\d\.pdf$/.test(dlg.name) && /DOCUMENTS/i.test(dlg.path) && pdf && /^data:application\/pdf;base64,/.test(pdf[1]) && /Pages:\s+3/.test(pi) && cs.every(x => /^ok/.test(x)) && M.saves.at(-1).pages == '3,2,4' && M.saves.at(-1).format == 'pdf', [dlg, pdf && pdf[0], pi, cs, M.saves.at(-1)]);
  await q(`(()=>{${sel('.scfmt')}.value='png';${W}.scan.pages.splice(2,1);${sel('.scsave')}.click()})()`); await until("!!document.querySelector('.fdlg .fdok')");
  await q("(()=>{const d=document.querySelector('.fdlg');d.querySelector('.fdname').value='Pages.png';d.querySelector('.fdname').dispatchEvent(new Event('input'));d.querySelector('.fdok').click()})()");
  await until(`!${W}.scan.busy&&/saved/.test(${sel('.scm')}.textContent)`);
  const pngs = await q("(()=>{const d=node(homePath('DOCUMENTS'));return Object.keys(d).filter(k=>/^Pages \\d\\.png$/i.test(k)).sort().map(k=>[k,d[k]])})()");
  let pc = []; pngs.forEach(([k, v], i) => { const f = path.join(T, 'p' + i + '.png'); fs.writeFileSync(f, Buffer.from(v.split(',')[1], 'base64')); pc.push(check(f, 'page=' + [2, 1][i], 'dpi=75')) });
  ok('9. Save as PNG with two pages: a file for each, "Pages 1.png" and "Pages 2.png", each checked', pngs.length == 2 && pc.every(x => /^ok/.test(x)), [pngs.map(x => x[0]), pc]);
  const errsay = async st => { await ctl('set?' + st); await q(`${sel('.scgo')}.click()`); await wait(400); await until(`!${W}.scan.busy`, 40000); return q(`[${sel('.scm')}.textContent,${sel('.scm')}.getAttribute('role'),${sel('.scm')}.style.color]`) };
  const j = await errsay('adf=jam'), o = await errsay('adf=open'), e = await errsay('adf=empty'); await ctl('set?adf=loaded&busy=1'); const bz = await errsay('busy=1'); await ctl('set?busy=0');
  ok('10. said plainly, as an alert: a paper jam, the cover open, the feeder empty, busy', /^Paper is jammed in the scanner's document feeder/.test(j[0]) && /cover or document feeder is open/.test(o[0]) && /no paper in the document feeder/.test(e[0]) && /scanner is busy/.test(bz[0]) && [j, o, e, bz].every(x => x[1] == 'alert'), [j, o, e, bz]);
  await q(`${W}.close()`); await wait(300);
  ok('...closing the window drops its session (scan/drop)', M.asked.at(-1) == 'drop', M.asked.slice(-3));
  M.found = false; await q("launch('scan')"); await until(`/No scanner found/.test(${sel('.scm')}.textContent)`, 10000);
  ok('11. no scanner: said, with what to check; Preview and Scan cannot be pressed', await q(`/switched on, and on the same network/.test(${sel('.scm')}.textContent)&&${sel('.scgo')}.disabled&&${sel('.scprev')}.disabled`), await q(`${sel('.scm')}.textContent`));
  M.found = true; await q(`${W}.close()`);
  await q("(()=>{wins.slice().forEach(w=>w.close());explorer(homePath('PICTURES'));return 1})()"); await wait(500);
  const xb = await q("(()=>{const E=wins.find(w=>w.app=='explorer');const b=E&&E.body.querySelector('.xcmd .xscan');if(!b)return null;b.click();return b.previousElementSibling.className})()");
  await until(`${W}&&${sel('.scres')}.options.length>0`);
  await q(`${W}.scan.pages.push({k:1,url:'',w:1,h:1});${sel('.scsave')}.disabled=false;${sel('.scsave')}.click()`); await until("!!document.querySelector('.fdlg .fdok')");
  const xp = await q("document.querySelector('.fdlg .fdpath').value"); await q("document.querySelector('.fdlg .fdx').click()");
  ok('12. File Explorer: "📠 Scan" after Download in its commands; Scan opened from there saves into that folder (the Save dialog starts there)', xb == 'dn' && /PICTURES/i.test(xp), [xb, xp]);
  await q("(()=>{wins.slice().forEach(w=>w.close());settings('devs');return 1})()"); await wait(1200);
  const dv = await q("(()=>{const x=document.querySelector('.scn62');return x?[x.querySelector('h4').textContent,x.querySelector('button').textContent]:null})()");
  await q("document.querySelector('.scn62 button').click()"); await wait(500);
  ok('13. Settings › Devices: a Scanners section with Open Scan, which opens Scan', dv && dv[0] == 'Scanners' && dv[1] == '📠 Open Scan' && await q(`!!${W}`), dv);
  await q("(()=>{wins.slice().forEach(w=>w.close());prDialog(null,'<p>x</p>',{title:'Doc'});return 1})()"); await wait(600);
  const pd = await q("(()=>{const b=document.querySelector('.prdlg .pscan');return b?[b.textContent,b.previousElementSibling.className]:null})()");
  await q("document.querySelector('.prdlg .pscan').click()"); await wait(400);
  ok('14. the Print dialog: "📠 Scan…" beside Find printers, which opens Scan', pd && pd[0] == '📠 Scan…' && pd[1] == 'pfind' && await q(`!!${W}`), pd);
  await q("(()=>{const o=document.querySelector('.prdlg');o&&o.closest('.rmodal').remove();wins.slice().forEach(w=>w.close());return 1})()");
  const p2 = await b.newPage(); await p2.route('**/*', r => { const u = r.request().url(); if (/\/$|index\.html/.test(u)) return r.fulfill({body: fs.readFileSync(path.join(WWW, 'index.html'), 'utf8'), contentType: 'text/html'}); r.continue() });
  await p2.goto('http://127.0.0.1:8642/'); await p2.waitForFunction(() => typeof scanApp == 'function', null, {timeout: 30000});
  const pb = await p2.evaluate("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});const W=scanApp();return [W.body.querySelector('.scm').textContent,W.body.querySelector('.scgo').disabled]})()");
  ok('15. a plain browser (no rudolfd): Scan says it needs Rudolf OS on the computer', /needs Rudolf OS on this computer/.test(pb[0]) && pb[1], pb);
  ok('16. no page errors', !errs.length, errs);
  await b.close(); srv.close(); ES.kill(); fs.rmSync(T, {recursive: true, force: true});
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('FAIL ' + (e.stack || e)); ES.kill(); process.exit(1) });
