// 1.5.7 (asked on 2026-11-11): the HP desktop showed two sign-in screens after its screen had locked, and the screen
// saver misbehaved on the Vivobook. The idle checks (core25.js idleCheck) and the lock screen:
// one lock screen only; a check whose answer from rudolfd came late (or across a sleep) decides nothing; waking from
// sleep counts as input; nothing locks by itself for a minute after an unlock; window.LOCKED is the real state;
// the saver draws smaller and slower on the Lite desktop.
// node lock57_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const M = {idle: 0, delay: 0, mode: 'desktop', suspends: 0};
const api = a => {
  if (a == 'idle') return {ok: true, idle: M.idle};
  if (a == 'media') return {ok: true, playing: 0};
  if (a == 'apps/windows') return {ok: true, windows: [], mode: M.mode};
  if (a == 'apps/desktop') { M.mode = 'desktop'; return {ok: true} }
  if (a.startsWith('power')) { M.suspends++; return {ok: true} }
  if (a == 'info') return {ok: true, backlight: false, live: 0, boot: 'uefi', kernel: 'test'};
  if (a == 'events') return {ok: true, events: ''};
  return {ok: true};
};
const srv = http.createServer((q, s) => { const [u0] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', async () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/events/wait') await new Promise(r => setTimeout(r, 1500));
    if (u == '/api/idle' && M.delay) await new Promise(r => setTimeout(r, M.delay));
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5) == 'events/wait' ? 'events' : u.slice(5)))) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8180, '127.0.0.1');
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const ctx = await b.newContext();
  await ctx.addInitScript(() => { localStorage.setItem('bd95sys', JSON.stringify({guest: true, sson: true, ssmin: 5, lockss: true, lockmin: 10, suspmin: 0})) });
  const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.stack.slice(0, 300)));
  await p.goto('http://127.0.0.1:8180/'); await p.waitForFunction(() => typeof idleCheck == 'function' && typeof ssRun == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='guest';PWR=null;return 1})()");
  const unlock = () => q("(()=>{const o=document.getElementById('lockov');if(!o)return false;o.querySelector('.lku').click();return true})()");
  const n = () => q("document.querySelectorAll('#lockov').length");
  const back = () => q("lastInput=Date.now();UNLOCKED_AT=0;1");
  ok('0. the test settings: saver after 5 min, lock after 10', await q("SYSC.sson&&SYSC.ssmin==5&&SYSC.lockmin==10"), await q('SYSC'));
  // 1. two locks: one lock screen
  await q("lockScreen();LOCKED=false;lockScreen();lockScreen('again')");
  ok('1. lockScreen twice (even with LOCKED cleared by mistake): one lock screen', await n() == 1, await n());
  ok('...window.LOCKED is the real state', await q("window.LOCKED===true"));
  await unlock(); await p.waitForTimeout(300);
  ok('...unlocked: none left, window.LOCKED false', await n() == 0 && await q("window.LOCKED===false"));
  // 2. after an unlock, nothing locks by itself for a minute
  M.idle = 11 * 60000; await q("lastInput=Date.now()-11*60000;idleCheck()"); await p.waitForTimeout(500);
  ok('2. idle 11 minutes right after an unlock: no lock, no saver', await n() == 0 && await q("!SAVER"), {n: await n()});
  // 3. normal: idle beyond the lock time locks once
  await q("UNLOCKED_AT=0;lastInput=Date.now()-11*60000;idleCheck()"); await p.waitForTimeout(500);
  ok('3. idle 11 minutes: locked once', await n() == 1, await n());
  await q("idleCheck()"); await p.waitForTimeout(400); ok('...a further check: still one', await n() == 1);
  await unlock(); await p.waitForTimeout(300);
  // 4. a check whose answer came late, after the person came back and unlocked, decides nothing
  await back(); M.idle = 11 * 60000; M.delay = 2500;
  await q("UNLOCKED_AT=0;lastInput=Date.now()-11*60000;window.__c=idleCheck();1"); await p.waitForTimeout(500);
  await q("lastInput=Date.now();1");   // the person moves the mouse while rudolfd's answer is on its way
  await q("window.__c"); await p.waitForTimeout(300); M.delay = 0;
  ok('4. rudolfd answered "idle 11 minutes" late, after input: no lock screen', await n() == 0, await n());
  // 5. only one check at a time
  M.delay = 1500; M.idle = 0; await q("UNLOCKED_AT=0;lastInput=Date.now();window.__k=0;const o=idleNow;idleNow=async()=>{__k++;return o()};idleCheck();idleCheck();idleCheck();1");
  await p.waitForTimeout(2200); M.delay = 0;
  ok('5. three checks at once: one asks rudolfd', await q("__k") == 1, await q("__k"));
  // 6. an answer that never comes: the check gives up after 4 s, the next one runs
  M.delay = 9000; await q("__k=0;idleCheck();1"); await p.waitForTimeout(4600);
  ok('6. rudolfd not answering: the check ends within 4 s (not stuck)', await q("!idleBusy"), await q('idleBusy')); M.delay = 0;
  // 7. waking from sleep: the wall clock jumped 2 hours, the page's own clock did not
  await q("lastInput=Date.now()-2*3600e3;UNLOCKED_AT=0;idleTick=Date.now()-2*3600e3-15000;idlePerf=performance.now()-15000;1");
  M.idle = 0; await q("idleCheck()"); await p.waitForTimeout(400);
  ok('7. after a 2-hour sleep the wake counts as input: no saver, no second lock screen', await n() == 0 && await q("!SAVER&&Date.now()-lastInput<5000"), {n: await n(), saver: await q('!!SAVER')});
  // ...but timers that were only slowed (a covered window) are not a sleep
  await q("idleTick=Date.now()-90000;idlePerf=performance.now()-90000;1");
  ok('...slowed timers (both clocks moved 90 s) are not taken for a sleep', await q("idleWoke()===false"));
  // 8. the saver, then the lock: one lock screen, the saver gone
  await q("UNLOCKED_AT=0;lastInput=Date.now()-6*60000;1"); M.idle = 6 * 60000; await q("idleCheck()"); await p.waitForTimeout(500);
  ok('8. idle 6 minutes: the saver starts', await q("!!SAVER&&!!document.getElementById('saver')"));
  await q("SAVER.t=Date.now()-5000;lastInput=Date.now()-11*60000;1"); M.idle = 11 * 60000; await q("idleCheck()"); await p.waitForTimeout(500);
  ok('...idle 11 minutes: the saver ends and one lock screen shows', await n() == 1 && await q("!SAVER&&!document.getElementById('saver')"), {n: await n()});
  await p.mouse.move(50, 50); await p.mouse.move(80, 90); await p.waitForTimeout(300);
  ok('...moving the mouse at the lock screen: still one lock screen', await n() == 1);
  await unlock(); await p.waitForTimeout(300); ok('...unlocked once: the desktop', await n() == 0);
  // 9. the Lite desktop: smaller and slower
  await q("document.documentElement.classList.add('webkit');1");
  const lite = await q("(async()=>{const s=ssRun({preview:true});await new Promise(r=>setTimeout(r,2000));const w=s.c.width,f=s.lp.frames;s.stop(false);return{w,f}})()");
  ok(`9. Lite: the saver draws ${lite.w} px wide, ${lite.f} frames in 2 s (at most 15 a second)`, lite.w <= 960 && lite.f <= 34 && lite.f >= 10, lite);
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); const k = R.filter(Boolean).length; console.log(`${k}/${R.length} passed`); process.exit(k == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
