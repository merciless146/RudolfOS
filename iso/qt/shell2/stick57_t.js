// 1.5.7: Make a Rudolf OS USB stick says it is a rescue stick (Recovery mode on it starts this computer's Rudolf OS with
// safe settings; a normal start, the stick's own) and Help explains starting from one.   node stick57_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 300) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8178, '127.0.0.1');
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto('http://127.0.0.1:8178/'); await p.waitForFunction(() => typeof HELP != 'undefined' && typeof stkTargets == 'function', null, {timeout: 30000});
  const h = await p.evaluate("(()=>{const x=HELP.find(h=>h[0]=='rescue');return x?x[2]():''})()");
  ok('1. Help: starting from a USB stick: Make a stick or a full image in DD mode, Fast Boot, the boot-menu keys', /Make a Rudolf OS USB stick/.test(h) && /DD mode/.test(h) && /Fast Boot/.test(h) && /F8/.test(h), h.slice(0, 200));
  ok('2. ...Recovery mode on a stick starts this computer\'s own Rudolf OS with safe settings; a normal start, the stick\'s own', /Recovery mode.*this computer's own Rudolf OS/.test(h) && /stick's own Rudolf OS/.test(h));
  const src = fs.readFileSync(path.join(__dirname, '../../../desktop/stick30.js'), 'utf8');
  ok('3. Settings: the stick is described as a rescue stick, and a warning when its boot files are older', /rescue stick/.test(src) && /Warning: the stick gets this drive/.test(src));
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
