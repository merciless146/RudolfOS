// Downloads 1.2.7: where downloads go (C:\DOWNLOADS, which Files' "Downloads" now opens) and a Rudolf OS release's own
// disc files (.partNN, .sha256, .iso) past the file-type rules, in the desktop page with a stand-in rudolfd, in
// Chromium. node dlrel_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.mjs': 'text/javascript', '.html': 'text/html'};
// the stand-in rudolfd: incoming downloads in memory; "keep" moves one to the Downloads folder on the disk
const IN = new Map(), KEPT = new Map(), M = {calls: []};
const api = (a, q) => { const p = new URLSearchParams(q || ''); M.calls.push(a + '?' + (q || ''));
  if (a == 'dl/file') { const n = p.get('name'), to = p.get('to');
    if (to == 'keep' || to == 'release') { KEPT.set(n, IN.get(n)); IN.delete(n); return {ok: true, path: '/home/rudolf/Downloads/' + n, size: (KEPT.get(n) || '').length, name: n} }
    if (to == 'del') { IN.delete(n); return {ok: true} }
    if (to == 'quar') return {ok: true, name: n, path: '/home/rudolf/Downloads/.quarantine/' + n} }
  if (a == 'dl/space') return {ok: true, free: 50e9, total: 100e9};
  return {ok: true};
};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/dl/read') { const p = new URLSearchParams(qs), b = Buffer.from(IN.get(p.get('name')) || KEPT.get(p.get('name')) || '');
      const off = +p.get('off'), len = +p.get('len'), st = off < 0 ? Math.max(0, b.length + off) : off; s.writeHead(200, {'content-type': 'application/octet-stream'}); return s.end(b.subarray(st, st + len)) }
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs))) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(8090, '127.0.0.1');
const REL = 'https://release-assets.githubusercontent.com/github-production-release-asset/1414456577/faa18e74-80b0-4bd7?sp=r&response-content-disposition=attachment%3B+filename%3D';
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  p.on('pageerror', e => console.log('page error:', e.message));
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8090/'); await p.waitForFunction(() => typeof dlIngest == 'function' && typeof explorer == 'function', null, {timeout: 30000});
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});USER=USER||ACCTS[0];USER.r='admin';window.elevate=async()=>true;SECC.pol.unk=false;return 1})()");
  // one download through the same path Oracle's downloads take: in, checked, kept or blocked
  const get = async (name, url, body) => { IN.set(name, body); return q(`(async()=>{const rec={n:dlCName(${JSON.stringify(name)}),u:${JSON.stringify(url)},t:Date.now(),sz:0,st:'wait',why:''};
    DLS.push(rec);await dlIngest({name:${JSON.stringify(name)},size:${body.length}},rec);return JSON.stringify({st:rec.st,why:rec.why,path:rec.path||''})})()`).then(JSON.parse) };
  const big = 'x'.repeat(2 * 1048576), r = {};
  r.part = await get('rudolf-os-1.2.6.iso.part00', REL + 'rudolf-os-1.2.6.iso.part00', big);
  r.sha = await get('rudolf-os-1.2.6.iso.sha256', REL + 'rudolf-os-1.2.6.iso.sha256', 'f'.repeat(64) + '  rudolf-os-1.2.6.iso\n');
  r.iso = await get('rudolf-os-1.2.6.iso', 'https://github.com/merciless146/RudolfOS/releases/download/disc-1.2.6/rudolf-os-1.2.6.iso', big);
  ok('13. a release part from the Rudolf OS releases is kept (with unknown types blocked)', r.part.st == 'saved', r.part);
  ok('...its .sha256 too (small: kept in C:\\DOWNLOADS itself)', r.sha.st == 'saved' && /^f{64}  rudolf-os-1\.2\.6\.iso/.test(await q("(()=>{const v=String(node(['C:','DOWNLOADS','RUDOLF-OS-1.2.6.ISO.SHA256'])||'');return v.startsWith('data:')?atob(v.split(',')[1]):v})()")), r.sha);
  ok('...and a whole disc image from the release address (.iso is otherwise a forbidden program type)', r.iso.st == 'saved', r.iso);
  ok('...the activity log says why they were allowed', /Allowed RUDOLF-OS-1\.2\.6\.ISO\.PART00: a Rudolf OS disc file from the Rudolf OS releases/.test(await q("JSON.stringify(SLOG.av)")));
  const other = await get('rudolf-os-1.2.6.iso.part01', 'https://example.org/rudolf-os-1.2.6.iso.part01', big);
  ok('...the same name from another site is blocked', other.st == 'blocked', other);
  const repo2 = await get('rudolf-os-1.2.6.iso.part02', 'https://release-assets.githubusercontent.com/github-production-release-asset/99999/x', big);
  ok('...and from another repository\'s releases', repo2.st == 'blocked', repo2);
  await q("window.elevate=async()=>false;1");   // another .iso is a program file: kept only if an administrator approves
  const iso = await get('random.iso', REL + 'random.iso', big);
  ok('...another .iso from the release address is a program file again (blocked without an administrator\'s approval)', iso.st == 'blocked', iso);
  await q("window.elevate=async()=>true;1");
  // 12. Downloads in one place
  ok('12. downloads are in C:\\DOWNLOADS', await q("!!node(['C:','DOWNLOADS','RUDOLF-OS-1.2.6.ISO.PART00'])"));
  await q("wins.slice().forEach(w=>w.close());explorer();1"); await new Promise(r => setTimeout(r, 1200));
  await q("(()=>{const W=wins.find(w=>w.app=='explorer');const e=[...W.body.querySelectorAll('span')].find(x=>x.textContent.trim()=='Downloads');e.click();return 1})()"); await new Promise(r => setTimeout(r, 800));
  const shown = await q("(()=>{const W=wins.find(w=>w.app=='explorer');return W.title||W.body.closest('.win')?.querySelector('.wt')?.textContent||document.querySelector('.win.active .wt')?.textContent||''})()");
  const rows = await q("[...wins.find(w=>w.app=='explorer').body.querySelectorAll('tr[data-k],.xi,.fi')].map(x=>x.textContent).join('|')");
  ok('...and Files\' "Downloads" opens that folder, with them in it', /RUDOLF-OS-1\.2\.6\.ISO\.PART00/.test(rows), [shown, rows.slice(0, 200)]);
  await b.close(); srv.close();
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
