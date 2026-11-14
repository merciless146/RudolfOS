// 1.4.4 (wallpaper31.js): a picture of your own as the desktop background, from your files or from this computer;
// its position; kept after a restart; a preset turns it off. The browser version (no rudolfd needed).
// node wallpaper_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path'), os = require('os');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.mjs': 'text/javascript', '.html': 'text/html'};
const errors = [];
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }
  const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8099, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const ctx = await b.newContext({viewport: {width: 1280, height: 720}});
  const open = async () => { const p = await ctx.newPage(); p.on('pageerror', e => { errors.push(e.message); console.log('page error:', e.stack.slice(0, 300)) });
    await p.goto('http://127.0.0.1:8099/'); await p.waitForFunction(() => typeof wpCss == 'function' && typeof launch == 'function', null, {timeout: 30000});
    await p.evaluate("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';const h=homePath();let n=fs;for(const x of h){if(!n[x])n[x]={};n=n[x]}return 1})()"); return p };
  let p = await open(); const q = s => p.evaluate(s);
  // a red picture in the user's files
  await q("(()=>{const c=document.createElement('canvas');c.width=800;c.height=450;const g=c.getContext('2d');g.fillStyle='#e11d48';g.fillRect(0,0,800,450);g.fillStyle='#fde047';g.fillRect(350,175,100,100);const h=homePath();let d=node(h);d['red.png']=c.toDataURL('image/png');try{fsSave()}catch(e){}window.__pick=[...h,'red.png'];pickVfsFile=async()=>window.__pick;return 1})()");
  await q("settings('disp')"); await wait(1200);
  ok('1. Display Properties: Picture, with Choose from my files and From this computer', await q("(()=>{const d=document.querySelector('.wp31');return !!d&&!!d.querySelector('.wpf')&&!!d.querySelector('.wpc')&&d.previousElementSibling.textContent=='Picture'})()"));
  await q("document.querySelector('.wp31 .wpf').click()"); await wait(1500);
  const bg = () => q("getComputedStyle(document.body).backgroundImage.slice(0,40)+' | '+getComputedStyle(document.body).backgroundSize");
  ok('2. a picture from my files becomes the background (kept as a JPEG no larger than the screen needs)', /url\("data:image\/jpeg/.test(await bg()) && /cover/.test(await bg()) && await q("S.wpname") == 'red.png', await bg());
  ok('...with a thumbnail, its name, Remove and Position', await q("(()=>{const d=document.querySelector('.wp31');return !!d.querySelector('.wpth')&&d.querySelector('.wpn').textContent=='red.png'&&!!d.querySelector('.wpr')&&!!d.querySelector('.wpfit')})()"));
  ok('...and Apply is offered', await q("secDirty('disp')"));
  await q("(()=>{const s=document.querySelector('.wp31 .wpfit');s.value='fit';s.dispatchEvent(new Event('change'))})()");
  ok('3. Position Fit shows the whole picture', /contain/.test(await bg()), await bg());
  await q("[...document.querySelectorAll('.sc .sfoot button')].find(b=>b.classList.contains('sfa')).click()"); await wait(300); await q("document.querySelector('.sdlg button:last-child').click()"); await wait(800);
  ok('...Apply keeps it', await q("JSON.parse(localStorage.getItem('bd95set')).wpimg==S.wpimg&&!!S.wpimg"));
  await p.close(); p = await open();
  await p.waitForFunction(() => /url\("data:image\/jpeg/.test(getComputedStyle(document.body).backgroundImage), null, {timeout: 10000}).catch(() => {});
  ok('4. after a restart of the page the picture is the background again (Fit)', /url\("data:image\/jpeg/.test(await bg()) && /contain/.test(await bg()), await bg());
  // from this computer
  const tmp = path.join(os.tmpdir(), 'wp-blue.png');
  const blue = await q("(()=>{const c=document.createElement('canvas');c.width=300;c.height=200;const g=c.getContext('2d');g.fillStyle='#2563eb';g.fillRect(0,0,300,200);return c.toDataURL('image/png')})()");
  fs.writeFileSync(tmp, Buffer.from(blue.split(',')[1], 'base64'));
  await q("settings('disp')"); await wait(1200);
  const [fc] = await Promise.all([p.waitForEvent('filechooser'), q("document.querySelector('.wp31 .wpc').click()")]);
  await fc.setFiles(tmp); await wait(1500);
  ok('5. a picture from this computer', await q("S.wpname") == 'wp-blue.png' && /url\("data:image\/jpeg/.test(await bg()), [await q("S.wpname"), await bg()]);
  ok('...only the new and the applied picture are kept', (await q("ld(WPIDS(),[]).length")) <= 2);
  await q("(()=>{const b=document.querySelector('.sc #b1');b.value='#336699';b.dispatchEvent(new Event('input',{bubbles:true}));return 1})()"); await wait(500);
  ok('6. choosing other colours turns the picture off', await q("!S.wpimg") && !/url\(/.test(await bg()), await bg());
  await q("[...document.querySelectorAll('.sc .sfoot button')].find(b=>b.classList.contains('sfv')).click()"); await wait(300); await q("document.querySelector('.sdlg button:last-child').click()"); await wait(1200);
  ok('...and Revert brings the applied picture back', await q("!!S.wpimg") && /url\("data:image\/jpeg/.test(await bg()), [await q("S.wpimg"), await bg()]);
  await q("document.querySelector('.wp31 .wpr').click()"); await wait(500);
  ok('7. Remove: the colours again', !/url\(/.test(await bg()) && await q("!S.wpimg"), await bg());
  ok('no page error', !errors.length, errors);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
