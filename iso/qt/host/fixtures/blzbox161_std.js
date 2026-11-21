// 1.6.1 (docs/plan-1.6.1.md Part 27): blzbox161_t.sh's Standard half: the desktop page in Chromium (Playwright) from the stand-in
// rudolfd (native_rd.py, with blzbox161_inject.js as DIR/inject.js), Ann signed in, __blzT's scenario run; its results printed as
// PASS/FAIL lines.   node blzbox161_std.js BASE PASSWORD
const {chromium} = require('playwright');
const [BASE, PW] = process.argv.slice(2);
(async () => {
  const b = await chromium.launch({executablePath: process.env.CHROME || undefined}); const ctx = await b.newContext({viewport: {width: 1280, height: 800}});
  await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => r.abort());
  const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto(BASE + '/'); await p.waitForFunction(() => typeof userLogin == 'function' && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
  await p.waitForTimeout(800); await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
  await p.waitForFunction(() => USER && getComputedStyle(document.getElementById('login')).display == 'none', null, {timeout: 60000});
  await p.waitForTimeout(1500); await p.evaluate(() => wins.slice().forEach(w => w.close()));
  await p.exposeFunction('__tkey', async k => { for (const x of k.split(' ')) await p.keyboard.press(x.replace(/^shift\+/i, 'Shift+').replace(/^Return$/, 'Enter')) });   // (the person's keys: Part 26)
  await p.evaluate(() => __blzT.start());
  await p.waitForFunction(() => __blzT.res.done, null, {timeout: 240000}).catch(() => 0);
  const r = await p.evaluate(() => __blzT.res);
  for (const [n, c, d] of r.r) console.log((c ? 'PASS ' : 'FAIL ') + 'Standard: ' + n + (c ? '' : ' - ' + d));
  if (!r.done) console.log('FAIL Standard: the scenario did not finish');
  const pe = errs.filter(e => !/Failed to fetch|NetworkError|Load failed/.test(e));
  console.log((pe.length ? 'FAIL ' : 'PASS ') + 'Standard: no page error' + (pe.length ? ' - ' + pe.slice(0, 3).join(' | ') : ''));
  await b.close();
})().catch(e => { console.log('FAIL Standard: ' + e); process.exit(1) });
