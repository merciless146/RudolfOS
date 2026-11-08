// 1.5.6: Oracle's Downloads page and its links go to the account's own Downloads (C:\USERS\<name>\DOWNLOADS), not the
// C:\DOWNLOADS that 1.5.5 removed (found on the Vivobook: "Open" gave 404 Not found, "Open Downloads folder" gave
// Folder not found); a file kept on the disk opened from Oracle is opened the way File Explorer opens it; a small
// file Oracle downloads itself goes into the account's Downloads.   node oracledl_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8093, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd|debian/.test(e.stack || e.message || '')) errs.push(e.message) });
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8093/'); await p.waitForFunction(() => typeof secPage == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});window.__sys=window.__sys||{};
    ACCTS.length=0;ACCTS.push({n:'boss',h:await pwMake('b1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'b1');
    const d=node(downloadsPath());d['NOTE.TXT']='hello';d['DISCORD.DEB']=dlMkRef('/persist/accounts/x/Downloads/DISCORD.DEB',2387236,'full');fsSave();
    DLS.length=0;DLS.push({n:'DISCORD.DEB',u:'https://discord.com/api/download?platform=linux&format=deb',t:Date.now(),st:'saved',disk:1,sz:2387236});return 1})()`);
  const dp = await q("downloadsPath().join('/')");
  // the Downloads page
  const pg = await q("(()=>{const lk=(h,t)=>`<a href='${h}'>${t}</a>`,esc=s=>String(s);return secPage('about:downloads',(t,h)=>h,lk,esc)})()");
  ok('1. Downloads page: "Open" and "Open Downloads folder" go to the account\'s Downloads', dp == 'C:/USERS/BOSS/DOWNLOADS' && pg.includes(`file:///${dp}/DISCORD.DEB`) && pg.includes(`about:files?p=${dp}`) && !/C:\/DOWNLOADS|C:\\\\DOWNLOADS/.test(pg) && /Saved to Downloads/.test(pg), [dp, pg.slice(0, 900)]);
  // Oracle itself
  const frame = async (u) => { await q(`(()=>{wins.slice().forEach(w=>w.close());browser(${JSON.stringify(u)});return 1})()`); await wait(900);
    return q("(()=>{const f=[...document.querySelectorAll('.win iframe')].pop();return f?(f.getAttribute('srcdoc')||''):''})()") };
  let h = await frame('about:files?p=' + dp);
  ok('2. the Downloads folder opens in Oracle and lists its files', /DISCORD\.DEB/.test(h) && /NOTE\.TXT/.test(h) && !/Folder not found/.test(h), h.slice(0, 600));
  await q("window.__op=[];window.__opOrig=openPath;openPath=function(p){__op.push(p.join('/'))}");
  h = await frame(`file:///${dp}/DISCORD.DEB`);
  const op = await q("__op");
  ok('3. a file kept on the disk (DISCORD.DEB) is opened the way File Explorer opens it, not "404 Not found"', op.includes(dp + '/DISCORD.DEB') && !/404 Not found/.test(h) && /kept on the disk/.test(h), [op, h.slice(0, 400)]);
  await q("openPath=__opOrig");
  h = await frame(`file:///${dp}/NOTE.TXT`);
  ok('4. a small file still opens in Oracle', /hello/.test(h), h.slice(0, 300));
  // a small file Oracle downloads itself (in the browser version every download is)
  await q("dlStart('data:text/plain,small%20file',{name:'small.txt'})"); await wait(1500);
  const s = await q("(()=>({mine:node([...downloadsPath(),'SMALL.TXT']),old:isDir(fs['C:'].DOWNLOADS)}))()");
  ok('5. a small download goes into the account\'s Downloads, and C:\\DOWNLOADS is not made again', s.mine == 'small file' && !s.old, s);
  // opening the .deb: what to do instead (the App Store)
  const dw = await q(`(async()=>{wins.slice().forEach(w=>w.close());dlLink=async()=>'about:blank';await dlOpen([...downloadsPath(),'DISCORD.DEB']);const w=wins.at(-1);const t=w&&w.body.textContent;let st=false;const bt=w&&[...w.body.querySelectorAll('button')].find(x=>/App Store/.test(x.textContent));if(bt){bt.click();await new Promise(r=>setTimeout(r,800));st=wins.some(x=>/App Store/i.test(x.title||x.el&&x.el.textContent||''))}return {t,st}})()`);
  ok('6. opening DISCORD.DEB says Debian packages are not installed from a file, and offers the App Store', dw && /package for Debian/.test(dw.t) && dw.st, dw);
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
