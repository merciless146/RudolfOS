// 2026-11-15 (docs/plan-1.6.0.md Part 13): Settings › Update and recovery › System extensions: what is installed and what
// this start did with each (GET sysext/status: in use, after a restart, left out and why, Try again), drawn as text;
// Install… from the extensions' list, Remove after asking, Try again and Start once without extensions, each with an
// administrator's name and password sent for rudolfd to check (elevate {cred:1}); a standard account's buttons off; the
// job's lines; not installed (no RUDOLFDATA): said.   node sysext60_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const M = {posts: [], job: null, persistent: true, started: 'with extensions', installed: [
  {name: 'usbprint', version: '1.0', title: 'USB printers', summary: 'Printing to printers plugged in by USB', size: 3355443, state: 'in-use', why: ''},
  {name: 'devtools', version: '2.0', title: '<img src=x onerror="window.__pwn=1">', summary: 'curl, strace', size: 900000, state: 'left-out', why: 'it would replace /usr/bin/curl, which the system already has'},
  {name: 'fw', version: '1.1', title: 'Firmware', summary: '', size: 2000, state: 'left-out', why: 'x', off: 'the desktop did not come up at the last start, the first with fw 1.1: left out (Try again in Settings)'}],
  available: [{name: 'usbprint', version: '1.0', title: 'USB printers', installed: '1.0', size: 3355443}, {name: 'wwan', version: '0.9', title: 'Cellular modules', summary: 'FCC unlock', size: 120000, license: 'GPL-2.0', installed: null}]};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0); let body = '';
  q.on('data', d => body += d); q.on('end', async () => {
    const J = (o, c) => { s.writeHead(c || 200, {'content-type': 'application/json'}); s.end(JSON.stringify(o)) };
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/events/wait') { await new Promise(r => setTimeout(r, 800)); s.writeHead(200); return s.end('{"ok":true,"events":""}') }
    if (u == '/api/sysext/status') return J({ok: true, persistent: M.persistent, base: 'ab'.repeat(32), kernel: '6.18.9', skipOnce: !!M.skip, started: M.started, installed: M.installed, available: M.available});
    if (u.startsWith('/api/sysext/') && q.method == 'POST') { M.posts.push([u.slice(12), new URLSearchParams(qs || '').get('name'), body]);
      if (u.endsWith('/install') || u.endsWith('/remove')) M.job = {n: 0, what: u.slice(12), name: new URLSearchParams(qs).get('name')};
      if (u.endsWith('/skip-once')) M.skip = 1; return J({ok: true, message: 'started'}) }
    if (u == '/api/job') { if (!M.job) return J({ok: true, running: false, status: 0, log: ''}); M.job.n++;
      const done = M.job.n > 2; const log = 'Getting ' + M.job.name + '\n' + (done ? 'Installed the system extension ' + M.job.name + ' 0.9: it is in use after the next restart\n' : '');
      if (done) { if (M.job.what == 'install') M.installed.push({name: 'wwan', version: '0.9', title: 'Cellular modules', size: 120000, state: 'next', why: ''}); else M.installed = M.installed.filter(x => x.name != M.job.name); M.job = null }
      return J({ok: true, running: !done, status: done ? 0 : null, log}) }
    if (u == '/api/sys/status') return J({ok: true, persistent: true, booted: 'a', slots: {}, current: {version: '1.6.0'}, state: {good: 'a'}, url: 'https://x/stable', auto: true, checked: 0});
    if (u.startsWith('/api/')) return J({ok: true});
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8745, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  const p = await b.newPage({viewport: {width: 1280, height: 900}}); const errs = []; p.on('pageerror', e => errs.push(String(e.stack || e).slice(0, 300)));
  await p.goto('http://127.0.0.1:8745/'); await p.waitForFunction(() => typeof sxSection == 'function' && typeof settings == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  // an administrator answers the password prompt (rudolfd checks it: here its name and password are what is sent)
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});USER=USER||ACCTS[0];USER.r='admin';window.__asked=[];window.elevate=async(r,o)=>{__asked.push([r,!!(o&&o.cred)]);return o&&o.cred?{n:'Ann',p:'pw-ann'}:true};return 1})()");
  const open = async () => { await q("(()=>{wins.slice().forEach(w=>w.close());settings('upd');return 1})()"); await wait(1100) };
  const rows = () => q("[...document.querySelectorAll('.sx60 tbody tr')].map(r=>[...r.cells].map(c=>c.textContent))");
  const btn = (sel, text) => q(`(()=>{const b=[...document.querySelectorAll('${sel}')].find(b=>b.textContent=='${text}');if(!b)return 0;b.click();return 1})()`);
  await open(); let r = await rows();
  ok('1. Update and recovery › System extensions: each one, its version, size and what this start did', r.length == 3 && /USB printers/.test(r[0][0]) && /Printing to printers/.test(r[0][0]) && r[0][1] == '1.0' && r[0][2] == '3.2 MB' && r[0][3] == 'In use', r);
  ok('...after Security updates', await q("(()=>{const x=document.querySelector('.sx60');return !!x&&!!x.previousElementSibling&&x.previousElementSibling.classList.contains('sysu')})()"));
  ok('...one left out says why', r[1][3] == 'Left out: it would replace /usr/bin/curl, which the system already has', r[1]);
  ok('...its name shown as text, never run as HTML', !(await q('window.__pwn')) && /<img/.test(r[1][0]), r[1]);
  ok('...one left out after a start that failed has Try again', /^Left out: the desktop did not come up/.test(r[2][3]) && /Try again/.test(r[2][4]), r[2]);
  await q("document.querySelectorAll('.sx60 tbody tr')[2].querySelector('.sxa').click()"); await wait(700);
  ok('2. Try again: an administrator\'s password, sent for rudolfd to check', M.posts[0] && M.posts[0][0] == 'retry' && M.posts[0][1] == 'fw' && JSON.parse(M.posts[0][2]).n == 'Ann' && JSON.parse(M.posts[0][2]).p == 'pw-ann' && (await q('__asked'))[0][1], [M.posts, await q('__asked')]);
  await q("document.querySelectorAll('.sx60 tbody tr')[1].querySelector('.sxx').click()"); await wait(300);
  ok('3. Remove asks first', /Remove the system extension .*gone after the next restart/.test(await q("(document.querySelector('.dqa')||{}).textContent||''")));
  await btn('.rmodal button', 'Cancel'); await wait(200); ok('...Cancel: nothing removed', M.posts.length == 1);
  await q("document.querySelectorAll('.sx60 tbody tr')[1].querySelector('.sxx').click()"); await wait(300); await btn('.rmodal button', 'Remove'); await wait(3500);
  ok('...Remove: with the password, the job\'s lines shown, the list read again, Restart now offered', M.posts[1] && M.posts[1][0] == 'remove' && M.posts[1][1] == 'devtools' && JSON.parse(M.posts[1][2]).n == 'Ann' && (await rows()).length == 2 && /Removed: it is gone after a restart/.test(await q("document.querySelector('.sx60 .sxm').textContent")) && /Getting devtools/.test(await q("document.querySelector('.sx60 .sxl').textContent")) && !(await q("document.querySelector('.sx60 .sxr').hidden")), [M.posts, await q("document.querySelector('.sx60 .sxm').textContent")]);
  await btn('.sx60 button', 'Install…'); await wait(300);
  const av = await q("[...document.querySelectorAll('.sxav .sxone')].map(d=>d.textContent)");
  ok('4. Install…: the extensions listed for this version that are not installed', av.length == 1 && /Cellular modules 0\.9/.test(av[0]) && /FCC unlock · 117 KB · GPL-2.0/.test(av[0]), av);
  await q("document.querySelector('.sxav .sxone button').click()"); await wait(3500); r = await rows();
  ok('...Install: with the password, then "in use after a restart"', M.posts[2] && M.posts[2][0] == 'install' && M.posts[2][1] == 'wwan' && r.some(x => /Cellular modules/.test(x[0]) && x[3] == 'In use after a restart') && /Installed: it is in use after a restart/.test(await q("document.querySelector('.sx60 .sxm').textContent")), [M.posts, r]);
  await btn('.sx60 button', 'Start once without extensions'); await wait(900);
  ok('5. Start once without extensions: with the password; said', M.posts[3] && M.posts[3][0] == 'skip-once' && /next start is without system extensions/.test(await q("document.querySelector('.sx60').textContent")), M.posts);
  M.started = 'Recovery mode starts without system extensions'; await open();
  ok('6. a start without them says so', /This start: Recovery mode starts without system extensions\./.test(await q("document.querySelector('.sx60 .sxs').textContent")));
  await q("USER.r='user'"); await open();
  ok('7. a standard account: Install…, Remove, Try again and Start once without extensions are off; Check is not', await q("(()=>{const B=[...document.querySelectorAll('.sx60 button')];const t=x=>B.filter(b=>b.textContent==x);return t('Install…')[0].disabled&&t('Remove').every(b=>b.disabled)&&t('Start once without extensions')[0].disabled&&!t('Check for extensions')[0].disabled})()"));
  M.persistent = false; await open();
  ok('8. not installed (no RUDOLFDATA): said, nothing to press', /need Rudolf OS installed/.test(await q("document.querySelector('.sx60 tbody').textContent")) && await q("[...document.querySelectorAll('.sx60 button')].every(b=>b.disabled)"));
  ok('9. no page errors', !errs.length, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
