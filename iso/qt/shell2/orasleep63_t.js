// 1.6.3 (docs/plan-1.6.3.md Part 4): Oracle frees the tabs you have not looked at for a while (desktop/orasleep63.js). With a
// stand-in shell: a background tab not looked at for the chosen time is asked whether it may sleep ("busy": sound, typed text;
// its answer says how far it is scrolled), a free one closes its page and keeps its address (💤); choosing it loads it again and
// scrolls it back; the shown tab, busy tabs and a game's tab stay; Never frees nothing; the memory guard's warning frees them at
// once; Settings › Oracle › Memory. And the shell's own side (iso/shell/shell.js): its busy answer's scroll and its scrollto.
//   node orasleep63_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 400) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8725, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto('http://127.0.0.1:8725/'); await p.waitForFunction(() => typeof ofreeTick == 'function' && typeof rtBusy == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  // the stand-in shell: tabs whose address has "music" play sound, "form" hold typed text; every page is scrolled to 640
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    window.__sent=[];window.__url={};SHELL=window;SHELL_CAPS=['tabs'];
    window.postMessage=m=>{__sent.push(m);if(m.op=='open')__url[m.tid]=m.url;
      if(m.op=='busy')setTimeout(()=>dispatchEvent(new MessageEvent('message',{source:window,data:{rudolfShell:'tabev',tid:m.tid,ev:'busy',id:m.id,m:/music/.test(__url[m.tid]),f:/form/.test(__url[m.tid]),y:640}})),20)};
    return 1})()`);
  await q(`(()=>{browser('https://example.com/a');return 1})()`); await wait(300);
  await q(`(()=>{const k=e=>document.querySelector('.win.act .otabs button');['https://example.com/music','https://example.com/form','https://example.com/b','https://example.com/c'].forEach(u=>{k().click();const a=document.querySelector('.win.act input.ad, .win.act .ad');a.value=u;a.dispatchEvent(new KeyboardEvent('keydown',{key:'Enter'}))});return 1})()`); await wait(300);
  ok('1. five real tabs open in the shell', (await q("Object.values(__url)")).length == 5);
  ok('2. the default: tabs not looked at for 30 minutes', await q("ofreeMin()==30"));
  ok('3. the first look starts each tab\'s clock: nothing is freed', (await q("ofreeTick(false)")) == 0 && (await q("__sent.filter(m=>m.op=='close').length")) == 0);
  // 10 minutes on: still nothing
  await q("(()=>{for(const k of OFREE.seen.keys())OFREE.seen.set(k,Date.now()-10*60000);return 1})()");
  ok('4. after 10 minutes nothing is freed', (await q("ofreeTick(false)")) == 0);
  // 31 minutes on
  await q("(()=>{for(const k of OFREE.seen.keys())OFREE.seen.set(k,Date.now()-31*60000);return 1})()");
  const n = await q("ofreeTick(false)");
  const closed = await q("__sent.filter(m=>m.op=='close').map(m=>__url[m.tid])");
  ok('5. after 30 minutes the free background tabs are freed (a and b): their pages closed', n == 2 && closed.length == 2 && closed.every(u => /\/(a|b)$/.test(u)), {n, closed});
  ok('6. the tab playing music, the one with typed text and the shown tab (c) stay', !closed.some(u => /music|form|\/c$/.test(u)));
  const labels = await q("[...document.querySelectorAll('.win.act .otab span')].map(x=>x.textContent)");
  ok('7. a freed tab shows 💤', labels.filter(x => x.startsWith('💤')).length == 2, labels);
  await q("(()=>{__sent.length=0;[...document.querySelectorAll('.win.act .otab')][0].click();return 1})()"); await wait(200);
  const re = await q("__sent.filter(m=>m.op=='open'||m.op=='scrollto').map(m=>[m.op,m.url||m.y])");
  ok('8. choosing a freed tab loads it again at its address, and scrolls it back where it was (640)', re.length == 2 && re[0][0] == 'open' && /example\.com\/a$/.test(re[0][1]) && re[1][0] == 'scrollto' && re[1][1] == 640, re);
  ok('9. ...and it is awake (no 💤)', !(await q("document.querySelectorAll('.win.act .otab')[0].textContent")).startsWith('💤'));
  // the shown tab is never freed, however long
  await q("(()=>{__sent.length=0;for(const k of OFREE.seen.keys())OFREE.seen.set(k,Date.now()-300*60000);return 1})()");
  const shownTid = await q("[...RT.entries()].find(([k,w])=>w.shown())[0]");
  await q("ofreeTick(false)");
  ok('10. the tab shown in its window is never freed', !(await q("__sent.filter(m=>m.op=='close').map(m=>m.tid)")).includes(shownTid));
  // Never
  await q("(()=>{[...document.querySelectorAll('.win.act .otab')][1].click();__sent.length=0;S.oraFree=0;for(const k of OFREE.seen.keys())OFREE.seen.set(k,Date.now()-300*60000);return 1})()"); await wait(100);
  ok('11. Never: nothing is freed', (await q("ofreeTick(false)")) == 0 && (await q("ofreeTick(true)")) == 0);
  // the memory guard's warning: freed at once (not in game mode), said in a notification
  await q("(()=>{S.oraFree=120;__sent.length=0;[...document.querySelectorAll('.win.act .otab')][3].click();[...document.querySelectorAll('.win.act .otab')][4].click();document.querySelectorAll('.ntoast').forEach(x=>x.remove());return 1})()"); await wait(100);
  await q("(()=>{for(const k of OFREE.seen.keys())OFREE.seen.set(k,Date.now()-60000);return 1})()");
  await q("(()=>{evDispatch('memory warn 4242 1940\\n');return 1})()"); await wait(800);
  const mw = await q("__sent.filter(m=>m.op=='close').map(m=>__url[m.tid])");
  ok('12. memory low: the background tabs that may sleep are freed at once (a, b), not those playing or holding text', mw.length == 2 && mw.every(u => /\/(a|b)$/.test(u)), mw);
  ok('13. ...and a notification says so', await q("[...document.querySelectorAll('.ntoast, .notif, .nc-item')].some(x=>/Oracle freed memory|tabs you were not looking at/.test(x.textContent))||(typeof NOTIFS!='undefined'&&JSON.stringify(NOTIFS).includes('Oracle freed memory'))"));
  // a game's full-screen tab is game mode's
  ok('14. a game\'s full-screen tab is left to game mode', /GAME_FS\.has\(tid\)/.test(await q("ofreeTick.toString()")));
  // Settings › Oracle › Memory
  await q("(()=>{settings('web');return 1})()"); await wait(800);
  const sel = await q("(()=>{const s=document.querySelector('#ofreesel');return s?[...s.options].map(o=>o.textContent+'='+o.value+(o.selected?'*':'')):null})()");
  ok('15. Settings › Oracle › Memory: 15 minutes, 30 minutes, 1 hour, 2 hours, Never (the choice shown)', sel && sel.join() == '15 minutes=15,30 minutes=30,1 hour=60,2 hours=120*,Never=0', sel);
  await q("(()=>{const s=document.querySelector('#ofreesel');s.value='60';s.dispatchEvent(new Event('change'));return 1})()");
  ok('16. choosing 1 hour keeps it at once (the page\'s Apply is web search\'s)', await q("S.oraFree==60&&JSON.parse(localStorage.getItem('bd95set')).oraFree==60"), await q("[S.oraFree,localStorage.getItem('bd95set')]"));
  // the shell's own side: the busy answer says how far the page is scrolled; scrollto scrolls once the page has loaded
  const SH = fs.readFileSync(path.join(__dirname, '../../shell/shell.js'), 'utf8');
  const BUSY = eval(SH.match(/const BUSY_JS = (`[\s\S]*?`);\n/)[1]);
  const p2 = await b.newPage(); await p2.setContent('<div style="height:5000px">long</div>'); await p2.evaluate('scrollTo(0,1234)');
  const by = JSON.parse(await p2.evaluate(BUSY));
  ok('17. the shell\'s busy answer says how far the page is scrolled (1234), and nothing else changes', by.y == 1234 && by.m === false && by.f === false, by);
  await p2.close();
  ok('18. the shell keeps scrollto for the next load only, a number, and scrolls only a page still at the top', /scrollto: \(m, t\) => \{ t\.sy = Math\.max\(0, Math\.min\(1e7, Math\.round\(num\(m\.y, 0\)\)\)\) \}/.test(SH) && /if \(t\.sy\) \{ const y = t\.sy; t\.sy = 0; f\.executeScript\(\{code: 'if\(!scrollY\)scrollTo\(0,' \+ y \+ '\)'\}\)/.test(SH));
  ok('19. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  const f = R.filter(x => !x).length; console.log(f ? `FAIL ${f} of ${R.length}` : `PASS ${R.length}/${R.length}`); process.exit(f ? 1 : 0);
})().catch(e => { console.log('FAIL ' + e.stack); process.exit(1) });
