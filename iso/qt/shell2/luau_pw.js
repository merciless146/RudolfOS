// Luau in a real browser (1.2.5): real Workers and WebAssembly exceptions, in Chromium. node luau_pw.js [CHROME]
// Serves iso/www on 127.0.0.1:8097 and loads desktop/luau30.js into a page there.
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.html': 'text/html', '.luau': 'text/plain'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  if (u == '/t.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end('<!doctype html><title>t</title><script>const RD=null;</script>') }
  const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f)) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8097, '127.0.0.1');
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  await p.goto('http://127.0.0.1:8097/t.html'); await p.addScriptTag({content: fs.readFileSync(path.join(ROOT, 'desktop/luau30.js'), 'utf8')});
  let t0 = Date.now(), r = await p.evaluate(() => luauRun('print("hi", 2^10)'));
  ok('runs in a real Worker', r.code == 0 && r.out == 'hi\t1024\n', [r, Date.now() - t0 + ' ms']);
  r = await p.evaluate(() => luauRun('local t = nil\nprint(t.x)'));
  ok('an error comes back through the WebAssembly exception path', r.code == 1 && /attempt to index nil/.test(r.out), r.out);
  t0 = Date.now(); r = await p.evaluate(() => luauRun('while true do end', {limit: 2000}));
  ok('an endless loop is stopped near its limit', r.code == 124 && Date.now() - t0 < 5000, [r.out, Date.now() - t0]);
  t0 = Date.now(); r = await p.evaluate(() => luauCheck('local s = GetPart("Screen")\nif s then s:Nonsense() end', {kind: 'pilot'}));
  const first = Date.now() - t0;
  ok('the Pilot.lua checker loads and finds the error', r.ok && r.problems.length == 1 && r.problems[0].line == 2, [r.out || r.error, first + ' ms with loading']);
  t0 = Date.now(); r = await p.evaluate(() => luauCheck('local s = GetPart("Screen")\nif s then s:ClearElements() end', {kind: 'pilot'}));
  ok('...and a second check reuses the loaded definitions', r.ok && r.problems.length == 0 && Date.now() - t0 < first, Date.now() - t0 + ' ms');
  r = await p.evaluate(() => luauBlitzen('local d = GetPart("Disk")\nif d then d:Write("k", 1) print(d:Read("k")) end', 'pilot'));
  ok('Blitzen\'s check-then-run for Pilot.lua', r.code == 0 && /\n1\n$/.test(r.out), r.out);
  await b.close(); srv.close();
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
