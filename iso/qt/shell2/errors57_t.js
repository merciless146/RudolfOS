// 1.5.7 (plan stage 6): clear errors. rudolfd's texts a person can meet are said as what to do, each with its Help
// page (a notification with one opens it); the texts the desktop's code looks for stay; every entry matches a text
// rudolfd really sends; a held-back download is offered again.   node errors57_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 600) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm', '.json': 'application/json'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8166, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await (await b.newContext()).newPage();
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.addInitScript(() => {
    window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0}; window.DLI = [];
    const f0 = window.fetch;
    window.fetch = (u, o) => { u = String(u); const q = new URL(u, location.href); const J = (x, st) => Promise.resolve(new Response(JSON.stringify(x), {status: st || 200, headers: {'content-type': 'application/json'}}));
      if (q.pathname == '/api/test/err') return J({ok: false, error: q.searchParams.get('m')}, 500);
      if (q.pathname == '/api/dl/incoming') return J({ok: true, items: DLI});
      if (u.includes('/api/')) return J({ok: true, items: [], apps: [], windows: []});
      return f0(u, o) } });
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8166/'); await p.waitForFunction(() => typeof errFriendly == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  // 1. every entry matches a text rudolfd sends
  const src = fs.readFileSync(path.join(ROOT, 'iso/sys/rudolfd.c'), 'utf8') + fs.readFileSync(path.join(ROOT, 'iso/sys/rudolfd-home.inc'), 'utf8');
  const sent = [...src.matchAll(/reply_err\(r->fd, \d+, "([^"]*)"\)/g)].map(m => m[1].replace(/\\'/g, "'"));
  const srcs = await q("ERR57.map(e=>[e[0].source,e[0].flags])");
  const unmatched = srcs.filter(([s, f]) => !sent.some(t => new RegExp(s, f).test(t)));
  ok('1. every clear text replaces a text rudolfd really sends', !unmatched.length, unmatched);
  const tr = m => q(`rd('test/err?m='+encodeURIComponent(${JSON.stringify(m)})).then(()=>null,e=>({m:e.message,raw:e.raw||null,help:e.help||null}))`);
  let r = await tr('write failed (drive full?)');
  ok('2. "write failed (drive full?)" says what to do, with the Help page; the old text is kept as raw', /^The disk is full\. Empty the Trash/.test(r.m) && r.raw == 'write failed (drive full?)' && r.help == 'files', r);
  r = await tr('rpk failed'); ok('3. "rpk failed" becomes what to do (Packages help)', /could not be installed\. Check the internet/.test(r.m) && r.help == 'packages', r);
  r = await tr('the file was changed meanwhile'); ok('4. texts the desktop looks for stay as they are', r.m == 'the file was changed meanwhile' && !r.raw, r);
  r = await tr('busy: the Debian sandbox is starting'); ok('5. ..."busy: " too', r.m == 'busy: the Debian sandbox is starting', r);
  r = await tr('something else entirely'); ok('6. an unknown text is unchanged', r.m == 'something else entirely', r);
  ok('7. rdPost goes through it too', /Rudolf OS is busy with another task/.test(await q("rdPost('test/err?m='+encodeURIComponent('another job is running')).then(()=>'',e=>e.message)")));
  // 8. the notification opens its Help page
  await q("(async()=>{document.querySelectorAll('.ntoast').forEach(x=>x.remove());try{await rd('test/err?m='+encodeURIComponent('no DHCP answer on this network'))}catch(e){notify('Not connected',e.message,{kind:'error'})}return 1})()"); await wait(300);
  await q("(()=>{const t=[...document.querySelectorAll('.ntoast')].find(t=>/Not connected/.test(t.textContent));t.click();return 1})()"); await wait(800);
  ok('8. clicking the notification opens Help at Troubleshooting', await q("(()=>{const W=wins.find(w=>w.app=='help');return !!W&&/Troubleshooting/.test((W.body.querySelector('.pkr.on')||{}).textContent||'')})()"));
  // 9. a download the browser held back is offered again
  await q("(()=>{DLS.length=0;DLN.pend.length=0;const r=dlNative('https://example.com/setup.deb');r.part='Unconfirmed 123.crdownload';r.gs=10;r.gt=Date.now()-200e3;r.sz=10;return 1})()");
  await q("(()=>{DLI.push({name:'Unconfirmed 123.crdownload',size:10,mtime:Date.now()-200e3,partial:true});document.querySelectorAll('.ntoast').forEach(x=>x.remove());return 1})()");
  await q("dlPoll()"); await wait(500);
  const dn = await q("(()=>{const t=[...document.querySelectorAll('.ntoast')].find(t=>/Download stopped/.test(t.textContent));return t?t.textContent:null})()");
  ok('9. a held-back download: the notification offers it again', dn && /held this file back/.test(dn) && /Click here to download it again/.test(dn), dn);
  await q("(()=>{DLI.length=0;window.__ds=[];const o=dlStart;dlStart=function(u){__ds.push(u)};const t=[...document.querySelectorAll('.ntoast')].find(t=>/Download stopped/.test(t.textContent));t.click();dlStart=o;return 1})()");
  ok('10. ...clicking it downloads the same address again', JSON.stringify(await q("__ds")) == '["https://example.com/setup.deb"]', await q("__ds"));
  ok('11. about:downloads has "Download again" for a failed download', /act\("redl",0\)/.test(await q("secPage('about:downloads',(t,h)=>h,(u,t)=>t,s=>String(s))")));
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
