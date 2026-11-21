// 1.6.0 (plan-1.6.1 Part 16, moved to 1.6.0): Settings › Update and recovery's Check this address and Save (an
// administrator's name and password sent; another build's address asked about first), Recovery mode's line, and Make a
// stick offering the update address's release first (another build said so before anything is erased). A stand-in rudolfd.
//   node ownbuilds60_t.js [CHROME]   (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 500) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8148, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.addInitScript(() => {
    window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0};
    window.T = {calls: []};
    const OTHER = {ok: true, good: true, key: 'other', fpr: '2b75 f857 8810 d850', version: '1.6.0-alex', serial: 2026120300, lines: [['ok', 'The address answers, with a release list.'], ['warn', 'This is another build of Rudolf OS, signed with key 2b75 f857 8810 d850: updates from it do not install on this computer. You can make a USB stick from it.']]};
    const f0 = window.fetch;
    window.fetch = (u, o) => { u = String(u); const q = new URL(u, location.href), J = x => Promise.resolve(new Response(JSON.stringify(Object.assign({ok: true}, x)), {headers: {'content-type': 'application/json'}}));
      if (u.includes('/api/')) T.calls.push([q.pathname.slice(5) + q.search, o && o.body || '']);
      if (q.pathname == '/api/sys/status') return J({persistent: true, booted: 'b', state: {good: 'b', prev: 'a'}, slots: {a: {complete: true}, b: {complete: true}, r: {}}, signer: 'official', developer: false, checked: 0, available: null, toStable: false, auto: true, url: 'https://alex.github.io/RudolfOS/stable', current: {serial: 1, version: '1.6.0'}, canUpdate: true, beta: false, recoveryOfficial: true, recoveryVersion: '1.6.0', recoverySerial: 2026112000, recoveryAt: 1795100000});
      if (q.pathname == '/api/sys/urlcheck') return (o && o.method) == 'POST' ? J(OTHER) : J({ok: false, error: 'use POST'});   /* (as rudolfd: urlcheck is past its POST-only line; 1.6.1: the page sent GET, the owner's Make the stick… said "use POST") */
      if (q.pathname == '/api/disks') return J({disks: [{name: 'sdc', removable: true, model: 'SanDisk', bytes: 64e9, partitions: [{name: 'sdc1', fs: 'vfat', bytes: 64e9}]}]});
      if (q.pathname == '/api/info') return J({persistent: true});
      if (u.includes('/api/')) return J({});
      return f0(u, o) } });
  await p.goto('http://127.0.0.1:8148/'); await p.waitForFunction(() => typeof sysuStatus == 'function' && IDB && IDB.db, null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    ACCTS.length=0;ACCTS.push({n:'boss',h:await pwMake('b1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'b1');
    window.EL=[];window.elevate=async(t,o)=>{EL.push(t);return o&&o.cred?{n:'boss',p:'b1'}:true};return 1})()`);
  await q("settings('upd')"); await wait(1500);
  const txt = await q("document.querySelector('.uadv')?document.querySelector('.uadv').parentElement.textContent:''");
  ok('1. Recovery mode\'s line: the official Rudolf OS 1.6.0, kept up to date from the official address', /Recovery mode \(in the start-up menu\) starts the official Rudolf OS 1\.6\.0/.test(txt) && /whatever the update address/.test(txt), txt.slice(0, 300));
  await q("document.querySelector('.uadv').open=true;document.querySelector('.uurl').value='https://alex.github.io/RudolfOS/stable';document.querySelector('.uchk').click()"); await wait(500);
  const ck = await q("document.querySelector('.uck').textContent");
  ok('2. Check this address: its lines, and whose key', /another build of Rudolf OS/.test(ck) && /Key: 2b75 f857 8810 d850 \(another build\)/.test(ck), ck);
  await q("document.querySelector('.usave').click()"); await wait(500);
  const asked = await q("(()=>{const m=document.querySelector('.rmodal p');return m?m.textContent:''})()");
  ok('3. Save: another build\'s address asked about first (updates will stop here)', /another build of Rudolf OS \(key 2b75 f857 8810 d850\)/.test(asked) && /updates will stop here/.test(asked), asked);
  await q("document.querySelector('.rmodal .y').click()"); await wait(600);
  const set = await q("T.calls.filter(c=>c[0].startsWith('sys/set'))");
  ok('4. ...then saved with an administrator\'s name and password', set.length == 1 && /k=url&v=https%3A%2F%2Falex\.github\.io%2FRudolfOS%2Fstable/.test(set[0][0]) && JSON.parse(set[0][1]).n == 'boss', set);
  // Make a stick
  await q("settings('stor')"); await wait(1500);
  const opts = await q("[...document.querySelectorAll('.stks option')].map(o=>[o.value,o.textContent])");
  ok('5. Make a stick: the update address\'s latest release offered first', opts.length >= 2 && opts[0][0] == '@address' && /alex\.github\.io\/RudolfOS\/stable/.test(opts[0][1]), opts);
  await q("EL.length=0;document.querySelector('.stks').value='@address';[...document.querySelectorAll('.acr button')].find(b=>/Make the stick/.test(b.textContent)).click()"); await wait(800);
  const el = await q('EL');
  ok('6. ...another person\'s build: said so (its key, its updates from there, Secure Boot, the official Recovery mode) before the erase question', /another build of Rudolf OS \(Rudolf OS 1\.6\.0-alex, key 2b75 f857 8810 d850\)/.test(el[0] || '') && /Recovery mode is the official Rudolf OS/.test(el[0]) && /Everything on it is erased/.test(el[1] || ''), el);
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
