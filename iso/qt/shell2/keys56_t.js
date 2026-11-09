// 1.5.6 (keys56.js): Chromium's own shortcuts never act on the desktop (Alt+F4 closed it on the Vivobook; in kiosk
// mode F5 reloads it, Ctrl+H puts the history over it, Shift+Esc opens Chromium's task manager...). Trusted keys
// through the DevTools protocol: Chromium's action is stopped (defaultPrevented), the desktop's handlers still see
// them, ordinary keys are left alone, the developer tools follow their switch, and same-origin frames are guarded.
//   node keys56_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8149, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
// key: [code, key, vk]; mods: 1 Alt, 2 Ctrl, 8 Shift
const K = {KeyW: ['KeyW', 'w', 87], KeyR: ['KeyR', 'r', 82], KeyH: ['KeyH', 'h', 72], KeyJ: ['KeyJ', 'j', 74], KeyS: ['KeyS', 's', 83], KeyP: ['KeyP', 'p', 80], KeyF: ['KeyF', 'f', 70], KeyC: ['KeyC', 'c', 67], KeyI: ['KeyI', 'i', 73], KeyA: ['KeyA', 'a', 65], KeyO: ['KeyO', 'o', 79],
  F4: ['F4', 'F4', 115], F5: ['F5', 'F5', 116], F1: ['F1', 'F1', 112], F12: ['F12', 'F12', 123], Escape: ['Escape', 'Escape', 27], Home: ['Home', 'Home', 36], Delete: ['Delete', 'Delete', 46]};
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd|debian/.test(e.stack || e.message || '')) errs.push(e.message) });
  const cdp = await p.context().newCDPSession(p);
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8149/'); await p.waitForFunction(() => window.__keys56 && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  await q("(()=>{['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});window.__seen=[];addEventListener('keydown',e=>{if(e.isTrusted)__seen.push({n:keys56Name(e),p:e.defaultPrevented})},true);return 1})()");
  // a key, then what the page's last handler saw (in the bubble phase, after keys56's guard)
  const press = async (code, mods) => { const [c, key, vk] = K[code]; await q("__seen.length=0");
    await cdp.send('Input.dispatchKeyEvent', {type: 'rawKeyDown', code: c, key, windowsVirtualKeyCode: vk, nativeVirtualKeyCode: vk, modifiers: mods});
    await cdp.send('Input.dispatchKeyEvent', {type: 'keyUp', code: c, key, windowsVirtualKeyCode: vk, nativeVirtualKeyCode: vk, modifiers: mods}); await wait(80);
    return q("__seen[0]||null") };
  const harmful = [['Alt+F4', 'F4', 1], ['Ctrl+W', 'KeyW', 2], ['Ctrl+Shift+W', 'KeyW', 10], ['F5', 'F5', 0], ['Ctrl+R', 'KeyR', 2], ['Shift+F5', 'F5', 8], ['Ctrl+H', 'KeyH', 2], ['Ctrl+J', 'KeyJ', 2],
    ['Ctrl+S', 'KeyS', 2], ['Ctrl+O', 'KeyO', 2], ['Ctrl+P', 'KeyP', 2], ['Ctrl+F', 'KeyF', 2], ['Shift+Esc', 'Escape', 8], ['F1', 'F1', 0], ['Alt+Home', 'Home', 1], ['Ctrl+Shift+O', 'KeyO', 10],
    ['Ctrl+Shift+Delete', 'Delete', 10], ['Ctrl+Shift+A', 'KeyA', 10]];
  const missed = [];
  for (const [n, c, m] of harmful) { const r = await press(c, m); if (!r || !r.p) missed.push([n, r]) }
  ok('1. Chromium\'s action is stopped for each harmful shortcut, and the desktop still sees the key', !missed.length, missed);
  const left = [];
  for (const [n, c, m] of [['Ctrl+C', 'KeyC', 2], ['Ctrl+A', 'KeyA', 2], ['a', 'KeyA', 0], ['Escape', 'Escape', 0], ['Shift+A', 'KeyA', 8]]) { const r = await press(c, m); if (!r || r.p) left.push([n, r]) }
  ok('2. copying, selecting all, typing and Escape are left alone', !left.length, left);
  let r = await press('F12', 0);
  ok('3. the developer tools (F12) are stopped while "Show developer tools" is off', r && r.p, r);
  await q("svj('bd95devtools',true)"); r = await press('F12', 0); const r2 = await press('KeyI', 10); await q("svj('bd95devtools',false)");
  ok('4. ...and allowed once it is on (F12, Ctrl+Shift+I)', r && !r.p && r2 && !r2.p, [r, r2]);
  // a same-origin frame
  await q("(()=>{const f=document.createElement('iframe');f.id='kf';f.srcdoc='<input id=i>';document.body.appendChild(f);return 1})()"); await wait(500);
  await q("(()=>{const w=document.getElementById('kf').contentWindow;w.__seen=[];w.addEventListener('keydown',e=>w.__seen.push(e.defaultPrevented));w.document.getElementById('i').focus();return 1})()");
  await cdp.send('Input.dispatchKeyEvent', {type: 'rawKeyDown', code: 'F5', key: 'F5', windowsVirtualKeyCode: 116, nativeVirtualKeyCode: 116, modifiers: 0}); await cdp.send('Input.dispatchKeyEvent', {type: 'keyUp', code: 'F5', key: 'F5', windowsVirtualKeyCode: 116, modifiers: 0}); await wait(100);
  const fr = await q("document.getElementById('kf').contentWindow.__seen");
  ok('5. inside a same-origin frame (an Oracle page without the shell) F5 is stopped too', fr.length && fr[0] === true, fr);
  // Oracle's real tabs (the shell's PAGE_JS in each website): Chromium's own pages never open from a website, and the
  // website still sees the key; Ctrl+Shift+R and Shift+F5 reload the tab (as F5 does)
  const src = fs.readFileSync(path.join(__dirname, '../../shell/shell.js'), 'utf8').split('\n'); const i0 = src.findIndex(l => l.startsWith('const PAGE_JS = `'));
  let i1 = i0; while (!/`;\s*$/.test(src[i1]) || i1 == i0) i1++;
  const PAGE_JS = new Function('NONCE', src.slice(i0, i1 + 1).join('\n') + ';return PAGE_JS')('N');
  const t = await b.newPage(); const tc = await t.context().newCDPSession(t); const said = [];
  t.on('console', m => { const x = m.text(); if (x.startsWith('N{')) said.push(JSON.parse(x.slice(1))) });
  await t.goto('http://127.0.0.1:8149/nothing-here'); await t.evaluate(PAGE_JS);
  await t.evaluate("window.__s=[];addEventListener('keydown',e=>__s.push(e.defaultPrevented))");
  const tp = async (code, mods) => { const [c, key, vk] = K[code]; await t.evaluate("__s.length=0");
    await tc.send('Input.dispatchKeyEvent', {type: 'rawKeyDown', code: c, key, windowsVirtualKeyCode: vk, nativeVirtualKeyCode: vk, modifiers: mods});
    await tc.send('Input.dispatchKeyEvent', {type: 'keyUp', code: c, key, windowsVirtualKeyCode: vk, nativeVirtualKeyCode: vk, modifiers: mods}); await wait(80); return t.evaluate("__s.slice()") };
  const th = await tp('KeyH', 2), tj = await tp('KeyJ', 2), ts = await tp('Escape', 8), tcc = await tp('KeyC', 2);
  ok('6. in a website tab Ctrl+H, Ctrl+J and Shift+Esc are stopped, the website still sees them; Ctrl+C is left alone', th[0] === true && tj[0] === true && ts[0] === true && tcc[0] === false, {th, tj, ts, tcc});
  said.length = 0; await tp('KeyR', 2); await tp('KeyR', 10); await tp('F5', 8);
  ok('7. in a tab Ctrl+R reloads as before (cr), and Ctrl+Shift+R and Shift+F5 reload it too (as F5), not Chromium', JSON.stringify(said) == '[{"k":"cr"},{"k":"f5"},{"k":"f5"}]', said);
  await t.close();
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
