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
  ok('7b. background work waits in game mode (the system update check, Linux apps\' and Debian\'s daily updates)', await q("[sysuTick,lxAutoUpdate,debAutoUpdate].every(f=>/gameMode\\(\\)/.test(f.toString()))"));
  const ai = await q(`(async()=>{const calls=[];const la=lxApi;lxApi=async p=>{calls.push(p);return p=='ai/status'?{loaded:true,mem:1500000000}:{ok:true}};
    const n=await gslBlitzen();lxApi=la;return {n,calls}})()`);
  ok('7c. Blitzen\'s model is unloaded in game mode (its next question loads it again)', ai.n > 0 && ai.calls.join() == 'ai/status,ai/unload', ai);
  // 9.2b: the guard's warning, in game mode: shown inside the full-screen page
  await q(`(()=>{dispatchEvent(new MessageEvent('message',{source:window,data:{rudolfShell:'tabev',tid:${JSON.stringify(gameTid)},ev:'fs',on:true}}));__sent.length=0;document.querySelectorAll('.ntoast').forEach(x=>x.remove());evDispatch('memory warn 4242 1940\\n');return 1})()`); await wait(300);
  const nt = await q("__sent.filter(m=>m.op=='notice')");
  ok('7d. memory getting low in game mode: a notice inside the full-screen game, with the page\'s size', nt.length == 1 && nt[0].tid == gameTid && /almost full/.test(nt[0].text) && /1\.9 GB/.test(nt[0].text), nt);
  await q(`(()=>{dispatchEvent(new MessageEvent('message',{source:window,data:{rudolfShell:'tabev',tid:${JSON.stringify(gameTid)},ev:'fs',on:false}}));return 1})()`);
  // the notice's own code (shell.js) in a page whose element is full screen: it is in the top layer, above it
  const SH = fs.readFileSync(path.join(__dirname, '../../shell/shell.js'), 'utf8');
  const nj = eval(SH.match(/const noticeJS = ([\s\S]*?`;)\n/)[1].replace(/;$/, ''))('Memory is almost full.', 5);
  const p2 = await b.newPage(); await p2.setContent('<div id=g style="width:200px;height:100px;background:#000" onclick="this.requestFullscreen()"></div>'); await p2.click('#g'); await wait(300);
  await p2.evaluate(nj); await wait(100);
  ok('7e. the notice shows over the page\'s own full screen (a popover, in the top layer)', await p2.evaluate("!!document.fullscreenElement&&document.getElementById('rudolf-notice').matches(':popover-open')&&/Memory is almost full/.test(document.getElementById('rudolf-notice').textContent)"));
  await p2.close();
  // 9.2 step 4: background apps with no window are offered to be quit, inside the game, by its button only
  await q(`(()=>{dispatchEvent(new MessageEvent('message',{source:window,data:{rudolfShell:'tabev',tid:${JSON.stringify(gameTid)},ev:'fs',on:true}}));__sent.length=0;
    window.__ended=[];apps57=async()=>[{id:'fp:com.discordapp.Discord',kind:'flatpak',app:'com.discordapp.Discord',name:'Discord',window:false,rss:620*1048576},{id:'w1',name:'Steam',window:true,rss:900*1048576},{id:'s',name:'Small',window:false,rss:20*1048576}];
    appName57=x=>x.name;appEnd57=async x=>{__ended.push(x.name)};return 1})()`);
  const n4 = await q("gslTrayOffer()"); const off = await q("__sent.filter(m=>m.op=='notice')");
  ok('7f. game mode offers to quit apps with no window (Discord, 620 MB), not those with a window or small ones', n4 == 1 && off.length == 1 && /Free 620(\.00)? MB for the game\? Discord runs in the background/.test(off[0].text) && off[0].btn == 'Quit it' && off[0].id > 0, off);
  ok('7g. nothing is quit until the button is pressed', (await q("__ended")).length == 0);
  await q(`(()=>{dispatchEvent(new MessageEvent('message',{source:window,data:{rudolfShell:'tabev',tid:${JSON.stringify(gameTid)},ev:'notice',id:${off.length ? off[0].id : 0}}}));return 1})()`); await wait(200);
  ok('7h. pressing it quits them', (await q("__ended")).join() == 'Discord');
  await q(`(()=>{dispatchEvent(new MessageEvent('message',{source:window,data:{rudolfShell:'tabev',tid:${JSON.stringify(gameTid)},ev:'fs',on:false}}));return 1})()`);
  // the notice's button in a real page: a person's click answers, a script's click does not
  const SH2 = fs.readFileSync(path.join(__dirname, '../../shell/shell.js'), 'utf8');
  const NONCE = 'rudolfTEST';
  const nj2 = eval(SH2.match(/const noticeJS = ([\s\S]*?`;)\n/)[1].replace(/;$/, ''))('Free 620 MB for the game?', 10, 'Quit it', 7);
  const p3 = await b.newPage(); const said = []; p3.on('console', m => { if (m.text().startsWith(NONCE)) said.push(m.text().slice(NONCE.length)) });
  await p3.setContent('<body>game</body>'); await p3.evaluate(nj2); await wait(100);
  await p3.evaluate("document.querySelector('#rudolf-notice button').click()"); await wait(100);
  ok('7i. a script in the page pressing the button does nothing', said.length == 0 && await p3.evaluate("!!document.getElementById('rudolf-notice')"), said);
  await p3.click('#rudolf-notice button'); await wait(100);
  ok('7j. a person pressing it answers (notice 7) and the notice goes', said.join() == '{"notice":7}' && await p3.evaluate("!document.getElementById('rudolf-notice')"), said);
  await p3.close();
  await q("(()=>{S.gmsleep=false;return 1})()");
  ok('8. switched off in Settings: nothing sleeps', (await q("gslSleep()")) == 0);
  await q("(()=>{settings('health');return 1})()"); await wait(800);
  ok('9. Settings › System health has the switch', await q("!!document.querySelector('#gsl59ck')"));
  ok('10. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  const k = R.filter(Boolean).length; console.log(`${k}/${R.length} passed`); process.exit(k == R.length ? 0 : 1);
})().catch(e => { console.log('ERROR', e.stack); process.exit(1) });
