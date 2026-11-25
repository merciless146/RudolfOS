// 1.6.3 Part 8, item 5 ("'Make a stick' needs further refinement and to be moved from update/recovery to backup settings"):
// the page's Settings (Chromium mode and Lite; Native's own Settings pages are Part 3's), with rudolfd's answers for two
// plugged-in sticks (a 16 GB SanDisk, a 2 GB one) and a 3.2 GB disc image in Downloads given by the test:
//  1. Storage and backup: Make a Rudolf OS USB stick is in its Backup part (after Backup's ways, before the next section);
//  2. Update and recovery keeps a link there, which opens Storage and backup at Make a stick;
//  3. Search in Settings finds Make a Rudolf OS USB stick in Storage and backup;
//  4. each stick shown with its name, model and size;
//  5. each choice (this Rudolf OS, a disc image) said in plain words;
//  6. a stick too small for the chosen image refused before anything is written (its button off, why said, no mkstick);
//  7. while it is made: a bar with the step and the time left;
//  8. at the end: what was checked, and that the stick is safe to remove.
//   NODE_PATH=... node stick163_t.js   (it builds nothing: the page from iso/www, served on port 8745)
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path'), cp = require('child_process'), os = require('os');
const ROOT = path.resolve(__dirname, '../../..'), PORT = +(process.env.STK_PORT || 8745), BASE = 'http://127.0.0.1:' + PORT;
const wait = ms => new Promise(r => setTimeout(r, ms));
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 600) : '')) };
const D = fs.mkdtempSync(path.join(os.tmpdir(), 'stick163-'));
cp.execFileSync('node', [path.join(ROOT, 'iso/qt/host/fixtures/deskls.js'), path.join(D, 'ls.json'), path.join(D, 'keys.json')]);
const KEYS = JSON.parse(fs.readFileSync(path.join(D, 'keys.json'))), PW = KEYS.Ann.pw;
const RD = cp.spawn('python3', [path.join(ROOT, 'iso/qt/native/same_rd.py'), String(PORT), D], {stdio: 'ignore', cwd: ROOT});
const DISKS = {disks: [{name: 'sda', removable: false, bytes: 256e9, model: 'Internal SSD', partitions: [{name: 'sda1', fs: 'iso9660', bytes: 3e9}]},
  {name: 'sdb', removable: true, bytes: 16e9, model: 'SanDisk Ultra', partitions: [{name: 'sdb1', fs: 'vfat', label: 'STICK', bytes: 16e9}]},
  {name: 'sdc', removable: true, bytes: 2.0e9, model: 'Tiny Stick', partitions: [{name: 'sdc1', fs: 'vfat', label: 'TINY', bytes: 2.0e9}]}]};
const IMGS = {ok: true, images: [{name: 'rudolf-os-1.6.2.iso', path: '/persist/accounts/x/Downloads/rudolf-os-1.6.2.iso', bytes: 3.2e9}]};
const LOG = ['Step 1/5: copying Rudolf OS (3000 MiB) to /dev/sdb ...', 'Step 2/5: checking the copy ...', 'The copy matches (SHA-256 0123456789abcdef).', 'Step 3/5: removing old partition tables ...',
  'Step 4/5: ...', 'Step 5/5: ...', 'Checking the new stick ...', 'Done: the Rudolf OS USB stick is ready on /dev/sdb. You can remove it now.'];
(async () => {
  for (let i = 0; i < 50; i++) { try { await fetch(BASE + '/'); break } catch (e) { await wait(200) } }
  const br = await chromium.launch(); const ctx = await br.newContext({viewport: {width: 1600, height: 1000}, timezoneId: 'UTC'});
  await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => r.abort());
  const calls = []; let jobAt = 0, jobT = 0;
  const j = (r, o) => r.fulfill({status: 200, contentType: 'application/json', body: JSON.stringify(Object.assign({ok: true}, o))});
  await ctx.route(/\/api\/(info|disks|install\/images|sys\/status|mkstick|job|stick\/info)(\?|$)/, r => { const u = new URL(r.request().url()), w = u.pathname.replace('/api/', '');
    calls.push(w + u.search);
    if (w == 'info') return j(r, {persistent: true, mediaDev: '/dev/sda1', bootSecure: 1, updateInstalled: true});
    if (w == 'disks') return j(r, DISKS); if (w == 'install/images') return j(r, IMGS); if (w == 'sys/status') return j(r, {booted: 'disc'});
    if (w == 'mkstick') { jobAt = 0; jobT = Date.now(); return j(r, {}) }
    if (w == 'job') { const n = Math.min(LOG.length, 1 + Math.floor((Date.now() - jobT) / 1500)); jobAt = n; return j(r, {running: n < LOG.length, status: n < LOG.length ? -1 : 0, log: LOG.slice(0, n).join('\n') + '\n'}) }
    return j(r, {}) });
  const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message + ' ' + String(e.stack || '').split('\n').slice(1, 3).join(' ')));
  await p.goto(BASE + '/'); await p.waitForFunction(() => typeof userLogin == 'function' && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
  await wait(800); await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
  await p.waitForFunction(() => USER && getComputedStyle(document.getElementById('login')).display == 'none', null, {timeout: 60000}); await wait(2000);
  await p.evaluate(() => { elevate = async () => true });   /* (the administrator's prompt is not what is tested here) */
  const admin = await p.evaluate(() => isAdmin());
  // 1. where it is
  await p.evaluate(() => settings('stor')); await p.waitForFunction(() => document.querySelector('.stk30 h4'), null, {timeout: 15000}).catch(() => {}); await wait(800);
  const order = await p.evaluate(() => { const W = SW, hs = [...W.el.querySelectorAll('h4')].map(h => h.textContent.trim()); return hs });
  const ib = order.indexOf('Backup'), im = order.indexOf('Make a Rudolf OS USB stick'), ik = order.findIndex((h, i) => i > ib && !/^(Back up|A Rudolf OS stick|Make a Rudolf OS USB stick)/.test(h) && h != 'Backup');
  ok('1. Storage and backup: Make a Rudolf OS USB stick is in its Backup part', ib >= 0 && im > ib && (ik < 0 || im < ik), order);
  // 4. the sticks
  const rows = await p.evaluate(() => [...document.querySelectorAll('.stk30 .stkl .acr')].map(r => r.firstChild.textContent));
  ok('4. each stick with its name, model and size', rows.some(t => /sdb — SanDisk Ultra — 16\.0 GB/.test(t)) && rows.some(t => /sdc — Tiny Stick — 2\.0 GB/.test(t)) && !rows.some(t => /sda/.test(t)), rows);
  // 5. plain words
  const w1 = await p.evaluate(() => document.querySelector('.stk30 .stkw') ? document.querySelector('.stk30 .stkw').textContent : '');
  await p.selectOption('.stk30 .stks', IMGS.images[0].path); await wait(200);
  const w2 = await p.evaluate(() => document.querySelector('.stk30 .stkw') ? document.querySelector('.stk30 .stkw').textContent : '');
  ok('5. each choice said in plain words', /This Rudolf OS as it is on this computer/.test(w1) && /A whole disc image \(rudolf-os-1\.6\.2\.iso\)/.test(w2), {w1, w2});
  // 6. too small
  const small = await p.evaluate(() => { const r = [...document.querySelectorAll('.stk30 .stkl .acr')].find(x => /sdc/.test(x.firstChild.textContent)); const m = r && r.nextElementSibling;
    return {off: r ? r.lastChild.disabled : null, why: m && m.classList.contains('stksm') && !m.hidden ? m.textContent : ''} });
  const c0 = calls.filter(c => c.startsWith('mkstick')).length;
  await p.evaluate(() => { const r = [...document.querySelectorAll('.stk30 .stkl .acr')].find(x => /sdc/.test(x.firstChild.textContent)); r && r.lastChild.click() }); await wait(800);
  const confirmShown = await p.evaluate(() => !!document.querySelector('.natdlg .stki'));
  ok('6. a stick too small for the image: refused before anything is written, and why said', admin && small.off === true && /Too small for rudolf-os-1\.6\.2\.iso/.test(small.why) && !confirmShown && calls.filter(c => c.startsWith('mkstick')).length == c0, {admin, small, confirmShown});
  await p.evaluate(() => document.querySelectorAll('.natdlg').forEach(o => o.remove()));
  // 7, 8. made: progress, time left, checked, safe to remove
  await p.selectOption('.stk30 .stks', ''); await wait(200);
  await p.evaluate(() => { const r = [...document.querySelectorAll('.stk30 .stkl .acr')].find(x => /sdb/.test(x.firstChild.textContent)); r.lastChild.click() });
  await p.waitForSelector('.natdlg .stki', {timeout: 5000}).catch(() => {}); await p.fill('.natdlg .stki', '16.0 GB').catch(() => {}); await p.click('.natdlg .stbtn button:not(.c)').catch(() => {});
  const seen = []; for (let i = 0; i < 30; i++) { await wait(500); const s = await p.evaluate(() => { const b = document.querySelector('.stk30 .stkp'); return b && !b.hidden ? [b.querySelector('progress').value, b.querySelector('.stkpt').textContent] : null }); if (s) seen.push(s); if (s && s[0] >= 100) break }
  ok('7. while it is made: a bar with the step', seen.some(s => s[0] > 0 && s[0] < 100 && /\(\d+ %\)/.test(s[1])) && seen.some(s => /Checking what was written|Checked/.test(s[1])), seen.slice(-6));
  await p.evaluate(() => { const b = document.querySelector('.stk30 .stkp'); if (b) b.dispatchEvent(new Event('x')) });
  const end = await p.evaluate(() => { const o = document.querySelector('.stk30 .stkok'); return o && !o.hidden ? o.textContent : '' });
  ok('8. at the end: what was checked, and the stick safe to remove', /Checked: what is on the stick matches/.test(end) && /safe to remove the stick now/.test(end) && calls.some(c => c.startsWith('mkstick?disk=sdb')), {end, calls: calls.filter(c => /mkstick/.test(c))});
  // the time left (the estimate after 10 s): from the bar's own words with a slower job
  const left = await p.evaluate(() => { if (typeof stkProgress != 'function') return ''; const d = document.createElement('div'); d.innerHTML = "<div class='stkp' hidden><progress max='100'></progress><div class='stkpt'></div></div><p class='stkok'></p>"; document.body.appendChild(d);
    const P = stkProgress(d.querySelector('.stkp'), d.querySelector('.stkok')); const n = Date.now; const t = n(); Date.now = () => t + 60000; P.line('Step 2/5: checking the copy ...'); Date.now = n; const s = d.querySelector('.stkpt').textContent; P.end({status: 0}); d.remove(); return s });
  ok('7b. ...and the time left', /about \d+ minutes? left/.test(left), {left});
  // 2. Update and recovery's link
  await p.evaluate(() => settings('upd')); await wait(2500);
  const link = await p.evaluate(() => { const a = document.querySelector('.stkmoved a'); return a ? a.textContent : '' });
  if (link) { await p.click('.stkmoved a'); await wait(3500) }
  const there = await p.evaluate(() => SW && SW.sec == 'stor' && !!document.querySelector('.stk30 h4') && (() => { const h = document.querySelector('.stk30 h4').getBoundingClientRect(); return h.top >= 0 && h.top < innerHeight })());
  ok('2. Update and recovery: a link to its new place, which opens Storage and backup at Make a stick', link == 'Storage and backup › Backup' && there, {link, there});
  // 3. Search in Settings
  const found = await p.evaluate(async () => { if (typeof SETIDX == 'undefined') return 'no search'; try { const idx = SETIDX; return JSON.stringify(Object.entries(idx || {}).filter(([k, v]) => JSON.stringify(v).includes('Make a Rudolf OS USB stick')).map(([k]) => k)) } catch (e) { return String(e) } });
  ok('3. Search in Settings finds Make a Rudolf OS USB stick in Storage and backup', /"stor"/.test(found) && !/"upd"/.test(found), {found});
  ok('no errors in the page', !errs.length, errs);
  await br.close(); RD.kill(); fs.rmSync(D, {recursive: true, force: true});
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('FAIL the test: ' + e.stack); try { RD.kill() } catch (x) {} process.exit(1) });
