// 1.5.6: the taskbar's style (Display Properties › Taskbar › Style): Standard, Wireframe, Monochrome, Candy and Retro,
// each in its own colours or in the accent colour when that is ticked for the taskbar.
//   node tbstyle56_t.js [CHROME] [SHOT-DIR]   (build iso/www first; SHOT-DIR: a picture of the bar in each style)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8159, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
const SHOT = process.argv[3];
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd|debian/.test(e.stack || e.message || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8159/'); await p.waitForFunction(() => typeof acApply == 'function' && typeof TB_STYLES != 'undefined' && IDB && IDB.db, null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    ACCTS.length=0;ACCTS.push({n:'ann',h:await pwMake('a1'),r:'admin'});saveUser();USER=ACCTS[0];await userLogin(USER,'a1');S.icons='rudolf';S.accent='#2563eb';S.tbac=false;S.tbstyle='standard';applySet();
    win('Notepad','🗒️',300,200);return 1})()`);
  await wait(500);
  await q("(()=>{settings('disp');return 1})()"); await wait(800);
  const opts = await q("(()=>{const s=document.querySelector('.tbgrp #dtbs');return s?[...s.options].map(o=>o.textContent):null})()");
  ok('1. Display Properties › Taskbar has Style: Standard, Wireframe, Monochrome, Candy, Retro (Standard by default)', JSON.stringify(opts) == '["Standard","Wireframe","Monochrome","Candy","Retro"]' && await q("document.querySelector('#dtbs').value") == 'standard', opts);
  const cs = (sel, prop) => q(`getComputedStyle(document.querySelector('${sel}')).${prop}`);
  const pick = async v => { await q(`(()=>{const s=document.querySelector('#dtbs');s.value='${v}';s.onchange({target:s});return 1})()`); await wait(250) };
  ok('2. Standard, no accent: the bar as before', await cs('#bar', 'backgroundColor') == 'rgb(17, 24, 39)' && !(await q("document.documentElement.classList.contains('tbac')")));
  const shots = [];
  const shot = async name => { if (!SHOT) return; await q("(()=>{wins.forEach(w=>{if(w!==SW)w.el.style.visibility='visible'});SW&&(SW.el.style.visibility='hidden');return 1})()");
    const f = path.join(SHOT, 'tb-' + name + '.png'); await p.screenshot({path: f, clip: {x: 0, y: 752, width: 1280, height: 48}}); shots.push(f); await q("(()=>{SW&&(SW.el.style.visibility='visible');return 1})()") };
  await shot('standard');
  await pick('wire');
  ok('3. Wireframe: a see-through bar, the buttons outlined with nothing inside', /rgba|color\(|oklab|srgb/.test(await cs('#bar', 'backgroundColor')) && await cs('#vol', 'backgroundColor') == 'rgba(0, 0, 0, 0)' && (await cs('#vol', 'borderTopWidth')) == '1px', [await cs('#bar', 'backgroundColor'), await cs('#vol', 'backgroundColor')]);
  await shot('wire');
  await pick('mono');
  const mono = await cs('#bar', 'backgroundColor'), m = mono.match(/\d+/g);
  ok('4. Monochrome: a grey bar, and the pictures in grey', m[0] == m[1] && m[1] == m[2] && /grayscale\(1\)/.test(await cs('#pins img', 'filter')), [mono, await cs('#pins img', 'filter')]);
  await shot('mono');
  await pick('candy');
  ok('5. Candy: a glossy bar, round "pill" buttons', /linear-gradient/.test(await cs('#bar', 'backgroundImage')) && parseFloat(await cs('#vol', 'borderTopLeftRadius')) >= 100 && /linear-gradient/.test(await cs('#vol', 'backgroundImage')), [await cs('#bar', 'backgroundImage'), await cs('#vol', 'borderTopLeftRadius')]);
  await shot('candy');
  await pick('retro');
  ok('6. Retro: a silver bar, square buttons with raised edges, black writing', await cs('#bar', 'backgroundColor') == 'rgb(192, 192, 192)' && await cs('#vol', 'borderTopLeftRadius') == '0px' && /inset/.test(await cs('#vol', 'boxShadow')) && await cs('#bar', 'color') == 'rgb(0, 0, 0)', [await cs('#bar', 'backgroundColor'), await cs('#vol', 'boxShadow')]);
  ok('7. ...the clock sunken, the other way round', (await cs('#tray', 'boxShadow')) != (await cs('#vol', 'boxShadow')));
  await shot('retro');
  // with the accent colour ticked for the taskbar: each style in the accent
  await q("(()=>{const c=document.querySelector('#dtbac');c.checked=true;c.onchange({target:c});return 1})()"); await wait(250);
  ok('8. Retro in the accent colour (blue): the bar is the accent, white writing', await cs('#bar', 'backgroundColor') == 'rgb(37, 99, 235)' && await cs('#bar', 'color') == 'rgb(255, 255, 255)', await cs('#bar', 'backgroundColor'));
  await shot('retro-accent');
  await pick('candy'); await shot('candy-accent');
  ok('9. Candy in the accent: its gradient is made of the accent', (await cs('#bar', 'backgroundImage')).includes('37, 99, 235'), await cs('#bar', 'backgroundImage'));
  await pick('mono');
  const mono2 = (await cs('#bar', 'backgroundColor')).match(/\d+/g);
  ok('10. Monochrome in the accent: the grey of the accent\'s lightness (still grey)', mono2[0] == mono2[1] && mono2[1] == mono2[2], mono2);
  await shot('mono-accent');
  await pick('wire'); await shot('wire-accent');
  await pick('standard');
  ok('11. Standard in the accent: as before (#162)', await cs('#bar', 'backgroundColor') == 'rgb(37, 99, 235)');
  await q("(()=>{const c=document.querySelector('#dtbac');c.checked=false;c.onchange({target:c});return 1})()"); await wait(250);
  ok('12. Standard, accent off: the usual bar again', await cs('#bar', 'backgroundColor') == 'rgb(17, 24, 39)' && !(await q("['wire','mono','candy','retro'].some(k=>document.documentElement.classList.contains('tbs-'+k))")));
  ok('13. the style is kept with the display settings', await q("DKEYS.includes('tbstyle')"));
  ok('no page error', !errs.length, errs);
  if (SHOT) console.log('shots: ' + shots.join(' '));
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
