// 1.6.3 (the owner, 2026-11-28): the setup wizard keeps its place when the desktop starts again or the computer is switched
// off in the middle of setup: the step and the name typed so far come back (bd95oobe) instead of Step 1. The password is
// never kept, so a restart after it is typed comes back to the step that asks for it, with the name still in its box; and
// once the wizard ends, nothing is kept.   node setupkeep63_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www'), PORT = 8766;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 700) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(PORT, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const p = await (await b.newContext({viewport: {width: 1024, height: 768}})).newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message));
  const step = async () => ((await p.textContent('#oobe .oostep')) || '').trim();
  const name = () => p.evaluate(() => { const n = document.querySelector('#oobe .nm'); return n ? n.value : null });
  const kept = () => p.evaluate(() => { try { return localStorage.getItem('bd95oobe') } catch (e) { return null } });
  const open = async () => { await p.waitForSelector('#oobe', {timeout: 30000}); await wait(300) };
  const nx = async () => { await p.click('#oobe .nx'); await wait(400) };
  const pw = async () => { await p.click('#oobe .p1'); await p.keyboard.type('abcd'); await p.click('#oobe .p2'); await p.keyboard.type('abcd') };
  const again = async () => { await p.reload(); await open() };   // (the desktop started again, or the computer was switched off)

  await p.goto(`http://127.0.0.1:${PORT}/`); await open();
  ok('1. a new installation: the wizard begins at step 1', await step() == 'Step 1 of 5', await step());
  await nx(); await nx();   // Welcome, Make it comfortable, Create your account
  ok('2. ...and reaches the account step', await step() == 'Step 3 of 5', await step());
  await p.click('#oobe .nm'); await p.keyboard.type('Ann'); await wait(400);
  ok('3. the step and the name typed are kept as they are made up', await kept() == '{"step":2,"name":"Ann"}', await kept());
  await again();
  ok('4. the desktop starts again: back at the account step, the name already in its box', await step() == 'Step 3 of 5' && await name() == 'Ann', [await step(), await name()]);
  ok('5. ...and both password boxes empty: the password is never kept', await p.evaluate(() => document.querySelector('#oobe .p1').value == '' && document.querySelector('#oobe .p2').value == ''), await kept());
  await pw(); await nx();
  ok('6. ...on to Privacy', await step() == 'Step 4 of 5', await step());
  await again();
  ok('7. after the password step, a restart comes back to the step that asks for it (not step 1, never the last one)', await step() == 'Step 3 of 5' && await name() == 'Ann', [await step(), await name()]);
  await p.click('#oobe .bk'); await wait(400);
  ok('8. Back keeps its place too', await step() == 'Step 2 of 5', await step());
  await again();
  ok('9. ...still there after a restart', await step() == 'Step 2 of 5', await step());
  await nx();   // (the account step again: the name is still filled in)
  ok('10. ...and the name comes back with it', await name() == 'Ann', await name());
  await pw(); await nx(); await nx(); await nx(); await wait(1000);   // Privacy, All set, Finish
  ok('11. Finish makes the account and clears the place', await p.evaluate(() => !document.getElementById('oobe') && ACCTS[0].n == 'Ann' && localStorage.getItem('bd95oobe') == null),
    await p.evaluate(() => [!!document.getElementById('oobe'), ACCTS[0].n, localStorage.getItem('bd95oobe')]));
  await p.reload(); await wait(2500);
  ok('12. ...so no wizard comes back on the next start', await p.evaluate(() => !document.getElementById('oobe')));
  ok('no page error', !errs.length, errs);
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed');
  await b.close(); srv.close(); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.log('FAIL ' + e.message); process.exit(1) });
