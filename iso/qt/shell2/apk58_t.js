// 1.5.8 (plan Part 7): an .apk opened says what the app is (read from its binary AndroidManifest.xml in the ZIP, never
// run) and offers Phone screen, its web version and the App Store; Phone screen sets itself up (Debian, then scrcpy)
// and shows how to turn on USB debugging. rudolfd stood in; the test app is made here (AXML written by hand).
// node apk58_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path'), zlib = require('zlib');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
// ---- a binary AndroidManifest.xml
function axml(els) { const strs = []; const si = s => { let i = strs.indexOf(s); if (i < 0) { strs.push(s); i = strs.length - 1 } return i };
  const chunks = [];
  for (const e of els) { const n = si(e.name); const at = Object.entries(e.at).map(([k, v]) => [si(k), v]);
    const b = Buffer.alloc(36 + at.length * 20); b.writeUInt16LE(0x102, 0); b.writeUInt16LE(16, 2); b.writeUInt32LE(b.length, 4); b.writeUInt32LE(1, 8); b.writeUInt32LE(0xffffffff, 12);
    b.writeUInt32LE(0xffffffff, 16); b.writeUInt32LE(n, 20); b.writeUInt16LE(20, 24); b.writeUInt16LE(20, 26); b.writeUInt16LE(at.length, 28);
    at.forEach(([k, v], i) => { const o = 36 + i * 20; b.writeUInt32LE(0xffffffff, o); b.writeUInt32LE(k, o + 4);
      if (typeof v == 'number') { b.writeUInt32LE(0xffffffff, o + 8); b.writeUInt16LE(8, o + 12); b[o + 15] = 16; b.writeUInt32LE(v, o + 16) }
      else { const s = si(v); b.writeUInt32LE(s, o + 8); b.writeUInt16LE(8, o + 12); b[o + 15] = 3; b.writeUInt32LE(s, o + 16) } });
    chunks.push(b) }
  const offs = []; let data = Buffer.alloc(0); for (const s of strs) { offs.push(data.length); const u = Buffer.alloc(2 + s.length * 2 + 2); u.writeUInt16LE(s.length, 0); for (let i = 0; i < s.length; i++) u.writeUInt16LE(s.charCodeAt(i), 2 + i * 2); data = Buffer.concat([data, u]) }
  while (data.length % 4) data = Buffer.concat([data, Buffer.from([0])]);
  const sp = Buffer.alloc(28 + strs.length * 4); sp.writeUInt16LE(1, 0); sp.writeUInt16LE(28, 2); sp.writeUInt32LE(sp.length + data.length, 4); sp.writeUInt32LE(strs.length, 8); sp.writeUInt32LE(0, 16); sp.writeUInt32LE(sp.length, 20);
  offs.forEach((o, i) => sp.writeUInt32LE(o, 28 + i * 4));
  const body = Buffer.concat([sp, data, ...chunks]); const h = Buffer.alloc(8); h.writeUInt16LE(3, 0); h.writeUInt16LE(8, 2); h.writeUInt32LE(8 + body.length, 4); return Buffer.concat([h, body]) }
function zip(files) { const locals = [], cds = []; let off = 0;
  for (const [name, raw] of files) { const c = zlib.deflateRawSync(raw), nb = Buffer.from(name);
    const l = Buffer.alloc(30); l.writeUInt32LE(0x04034b50, 0); l.writeUInt16LE(8, 8); l.writeUInt32LE(c.length, 18); l.writeUInt32LE(raw.length, 22); l.writeUInt16LE(nb.length, 26);
    const cd = Buffer.alloc(46); cd.writeUInt32LE(0x02014b50, 0); cd.writeUInt16LE(8, 10); cd.writeUInt32LE(c.length, 20); cd.writeUInt32LE(raw.length, 24); cd.writeUInt16LE(nb.length, 28); cd.writeUInt32LE(off, 42);
    locals.push(l, nb, c); cds.push(cd, nb); off += 30 + nb.length + c.length }
  const cdb = Buffer.concat(cds); const e = Buffer.alloc(22); e.writeUInt32LE(0x06054b50, 0); e.writeUInt16LE(files.length, 8); e.writeUInt16LE(files.length, 10); e.writeUInt32LE(cdb.length, 12); e.writeUInt32LE(off, 16);
  return Buffer.concat([...locals, cdb, e]) }
const man = axml([{name: 'manifest', at: {package: 'com.example.notesy', versionName: '3.2.1'}}, {name: 'uses-sdk', at: {minSdkVersion: 26}},
  {name: 'uses-permission', at: {name: 'android.permission.INTERNET'}}, {name: 'uses-permission', at: {name: 'android.permission.CAMERA'}}, {name: 'uses-permission', at: {name: 'com.example.CUSTOM'}},
  {name: 'application', at: {label: 'Notesy'}}]);
const APK = zip([['classes.dex', Buffer.alloc(5000, 7)], ['AndroidManifest.xml', man], ['res/x.png', Buffer.alloc(100)]]);
const M = {deb: {ok: true, installed: true, scrcpy: false}, posts: []};
const api = (a, method) => {
  if (method == 'POST') M.posts.push(a);
  if (a == 'debian/status') return M.deb;
  if (a == 'debian/scrcpy') { M.deb.scrcpy = true; return {ok: true} }
  if (a == 'job') return {ok: true, running: false, status: 0, log: 'done\n'};
  if (a == 'events') return {ok: true, events: ''};
  if (a == 'info') return {ok: true, backlight: false, live: 0, boot: 'uefi', kernel: 'test'};
  return {ok: true};
};
const srv = http.createServer((q, s) => { const [u0] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', async () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/big.apk') { const r = /bytes=(\d+)-(\d+)/.exec(q.headers.range || ''); if (q.method == 'HEAD') { s.writeHead(200, {'content-length': APK.length}); return s.end() }
      if (r) { s.writeHead(206, {'content-range': `bytes ${r[1]}-${r[2]}/${APK.length}`}); return s.end(APK.subarray(+r[1], +r[2] + 1)) } s.writeHead(200); return s.end(APK) }
    if (u == '/api/events/wait') await new Promise(r => setTimeout(r, 800));
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5) == 'events/wait' ? 'events' : u.slice(5).replace(/\?.*/, ''), q.method))) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8182, '127.0.0.1');
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const p = await (await b.newContext({viewport: {width: 1280, height: 900}})).newPage(); const errs = []; p.on('pageerror', e => errs.push(e.stack.slice(0, 300)));
  await p.goto('http://127.0.0.1:8182/'); await p.waitForFunction(() => typeof wzApk == 'function' && typeof phoneScreen == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';window.elevate=async()=>true;return 1})()");
  // 1. an .apk kept in the desktop's storage (a data: URL)
  await q(`(()=>{fs['C:']['notesy.apk']='data:application/vnd.android.package-archive;base64,${APK.toString('base64')}';return 1})()`);
  const info = await q("apkInfo(['C:','notesy.apk'])");
  ok('1. read from the file: name, package, version, Android 8, permissions', info.name == 'Notesy' && info.pkg == 'com.example.notesy' && info.version == '3.2.1' && info.minSdk == 26 && info.perms.length == 3, info);
  // 2. the wizard on opening it
  await q("openPath(['C:','notesy.apk'])"); await p.waitForSelector('.wz .apk58', {timeout: 8000});
  const t = await q("[...document.querySelectorAll('.wz')].pop().textContent");
  ok('2. opening it: "Notesy 3.2.1", Android 8 or later, the internet and the camera, 1 more', /Notesy 3\.2\.1/.test(t) && /Android 8 or later/.test(t) && /the internet, the camera/.test(t) && /and 1 more/.test(t), t.slice(0, 400));
  ok('...it says nothing was run or installed', /Nothing was run or installed/.test(t));
  const btn = await q("[...[...document.querySelectorAll('.wz')].pop().querySelectorAll('.wzf button')].map(b=>b.textContent)");
  ok('...and offers Phone screen, Web version, App Store', ['Phone screen', 'Web version', 'App Store'].every(x => btn.includes(x)), btn);
  // 3. a large .apk on the disk (a reference): read in ranges, not whole
  const big = await q("(async()=>{dlLink=async()=>'/big.apk';fs['C:']['big.apk']=DLREF+JSON.stringify({p:'/x/big.apk',s:"+APK.length+"});return apkInfo(['C:','big.apk'])})()");
  ok('3. a large .apk on the disk is read in parts (ranges): same answer', big.pkg == 'com.example.notesy', big);
  // 4. not an apk
  await q("fs['C:']['bad.apk']='data:application/octet-stream;base64,'+btoa('hello');1");
  const bad = await q("apkInfo(['C:','bad.apk']).then(()=>'',e=>e.message)");
  ok('4. a file that is not an Android app: said so', /not an Android app/.test(bad), bad);
  // 5. Phone screen: scrcpy not there yet -> Get it -> instructions
  await q("wins.slice().forEach(w=>w.close());phoneScreen()"); await p.waitForSelector('.phs58 .ps-i', {timeout: 8000});
  ok('5. Phone screen without scrcpy: offers to get it', true);
  await p.click('.phs58 .ps-i'); await p.waitForSelector('.phs58 .ps-go', {timeout: 10000});
  ok('...Get it: installs scrcpy in Debian (rudolfd debian/scrcpy), then shows how', M.posts.includes('debian/scrcpy'), M.posts);
  const how = await q("document.querySelector('.phs58').textContent");
  ok('...the steps: USB debugging by the owner, never changed by Rudolf OS', /USB debugging/.test(how) && /never changes this for you/.test(how), how.slice(0, 300));
  M.posts = []; await p.click('.phs58 .ps-go'); await p.waitForTimeout(500);
  ok('6. Show the phone: starts scrcpy in Debian', M.posts.some(x => x == 'debian/launch'), M.posts);
  M.deb = {ok: true, installed: false}; await q("wins.slice().forEach(w=>w.close());phoneScreen()"); await p.waitForSelector('.phs58 .ps-l', {timeout: 8000});
  ok('7. without Linux set up: offers to set it up first', true);
  ok('8. Phone screen is in the Start menu', await q("!!APPS.phonescreen"));
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); const k = R.filter(Boolean).length; console.log(`${k}/${R.length} passed`); process.exit(k == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
