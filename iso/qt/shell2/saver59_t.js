// 1.5.9: the screen saver starts after the idle time even when a full-screen element is no longer on the screen or a
// saver's picture was taken away; why it waits is written to the page log. A stand-in rudolfd.   node saver59_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www'), J = {'content-type': 'application/json'};
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
let idle = 3600000, playing = 0;
const srv = http.createServer((q, s) => { const u = q.url.split('?')[0]; q.on('data', () => {}); q.on('end', () => {
  if (u == '/') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};window.__LOG=[];window.__rdlog=m=>__LOG.push(m);</script>')) }
  if (u == '/api/idle') { s.writeHead(200, J); return s.end(JSON.stringify({ok: true, idle})) }
  if (u == '/api/media') { s.writeHead(200, J); return s.end(JSON.stringify({ok: true, playing, mic: 0})) }
  if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200, J); s.end('{"ok":true,"events":[]}') }, 20000); s.writeHead(200, J); return s.end('{"ok":true}') }
  s.writeHead(404); s.end() }) }).listen(8289, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => { if (/idle|saver|fsStandIn/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8289/'); await p.waitForFunction(() => typeof idleCheck == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x'}];USER=ACCTS[0];UNLOCKED_AT=0;SYSC.sson=true;SYSC.ssmin=10;return 1})()`);
  const check = async () => { await q('idleCheck()'); await wait(800) };
  const stop = () => q('saverStop()');
  await check(); ok('1. idle past the time: the screen saver starts', await q('!!SAVER&&SAVER.c.isConnected')); await stop();
  // the desktop's full-screen stand-in (inside the shell), an element in a window that is then minimised
  await q(`(()=>{fsStandIn(()=>true);const w=document.createElement('div');w.className='win';w.id='tw';w.innerHTML="<div class='tb'><span class='t'>FNF</span></div><div id='game' style='width:200px;height:100px'></div>";document.body.appendChild(w);document.getElementById('game').requestFullscreen();return 1})()`); await wait(300);
  await check();
  ok('2. a game really full screen: the saver waits, and the page log says why', await q('!SAVER') && await q("__LOG.some(m=>/the screen saver waits: something is full screen \\(div#game in “FNF”\\)/.test(m))"), await q('__LOG'));
  await q("document.getElementById('tw').style.display='none'"); await wait(1400);
  ok('3. its window minimised: it is no longer full screen (the stand-in lets go; game mode ends too)', await q('!document.fullscreenElement&&!gameMode()'));
  await check(); ok('4. ...and the saver starts', await q('!!SAVER')); await stop();
  // a saver whose picture something removed
  await check(); await q("SAVER.c.remove()"); await check();
  ok('5. a saver whose picture was taken away no longer blocks a new one', await q('!!SAVER&&SAVER.c.isConnected') && await q("__LOG.some(m=>/picture was gone/.test(m))"), await q('__LOG')); await stop();
  playing = 1; await check();
  ok('6. sound playing: the saver waits, said once', await q('!SAVER') && await q("__LOG.filter(m=>/sound is playing/.test(m)).length==1"));
  await check(); ok('...(not said again while nothing changes)', await q("__LOG.filter(m=>/sound is playing/.test(m)).length==1"));
  playing = 0; idle = 60000; await check(); ok('7. not idle long enough: no saver', await q('!SAVER'));
  ok('8. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
