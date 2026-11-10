// 1.5.6: the accent colour on the taskbar and on the desktop icons (Display Properties: a tick box each, and Normal,
// Negative or Complementary), and 16 accent colours instead of 8.   node accent56_t.js [CHROME] [SHOT-DIR]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8157, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
const SHOT = process.argv[3];
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd|debian/.test(e.stack || e.message || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8157/'); await p.waitForFunction(() => typeof acApply == 'function' && typeof fs != 'undefined' && fs['C:'] && IDB && IDB.db, null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    ACCTS.length=0;ACCTS.push({n:'ann',h:await pwMake('a1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'a1');S.icons='rudolf';S.accent='#2563eb';applySet();return 1})()`);
  await wait(400);
  // the functions
  ok('1. Negative turns each of red, green and blue over', await q("acVariant('#2563eb','negative')") == '#da9c14', await q("acVariant('#2563eb','negative')"));
  const comp = await q("acVariant('#2563eb','comp')");
  ok('2. Complementary is the opposite hue (the same saturation and lightness)', await q(`(()=>{const a=icRgb2hsl(...icHex2rgb('#2563eb')),b=icRgb2hsl(...icHex2rgb('${comp}'));return Math.abs(((b[0]-a[0]+360)%360)-180)<2&&Math.abs(a[1]-b[1])<2&&Math.abs(a[2]-b[2])<2})()`), comp);
  ok('3. Normal is the accent itself', await q("acVariant('#2563eb','normal')") == '#2563eb');
  ok('4. the writing on the bar: white on a dark colour, black on a light one', await q("acInk('#2563eb')") == '#ffffff' && await q("acInk('#ca8a04')") == '#000000');
  // Display Properties
  await q("(()=>{settings('disp');return 1})()"); await wait(800);
  const sw = await q("[...document.querySelectorAll('.swb')].map(b=>b.dataset.c)");
  ok('5. 16 accent colours, the 8 of before among them, none twice', sw.length == 16 && new Set(sw).size == 16 && ['#2563eb', '#16a34a', '#dc2626', '#9333ea', '#ea580c', '#0891b2', '#db2777', '#475569'].every(c => sw.includes(c)), sw);
  ok('6. two tick boxes, each with Normal / Negative / Complementary, off by default (their choice greyed out)', await q("(()=>{const t=document.querySelector('#dtbac'),i=document.querySelector('#dicac'),m=[...document.querySelector('#dtbacm').options].map(o=>o.textContent).join();return !t.checked&&!i.checked&&document.querySelector('#dtbacm').disabled&&document.querySelector('#dicacm').disabled&&m=='Normal,Negative,Complementary'})()"));
  const barBg = () => q("getComputedStyle(document.getElementById('bar')).backgroundColor");
  const hex = rgb => '#' + rgb.match(/\d+/g).slice(0, 3).map(v => (+v).toString(16).padStart(2, '0')).join('');
  const before = hex(await barBg());
  ok('7. off: the taskbar keeps its usual colour', before == '#111827', before);
  await q("(()=>{const c=document.querySelector('#dtbac');c.checked=true;c.onchange({target:c});return 1})()"); await wait(200);
  ok('8. ticked: the taskbar is the accent colour, with white writing', hex(await barBg()) == '#2563eb' && await q("getComputedStyle(document.getElementById('bar')).color") == 'rgb(255, 255, 255)' && !(await q("document.querySelector('#dtbacm').disabled")), hex(await barBg()));
  await q("(()=>{const s=document.querySelector('#dtbacm');s.value='negative';s.onchange({target:s});return 1})()"); await wait(200);
  ok('9. Negative: the taskbar is the negative, with black writing (it is light)', hex(await barBg()) == '#da9c14' && await q("getComputedStyle(document.getElementById('bar')).color") == 'rgb(0, 0, 0)', hex(await barBg()));
  await q("(()=>{const s=document.querySelector('#dtbacm');s.value='comp';s.onchange({target:s});return 1})()"); await wait(200);
  ok('10. Complementary: the taskbar is the opposite colour', hex(await barBg()) == comp, [hex(await barBg()), comp]);
  // a new accent from the swatches follows
  await q("(()=>{[...document.querySelectorAll('.swb')].find(b=>b.dataset.c=='#16a34a').click();return 1})()"); await wait(200);
  ok('11. another accent (green): the taskbar follows (complementary of green)', hex(await barBg()) == await q("acVariant('#16a34a','comp')"), hex(await barBg()));
  // desktop icons
  const deskSrc = () => q("[...document.querySelectorAll('#desk > .ic > b img.ico')].map(i=>i.getAttribute('src'))");
  const s0 = await deskSrc();
  ok('12. the desktop has Rudolf icons to colour', s0.length >= 3, s0.length);
  await q("(()=>{const c=document.querySelector('#dicac');c.checked=true;c.onchange({target:c});return 1})()"); await wait(300);
  const s1 = await deskSrc(), g = await q("acVariant('#16a34a','normal')");
  const shades = await q(`(()=>{const o=icHex2rgb('${g}'),[h,s,l]=icRgb2hsl(...o);return[icHsl2hex(h,s,Math.min(l+10,86)),icHsl2hex(h,s,Math.max(l-12,10))]})()`);
  ok('13. ticked: every desktop icon is drawn in shades of the accent', s1.length == s0.length && s1.every(u => decodeURIComponent(u).includes(shades[0]) && decodeURIComponent(u).includes(shades[1])), [shades, decodeURIComponent(s1[0]).slice(0, 300)]);
  ok('14. ...the Start menu\'s icons keep their own colours', await q(`[...document.querySelectorAll('#menu img.ico')].every(i=>!decodeURIComponent(i.getAttribute('src')).includes('${shades[0]}'))`));
  await q("(()=>{const s=document.querySelector('#dicacm');s.value='negative';s.onchange({target:s});return 1})()"); await wait(300);
  const neg = await q("acVariant('#16a34a','negative')");
  const s2 = await deskSrc(), nsh = await q(`(()=>{const o=icHex2rgb('${neg}'),[h,s,l]=icRgb2hsl(...o);return icHsl2hex(h,s,Math.min(l+10,86))})()`);
  ok('15. Negative for the icons: their own choice, apart from the taskbar\'s', s2.every(u => decodeURIComponent(u).includes(nsh)) && hex(await barBg()) == await q("acVariant('#16a34a','comp')"), nsh);
  // a new icon on the desktop takes the colour too
  await q("(()=>{const d=document.createElement('div');d.className='ic';d.innerHTML='<b>📁</b><span>New</span>';document.getElementById('desk').appendChild(d);iconsApply();return 1})()"); await wait(400);
  const s3 = await deskSrc();
  ok('16. an icon put on the desktop later takes it too', s3.length == s0.length + 1 && s3.every(u => decodeURIComponent(u).includes(nsh)), s3.length);
  if (SHOT) { await q("(()=>{wins.slice().forEach(w=>w.el.style.display='none');return 1})()"); await p.screenshot({path: path.join(SHOT, 'accent-desk.png')}); await q("(()=>{wins.slice().forEach(w=>w.el.style.display='');document.querySelector('#dac').scrollIntoView({block:'center'});return 1})()"); await p.screenshot({path: path.join(SHOT, 'accent-settings.png')}) }
  // off again: as before
  await q("(()=>{['#dtbac','#dicac'].forEach(i=>{const c=document.querySelector(i);c.checked=false;c.onchange({target:c})});return 1})()"); await wait(300);
  const s4 = await deskSrc();
  ok('17. both off: the taskbar and the icons as before', hex(await barBg()) == '#111827' && s4.every(u => !decodeURIComponent(u).includes(nsh)), hex(await barBg()));
  // kept with the other display settings
  ok('18. the four settings are kept with the display settings', await q("['tbac','tbacm','icac','icacm'].every(k=>DKEYS.includes(k))"));
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
