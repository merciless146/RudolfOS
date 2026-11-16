// 1.6.0 (the owner, 2026-11-16): Settings › Update and recovery › "Roll back to last stable version…": shown only with Get beta
// versions on or on a system that is not a published stable release (a beta, a test or a developer's build), hidden
// otherwise; its prompts (which version, nothing downloaded or downloaded, what the older release does not use, the
// refusal when it cannot read this computer's data, an administrator's name and password sent to rudolfd, the restart),
// and a developer's build sent to Return to official Rudolf OS. A stand-in rudolfd.   node tostable60_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 600) : '')) };
// the computer: on the stable 1.5.9, or on the beta 1.6.0 with beta versions on, or a 1.6.0 test build with them off
let S = {beta: false, stable: true, signer: 'official', developer: false}, how = 'slot', compatible = true; const posts = [];
const status = () => ({ok: true, persistent: true, booted: 'b', current: {serial: S.stable ? 2026111503 : 2026111611, version: S.stable ? '1.5.9.261115' : '1.6.0.261116'}, state: {good: 'b', prev: 'a'},
  slots: {a: {complete: true}, b: {complete: true}, r: {}}, canUpdate: true, signer: S.signer, developer: S.developer, recoveryOfficial: true, url: 'https://merciless146.github.io/RudolfOS/' + (S.beta ? 'beta' : 'stable'),
  auto: true, beta: S.beta, toStable: false, checked: 0, available: null, runningStable: S.stable, stableVersion: '1.5.9.261115'});
const info = () => ({ok: true, running: {serial: 2026111611, version: '1.6.0.261116'}, signer: S.signer, developer: S.developer, beta: S.beta, canUpdate: true, runningStable: S.stable, dataFmt: 2, reads: [1, 2],
  stable: {serial: 2026111503, version: '1.5.9.261115', released: '2026-11-15', datafmt: compatible ? 2 : 1, reads: compatible ? [1, 2] : [1, 1], recorded: false},
  how: S.signer != 'official' ? 'official' : how, slot: how == 'slot' ? 'a' : '', compatible,
  why: compatible ? '' : "Rudolf OS 1.5.9.261115 cannot read the accounts and files on this computer: here each account is its own Linux user, its folder and files belonging to it alone (since Rudolf OS 1.5.9), and Rudolf OS 1.5.9.261115 only knows the way where every account's files belong to one Linux user, the desktop's (as before Rudolf OS 1.5.9). Going back to it would leave accounts whose folders and files it cannot open, so nothing was changed.",
  notes: ["Linux apps' sign-ins saved in the keyring (new in 1.6.0) are not used: those apps may ask you to sign in again", 'system extensions (such as USB printers) are not used', "Messages and the month's mobile data count are kept but not shown"]});
let jobLog = '';
const api = (a, q, m, body) => { const sp = new URLSearchParams(q || '');
  if (a == 'sys/status') return status();
  if (a == 'sys/stable') return info();
  if (a == 'sys/check' && m == 'POST') { posts.push('check'); jobLog = 'Checking https://merciless146.github.io/RudolfOS/stable\nRudolf OS is up to date (1.6.0.261116).\n'; return {ok: true, message: 'started'} }
  if (a == 'sys/tostable' && m == 'POST') { let b = {}; try { b = JSON.parse(body) } catch (e) {} posts.push('tostable ' + b.n + ' ' + b.p);
    jobLog = how == 'slot' ? 'The last stable version: Rudolf OS 1.5.9.261115 (released 2026-11-15).\nRudolf OS 1.5.9.261115 is still on this computer (system slot a): nothing to download. Beta versions are off.\nReady: Rudolf OS 1.5.9.261115 starts the next time the computer restarts.\n'
      : 'Beta versions are off. Downloading Rudolf OS 1.5.9.261115 into the spare system slot (only the changes).\nbase: downloading the changes (291 MB file)\nReady: Rudolf OS 1.5.9.261115 starts the next time the computer restarts (40211 KB downloaded).\n';
    S.beta = false; return {ok: true, message: 'started'} }
  if (a == 'sys/return' && m == 'POST') { posts.push('return ' + sp.get('erase')); jobLog = 'Returning to the official Rudolf OS 1.5.9: it starts the next time the computer restarts, with your accounts, files and apps.\n'; return {ok: true, message: 'started'} }
  if (a == 'power' && m == 'POST') { posts.push('power ' + sp.get('action')); return {ok: true} }
  if (a == 'job') return {ok: true, running: false, status: 0, log: jobLog};
  if (a == 'info') return {ok: true, vendor: 'ASUS', product: 'Vivobook', cpu: 'x', threads: 4, memTotal: 3.4e9, kernel: '6.18', boot: 'uefi', live: 0};
  return {ok: true} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0); let body = '';
  q.on('data', c => body += c); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/a/')) { const f = path.join(WWW, u); if (fs.existsSync(f)) { s.writeHead(200); return s.end(fs.readFileSync(f)) } }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs, q.method, body))) }
    s.writeHead(404); s.end() }) }).listen(8611, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8611/'); await p.waitForFunction(() => typeof sysuToStable == 'function' && typeof updPanel == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x'},{n:'Ben',r:'user',h:'y'}];USER=ACCTS[0];pwCheck=async(a,pw)=>a.n=='Ann'&&pw=='sesame';return 1})()`);
  const open = async () => { await q("(()=>{settings('upd');return 1})()"); await wait(1500);
    return q("(()=>{const x=document.querySelector('.sysu');const bt=x&&x.querySelector('.ustable');return x?{btn:bt?bt.textContent:null,dis:bt?bt.disabled:null,row:[...x.querySelectorAll('.rmrow button')].map(b=>b.textContent)}:null})()") };
  const dlg = () => q("(()=>{const o=[...document.querySelectorAll('.rmodal')].at(-1);return o?{h:o.querySelector('h3')?.textContent,t:o.textContent.replace(/\\s+/g,' '),b:[...o.querySelectorAll('.rmrow button')].map(b=>b.textContent)}:null})()");
  const click = t => q(`(()=>{const o=[...document.querySelectorAll('.rmodal')].at(-1);const b=[...o.querySelectorAll('button')].find(b=>b.textContent==${JSON.stringify(t)});b.click();return 1})()`);
  // hidden on a stable release with beta versions off
  let d = await open();
  ok('1. on the stable 1.5.9 with Get beta versions off: no "Roll back to last stable version" (Roll back system… stays)', d && d.btn === null && d.row.includes('Roll back system…'), d);
  S = {beta: true, stable: false, signer: 'official', developer: false}; d = await open();
  ok('2. on the beta with Get beta versions on: shown', d && d.btn == 'Roll back to last stable version…' && !d.dis, d);
  S = {beta: true, stable: true, signer: 'official', developer: false}; d = await open();
  ok('3. Get beta versions on while a stable release runs: shown too', d && d.btn == 'Roll back to last stable version…', d);
  S = {beta: false, stable: false, signer: 'official', developer: false}; d = await open();
  ok('4. a build the update server does not list as stable (a test build) with Get beta versions off: shown', d && d.btn == 'Roll back to last stable version…', d);
  await q('USER=ACCTS[1]'); d = await open();
  ok('5. a standard account sees it but cannot use it', d && d.btn && d.dis, d);
  await q('USER=ACCTS[0]');
  // the prompt: which version, nothing downloaded (the other slot), what 1.5.9 does not use
  S = {beta: true, stable: false, signer: 'official', developer: false}; how = 'slot'; await open(); posts.length = 0;
  await q("document.querySelector('.sysu .ustable').click()"); await wait(2500); d = await dlg();
  ok('6. it checks for updates first, then says which version it goes to, that nothing is downloaded, that beta versions go off and the data stays', posts[0] == 'check' && d && d.h == 'Roll back to last stable version'
    && /Rudolf OS 1\.5\.9\.261115 \(stable, released 2026-11-15\) starts the next time the computer restarts: it is still on this computer, so nothing is downloaded/.test(d.t)
    && /Get beta versions is turned off/.test(d.t) && /accounts, files and apps are kept/.test(d.t) && d.b.join() == 'Cancel,Roll back', [posts, d]);
  ok('7. ...and what 1.5.9 does not use while it runs (the keyring\'s sign-ins, system extensions, Messages)', /While 1\.5\.9\.261115 runs:/.test(d.t) && /keyring/.test(d.t) && /System extensions/.test(d.t) && /Messages/.test(d.t), d);
  await click('Cancel'); await wait(300);
  ok('8. Cancel: nothing asked of rudolfd', posts.join() == 'check' && !(await dlg()), posts);
  await q("document.querySelector('.sysu .ustable').click()"); await wait(2500); await click('Roll back'); await wait(400);
  const el = await q("document.querySelector('.rmodal .rmr')?.textContent||''");
  ok('9. then an administrator\'s name and password (every time), naming the version', /Roll back to 1\.5\.9\.261115, the last stable version of Rudolf OS/.test(el), el);
  await p.type('.rmodal .ep', 'wrong'); await p.keyboard.press('Enter'); await wait(500);
  ok('10. a wrong password: refused, nothing sent', !posts.some(x => x.startsWith('tostable')) && /Wrong password/.test(await q("document.querySelector('.rmodal .rmmsg')?.textContent||''")), posts);
  await p.fill('.rmodal .ep', ''); await p.type('.rmodal .ep', 'sesame'); await p.keyboard.press('Enter'); await wait(2500);
  d = await dlg();
  ok('11. rudolfd gets the administrator\'s name and password (it checks them), the job\'s lines are shown as updates show them, then the restart', posts.includes('tostable Ann sesame')
    && /nothing to download/.test(await q("document.querySelector('.sysu .ulog')?.textContent||''")) && d && d.h == 'Restart to roll back' && /1\.5\.9\.261115 \(stable, released 2026-11-15\) starts when the computer restarts/.test(d.t) && d.b.join() == 'Later,Restart now', [posts, d]);
  await click('Restart now'); await wait(500);
  ok('12. Restart now restarts', posts.at(-1) == 'power reboot', posts);
  // downloaded
  S = {beta: true, stable: false, signer: 'official', developer: false}; how = 'download'; await open();
  await q("document.querySelector('.sysu .ustable').click()"); await wait(2500); d = await dlg();
  ok('13. the other slot without it: "downloaded first, as updates are (only the changes)"', d && /it is downloaded first, as updates are \(only the changes\)/.test(d.t), d);
  await click('Cancel');
  // refused: the stable release cannot read this computer's data
  compatible = false; how = 'slot'; posts.length = 0; await open();
  await q("document.querySelector('.sysu .ustable').click()"); await wait(2500); d = await dlg();
  ok('14. a stable release that cannot read this computer\'s data: refused with why in plain words, only OK, nothing sent', d && d.h == 'Cannot roll back to 1.5.9.261115' && /cannot read the accounts and files on this computer/.test(d.t)
    && /Your system stays as it is/.test(d.t) && d.b.join() == 'OK' && posts.join() == 'check', [posts, d]);
  await click('OK'); compatible = true;
  // a developer's build: Return to official Rudolf OS
  S = {beta: false, stable: false, signer: 'custom', developer: true}; posts.length = 0; d = await open();
  ok('15. a developer\'s build: shown', d && d.btn, d);
  await q("document.querySelector('.sysu .ustable').click()"); await wait(800); d = await dlg();
  ok('16. ...and it is Return to official Rudolf OS (official keys, keep or erase), not a second way', d && /Return to official Rudolf OS/.test(d.h) && posts.length == 0, [d, posts]);
  await q("document.querySelector('.rmodal .y').click()"); await wait(400); await p.type('.rmodal .ep', 'sesame'); await p.keyboard.press('Enter'); await wait(2000);
  ok('17. ...which returns to official, keeping the data', posts.at(-1) == 'return 0', posts);
  ok('18. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
