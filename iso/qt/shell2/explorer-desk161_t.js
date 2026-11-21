// 1.6.1 (docs/plan-1.6.1.md Part 28; the owner, 2026-11-21): File Explorer's items dragged onto the desktop and pasted there, in the desktop
// page (iso/www/index.html) with xdrag161_t.js's stand-in rudolfd (its drives in memory): onto the empty desktop (moved from C:, copied
// with Ctrl, a drive's file moved after it is checked, a read-only place's copied), the icons where they were dropped, onto a desktop
// folder and the Trash; Cut and Copy in File Explorer, Paste on the desktop (shown only with something cut or copied), Ctrl+V, Paste
// into folder, names that clash; Cut and Copy on the desktop, Paste in File Explorer; the native desktop's pages (the shell's drop done by
// the page the drag came from, the places given to the shell, the shell's Paste done by do-desk). node explorer-desk161_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path'), crypto = require('crypto');
const WWW = path.join(__dirname, '../../../iso/www'), PORT = +process.env.XDESK_PORT || 8640;
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
  if (a == 'desktop/apps' && body.length) { try { M.cat = JSON.parse(body.toString()) } catch (e) {} return J(s, {}) }
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
  await p.goto(`http://127.0.0.1:${PORT}/`); await p.waitForFunction(() => typeof explorer == 'function' && typeof dkPaste == 'function' && FSLOADED && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    ACCTS.length=0;ACCTS.push({n:'ann',h:await pwMake('a1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'a1');window.elevate=async()=>true;
    const D=node(downloadsPath());Object.keys(D).forEach(k=>delete D[k]);for(let i=1;i<=9;i++)D['PIC-'+i+'.TXT']='picture '+i;D['ROMS']={'A.TXT':'a'};D['NOTE.TXT']='a note';
    const K=node(homePath('DESKTOP'));Object.keys(K).forEach(k=>delete K[k]);K['BOX']={};fsSave();EXPL.forEach(f=>f());return 1})()`);
  const wait = ms => new Promise(r => setTimeout(r, ms));
  const C = s => q(`JSON.stringify(node(${s}))`).then(x => x === undefined ? undefined : JSON.parse(x));
  const DK = () => C("homePath('DESKTOP')"), DLd = () => C('downloadsPath()');
  const DL = await q("downloadsPath().join('/')");
  const one = async (remote) => { await q(`(()=>{wins.slice().forEach(w=>w.close());const A=explorer(downloadsPath());A.el.style.cssText='left:760px;top:0;width:640px;height:560px';window.WA=A;${remote ? `A.openRemote(${remote});` : ''}return 1})()`); await wait(900) };
  const rect = (k) => q(`(()=>{const e=WA.body.querySelector('[data-k="'+${JSON.stringify(k)}+'"]');if(!e)return null;const r=e.getBoundingClientRect();return {x:r.left+Math.min(40,r.width/2),y:r.top+r.height/2}})()`);
  const icon = n => q(`(()=>{const e=[...DESK.querySelectorAll(':scope > .ic')].find(x=>(x.dataset.fn||x.querySelector('span').textContent)==${JSON.stringify(n)});if(!e)return null;const r=e.getBoundingClientRect();return {x:r.left+r.width/2,y:r.top+r.height/3,l:r.left,t:r.top,w:r.width,h:r.height,cell:cells[e.dataset.n]||null}})()`);
  const cellAt = (x, y) => q(`(()=>{const g=gridInfo(),r=DESK.getBoundingClientRect();return [Math.floor((${x}-r.left-8)/g.w),Math.floor((${y}-r.top-8)/g.h)]})()`);
  const saved = n => q(`(()=>{try{return JSON.parse(localStorage.getItem(typeof cellKey=='function'?cellKey():'bd95cells')||'{}')['file:'+${JSON.stringify(n)}]||null}catch(e){return null}})()`);
  const done = () => p.waitForFunction(() => !wins.some(w => w.xfer && !w.xfer.end), null, {timeout: 30000}).then(() => wait(500)).catch(() => 0);
  const drag = async (k, t, ctrl) => { const a = await rect(k); if (!a) return false; await p.mouse.move(a.x, a.y); await p.mouse.down(); await p.mouse.move(a.x + 20, a.y + 5, {steps: 3}); await p.mouse.move(t.x, t.y, {steps: 10});
    await wait(60); if (ctrl) await p.keyboard.down('Control'); await p.mouse.move(t.x + 1, t.y + 1, {steps: 2}); await p.mouse.up(); if (ctrl) await p.keyboard.up('Control'); await wait(300); return true };
  const ctrlSel = async ks => { for (const k of ks) { const r = await rect(k); await p.keyboard.down('Control'); await p.mouse.click(r.x, r.y); await p.keyboard.up('Control') } };
  const freePt = (n) => q(`(()=>{const g=gridInfo(),r=DESK.getBoundingClientRect(),out=[];for(let c=g.nc-1;c>=0;c--)for(let w=0;w<g.nr;w++){const x=r.left+8+c*g.w+g.w/2,y=r.top+8+w*g.h+g.h/2;
    if(x>740||y>r.bottom-20)continue;const e=document.elementFromPoint(x,y);if(e===DESK&&document.elementFromPoint(x-g.w/2+6,y-g.h/2+6)===DESK&&document.elementFromPoint(x+g.w/2-6,y+g.h/2-6)===DESK)out.push({x:Math.round(x),y:Math.round(y)})}return out.length?out[(${n || 0})%out.length]:null})()`);
  const adj = (a, b) => a && b && Math.max(Math.abs(a[0] - b[0]), Math.abs(a[1] - b[1])) == 1;
  /* the menus: the desktop's (right-click on its empty area), an icon's, File Explorer's */
  const menuAt = (x, y, target) => q(`(()=>{const t=${target || 'document.elementFromPoint(' + x + ',' + y + ')'};t.dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,cancelable:true,clientX:${x},clientY:${y},button:2}));
    const it=[...document.querySelectorAll('.ctx .cmi')].map(x=>[x.querySelector('span').textContent,!x.classList.contains('dis')]);return it})()`);
  const pick = label => q(`(()=>{const it=[...document.querySelectorAll('.ctx .cmi')].find(x=>x.querySelector('span').textContent==${JSON.stringify(label)});const ok=!!it&&!it.classList.contains('dis');if(ok)it.click();document.querySelectorAll('.ctx').forEach(x=>x.remove());return ok})()`);
  const closeMenus = () => q("document.querySelectorAll('.ctx').forEach(x=>x.remove())");
  const exMenu = async (ks, label) => { await q(`(()=>{WA.body.querySelectorAll('.xc [data-k].s').forEach(x=>x.classList.remove('s'));return 1})()`); await p.mouse.click(5, 5);
    const r0 = await rect(ks[0]); await p.mouse.click(r0.x, r0.y); for (const k of ks.slice(1)) { const r = await rect(k); await p.keyboard.down('Control'); await p.mouse.click(r.x, r.y); await p.keyboard.up('Control') }
    const r = await rect(ks[0]); await p.mouse.click(r.x, r.y, {button: 'right'}); await wait(100); return pick(label) };
  /* ===== 1. dragging onto the desktop's empty area ===== */
  await one();
  let pt = await freePt(0); await drag(DL + '/PIC-1.TXT', pt); await wait(300);
  let ic = await icon('PIC-1.TXT'), want = await cellAt(pt.x, pt.y);
  ok('1.1 a file dragged from Downloads onto the empty desktop: moved into the Desktop folder (gone from Downloads)', (await DK())['PIC-1.TXT'] == 'picture 1' && !(await DLd())['PIC-1.TXT'], await DK());
  ok(`1.2 ...its icon in the cell under the pointer (${want}), kept as icons' places are`, ic && JSON.stringify(ic.cell) == JSON.stringify(want) && JSON.stringify(await saved('PIC-1.TXT')) == JSON.stringify(want) && pt.x >= ic.l - 10 && pt.x <= ic.l + ic.w + 10, {ic, want});
  // several, chosen with Ctrl+click, then the selection box
  await ctrlSel([DL + '/PIC-2.TXT', DL + '/PIC-3.TXT', DL + '/PIC-4.TXT']); pt = await freePt(12); await drag(DL + '/PIC-3.TXT', pt); await wait(300);
  const c2 = await icon('PIC-2.TXT'), c3 = await icon('PIC-3.TXT'), c4 = await icon('PIC-4.TXT'); want = await cellAt(pt.x, pt.y);
  ok('1.3 three chosen with Ctrl+click and dragged: all three on the desktop, gone from Downloads', ['PIC-2.TXT', 'PIC-3.TXT', 'PIC-4.TXT'].every(async n => true) && c2 && c3 && c4 && !['PIC-2.TXT', 'PIC-3.TXT', 'PIC-4.TXT'].some(n => n in {}) && !Object.keys(await DLd()).some(n => /^PIC-[234]/.test(n)), Object.keys(await DK()));
  ok(`1.4 ...the first at the drop point (${want}), the others next to it`, c2 && JSON.stringify(c2.cell) == JSON.stringify(want) && adj(c2.cell, c3.cell) && (adj(c2.cell, c4.cell) || adj(c3.cell, c4.cell)) && new Set([c2.cell + '', c3.cell + '', c4.cell + '']).size == 3, [c2, c3, c4].map(x => x && x.cell));
  { const bx = await q("(()=>{const r=WA.body.querySelector('.xc').getBoundingClientRect();return {x:r.left,y:r.top,w:r.width,h:r.height}})()"), a = await rect(DL + '/PIC-5.TXT'), z = await rect(DL + '/PIC-6.TXT');
    await q("WA.body.querySelector('.xc table')&&(WA.body.querySelector('.xc table').style.width='60%')");
    await p.mouse.move(bx.x + bx.w - 20, z.y + 4); await p.mouse.down(); await p.mouse.move(a.x, a.y - 4, {steps: 8}); await p.mouse.up();
    const s = await q("[...WA.body.querySelectorAll('.xc [data-k].s')].map(e=>e.dataset.k.split('/').pop())");
    pt = await freePt(24); await drag(DL + '/PIC-5.TXT', pt);
    ok(`1.5 chosen with the selection box (${s}) and dragged: both on the desktop, next to each other`, s.includes('PIC-5.TXT') && s.includes('PIC-6.TXT') && (await DK())['PIC-6.TXT'] && (await DK())['PIC-5.TXT'] && adj((await icon('PIC-5.TXT')).cell, (await icon('PIC-6.TXT')).cell), {s, k: Object.keys(await DK())}) }
  // Ctrl: a copy; again: the name clashes as File Explorer names it
  const npt = await freePt(30); await drag(DL + '/NOTE.TXT', npt, true);
  ok('1.6 Ctrl+drag copies (Downloads keeps it)', (await DK())['NOTE.TXT'] == 'a note' && (await DLd())['NOTE.TXT'] == 'a note', await DK());
  await drag(DL + '/NOTE.TXT', npt, true); ic = await icon('NOTE - COPY.TXT');
  ok('1.7 ...again: a name that clashes is named as File Explorer names it (NOTE - COPY.TXT), its icon beside the other', (await DK())['NOTE - COPY.TXT'] == 'a note' && ic && adj(ic.cell, (await icon('NOTE.TXT')).cell), {k: Object.keys(await DK()), ic});
  // a folder onto a desktop folder's icon: into it
  const bxi = await icon('BOX'); await drag(DL + '/ROMS', bxi);
  ok('1.8 a folder dropped on a desktop folder\'s icon: into that folder, with what is in it', (await DK()).BOX.ROMS && (await DK()).BOX.ROMS['A.TXT'] == 'a' && !(await DLd()).ROMS && !(await DK()).ROMS, await DK());
  // the Trash
  const tri = await icon('Trash'); await drag(DL + '/PIC-7.TXT', tri);
  ok('1.9 a file dropped on the Trash: in the Trash, gone from Downloads, not on the desktop', !(await DLd())['PIC-7.TXT'] && !(await DK())['PIC-7.TXT'] && await q("TR.some(t=>t.n=='PIC-7.TXT')"), await q('TR.map(t=>t.n)'));
  // a system item cannot be moved: nothing moves
  await q("(()=>{WA.openPath(['C:']);return 1})()"); await wait(500);
  const sysK = await q("(()=>{const e=[...WA.body.querySelectorAll('.xc [data-k]')].find(e=>{const p=e.dataset.k.split('/');return p.length==2&&isProt(p)});return e&&e.dataset.k})()");
  if (sysK) { const nm = sysK.split('/').pop(); await drag(sysK, await freePt(36)); await wait(300);
    ok(`1.10 a system item (C:\\${nm}) dragged: not moved ("System items cannot be moved.")`, !(await DK())[nm] && (await C("['C:']"))[nm] !== undefined && (await q("DKX.said")) == 'System items cannot be moved.', {nm, k: Object.keys(await DK())}) }
  else ok('1.10 a system item to drag', false, 'none in C:');
  /* ===== 2. a drive, the Games folder, a read-only place ===== */
  await one(STICK); M.calls = [];
  pt = await freePt(42); await drag('R:notes.txt', pt); await done();
  ic = await icon('NOTES.TXT'); want = await cellAt(pt.x, pt.y);
  ok('2.1 a USB drive\'s file dragged onto the desktop: in the Desktop folder, moved after it was checked (fs/hash), gone from the drive', (await DK())['NOTES.TXT'] == 'on the stick\n' && !F.has('/media/sdb1/notes.txt') && M.calls.some(c => c.startsWith('fs/hash')), {k: Object.keys(await DK()), calls: M.calls});
  ok(`2.2 ...its icon at the drop point (${want})`, ic && JSON.stringify(ic.cell) == JSON.stringify(want), ic);
  await drag('R:photo-1.png', pt, true); await done();
  ok('2.3 Ctrl+drag from the drive: copied (the drive keeps it), its icon next to the one there', /^data:image\/png/.test((await DK())['PHOTO-1.PNG'] || '') && F.has('/media/sdb1/photo-1.png') && adj((await icon('PHOTO-1.PNG')).cell, (await icon('NOTES.TXT')).cell), Object.keys(await DK()));
  await q(`(()=>{WA.openRemote({dev:'path',root:'/home/rudolf/Games/DC',path:'/home/rudolf/Games/DC',label:'DC',ro:true});return 1})()`); await wait(800);
  await drag('R:cover.png', await freePt(50)); await done();
  const roMsg = await q("(()=>{const w=wins.find(w=>w.xfer);return w?w.body.textContent:''})()");
  ok('2.4 from a read-only place: copied, not moved (the place keeps it), as File Explorer copies from it', (await DK())['COVER.PNG'] && F.has('/home/rudolf/Games/DC/cover.png') && /Copied COVER\.PNG into Desktop/.test(roMsg), roMsg);
  await q("wins.filter(w=>w.xfer).forEach(w=>w.close())");
  // the Desktop folder refused (Explorer's own check): nothing written, Explorer's message
  await q("(()=>{window.__fcw=fsCanWrite;fsCanWrite=p=>pkey(p)==pkey(homePath('DESKTOP'))?false:__fcw(p);return 1})()");
  await q(`(()=>{WA.openPath(downloadsPath());return 1})()`); await wait(500); await drag(DL + '/PIC-8.TXT', await freePt(3));
  const refused = !(await DK())['PIC-8.TXT'] && (await DLd())['PIC-8.TXT'] && (await q("DKX.said")) == 'That folder belongs to another account.';
  await q("(()=>{fsCanWrite=__fcw;return 1})()");
  ok('2.5 a place File Explorer may not write: refused with File Explorer\'s message, nothing moved', refused);
  /* ===== 3. Cut, Copy and Paste ===== */
  await q("(()=>{FC.op=null;FC.items=[];FC.r=null;return 1})()"); await one();
  let mp = await freePt(5); let m = await menuAt(mp.x, mp.y); await closeMenus();
  ok('3.1 the desktop\'s menu before anything is cut or copied: Paste there, greyed', m.some(x => x[0] == 'Paste' && !x[1]), m);
  ok('3.2 File Explorer\'s menu: Cut and Copy, with two files chosen', await exMenu([DL + '/PIC-8.TXT', DL + '/PIC-9.TXT'], 'Cut'), await q('FC'));
  mp = await freePt(5); m = await menuAt(mp.x, mp.y); ok('3.3 ...then the desktop\'s menu: Paste', m.some(x => x[0] == 'Paste' && x[1]), m);
  want = await cellAt(mp.x, mp.y); await pick('Paste'); await done();
  ok(`3.4 Paste after Cut: both moved into the Desktop folder, the first icon where the menu was opened (${want}), the other next to it`, (await DK())['PIC-8.TXT'] && (await DK())['PIC-9.TXT'] && !(await DLd())['PIC-8.TXT'] && JSON.stringify((await icon('PIC-8.TXT')).cell) == JSON.stringify(want) && adj((await icon('PIC-8.TXT')).cell, (await icon('PIC-9.TXT')).cell), {k: Object.keys(await DK()), a: await icon('PIC-8.TXT')});
  mp = await freePt(5); m = await menuAt(mp.x, mp.y); await closeMenus();
  ok('3.5 ...Cut empties the clipboard: Paste greyed again', m.some(x => x[0] == 'Paste' && !x[1]) && !(await q('FC.op')), m);
  await q("(()=>{const D=node(downloadsPath());D['COPY-ME.TXT']='copy me';fsSave();EXPL.forEach(f=>f());return 1})()"); await wait(300);
  ok('3.6 Copy in File Explorer\'s menu', await exMenu([DL + '/COPY-ME.TXT'], 'Copy'));
  mp = await freePt(8); await menuAt(mp.x, mp.y); await pick('Paste'); await done(); mp = await freePt(8); await menuAt(mp.x, mp.y); await pick('Paste'); await done();
  ok('3.7 Paste after Copy, twice: two copies (COPY-ME.TXT, COPY-ME - COPY.TXT), Downloads keeps it, the clipboard kept', (await DK())['COPY-ME.TXT'] == 'copy me' && (await DK())['COPY-ME - COPY.TXT'] == 'copy me' && (await DLd())['COPY-ME.TXT'] && (await q('FC.op')) == 'copy', Object.keys(await DK()));
  // Ctrl+V on the desktop (nothing in front: a click on its empty area first)
  mp = await freePt(10); await p.mouse.click(mp.x, mp.y); await p.keyboard.press('Control+v'); await done();
  ok('3.8 Ctrl+V on the desktop: pasted too', (await DK())['COPY-ME - COPY (2).TXT'] == 'copy me' || Object.keys(await DK()).filter(n => /^COPY-ME/.test(n)).length == 3, Object.keys(await DK()));
  // Ctrl+V in a window does not paste on the desktop
  const nDesk = Object.keys(await DK()).length; await q("WA.body.querySelector('.xc').focus()"); await p.keyboard.press('Control+v'); await done();
  ok('3.9 ...but Ctrl+V in File Explorer is File Explorer\'s (Downloads), not the desktop\'s', Object.keys(await DK()).length == nDesk && Object.keys(await DLd()).filter(n => /^COPY-ME/.test(n)).length == 2, {dl: Object.keys(await DLd()), nDesk, k: Object.keys(await DK()), act: await q('document.activeElement.className')});
  // Paste into folder
  m = await menuAt(0, 0, `[...DESK.querySelectorAll(':scope > .ic.fic')].find(x=>x.dataset.fn=='BOX')`);
  ok('3.10 a desktop folder\'s menu: Paste into folder (with something copied)', m.some(x => x[0] == 'Paste into folder' && x[1]), m);
  await pick('Paste into folder'); await done();
  ok('3.11 ...pasted into that folder', (await DK()).BOX['COPY-ME.TXT'] == 'copy me', (await DK()).BOX);
  m = await menuAt(0, 0, `[...DESK.querySelectorAll(':scope > .ic.fic')].find(x=>x.dataset.fn=='NOTE.TXT')`); await closeMenus();
  ok('3.12 a file\'s menu: Cut and Copy, no Paste into folder', m.some(x => x[0] == 'Cut' && x[1]) && m.some(x => x[0] == 'Copy' && x[1]) && !m.some(x => x[0] == 'Paste into folder'), m);
  // a drive's file copied in File Explorer, pasted on the desktop
  await one(STICK); await p.mouse.click(5, 5); const pr = await rect('R:photo-1.png'); await p.mouse.click(pr.x, pr.y); await p.mouse.click(pr.x, pr.y, {button: 'right'}); await wait(100); await pick('Copy');
  mp = await freePt(14); await menuAt(mp.x, mp.y); await pick('Paste'); await done();
  ok('3.13 a USB drive\'s file copied in File Explorer, Paste on the desktop: copied (PHOTO-1 - COPY.PNG), the drive keeps it', /^data:image\/png/.test((await DK())['PHOTO-1 - COPY.PNG'] || '') && F.has('/media/sdb1/photo-1.png'), Object.keys(await DK()));
  // the other way: Cut and Copy on the desktop, Paste in File Explorer
  await q(`(()=>{wins.slice().forEach(w=>w.close());const A=explorer(homePath('DOCUMENTS'));A.el.style.cssText='left:760px;top:0;width:640px;height:560px';window.WA=A;const D=node(homePath('DOCUMENTS'));Object.keys(D).forEach(k=>delete D[k]);fsSave();EXPL.forEach(f=>f());return 1})()`); await wait(600);
  await menuAt(0, 0, `[...DESK.querySelectorAll(':scope > .ic.fic')].find(x=>x.dataset.fn=='NOTE.TXT')`); await pick('Copy');
  await q("(()=>{WA.body.querySelector('.ps').click();return 1})()"); await done();
  ok('3.14 Copy on a desktop file, Paste in File Explorer (Documents): copied, the desktop keeps it', (await C("homePath('DOCUMENTS')"))['NOTE.TXT'] == 'a note' && (await DK())['NOTE.TXT'], await C("homePath('DOCUMENTS')"));
  mp = await freePt(16); await p.mouse.click(mp.x, mp.y); const i8 = await icon('PIC-8.TXT'), i9 = await icon('PIC-9.TXT'); await p.mouse.click(i8.x, i8.y); await p.keyboard.down('Control'); await p.mouse.click(i9.x, i9.y); await p.keyboard.up('Control');
  await p.keyboard.press('Control+x'); await q("(()=>{WA.body.querySelector('.xc').focus();return 1})()"); await p.keyboard.press('Control+v'); await done();
  ok('3.15 two desktop files chosen, Ctrl+X on the desktop, Ctrl+V in File Explorer: both moved there, off the desktop', (await C("homePath('DOCUMENTS')"))['PIC-8.TXT'] && (await C("homePath('DOCUMENTS')"))['PIC-9.TXT'] && !(await DK())['PIC-8.TXT'] && !(await icon('PIC-9.TXT')), {d: Object.keys(await C("homePath('DOCUMENTS')")), k: Object.keys(await DK())});
  /* ===== 4. the desktop's own drags, as before ===== */
  await q("(()=>{wins.slice().forEach(w=>w.close());const N=notepad();N.el.style.cssText='left:760px;top:0;width:600px;height:400px';window.WN=N;return 1})()"); await wait(500);
  { const a = await icon('NOTE.TXT'); await p.mouse.move(a.x, a.y); await p.mouse.down(); await p.mouse.move(a.x - 50, a.y, {steps: 4}); await p.mouse.move(1000, 200, {steps: 10}); await p.mouse.up(); await wait(800);
    ok('4.1 a desktop file dragged onto Notepad\'s window: opened there (as before), still on the desktop', await q("wins.some(w=>w.app=='notepad'&&/NOTE\\.TXT/.test(w.el.querySelector('.tb .t').textContent+(w.file||'')))") && (await DK())['NOTE.TXT'], await q("wins.map(w=>w.app+':'+w.el.querySelector('.tb .t').textContent)")) }
  { const a = await icon('NOTE - COPY.TXT'), t = await icon('Trash'); await p.mouse.move(a.x, a.y); await p.mouse.down(); await p.mouse.move(a.x + 30, a.y, {steps: 4}); await p.mouse.move(t.x, t.y, {steps: 10}); await p.mouse.up(); await wait(500);
    ok('4.2 a desktop file dragged onto the Trash: in the Trash (as before)', !(await DK())['NOTE - COPY.TXT'] && await q("TR.some(t=>t.n=='NOTE - COPY.TXT')")) }
  { const a = await icon('NOTES.TXT'), c0 = a.cell, t = await freePt(20), tc = await cellAt(t.x, t.y); await p.mouse.move(a.x, a.y); await p.mouse.down(); await p.mouse.move(a.x + 10, a.y + 10, {steps: 3}); await p.mouse.move(t.x, t.y, {steps: 10}); await p.mouse.up(); await wait(400);
    const c1 = (await icon('NOTES.TXT')).cell; ok('4.3 a desktop icon dragged on the desktop: to the new place (as before)', c1 && JSON.stringify(c1) != JSON.stringify(c0) && Math.abs(c1[0] - tc[0]) <= 1 && Math.abs(c1[1] - tc[1]) <= 1, [c0, c1, tc]) }
  /* ===== 5. the native desktop's pages (each window a page of its own, as far as Chromium shows it) ===== */
  reset();
  const c5 = await b.newContext({viewport: {width: 900, height: 640}});
  let pg = await c5.newPage(); pg.on('pageerror', e => { if (!/volUpd|debian/.test(e.stack || e.message || '')) errs.push('native: ' + e.message) });
  await pg.goto(`http://127.0.0.1:${PORT}/`); await pg.waitForFunction(() => typeof oaOpen == 'function' && typeof acctNew == 'function' && FSLOADED, null, {timeout: 30000});
  const acct2 = await pg.evaluate(`(async()=>{svj('bd95setup',1);['oobe'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    const a=await acctNew('Ann','pw-one','admin');ACCTS=[a];window.__sys=1;saveUser();USER=a;await userLogin(a,'pw-one');
    const D=node(downloadsPath());Object.keys(D).forEach(k=>delete D[k]);D['GAME-NOTES.TXT']='notes for the game';D['MAP.TXT']='a map';D['KEEP.TXT']='keep';
    const K=node(homePath('DESKTOP'));Object.keys(K).forEach(k=>delete K[k]);K['BOX']={};fsSave();fsFlush();await new Promise(r=>setTimeout(r,800));
    const hex=u=>[...new Uint8Array(u)].map(x=>x.toString(16).padStart(2,'0')).join('');return {id:a.id,pk:hex(await pbkBits('pw-one',a.ks,256)),fk:hex(FKRAW)}})()`);
  await pg.close(); M.session = {ok: true, id: acct2.id, n: 'Ann', r: 'admin', pk: acct2.pk, fk: acct2.fk};
  const oa = async (h) => { const x = await c5.newPage(); x.on('pageerror', e => { if (!/volUpd|debian/.test(e.stack || e.message || '')) errs.push('native: ' + e.message) });
    await x.addInitScript("window.__msgs=[];window.webkit={messageHandlers:{rudolfNative:{postMessage:m=>{__msgs.push(m)}}}};"); await x.goto(`http://127.0.0.1:${PORT}/#app=${h}`); return x };
  const A = await oa('explorer'); await A.waitForSelector('.win.oamain', {timeout: 30000}); await wait(800);
  await A.evaluate("(()=>{OA.main.openPath(downloadsPath());return 1})()"); await wait(800);
  const DLp = await A.evaluate("downloadsPath().join('/')");
  const startIn = (pgx, k) => pgx.evaluate(`(()=>{const r=OA.main.body.querySelector('[data-k="'+${JSON.stringify(k)}+'"]');if(!r)return null;const dt=new DataTransfer();r.dispatchEvent(new DragEvent('dragstart',{bubbles:true,cancelable:true,dataTransfer:dt}));
    const o={};[...dt.types].forEach(t=>o[t]=dt.getData(t));r.dispatchEvent(new DragEvent('dragend',{bubbles:true,dataTransfer:dt}));return o})()`);
  const NK = pgx => pgx.evaluate("JSON.stringify(node(homePath('DESKTOP')))").then(JSON.parse);
  await startIn(A, DLp + '/GAME-NOTES.TXT'); await wait(300); M.cat = null;
  let took = await A.evaluate("__deskDrop({c:7,r:3,nc:14,nr:8})"); await wait(1500);
  ok('5.1 Native: the shell\'s drop on the desktop, done by the Explorer window\'s page (after the drag ended): moved into the Desktop folder', took == 1 && (await NK(A))['GAME-NOTES.TXT'] && await A.evaluate("!node(downloadsPath())['GAME-NOTES.TXT']"), await NK(A));
  ok('5.2 ...its place, the cell the shell named, given to the shell with the page\'s list ("place"), and the page\'s own places', M.cat && M.cat.place && JSON.stringify(M.cat.place.c['file:GAME-NOTES.TXT']) == '[7,3]' && await A.evaluate("JSON.stringify(cells['file:GAME-NOTES.TXT'])") == '[7,3]', M.cat && M.cat.place);
  await startIn(A, DLp + '/MAP.TXT'); took = await A.evaluate("__deskDrop({on:'BOX',copy:1})"); await wait(1200);
  ok('5.3 ...dropped on a desktop folder with Ctrl: copied into it', took == 1 && (await NK(A)).BOX['MAP.TXT'] == 'a map' && await A.evaluate("!!node(downloadsPath())['MAP.TXT']"), await NK(A));
  await startIn(A, DLp + '/MAP.TXT'); took = await A.evaluate("__deskDrop({trash:1})"); await wait(800);
  ok('5.4 ...dropped on the Trash: in the Trash', took == 1 && await A.evaluate("!node(downloadsPath())['MAP.TXT']&&TR.some(t=>t.n=='MAP.TXT')"));
  // Copy in the Explorer window's page: the shell told (op fc), the clipboard where the other pages read it
  await A.evaluate(`(()=>{__msgs.length=0;const r=OA.main.body.querySelector('[data-k="'+downloadsPath().join('/')+'/KEEP.TXT"]');r.click();OA.main.body.querySelector('.cp').click();return 1})()`);
  const fcm = await A.evaluate("__msgs.filter(m=>m.op=='fc').map(m=>m.n)");
  ok('5.5 Copy in an Explorer window\'s page: the shell told there is 1 item to paste (op fc)', fcm.at(-1) == '1', fcm);
  // the shell's desktop menu's Paste: done by its hidden page (#app=do-bg, __oaDo, as the shell's bg_route hands it), at the cell the menu was opened over
  const G = await oa('do-bg'); await G.waitForFunction(() => typeof window.__oaDo == 'function' && USER, null, {timeout: 40000});
  const doIt = async ex => { await G.evaluate("__msgs.length=0"); const t = await G.evaluate(`__oaDo('do-desk',${JSON.stringify(ex)},'')`); await G.waitForFunction(() => window.__msgs.some(m => m.op == 'done'), null, {timeout: 30000}).catch(() => 0); await wait(1500);
    return {t, done: await G.evaluate("__msgs.filter(m=>m.op=='done')"), fc: await G.evaluate("__msgs.filter(m=>m.op=='fc').map(m=>m.n)")} };
  await G.evaluate("(()=>{node(homePath('DOCUMENTS'))['K0.TXT']='k';fsSave();return 1})()"); await wait(1500);   // (a warm-up: a fresh page's first save is not told to the other pages in Chromium here, as before this change)
  M.cat = null; let pd = await doIt('&item=Paste&cell=2,4&nc=14&nr=8');
  ok('5.6 the shell\'s Paste (do-desk in its hidden page): pasted into the Desktop folder, done without an error; the shell given the cell (2,4) and the names to place', (await NK(A))['KEEP.TXT'] == 'keep' && pd.done.length && !pd.done[0].err && M.cat && M.cat.place && JSON.stringify(M.cat.place.at) == '[2,4]' && M.cat.place.n.includes('KEEP.TXT'), {pd, k: await NK(A), g: await NK(G), place: M.cat && M.cat.place});
  pd = await doIt('&on=BOX&item=Paste into folder');
  ok('5.7 the shell\'s Paste into folder on a desktop folder: pasted into it', (await NK(A)).BOX['KEEP.TXT'] == 'keep', {pd, k: await NK(A)});
  pd = await doIt('&on=KEEP.TXT&item=Cut');
  await A.evaluate("(()=>{__msgs.length=0;OA.main.openPath(homePath('DOCUMENTS'));return 1})()"); await wait(500); await A.evaluate("(()=>{OA.main.body.querySelector('.ps').click();return 1})()"); await wait(1500);
  ok('5.8 the shell\'s Cut on a desktop file, Paste in the Explorer window\'s page: moved there (the shell told 1, then 0)', pd.fc.at(-1) == '1' && await A.evaluate("!!node(homePath('DOCUMENTS'))['KEEP.TXT']") && !(await NK(A))['KEEP.TXT'] && (await A.evaluate("__msgs.filter(m=>m.op=='fc').map(m=>m.n)")).at(-1) == '0', {pd, k: await NK(A), d: await A.evaluate("Object.keys(node(homePath('DOCUMENTS')))")});
  await c5.close();
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
