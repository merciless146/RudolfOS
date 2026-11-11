// 1.5.7: "Force quit" on an app's right-click menu, below Close: a desktop app's window (all its windows closed at once),
// a Linux or Windows program's taskbar button (its whole app, background processes included, by the window's id; the
// program behind the window when it is not a listed app), the Start menu (a Linux app that runs), and Task Manager's
// Apps.   node forcequit57_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 400) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8177, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.addInitScript(() => { window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0}; window.CALLS = [];
    window.RUNNING = [{id: 'wine:123', name: 'StarportGE.exe', kind: 'wine', rss: 4e8, window: true, wins: ['2a00002']},
      {id: 'flatpak:com.discordapp.Discord', app: 'com.discordapp.Discord', name: 'Discord', kind: 'flatpak', rss: 3e8, window: false, wins: []}];
    const f0 = window.fetch; window.fetch = (u, o) => { u = String(u); if (u.includes('/api/')) { const a = u.split('/api/')[1]; CALLS.push(((o && o.method) || 'GET') + ' ' + a);
      const body = a.startsWith('apps/running') ? {apps: RUNNING} : a.startsWith('apps/end') ? {processes: 2, forced: true} :
        a.startsWith('apps/windows') ? {windows: [{win: '2a00002', web: false, shown: false, fullscreen: false, class: 'starportge.exe', instance: 'starportge.exe', title: 'Starport GE'}, {win: '2b00003', web: false, shown: false, fullscreen: false, class: 'xterm', instance: 'xterm', title: 'xterm'}], mode: 'desktop'} :
        {items: [], apps: [], windows: [], events: []};
      return Promise.resolve(new Response(JSON.stringify(Object.assign({ok: true}, body)), {headers: {'content-type': 'application/json'}})) } return f0(u, o) } });
  await p.goto('http://127.0.0.1:8177/'); await p.waitForFunction(() => typeof fqWinItem == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  await p.evaluate(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS.length=0;ACCTS.push({n:'ann',h:await pwMake('a1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'a1');return 1})()`);
  await wait(800);
  const menu = () => p.evaluate("[...document.querySelectorAll('.cmenu div, .ctx div, .cmenu li, .ctx .cmi')].filter(e=>!e.querySelector('div')).map(e=>e.textContent.trim()).filter(Boolean)");
  const click = t => p.evaluate(`(()=>{const e=[...document.querySelectorAll('.cmenu div, .ctx div, .cmenu li, .ctx .cmi')].filter(e=>!e.querySelector('div')).find(e=>e.textContent.trim().includes(${JSON.stringify(t)}));e&&e.click();return !!e})()`);
  // a desktop app: two Notepad windows
  await p.evaluate("launch('notepad');launch('notepad')"); await wait(600);
  const n0 = await p.evaluate("wins.filter(w=>w.app=='notepad').length");
  await p.evaluate("(()=>{const W=wins.find(w=>w.app=='notepad');winMenu(W,300,300);return 1})()"); await wait(300);
  let m = await menu(); const ci = m.findIndex(t => /^✕?\s*Close/.test(t)), fi = m.findIndex(t => /Force quit Notepad/.test(t));
  ok('1. a desktop app\'s window menu: "Force quit Notepad" just below Close', n0 == 2 && ci >= 0 && fi == ci + 1, m);
  await click('Force quit Notepad'); await wait(400);
  ok('2. ...it closes all of Notepad\'s windows at once', await p.evaluate("wins.filter(w=>w.app=='notepad').length==0"));
  // a Windows program's taskbar button
  await p.evaluate("lxWinPoll()"); await wait(800);
  await p.evaluate("(()=>{const b=document.querySelector('.lxtb[data-win=\"2a00002\"]');b.dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,clientX:200,clientY:700}));return 1})()"); await wait(300);
  m = await menu();
  ok('3. a program\'s taskbar button: Show, Close window, then Force quit', m.some(t => /Close window/.test(t)) && m.findIndex(t => /Force quit/.test(t)) == m.findIndex(t => /Close window/.test(t)) + 1, m);
  await p.evaluate("CALLS.length=0"); await click('Force quit'); await wait(800);
  ok('4. ...it force-quits its whole app (all its processes, by the window\'s id)', await p.evaluate("CALLS.includes('POST apps/end?id=wine%3A123&force=1')"), await p.evaluate('CALLS'));
  await p.evaluate("(()=>{const b=document.querySelector('.lxtb[data-win=\"2b00003\"]');b.dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,clientX:200,clientY:700}));return 1})()"); await wait(300);
  await p.evaluate("CALLS.length=0;FQ57.list=null"); await click('Force quit'); await wait(800);
  ok('5. a window that is not a listed app: the program behind it is ended', await p.evaluate("CALLS.some(x=>/apps\\/kill\\?win=2b00003/.test(x))"), await p.evaluate('CALLS'));
  // the Start menu: a Linux app running in the background
  await p.evaluate("regApp('lx:discord',{name:'Discord',icon:'🐧',lxApp:'com.discordapp.Discord',open:()=>{}})");
  await p.evaluate("FQ57.list=null;fqList()"); await wait(400);
  const it = await p.evaluate("(()=>{const i=fqAppItem('lx:discord');return i&&[i[0],i[2]]})()");
  ok('6. the Start menu: "Force quit Discord" for a Linux app running in the background', it && it[0] == 'Force quit Discord' && it[1] === true, it);
  await p.evaluate("CALLS.length=0;fqAppItem('lx:discord')[1]()"); await wait(800);
  ok('7. ...it force-quits it', await p.evaluate("CALLS.includes('POST apps/end?id=flatpak%3Acom.discordapp.Discord&force=1')"), await p.evaluate('CALLS'));
  // Task Manager › Apps
  const tm = await p.evaluate("(()=>{const d=document.createElement('div');tmApps57(d,RUNNING,null);return [...d.querySelectorAll('button')].map(b=>b.textContent)})()");
  ok('8. Task Manager\'s Apps: End and Force quit for each app', tm.filter(t => t == 'Force quit').length == 2 && tm.filter(t => t == 'End').length == 2, tm);
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
