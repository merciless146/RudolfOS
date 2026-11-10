// 1.5.7: the desktop's right-click menu has Task Manager, which opens it; Ctrl+Alt+Del and Ctrl+Shift+Esc open it above
// every window and ask for the desktop's processes to go first, until it closes.   node deskmenu57_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 500) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8172, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto('http://127.0.0.1:8172/'); await p.waitForFunction(() => typeof deskMenu == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS.length=0;ACCTS.push({n:'ann',h:await pwMake('a1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'a1');return 1})()`);
  await wait(800);
  await p.mouse.click(640, 400, {button: 'right'}); await wait(400);
  const items = await q("[...document.querySelectorAll('.cmenu div, .cmenu li, #cmenu div')].map(e=>e.textContent.trim()).filter(Boolean)");
  ok('1. the desktop\'s right-click menu lists Task Manager (with Ctrl+Shift+Esc)', items.some(t => /^📊?\s*Task Manager/.test(t) && /Ctrl\+Shift\+Esc/.test(t)), items);
  await q("(()=>{const e=[...document.querySelectorAll('.cmenu div, .cmenu li, #cmenu div')].find(e=>/Task Manager/.test(e.textContent)&&!e.querySelector('div'));e.click();return 1})()"); await wait(800);
  ok('2. ...and it opens Task Manager', await q("wins.some(w=>/Task Manager/.test(w.el.querySelector('.t').textContent))"));
  // Ctrl+Alt+Del: rudolf-display brings the desktop forward and says "taskmgr" through rudolfd's events
  const p2 = await b.newPage(); const e2 = []; p2.on('pageerror', e => e2.push(e.message));
  await p2.addInitScript(() => { window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0};
    window.__PRI = []; const f0 = window.fetch; window.fetch = (u, o) => { u = String(u); if (u.includes('/api/desktop/priority')) window.__PRI.push(((o && o.method) || 'GET') + ' ' + u.split('/api/')[1]); if (u.includes('/api/')) return Promise.resolve(new Response(JSON.stringify({ok: true, items: [], apps: [], windows: [], events: ''}), {headers: {'content-type': 'application/json'}})); return f0(u, o) } });
  await p2.goto('http://127.0.0.1:8172/'); await p2.waitForFunction(() => typeof evDispatch == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  await p2.evaluate(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS.length=0;ACCTS.push({n:'ann',h:await pwMake('a1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'a1');return 1})()`);
  await wait(800); await p2.evaluate("evDispatch('osd volume 40\\ntaskmgr\\n')"); await wait(800);
  ok('3. Ctrl+Alt+Del (the "taskmgr" event from rudolf-display) opens Task Manager', await p2.evaluate("wins.some(w=>/Task Manager/.test(w.el.querySelector('.t').textContent))"));
  ok('3b. ...above every window (Always on top), with the desktop put first', await p2.evaluate("__tmW.el.classList.contains('ontop')&&__PRI.includes('POST desktop/priority?on=1')"), await p2.evaluate('__PRI'));
  ok('3c. ...opened once, not twice', await p2.evaluate("wins.filter(w=>/Task Manager/.test(w.el.querySelector('.t').textContent)).length==1"));
  await p2.evaluate("launch('explorer')"); await wait(500);
  ok('3d. another window opened afterwards stays under it', await p2.evaluate("(()=>{const o=wins.filter(w=>w!==__tmW);return o.length>0&&o.every(w=>(+w.el.style.zIndex||0)<(+__tmW.el.style.zIndex||0))})()"), await p2.evaluate("wins.map(w=>[w.el.querySelector('.t').textContent,w.el.style.zIndex])"));
  await p2.evaluate("__tmW.close()"); await wait(400);
  ok('3e. closing it gives the desktop its usual share again', await p2.evaluate("__PRI.includes('POST desktop/priority?on=0')"), await p2.evaluate('__PRI'));
  await p2.evaluate("launch('taskmgr')"); await wait(400);
  ok('3f. opened from the Start menu or the right-click menu it is an ordinary window', await p2.evaluate("!__tmW.el.classList.contains('ontop')"));
  await p2.evaluate("__tmW.close()"); await wait(300);
  await p2.keyboard.down('Control'); await p2.keyboard.down('Shift'); await p2.keyboard.press('Escape'); await p2.keyboard.up('Shift'); await p2.keyboard.up('Control'); await wait(500);
  ok('3g. Ctrl+Shift+Esc in the page opens it the same way', await p2.evaluate("!!(__tmW&&wins.includes(__tmW)&&__tmW.el.classList.contains('ontop'))"));
  ok('4. ...and Help\'s keyboard shortcuts list it', await p2.evaluate("HELP.find(h=>h[0]=='keys')[2]().includes('Ctrl+Alt+Del')"));
  ok('no page error (with rudolfd)', !e2.length, e2);
  const C = fs.readFileSync(path.join(__dirname, '../../sys/rudolf-display.c'), 'utf8');
  ok('5. rudolf-display: Ctrl+Alt+Del brings the desktop forward, says "taskmgr", and does not pass Delete on', /c == KEY_DELETE && mod_ctrl && mod_alt[\s\S]{0,200}wm_set\(0\)[\s\S]{0,200}taskmgr\\n[\s\S]{0,80}return;/.test(C));
  ok('5b. rudolf-display: Ctrl+Shift+Esc too, and both put the desktop first', /c == KEY_ESC && mod_ctrl && shift_down/.test(C) && /desktop-priority", "on"/.test(C));
  const D = fs.readFileSync(path.join(__dirname, '../../sys/rudolfd.c'), 'utf8');
  ok('5c. rudolfd: desktop/priority runs desktop-priority on or off', /"desktop\/priority"[\s\S]{0,1200}desktop-priority on[\s\S]{0,100}desktop-priority off/.test(D));
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
