// 1.5.9 (plan 9.2): game mode puts Oracle's background tabs to sleep: the shell is asked whether each may sleep
// ("busy": sound playing, text typed), a free one closes its page and keeps its address (💤), selecting it loads it
// again; the current tab, the game's tab and busy tabs stay.   node gamesleep59_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 400) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8268, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto('http://127.0.0.1:8268/'); await p.waitForFunction(() => typeof gslSleep == 'function' && typeof rtBusy == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  // a stand-in shell: it records what the desktop sends and answers "busy" (tabs whose address has "music" play sound,
  // "form" holds typed text)
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    window.__sent=[];window.__url={};SHELL=window;SHELL_CAPS=['tabs'];
    window.postMessage=m=>{__sent.push(m);if(m.op=='open')__url[m.tid]=m.url;
      if(m.op=='busy')setTimeout(()=>dispatchEvent(new MessageEvent('message',{source:window,data:{rudolfShell:'tabev',tid:m.tid,ev:'busy',id:m.id,m:/music/.test(__url[m.tid]),f:/form/.test(__url[m.tid])}})),20)};
    return 1})()`);
  await q(`(()=>{browser('https://example.com/a');return 1})()`); await wait(300);
  await q(`(()=>{const k=e=>document.querySelector('.win.act .otabs button');['https://example.com/music','https://example.com/form','https://example.com/b','https://example.com/game'].forEach(u=>{k().click();const a=document.querySelector('.win.act input.ad, .win.act .ad');a.value=u;a.dispatchEvent(new KeyboardEvent('keydown',{key:'Enter'}))});return 1})()`); await wait(300);
  const opened = await q("Object.values(__url)");
  ok('1. five real tabs open in the shell', opened.length == 5, opened);
  const gameTid = await q("Object.keys(__url).find(k=>/game/.test(__url[k]))");
  // the game's tab is current (shown); it goes full screen
  await q(`(()=>{dispatchEvent(new MessageEvent('message',{source:window,data:{rudolfShell:'tabev',tid:${JSON.stringify(gameTid)},ev:'fs',on:true}}));return 1})()`);
  const n = await q("gslSleep()");
  const closed = await q("__sent.filter(m=>m.op=='close').map(m=>__url[m.tid])");
  ok('2. game mode: the free background tabs sleep (a and b); their pages are closed', n == 2 && closed.length == 2 && closed.every(u => /\/(a|b)$/.test(u)), {n, closed});
  ok('3. the tab playing music, the one with typed text and the game stay awake', !closed.some(u => /music|form|game/.test(u)));
  const labels = await q("[...document.querySelectorAll('.win.act .otab span')].map(x=>x.textContent)");
  ok('4. a sleeping tab shows 💤', labels.filter(x => x.startsWith('💤')).length == 2, labels);
  await q(`(()=>{dispatchEvent(new MessageEvent('message',{source:window,data:{rudolfShell:'tabev',tid:${JSON.stringify(gameTid)},ev:'fs',on:false}}));__sent.length=0;[...document.querySelectorAll('.win.act .otab')][0].click();return 1})()`); await wait(200);
  const re = await q("__sent.filter(m=>m.op=='open').map(m=>m.url)");
  ok('5. selecting a sleeping tab loads its page again, at its address', re.length == 1 && /example\.com\/a$/.test(re[0]), re);
  ok('6. ...and it is awake (no 💤)', !(await q("document.querySelectorAll('.win.act .otab')[0].textContent")).startsWith('💤'));
  ok('7. a tab that does not answer counts as busy (stays)', await q("(async()=>{const x=window.postMessage;window.postMessage=m=>{};const r=await rtBusy([...RT.keys()][0]);window.postMessage=x;return r.m&&r.f&&r.late})()"));
  await q("(()=>{S.gmsleep=false;return 1})()");
  ok('8. switched off in Settings: nothing sleeps', (await q("gslSleep()")) == 0);
  await q("(()=>{settings('health');return 1})()"); await wait(800);
  ok('9. Settings › System health has the switch', await q("!!document.querySelector('#gsl59ck')"));
  ok('10. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  const k = R.filter(Boolean).length; console.log(`${k}/${R.length} passed`); process.exit(k == R.length ? 0 : 1);
})().catch(e => { console.log('ERROR', e.stack); process.exit(1) });
