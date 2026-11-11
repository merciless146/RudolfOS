// 1.5.7: Task Manager › Processes: End process tree (as in Windows XP's Task Manager), beside End process and End now,
// and the same three on a process's right-click menu.   node tmtree57_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 400) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8176, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.addInitScript(() => { window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0}; window.CALLS = [];
    const P = [{pid: 100, ppid: 1, user: 'rudolf', name: 'steam', cmd: 'steam', tag: 'app', cpu: 1, rss: 300e6, prot: 0},
      {pid: 101, ppid: 100, user: 'rudolf', name: 'steamwebhelper', cmd: 'steamwebhelper', tag: 'app', cpu: 1, rss: 200e6, prot: 0},
      {pid: 102, ppid: 101, user: 'rudolf', name: 'steamwebhelper', cmd: 'steamwebhelper --type=gpu', tag: 'app', cpu: 1, rss: 100e6, prot: 0},
      {pid: 200, ppid: 1, user: 'root', name: 'rudolfd', cmd: 'rudolfd', tag: 'system', cpu: 0, rss: 5e6, prot: 1}];
    const f0 = window.fetch; window.fetch = (u, o) => { u = String(u); if (u.includes('/api/')) { const a = u.split('/api/')[1]; CALLS.push(((o && o.method) || 'GET') + ' ' + a);
      const body = a.startsWith('processes') ? {items: P} : a.startsWith('proc/end') ? {message: 'steam', ended: 3, kept: 0} : {items: [], apps: [], windows: [], events: []};
      return Promise.resolve(new Response(JSON.stringify(Object.assign({ok: true}, body)), {headers: {'content-type': 'application/json'}})) } return f0(u, o) } });
  await p.goto('http://127.0.0.1:8176/'); await p.waitForFunction(() => typeof taskmgr26 == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  await p.evaluate(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS.length=0;ACCTS.push({n:'ann',h:await pwMake('a1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'a1');window.elevate=async()=>true;return 1})()`);
  await wait(800); await p.evaluate("launch('taskmgr')"); await wait(800);
  await p.evaluate("(()=>{const t=[...__tmW.el.querySelectorAll('button,[role=tab],.tmtab,li')].find(e=>/^Processes$/.test(e.textContent.trim()));t&&t.click();return !!t})()"); await wait(1500);
  const has = await p.evaluate("(()=>{const b=[...__tmW.el.querySelectorAll('button')].map(b=>b.textContent);return b})()");
  ok('1. Processes: End process, End process tree and End now', ['End process', 'End process tree', 'End now'].every(x => has.includes(x)), has);
  await p.evaluate("__tmW.el.querySelector('tr[data-pid=\"100\"]').click()"); await wait(200);
  await p.evaluate("[...__tmW.el.querySelectorAll('button')].find(b=>b.textContent=='End process tree').click()"); await wait(500);
  const dlg = await p.evaluate("(()=>{const d=[...document.querySelectorAll('.sdlg,.rmbox,.dlg,[role=dialog]')].map(e=>e.textContent).join(' | ');return d})()");
  ok('2. it says what ends: the process and the 2 processes it started', /End steam \(process 100\) and the 2 processes it started/.test(dlg), dlg.slice(0, 300));
  await p.evaluate("(()=>{const b=[...document.querySelectorAll('button')].filter(b=>b.textContent=='End process tree').pop();b.click()})()"); await wait(800);
  ok('3. it asks rudolfd to end the tree', await p.evaluate("CALLS.some(x=>x=='POST proc/end?pid=100&tree=1')"), await p.evaluate('CALLS.filter(x=>/proc/.test(x))'));
  await p.evaluate("(()=>{const r=__tmW.el.querySelector('tr[data-pid=\"101\"]');r.dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,clientX:400,clientY:300}));return 1})()"); await wait(400);
  const m = await p.evaluate("[...document.querySelectorAll('.cmenu div, .ctx div, .cmenu li, .ctx .cmi')].map(e=>e.textContent.trim()).filter(Boolean)");
  ok('4. the right-click menu has End process, End process tree and End now', ['End process', 'End process tree', 'End now'].every(x => m.some(t => t.startsWith(x))), m);
  await p.keyboard.press('Escape'); await wait(200);
  await p.evaluate("__tmW.el.querySelector('tr[data-pid=\"200\"]').click()"); await wait(200);
  ok('5. a process Rudolf OS needs cannot be ended, tree or not', await p.evaluate("[...__tmW.el.querySelectorAll('button')].filter(b=>/^End (process|process tree|now)$/.test(b.textContent)).every(b=>b.disabled)"));
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
