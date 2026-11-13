// 1.5.9 (plan 8.1): User Settings › Developer: a publisher key made in the desktop. Its private half only in a backup
// protected by a passphrase (in Documents); the account keeps the public half and its fingerprint; the backup opens with
// the passphrase into the same raw key rpk-tool.py signs with. A stand-in rudolfd.   node devkey59_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path'), crypto = require('crypto');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
const srv = http.createServer((q, s) => { const u = q.url.split('?')[0]; q.on('data', () => {}); q.on('end', () => {
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
  if (u.startsWith('/a/')) { const f = path.join(WWW, u); if (fs.existsSync(f)) { s.writeHead(200); return s.end(fs.readFileSync(f)) } }
  if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000); s.writeHead(200, {'content-type': 'application/json'}); return s.end('{"ok":true}') }
  s.writeHead(404); s.end() }) }).listen(8281, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8281/'); await p.waitForFunction(() => typeof dkMake == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x'}];USER=ACCTS[0];const h=['C:','USERS','ANN','DOCUMENTS'];let n=fs;for(const x of h){if(!n[x])n[x]={};n=n[x]}const d=document.createElement('div');d.id='ut';document.body.appendChild(d);usrPanelS(d);return 1})()`); await wait(500);
  ok('1. User Settings › Developer: no key yet, make one or use a backup', /Developer: publisher key/.test(await q("document.querySelector('#ut .dk59')?.textContent||''")) && await q("!!document.querySelector('#ut .dkm')"));
  await q("document.querySelector('#ut .dkm').click()"); await wait(300);
  await p.type('.rmodal .dk0', 'short'); await p.type('.rmodal .dk1', 'short'); await q("document.querySelector('.rmodal .y').click()"); await wait(400);
  ok('2. a passphrase under 12 characters is refused', await q("!document.querySelector('#ut .dkp')"));
  await p.type('.rmodal .dk0', 'correct horse battery'); await p.type('.rmodal .dk1', 'correct horse battery'); await q("document.querySelector('.rmodal .y').click()"); await wait(4000);
  const st = await q("(()=>{const k=dkGet();const f=node(['C:','USERS','ANN','DOCUMENTS',DK_FILE]);return {k,f}})()");
  const bk = st.f && JSON.parse(st.f);
  ok('3. made: the backup in Documents (encrypted: no private key in it as text), the public half and fingerprint kept for the account', bk && bk.format == 'rudolf-publisher-key 1' && bk.rounds == 600000 && st.k && st.k.pub == bk.pub && /^[0-9A-F]{4}( [0-9A-F]{4}){3}$/.test(st.k.fp) && !/BEGIN|PRIVATE/.test(st.f), st);
  ok('4. ...shown in User Settings', (await q("document.querySelector('#ut .dkp')?.value")) == bk.pub && new RegExp(st.k.fp).test(await q("document.querySelector('#ut .dk59').textContent")));
  const raw = await q(`dkOpen(${JSON.stringify(bk)},'correct horse battery')`);
  const priv = crypto.createPrivateKey({key: Buffer.concat([Buffer.from('302e020100300506032b657004220420', 'hex'), Buffer.from(raw, 'base64')]), format: 'der', type: 'pkcs8'});
  const pub = crypto.createPublicKey(priv).export({format: 'der', type: 'spki'}).slice(-32).toString('base64');
  ok('5. the backup opens with the passphrase into the raw key rpk-tool signs with, whose public half is the one shown', pub == bk.pub, {pub, bk: bk.pub});
  const bad = await q(`dkOpen(${JSON.stringify(bk)},'wrong passphrase!!').then(()=>'opened',e=>e.message)`);
  ok('6. a wrong passphrase does not open it', bad == 'Wrong passphrase.', bad);
  await q("document.querySelector('#ut .dkf').click()"); await wait(200);
  ok('7. forgotten on this computer: the backup stays in Documents', await q("!dkGet()&&!!node(['C:','USERS','ANN','DOCUMENTS',DK_FILE])"));
  await q("(()=>{pickVfsFile=async()=>['C:','USERS','ANN','DOCUMENTS',DK_FILE];document.querySelector('#ut .dki').click();return 1})()"); await wait(300);
  await p.type('.rmodal .dk0', 'correct horse battery'); await q("document.querySelector('.rmodal .y').click()"); await wait(3000);
  ok('8. used again from the backup and its passphrase', (await q("dkGet()&&dkGet().pub")) == bk.pub);
  ok('9. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
