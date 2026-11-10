// 1.5.6 (moved from the 1.5.7 plan): the logo and the version are one Start button. It does only what Start does
// (a click: the Start menu; a right-click: its menu), shows the whole version (1.5.6, 1.10.0, 2.0, 10.3.1 … never
// cut), beside the logo at the bottom or the top and under it at the left or the right. Versions are compared part by
// part as numbers (verCmp).   node startbtn56_t.js [CHROME] [SHOT-DIR]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8163, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
const SHOT = process.argv[3];
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd|debian/.test(e.stack || e.message || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8163/'); await p.waitForFunction(() => typeof verCmp == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    ACCTS.length=0;ACCTS.push({n:'ann',h:await pwMake('a1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'a1');return 1})()`);
  await wait(400);
  const st = await q("(()=>{const s=document.getElementById('start');return{osver:!!document.getElementById('osver'),img:!!s.querySelector('img'),txt:(s.querySelector('.stv')||{}).textContent,label:s.getAttribute('aria-label')}})()");
  ok('1. one button: the logo and "Rudolf OS" with the version, no second button beside it', !st.osver && st.img && st.txt == 'Rudolf OS ' + await q('RVER'), st);
  ok('2. screen readers and the keyboard know it as Start', st.label == 'Start', st.label);
  // a click on its words: the Start menu, not Settings
  await q("(()=>{wins.slice().forEach(w=>w.close());document.querySelector('#start .stv').click();return 1})()"); await wait(200);
  ok('3. a click (on the version too) opens the Start menu, nothing else', await q("getComputedStyle(document.getElementById('menu')).display") == 'flex' && await q("wins.length") == 0, [await q("getComputedStyle(document.getElementById('menu')).display"), await q("wins.map(w=>w.title)")]);
  await q("(()=>{document.getElementById('menu').style.display='none';const t=document.querySelector('#start .stv');const r=t.getBoundingClientRect();t.dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,cancelable:true,clientX:r.x+5,clientY:r.y+5}));return 1})()"); await wait(200);
  const cm = await q("(()=>{const m=document.querySelector('.cmenu');return m?m.textContent:''})()");
  ok('4. a right-click opens Start\'s own menu (not Settings › About)', cm.length > 0 && await q("wins.length") == 0, cm);
  await q("(()=>{document.querySelectorAll('.cmenu').forEach(m=>m.remove());return 1})()");
  // the whole version, whatever it is
  const fit = v => q(`(()=>{const t=document.querySelector('#start .stv'),was=t.textContent;t.textContent='Rudolf OS ${v}';const s=document.getElementById('start');
    const r={w:Math.round(s.getBoundingClientRect().width),cut:t.scrollWidth>t.clientWidth+1||s.scrollWidth>s.clientWidth+1};t.textContent=was;return r})()`);
  const ws = []; let cut = [];
  for (const v of ['1.5.7', '1.10.0', '1.12.15', '2.0', '10.3.1']) { const r = await fit(v); ws.push(r.w); if (r.cut) cut.push(v) }
  ok('5. at the bottom, every version is shown whole (1.5.7, 1.10.0, 1.12.15, 2.0, 10.3.1); the button grows with it', !cut.length && ws[2] > ws[0], {ws, cut});
  if (SHOT) await p.screenshot({path: path.join(SHOT, 'start-bottom.png'), clip: {x: 0, y: 752, width: 640, height: 48}});
  // at the left: under the logo, never cut
  await q("(()=>{S.tb='left';applySet();return 1})()"); await wait(400);
  const side = await q("(()=>{const s=document.getElementById('start'),t=s.querySelector('.stv'),i=s.querySelector('img');const a=i.getBoundingClientRect(),c=t.getBoundingClientRect();return{under:c.top>=a.bottom-1,cut:t.scrollWidth>t.clientWidth+1}})()");
  ok('6. at the left: the version under the logo, not cut', side.under && !side.cut, side);
  cut = []; for (const v of ['1.12.15', '10.3.1']) { const r = await fit(v); if (r.cut) cut.push(v) }
  ok('7. ...longer versions there too (on two lines if need be)', !cut.length, cut);
  if (SHOT) await p.screenshot({path: path.join(SHOT, 'start-left.png'), clip: {x: 0, y: 0, width: 160, height: 140}});
  await q("(()=>{S.tb='bottom';applySet();return 1})()");
  // versions as numbers
  const c = await q("[['1.10.0','1.9.9'],['2.0','2.0.0'],['1.5.10','1.5.9'],['10.3.1','2.0'],['1.5.6','1.5.7'],['1.12.15','1.12.15']].map(([a,b])=>verCmp(a,b))");
  ok('8. versions compared part by part as numbers: 1.10.0 > 1.9.9, 2.0 = 2.0.0, 1.5.10 > 1.5.9, 10.3.1 > 2.0, 1.5.6 < 1.5.7', JSON.stringify(c) == '[1,0,1,1,-1,0]', c);
  const order = await q("(()=>{const d=document.createElement('div');d.innerHTML=changesHtml();const o=[...d.querySelectorAll('h3')].map(h=>h.textContent.replace(/^Rudolf OS ([0-9.]+).*/,'$1'));return{o:o.slice(0,6),sorted:o.every((v,i)=>i==0||verCmp(o[i-1],v)>=0),n:o.length}})()");
  ok('9. What\'s new lists the versions newest first, as numbers', order.n > 2 && order.sorted, order);
  // the screen saver and About still take the logo from it
  ok('10. About and the screen saver still find the logo in it', await q("!!document.querySelector('#start img').src"));
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
