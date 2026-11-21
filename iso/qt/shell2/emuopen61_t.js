// 1.6.1 (the owner, 2026-11-20: Emu's "Open a file…" refused a 693 MB ZIP: "larger than 512.0 MB; copy it in Files
// instead"): discs, ZIPs and large files chosen there are not read into the page; the picker hands over their place on the
// disk (picker26.js data-rf-real) and Emu adds them to the game library (rudolfd games/import: a ZIP unpacked), then plays
// the one added. A cartridge still plays at once; a 7-Zip file is refused with a reason; other pickers keep their limit.
// rudolfd stood in; made-up files only.   node emuopen61_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const DL = '/home/rudolf/Downloads';
const DIRS = {[DL]: [{name: 'MADE UP RACER.ZIP', dir: false, size: 726900000, mtime: 1}, {name: 'small.zip', dir: false, size: 5000, mtime: 1}, {name: 'packed.7z', dir: false, size: 700000000, mtime: 1},
  {name: 'tiny.sfc', dir: false, size: 4, mtime: 1}, {name: 'huge.bin', dir: false, size: 600 * 1048576, mtime: 1}]};
const CALLS = []; let IMPORT = '';
const api = (a, q, method, body) => { const sp = new URLSearchParams(q || '');
  if (a == 'fs/list') return {ok: true, items: DIRS[sp.get('path')] || []};
  if (a == 'games/import') { IMPORT = body; return {ok: true, msg: 'started'} }
  if (a == 'job') return {ok: true, running: false, status: 0, log: 'progress 1 1 MADE UP RACER.ZIP\nadded DC/MADE UP RACER.cdi\ndone 1 added, 0 skipped\n'};
  if (a == 'games/info') return {ok: true, used: 1000, free: 5e10, persistent: true};
  return {ok: true} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0); let body = '';
  q.on('data', c => body += c); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/fs/read') { CALLS.push(new URLSearchParams(qs).get('path')); s.writeHead(200); return s.end('ROM!') }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs, q.method, body))) }
    s.writeHead(404); s.end() }) }).listen(8613, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8613/'); await p.waitForFunction(() => typeof emuPickFile == 'function' && typeof rfPick == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});USER=USER||ACCTS[0];USER.r='admin';window.__played=[];emuOpenReal=x=>__played.push(x);emuPlay=g=>__played.push('play:'+g.name+':'+(g.bytes?g.bytes.length:0));return 1})()");
  const pick = async name => { await q('emuPickFile()'); await p.waitForSelector('.rfpick .pkr'); await p.check('.rfpick .rfall').catch(() => {});
    await wait(300); await p.locator('.rfpick .pkr', {hasText: name}).first().dblclick(); await wait(500) };
  // 1. the large ZIP
  await pick('MADE UP RACER.ZIP');
  const m1 = await q("(document.querySelector('.rfpick .rfmsg')||{}).textContent||''");
  ok('1. a 693 MB ZIP is taken (no "larger than 512 MB; copy it in Files instead"), and nothing is read into the page', !/larger than/.test(m1) && !document_has_read(CALLS, 'MADE UP'), m1);
  const ask = await q("[...document.querySelectorAll('.natdlg h3')].map(h=>h.textContent).join('|')+' '+[...document.querySelectorAll('.natdlg .emuaskb')].map(h=>h.textContent).join('|')");
  ok('2. Emu asks to add it to the game library (the ZIP unpacked, the original kept)', /Add to the game library\?/.test(ask) && /ZIP is unpacked/.test(ask) && /original stays/.test(ask), ask);
  await p.click('.natdlg .g'); let w3 = '';
  for (let k = 0; k < 40 && !/Added:/.test(w3); k++) { await wait(100); w3 = await q("[...document.querySelectorAll('.win')].map(w=>w.textContent).find(t=>/Adding to the game library/.test(t))||''") }
  ok('3. rudolfd games/import is asked for that file', IMPORT == DL + '/MADE UP RACER.ZIP', IMPORT);
  await wait(1500);
  ok('4. a window shows it being added, then what was added', /Added: 1 added, 0 skipped/.test(w3) && /✓ DC\/MADE UP RACER\.cdi/.test(w3), w3);
  ok('5. ...and the game added starts (from the library: Flycast for a Dreamcast disc)', (await q('__played')).includes('/home/rudolf/Games/DC/MADE UP RACER.cdi'), await q('__played'));
  // 6. a small ZIP goes the same way
  IMPORT = ''; await pick('small.zip'); await wait(200);
  ok('6. a small ZIP too: added to the library, not played as a cartridge', /Add to the game library/.test(await q("(document.querySelector('.natdlg h3')||{}).textContent||''")));
  await q("document.querySelectorAll('.natdlg .c').forEach(b=>b.click())");
  // 7. 7-Zip
  await pick('packed.7z'); await wait(300);
  const n7 = await q("[...document.querySelectorAll('.ntoast')].map(t=>t.textContent).join('|')");
  ok('7. a 7-Zip file: refused with a reason (Emu cannot unpack it)', /7-Zip file, which Emu cannot unpack/.test(n7), n7);
  // 8. a cartridge plays at once, read as before
  await q('__played.length=0'); await pick('tiny.sfc'); await wait(500);
  ok('8. a small cartridge (.sfc) is read and played at once, as before', (await q('__played')).some(x => /^play:tiny\.sfc:4$/.test(x)), await q('__played'));
  // 9. another picker keeps its limit
  await q("(()=>{const i=document.createElement('input');i.type='file';rfPick(i);return 1})()"); await p.waitForSelector('.rfpick .pkr');
  await p.locator('.rfpick .pkr', {hasText: 'huge.bin'}).dblclick(); await wait(300);
  ok('9. other pickers keep the 512 MB limit and its message', /larger than 512/.test(await q("(document.querySelector('.rfpick .rfmsg')||{}).textContent||''")));
  ok('no page errors', !errs.length, errs);
  await b.close(); srv.close();
  const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
function document_has_read(calls, s) { return calls.some(c => String(c).includes(s)) }
