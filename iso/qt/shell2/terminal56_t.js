// 1.5.6: one Terminal (the Command Prompt's ⬛ icon): the Command Prompt is a tab of it (drive C:, also in a web
// browser), prompt() opens a C: tab (its commands, rpk and the Unix tools still work there), EXIT closes the tab;
// the Start menu has one Terminal and no Command Prompt; the Developer Console is out of the Start menu unless
// Settings › Apps turns it on, and is in Task Manager.   node terminal56_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 500) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8144, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto('http://127.0.0.1:8144/'); await p.waitForFunction(() => typeof termUI == 'function' && FSLOADED && fs['C:'] && fs['C:'].USERS, null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>{w.__force=1;w.close()});USER=USER||ACCTS[0];return 1})()");
  const menu = await q("(startMenuApps(),[...document.querySelectorAll('#menu li')].map(l=>l.textContent.trim()))");
  ok('1. the Start menu has one Terminal (⬛), and no Command Prompt, Terminal (Linux) or Developer Console', menu.filter(t => /Terminal/.test(t)).length == 1 && !menu.some(t => /Command Prompt|Rudolf Prompt|Developer Console/.test(t)), menu);
  await q("launch('terminal')"); await wait(600);
  let r = await q("(()=>{const w=wins.at(-1);return {t:w.el.querySelector('.t').textContent,tabs:[...w.body.querySelectorAll('.dttab span')].map(x=>x.textContent),hasTerm:!!w.body.querySelector('.dtbox.dtcmd .term input'),keys:w.body.querySelector('.dtkeys').style.display}})()");
  ok('2. without Linux (a web browser) the Terminal opens on a Command Prompt tab (C:), without the Linux keys row', r.t == 'Command Prompt' && r.tabs.join() == 'C:' && r.hasTerm && r.keys == 'none', r);
  const type = (w, line) => q(`(()=>{const i=wins.at(-1).body.querySelector('.dtbox:not([style*="none"]) .term input')||wins.at(-1).body.querySelector('.term input');i.value=${JSON.stringify(line)};i.dispatchEvent(new KeyboardEvent('keydown',{key:'Enter',bubbles:true}));return 1})()`);
  await type(0, 'VER'); await wait(200);
  ok('3. its commands work (VER)', /Rudolf OS \[Version/.test(await q("wins.at(-1).body.querySelector('.dtbox .term').innerText")));
  await type(0, 'help'); await wait(200);
  ok('...and rpk is offered there (HELP)', /RPK/.test(await q("wins.at(-1).body.querySelector('.dtbox .term').innerText")));
  await type(0, 'echo hi | wc -c'); await wait(600);
  ok('...and the Unix tools (a pipe)', /\b3\b/.test(await q("wins.at(-1).body.querySelector('.dtbox .term').innerText")), await q("wins.at(-1).body.querySelector('.dtbox .term').innerText.slice(-200)"));
  await q("wins.at(-1).body.querySelector('.dtnew').click()"); await wait(300);
  ok('4. ＋ opens another C: tab (no Linux here)', (await q("[...wins.at(-1).body.querySelectorAll('.dttab span')].map(x=>x.textContent).join()")) == 'C:,C:');
  await type(0, 'EXIT'); await wait(300);
  ok('5. EXIT closes its tab, not the window', (await q("[...wins.at(-1).body.querySelectorAll('.dttab')].length")) == 1 && await q("wins.at(-1).el.isConnected"));
  const n0 = await q("wins.length");
  await q("(()=>{const P=prompt();P.print('PRINTED');return 1})()"); await wait(300);
  r = await q("(()=>{const w=wins.at(-1);return {t:w.el.querySelector('.t').textContent,txt:w.body.querySelector('.dtbox .term').innerText,icon:w.icon}})()");
  ok('6. prompt() (Lua, .BAT files) opens a Terminal with a C: tab and prints there', (await q("wins.length")) == n0 + 1 && /PRINTED/.test(r.txt), r);
  await q("(()=>{fs['C:'].PROGRAMS=fs['C:'].PROGRAMS||{};fs['C:'].PROGRAMS['T56.LUA']='print(\"lua ok \"..(2+3))';openPath(['C:','PROGRAMS','T56.LUA']);return 1})()"); await wait(600);
  ok('7. opening a .LUA file runs it in a Terminal C: tab', /lua ok 5/.test(await q("wins.at(-1).body.querySelector('.dtbox .term').innerText")));
  const desk = await q("(()=>{const d=[...document.querySelectorAll('#desk > .ic')].find(x=>/^Terminal$/.test((x.querySelector('span')||{}).textContent));return d?{icon:d.querySelector('b').textContent||(d.querySelector('b img')||{}).dataset?.emo||''}:null})()");
  ok('8. the desktop icon is Terminal, with the Command Prompt\'s icon', desk && /⬛/.test(desk.icon || '⬛'), desk);
  await q("svj('bd95devtools',true)");
  const menu2 = await q("(startMenuApps(),[...document.querySelectorAll('#menu li')].map(l=>l.textContent.trim()))");
  ok('9. "Show developer tools" puts the Developer Console in the Start menu', menu2.some(t => /Developer Console/.test(t)), menu2);
  await q("svj('bd95devtools',false);launch('taskmgr')"); await wait(800);
  await q("wins.at(-1).body.querySelector('.tmdev').click()"); await wait(400);
  ok('10. Task Manager has a Developer Console button that opens it', /Developer Console/.test(await q("wins.at(-1).el.querySelector('.t').textContent")));
  ok('11. no page errors', !errs.length, errs);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
