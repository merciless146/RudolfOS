// 1.5.8: the northern lights over Glacier Lake at night: on with the dark picture only, in the accent colour by the
// Normal / Negative / Complementary rules, under everything on the desktop, kept to the sky and the lake by the mask,
// moving, and standing still with Reduce motion.   node aurora58_t.js [CHROME] [SHOT-DIR]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8262, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
const SHOT = process.argv[3];
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd|debian/.test(e.stack || e.message || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8262/'); await p.waitForFunction(() => typeof aurUpdate == 'function' && typeof DYN != 'undefined', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});S.accent='#16a34a';S.aur=true;S.aurm='normal';S.rmotion=false;S.dyn='lake';S.dm='light';applySet();return 1})()`);
  await wait(300);
  ok('1. Glacier Lake has its sky mask', await q("/^data:image\\/webp;base64,/.test(DYN.lake.am)"));
  ok('2. light mode: no northern lights', await q("!AUR.on&&!AUR.timer"));
  await q("(()=>{S.dm='dark';applySet();return 1})()"); await wait(700);
  ok('3. dark mode: the lights are on, moving (a timer), the overlay the first thing on the page, under the icons', await q("AUR.on&&!!AUR.timer&&document.body.firstChild.id=='aurora'&&getComputedStyle(document.getElementById('aurora')).pointerEvents=='none'"));
  // where the picture is drawn (center/cover): the canvas covers the same box
  ok('4. the canvas covers the picture as the background does (center/cover)', await q(`(()=>{const r=AUR.cv.getBoundingClientRect(),s=Math.max(innerWidth/1600,innerHeight/1067);return Math.abs(r.width-1600*s)<1&&Math.abs(r.left-(innerWidth-1600*s)/2)<1&&Math.abs(r.top-(innerHeight-1067*s)/2)<1})()`));
  const px = (x, y) => q(`Array.from(AUR.cv.getContext('2d').getImageData(${x},${y},1,1).data)`);
  // the sky just above the peaks, a mountainside, and the open water where the sky is reflected
  const sum = async (x0, y0, x1, y1) => q(`(()=>{const d=AUR.cv.getContext('2d').getImageData(${x0},${y0},${x1 - x0},${y1 - y0}).data;let a=0;for(let i=3;i<d.length;i+=4)a+=d[i];return a/(d.length/4)})()`);
  const sky = await sum(150, 20, 330, 50), rock = await sum(160, 75, 230, 110), lake = await sum(150, 230, 300, 266);
  ok('5. the lights show in the sky', sky > 8, sky);
  ok('6. ...not on the mountains in front of them', rock < 2, rock);
  ok('7. ...and the lake reflects them', lake > 4, lake);
  const hue = async () => q(`(()=>{const d=AUR.cv.getContext('2d').getImageData(0,0,400,130).data;let r=0,g=0,b=0;for(let i=0;i<d.length;i+=4){const a=d[i+3];r+=d[i]*a;g+=d[i+1]*a;b+=d[i+2]*a}return icRgb2hsl(r,g,b)[0]})()`);
  const hd = (a, b2) => { const d = Math.abs(a - b2) % 360; return Math.min(d, 360 - d) };
  const accH = await q("icRgb2hsl(...icHex2rgb('#16a34a'))[0]");
  const h1 = await hue();
  ok('8. Normal: the lights are the accent colour (green)', hd(h1, accH) < 30, [h1, accH]);
  if (SHOT) await p.screenshot({path: SHOT + '/aurora-normal.png'});
  await q("(()=>{settings('disp');return 1})()"); await wait(800);
  ok('9. Display Properties: a tick box and Normal / Negative / Complementary', await q("(()=>{const c=document.querySelector('#daur'),m=document.querySelector('#daurm');return !!c&&c.checked&&[...m.options].map(o=>o.textContent).join()=='Normal,Negative,Complementary'})()"));
  await q("(()=>{const s=document.querySelector('#daurm');s.value='negative';s.onchange({target:s});return 1})()"); await wait(400);
  const negH = await q("icRgb2hsl(...icHex2rgb(acVariant('#16a34a','negative')))[0]"), h2 = await hue();
  ok('10. Negative: the lights are the accent\'s negative (pink)', hd(h2, negH) < 30, [h2, negH]);
  await q("(()=>{const s=document.querySelector('#daurm');s.value='comp';s.onchange({target:s});return 1})()"); await wait(400);
  const compH = await q("icRgb2hsl(...icHex2rgb(acVariant('#16a34a','comp')))[0]"), h3 = await hue();
  ok('11. Complementary: the lights are the opposite hue (magenta)', hd(h3, compH) < 30 && hd(h3, accH) > 120, [h3, compH]);
  if (SHOT) await p.screenshot({path: SHOT + '/aurora-comp.png'});
  await q("(()=>{S.accent='#2563eb';themeApply();return 1})()"); await wait(400);
  ok('12. a new accent colour: the lights follow it', hd(await hue(), await q("icRgb2hsl(...icHex2rgb(acVariant('#2563eb','comp')))[0]")) < 30);
  const f1 = await q("AUR.cv.toDataURL()"); await wait(700); const f2 = await q("AUR.cv.toDataURL()");
  ok('13. the lights move', f1 != f2);
  await q("(()=>{S.rmotion=true;applySet();return 1})()"); await wait(300);
  const g1 = await q("AUR.cv.toDataURL()"); await wait(500); const g2 = await q("AUR.cv.toDataURL()");
  ok('14. Reduce motion: they stand still (no timer)', !(await q("AUR.timer")) && g1 == g2 && await q("AUR.on"));
  await q("(()=>{S.rmotion=false;const c=document.querySelector('#daur');c.checked=false;c.onchange({target:c});return 1})()"); await wait(300);
  ok('15. unticked: off', await q("!AUR.on&&!AUR.timer&&document.getElementById('aurora').style.display=='none'&&document.querySelector('#daurm').disabled"));
  await q("(()=>{S.aur=true;S.dyn='';applySet();return 1})()"); await wait(200);
  ok('16. another background: off', await q("!AUR.on"));
  ok('17. no errors', errs.length == 0, errs);
  await b.close(); srv.close();
  const k = R.filter(Boolean).length; console.log(`${k}/${R.length} passed`); process.exit(k == R.length ? 0 : 1);
})().catch(e => { console.log('ERROR', e.stack); process.exit(1) });
