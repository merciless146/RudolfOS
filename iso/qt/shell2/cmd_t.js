// Command Prompt and Lua (1.3.0): the version they print is Rudolf OS's own (RVER), not "1.1" / "1.0" as before.
// node cmd_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../..', 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.mjs': 'text/javascript', '.html': 'text/html'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }
  const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8096, '127.0.0.1');
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8096/'); await p.waitForFunction(() => typeof prompt == 'function' && typeof runLua == 'function', null, {timeout: 30000});
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});USER=USER||ACCTS[0];return 1})()");
  const v = await q('RVER');
  const P = await q("(()=>{window.__P=prompt();return __P.out?__P.out.textContent:document.querySelector('.win.active pre,.win.active .out')?.textContent||''})()");
  const txt = await q("[...document.querySelectorAll('.win')].map(w=>w.innerText).join('\\n')");
  ok('Command Prompt opens with this version', txt.includes(`Rudolf OS [Version ${v}]`) && !txt.includes('[Version 1.1]'), txt.slice(0, 120));
  await q("__P.exec('VER');1");
  const t2 = await q("[...document.querySelectorAll('.win')].map(w=>w.innerText).join('\\n')");
  ok('...and VER prints it', (t2.match(new RegExp(`Rudolf OS \\[Version ${v.replace(/\./g, '\\.')}\\]`, 'g')) || []).length >= 2, t2.slice(-150));
  const lua = await q("(async()=>{let o='';await runLua('print(sys.version())',{write:s=>o+=s,print:s=>o+=s+'\\n'},'T');return o})()");
  ok('Lua sys.version() gives it too (was "Rudolf OS 1.0")', lua.trim() == `Rudolf OS ${v}`, lua);
  await b.close(); srv.close();
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
