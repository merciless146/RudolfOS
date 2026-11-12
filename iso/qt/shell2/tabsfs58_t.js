// 1.5.8: fullscreen in Oracle's real tabs is Chromium's own (the shell's manifest lets websites use it), on a page
// with a strict Content Security Policy and Trusted Types as YouTube has; and a website opened before the shell said
// hello moves to a real tab once it has. (host.sh start fresh first.)   node tabsfs58_t.js XIN [PORT]
const {list, conn} = require('./cdp.js');
const http = require('http'), {execFileSync} = require('child_process');
const XIN = process.argv[2], PORT = +(process.argv[3] || 8248);
const wait = ms => new Promise(r => setTimeout(r, ms));
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (!c && d !== undefined ? ' - ' + JSON.stringify(d).slice(0, 300) : '')) };
const CSP = "script-src 'nonce-n0' 'strict-dynamic'; require-trusted-types-for 'script'; trusted-types tube";
const PAGE = `<!doctype html><html><head><title>Tube</title><script nonce="n0">trustedTypes.createPolicy('tube',{createHTML:s=>s})</script></head>
<body style="margin:0"><div id=player style="width:320px;height:180px;background:#000"><video></video></div>
<button id=go style="position:absolute;left:20px;top:220px;width:120px;height:40px">fs</button>
<script nonce="n0">window.CH=[];document.addEventListener('fullscreenchange',()=>CH.push(document.fullscreenElement?document.fullscreenElement.id:'none'));
document.getElementById('go').onclick=()=>document.getElementById('player').requestFullscreen().catch(e=>CH.push('err '+e.message))</script></body></html>`;
const srv = http.createServer((q, s) => { s.writeHead(200, {'Content-Type': 'text/html', 'Content-Security-Policy': CSP, 'Cache-Control': 'no-store'}); s.end(PAGE) }).listen(PORT, '127.0.0.1');
(async () => {
  const desk = (await list()).find(t => t.url.startsWith('http://127.0.0.1:8080'));
  const d = await conn(desk.webSocketDebuggerUrl);
  const ev = async e => { const r = await d.send('Runtime.evaluate', {expression: e, awaitPromise: true, returnByValue: true}); return r.result.exceptionDetails ? 'EXC ' + JSON.stringify(r.result.exceptionDetails).slice(0, 300) : r.result.result.value };
  const until = async (f, ms = 10000) => { const end = Date.now() + ms; while (Date.now() < end) { try { if (await f()) return true } catch (x) {} await wait(250) } return false };
  const tab = async (q, e, g) => { const t = (await list()).find(x => x.type == 'webview' && x.url.includes(q)); if (!t) return 'NO TAB'; const c = await conn(t.webSocketDebuggerUrl);
    const r = await c.send('Runtime.evaluate', {expression: e, awaitPromise: true, returnByValue: true, userGesture: !!g}); c.close(); return r.result.result ? r.result.result.value : r };
  const shell = async e => { const t = (await list()).find(x => x.url.startsWith('isolated-app://')); const c = await conn(t.webSocketDebuggerUrl);
    const r = await c.send('Runtime.evaluate', {expression: e, returnByValue: true}); c.close(); return r.result.result ? r.result.result.value : r };
  const OFF = await ev('[screenX,screenY]');
  const X = (...a) => execFileSync(XIN, a.map(String), {env: Object.assign({}, process.env, {DISPLAY: ':77'})});
  // the test machine has no network of its own (rudolfd says offline): Oracle is told it is online
  await ev(`window.__net=0;try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});typeof closeAll=='function'&&closeAll();1`);
  ok('the desktop is in the shell with real tabs', await until(async () => await ev('rtOn()') === true, 20000));
  const U = `http://tube.test:${PORT}/watch`;
  await ev(`browser('${U}?a');1`);
  ok('1. the page opens in a real tab', await until(async () => await tab('/watch?a', 'document.readyState') == 'complete'));
  ok('2. its title reaches Oracle (the page scripts run on a page with Trusted Types)', await until(async () => await ev(`wins.some(w=>/Tube - Oracle/.test(w.el.textContent))`)));
  const inner0 = await tab('/watch?a', 'innerWidth');
  // a real click on the page's button (a user activation, as with YouTube's fullscreen button)
  const r = JSON.parse(await ev(`(()=>{const w=wins[wins.length-1];w.focus();const r=w.body.querySelector('.fh').getBoundingClientRect();return JSON.stringify([r.left,r.top])})()`));
  X('move', OFF[0] + r[0] + 80, OFF[1] + r[1] + 240, 'click', 1); await wait(1500);
  let st = await tab('/watch?a', `JSON.stringify({fe:document.fullscreenElement&&document.fullscreenElement.id,ch:CH,w:innerWidth,pw:document.getElementById('player').getBoundingClientRect().width,sw:screen.width})`);
  st = JSON.parse(st);
  ok('3. fullscreen: the player fills the screen and stays (Chromium did not take it back)', st.fe == 'player' && st.pw >= st.sw - 2 && st.ch.join() == 'player', st);
  ok('...the tab\'s frame is what the shell shows fullscreen', await shell(`!!document.fullscreenElement&&document.fullscreenElement.tagName=='CONTROLLEDFRAME'`) === true);
  X('key', 'Escape'); await wait(1200);
  st = JSON.parse(await tab('/watch?a', `JSON.stringify({fe:document.fullscreenElement&&document.fullscreenElement.id,ch:CH,w:innerWidth})`));
  ok('4. Esc leaves fullscreen: the page is back at its size in Oracle\'s window', !st.fe && st.w == inner0 && st.ch.join() == 'player,none', [st, inner0]);
  ok('...and the shell is no longer fullscreen', await shell('!document.fullscreenElement') === true);
  await ev('closeAll();1');
  ok('5. closing all windows (signing out) closes their real tabs too', await until(async () => (await tab('/watch?a', '1')) === 'NO TAB'));
  // 5. a website opened before the shell said hello
  // (the shell says hello again now and then; here the desktop is made to have missed it for a moment)
  await ev('closeAll();1'); await wait(500);
  const before = await ev(`(()=>{window.__caps=SHELL_CAPS;SHELL_CAPS=[];browser('${U}?b');const n=RT.size;SHELL_CAPS=window.__caps;return n})()`);
  await ev(`dispatchEvent(new Event('rudolf-shell'));1`);
  ok('6. a page opened before the shell said hello moves to a real tab when it does', before === 0 && await until(async () => await tab('/watch?b', 'document.readyState') == 'complete'), before);
  await ev('closeAll();1'); d.close(); srv.close();
  const k = R.filter(Boolean).length; console.log(`${k}/${R.length} passed`); process.exit(k == R.length ? 0 : 1);
})().catch(e => { console.log('ERROR', e.stack); process.exit(1) });
