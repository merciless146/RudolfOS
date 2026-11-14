// 1.5.9: Display Properties: the colour presets gone; the drawn backgrounds as Dynamic (moving) and Static. A stand-in rudolfd.
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const srv = http.createServer((q, s) => { const [u0] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/a/')) { const f = path.join(WWW, u); if (fs.existsSync(f)) { s.writeHead(200); return s.end(fs.readFileSync(f)) } }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      s.writeHead(200, {'content-type': 'application/json'}); return s.end('{"ok":true}') }
    s.writeHead(404); s.end() }) }).listen(8287, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => { if (/dispPanel|drawW|dynBg|themeApply/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8287/'); await p.waitForFunction(() => typeof dispPanel0 == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x'}];USER=ACCTS[0];S.accent='#2563eb';S.darkui=false;themeApply();return 1})()`);
  await q("notepad();settings('disp')"); await wait(1200);
  const g = await q(`(()=>{const c=SW.body.querySelector('.sc');const labs=[...c.querySelectorAll(':scope > .r2 > label, .r2 > label')].map(l=>ssClean(l));
    const names=id=>[...c.querySelectorAll('#'+id+' .dynb')].map(b=>b.dataset.dyn);return {labs,dyn:names('dynw'),st:names('dyns'),p:c.querySelectorAll('[data-p]').length,
    tools:['bs','b1','b2'].every(i=>!!c.querySelector('#'+i))}})()`);
  ok('1. Display Properties: no colour presets any more', !g.labs.includes('Presets') && g.p == 0, g.labs.slice(0, 6));
  ok('2. Dynamic Backgrounds: None, Spirit Falls and Glacier Lake (the ones that move)', JSON.stringify(g.dyn) == '["","oasis","lake"]', g.dyn);
  ok('3. Static Backgrounds: the other drawn pictures, Carbon Fibre fifth, Moonlit Stag sixth, Feather Sky last', JSON.stringify(g.st) == JSON.stringify(['alpine', 'waves', 'gold', 'teal', 'carbon', 'stag', 'panels', 'sky']), g.st);
  const li = g.labs.indexOf('Dynamic Backgrounds'), si = g.labs.indexOf('Static Backgrounds');
  ok('4. ...right after it', li >= 0 && si == li + 1, g.labs.slice(0, 6));
  ok('5. the simple colours and patterns are where they were (Background style, Color 1 / Color 2)', g.tools && g.labs.includes('Background style') && g.labs.includes('Color 1 / Color 2'));
  await q("SW.body.querySelector('#dyns .dynb[data-dyn=alpine]').click()"); await wait(400);
  ok('6. a static background is chosen like the others; it is marked', await q("S.dyn=='alpine'&&SW.body.querySelector('#dyns .dynb[data-dyn=alpine]').classList.contains('on')&&!SW.body.querySelector('#dynw .dynb.on')"));
  await q("SW.body.querySelector('#dynw .dynb[data-dyn=oasis]').click()"); await wait(400);
  ok('7. ...and a dynamic one: only it is marked', await q("S.dyn=='oasis'&&SW.body.querySelectorAll('.dynb.on').length==1"));
  await q("(()=>{const b=SW.body.querySelector('.sc #b1');b.value='#336699';b.dispatchEvent(new Event('input',{bubbles:true}));return 1})()"); await wait(400);
  ok('8. other colours: no drawn background, None marked', await q("S.dyn==''&&SW.body.querySelector('#dynw .dynb[data-dyn=\"\"]').classList.contains('on')"));
  for (const id of ['carbon', 'stag', 'sky']) for (const m of ['l', 'd', 'tl', 'td']) {
    const sz = await q(`new Promise(r=>{const i=new Image();i.onload=()=>r([i.naturalWidth,i.naturalHeight]);i.onerror=()=>r(null);i.src=DYN['${id}']['${m}']})`);
    if (!sz || sz[0] < (m.length == 1 ? 1600 : 176)) { ok(`10. ${id} ${m}: its picture loads`, false, sz); break } }
  ok('10. Carbon Fibre, Moonlit Stag and Feather Sky: both pictures (light and dark) and their thumbnails load', true);
  await q("SW.body.querySelector('#dyns .dynb[data-dyn=carbon]').click()"); await wait(400);
  ok('11. Carbon Fibre as the background', await q("S.dyn=='carbon'&&/url\\(/.test(document.body.style.background||getComputedStyle(document.body).backgroundImage)"));
  ok('9. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
