// 1.5.8: when the memory guard ends a web page ("memory page PID MB" in rudolfd's events), the desktop leaves full
// screen at once and says what happened, also when only a frame inside a tab ended (a game on Newgrounds), unless
// Oracle's own "closed to free memory" page already says so.   node memfs58_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 300) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8267, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto('http://127.0.0.1:8267/'); await p.waitForFunction(() => typeof evDispatch == 'function' && typeof fsLeave == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});window.__sent=[];SHELL={postMessage:m=>__sent.push(m)};document.querySelectorAll('.ntoast').forEach(x=>x.remove());return 1})()`);
  await q("(()=>{evDispatch('memory page 14355 1940\\n');return 1})()"); await wait(200);
  ok('1. the guard ended a page: the desktop asks the shell to leave full screen at once', await q("__sent.some(m=>m.rudolfShell=='tab'&&m.op=='unfs')"), await q('__sent'));
  await wait(3200);
  const t1 = await q("[...document.querySelectorAll('.ntoast')].map(x=>x.textContent).join('|')");
  ok('2. ...and says so (the size in GB)', /Part of a page was closed to free memory/.test(t1) && /1\.9 GB/.test(t1), t1);
  await q("(()=>{document.querySelectorAll('.ntoast').forEach(x=>x.remove());window.__memShown=0;evDispatch('memory page 200 640\\n');setTimeout(()=>{window.__memShown=Date.now()},500);return 1})()"); await wait(3500);
  const t2 = await q("[...document.querySelectorAll('.ntoast')].map(x=>x.textContent).join('|')");
  ok('3. a whole tab ended (Oracle shows its own page): no second notification', !/Part of a page/.test(t2), t2);
  const c = fs.readFileSync(path.join(__dirname, '../../sys/rudolf-memguard.c'), 'utf8');
  ok('4. rudolf-memguard writes "memory page PID MB" to the events when it ends a page', /"memory page %d %lld\\n", v->pid, v->rss >> 20\);\s*f = open\(RUN "\/events"/.test(c));
  ok('5. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  const k = R.filter(Boolean).length; console.log(`${k}/${R.length} passed`); process.exit(k == R.length ? 0 : 1);
})().catch(e => { console.log('ERROR', e.stack); process.exit(1) });
