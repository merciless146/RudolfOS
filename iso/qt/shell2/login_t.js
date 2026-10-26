// Login screen 1.2.7: the power button (shut down, restart or sleep without signing in), in the desktop page with a
// stand-in rudolfd, in Chromium. node login_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.mjs': 'text/javascript', '.html': 'text/html'};
const M = {calls: []};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html' || u == '/plain.html') { s.writeHead(200, {'content-type': 'text/html'}); const h = fs.readFileSync(path.join(WWW, 'index.html'), 'utf8');
      // plain.html: the same page outside Rudolf OS (no rudolfd), where there is no computer to switch off
      return s.end(u == '/plain.html' ? h : h.replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/api/')) { M.calls.push(u.slice(5) + (qs ? '?' + qs : '')); s.writeHead(200, {'content-type': 'application/json'}); return s.end('{"ok":true}') }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(8092, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  p.on('pageerror', e => console.log('page error:', e.message));
  const q = s => p.evaluate(s);
  // past the first-start setup, to the login screen
  await p.goto('http://127.0.0.1:8092/'); await p.waitForFunction(() => typeof shutdownDlg == 'function', null, {timeout: 30000});
  await q("try{svj('bd95setup',1)}catch(e){};1"); await p.reload(); await p.waitForFunction(() => typeof shutdownDlg == 'function', null, {timeout: 30000});
  await q("(()=>{const o=document.getElementById('oobe');o&&o.remove();USER=null;showLogin();return 1})()"); await wait(500);
  const vis = await q("(()=>{const b=document.getElementById('lgpower'),L=document.getElementById('login');if(!b)return 'no button';const r=b.getBoundingClientRect();return getComputedStyle(L).display!='none'&&r.width>30&&r.right<=innerWidth&&r.bottom<=innerHeight&&document.elementFromPoint(r.x+r.width/2,r.y+r.height/2)===b?'shown':'hidden'})()");
  ok('the login screen has a power button, in the corner and on top', vis == 'shown', vis);
  ok('...named for screen readers', await q("document.getElementById('lgpower').getAttribute('aria-label')") == 'Power: shut down, restart or sleep');
  await q("document.getElementById('lgpower').click();1"); await wait(300);
  const opts = await q("[...document.querySelectorAll('.rmodal button')].map(b=>b.textContent).join('|')");
  ok('...it opens the Power dialog above the login screen', /Shut down/.test(opts) && /Restart/.test(opts) && /Sleep/.test(opts) && await q("(()=>{const b=[...document.querySelectorAll('.rmodal button')].find(b=>/Shut down/.test(b.textContent));const r=b.getBoundingClientRect();return document.elementFromPoint(r.x+r.width/2,r.y+r.height/2)===b})()"), opts);
  M.calls = []; await q("[...document.querySelectorAll('.rmodal button')].find(b=>b.textContent=='Cancel').click();1"); await wait(300);
  ok('Cancel goes back to the login screen and switches nothing off', !await q("!!document.querySelector('.rmodal')") && await q("getComputedStyle(document.getElementById('login')).display!='none'") && !M.calls.some(c => /^power/.test(c)), M.calls);
  await q("document.getElementById('lgpower').click();1"); await wait(300);
  await q("[...document.querySelectorAll('.rmodal button')].find(b=>/Shut down/.test(b.textContent)).click();1"); await wait(1500);
  ok('Shut down switches the computer off, without anyone signed in', M.calls.includes('power?action=poweroff') && await q("USER==null"), M.calls);
  await p.goto('http://127.0.0.1:8092/plain.html'); await p.waitForFunction(() => typeof shutdownDlg == 'function', null, {timeout: 30000});
  ok('outside Rudolf OS (no rudolfd) there is no power button', !await q("!!document.getElementById('lgpower')"));
  await b.close(); srv.close();
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
