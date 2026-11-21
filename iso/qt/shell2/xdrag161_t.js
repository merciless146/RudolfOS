// 1.6.1 (docs/plan-1.6.1.md Part 22; the owner, 2026-11-20): File Explorer's selection box, files dragged between windows
// (C: and rudolfd's places: a USB drive and the Games (Emu library) folder), Cut/Copy/Paste across them, the progress window,
// moves that are checked before the originals go, and a file kept on the disk copied as a file of its own. In the desktop page
// (iso/www/index.html) with a stand-in rudolfd that keeps its drives in memory. node xdrag161_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path'), crypto = require('crypto');
const WWW = path.join(__dirname, '../../../iso/www'), PORT = +process.env.XDRAG_PORT || 8601;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 700) : '')) };
const MT = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.html': 'text/html', '.webp': 'image/webp', '.woff2': 'font/woff2', '.woff': 'font/woff'};
/* ---- the stand-in's places: path -> {dir} or {b: Buffer}, each with mtime; three stores (a move across them is not a rename) ---- */
const F = new Map(), M = {calls: [], slow: 0, failWrite: '', badHash: '', links: new Map(), tmp: new Map()};
const store = p => p.startsWith('/media/sdb1') ? 'sdb1' : p.startsWith('/home/rudolf/Games') ? 'games' : p.startsWith('/home/rudolf/Downloads') ? 'dl' : 'x';
const par = p => p.replace(/\/[^/]+$/, '');
const mkdirs = p => { while (p && p != '/' && !F.has(p)) { F.set(p, {dir: true, mtime: 1.7e12}); p = par(p) } };
const put = (p, data, mtime) => { mkdirs(par(p)); F.set(p, {b: Buffer.from(data), mtime: mtime || 1.71e12}) };
const kids = d => [...F.keys()].filter(k => par(k) == d && k != d);
const sha = b => crypto.createHash('sha256').update(b).digest('hex');
const reset = () => { F.clear(); M.tmp.clear();
  mkdirs('/media/sdb1'); mkdirs('/home/rudolf/Games/DC'); mkdirs('/home/rudolf/Downloads/.accounts');
  put('/home/rudolf/Games/DC/manual.pdf', '%PDF-1.4 made up for the test\n'.repeat(40)); put('/home/rudolf/Games/DC/track01.bin', Buffer.alloc(300000, 7));
  put('/home/rudolf/Games/DC/cover.png', 'png made up'); mkdirs('/home/rudolf/Games/DC/Extras'); put('/home/rudolf/Games/DC/Extras/readme.txt', 'extras\n');
  put('/media/sdb1/photo-1.png', Buffer.alloc(1200, 1)); put('/media/sdb1/notes.txt', 'on the stick\n'); mkdirs('/media/sdb1/Backup') };
reset();
const J = (s, o, st) => { s.writeHead(st || 200, {'content-type': 'application/json'}); s.end(JSON.stringify(Object.assign({ok: !st || st < 300}, o))) };
const E = (s, st, m) => J(s, {error: m}, st);
const later = ms => new Promise(r => setTimeout(r, ms));
async function api(u, qs, body, s) { const q = new URLSearchParams(qs || ''), p = q.get('path'), a = u.slice(5);
  if (a.startsWith('fs/')) M.calls.push(a + ' ' + (p || (q.get('from') + ' > ' + q.get('to'))));
  if (a == 'disks') return J(s, {disks: [{name: 'sdb', bytes: 16e9, removable: true, model: 'Made-up Stick', partitions: [{name: 'sdb1', bytes: 16e9, fs: 'exfat', label: 'STICK', mount: '/media/sdb1'}]}]});
  if (a == 'fs/list') { if (!F.has(p) || !F.get(p).dir) return E(s, 404, 'no such folder');
    return J(s, {items: kids(p).map(k => { const e = F.get(k); return {name: path.basename(k), dir: !!e.dir, size: e.dir ? 0 : e.b.length, mtime: e.mtime} })}) }
  if (a == 'fs/tree') { if (!F.has(p)) return E(s, 404, 'no such folder'); const items = [];
    const walk = d => kids(d).forEach(k => { const e = F.get(k); items.push({p: k.slice(p.length + 1), dir: !!e.dir, size: e.dir ? 0 : e.b.length, mtime: e.mtime}); if (e.dir) walk(k) }); walk(p); return J(s, {items}) }
  if (a == 'fs/read') { if (M.slow) await later(M.slow); const e = F.get(p); if (!e || e.dir) return E(s, 404, 'not found'); s.writeHead(200, {'content-type': 'application/octet-stream'}); return s.end(e.b) }
  if (a == 'fs/hash') { const e = F.get(p); if (!e || e.dir) return E(s, 404, 'not found'); return J(s, {sha256: M.badHash && p.includes(M.badHash) ? '0'.repeat(64) : sha(e.b), size: e.b.length}) }
  if (a == 'fs/mkdir') { if (!F.has(par(p))) return E(s, 500, 'No such file or directory'); mkdirs(p); return J(s, {message: 'created'}) }
  if (a == 'fs/delete') { const e = F.get(p); if (!e) return E(s, 404, 'not found'); if (e.dir && kids(p).length) return E(s, 500, 'Directory not empty'); F.delete(p); return J(s, {message: 'deleted'}) }
  if (a == 'fs/write') { if (M.slow) await later(M.slow); if (M.failWrite && p.includes(M.failWrite)) return E(s, 500, 'write failed (drive full?)');
    if (!F.has(par(p))) return E(s, 500, 'cannot write here (read-only drive?)');
    const part = q.get('part'), fin = q.get('final') == '1';
    if (part != null) { const off = +part, cur = M.tmp.get(p) || Buffer.alloc(0); if (off && cur.length != off) return E(s, 409, 'a piece is missing: start again');
      const nb = Buffer.concat([off ? cur : Buffer.alloc(0), body]); if (!fin) { M.tmp.set(p, nb); return J(s, {message: 'piece', size: nb.length}) } M.tmp.delete(p); put(p, nb, +q.get('mtime') || 0); return J(s, {message: 'saved', size: nb.length}) }
    put(p, body, +q.get('mtime') || 0); return J(s, {message: 'saved', size: body.length}) }
  if (a == 'fs/copy' || a == 'fs/move') { const f = q.get('from'), t = q.get('to'); if (M.slow) await later(M.slow); const e = F.get(f); if (!e) return E(s, 404, 'not found'); if (F.has(t)) return E(s, 409, 'something with that name is already there');
    if ((t + '/').startsWith(f + '/')) return E(s, 400, 'a folder cannot go inside itself');
    if (a == 'fs/copy') { if (e.dir) return E(s, 400, 'only a file is copied at a time'); if (M.failWrite && t.includes(M.failWrite)) return E(s, 500, 'the drive is full'); put(t, e.b, e.mtime); return J(s, {message: 'copied', size: e.b.length}) }
    if (store(f) != store(t)) return E(s, 409, 'another drive: copy it, then delete it');
    for (const k of [...F.keys()].filter(k => k == f || k.startsWith(f + '/')).sort((x, y) => x.length - y.length)) { F.set(t + k.slice(f.length), F.get(k)); F.delete(k) } return J(s, {message: 'moved'}) }
  if (a == 'dl/file' && q.get('to') == 'copy') { const dir = '/home/rudolf/Downloads/.accounts/' + q.get('acct') + '/', src = dir + q.get('name'); if (q.get('from') != 'home' || !F.has(src)) return E(s, 404, 'the file is not there any more');
    const as = q.get('as') || q.get('name'); let dst = dir + as; const m = /^(.*?)(\.[^.]*)?$/.exec(as); for (let i = 2; F.has(dst); i++) dst = dir + m[1] + ' (' + i + ')' + (m[2] || '');
    put(dst, F.get(src).b); M.calls.push('dl/file copy ' + src + ' > ' + dst); return J(s, {path: dst, name: path.basename(dst), size: F.get(src).b.length}) }
  if (a == 'dl/link') { if (!F.has(p)) return E(s, 404, 'the file is not there any more'); const id = crypto.randomBytes(8).toString('hex'); M.links.set(id, p); return J(s, {url: '/dlf/' + id}) }
  if (a == 'dl/home') return J(s, {items: []});
  if (a == 'events/wait') { await later(20000); return J(s, {events: []}) }
  if (a == 'desktop/session') return M.session ? J(s, M.session) : E(s, 404, 'no native session');
  return J(s, {}) }
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0); const bs = [];
  q.on('data', d => bs.push(d)); q.on('end', () => { const body = Buffer.concat(bs);
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/dlf/')) { const p = M.links.get(u.slice(5)); const e = p && F.get(p); if (!e) { s.writeHead(404); return s.end() } s.writeHead(200, {'content-type': 'application/octet-stream'}); return s.end(e.b) }
    if (u.startsWith('/api/')) return api(u, qs, body, s).catch(e => E(s, 500, e.message));
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': MT[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(PORT, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
const GAMES = "{dev:'games',root:'/home/rudolf/Games',path:'/home/rudolf/Games/DC',label:'Games'}", STICK = "{dev:'sdb1',root:'/media/sdb1',path:'/media/sdb1',label:'STICK (16.0 GB)'}";
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const ctx = await b.newContext({viewport: {width: 1400, height: 900}});
  const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => { if (!/volUpd|debian/.test(e.stack || e.message || '')) errs.push(e.message) });
  const q = s => p.evaluate(s);
  await p.goto(`http://127.0.0.1:${PORT}/`); await p.waitForFunction(() => typeof explorer == 'function' && typeof xfRun == 'function' && FSLOADED && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    ACCTS.length=0;ACCTS.push({n:'ann',h:await pwMake('a1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'a1');window.elevate=async()=>true;
    const D=node(downloadsPath());Object.keys(D).forEach(k=>delete D[k]);
    for(let i=1;i<=6;i++)D['PIC-'+i+'.TXT']='picture '+i;D['ROMS']={};D['NOTE.TXT']='a note';
    const L=node(homePath('DOCUMENTS'));Object.keys(L).forEach(k=>delete L[k]);for(let i=1;i<=60;i++)L['DOC-'+String(i).padStart(2,'0')+'.TXT']='doc '+i;
    fsSave();return 1})()`);
  /* two windows side by side, as in the photo: A on the left, B on the right */
  const two = async (a, bRemote) => { await q(`(()=>{wins.slice().forEach(w=>w.close());const A=explorer(${a});A.el.style.cssText='left:0;top:0;width:680px;height:560px';window.WA=A;
    const B=explorer(downloadsPath());B.el.style.cssText='left:700px;top:0;width:680px;height:560px';window.WB=B;${bRemote ? `B.openRemote(${bRemote});` : ''}return 1})()`); await wait(900) };
  const box = w => q(`(()=>{const r=${w}.body.querySelector('.xc').getBoundingClientRect();return {x:r.left,y:r.top,w:r.width,h:r.height}})()`);
  const rect = (w, k) => q(`(()=>{const e=${w}.body.querySelector('[data-k="'+${JSON.stringify(k)}+'"]');if(!e)return null;const r=e.getBoundingClientRect();return {x:r.left+Math.min(40,r.width/2),y:r.top+r.height/2}})()`);
  const sel = w => q(`[...${w}.body.querySelectorAll('.xc [data-k].s')].map(e=>e.dataset.k.split('/').pop())`);
  const view = async (w, v) => { await q(`(()=>{const s=${w}.body.querySelector('.vw');s.value='${v}';s.onchange({target:s});return 1})()`); await wait(250) };
  const DL = "downloadsPath().join('/')";
  await q("document.addEventListener('dragover',()=>setTimeout(()=>{if(document.querySelector('.xp25 .dt,.xp25 .xc.xdt'))window.HL=1},0),true)");
  /* ===== 1. the selection box ===== */
  await two('downloadsPath()');
  for (const v of ['list', 'icons', 'thumbs']) {
    await view('WA', v); const bx = await box('WA'); const last = await rect('WA', (await q(DL)) + '/PIC-6.TXT'), first = await rect('WA', (await q(DL)) + '/ROMS');
    const sx = bx.x + bx.w - 20, sy = bx.y + bx.h - 20;   // the bottom right: below or beside the items
    await p.mouse.move(sx, sy); await p.mouse.down(); await p.mouse.move((sx + last.x) / 2, (sy + last.y) / 2, {steps: 4});
    await p.mouse.move(Math.min(last.x, first.x) - 2, last.y - 2, {steps: 6}); const mid = await sel('WA');
    await p.mouse.move(Math.min(first.x, last.x) - 4, first.y - 4, {steps: 6}); await wait(80); const live = await sel('WA'), marq = await q("!!WA.body.querySelector('.xmarq')");
    await p.mouse.up(); const after = await sel('WA');
    ok(`1.${v}: pressed on an empty spot and dragged: a box drawn, the items it touches selected while dragging (${mid.length}, then ${live.length})`, marq && mid.length >= 1 && live.length > mid.length && live.includes('PIC-6.TXT') && live.includes('ROMS'), {mid, live, marq});
    ok(`1.${v}: ...the selection stays after the button is let go, no box left`, JSON.stringify(after) == JSON.stringify(live) && !(await q("!!WA.body.querySelector('.xmarq')")), after);
    if (v == 'list') {
      // Ctrl adds: a second box over NOTE.TXT only, with Ctrl held
      const n = await rect('WA', (await q(DL)) + '/NOTE.TXT'); await p.keyboard.down('Control'); await p.mouse.move(sx, sy); await p.mouse.down(); await p.mouse.move(n.x + 60, n.y, {steps: 8}); await p.mouse.up(); await p.keyboard.up('Control');
      const s2 = await sel('WA'); ok('1.list: Ctrl adds to the selection (the box over another item keeps the rest)', s2.length >= after.length && s2.includes('NOTE.TXT') && s2.includes('ROMS'), s2);
      // a plain click on an empty spot clears it
      await p.mouse.click(sx, sy); ok('1.list: a plain click on an empty spot clears the selection', (await sel('WA')).length == 0, await sel('WA'));
      // pressing on a row does not start a box (it starts a drag of the row)
      const r1 = await rect('WA', (await q(DL)) + '/PIC-1.TXT'); await p.mouse.move(r1.x, r1.y); await p.mouse.down(); await p.mouse.move(r1.x + 3, r1.y + 40, {steps: 3}); const nob = !(await q("!!WA.body.querySelector('.xmarq')")); await p.mouse.up();
      ok('1.list: pressed on a row: no selection box', nob);
      await p.mouse.click(sx, sy);
    }
  }
  // scrolling: the long Documents folder in a short window, the box taken to its bottom edge
  await q("(()=>{WA.close();const A=explorer(homePath('DOCUMENTS'));A.el.style.cssText='left:0;top:0;width:680px;height:330px';window.WA=A;return 1})()"); await wait(500); await view('WA', 'list');
  { const bx = await box('WA'), r1 = await rect('WA', (await q("homePath('DOCUMENTS').join('/')")) + '/DOC-01.TXT');
    await q("WA.body.querySelector('.xc').scrollTop=0");
    // the right of the list's rows is still a row: start in the empty Size column's gutter? no: the rows span the width. Start just below the header, at a row's left edge is a row too;
    // so the box starts on the list's own background by pressing with nothing under the pointer: the table is made narrow for this check
    await q("WA.body.querySelector('.xc table').style.width='60%'");
    const sx = bx.x + bx.w - 30, sy = r1.y; await p.mouse.move(sx, sy); await p.mouse.down(); await p.mouse.move(sx - 300, sy + 30, {steps: 5}); await p.mouse.move(sx - 300, bx.y + bx.h + 30, {steps: 5});
    await wait(1500); const st = await q("WA.body.querySelector('.xc').scrollTop"), n = (await sel('WA')).length; await p.mouse.up();
    ok(`1. the list scrolls when the box reaches its edge, selecting as it goes (scrolled ${st} px, ${n} selected)`, st > 100 && n > 15, {st, n}) }
  // keyboard unchanged: Ctrl+A, the arrows
  await q("WA.body.querySelector('.xc').focus()"); await p.keyboard.press('Control+a'); const all = (await sel('WA')).length; await p.keyboard.press('ArrowDown');
  ok('1. the keys as before: Ctrl+A selects all, an arrow one', all == 60 && (await sel('WA')).length == 1, all);
  // a drive's pane: the box, Shift+click ranges, Ctrl+click
  await two('downloadsPath()', GAMES);
  { const bx = await box('WB'), a = await rect('WB', 'R:Extras'), c = await rect('WB', 'R:track01.bin');
    await p.mouse.move(bx.x + bx.w - 20, bx.y + bx.h - 20); await p.mouse.down(); await p.mouse.move(c.x, c.y, {steps: 8}); const live = await sel('WB'); await p.mouse.up();
    ok('1. a drive\'s pane: the selection box too (live)', live.length >= 1 && live.includes('R:track01.bin'), live);
    await p.mouse.click(a.x, a.y); await p.keyboard.down('Shift'); await p.mouse.click(c.x, c.y); await p.keyboard.up('Shift'); const rg = await sel('WB');
    ok('1. a drive\'s pane: Shift+click selects the rows between', JSON.stringify(rg) == JSON.stringify(['R:Extras', 'R:cover.png', 'R:manual.pdf', 'R:track01.bin']), rg);
    await p.keyboard.down('Control'); await p.mouse.click(a.x, a.y); await p.keyboard.up('Control');
    ok('1. a drive\'s pane: Ctrl+click takes one away', (await sel('WB')).length == 3, await sel('WB')) }
  /* ===== 2. dragging between windows ===== */
  const drag = async (fromW, k, toW, toK, ctrl) => { const a = await rect(fromW, k); let t; if (toK) t = await rect(toW, toK); else { const bx = await box(toW); t = {x: bx.x + bx.w / 2, y: bx.y + bx.h - 25} }
    if (!a || !t) return false; await q("window.HL=0"); await p.mouse.move(a.x, a.y); await p.mouse.down(); await p.mouse.move(a.x + 20, a.y + 5, {steps: 3}); await p.mouse.move(t.x, t.y, {steps: 10});
    await wait(60); const hl = await q('!!window.HL'); if (ctrl) await p.keyboard.down('Control'); await p.mouse.move(t.x + 1, t.y + 1, {steps: 2});
    await p.mouse.up(); if (ctrl) await p.keyboard.up('Control'); return hl };
  const done = () => p.waitForFunction(() => !wins.some(w => w.xfer && !w.xfer.end), null, {timeout: 30000}).then(() => wait(400)).catch(() => 0);
  const C = s => q(`JSON.stringify(node(${s}))`).then(x => x === undefined ? undefined : JSON.parse(x));
  // C: folder -> C: folder in another window (as before)
  await q("(()=>{const D=node(homePath('DOCUMENTS'));Object.keys(D).forEach(k=>delete D[k]);D['WORK']={};fsSave();return 1})()");
  await q("(()=>{wins.slice().forEach(w=>w.close());const A=explorer(downloadsPath());A.el.style.cssText='left:0;top:0;width:680px;height:560px';window.WA=A;const B=explorer(homePath('DOCUMENTS'));B.el.style.cssText='left:700px;top:0;width:680px;height:560px';window.WB=B;return 1})()"); await wait(500);
  let hl = await drag('WA', (await q(DL)) + '/PIC-1.TXT', 'WB'); await wait(300);
  ok('2.1 C: -> another window\'s C: folder (its list): moved, the target highlighted while over it', hl && (await C("homePath('DOCUMENTS')"))['PIC-1.TXT'] == 'picture 1' && !(await C('downloadsPath()'))['PIC-1.TXT'], hl);
  hl = await drag('WA', (await q(DL)) + '/PIC-2.TXT', 'WB', (await q("homePath('DOCUMENTS').join('/')")) + '/WORK'); await wait(300);
  ok('2.2 ...onto a folder row: into that folder, the row highlighted', hl && (await C("homePath('DOCUMENTS')")).WORK['PIC-2.TXT'] == 'picture 2', hl);
  await drag('WA', (await q(DL)) + '/PIC-3.TXT', 'WB', null, true); await wait(300);
  ok('2.3 Ctrl+drag copies (the original stays)', (await C("homePath('DOCUMENTS')"))['PIC-3.TXT'] && (await C('downloadsPath()'))['PIC-3.TXT'], await C("homePath('DOCUMENTS')"));
  const before = JSON.stringify(await C('downloadsPath()')); await drag('WA', (await q(DL)) + '/NOTE.TXT', 'WA'); await wait(300);
  ok('2.4 dropped on its own folder: nothing happens', JSON.stringify(await C('downloadsPath()')) == before);
  // C: -> a drive (the Games folder): moved, checked (SHA-256), the original gone after
  await two('downloadsPath()', GAMES); M.calls = [];
  hl = await drag('WA', (await q(DL)) + '/PIC-4.TXT', 'WB'); await done();
  const gp = F.get('/home/rudolf/Games/DC/pic-4.txt');
  ok('2.5 C: -> the Games window: the file is on the drive (its name as Linux reads it), the original gone', hl && gp && gp.b.toString() == 'picture 4' && !(await C('downloadsPath()'))['PIC-4.TXT'], {hl, gp: !!gp, calls: M.calls});
  ok('2.6 ...checked before the original went: written, then its SHA-256 asked', M.calls.findIndex(c => c.startsWith('fs/write') && c.includes('pic-4.txt')) >= 0 && M.calls.some(c => c.startsWith('fs/hash') && c.includes('pic-4.txt')), M.calls);
  // a drive -> C:
  M.calls = []; hl = await drag('WB', 'R:cover.png', 'WA'); await done();
  ok('2.7 the Games window -> Downloads: in C:, gone from the drive (checked first)', hl && /^data:image\/png;base64,/.test((await C('downloadsPath()'))['COVER.PNG']) && !F.has('/home/rudolf/Games/DC/cover.png') && M.calls.some(c => c.startsWith('fs/hash')), M.calls);
  // a folder from the drive into a C: folder row
  hl = await drag('WB', 'R:Extras', 'WA', (await q(DL)) + '/ROMS'); await done();
  ok('2.8 a folder from the drive onto a C: folder row: into it, with what is in it', (await C('downloadsPath()')).ROMS.EXTRAS && (await C('downloadsPath()')).ROMS.EXTRAS['README.TXT'] == 'extras\n' && !F.has('/home/rudolf/Games/DC/Extras'), await C('downloadsPath()'));
  // Ctrl+drag from the drive copies
  await drag('WB', 'R:manual.pdf', 'WA', null, true); await done();
  ok('2.9 Ctrl+drag from the drive copies (the drive keeps it)', (await C('downloadsPath()'))['MANUAL.PDF'] && F.has('/home/rudolf/Games/DC/manual.pdf'));
  // drive -> drive: the Games folder -> the stick (another store: copied, checked, deleted), and on the same drive (a rename)
  await q(`(()=>{WA.openRemote(${STICK});return 1})()`); await wait(800); M.calls = [];
  hl = await drag('WB', 'R:track01.bin', 'WA'); await done();
  ok('2.10 the Games folder -> the stick: copied by rudolfd, checked, then deleted from Games', hl && F.has('/media/sdb1/track01.bin') && !F.has('/home/rudolf/Games/DC/track01.bin') && M.calls.some(c => c.startsWith('fs/copy')) && M.calls.filter(c => c.startsWith('fs/hash')).length >= 2, M.calls);
  M.calls = []; await drag('WA', 'R:notes.txt', 'WA', 'R:Backup'); await done();
  ok('2.11 on the same drive, onto a folder: a rename (fs/move), nothing copied', F.has('/media/sdb1/Backup/notes.txt') && !F.has('/media/sdb1/notes.txt') && M.calls.some(c => c.startsWith('fs/move')) && !M.calls.some(c => c.startsWith('fs/copy')), M.calls);
  const pfx = await q(`(()=>{const b=WA.body.querySelector('.xc');const one=(ctrl,set)=>{set();let v='none';const dt=new DataTransfer();Object.defineProperty(dt,'dropEffect',{get(){return v},set(x){v=x}});
    b.dispatchEvent(new DragEvent('dragover',{bubbles:true,cancelable:true,ctrlKey:ctrl,dataTransfer:dt}));FC.drag=FC.dragR=null;return v};
    return [one(false,()=>{FC.drag=[downloadsPath().concat('NOTE.TXT')]}),one(true,()=>{FC.drag=[downloadsPath().concat('NOTE.TXT')]}),one(false,()=>{FC.dragR={r:{path:'/usr/share',ro:true},items:[{name:'x'}]}})]})()`);
  ok('2.12 the pointer shows which: move, Ctrl copy, from a read-only folder copy', JSON.stringify(pfx) == '["move","copy","copy"]', pfx);
  /* ===== 3. the progress window ===== */
  await two('downloadsPath()', GAMES);
  await q("(()=>{const D=node(downloadsPath());D['BIG-A.BIN']='data:application/octet-stream;base64,'+btoa('A'.repeat(30000));D['BIG-B.BIN']='data:application/octet-stream;base64,'+btoa('B'.repeat(30000));D['BIG-C.BIN']='data:application/octet-stream;base64,'+btoa('C'.repeat(30000));fsSave();EXPL.forEach(f=>f());return 1})()"); await wait(400);
  M.slow = 700; await q(`(()=>{const pn=WA.body;const ks=['BIG-A.BIN','BIG-B.BIN','BIG-C.BIN'].map(n=>downloadsPath().join('/')+'/'+n);return 1})()`);
  // select the three with Ctrl+clicks, then drag one of them: all three go
  for (const n of ['BIG-A.BIN', 'BIG-B.BIN', 'BIG-C.BIN']) { const r = await rect('WA', (await q(DL)) + '/' + n); await p.keyboard.down('Control'); await p.mouse.click(r.x, r.y); await p.keyboard.up('Control') }
  ok('3.0 Ctrl+click selects several', (await sel('WA')).length == 3, await sel('WA'));
  await drag('WA', (await q(DL)) + '/BIG-B.BIN', 'WB'); await wait(1100);
  const pw = () => q("(()=>{const w=wins.find(w=>w.xfer);if(!w)return null;const g=s=>(w.body.querySelector(s)||{}).textContent;return {t:g('.xft'),n:g('.xfn'),c:g('.xfc'),b:g('.xfb'),e:g('.xfe'),bar:w.body.querySelector('.xfbar').value,title:w.el.querySelector('.tb .t').textContent,btn:g('.xfx'),open:w.el.isConnected}})()");
  let w1 = await pw(); await wait(900); const w2 = await pw();
  ok(`3.1 a window "Moving 3 items" with the file, n of N, bytes, a bar: ${w1 && [w1.t, w1.n, w1.c, w1.b].join(' | ')}`, w1 && w1.t == 'Moving 3 items' && /^BIG-[ABC]\.BIN$/i.test(w1.n) && /^[123] of 3$/.test(w1.c) && /of 87\.9 KB|of 90 KB|of 88 KB/.test(w1.b) && w1.btn == 'Cancel', w1);
  ok(`3.2 ...it moves on (${w1 && w1.c} → ${w2 && w2.c}, bar ${w1 && w1.bar} → ${w2 && w2.bar}); the time left once it can be told: "${w2 && w2.e}"`, w2 && w2.bar > w1.bar && /left/.test(w2.e || '') , [w1, w2]);
  await done(); await wait(2000);
  ok('3.3 done: the three are on the drive, gone from Downloads; the window closed by itself', ['big-a.bin', 'big-b.bin', 'big-c.bin'].every(n => F.has('/home/rudolf/Games/DC/' + n)) && !Object.keys(await C('downloadsPath()')).some(n => /^BIG-/.test(n)) && !(await pw()), await pw());
  // Cancel: the originals untouched, what was copied taken away again
  await q("(()=>{const D=node(downloadsPath());['X1','X2','X3'].forEach(n=>D[n+'.BIN']='data:application/octet-stream;base64,'+btoa(n.repeat(9000)));fsSave();EXPL.forEach(f=>f());return 1})()"); await wait(400);
  await q("(()=>{const D=downloadsPath();const pn=null;return 1})()");
  for (const n of ['X1.BIN', 'X2.BIN', 'X3.BIN']) { const r = await rect('WA', (await q(DL)) + '/' + n); await p.keyboard.down('Control'); await p.mouse.click(r.x, r.y); await p.keyboard.up('Control') }
  await drag('WA', (await q(DL)) + '/X1.BIN', 'WB'); await wait(1000);
  await q("wins.find(w=>w.xfer).body.querySelector('.xfx').click()"); await done(); const wc = await pw();
  ok('3.4 Cancel: the originals untouched, nothing left on the drive, the window stays saying so', ['X1.BIN', 'X2.BIN', 'X3.BIN'].every(async n => true) && Object.keys(await C('downloadsPath()')).filter(n => /^X\d\.BIN$/.test(n)).length == 3 && !['x1.bin', 'x2.bin', 'x3.bin'].some(n => F.has('/home/rudolf/Games/DC/' + n)) && wc && /nothing was moved/i.test(wc.t) && wc.btn == 'Close', wc);
  await q("wins.find(w=>w.xfer)&&wins.find(w=>w.xfer).close()");
  // a failure: the second file cannot be written: nothing moved, said so, details kept
  M.slow = 0; M.failWrite = 'x2.bin';
  for (const n of ['X1.BIN', 'X2.BIN', 'X3.BIN']) { const r = await rect('WA', (await q(DL)) + '/' + n); if (!(await sel('WA')).includes(n)) { await p.keyboard.down('Control'); await p.mouse.click(r.x, r.y); await p.keyboard.up('Control') } }
  await drag('WA', (await q(DL)) + '/X1.BIN', 'WB'); await done(); await wait(1700); const wf = await pw();
  ok('3.5 a failed move: the originals untouched, what was copied taken away, the window stays with what went wrong', Object.keys(await C('downloadsPath()')).filter(n => /^X\d\.BIN$/.test(n)).length == 3 && !F.has('/home/rudolf/Games/DC/x1.bin') && wf && /Not moved/.test(wf.t) && /write failed|full/.test(wf.t) && wf.open, wf);
  await q("wins.find(w=>w.xfer)&&wins.find(w=>w.xfer).close()"); M.failWrite = '';
  // a changed copy (SHA-256 differs): not moved
  M.badHash = 'x1.bin'; await drag('WA', (await q(DL)) + '/X1.BIN', 'WB'); await done();
  ok('3.6 a copy that arrived changed (SHA-256): not moved, the original kept', (await C('downloadsPath()'))['X1.BIN'] && !F.has('/home/rudolf/Games/DC/x1.bin') && /changed/.test((await pw() || {}).t || ''), await pw());
  await q("wins.filter(w=>w.xfer).forEach(w=>w.close())"); M.badHash = '';
  // a read-only place takes nothing (an app's folder: the address bar's /usr/...)
  await q("(()=>{WB.openRemote({dev:'path',root:'/home/rudolf/Games/DC',path:'/home/rudolf/Games/DC',label:'DC',ro:true});return 1})()"); await wait(700);
  const roOver = await q(`(()=>{FC.drag=[downloadsPath().concat('X2.BIN')];const b=WB.body.querySelector('.xc');const dt=new DataTransfer();const ev=new DragEvent('dragover',{bubbles:true,cancelable:true,dataTransfer:dt});b.dispatchEvent(ev);
    const dv=new DragEvent('drop',{bubbles:true,cancelable:true,dataTransfer:dt});b.dispatchEvent(dv);return {eff:dt.dropEffect,hl:b.classList.contains('xdt')}})()`); await wait(500);
  ok('3.7 a read-only folder is not a drop target (no highlight, the pointer says no), nothing is written', roOver.eff == 'none' && !roOver.hl && !F.has('/home/rudolf/Games/DC/x2.bin') && (await C('downloadsPath()'))['X2.BIN'], roOver);
  const roMenu = await q("(()=>{const r=WB.body.querySelector('tr[data-k=\"R:manual.pdf\"]');r.click();r.dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,clientX:900,clientY:200}));const m=document.querySelector('.ctx');const it=m?[...m.querySelectorAll('.cmi')].map(x=>[x.querySelector('span').textContent,!x.classList.contains('dis')]):[];document.querySelectorAll('.ctx').forEach(x=>x.remove());return it})()");
  ok('3.8 ...its menu: Copy and Copy into Rudolf OS, no Cut, Paste or Delete', roMenu.some(x => x[0] == 'Copy' && x[1]) && !roMenu.some(x => /^(Cut|Paste|Delete)$/.test(x[0]) && x[1]), roMenu);
  /* ===== 4. Cut / Copy / Paste across windows, from the menus ===== */
  await two('downloadsPath()', GAMES);
  const menu = async (w, k, label, bg) => q(`(()=>{const b=${w}.body;${k ? `const r=b.querySelector('[data-k="'+${JSON.stringify(k)}+'"]');if(!r.classList.contains('s'))r.click();r.dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,clientX:300,clientY:300}));` : `b.querySelector('.xc').dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,clientX:300,clientY:500}));`}
    const it=[...document.querySelectorAll('.ctx .cmi')].find(x=>x.querySelector('span').textContent==${JSON.stringify(label)});if(!it||it.classList.contains('dis')){document.querySelectorAll('.ctx').forEach(x=>x.remove());return false}it.click();return true})()`);
  let m1 = await menu('WB', 'R:manual.pdf', 'Cut'); await wait(500); let m2 = await menu('WA', null, 'Paste'); await done();
  ok('4.1 a drive\'s right-click Cut, Paste in Downloads (another window): moved', m1 && m2 && (await C('downloadsPath()'))['MANUAL - COPY.PDF'] && !F.has('/home/rudolf/Games/DC/manual.pdf'), {m1, m2, d: Object.keys(await C('downloadsPath()'))});
  m1 = await menu('WA', (await q(DL)) + '/X2.BIN', 'Copy'); await wait(500); m2 = await menu('WB', null, 'Paste'); await done();
  ok('4.2 C:\'s Copy, Paste on the drive (the menu of its list): copied, the original stays', m1 && m2 && F.has('/home/rudolf/Games/DC/x2.bin') && (await C('downloadsPath()'))['X2.BIN'], {m1, m2});
  m1 = await menu('WA', (await q(DL)) + '/X3.BIN', 'Cut'); await wait(500); m2 = await menu('WB', null, 'Paste'); await done();
  ok('4.3 C:\'s Cut, Paste on the drive: moved', m1 && m2 && F.has('/home/rudolf/Games/DC/x3.bin') && !(await C('downloadsPath()'))['X3.BIN'], {m1, m2});
  const props = await q("(()=>{const r=WB.body.querySelector('tr[data-k=\"R:x3.bin\"]');r.click();r.dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,clientX:900,clientY:200}));const it=[...document.querySelectorAll('.ctx .cmi')].find(x=>x.querySelector('span').textContent=='Properties');it.click();const w=wins.at(-1);return w.body.textContent})()");
  ok('4.4 a drive\'s file: Properties', /x3\.bin/.test(props) && /Games/.test(props) && /18 KB|17\.6 KB|18\.0 KB/.test(props), props);
  await q("wins.at(-1).close()");
  const cprops = await q("(()=>{fileProps(downloadsPath().concat('X2.BIN'));const w=wins.at(-1);const t=w.body.textContent;w.close();return t})()");
  ok('4.4b C:\'s Properties opens with its name, size and buttons (it stopped at its first line before)', /X2\.BIN/.test(cprops) && /Location/.test(cprops) && /Close/.test(cprops), cprops);
  m1 = await menu('WB', 'R:x2.bin', 'Delete'); await wait(200); await q("(()=>{const d=WB.body.querySelector('.dlg');[...d.querySelectorAll('button')].find(b=>b.textContent=='Delete').click();return 1})()"); await done();
  ok('4.5 a drive\'s Delete: asked first, then gone', m1 && !F.has('/home/rudolf/Games/DC/x2.bin'));
  /* ===== 5. a file kept on the disk (a large download): copied as a file of its own ===== */
  const acct = await q('dlAcct()'); put(`/home/rudolf/Downloads/.accounts/${acct}/BIG.ISO`, Buffer.alloc(700000, 9));
  await q(`(()=>{const D=node(downloadsPath());D['BIG.ISO']=dlMkRef('/home/rudolf/Downloads/.accounts/${acct}/BIG.ISO',700000,'part');fsSave();EXPL.forEach(f=>f());return 1})()`); await wait(400);
  await q("(()=>{wins.slice().forEach(w=>w.close());const A=explorer(downloadsPath());A.el.style.cssText='left:0;top:0;width:680px;height:560px';window.WA=A;return 1})()"); await wait(400);
  m1 = await menu('WA', (await q(DL)) + '/BIG.ISO', 'Copy'); m2 = await menu('WA', null, 'Paste'); await wait(800);
  const cp = await q("(()=>{const v=node(downloadsPath())['BIG - COPY.ISO'];return {v,info:v&&dlRefInfo(v),type:v&&fType('BIG - COPY.ISO',v),size:v&&sizeOf(v)}})()");
  ok('5.1 a 700 KB file kept on the disk, copied and pasted: a reference to a file of its own on the disk, "on the disk", its size', cp.info && cp.info.p != `/home/rudolf/Downloads/.accounts/${acct}/BIG.ISO` && F.has(cp.info.p) && /on the disk/.test(cp.type) && cp.size == 700000, cp);
  ok('5.2 ...copied by rudolfd (dl/file to=copy) and tracked (dlRefsAdd)', M.calls.some(c => c.startsWith('dl/file copy')) && !!(await q(`ld(DLRKA(),{})[${JSON.stringify(cp.info && cp.info.p)}]`)), M.calls.slice(-3));
  const mv = await q("(async()=>{const D=node(downloadsPath());D.KEEP={};fsSave();return 1})()"); await wait(200); await q("EXPL.forEach(f=>f())"); await wait(300);
  m1 = await menu('WA', (await q(DL)) + '/BIG - COPY.ISO', 'Cut'); m2 = await q("(()=>{const d=downloadsPath().concat('KEEP');WA.openPath(d);return 1})()"); await wait(300); m2 = await menu('WA', null, 'Paste'); await wait(500);
  const mvd = await q("(()=>{const v=(node(downloadsPath().concat('KEEP'))||{})['BIG - COPY.ISO'];return v&&isDlRef(v)?dlRefInfo(v).p:null})()");
  ok('5.3 moved (Cut, Paste): still a reference to the same file on the disk', mvd == (cp.info && cp.info.p), mvd);
  // another account's or the shared folder's file kept on the disk is refused, said so
  await q("(()=>{const D=node(downloadsPath());D['OLD.ISO']=dlMkRef('/home/rudolf/Downloads/.accounts/shared/OLD.ISO',5e6,'part');fsSave();WA.openPath(downloadsPath());return 1})()"); await wait(300);
  m1 = await menu('WA', (await q(DL)) + '/OLD.ISO', 'Copy'); m2 = await menu('WA', null, 'Paste'); await wait(600);
  const st = await q("(()=>{const w=wins.find(w=>w.xfer);return w?w.body.textContent:''})()"); await q("wins.filter(w=>w.xfer).forEach(w=>w.close())");
  ok('5.4 one Rudolf OS cannot copy (the shared folder) is refused, said so; no reference copied', !(await C('downloadsPath()'))['OLD - COPY.ISO'] && /kept on the disk and could not be copied/.test(st), st);
  // copies made before 1.6.1 (two references to one file) are made files of their own, not deleted
  await q(`(()=>{const D=node(downloadsPath());D['BIG - COPY 2.ISO']=D['BIG.ISO'];fsSave();return 1})()`);
  await q("(async()=>{await dlScope({dir:DLADIR(),key:DLRKA(),place:downloadsPath(),acct:dlAcct()});return 1})()");
  const fx = await q("(()=>{const D=node(downloadsPath());const a=D['BIG.ISO'],b=D['BIG - COPY 2.ISO'];return {a:a&&dlRefInfo(a).p,b:b&&dlRefInfo(b).p}})()");
  ok('5.5 an old copy that named the same file: kept, now a file of its own on the disk (dlScope)', fx.a && fx.b && fx.a != fx.b && F.has(fx.b), fx);
  /* ===== 6. the native desktop: each window a page of its own (oneapp59.js), as far as Chromium shows it: two #app=explorer pages,
     the drag's data carried from one page's dragstart to the other's drop (text/x-rudolf-paths, text/x-rudolf-remote), the
     clipboard shared, the progress window in the page that does the work ===== */
  reset(); M.calls = [];
  const c2 = await b.newContext({viewport: {width: 900, height: 640}});
  let pg = await c2.newPage(); pg.on('pageerror', e => { if (!/volUpd|debian/.test(e.stack || e.message || '')) errs.push('native: ' + e.message) });
  await pg.goto(`http://127.0.0.1:${PORT}/`); await pg.waitForFunction(() => typeof oaOpen == 'function' && typeof acctNew == 'function' && FSLOADED, null, {timeout: 30000});
  const acct2 = await pg.evaluate(`(async()=>{svj('bd95setup',1);['oobe'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    const a=await acctNew('Ann','pw-one','admin');ACCTS=[a];window.__sys=1;saveUser();USER=a;await userLogin(a,'pw-one');
    const D=node(downloadsPath());Object.keys(D).forEach(k=>delete D[k]);D['GAME-NOTES.TXT']='notes for the game';D['MAP.TXT']='a map';D['KEEP']={};fsSave();fsFlush();await new Promise(r=>setTimeout(r,800));
    const hex=u=>[...new Uint8Array(u)].map(x=>x.toString(16).padStart(2,'0')).join('');return {id:a.id,pk:hex(await pbkBits('pw-one',a.ks,256)),fk:hex(FKRAW)}})()`);
  await pg.close(); M.session = {ok: true, id: acct2.id, n: 'Ann', r: 'admin', pk: acct2.pk, fk: acct2.fk};
  const oa = async () => { const x = await c2.newPage(); x.on('pageerror', e => { if (!/volUpd|debian/.test(e.stack || e.message || '')) errs.push('native: ' + e.message) });
    await x.addInitScript("window.webkit={messageHandlers:{rudolfNative:{postMessage:()=>{}}}};"); await x.goto(`http://127.0.0.1:${PORT}/#app=explorer`);
    await x.waitForSelector('.win.oamain', {timeout: 30000}); await wait(800); return x };
  const A = await oa(), B = await oa();
  await A.evaluate("(()=>{OA.main.openPath(downloadsPath());return 1})()"); await B.evaluate(`(()=>{OA.main.openRemote(${GAMES});return 1})()`); await wait(1200);
  /* a drag: dragstart on a row in one page (its handlers fill the DataTransfer), the same types and data given to the other page's
     dragenter, dragover and drop, as the native shell's WebKit hands a drag from another window */
  const startIn = (pgx, k) => pgx.evaluate(`(()=>{const r=OA.main.body.querySelector('[data-k="'+${JSON.stringify(k)}+'"]');if(!r)return null;const dt=new DataTransfer();r.dispatchEvent(new DragEvent('dragstart',{bubbles:true,cancelable:true,dataTransfer:dt}));
    const o={};[...dt.types].forEach(t=>o[t]=dt.getData(t));r.dispatchEvent(new DragEvent('dragend',{bubbles:true,dataTransfer:dt}));return o})()`);
  const dropIn = (pgx, data, onK, ctrl) => pgx.evaluate(`(async()=>{const d=${JSON.stringify(data)};const box=OA.main.body.querySelector('.xc');const tg=${onK ? `OA.main.body.querySelector('[data-k="'+${JSON.stringify(onK)}+'"]')` : 'box'};
    const mk=()=>{const dt=new DataTransfer();for(const t in d)dt.setData(t,d[t]);try{dt.effectAllowed='copyMove'}catch(e){}return dt};const r=tg.getBoundingClientRect(),xy={clientX:r.left+20,clientY:r.top+(tg===box?r.height-20:r.height/2),ctrlKey:${!!ctrl}};
    tg.dispatchEvent(new DragEvent('dragenter',Object.assign({bubbles:true,cancelable:true,dataTransfer:mk()},xy)));const ov=mk();{let v='none';Object.defineProperty(ov,'dropEffect',{get(){return v},set(x){v=x}})}/* (a DataTransfer made by a script keeps no drop effect: what the handlers set is read here) */const e2=new DragEvent('dragover',Object.assign({bubbles:true,cancelable:true,dataTransfer:ov},xy));tg.dispatchEvent(e2);
    const hl=!!OA.main.body.querySelector('.xc.xdt,.dt'),eff=ov.dropEffect;tg.dispatchEvent(new DragEvent('drop',Object.assign({bubbles:true,cancelable:true,dataTransfer:mk()},xy)));return {taken:e2.defaultPrevented,eff,hl}})()`);
  const waitXf = pgx => pgx.waitForFunction(() => !wins.some(w => w.xfer && !w.xfer.end), null, {timeout: 30000}).then(() => wait(600)).catch(() => 0);
  const dl = A.evaluate.bind(A, "downloadsPath().join('/')"); const DLp = await dl();
  let d = await startIn(A, DLp + '/GAME-NOTES.TXT');
  ok('6.1 a row dragged in one window\'s page: its paths go with the drag (text/x-rudolf-paths)', d && JSON.parse(d['text/x-rudolf-paths'] || '[]').length == 1, d);
  let r = await dropIn(B, d); await waitXf(B); await wait(1500);
  ok('6.2 ...dropped in the Games window\'s page: taken (the pointer: move, the list highlighted), on the drive', r.taken && r.eff == 'move' && r.hl && F.has('/home/rudolf/Games/DC/game-notes.txt'), {r, f: [...F.keys()].filter(k => k.includes('Games'))});
  const gone = await A.evaluate("!node(downloadsPath())['GAME-NOTES.TXT']");
  ok('6.3 ...and gone from Downloads in the first window too (the other page saved; this one read the files again)', gone, await A.evaluate("Object.keys(node(downloadsPath()))"));
  d = await startIn(B, 'R:cover.png');
  ok('6.4 a drive\'s row dragged: the place and the items go with it (text/x-rudolf-remote)', d && /cover\.png/.test(d['text/x-rudolf-remote'] || '') && JSON.parse(d['text/x-rudolf-remote']).r.path == '/home/rudolf/Games/DC', d);
  r = await dropIn(A, d, DLp + '/KEEP'); await waitXf(A); await wait(1200);
  const inA = await A.evaluate("(node(downloadsPath()).KEEP||{})['COVER.PNG']||null"), rows = await B.evaluate("[...OA.main.body.querySelectorAll('tr[data-k]')].map(t=>t.dataset.k)");
  ok('6.5 ...dropped on a folder in the Downloads page: in it, gone from the drive, the Games page shows it gone (told by the other page)', r.taken && inA && !F.has('/home/rudolf/Games/DC/cover.png') && !rows.includes('R:cover.png'), {r, inA: !!inA, rows});
  // Ctrl: copy
  d = await startIn(B, 'R:manual.pdf'); r = await dropIn(A, d, null, true); await waitXf(A);
  ok('6.6 Ctrl+drop between the pages copies (the drive keeps it)', r.eff == 'copy' && F.has('/home/rudolf/Games/DC/manual.pdf') && await A.evaluate("!!node(downloadsPath())['MANUAL.PDF']"), r);
  // the clipboard between the pages: Cut in one, Paste in the other
  await A.evaluate(`(()=>{const r=OA.main.body.querySelector('[data-k="'+downloadsPath().join('/')+'/MAP.TXT"]');r.click();OA.main.body.querySelector('.cu').click();return 1})()`);
  await B.evaluate("(()=>{OA.main.body.querySelector('.ps').click();return 1})()"); await waitXf(B); await wait(1500);
  ok('6.7 Cut in one window\'s page, Paste in another\'s: moved', F.has('/home/rudolf/Games/DC/map.txt') && await A.evaluate("!node(downloadsPath())['MAP.TXT']"), [...F.keys()].filter(k => k.includes('Games')));
  // the progress window, in the page that does the work
  M.slow = 600; await A.evaluate("(()=>{const D=node(downloadsPath());D['A1.BIN']='data:application/octet-stream;base64,'+btoa('a'.repeat(20000));D['A2.BIN']='data:application/octet-stream;base64,'+btoa('b'.repeat(20000));fsSave();EXPL.forEach(f=>f());return 1})()"); await wait(800);
  await A.evaluate(`(()=>{const b=OA.main.body;['A1.BIN','A2.BIN'].forEach(n=>b.querySelector('[data-k="'+downloadsPath().join('/')+'/'+n+'"]').dispatchEvent(new MouseEvent('click',{bubbles:true,ctrlKey:true})));return 1})()`);
  d = await startIn(A, DLp + '/A1.BIN'); r = await dropIn(B, d); await wait(700);
  const pwN = await B.evaluate("(()=>{const w=wins.find(w=>w.xfer);if(!w)return null;const r=w.el.getBoundingClientRect();return {t:w.body.querySelector('.xft').textContent,c:w.body.querySelector('.xfc').textContent,vis:r.width>100&&r.height>100&&getComputedStyle(w.el).display!='none',inside:r.right<=innerWidth+1&&r.bottom<=innerHeight+1}})()");
  ok('6.8 the progress window in the page doing the work: "Moving 2 items", n of N, shown inside its window', pwN && pwN.t == 'Moving 2 items' && /of 2$/.test(pwN.c) && pwN.vis && pwN.inside, pwN);
  await waitXf(B); M.slow = 0;
  ok('6.9 ...both arrived', F.has('/home/rudolf/Games/DC/a1.bin') && F.has('/home/rudolf/Games/DC/a2.bin'));
  await c2.close();
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
