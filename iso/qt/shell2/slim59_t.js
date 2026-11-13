// 1.5.9 (plan 10.1): the dynamic backgrounds' pictures and the fonts are files beside the page (www/a/, named by their
// contents), not text inside it: the page is about 1.5 MB smaller, and a picture or a font is fetched only when shown
// or used.   node slim59_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path'), crypto = require('crypto');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 400) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.webp': 'image/webp', '.jpg': 'image/jpeg', '.png': 'image/png', '.woff2': 'font/woff2', '.woff': 'font/woff'};
const got = [];
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]); got.push(u);
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8269, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const html = fs.readFileSync(path.join(WWW, 'index.html'), 'utf8'), A = path.join(WWW, 'a');
  ok('1. the page is under 4.2 MB (5.5 MB in 1.5.8)', html.length < 4.2e6, html.length);
  ok('2. no fonts and no dynamic background pictures inside it', !/data:font\//.test(html) && !/const DYN=\{[^\n]*data:image/.test(html));
  const files = fs.readdirSync(A), refs = new Set([...html.matchAll(/a\/([0-9a-f]{16}\.(webp|jpg|png|woff2?))/g)].map(m => m[1]));
  ok('3. every file the page names is there, and no other', [...refs].every(f => files.includes(f)) && files.every(f => refs.has(f)), {refs: refs.size, files: files.length});
  ok('4. each file is named by its contents (a new picture gets a new name; an open desktop never mixes versions)', files.every(f => crypto.createHash('sha256').update(fs.readFileSync(path.join(A, f))).digest('hex').slice(0, 16) == f.split('.')[0]));
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1366, height: 768}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto('http://127.0.0.1:8269/'); await p.waitForFunction(() => typeof DYN != 'undefined' && typeof applySet == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});S.dyn='';applySet();return 1})()`); await wait(500);
  const before = got.filter(u => u.startsWith('/a/')).length;
  ok('5. with no dynamic background, no background picture is fetched at start', got.filter(u => u.startsWith('/a/') && u.endsWith('.webp')).length == 0, got.filter(u => u.startsWith('/a/')));
  await q("(()=>{S.dyn='oasis';S.dm='dark';applySet();return 1})()"); await wait(800);
  const fetched = got.filter(u => u.startsWith('/a/') && u.endsWith('.webp'));
  ok('6. choosing Spirit Falls (dark) fetches its picture (and its sky mask for the lanterns)', fetched.includes('/' + await q("DYN.oasis.d")) && fetched.length <= 3, fetched);
  ok('7. the background shows it', await q("new Promise(r=>{const i=new Image();i.onload=()=>r(i.naturalWidth==1600);i.onerror=()=>r(false);i.src=DYN.oasis.d})"));
  ok('8. a font is a file too, and loads when used', await q("(async()=>{const f=[...document.fonts].find(x=>/OpenDyslexic/.test(x.family));if(!f)return false;await f.load();return f.status=='loaded'})()"));
  ok('9. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  const k = R.filter(Boolean).length; console.log(`${k}/${R.length} passed`); process.exit(k == R.length ? 0 : 1);
})().catch(e => { console.log('ERROR', e.stack); process.exit(1) });
