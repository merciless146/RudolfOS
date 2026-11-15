// 2026-11-15 (docs/plan-1.6.0.md Part 11): the desktop's idle checks with apps' held requests (rudolfd's media answer:
// idleHeld, sleepHeld, heldBy). Idle for an hour: nothing held, the saver starts, the lock and sleep come; the screen
// held (a game, a video): no saver (and why is logged, naming the app), no lock, no sleep; the computer only held
// awake: the saver and the lock as usual, no sleep.   node inhibit60_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const M = {ih: 0, sh: 0, posts: []};
const srv = http.createServer((q, s) => { const [u0] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', async () => {
    const J = o => { s.writeHead(200, {'content-type': 'application/json'}); s.end(JSON.stringify(o)) };
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/events/wait') { await new Promise(r => setTimeout(r, 800)); s.writeHead(200); return s.end('{"ok":true,"events":""}') }
    if (u == '/api/idle') return J({ok: true, idle: 3600000});
    if (u == '/api/media') return J({ok: true, playing: 0, mic: 0, idleHeld: M.ih, sleepHeld: M.sh, heldBy: M.ih || M.sh ? ['Steam'] : []});
    if (u == '/api/power' && q.method == 'POST') { M.posts.push(q.url); return J({ok: true}) }
    if (u.startsWith('/api/')) return J({ok: true});
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8346, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  const p = await b.newPage({viewport: {width: 1280, height: 900}}); const errs = []; p.on('pageerror', e => errs.push(String(e.stack || e).slice(0, 300)));
  await p.goto('http://127.0.0.1:8346/'); await p.waitForFunction(() => typeof idleCheck == 'function' && typeof INH60 == 'object', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});USER=USER||ACCTS[0];window.__notes=[];window.__rdlog=m=>__notes.push(m);return 1})()");
  const round0 = () => q("(async()=>{saverStop&&saverStop();SAVER=null;LOCKED=false;document.querySelectorAll('#lock,.lockscr').forEach(e=>e.remove());UNLOCKED_AT=0;Object.assign(SYSC,{sson:true,ssmin:1,lockmin:30,suspmin:45});PWR={now:{sleep:45}};idleBusy=false;idleTick=Date.now();idlePerf=performance.now();await idleCheck();return {saver:!!SAVER,locked:LOCKED,inh:INH60}})()");
  const round = async () => { await wait(400); M.posts = []; const r = await round0(); await wait(600); return r };   /* (sleep is asked for without waiting) */
  M.ih = 0; M.sh = 0; M.posts = []; let r = await round();
  ok('1. idle an hour, nothing held: locked, and sleep asked for', r.locked && M.posts.some(x => /action=suspend/.test(x)), [r, M.posts]);
  M.ih = 1; M.posts = []; await q("__notes=[]"); r = await round();
  ok('2. an app holds the screen on: no saver, no lock, no sleep', !r.saver && !r.locked && !M.posts.length && r.inh.idle == 1, [r, M.posts]);
  ok('...the page log says why, naming the app', (await q("__notes.join('|')")).includes('the screen saver waits: an app asked to keep the screen on (Steam)'), await q("__notes"));
  M.ih = 0; M.sh = 1; M.posts = []; r = await round();
  ok('3. an app only holds the computer awake: the lock as usual, but no sleep', r.locked && !M.posts.length && r.inh.sleep == 1, [r, M.posts]);
  M.sh = 0; M.posts = []; r = await round();
  ok('4. let go: sleep again when idle', r.locked && M.posts.some(x => /action=suspend/.test(x)), [r, M.posts]);
  ok('5. no page errors', !errs.length, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
