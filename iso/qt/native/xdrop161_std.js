/* 1.6.1 (docs/plan-1.6.1.md Part 13): what Standard (the desktop page in Chromium) makes of files dropped from outside, for
   xdrop161_t.sh to compare with Native's. The page is served by same_rd.py as rudolfd serves it to Standard; Ann signs in; each
   drop is the events Chromium gives the page for a drop from another program (dragenter, dragover, drop, with the files in the
   DataTransfer, as Chromium reads them from the drop's text/uri-list), at the same places as Native's: the empty desktop, a
   folder's icon, the Trash, two files at once, a taskbar button (hovered 1.5 s, an app's window minimised), and text (no file).
   node xdrop161_std.js PORT PASSWORD FILES-DIR OUT.json  (FILES-DIR: the files Native's drops name: hello.txt, dot.png, two.txt) */
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path');
const [PORT, PW, FD, OUT] = process.argv.slice(2); const BASE = 'http://127.0.0.1:' + PORT;
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const br = await chromium.launch(); const ctx = await br.newContext({viewport: {width: 1280, height: 800}});
  await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => r.abort());
  const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto(BASE + '/'); await p.waitForFunction(() => typeof userLogin == 'function' && document.getElementById('login') && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
  await wait(1500); await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
  await p.waitForFunction(() => typeof USER != 'undefined' && USER && getComputedStyle(document.getElementById('login')).display == 'none', null, {timeout: 60000});
  await wait(4000);
  /* a folder on the desktop, as New folder makes it */
  await p.evaluate(() => { const d = deskFiles(); d['NEW FOLDER'] = {}; fsSave(); EXPL.forEach(f => f()) }); await wait(800);
  const files = {hello: ['hello.txt', 'text/plain'], dot: ['dot.png', 'image/png'], two: ['two.txt', 'text/plain']};
  const B = {}; for (const k in files) B[k] = fs.readFileSync(path.join(FD, files[k][0])).toString('base64');
  /* a drop from outside at the middle of SEL (or X Y): the events, with the files; returns whether the page took it (dragover prevented) */
  const drop = (where, ks, text, hoverMs) => p.evaluate(async ([where, ks, text, hoverMs, B, files]) => {
    const pt = typeof where == 'string' ? (() => { const e = where.startsWith('#') || where.startsWith('.') ? document.querySelector(where) : [...document.querySelectorAll('#desk > .ic')].find(x => (x.dataset.fn || x.querySelector('span').textContent.trim()) == where);
      if (!e) return null; const r = e.getBoundingClientRect(); return [r.left + r.width / 2, r.top + Math.min(20, r.height / 2)] })() : where;
    if (!pt) return {err: 'no ' + where};
    const dt = new DataTransfer();
    for (const k of ks) { const raw = Uint8Array.from(atob(B[k]), c => c.charCodeAt(0)); dt.items.add(new File([raw], files[k][0], {type: files[k][1]})) }
    if (text) dt.setData('text/plain', text);
    const el = document.elementFromPoint(pt[0], pt[1]); const o = {clientX: pt[0], clientY: pt[1], bubbles: true, cancelable: true, dataTransfer: dt};
    el.dispatchEvent(new DragEvent('dragenter', o)); let took = false; const t0 = Date.now();
    do { const ev = new DragEvent('dragover', o); el.dispatchEvent(ev); took = ev.defaultPrevented; await new Promise(r => setTimeout(r, 100)) } while (Date.now() - t0 < (hoverMs || 0));
    if (!took) { el.dispatchEvent(new DragEvent('dragleave', o)); return {took: false, on: el.id || el.className} }
    el.dispatchEvent(new DragEvent('drop', o)); await new Promise(r => setTimeout(r, 1500)); return {took: true, on: el.id || el.className}
  }, [where, ks, text || '', hoverMs || 0, B, files]);
  const R = {};
  const desk = () => p.evaluate(() => { const d = deskFiles(), o = {}; for (const n of Object.keys(d).sort()) { const v = d[n]; o[n] = isDir(v) ? {dir: Object.keys(v)} : typeof v == 'string' ? (v.startsWith('data:') ? 'data:' + v.slice(5, v.indexOf(',')) : 'text:' + v) : typeof v } return o });
  const trashN = () => p.evaluate(() => { try { const t = node(['C:', 'TRASH']) || {}; return Object.keys(t).length } catch (e) { return -1 } });
  R.empty = await drop([900, 450], ['hello']); R.afterEmpty = await desk();
  R.folder = await drop('NEW FOLDER', ['hello']); R.afterFolder = await desk();
  R.trash = await drop('Trash', ['dot']); R.afterTrash = await desk(); R.trashCount = await trashN();
  R.two = await drop([1000, 300], ['two', 'dot']); R.afterTwo = await desk();
  R.text = await drop([700, 300], [], 'just some words'); R.afterText = await desk();
  /* a taskbar button, its app's window minimised, hovered 1.5 s */
  await p.evaluate(() => launch('notepad')); await wait(1500);
  await p.evaluate(() => { const w = wins.filter(o => o.app == 'notepad').pop(); w.el.querySelector('.mn').click() }); await wait(500);
  const btn = await p.evaluate(() => { const w = wins.filter(o => o.app == 'notepad').pop(); const b = [...document.querySelectorAll('#bar button, #bar .tbb, #bar [role=button]')].find(x => /Notepad/.test(x.title || x.getAttribute('aria-label') || x.textContent));
    if (!b) return null; const r = b.getBoundingClientRect(); return [r.left + r.width / 2, r.top + r.height / 2] });
  R.bar = btn ? await drop(btn, ['hello'], '', 1500) : {err: 'no taskbar button'};
  R.barWindowShown = await p.evaluate(() => { const w = wins.filter(o => o.app == 'notepad').pop(); return !!w && w.el.style.display != 'none' && !w.min });
  R.afterBar = await desk(); R.errors = errs;
  fs.writeFileSync(OUT, JSON.stringify(R, null, 1)); await br.close();
})().catch(e => { console.error(e); process.exit(1) });
