// 1.5.8: Make a stick makes a full rescue stick (its system kept a second time as a rescue system, the room checked
// before anything is erased, a half-made stick cleared), Settings says what a plugged-in stick holds, Settings ›
// Update says when the rescue system is running, and Help explains it.   node stick58_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 300) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8264, '127.0.0.1');
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto('http://127.0.0.1:8264/'); await p.waitForFunction(() => typeof stkInfoText == 'function' && typeof HELP != 'undefined', null, {timeout: 30000});
  const t = j => p.evaluate(j => stkInfoText(j), j);
  const sys = {slot: 'a', version: '1.5.8', serial: '2026111300'};
  ok('1. a full rescue stick: its system and its rescue system', /full rescue stick, Rudolf OS 1\.5\.8, with its rescue system/.test(await t({rudolf: true, info: {bootonly: true, image: {version: '1.5.8'}, system: sys, rescue: {version: '1.5.8'}, canStart: true}})));
  ok('2. a stick made before 1.5.8: no rescue system, make it again', /without a rescue system.*Make it again/.test(await t({rudolf: true, info: {bootonly: true, image: {}, system: sys, rescue: null, canStart: true}})));
  ok('3. boot files only, nothing installed: it cannot start on its own', /boot files only.*cannot start on its own/.test(await t({rudolf: true, info: {bootonly: true, image: {version: '1.5.7'}, system: null, rescue: null, canStart: false}})));
  ok('4. a whole disc image: it starts on its own', /whole disc image \(Rudolf OS 1\.5\.6\).*starts on its own/.test(await t({rudolf: true, info: {bootonly: false, image: {version: '1.5.6'}, system: null, rescue: null, canStart: true}})));
  ok('5. not a Rudolf OS stick: nothing said', await t({rudolf: false}) === '');
  const src = fs.readFileSync(path.join(__dirname, '../../../desktop/stick30.js'), 'utf8');
  ok('6. Settings: each stick is a full rescue stick, with its rescue system; asks rudolfd what an older stick holds', /full rescue stick/.test(src) && /rescue system/.test(src) && /stick\/info\?disk=/.test(src));
  const h = await p.evaluate("(()=>{const x=HELP.find(h=>h[0]=='rescue');return x?x[2]():''})()");
  ok('7. Help: the rescue system, and the boot-files-only menu title', /rescue system/.test(h) && /boot files: the system installed on this drive/.test(h), h.slice(0, 200));
  const up = fs.readFileSync(path.join(__dirname, '../../../desktop/sysupd25.js'), 'utf8');
  ok('8. Settings › Update: says when the rescue system is running', /s\.booted=='r'/.test(up) && /Rescue system:/.test(up));
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
