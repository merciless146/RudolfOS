// 1.4.0 in the desktop page, with a stand-in rudolfd, in Chromium: Settings › Security › Development link (switching
// on asks for an administrator's password; the token goes in the body, never into the page again), the banner over
// everything with End now, the Allow prompt (Allow and Refuse), the log, the system report. node dl140_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../..', 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.mjs': 'text/javascript', '.html': 'text/html'};
const TOKEN = 'github_pat_PAGETEST_secret_0123456789';
const now = () => Math.floor(Date.now() / 1000);
const M = {calls: [], bodies: [], on: false, repo: '', until: 0, ask: true, restart: false, home: false, hasToken: false, pending: null, answers: []};
const LOG = '{"t":"2026-10-27T08:00:00Z","id":"000001","kind":"sh","what":"uname -a","result":"status 0","approved":"allowed on the computer"}\n';
const api = (a, q, body) => { const Q = new URLSearchParams(q || ''); M.calls.push(a + (q ? '?' + q : '')); if (body && body.length) M.bodies.push(a + ':' + body.toString());
  if (a == 'devlink') return {ok: true, available: true, on: M.on, repo: M.repo, until: M.until, secondsLeft: M.on ? M.until - now() : 0, restart: M.restart, ask: M.ask, home: M.home, hasToken: M.hasToken, persistent: true, status: M.on ? {state: 'on', last: '', error: ''} : {state: 'off'}, ...(M.on && M.pending ? {pending: M.pending} : {})};
  if (a == 'devlink/on') { if (Q.get('repo') == 'owner/public') return {ok: false, error: 'the repository is public: use a private one, so nobody else can read what is run here', __status: 400};
    M.on = true; M.repo = Q.get('repo'); M.until = now() + 3600 * +Q.get('hours'); M.ask = Q.get('ask') == '1'; M.restart = Q.get('restart') == '1'; M.home = Q.get('home') == '1'; M.hasToken = true; return {ok: true, message: 'on'} }
  if (a == 'devlink/off') { M.on = false; return {ok: true, message: 'off'} }
  if (a == 'devlink/answer') { M.answers.push(Q.get('id') + ':' + Q.get('a')); M.pending = null; return {ok: true} }
  if (a == 'devlink/log') return {ok: true, log: LOG};
  if (a == 'system') return {ok: true, renderer: 'AMD', display: 'gpu', audio: '', net: '', services: {sound: true}, machine: 'Test', firmware: 'Test', kernel: {boot: 'uefi', messages: '', release: '6.18'}, cpu: {}, memory: {total: 8e9}, gpu: '', storage: '', input: '', uptime: 100, load: [0, 0, 0]};
  if (a == 'stats') return {ok: true, battery: [], cpu: 1, memTotal: 8e9, memAvailable: 4e9, load: 0};
  if (a == 'info') return {ok: true, live: false, boot: 'uefi', media: 'disk', uptime: 100, load: [0, 0, 0], memTotal: 8e9, memAvailable: 4e9, kernel: '6.18'};
  if (a == 'secureboot/status') return {ok: true, efi: true, on: false};
  if (a == 'crashes') return {ok: true, crashes: []};
  return {ok: true} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0); const ch = []; q.on('data', c => ch.push(c)); q.on('end', () => {
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
  if (u.startsWith('/api/')) { const r = api(u.slice(5), qs, Buffer.concat(ch)); s.writeHead(r.__status || 200, {'content-type': 'application/json'}); return s.end(JSON.stringify(r)) }
  const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(8099, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage(); p.on('pageerror', e => /toUpperCase/.test(e.message) || console.log('page error:', e.message));
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8099/'); await p.waitForFunction(() => typeof dlSection == 'function' && typeof secPanelS == 'function', null, {timeout: 30000});
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';window.ELEV=[];window.elevate=async r=>{ELEV.push([r,window.__elevTrust]);return true};return 1})()");
  const open = async () => { await q("(()=>{wins.slice().forEach(w=>w.close());settings('sec');return 1})()"); await p.waitForSelector('.dl40 .dls', {timeout: 8000}).catch(() => 0); await wait(800) };
  await open();
  const t0 = await q("document.querySelector('.dl40 .dls').textContent");
  ok('Settings › Security has the Development link, off, saying what it allows and "test computer only"', /^Off\./.test(t0) && /full control/.test(t0) && /test computer only/.test(t0), t0);
  ok('...with the repository, the token (hidden), how long, Ask first (ticked), stay across a restart, home folders (not ticked)', await q("(()=>{const f=document.querySelector('.dl40');return !!f.querySelector('.dlr')&&f.querySelector('.dlk').type=='password'&&f.querySelector('.dlh').value=='2'&&f.querySelector('.dla').checked&&!f.querySelector('.dlb').checked&&!f.querySelector('.dlhome').checked})()"));
  const sw = "[...document.querySelectorAll('.dl40 button')].find(x=>x.textContent=='Switch on…').click()";
  await q(`(()=>{document.querySelector('.dl40 .dlr').value='not a repo';${sw};return 1})()`); await wait(300);
  ok('a repository not written owner/name is refused before anything is asked', /owner\/name/.test(await q("document.querySelector('.dl40 .dlerr').textContent")) && !M.calls.some(c => c.startsWith('devlink/on')));
  await q(`(()=>{document.querySelector('.dl40 .dlr').value='owner/public';document.querySelector('.dl40 .dlk').value='${TOKEN}';${sw};return 1})()`); await wait(800);
  ok('GitHub\'s answer is shown when the token or repository is refused', /public/.test(await q("document.querySelector('.dl40 .dlerr').textContent")), await q("document.querySelector('.dl40 .dlerr').textContent"));
  M.calls.length = 0; M.bodies.length = 0;
  await q(`(()=>{document.querySelector('.dl40 .dlr').value='owner/rudolfos-devlink';document.querySelector('.dl40 .dlk').value='${TOKEN}';document.querySelector('.dl40 .dlh').value='4';document.querySelector('.dl40 .dlb').checked=true;${sw};return 1})()`); await wait(1500);
  const el = await q("ELEV[ELEV.length-1]");
  ok('switching on asks for an administrator\'s password, fresh (no trust from an earlier prompt), naming the repository', el && /owner\/rudolfos-devlink/.test(el[0]) && !el[1], el);
  const onc = M.calls.find(c => c.startsWith('devlink/on'));
  ok('...rudolfd is asked with the choices, the token only in the request body', onc && /repo=owner%2Frudolfos-devlink/.test(onc) && /hours=4/.test(onc) && /ask=1/.test(onc) && /restart=1/.test(onc) && /home=0/.test(onc) && !onc.includes(TOKEN) && M.bodies.some(x => x == 'devlink/on:' + TOKEN), [onc, M.bodies]);
  ok('...and the token is not left anywhere in the page', !(await q("document.documentElement.outerHTML+[...document.querySelectorAll('input')].map(i=>i.value).join()")).includes(TOKEN));
  await p.waitForSelector('#devlinkBanner', {timeout: 8000}).catch(() => 0);
  const bn = await q("(()=>{const b=document.getElementById('devlinkBanner');if(!b)return null;const c=getComputedStyle(b);return {t:b.textContent,z:c.zIndex,pos:c.position,top:b.getBoundingClientRect().top}})()");
  ok('the banner: fixed at the top, above everything, with the repository, the time left and End now', bn && bn.pos == 'fixed' && bn.top == 0 && bn.z == '2147483647' && /Development link on/.test(bn.t) && /owner\/rudolfos-devlink/.test(bn.t) && /(3 h 59|4 h 0) min left/.test(bn.t) && /End now/.test(bn.t), bn);
  await open(); const t1 = await q("document.querySelector('.dl40 .dls').textContent");
  ok('Settings says it is on, until when, and how', /^On until/.test(t1) && /each one waits for you to allow it/.test(t1) && /also after a restart/.test(t1), t1);
  await q("(()=>{wins.slice().forEach(w=>w.close());const W=win('Notepad','x',800,600);W.el&&W.el.requestFullscreen&&0;return 1})()");
  ok('...the banner stays above a window', await q("(()=>{const b=document.getElementById('devlinkBanner').getBoundingClientRect();const e=document.elementFromPoint(b.left+40,b.top+8);return !!(e&&e.closest('#devlinkBanner'))})()"));
  // Ask first
  M.pending = {id: '000007', kind: 'sh', what: 'efibootmgr -v\ncat /proc/cmdline', since: now()};
  await p.waitForSelector('#devlinkAsk', {timeout: 8000}).catch(() => 0);
  const ak = await q("(()=>{const o=document.getElementById('devlinkAsk');return o?o.textContent:''})()");
  ok('Ask first: the command shows with its full text, Allow and Refuse', /000007/.test(ak) && /efibootmgr -v\s*cat \/proc\/cmdline/.test(ak) && /Refuse/.test(ak) && /Allow/.test(ak), ak);
  ok('...Refuse has the focus (Enter does not allow by accident)', await q("document.activeElement&&document.activeElement.classList.contains('dln')"));
  await q("document.querySelector('#devlinkAsk .dly').click()"); await wait(500);
  ok('...Allow answers that command, and the prompt goes', M.answers.includes('000007:allow') && !(await q("!!document.getElementById('devlinkAsk')")), M.answers);
  M.pending = {id: '000008', kind: 'restart', what: 'restart the computer', since: now()};
  await p.waitForSelector('#devlinkAsk', {timeout: 8000}).catch(() => 0); await q("document.querySelector('#devlinkAsk .dln').click()"); await wait(500);
  ok('...Refuse answers refuse', M.answers.includes('000008:refuse'), M.answers);
  // the log, the report, End now
  await open(); await q("[...document.querySelectorAll('.dl40 button')].find(x=>x.textContent=='Log…').click()"); await wait(700);
  const lg = await q("[...document.querySelectorAll('.rmodal pre')].map(x=>x.textContent).join()");
  ok('the log lists what was run, its result and who allowed it', /000001\s+sh\s+status 0\s+\(allowed on the computer\)/.test(lg) && /uname -a/.test(lg), lg);
  await q("document.querySelectorAll('.rmodal').forEach(x=>x.remove())");
  const rep = await q("sysReport('').then(r=>r.text)");
  ok('the system report says the link is on, until when and from where, without the token', /Development link: on until .* \(owner\/rudolfos-devlink, asks first\)/.test(rep) && !rep.includes(TOKEN), (rep.match(/Development link.*/) || [''])[0]);
  await q("document.querySelector('#devlinkBanner .dle').click()"); await wait(5000);
  ok('End now (no password needed to stop it) switches it off and the banner goes', M.calls.includes('devlink/off') && !M.on && !(await q("!!document.getElementById('devlinkBanner')")));
  console.log(`${R.filter(x => x).length}/${R.length} passed`); await b.close(); srv.close(); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
