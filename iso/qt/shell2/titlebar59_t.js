// 1.5.9: Display Properties › Title bars: the taskbar's styles for the windows' title bars, and the accent colour as
// Normal / Negative / Complementary. A stand-in rudolfd.   node titlebar59_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const srv = http.createServer((q, s) => { const [u0] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      s.writeHead(200, {'content-type': 'application/json'}); return s.end('{"ok":true}') }
    s.writeHead(404); s.end() }) }).listen(8286, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => { if (/\btw[A-Z]|titlebar|dispPanelS|themeApply/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8286/'); await p.waitForFunction(() => typeof twApply == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x'}];USER=ACCTS[0];S.accent='#2563eb';S.darkui=false;themeApply();return 1})()`);
  await q("notepad();settings('disp')"); await wait(1200);
  const g = await q(`(()=>{const hs=[...SW.body.querySelectorAll('.sc h4')].map(h=>h.textContent.trim());const s=SW.body.querySelector('#dtws'),m=SW.body.querySelector('#dtwm');
    return {hs,st:s&&[...s.options].map(o=>o.textContent),md:m&&[...m.options].map(o=>o.textContent),tb:[...SW.body.querySelectorAll('.tbgrp select')].map(x=>x.id),v:s&&[s.value,m.value]}})()`);
  const ti = g.hs.indexOf('Title bars'), ki = g.hs.indexOf('Taskbar');
  ok('1. Display Properties: a Title bars group right after the Taskbar group', ti > 0 && ti == ki + 1, g.hs);
  ok("2. ...its styles are the taskbar's", JSON.stringify(g.st) == JSON.stringify(await q("[...SW.body.querySelector('#dtbs').options].map(o=>o.textContent)")) && g.st.length == 5, g.st);
  ok('3. ...and a Normal / Negative / Complementary drop-down, as the taskbar and icons have; Standard and Normal at first', JSON.stringify(g.md) == '["Normal","Negative","Complementary"]' && JSON.stringify(g.v) == '["standard","normal"]', g);
  const bg = sel => q(`getComputedStyle(document.querySelector('${sel}')).backgroundColor`);
  const hex2rgb = h => 'rgb(' + [1, 3, 5].map(i => parseInt(h.slice(i, i + 2), 16)).join(', ') + ')';
  const a0 = await bg('.win.act>.tb');
  ok('4. Standard + Normal: the title bar as always (the accent colour)', a0 == hex2rgb('#2563eb'), a0);
  await p.selectOption('#dtwm', 'negative'); await wait(200);
  const neg = await q("acVariant('#2563eb','negative')"), a1 = await bg('.win.act>.tb'), i1 = await q("getComputedStyle([...document.querySelectorAll('#desk .win, body > .win')].find(w=>!w.classList.contains('act'))||document.querySelector('.twprev .win:not(.act)')).getPropertyValue('--x')||1");
  ok('5. Negative: the active title bar takes the negative of the accent colour', a1 == hex2rgb(neg), [a1, neg]);
  const ink = await q("getComputedStyle(document.querySelector('.win.act>.tb')).color"), want = await q("acInk(acVariant('#2563eb','negative'))");
  ok('6. ...with writing that reads on it (white or black)', ink == hex2rgb(want), [ink, want]);
  await p.selectOption('#dtwm', 'comp'); await wait(200);
  ok('7. Complementary: the colour opposite on the colour wheel', await bg('.win.act>.tb') == hex2rgb(await q("acVariant('#2563eb','comp')")));
  const inact = await q("(()=>{const w=wins.find(x=>x!==SW);return w?getComputedStyle(w.el.querySelector('.tb')).filter:''})()");
  ok('8. ...the other windows stay muted (grey)', /grayscale\(1\)/.test(inact), inact);
  const styles = {};
  for (const st of ['wire', 'mono', 'candy', 'retro']) { await p.selectOption('#dtws', st); await wait(200);
    styles[st] = await q(`(()=>{const t=document.querySelector('.win.act>.tb'),b=t.querySelector('.cl'),cs=getComputedStyle(t),bs=getComputedStyle(b);return {cls:document.documentElement.classList.contains('tws-${st}'),bg:cs.backgroundColor,img:cs.backgroundImage,br:bs.borderRadius,bb:bs.backgroundColor,win:getComputedStyle(t.parentElement).borderTopLeftRadius}})()`) }
  ok('9. Wireframe: see-through, outlined buttons', styles.wire.cls && /(rgba\(.*0\.1|\/ 0\.1)/.test(styles.wire.bg) && styles.wire.bb == 'rgba(0, 0, 0, 0)', styles.wire);
  const mono = styles.mono.bg.match(/\d+/g); ok('10. Monochrome: grey', styles.mono.cls && mono[0] == mono[1] && mono[1] == mono[2], styles.mono);
  ok('11. Candy: glossy, round buttons', styles.candy.cls && /gradient/.test(styles.candy.img) && parseFloat(styles.candy.br) > 100, styles.candy);
  ok('12. Retro: a gradient bar, square corners, silver buttons', styles.retro.cls && /gradient/.test(styles.retro.img) && styles.retro.br == '0px' && styles.retro.win == '0px' && styles.retro.bb == 'rgb(192, 192, 192)', styles.retro);
  // Apply and Revert, as the page's other settings
  ok('13. not applied yet: the page says so', await q("secDirty('disp')"));
  await q("[...SW.body.querySelectorAll('.sfoot button')].find(x=>x.textContent=='Revert').click()"); await wait(200); await q("[...document.querySelectorAll('button')].filter(x=>x.textContent=='Confirm'&&x.offsetParent).pop().click()"); await wait(300);
  ok('14. Revert: back to Standard and Normal, as always', await q("S.twstyle=='standard'&&S.twacm=='normal'&&!document.documentElement.classList.contains('twcol')") && await bg('.win.act>.tb') == hex2rgb('#2563eb'));
  await q("SW.show('disp',1)"); await wait(500); await p.selectOption('#dtws', 'retro'); await p.selectOption('#dtwm', 'negative'); await wait(200);
  await q("[...SW.body.querySelectorAll('.sfoot button')].find(x=>x.textContent=='Apply').click()"); await wait(200); await q("[...document.querySelectorAll('button')].filter(x=>x.textContent=='Confirm'&&x.offsetParent).pop().click()"); await wait(400);
  ok('15. Apply: kept (Retro, Negative), nothing left unapplied', await q("SC.twstyle=='retro'&&SC.twacm=='negative'&&!secDirty('disp')"));
  ok('16. Search in Settings finds it', await q("ssMatch(ssAll(SW.SEC),'title bar').some(e=>e.t=='Title bars'&&e.k=='disp')"));
  ok('17. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
