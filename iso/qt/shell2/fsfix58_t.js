// 1.5.8: nothing full screen hides Task Manager or the power dialog (they ask the shell to leave full screen first),
// the shell leaves full screen when a tab's page ends while full screen, and each Settings page gets a fresh box, so
// no page keeps listening once another is shown.   node fsfix58_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 300) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8266, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto('http://127.0.0.1:8266/'); await p.waitForFunction(() => typeof fsLeave == 'function' && typeof settings == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});window.__sent=[];SHELL={postMessage:m=>__sent.push(m)};return 1})()`);
  await q('(()=>{tmUrgent();return 1})()'); await wait(300);
  ok('1. Task Manager by its shortcut asks the shell to leave full screen first', await q("__sent.some(m=>m.rudolfShell=='tab'&&m.op=='unfs')"), await q('__sent'));
  await q('(()=>{__sent.length=0;shutdownDlg();return 1})()'); await wait(200);
  ok('2. ...and so does the power dialog', await q("__sent.some(m=>m.op=='unfs')&&!!document.querySelector('.rmodal')"));
  await q("(()=>{document.querySelectorAll('.rmodal').forEach(o=>o.remove());return 1})()");
  const sh = fs.readFileSync(path.join(__dirname, '../../shell/shell.js'), 'utf8');
  ok('3. the shell: a page that ends while full screen leaves it, and unfs leaves full screen', /addEventListener\('exit', e => \{ if \(FS_TAB === t \|\| document\.fullscreenElement === f\) document\.exitFullscreen\(\)/.test(sh) && /m\.op == 'unfs'.*exitFullscreen/.test(sh));
  // Settings: Display Properties' listeners do not follow to the next page
  await q("(()=>{settings('disp');return 1})()"); await wait(600);
  const box1 = await q("(()=>{window.__b1=SW.body.querySelector('.sc');return !!__b1})()");
  await q("(()=>{SW.show('vol',true);return 1})()"); await wait(400);
  ok('4. each Settings page gets a fresh box', box1 && await q("SW.body.querySelector('.sc')!==__b1&&!__b1.isConnected"));
  await q("(()=>{const c=SW.body.querySelector('.sc');const i=c.querySelector('input,select');if(i)i.dispatchEvent(new Event('input',{bubbles:true}));c.dispatchEvent(new Event('input',{bubbles:true}));return 1})()"); await wait(200);
  ok('5. ...so an input on another page does not run Display Properties\' handler (no "Cannot read properties of null")', !errs.some(e => /reading 'value'/.test(e)), errs);
  await q("(()=>{SW.show('disp',true);return 1})()"); await wait(500);
  ok('6. Display Properties still works when shown again', await q("!!SW.body.querySelector('.sc #bs')&&!!SW.body.querySelector('.sc #dynw')"));
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
