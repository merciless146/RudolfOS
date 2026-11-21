// 1.6.1 (docs/plan-1.6.1.md Part 25): progress on a minimized window's taskbar button (tbprog61.js). The fill follows a percentage,
// creeps for 'busy' (grows, stays under 90 %, completes when the work ends), is shown only while the window is minimized, goes when
// the work ends; its colour (Green, a preset, the accent colour, any colour) from Display Properties › Taskbar › Progress colour; the
// title stays readable in every taskbar and button style, dark and light, high contrast, at the bottom, top, left and right; a
// <progress> or [role=progressbar] inside a minimized window is found by itself; an Oracle window loading; the sources' hooks (a
// File Explorer copy, a rudolfd job, a text message, a download); on the native desktop the page tells the shell (op "prog").
// A stand-in rudolfd.   node tbprog61_t.js [CHROME]   (ports 8600, 8601)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www'), J = {'content-type': 'application/json'};
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 500) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm', '.woff2': 'font/woff2'};
const M = {session: null, job: 0};
const LS = "window.__rudolfLsFlush=()=>Promise.resolve();";
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]); q.on('data', () => {}); q.on('end', () => {
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};' + LS + '</script>')) }
  if (u.startsWith('/api/')) {
    if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200, J); s.end('{"ok":true,"events":[]}') }, 20000);
    if (u == '/api/desktop/session') { if (!M.session) { s.writeHead(404, J); return s.end('{"ok":false,"error":"no native session"}') } s.writeHead(200, J); return s.end(JSON.stringify(M.session)) }
    if (u == '/api/job') { s.writeHead(200, J); return s.end(JSON.stringify({ok: true, name: 'pkg', running: M.job > 0, status: M.job > 0 ? -1 : 0, log: 'working\n'})) }
    if (u == '/api/mobile/status') { s.writeHead(200, J); return s.end(JSON.stringify({ok: true, modems: [{id: 0, sim: true, locked: 'none', state: 'registered'}]})) }
    if (u == '/api/mobile/sms/send') return setTimeout(() => { s.writeHead(200, J); s.end('{"ok":true,"id":7}') }, 1500);
    s.writeHead(200, J); return s.end('{"ok":true}') }
  const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(8600, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
const URL0 = 'http://127.0.0.1:8600/';
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const ctx = await b.newContext({viewport: {width: 1280, height: 860}});
  const p = await ctx.newPage();
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto(URL0); await p.waitForFunction(() => typeof winProgress == 'function' && typeof tbbApply == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x'}];USER=ACCTS[0];S.accent='#2563eb';S.dm='dark';themeApply();applySet();
    window.TW=win('Untitled - Notepad','📝',360,240);TW.app='notepad';window.TW2=win('Report','📄',360,240);TW2.app='wordpro';window.TW3=win('Progress','📦',360,240);TW3.app='other';return 1})()`);
  // the fill's place and size, against its button's
  const fill = w => q(`(()=>{const b=${w}.tb,f=b.querySelector(':scope > .tbpf');if(!f)return null;const r=b.getBoundingClientRect(),x=f.getBoundingClientRect(),cs=getComputedStyle(f),bs=getComputedStyle(b),bw=parseFloat(bs.borderLeftWidth)||0,bt=parseFloat(bs.borderTopWidth)||0;
    return {frac:x.width/(r.width-2*bw-6),dl:x.left-r.left-bw,dt:x.top-r.top-bt,db:r.bottom-bt-x.bottom,bg:cs.backgroundColor,op:+cs.opacity,z:cs.zIndex,iso:bs.isolation,pos:bs.position,txt:b.textContent}})()`);
  const minimize = w => q(`${w}.el.querySelector('.mn').click()`);
  // 1. a percentage, while the window is shown: nothing; minimized: the fill at 40 %, 3 px inside
  await q('winProgress(TW,40)'); await wait(300);
  ok('1. a window at work but shown: no fill on its button', (await fill('TW')) == null);
  await minimize('TW'); await wait(600); let f = await fill('TW');
  ok('2. minimized: its button fills to 40 %, 3 px inside its edge, behind its picture and title', f && Math.abs(f.frac - .4) < .01 && Math.abs(f.dl - 3) < .6 && Math.abs(f.dt - 3) < .6 && Math.abs(f.db - 3) < .6 && f.z == '-1' && f.iso == 'isolate' && /Notepad/.test(f.txt), f);
  ok('3. green by default (#16a34a), half see-through', f && f.bg == 'color(srgb 0.0862745 0.639216 0.290196 / 0.5)' || f && /rgba\(22, 163, 74, 0\.5\)/.test(f.bg), f && f.bg);
  await q('winProgress(TW,75)'); await wait(700); f = await fill('TW');
  ok('4. the width tracks the percentage (75 %)', f && Math.abs(f.frac - .75) < .01, f);
  // 5. restored: hidden; minimized again: the current state
  await q('TW.focus()'); await wait(500);
  ok('5. restoring the window hides the fill', (await fill('TW')) == null);
  await q('winProgress(TW,60)'); await minimize('TW'); await wait(700); f = await fill('TW');
  ok('6. minimizing again shows where the work is now (60 %)', f && Math.abs(f.frac - .6) < .01, f);
  // 7. done: to 100 %, fades, gone
  await q('winProgress(TW,null)'); await wait(380); f = await fill('TW');
  ok('7. the work done: the fill completes (100 %)', f && f.frac > .99, f);
  await wait(900);
  ok('8. ...then fades and is gone', (await fill('TW')) == null);
  // 9. busy: the creeping fill
  await minimize('TW2'); await q("winProgress(TW2,'busy')"); await wait(1000); const b1 = await fill('TW2'); await wait(2000); const b2 = await fill('TW2');
  const exp = t => .9 * (1 - Math.exp(-t / 6));
  ok('9. busy: the fill creeps (grows over time, about 90 % × (1 − e^(−t/6 s)))', b1 && b2 && b2.frac > b1.frac + .05 && Math.abs(b2.frac - exp(3)) < .05, [b1 && b1.frac, b2 && b2.frac, exp(3)]);
  await q('TBP.m.get(TW2).b0-=120000'); await wait(300); const b3 = await fill('TW2');
  ok('10. ...and stays under 90 % however long it takes', b3 && b3.frac < .9001 && b3.frac > .88, b3 && b3.frac);
  await q('winProgress(TW2,null)'); await wait(380); const b4 = await fill('TW2'); await wait(900);
  ok('11. ...and completes when the work ends, then goes', b4 && b4.frac > .99 && (await fill('TW2')) == null, b4);
  // 12. several sources on one window: a percentage wins over busy, percentages averaged
  await minimize('TW3'); await q("winProgress(TW3,'busy','a');winProgress(TW3,20,'b');winProgress(TW3,60,'c')"); await wait(700); f = await fill('TW3');
  ok('12. several things at once: the percentages averaged (40 %), over the busy one', f && Math.abs(f.frac - .4) < .01, f);
  await q("winProgress(TW3,null,'a');winProgress(TW3,null,'b');winProgress(TW3,null,'c')"); await wait(1400);
  // 13. found by itself: a <progress> inside a minimized window
  await q(`(()=>{TW3.body.innerHTML="<progress max='200' value='60'></progress>";return 1})()`); await wait(2200); f = await fill('TW3');
  ok('13. a <progress> inside a minimized window is found (60 of 200: 30 %)', f && Math.abs(f.frac - .3) < .01, f);
  await q(`(()=>{TW3.body.querySelector('progress').removeAttribute('value');return 1})()`); await wait(1500); f = await fill('TW3');
  ok('14. ...without a value: the creeping fill', f && f.frac > 0 && f.frac < .9, f);
  await q(`(()=>{TW3.body.innerHTML="<div role='progressbar' aria-valuemin='0' aria-valuemax='50' aria-valuenow='40'></div>";return 1})()`); await wait(2200); f = await fill('TW3');
  ok('15. [role=progressbar] with aria-valuenow (40 of 50: 80 %)', f && Math.abs(f.frac - .8) < .01, f);
  await q(`(()=>{TW3.body.innerHTML="<progress max='10' value='10'></progress>";return 1})()`); await wait(2600);
  ok('16. one at 100 %: finished, no fill', (await fill('TW3')) == null);
  await q(`(()=>{TW3.body.innerHTML="<div hidden><progress max='10' value='3'></progress></div>";return 1})()`); await wait(1500);
  ok('17. a hidden one is not counted', (await fill('TW3')) == null);
  await q(`(()=>{TW3.body.innerHTML='';TW3.oLoading=()=>true;return 1})()`); await wait(1500); f = await fill('TW3');
  ok('18. an Oracle window loading a page (W.oLoading): the creeping fill', f && f.frac > 0 && f.frac < .9, f);
  await q('TW3.oLoading=()=>false'); await wait(2600);
  ok('19. ...gone when it has loaded', (await fill('TW3')) == null);
  await q(`(()=>{TW3.body.innerHTML="<progress max='100' value='45'></progress>";return 1})()`); await wait(1300);
  await q('TW3.focus()'); await wait(1300);
  ok('20. found by itself only while minimized (the window shown: none)', (await fill('TW3')) == null && !(await q("TBP.m.has(TW3)&&'auto' in TBP.m.get(TW3).src")));
  await q("TW3.body.innerHTML=''");
  // 21-24: the colour setting
  await q("settings('disp')"); await wait(1200);
  const row = await q(`(()=>{const tg=SW.body.querySelector('.tbgrp');const labs=[...tg.querySelectorAll(':scope > label')].map(l=>l.textContent);const s=tg.querySelector('#dtbpc'),c=tg.querySelector('#dtbpcc');return{labs,opts:s&&[...s.options].map(o=>o.textContent),v:s&&s.value,ch:c&&c.hidden,t:c&&c.type}})()`);
  ok('21. Display Properties › Taskbar: Progress colour, its presets, the accent colour and any colour; Green at first', row.labs.includes('Progress colour') && JSON.stringify(row.opts) == '["Green (as it comes)","Blue","Orange","Purple","Red","The accent colour","Any colour…"]' && row.v == 'green' && row.ch && row.t == 'color', row);
  await q("winProgress(TW,50)"); await minimize('TW'); await wait(500);
  await p.selectOption('#dtbpc', 'blue'); await wait(300); f = await fill('TW');
  ok('22. a preset (Blue) is applied at once', f && /0\.145098 0\.388235 0\.921569|37, 99, 235/.test(f.bg) && await q("S.tbpcol=='blue'"), f && f.bg);
  await q("S.accent='#9333ea';themeApply()"); await p.selectOption('#dtbpc', 'accent'); await wait(300); f = await fill('TW');
  ok('23. the accent colour', f && /0\.576471 0\.2 0\.917647|147, 51, 234/.test(f.bg) && await q("S.tbpcol=='accent'"), f && f.bg);
  await p.selectOption('#dtbpc', 'custom'); await q(`(()=>{const c=SW.body.querySelector('#dtbpcc');c.value='#c026d3';c.dispatchEvent(new Event('input'));return 1})()`); await wait(300); f = await fill('TW');
  ok('24. any colour (the colour picker shown, its colour applied)', f && /0\.752941 0\.14902 0\.827451|192, 38, 211/.test(f.bg) && await q("S.tbpcol=='#c026d3'&&!SW.body.querySelector('#dtbpcc').hidden"), f && f.bg);
  ok('25. saved with the other taskbar settings (applied: the page says it is not yet)', await q("DKEYS.includes('tbpcol')&&secDirty('disp')"));
  await q("[...SW.body.querySelectorAll('.sfoot button')].find(x=>x.textContent=='Revert').click()"); await wait(200); await q("[...document.querySelectorAll('button')].filter(x=>x.textContent=='Confirm'&&x.offsetParent).pop().click()"); await wait(300);
  ok('26. Revert: green again', await q("S.tbpcol=='green'&&getComputedStyle(document.documentElement).getPropertyValue('--tbpc').trim()=='#16a34a'"));
  ok('27. Search in Settings finds it', await q("ssMatch(ssAll(SW.SEC),'progress colour').some(e=>e.t=='Progress colour')"));
  await q('SW.close()');
  // 28. readable in every style: the title's colour against the fill over the button (or the bar), contrast at least 3:1
  const lum = c => { const a = c.map(v => { v /= 255; return v <= .03928 ? v / 12.92 : Math.pow((v + .055) / 1.055, 2.4) }); return .2126 * a[0] + .7152 * a[1] + .0722 * a[2] };
  const bad = [];
  for (const tb of ['bottom', 'top', 'left', 'right']) for (const st of ['standard', 'wire', 'mono', 'candy', 'retro']) for (const bst of ['standard', 'wire', 'mono', 'candy', 'retro']) for (const dm of ['dark', 'light']) for (const hc of [0, 1]) for (const col of ['green', 'red', 'blue']) {
    if ((tb != 'bottom' && (hc || dm == 'light' || col != 'green')) || (st != 'standard' && bst != 'standard' && col != 'green')) continue;
    const r = await q(`(()=>{S.tb=${JSON.stringify(tb)};S.tbstyle=${JSON.stringify(st)};S.tbbstyle=${JSON.stringify(bst)};S.dm=${JSON.stringify(dm)};S.cb=${hc ? "'contrast'" : "'none'"};S.tbpcol=${JSON.stringify(col)};applySet();themeApply();tbpDraw();
      const b=TW.tb,f=b.querySelector(':scope > .tbpf');if(!f)return{none:1};
      const rgb=s=>{const m=s.match(/rgba?\\(([^)]+)\\)/);if(m){const v=m[1].split(',').map(Number);return[v[0],v[1],v[2],v.length>3?v[3]:1]}const c=s.match(/color\\(srgb ([^)]+)\\)/);if(c){const v=c[1].replace('/','').split(/\\s+/).filter(Boolean).map(Number);return[v[0]*255,v[1]*255,v[2]*255,v.length>3?v[3]:1]}return[0,0,0,0]};
      let under=rgb(getComputedStyle(b).backgroundColor);if(under[3]<.5)under=rgb(getComputedStyle(document.getElementById('bar')).backgroundColor);if(under[3]<.5)under=[30,41,59,1];
      const fc=rgb(getComputedStyle(f).backgroundColor),a=fc[3],mix=[0,1,2].map(i=>fc[i]*a+under[i]*(1-a)),tx=rgb(getComputedStyle(b).color);
      const r=b.getBoundingClientRect(),x=f.getBoundingClientRect();return{mix,tx,inside:x.left>=r.left+2&&x.top>=r.top+2&&x.bottom<=r.bottom-2,z:getComputedStyle(f).zIndex}})()`);
    if (r.none) { bad.push([tb, st, bst, dm, hc, col, 'no fill']); continue }
    const l1 = lum(r.mix), l2 = lum(r.tx), cr = (Math.max(l1, l2) + .05) / (Math.min(l1, l2) + .05);
    if (cr < 3 || !r.inside || r.z != '-1') bad.push([tb, st, bst, dm, hc, col, +cr.toFixed(2), r.inside]) }
  ok('28. readable everywhere: the fill inside the button and behind the title, the title at 3:1 or more over it (bottom, top, left, right; five taskbar and five button styles; dark, light; high contrast; green, red, blue)', !bad.length, bad.slice(0, 8));
  await q("S.tb='bottom';S.tbstyle='standard';S.tbbstyle='standard';S.dm='dark';S.cb='none';S.tbpcol='green';applySet();themeApply();winProgress(TW,null)"); await wait(1200);
  // 29. a group of windows: its one button shows a minimized member's progress
  await q("(()=>{S.tbgroup=true;window.G1=win('Doc 1','📄',300,200);G1.app='grp';window.G2=win('Doc 2','📄',300,200);G2.app='grp';tbRefreshNow();G2.el.querySelector('.mn').click();winProgress(G2,30);return 1})()"); await wait(700);
  const g = await q("(()=>{const b=[...document.querySelectorAll('#tasks > button.grp')].find(x=>x.style.display!='none');const f=b&&b.querySelector('.tbpf');return f?f.getBoundingClientRect().width/(b.getBoundingClientRect().width-6):null})()");
  ok('29. windows grouped on one button: a minimized one\'s progress on it', g != null && Math.abs(g - .3) < .02, g);
  await q("winProgress(G2,null);G1.close();G2.close()"); await wait(1200);
  // 30. the button's text replaced (a new title): the fill comes back
  await q("winProgress(TW,50);TW.setTitle('Renamed - Notepad')"); await wait(400); f = await fill('TW');
  ok('30. a window whose title changes keeps its fill', f && Math.abs(f.frac - .5) < .01 && /Renamed/.test(f.txt), f);
  await q('winProgress(TW,null)'); await wait(1200);
  // 31-33: the sources: a File Explorer copy (xfJob) on the window it was asked in, a rudolfd job (pkgJob), a text message
  await q("TW2.focus()"); await p.mouse.click(5, 5); await q("TW2.focus()");
  await q("(()=>{window.XJ=xfJob('copy',3,{});XJ.total=1000;XJ.bytes=250;TW2.el.querySelector('.mn').click();return 1})()"); await wait(900); f = await fill('TW2');
  ok('31. a File Explorer copy: on the window it was asked in (25 %)', f && Math.abs(f.frac - .25) < .02, f);
  await q("XJ.finish(true,'Copied')"); await wait(1400);
  ok('32. ...gone when it has finished', (await fill('TW2')) == null);
  await q("TW2.focus()"); await p.mouse.click(5, 5); await q("TW2.focus()"); M.job = 1;
  await q("(()=>{window.PJ=pkgJob('pkg/install?name=x');setTimeout(()=>TW2.el.querySelector('.mn').click(),50);return 1})()"); await wait(1500); f = await fill('TW2');
  ok('33. a rudolfd job (installing a package): the creeping fill on the window that asked', f && f.frac > 0 && f.frac < .9, f);
  M.job = 0; await wait(2600);
  ok('34. ...gone when the job has finished', (await fill('TW2')) == null);
  await q("(()=>{MSG.W=TW3;TW3.el.querySelector('.mn').click();window.MS=msgSend('5551234','hi').catch(e=>String(e));return 1})()"); await wait(700); f = await fill('TW3');
  ok('35. a text message being sent: on Messages\' window', f && f.frac > 0, f);
  await q('MS'); await wait(1300);
  ok('36. ...gone when it is sent', (await fill('TW3')) == null);
  // 37. a download (dl27.js): on the Oracle window it was started in
  await q("(()=>{TW.app='browser';TW.focus();return 1})()"); await p.mouse.click(5, 5); await q('TW.focus()');
  await q("(()=>{window.DR=dlNative('https://example.com/big.iso',{name:'big.iso'});DR.tot=1000;DR.sz=0;TW.el.querySelector('.mn').click();return 1})()"); await wait(1300);
  await q("DR.sz=400"); await wait(1300); f = await fill('TW');
  ok('37. a download: on the Oracle window it started in (40 %, its size known)', f && Math.abs(f.frac - .4) < .03, f);
  await q("dlDone(DR,'failed','test')"); await wait(2400);
  ok('38. ...gone when it has finished', (await fill('TW')) == null);
  ok('39. no page errors', errs.length == 0, errs);
  await p.close();
  // ---- the native desktop: an app's window is a page of its own; it tells the shell (op "prog"), whether minimized or not (the shell knows)
  const p2 = await ctx.newPage(); const e2 = []; p2.on('pageerror', e => e2.push(e.message));
  await p2.goto(URL0); await p2.waitForFunction(() => typeof acctNew == 'function', null, {timeout: 30000});
  const acct = await p2.evaluate(`(async()=>{svj('bd95setup',1);['oobe'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    const a=await acctNew('Ann','pw-one','admin');ACCTS=[a];window.__sys=1;saveUser();USER=a;await userLogin(a,'pw-one');fsSave();fsFlush();await new Promise(r=>setTimeout(r,800));
    const hex=u=>[...new Uint8Array(u)].map(x=>x.toString(16).padStart(2,'0')).join('');return {id:a.id,pk:hex(await pbkBits('pw-one',a.ks,256)),fk:hex(FKRAW)}})()`);
  await p2.close(); M.session = {ok: true, id: acct.id, n: 'Ann', r: 'admin', pk: acct.pk, fk: acct.fk};
  const p3 = await ctx.newPage(); p3.on('pageerror', e => e2.push(e.message));
  await p3.addInitScript(() => { window.__posts = []; window.webkit = {messageHandlers: {rudolfNative: {postMessage: m => window.__posts.push(JSON.parse(JSON.stringify(m)))}}} });
  await p3.goto(URL0 + '#app=notepad&w=7'); await p3.waitForFunction(() => typeof OA == 'object' && OA.main, null, {timeout: 40000}); await wait(800);
  const posts = () => p3.evaluate(() => __posts.filter(m => m.op == 'prog').map(m => m.w + ':' + m.v));
  await p3.evaluate(() => winProgress(OA.main, 40)); await wait(400);
  await p3.evaluate(() => winProgress(OA.main, 'busy', 'x')); await wait(400);
  await p3.evaluate(() => { winProgress(OA.main, null); winProgress(OA.main, null, 'x') }); await wait(400);
  ok('40. Native: the page tells the shell its window\'s progress (40, busy under a percentage: 40, done: nothing)', JSON.stringify(await posts()) == '["7:40","7:"]', await posts());
  await p3.evaluate(() => { __posts.length = 0; winProgress(OA.main, 'busy') }); await wait(400); await p3.evaluate(() => winProgress(OA.main, null)); await wait(400);
  ok('41. ...busy, then done', JSON.stringify(await posts()) == '["7:busy","7:"]', await posts());
  await p3.evaluate(() => { __posts.length = 0; const e = document.createElement('progress'); e.max = 100; e.value = 30; OA.main.body.appendChild(e); window.__pe = e }); await wait(1500);
  await p3.evaluate(() => __pe.remove()); await wait(1500);
  ok('42. ...a <progress> in its page found by itself (the shell shows it only while minimized)', JSON.stringify(await posts()) == '["7:30","7:"]', await posts());
  ok('43. no page errors on the native desktop\'s page', e2.length == 0, e2);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
