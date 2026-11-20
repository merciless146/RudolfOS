// 1.6.1 (emu26.js): Genesis / Mega Drive (ClownMDEmu) and PlayStation (PCSX-ReARMed) in Emu, libretro cores as
// WebAssembly with Emu's frontend (iso/sys/emu/lr_api.c). The library shows them under their tabs (a Genesis game by the
// name in its header, a PlayStation .cue disc by its folder's name); a Genesis game plays from memory, a PlayStation disc
// is read from the library as the core asks for it (synchronous range requests to fs/read); each console has its own
// buttons (keyboard, gamepad, on screen, the Controls window), the PlayStation the DualShock's sticks; battery memory,
// memory cards and save states go to the console's Saves and States. Made-up programs only (a red screen each), no games.
//   node emulr61_t.js CORES-DIR [CHROME]   (CORES-DIR: clownmd.wasm and pcsx.wasm, as rudolfos-library's
//   apps/rudolf-emu/www; build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www'), CORES = process.argv[2];
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
const G = '/home/rudolf/Games';
// a Genesis program: the TMSS write, the VDP on, colour 0 red, a tone; then it waits
const mdRom = () => { const r = Buffer.alloc(0x10000); r.writeUInt32BE(0x00FFFE00, 0); r.writeUInt32BE(0x200, 4); for (let i = 2; i < 64; i++) r.writeUInt32BE(0x300, i * 4);
  r.write('SEGA MEGA DRIVE ', 0x100, 'latin1'); r.write('RUDOLF TEST ROM ', 0x120, 'latin1'); r.writeUInt32BE(0xFFFF, 0x1A4);
  Buffer.from('23FC5345474100A14000' + '33FC800400C00004' + '33FC814400C00004' + '33FC8F0200C00004' + '23FCC000000000C00004' + '33FC000E00C00000' + '33FC870000C00004' +
    '13FC008E00C00011' + '13FC000F00C00011' + '13FC009000C00011' + '60FE', 'hex').copy(r, 0x200); r.writeUInt16BE(0x4E73, 0x300); return r };
// a PlayStation program (PS-X EXE): the GPU on, a red rectangle over the screen; then it waits
const psExe = () => { const code = [0x3C081F80, 0x3C090300, 0xAD091814, 0x3C090800, 0x35290001, 0xAD091814, 0x3C090500, 0xAD091814, 0x3C0906C6, 0x35290260, 0xAD091814, 0x3C090704, 0x35292018, 0xAD091814,
    0x3C090200, 0x352900FF, 0xAD091810, 0xAD001810, 0x3C0900F0, 0x35290140, 0xAD091810, 0x1000FFFF, 0];
  const body = Buffer.alloc(2048); code.forEach((c, i) => body.writeUInt32LE(c >>> 0, i * 4)); const h = Buffer.alloc(2048); h.write('PS-X EXE', 0, 'latin1');
  [0x80010000, 0, 0x80010000, 2048, 0, 0, 0, 0, 0x801FFFF0, 0].forEach((v, i) => h.writeUInt32LE(v >>> 0, 0x10 + 4 * i)); return Buffer.concat([h, body]) };
const FILES = {[G + '/MD/test.md']: mdRom(), [G + '/PS1/Red Test/red.exe']: psExe()};
const it = (name, size, dir) => ({name, dir: !!dir, size, mtime: 1});
const DIRS = {[G + '/MD']: [it('test.md', 65536), it('Saves', 0, 1), it('States', 0, 1)],
  [G + '/PS1']: [it('Made Up Disc', 0, 1), it('Another Made Up.chd', 400000000), it('Saves', 0, 1), it('States', 0, 1), it('BIOS', 0, 1), it('Red Test', 0, 1)],
  [G + '/PS1/Made Up Disc']: [it('Made Up Disc.cue', 90), it('Made Up Disc (Track 1).bin', 500000000)], [G + '/PS1/Red Test']: [it('red.exe', 4096)], [G + '/PS1/BIOS']: [],
  '/home/rudolf/Downloads': [it('a.md', 9), it('b.cue', 9), it('b.bin', 9), it('c.pbp', 9), it('notes.txt', 3)]};
const WRITES = [], RANGES = [];
const api = (a, q, method, body) => { const sp = new URLSearchParams(q || '');
  if (a == 'fs/list') return {ok: true, items: DIRS[sp.get('path')] || []};
  if (a == 'fs/write') { WRITES.push({path: sp.get('path'), size: body.length}); FILES[sp.get('path')] = body; return {ok: true} }
  if (a == 'games/info') return {ok: true, used: 1000, free: 5e10, persistent: true};
  if (a == 'disks') return {ok: true, disks: []};
  if (a == 'pkg/status') return {ok: true, installed: [{name: 'rudolf-emu', version: '1.1.0'}], available: [], sources: []};
  return {ok: true} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0); const chunks = [];
  q.on('data', c => chunks.push(c)); q.on('end', () => { const body = Buffer.concat(chunks);
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    const m = /^\/pkg\/rudolf-emu\/www\/(\w+\.wasm)$/.exec(u);
    if (m && fs.existsSync(path.join(CORES, m[1]))) { s.writeHead(200, {'content-type': 'application/wasm'}); return s.end(fs.readFileSync(path.join(CORES, m[1]))) }
    if (u == '/api/fs/read') { const f = FILES[new URLSearchParams(qs).get('path')]; if (!f) { s.writeHead(404, {'content-type': 'application/json'}); return s.end('{"ok":false,"error":"no such file"}') }
      const r = /bytes=(\d+)-(\d+)/.exec(q.headers.range || ''); if (r) { const a = +r[1], b = Math.min(+r[2], f.length - 1); RANGES.push({path: new URLSearchParams(qs).get('path'), a, b});
        s.writeHead(206, {'content-type': 'application/octet-stream', 'content-range': `bytes ${a}-${b}/${f.length}`}); return s.end(f.subarray(a, b + 1)) }
      s.writeHead(200, {'content-type': 'application/octet-stream'}); return s.end(f) }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs, q.method, body))) }
    s.writeHead(404); s.end() }) }).listen(8098, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[3] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8098/'); await p.waitForFunction(() => typeof emuLibrary == 'function' && typeof fs != 'undefined' && fs['C:'], null, {timeout: 30000});
  await p.evaluate(() => { try { svj('bd95setup', 1); svj('bd95emutab', 'all'); svj('bd95emubl', false) } catch (e) {} ['oobe', 'login'].forEach(i => { const o = document.getElementById(i); o && o.remove() }); USER = USER || ACCTS[0]; wins.slice().forEach(w => w.close && w.close());
    window.__notes = []; const n0 = notify; notify = (t, m, o) => { window.__notes.push(t + ' | ' + m); return n0(t, m, o) }; elevate = async () => true;
    /* a gamepad: the left stick half right and full up, the right stick a little left (inside the dead zone), button 10 held */
    window.__pad = null; navigator.getGamepads = () => [window.__pad] });
  await p.evaluate(() => emuLibrary()); await wait(1500);
  const tab = async t => { await p.evaluate(t => wins.filter(w => w.body.classList.contains('emulib')).at(-1).body.querySelector(`.eltabs [data-t='${t}']`).click(), t); await wait(300);
    return p.evaluate(() => [...wins.filter(w => w.body.classList.contains('emulib')).at(-1).body.querySelectorAll('.eltile')].map(x => x.innerText.replace(/\s+/g, ' '))) };
  const tabs = await p.evaluate(() => [...wins.at(-1).body.querySelectorAll('.eltabs button')].map(x => x.textContent));
  ok('1. the library has Genesis and PlayStation tabs', tabs.includes('Genesis') && tabs.includes('PlayStation'), tabs);
  const md = await tab('md'); ok('2. the Genesis tab shows its game by the name in its header', md.length == 1 && /Rudolf Test Rom Genesis · 64/.test(md[0]), md);
  const ps = await tab('ps1'); ok('3. the PlayStation tab shows a .cue disc by its folder\'s name (its tracks counted) and a .chd, not the BIOS, Saves or States folders',
    ps.length == 2 && ps.some(t => /^💿 Made Up Disc PlayStation · 47[67]/.test(t)) && ps.some(t => /Another Made Up PlayStation/.test(t)), ps);
  const so = await p.evaluate(() => { const G2 = EMU_GAMES; return ['/x/a.MD', '/x/a.gen', '/x/a.smd', '/x/a.cue', '/x/a.pbp', G2 + '/PS1/a.chd', G2 + '/DC/a.chd', '/x/a.chd', G2 + '/MD/a.bin', G2 + '/PS1/x/a.bin', '/x/a.sfc', '/x/a.gdi'].map(emuSysOf).join(' ') });
  ok('4. which console a file is for: by its name, a .chd and a .bin by where they are', so == 'md md md ps1 ps1 ps1 dc dc md ps1 snes dc', so);
  const smd = await p.evaluate(() => { const r = new Uint8Array(32768); for (let i = 0; i < r.length; i++) r[i] = (i * 7 + (i >> 8)) & 255; r.set([83, 69, 71, 65], 0x100);
    const s = new Uint8Array(512 + r.length); for (let b = 0; b < r.length; b += 16384) for (let i = 0; i < 8192; i++) { s[512 + b + i] = r[b + 2 * i + 1]; s[512 + b + 8192 + i] = r[b + 2 * i] }
    const back = emuMdRom(s), same = back.length == r.length && back.every((v, i) => v == r[i]); return same && emuMdRom(r) === r });
  ok('5. an interleaved .smd Genesis game is put back in order (a plain one is left as it is)', smd);
  // ---- Genesis: plays from memory ----
  await tab('md');
  await p.evaluate(() => wins.filter(w => w.body.classList.contains('emulib')).at(-1).body.querySelector('.eltile').click());
  await p.waitForFunction(() => wins.some(w => w.emuApi), null, {timeout: 20000}).catch(() => {}); await wait(1500);
  const m1 = await p.evaluate(() => { const W = wins.find(w => w.emuApi); if (!W) return {err: 'no player', notes: window.__notes};
    const c = W.emuApi.canvas, d = c.getContext('2d').getImageData(c.width >> 1, c.height >> 1, 1, 1).data;
    return {title: W.title || W.el.querySelector('.ttl, .title, .wt')?.textContent, w: c.width, h: c.height, px: [...d], frames: W.emuApi.frames(), pad: [...W.body.querySelectorAll('.emupad [data-b]')].map(b => b.dataset.b).join(' ')} });
  ok('6. a Genesis game plays: frames run, its picture (256 x 224, the red the program sets) is on the canvas', m1.frames > 30 && m1.w == 256 && m1.h == 224 && m1.px[0] > 200 && m1.px[1] < 40 && m1.px[2] < 40, m1);
  ok('7. ...with the Genesis\'s on-screen buttons: A B C, X Y Z, Mode and Start', m1.pad == 'Up Left Right Down Mode Start X Y Z A B C', m1.pad);
  const bits = await p.evaluate(async () => { const W = wins.find(w => w.emuApi), A = W.emuApi, out = [];
    for (const [b, k] of [['A', 'KeyA'], ['B', 'KeyS'], ['C', 'KeyD'], ['X', 'KeyQ'], ['Y', 'KeyW'], ['Z', 'KeyE'], ['Mode', 'ShiftRight'], ['Start', 'Enter']]) {
      W.body.dispatchEvent(new KeyboardEvent('keydown', {code: k, bubbles: true})); A.step(true); out.push(A.bits()); W.body.dispatchEvent(new KeyboardEvent('keyup', {code: k, bubbles: true})) }
    window.__pad = {buttons: Array.from({length: 17}, (_, i) => ({pressed: i == 2})), axes: [-0.9, 0, 0, 0]}; await new Promise(r => setTimeout(r, 120)); A.step(true); out.push(A.bits()); window.__pad = null; await new Promise(r => setTimeout(r, 120));
    return out });
  ok('8. ...its keys are the RetroPad bits the core reads (A Y, B B, C A, X L, Y X, Z R, Mode Select, Start)', bits.slice(0, 8).join(' ') == [2, 1, 256, 1024, 512, 2048, 4, 8].join(' '), bits);
  ok('9. ...a gamepad\'s left button is A and its left stick the d-pad', bits[8] == (2 | 64), bits[8]);
  const st1 = await p.evaluate(async () => { const A = wins.find(w => w.emuApi).emuApi; await A.saveState(); return A.loadState().then(() => wins.find(w => w.emuApi).body.querySelector('.emsg').textContent) });
  ok('10. ...a save state goes to MD/States and loads again', WRITES.some(w => w.path == G + '/MD/States/test.st1' && w.size > 1000000) && /State 1 (saved|loaded)/.test(st1), {WRITES, st1});
  ok('11. ...its untouched battery memory is not written', !WRITES.some(w => /\.sav$/.test(w.path)), WRITES);
  await p.evaluate(() => wins.find(w => w.emuApi).close()); await wait(300);
  // ---- PlayStation: the disc is read as the core asks for it ----
  RANGES.length = 0; WRITES.length = 0;
  await p.evaluate(() => emuPlay({sys: 'ps1', name: 'red.exe', real: EMU_GAMES + '/PS1/Red Test/red.exe', title: 'Red Test'}));
  await p.waitForFunction(() => wins.some(w => w.emuApi), null, {timeout: 20000}).catch(() => {}); await wait(2500);
  const p1 = await p.evaluate(() => { const W = wins.find(w => w.emuApi); if (!W) return {err: 'no player', notes: window.__notes};
    const c = W.emuApi.canvas, d = c.getContext('2d').getImageData(c.width >> 1, c.height >> 1, 1, 1).data;
    return {w: c.width, h: c.height, px: [...d], frames: W.emuApi.frames(), fetched: W.emuApi.disc.fetched(), files: Object.keys(W.emuApi.disc.files), pad: [...W.body.querySelectorAll('.emupad [data-b]')].map(b => b.dataset.b).join(' ')} });
  ok('12. a PlayStation disc plays from the library: frames run, the red picture (320 x 240) is on the canvas', p1.frames > 30 && p1.w == 320 && p1.h == 240 && p1.px[0] > 200 && p1.px[1] < 40, p1);
  ok('13. ...read from fs/read in ranges as the core asks, not downloaded whole first', RANGES.length > 0 && RANGES.every(r => r.path == G + '/PS1/Red Test/red.exe' && r.b - r.a < 262144) && p1.fetched == 4096, {RANGES, f: p1.fetched});
  ok('14. ...with the PlayStation\'s on-screen buttons: L1 L2 R1 R2, △ □ ○ ✕, Select and Start', p1.pad == 'L2 L1 Up Left Right Down Select Start R2 R1 Triangle Square Circle Cross', p1.pad);
  const pb = await p.evaluate(async () => { const W = wins.find(w => w.emuApi), A = W.emuApi, out = [];
    for (const k of ['KeyZ', 'KeyX', 'KeyA', 'KeyS', 'KeyQ', 'KeyW', 'Digit1', 'Digit2', 'KeyC', 'KeyV']) { W.body.dispatchEvent(new KeyboardEvent('keydown', {code: k, bubbles: true})); A.step(true); out.push(A.bits()); W.body.dispatchEvent(new KeyboardEvent('keyup', {code: k, bubbles: true})) }
    W.body.dispatchEvent(new KeyboardEvent('keydown', {code: 'KeyJ', bubbles: true})); A.step(true); const kst = A.axes().slice(); W.body.dispatchEvent(new KeyboardEvent('keyup', {code: 'KeyJ', bubbles: true}));
    window.__pad = {buttons: Array.from({length: 17}, (_, i) => ({pressed: i == 10})), axes: [0.5, -1, -0.1, 0.6]}; await new Promise(r => setTimeout(r, 120));
    const ax = A.axes().slice(), b10 = A.bits(); window.__pad = null; await new Promise(r => setTimeout(r, 120)); return {out, kst, ax, b10} });
  ok('15. ...its keys are the RetroPad bits (✕ B, ○ A, □ Y, △ X, L1 L, R1 R, L2, R2, L3, R3)', pb.out.join(' ') == [1, 256, 2, 512, 1024, 2048, 4096, 8192, 16384, 32768].join(' '), pb.out);
  ok('16. ...the gamepad\'s sticks are the DualShock\'s (a small tilt is nothing), not the d-pad', pb.ax.join(' ') == '16384 -32767 0 19660' && !(pb.b10 & 0xF0), pb);
  ok('17. ...its left-stick click is L3 (not fast-forward), and J I K L tilt the left stick from the keyboard', (pb.b10 & 16384) && pb.kst[0] == -32767, pb);
  const st2 = await p.evaluate(async () => { const A = wins.find(w => w.emuApi).emuApi; await A.saveState(); await A.loadState(); return wins.find(w => w.emuApi).body.querySelector('.emsg').textContent });
  ok('18. ...a save state goes to PS1/States (not inside the disc\'s folder) and loads again', WRITES.some(w => w.path == G + '/PS1/States/red.st1' && w.size > 4000000) && /State 1 loaded/.test(st2), {WRITES, st2});
  const card = await p.evaluate(async () => { const W = wins.find(w => w.emuApi), x = W.emuApi.x; const n = x.lr_save_size(); new Uint8Array(x.memory.buffer, x.lr_save_ptr(), n)[200] ^= 0x5a; W.close(); await new Promise(r => setTimeout(r, 500)); return n });
  ok('19. ...a changed memory card (128 KB) goes to PS1/Saves when the game closes', card == 131072 && WRITES.some(w => w.path == G + '/PS1/Saves/red.sav' && w.size == 131072), {card, WRITES});
  // ---- the Controls window of each ----
  const ctl = await p.evaluate(async () => { const a = emuControls('ps1'), t1 = a.body.innerText; a.close(); const b2 = emuControls('md'), t2 = b2.body.innerText; b2.close(); return {t1, t2} });
  ok('20. the Controls window lists each console\'s buttons: the PlayStation\'s (with the sticks)', /✕ Cross/.test(ctl.t1) && /△ Triangle/.test(ctl.t1) && /L2/.test(ctl.t1) && /R3/.test(ctl.t1) && /Left stick up/.test(ctl.t1) && /DualShock/.test(ctl.t1) && !/Picture shape/.test(ctl.t1), ctl.t1);
  ok('21. ...and the Genesis\'s', /\bC\b/.test(ctl.t2) && /\bZ\b/.test(ctl.t2) && /Mode/.test(ctl.t2) && !/Select/.test(ctl.t2), ctl.t2);
  // ---- a PlayStation game picked from this computer: the library only ----
  const pick = await p.evaluate(async () => { window.__notes.length = 0; await emuPlay({sys: 'ps1', name: 'x.cue', bytes: new Uint8Array(4)}); return window.__notes.join(' / ') });
  ok('22. a PlayStation disc that is not in the library says to add it there', /add it there first/.test(pick), pick);
  // ---- Add games lists Genesis and PlayStation files ----
  await p.evaluate(() => emuAddGames()); await wait(1200);
  const add = await p.evaluate(() => [...wins.at(-1).body.querySelectorAll('.ealist .earow span')].map(x => x.textContent).join(' '));
  ok('23. "Add games" lists Genesis and PlayStation files (.md, .cue, .bin, .pbp), not text files', add == 'a.md b.bin b.cue c.pbp', add);
  ok('24. no page errors', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1)
})().catch(e => { console.log('FAIL', e.stack); process.exit(1) });
