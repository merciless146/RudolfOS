// 1.5.9: on installed Rudolf OS the start-up chime plays when the sign-in screen appears, without waiting for a key or a
// click; the browser build (no rudolfd) still waits for the first key or click. A stand-in rudolfd.   node chime59_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www'), J = {'content-type': 'application/json'};
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 300) : '')) };
const srv = http.createServer((q, s) => { const u = q.url.split('?')[0]; q.on('data', () => {}); q.on('end', () => {
  if (u == '/' || u == '/web') { s.writeHead(200, {'content-type': 'text/html'}); let h = fs.readFileSync(path.join(WWW, 'index.html'), 'utf8');
    if (u == '/') h = h.replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>'); return s.end(h) }
  if (u.startsWith('/a/')) { const f = path.join(WWW, u); if (fs.existsSync(f)) { s.writeHead(200); return s.end(fs.readFileSync(f)) } }
  if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200, J); s.end('{"ok":true,"events":[]}') }, 20000); s.writeHead(200, J); return s.end('{"ok":true}') }
  s.writeHead(404); s.end() }) }).listen(8291, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--autoplay-policy=no-user-gesture-required']});
  const setup = `(()=>{try{svj('bd95setup',1)}catch(e){}const o=document.getElementById('oobe');o&&o.remove();return 1})()`;
  // installed: the sign-in screen at start-up
  let p = await b.newPage(); await p.goto('http://127.0.0.1:8291/'); await p.waitForFunction(() => typeof sfx == 'function', null, {timeout: 30000});
  await p.evaluate(setup); await wait(2500);
  const st = await p.evaluate("({shown:getComputedStyle(document.getElementById('login')).display!='none',sfx:window.__sfx.map(x=>x[0]),ac:typeof AC=='object'&&AC?AC.state:null})");
  ok('1. installed: the sign-in screen is up and the start-up chime has played, with no key or click', st.shown && st.sfx.includes('startup'), st);
  await p.keyboard.press('Shift'); await wait(300);
  ok('2. ...a key afterwards does not play it again', (await p.evaluate("window.__sfx.filter(x=>x[0]=='startup').length")) == 1);
  await p.close();
  // the browser build: waits for the first key or click (browsers block sound before it)
  p = await b.newPage(); await p.goto('http://127.0.0.1:8291/web'); await p.waitForFunction(() => typeof sfx == 'function', null, {timeout: 30000});
  await p.evaluate(setup); await wait(2500);
  ok('3. the browser build: not before a key or click', !(await p.evaluate("window.__sfx.some(x=>x[0]=='startup')")));
  await p.evaluate("document.getElementById('login').style.display='flex'"); await p.mouse.click(5, 5); await wait(300);
  ok('4. ...and at the first click', await p.evaluate("window.__sfx.some(x=>x[0]=='startup')"));
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
