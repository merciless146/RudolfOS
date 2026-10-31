// 1.4.4 part 5 (saver31.js): the screen saver: each of the eight styles draws frames for two seconds into a software
// (processor) canvas without an error; the Start after times, On/Off, Preview, the password after it, rudolfd's idle
// (input in a game ends it and keeps it from starting), and the old saved value carried over. Chromium without a GPU.
// node saver_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.mjs': 'text/javascript', '.html': 'text/html'};
const M = {calls: [], idle: 0, errors: []};
const api = (a, q) => {
  M.calls.push(a);
  if (a == 'idle') return {ok: true, idle: M.idle};
  if (a == 'info') return {ok: true, backlight: false, live: 0, boot: 'uefi', kernel: 'test'};
  if (a == 'events') return {ok: true, events: ''};
  if (a == 'volume') return {ok: true, volume: 50, muted: false};
  return {ok: true};
};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', async () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/events/wait') await new Promise(r => setTimeout(r, 1500));
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5) == 'events/wait' ? 'events' : u.slice(5), qs))) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(8097, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const ctx = await b.newContext();
  // the old saved value: "Off" (0 minutes) from User Settings before 1.4.4
  await ctx.addInitScript(() => { if (!sessionStorage.getItem('t')) { sessionStorage.setItem('t', 1); localStorage.setItem('bd95sys', JSON.stringify({guest: true, ssmin: 0, lockss: true, lockmin: 0, suspmin: 0})) } });
  const p = await ctx.newPage(); p.on('pageerror', e => { M.errors.push(e.message); console.log('page error:', e.stack.slice(0, 300)) });
  await p.goto('http://127.0.0.1:8097/'); await p.waitForFunction(() => typeof ssRun == 'function' && typeof launch == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';window.elevate=async()=>true;window.__locks=0;const L=lockScreen;lockScreen=function(){__locks++};return 1})()");
  ok('1. a saved "Off" from before stays off (and its time becomes 10 minutes for when it is switched on)', await q("SYSC.sson===false&&SYSC.ssmin==10"), await q("SYSC"));
  // each style
  const res = [];
  for (const k of await q("SS_STYLES.map(s=>s[0])")) {
    const r = await q(`(async()=>{const c=document.createElement('canvas');c.width=640;c.height=360;document.body.appendChild(c);let err=null;const ow=console.warn;console.warn=e=>{err=String(e)};
      const lp=ssLoop(c,'${k}',{time:true});await new Promise(r=>setTimeout(r,2000));lp.stop();console.warn=ow;
      const g=c.getContext('2d',{willReadFrequently:true});const a=g.getContextAttributes();const px=g.getImageData(0,0,640,360).data;let lit=0;for(let i=0;i<px.length;i+=16)if(px[i]+px[i+1]+px[i+2]>60)lit++;
      const gl=(()=>{try{return !!c.getContext('webgl')}catch(e){return false}})();c.remove();return{k:'${k}',frames:lp.frames,soft:a.willReadFrequently===true,lit,err,gl}})()`);
    res.push(r) }
  for (const r of res) ok(`2. ${r.k}: ${r.frames} frames in 2 s (at most 30 a second), drawn, in a processor canvas, no error`, r.frames >= 20 && r.frames <= 64 && r.lit > 0 && r.soft && !r.err && !r.gl, r);
  // the frame time in a CPU-only Chromium, at 1280 px
  const ft = await q(`(async()=>{const out={};for(const [k] of SS_STYLES){const c=document.createElement('canvas');c.width=1280;c.height=720;const g=c.getContext('2d',{willReadFrequently:true});const f=SS_DRAW[k](g,1280,720);let t=0;const t0=performance.now();for(let i=0;i<30;i++){f(t,33);g.getImageData(0,0,1,1);t+=33}out[k]=+((performance.now()-t0)/30).toFixed(2)}return out})()`);
  console.log('frame times (ms, 1280×720, CPU):', JSON.stringify(ft));
  ok('3. every style draws a 1280×720 frame in under 20 ms on the processor', Object.values(ft).every(v => v < 20), ft);
  // Display Properties › Screen saver
  await q("settings('disp')"); await wait(1200);
  ok('4. Display Properties › Screen saver: On, Start after (1-60 minutes), the eight styles, a live preview, Show the time, Ask for the password', await q("(()=>{const d=document.querySelector('.ss31');return !!d&&[...d.querySelectorAll('#ssmin option')].map(o=>+o.value).join()=='1,2,3,5,10,15,20,30,45,60'&&d.querySelectorAll('#ssst option').length==8&&!!d.querySelector('#sstime')&&!!d.querySelector('#sslock')&&!d.querySelector('#sson').checked})()"));
  await wait(600);
  ok('...the preview draws', await q("(()=>{const c=document.querySelector('.sspc');const px=c.getContext('2d',{willReadFrequently:true}).getImageData(0,0,240,135).data;let n=0;for(let i=0;i<px.length;i+=4)if(px[i]+px[i+1]+px[i+2]>60)n++;return n})()") > 0);
  await q("(()=>{const d=document.querySelector('.ss31');const on=d.querySelector('#sson');on.checked=true;on.dispatchEvent(new Event('change',{bubbles:true}));const m=d.querySelector('#ssmin');m.value='3';m.dispatchEvent(new Event('change',{bubbles:true}));const s=d.querySelector('#ssst');s.value='aurora';s.dispatchEvent(new Event('change',{bubbles:true}))})()");
  ok('...the choices are kept (On, 3 minutes, Aurora)', await q("(()=>{const v=JSON.parse(localStorage.getItem('bd95sys'));return v.sson===true&&v.ssmin==3&&v.ssstyle=='aurora'})()"), await q("localStorage.getItem('bd95sys')"));
  ok('5. User Settings no longer has the screen saver\'s time or its password option', await q("(async()=>{SW.show('usr',1);await new Promise(r=>setTimeout(r,800));return !document.getElementById('uss')&&!document.getElementById('ulk')})()"));
  await q("SW.show('disp',1)"); await wait(800);
  await q("document.querySelector('.ss31 .ssprev').click()"); await wait(800);
  ok('6. Preview: the saver covers the screen with the chosen style', await q("!!SAVER&&SAVER.style=='aurora'&&!!document.getElementById('saver')&&SAVER.preview"));
  await p.mouse.move(100, 100); await p.mouse.move(300, 200); await wait(300);
  ok('...the pointer moving ends it, without asking for the password (a preview)', await q("!SAVER&&!document.getElementById('saver')&&__locks==0"));
  await q("wins.slice().forEach(w=>w.close())");
  // Start after (a stand-in clock: the last input is set back)
  M.idle = 10 * 60000;
  await q("lastInput=Date.now()-2*60000"); await q("idleCheck()"); await wait(200);
  ok('7. not after 2 minutes when set to 3', await q("!SAVER"));
  await q("lastInput=Date.now()-3*60000-1000"); await q("idleCheck()"); await wait(300);
  ok('...after 3 minutes it starts, with Aurora and the time', await q("!!SAVER&&SAVER.style=='aurora'&&!SAVER.preview"));
  await wait(3200); M.idle = 200; await wait(2000);
  ok('8. input the page does not see (a game: rudolfd\'s idle drops) ends it', await q("!SAVER"));
  ok('...and asks for the password', await q("__locks") >= 1, await q("__locks"));
  await q("lastInput=Date.now()-60*60000"); M.idle = 4000; await q("idleCheck()"); await wait(300);
  ok('9. while a game is played (rudolfd\'s idle is 4 s) it does not start', await q("!SAVER"));
  M.idle = 10 * 60000; await q("SYSC.sson=false"); await q("idleCheck()"); await wait(300);
  ok('10. Off: it never starts', await q("!SAVER"));
  ok('no page error', !M.errors.length, M.errors);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
