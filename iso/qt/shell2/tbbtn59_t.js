// 1.5.9: Display Properties › Taskbar › Buttons: the taskbar's buttons styled apart from the bar (Standard as now,
// Wireframe, Monochrome, Candy, Retro), "Match the theme colours" with Normal / Negative / Complementary, each
// style's own colours when unticked. A stand-in rudolfd.   node tbbtn59_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www'), J = {'content-type': 'application/json'};
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 400) : '')) };
const srv = http.createServer((q, s) => { const u = q.url.split('?')[0]; q.on('data', () => {}); q.on('end', () => {
  if (u == '/') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
  if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200, J); s.end('{"ok":true,"events":[]}') }, 20000); s.writeHead(200, J); return s.end('{"ok":true}') }
  s.writeHead(404); s.end() }) }).listen(8294, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => { if (/tbb|dispPanelS|themeApply/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8294/'); await p.waitForFunction(() => typeof tbbApply == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x'}];USER=ACCTS[0];S.accent='#2563eb';S.tbstyle='standard';S.tbac=false;return 1})()`);
  await q("settings('disp')"); await wait(1200);
  const g = await q(`(()=>{const tg=SW.body.querySelector('.tbgrp');const labs=[...tg.querySelectorAll(':scope > label')].map(l=>l.textContent);const s=tg.querySelector('#dtbbs'),c=tg.querySelector('#dtbbac'),m=tg.querySelector('#dtbbacm');
    return {labs,st:s&&[...s.options].map(o=>o.textContent),v:s&&s.value,ck:c&&c.checked,md:m&&[...m.options].map(o=>o.textContent),mdis:m&&m.disabled}})()`);
  ok('1. Taskbar: a Buttons style and Button colours, after the taskbar\'s own Style', JSON.stringify(g.labs.slice(-3)) == '["Style","Buttons","Button colours"]', g.labs);
  ok('2. ...the five styles (Standard is as now), Standard and unticked at first', JSON.stringify(g.st) == '["Standard (as the taskbar)","Wireframe","Monochrome","Candy","Retro"]' && g.v == 'standard' && !g.ck, g);
  ok('3. ...Normal / Negative / Complementary, off while unticked', JSON.stringify(g.md) == '["Normal","Negative","Complementary"]' && g.mdis, g);
  const btn = () => q("(()=>{const b=document.querySelector('#bar #tasks button:not(.on)')||document.querySelector('#bar #pins .pin')||document.querySelector('#bar #vol');const cs=getComputedStyle(b);return{bg:cs.backgroundColor,img:cs.backgroundImage,br:cs.borderRadius,bd:cs.borderTopWidth+' '+cs.borderTopStyle,sh:cs.boxShadow}})()");
  const bar = () => q("getComputedStyle(document.getElementById('bar')).backgroundColor");
  const b0 = await btn(), bar0 = await bar();
  ok('4. Standard, unticked: the buttons as they always were (nothing changed)', !(await q("document.documentElement.classList.contains('tbbon')")), b0);
  await p.selectOption('#dtbbs', 'retro'); await wait(200);
  const b1 = await btn();
  ok('5. Retro, its own colours: silver, square, raised; the bar itself unchanged', b1.bg == 'rgb(192, 192, 192)' && b1.br == '0px' && /inset/.test(b1.sh) && await bar() == bar0, [b1, bar0]);
  await q("(()=>{const c=SW.body.querySelector('#dtbbac');c.checked=true;c.onchange({target:c});return 1})()"); await p.selectOption('#dtbbacm', 'negative'); await wait(200);
  const neg = await q("acVariant('#2563eb','negative')"), b2 = await btn();
  const rgb = h => 'rgb(' + [1, 3, 5].map(i => parseInt(h.slice(i, i + 2), 16)).join(', ') + ')';
  ok('6. Match the theme colours, Negative: the buttons take the accent\'s negative', b2.bg == rgb(neg) && !(await q("SW.body.querySelector('#dtbbacm').disabled")), [b2, neg]);
  await p.selectOption('#dtbbacm', 'comp'); await wait(200);
  ok('7. ...Complementary', (await btn()).bg == rgb(await q("acVariant('#2563eb','comp')")));
  await p.selectOption('#dtbbs', 'wire'); await wait(200); const b3 = await btn();
  ok('8. Wireframe: see-through, outlined', b3.bg == 'rgba(0, 0, 0, 0)' && /1px solid/.test(b3.bd), b3);
  await p.selectOption('#dtbbs', 'candy'); await wait(200); const b4 = await btn();
  ok('9. Candy: glossy pills', /gradient/.test(b4.img) && parseFloat(b4.br) > 100, b4);
  await p.selectOption('#dtbbs', 'mono'); await wait(200); const b5 = await btn(), m5 = b5.bg.match(/\d+/g);
  ok('10. Monochrome: grey', m5[0] == m5[1] && m5[1] == m5[2], b5);
  ok('11. the open window\'s button keeps its accent highlight', await q("getComputedStyle(document.querySelector('#bar #tasks button.on')||document.body).backgroundColor") == 'rgb(37, 99, 235)' || !(await q("!!document.querySelector('#bar #tasks button.on')")));
  ok('12. not applied yet: the page says so', await q("secDirty('disp')"));
  await q("[...SW.body.querySelectorAll('.sfoot button')].find(x=>x.textContent=='Revert').click()"); await wait(200); await q("[...document.querySelectorAll('button')].filter(x=>x.textContent=='Confirm'&&x.offsetParent).pop().click()"); await wait(300);
  ok('13. Revert: back to Standard, unticked, as before', await q("S.tbbstyle=='standard'&&!S.tbbac&&!document.documentElement.classList.contains('tbbon')") && JSON.stringify(await btn()) == JSON.stringify(b0));
  ok('14. Search in Settings finds it', await q("ssMatch(ssAll(SW.SEC),'button colours').some(e=>e.t=='Button colours')"));
  ok('15. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
