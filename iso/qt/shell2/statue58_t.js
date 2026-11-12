// 1.5.8: the stone statue on Spirit Falls' right bank: one of 16 (the Chinese zodiac, a panda, an elephant, a leopard,
// a deer), chosen by the accent colour, light and dark, laid over the picture where the bank is, only with Spirit
// Falls.   node statue58_t.js [CHROME] [SHOT-DIR]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8263, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
const SHOT = process.argv[3];
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1366, height: 768}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd|debian/.test(e.stack || e.message || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8263/'); await p.waitForFunction(() => typeof stUpdate == 'function' && typeof DYN != 'undefined', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});S.accent='#2563eb';S.dyn='lake';S.dm='light';applySet();return 1})()`);
  ok('1. another background: no statue', await q("!STS.el||STS.el.style.display=='none'"));
  ok('2. each of the 16 accent colours has its own animal, all 16 of them', await q("(()=>{const a=ACCENTS.map(x=>ST_OF[x[0]]);return a.every(Boolean)&&new Set(a).size==16&&ST_ORDER.every(n=>a.includes(n))})()"));
  ok('3. the animals: the twelve of the zodiac, a panda, an elephant, a leopard and a deer', await q("['rat','ox','tiger','rabbit','dragon','snake','horse','goat','monkey','rooster','dog','pig','panda','elephant','leopard','deer'].every(n=>ST.parts[n]&&ST.parts[n].length>4)"));
  await q("(()=>{S.dyn='oasis';applySet();return 1})()"); await wait(500);
  ok('4. Spirit Falls: the statue stands there, under everything else on the page', await q("STS.el.style.display==''&&STS.el.parentNode==document.body&&getComputedStyle(STS.el).pointerEvents=='none'&&STS.img.naturalWidth>0"));
  ok('5. the accent Blue: the Dog', await q("stAnimal()=='dog'&&STS.key.startsWith('dog|light|')"));
  ok('6. it is where the bank is in the picture (center/cover)', await q(`(()=>{const r=STS.img.getBoundingClientRect(),s=Math.max(innerWidth/1600,innerHeight/1067),ox=(innerWidth-1600*s)/2,oy=(innerHeight-1067*s)/2;return Math.abs(r.left-(ox+ST.box[0]*s))<1&&Math.abs(r.top-(oy+ST.box[1]*s))<1&&Math.abs(r.width-ST.box[2]*s)<1})()`));
  const src1 = await q('STS.img.src');
  await q("(()=>{S.accent='#dc2626';themeApply();return 1})()"); await wait(300);
  ok('7. a new accent colour (Red): the Dragon, a different picture', await q("stAnimal()=='dragon'") && await q('STS.img.src') != src1);
  await q("(()=>{S.dm='dark';applySet();return 1})()"); await wait(300);
  ok('8. dark mode: the dark statue', await q("STS.key.startsWith('dragon|dark|')&&/%231a1c36/i.test(STS.img.src)"));
  await q("(()=>{S.accent='#e11d48';themeApply();return 1})()");
  ok('9. a colour of one\'s own takes the nearest accent\'s animal', await q("stAnimal()")== await q("ST_OF['#dc2626']==stAnimal()||ST_OF['#db2777']==stAnimal()?stAnimal():'x'"));
  ok('10. every statue draws, light and dark, the same each time', await q("(()=>{const out=[];for(const n of ST_ORDER)for(const m of ['light','dark']){STS.cache={};const a=stSvg(n,m);STS.cache={};out.push(a==stSvg(n,m)&&a.length>2000)}return out.every(Boolean)})()"));
  if (SHOT) { await q("(()=>{S.accent='#475569';S.dm='light';applySet();themeApply();return 1})()"); await wait(500); await p.screenshot({path: SHOT + '/statue-panda.png'}) }
  await q("(()=>{settings('disp');return 1})()"); await wait(800);
  // the glow in its dark features: the accent colour by the chosen rule
  const glowHue = () => q("icRgb2hsl(...icHex2rgb(stGlow()))[0]");
  await q("(()=>{S.accent='#2563eb';themeApply();return 1})()");
  const gN = await glowHue(), aH = await q("icRgb2hsl(...icHex2rgb('#2563eb'))[0]");
  ok('11a. the glow: Normal is the accent colour\'s hue', Math.abs(gN - aH) < 3 && await q("decodeURIComponent(STS.img.src).includes(stGlow())"), [gN, aH]);
  await q("(()=>{const s=document.querySelector('#dstm');s.value='comp';s.onchange({target:s});return 1})()"); await wait(200);
  const gC = await glowHue();
  ok('11b. Complementary: the opposite hue, and the statue redrawn with it', Math.abs(((gC - aH + 360) % 360) - 180) < 3 && await q("decodeURIComponent(STS.img.src).includes(stGlow())"), [gC, aH]);
  await q("(()=>{const s=document.querySelector('#dstm');s.value='negative';s.onchange({target:s});return 1})()"); await wait(200);
  const nH = await q("icRgb2hsl(...icHex2rgb(acVariant('#2563eb','negative')))[0]"), gG = await glowHue();
  ok('11c. Negative: the hue of the accent\'s negative', Math.abs(gG - nH) < 3, [gG, nH]);
  // the petals blowing across
  const c1 = await q("PET.cv.toDataURL()"); await wait(400); const c2 = await q("PET.cv.toDataURL()");
  ok('11d. cherry petals blow across (70 of them, moving)', await q("PET.ps.length==70&&!!PET.timer") && c1 != c2);
  ok('11. Display Properties says which statue stands there', await q("/stands on the right bank/.test((document.querySelector('.stmsg')||{}).textContent||'')"));
  await q("(()=>{S.dyn='';applySet();return 1})()");
  ok('12. no background: no statue, no petals', await q("STS.el.style.display=='none'&&!PET.timer"));
  ok('13. no errors', errs.length == 0, errs);
  await b.close(); srv.close();
  const k = R.filter(Boolean).length; console.log(`${k}/${R.length} passed`); process.exit(k == R.length ? 0 : 1);
})().catch(e => { console.log('ERROR', e.stack); process.exit(1) });
