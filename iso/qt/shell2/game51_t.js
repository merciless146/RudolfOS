// 1.5.1 (game51.js): Xbox Cloud Gaming and GeForce NOW in the Start menu, as apps and in the Games tab; game mode holds
// notifications while a real tab is full screen and says how many came afterwards. node game51_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 300) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8099, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto('http://127.0.0.1:8099/'); await p.waitForFunction(() => typeof gameMode == 'function' && typeof fs != 'undefined' && fs['C:'], null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});USER=USER||ACCTS[0];return 1})()");
  const menu = await q("[...document.querySelectorAll('#menu li')].map(l=>l.dataset.a)");
  ok('1. the Start menu has Xbox Cloud Gaming and GeForce NOW', menu.includes('xcloud') && menu.includes('gfn'), menu);
  ok('2. both are apps, first in the Games tab list', await q("!!(APPS.xcloud&&APPS.gfn)&&GAMES_BUILTIN[0][0]=='gfn'&&GAMES_BUILTIN[1][0]=='xcloud'"));
  const opened = await q("(()=>{let u=null;const o=browser;browser=x=>{u=x};launch('xcloud');browser=o;return u})()");
  ok('3. Xbox Cloud Gaming opens xbox.com/play in Oracle', opened == 'https://www.xbox.com/play', opened);
  await q("NC.length=0;1");
  await q("window.postMessage({rudolfShell:'tabev',tid:7,ev:'fs',on:true},'*')"); await wait(200);
  ok('4. a real tab full screen is game mode', await q("gameMode()"));
  await q("notify('Mail','A new message');notify('Update','Ready to install');1"); await wait(200);
  const t = await q("[document.querySelectorAll('.ntoast').length, NC.length]");
  ok('5. notifications wait in the centre: no pop-up', t[0] == 0 && t[1] == 2, t);
  await q("notify('Disk','Something failed',{kind:'error'});1"); await wait(200);
  ok('6. ...but errors still show', await q("[...document.querySelectorAll('.ntoast')].some(x=>/Something failed/.test(x.textContent))"));
  await q("window.postMessage({rudolfShell:'tabev',tid:7,ev:'fs',on:false},'*')"); await wait(300);
  ok('7. afterwards one notification says how many came', await q("!gameMode()&&[...document.querySelectorAll('.ntoast')].some(x=>/2 notifications came in/.test(x.textContent))"));
  await q("notify('Later','Normal again');1"); await wait(200);
  ok('8. out of game mode, notifications pop up again', await q("[...document.querySelectorAll('.ntoast')].some(x=>/Normal again/.test(x.textContent))"));
  ok('9. no page errors', !errs.length, errs);
  console.log(`${R.filter(x => x).length}/${R.length} passed`); await b.close(); srv.close(); process.exit(R.every(x => x) ? 0 : 1);
})();
