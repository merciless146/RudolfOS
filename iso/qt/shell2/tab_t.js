// Oracle's real tabs inside the shell, end to end (host.sh start fresh first): node tab_t.js XIN SHOTDIR
// The page is placed over Oracle's window and follows it; what covers it is cut away; signing in keeps its cookie;
// the address and title follow the page; a sign-in pop-up keeps its opener; alert/confirm/prompt; shortcuts inside
// a page; Ctrl+W no longer closes the shell; the firewall; one set of cookies per account.
const {list, conn} = require('./cdp.js');
const {execFileSync} = require('child_process');
const path = require('path');
const XIN = process.argv[2], OUT = process.argv[3] || '.';
const X = (...a) => { for (let i = 0; i < a.length; i++) if (a[i] === 'move') { a[i + 1] += OFF[0]; a[i + 2] += OFF[1] } execFileSync(XIN, a.map(String), {env: Object.assign({}, process.env, {DISPLAY: ':77'})}) };
const wait = ms => new Promise(r => setTimeout(r, ms));
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
let shotN = 0, OFF = [0, 0];   // where the desktop's (0,0) is on the screen (no window manager: Chromium's window may sit off the corner)
function shot(name, pts) {
  const f = path.join(OUT, `tab_${String(++shotN).padStart(2, '0')}_${name}.png`);
  execFileSync('import', ['-display', ':77', '-window', 'root', f]);
  return JSON.parse(execFileSync('python3', [path.join(__dirname, 'px.py'), f, ...pts.map(p => [p[0] + OFF[0], p[1] + OFF[1]].join(','))]).toString());
}
const green = p => p[1] > 150 && p[0] < 60 && p[2] < 60;
(async () => {
  const desk = (await list()).find(t => t.url.startsWith('http://127.0.0.1:8080'));
  const d = await conn(desk.webSocketDebuggerUrl);
  const ev = async (e, g) => { const r = await d.send('Runtime.evaluate', {expression: e, awaitPromise: true, returnByValue: true, userGesture: !!g}); return r.result.exceptionDetails ? 'EXC ' + JSON.stringify(r.result.exceptionDetails).slice(0, 300) : r.result.result.value };
  const until = async (e, ms = 15000) => { const end = Date.now() + ms; while (Date.now() < end) { if (await ev(e) === true) return true; await wait(250) } return false };
  const inTab = async (match, e, g) => {
    const t = (await list()).find(x => x.type == 'webview' && match(x.url)); if (!t) return 'NO TAB';
    const c = await conn(t.webSocketDebuggerUrl);
    const r = await c.send('Runtime.evaluate', {expression: e, awaitPromise: true, returnByValue: true, userGesture: !!g}); c.close();
    return r.result.result ? r.result.result.value : r;
  };
  const tabUntil = async (match, e, ms = 15000) => { const end = Date.now() + ms; while (Date.now() < end) { try { if (await inTab(match, e) === true) return true } catch (x) {} await wait(300) } return false };
  OFF = await ev('[screenX,screenY]');
  ok('the desktop is in the shell with real tabs', await until('typeof rtOn=="function"&&rtOn()', 20000), await ev('JSON.stringify([!!SHELL,SHELL_CAPS])'));
  await until(`!!document.getElementById('oobe')||!!document.getElementById('login')&&getComputedStyle(document.getElementById('login')).display!='none'`, 8000);
  await ev(`try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});typeof closeAll=='function'&&closeAll();1`);

  // --- a website opens as a real tab, over the tab's area
  await ev(`browser('http://site.test:8099/login');window.__W=wins[wins.length-1];__W.el.style.left='100px';__W.el.style.top='60px';1`);
  ok('a website opens in a real tab', await until('RT.size==1', 5000) && await tabUntil(u => u.includes('site.test:8099/login'), 'document.readyState=="complete"'));
  await wait(800);
  let fr = await ev(`JSON.stringify((()=>{const r=__W.body.querySelector('.fh').getBoundingClientRect();return[r.left,r.top,r.right,r.bottom]})())`);
  fr = JSON.parse(fr); const mid = [Math.round((fr[0] + fr[2]) / 2), Math.round((fr[1] + fr[3]) / 2)];
  let p = shot('open', [mid, [fr[0] + 5, fr[3] - 5], [fr[2] - 5, fr[3] - 5], [fr[0] + 5, fr[1] - 12], [fr[0] - 20, mid[1]]]);
  ok('the page is shown exactly over the tab area (not over the toolbar or outside)', green(p[0]) && green(p[1]) && green(p[2]) && !green(p[3]) && !green(p[4]), p);
  ok('the Oracle iframe is not used', await ev(`[...__W.body.querySelectorAll('.fh iframe')].every(f=>f.style.display=='none')`));

  // --- signing in keeps its cookie (first-party), the address and title follow the page
  await inTab(u => u.includes('/login'), `document.getElementById('u').value='alice';document.getElementById('p').value='pw';document.getElementById('go').click();1`, true);
  ok('signing in works inside Oracle (cookie kept)', await tabUntil(u => u.includes('/me'), `document.getElementById('me')&&document.getElementById('me').textContent=='signed in as alice'`));
  ok('the address bar follows the page', await until(`__W.body.querySelector('.ad').value=='http://site.test:8099/me'`, 5000), await ev(`__W.body.querySelector('.ad').value`));
  ok('the tab shows the page title', await until(`__W.body.querySelector('.otab.on').title=='Account'`, 6000), await ev(`__W.body.querySelector('.otab.on').title`));
  ok('Back is available after the page navigated by itself', await ev(`!__W.body.querySelector('.bk').disabled`));
  await ev(`__W.body.querySelector('.bk').click();1`);
  ok('Back goes back inside the page', await tabUntil(u => u.includes('/login'), 'location.pathname=="/login"', 6000));
  await ev(`__W.body.querySelector('.fw').click();1`); await tabUntil(u => u.includes('/me'), 'location.pathname=="/me"', 6000);

  // --- moving the window: the page follows; a window on top cuts it; minimised: hidden
  await ev(`__W.el.style.left='300px';1`); await wait(500);
  fr = JSON.parse(await ev(`JSON.stringify((()=>{const r=__W.body.querySelector('.fh').getBoundingClientRect();return[r.left,r.top,r.right,r.bottom]})())`));
  p = shot('moved', [[fr[0] + 6, fr[1] + 6], [fr[0] - 30, fr[1] + 40]]);
  ok('the page follows the window when it moves', green(p[0]) && !green(p[1]), p);
  await ev(`(()=>{const N=win('Cover','📝',300,200);N.el.style.left='${fr[0] + 40}px';N.el.style.top='${fr[1] + 40}px';window.__N=N;return 1})()`); await wait(500);
  p = shot('covered', [[fr[0] + 120, fr[1] + 120], [fr[2] - 20, fr[3] - 20], [fr[0] + 10, fr[1] + 10]]);
  ok('a window on top hides the page where it covers it', !green(p[0]) && green(p[1]) && green(p[2]), p);
  await ev(`__W.body.querySelector('.mn').click();1`); await wait(300);
  p = shot('menu', [[fr[2] - 60, fr[1] + 20]]);
  ok('Oracle\'s own menu shows over the page', !green(p[0]), p);
  await ev(`document.body.dispatchEvent(new PointerEvent('pointerdown',{bubbles:true}));__N.close();1`); await wait(300);
  await ev(`__W.el.querySelector('.tb .mn').click();1`); await wait(400);
  p = shot('minimised', [[fr[0] + 60, fr[1] + 60]]);
  ok('a minimised Oracle window hides its page', !green(p[0]), p);
  await ev(`__W.focus();1`); await wait(400);
  p = shot('restored', [[fr[0] + 60, fr[1] + 60]]);
  ok('...and shows it again when restored', green(p[0]), p);
  await ev(`__W.body.querySelector('.otabs button').click();1`); await wait(500);
  p = shot('newtab', [[fr[0] + 60, fr[1] + 60]]);
  ok('another Oracle tab hides the real one', !green(p[0]) && await ev(`__W.body.querySelectorAll('.otab').length==2`), p);
  await ev(`__W.body.querySelectorAll('.otab')[0].click();1`); await wait(400);

  // --- a sign-in pop-up keeps its opener and closes itself
  const nwins = await ev('wins.length');
  await ev(`__W.body.querySelector('.ad').value='http://site.test:8099/popup';__W.body.querySelector('.ad').dispatchEvent(new KeyboardEvent('keydown',{key:'Enter'}));1`);
  await tabUntil(u => u.includes('/popup'), 'document.readyState=="complete"');
  await inTab(u => u.includes('/popup'), `document.getElementById('b').click();1`, true);
  ok('the pop-up opens in its own Oracle window', await until(`wins.length==${nwins + 1}`, 6000), await ev('wins.length'));
  ok('the pop-up talks to its opener (window.opener kept)', await tabUntil(u => u.includes('/popup'), `document.getElementById('got').textContent=='got token-123'`, 8000));
  ok('the pop-up closing itself closes its Oracle window', await until(`wins.length==${nwins}`, 6000), await ev('wins.length'));

  // --- alert, confirm, prompt
  await inTab(u => u.includes('/popup'), `setTimeout(()=>{window.__c=confirm('Leave this page?')},10);1`);
  ok('confirm() shows a dialog in the Oracle window', await until(`!!__W.body.querySelector('.sdlg .rto')&&__W.body.querySelector('.sdlg').textContent.includes('Leave this page?')`, 5000));
  await ev(`__W.body.querySelector('.sdlg .rto').click();1`);
  ok('...and the page gets the answer', await tabUntil(u => u.includes('/popup'), 'window.__c===true', 4000));
  await inTab(u => u.includes('/popup'), `setTimeout(()=>{window.__p=prompt('Name?','x')},10);1`);
  await until(`!!__W.body.querySelector('.sdlg .rtp')`, 5000);
  await ev(`__W.body.querySelector('.sdlg .rtp').value='Rudolf';__W.body.querySelector('.sdlg .rto').click();1`);
  ok('prompt() returns what was typed', await tabUntil(u => u.includes('/popup'), `window.__p==='Rudolf'`, 4000));

  // --- shortcuts inside a page (real keys), Ctrl+W no longer closes the shell
  fr = JSON.parse(await ev(`JSON.stringify((()=>{const r=__W.body.querySelector('.fh').getBoundingClientRect();return[r.left,r.top,r.right,r.bottom]})())`));
  X('move', fr[0] + 200, fr[1] + 200, 'click', 1, 'sleep', 400);
  // (on Rudolf OS, rudolf-display turns Ctrl+T and Ctrl+W into Ctrl+F21 and Ctrl+F20 for the desktop's window, X
  // key codes 199 and 198; here the X input comes straight from the test, so it sends what rudolf-display would)
  X('kdown', 'Control_L', 'kc', 199, 'kup', 'Control_L', 'sleep', 600);
  ok('Ctrl+T inside a page opens an Oracle tab', await until(`__W.body.querySelectorAll('.otab').length==3`, 3000), await ev(`__W.body.querySelectorAll('.otab').length`));
  X('kdown', 'Control_L', 'kc', 198, 'kup', 'Control_L', 'sleep', 1500);
  ok('Ctrl+W closes the Oracle tab, not the shell', await ev(`__W.body.querySelectorAll('.otab').length==2`) && (await list()).some(t => t.url.startsWith('isolated-app://')) && await ev(`typeof __W=='object'&&wins.includes(__W)`), await ev(`__W.body.querySelectorAll('.otab').length`));
  await ev(`__W.body.querySelectorAll('.otab')[0].click();1`); await wait(400);
  X('move', fr[0] + 200, fr[1] + 200, 'click', 1, 'sleep', 300, 'kdown', 'Control_L', 'key', 'l', 'kup', 'Control_L', 'sleep', 500);
  ok('Ctrl+L inside a page goes to the address bar', await ev(`document.activeElement==__W.body.querySelector('.ad')`));

  // --- the firewall applies inside real tabs
  await ev(`SECC.fw={...SECC.fw,on:true,block:['blocked.test']};rtFw(true);1`); await wait(300);
  await inTab(u => u.includes('/popup'), `location.href='http://blocked.test:8099/';1`);
  ok('the firewall blocks a site the page navigates to', await until(`__W.body.querySelector('.stat').textContent.includes('Blocked by the firewall')`, 6000), await ev(`__W.body.querySelector('.stat').textContent`));
  await ev(`SECC.fw={...SECC.fw,block:[]};rtFw(true);1`);

  // --- Flash (Ruffle) inside a real tab
  await ev(`__W.body.querySelector('.ad').value='http://site.test:8099/flash';__W.body.querySelector('.ad').dispatchEvent(new KeyboardEvent('keydown',{key:'Enter'}));1`);
  await tabUntil(u => u.includes('/flash'), 'document.readyState=="complete"');
  await wait(3000);
  const ruf = await inTab(u => u.includes('/flash'), `JSON.stringify({ruffle:!!document.querySelector('ruffle-embed,ruffle-object,ruffle-player'),tags:[...document.body.children].map(e=>e.tagName)})`);
  fr = JSON.parse(await ev(`JSON.stringify((()=>{const r=__W.body.querySelector('.fh').getBoundingClientRect();return[r.left,r.top,r.right,r.bottom]})())`));
  p = shot('flash', [[fr[0] + 150, fr[1] + 100]]);
  ok('Flash content in a real tab gets Ruffle', JSON.parse(ruf).ruffle && p[0][2] > 200 && p[0][1] < 60, [ruf, p]);
  ok('...and Oracle says so', await ev(`__W.body.querySelector('.stat').textContent.includes('Ruffle')`), await ev(`__W.body.querySelector('.stat').textContent`));

  // --- a page in fullscreen covers the screen, and comes back into its window
  const fb = JSON.parse(await inTab(u => u.includes('/flash'), `JSON.stringify((()=>{const r=document.getElementById('fs').getBoundingClientRect();return[r.left+r.width/2,r.top+r.height/2]})())`));
  X('move', Math.round(fr[0] + fb[0]), Math.round(fr[1] + fb[1]), 'click', 1, 'sleep', 1500);
  p = shot('fullscreen', [[1260, 400], [640, 780]]);
  ok('a page can go fullscreen (covers the screen)', green(p[0]) && green(p[1]), p);
  ok('...and sees itself fullscreen (document.fullscreenElement)', await inTab(u => u.includes('/flash'), '!!document.fullscreenElement'));
  ok('a page cannot go fullscreen without a click', await inTab(u => u.includes('/flash'), `document.exitFullscreen().then(()=>new Promise(r=>setTimeout(r,6000))).then(()=>document.documentElement.requestFullscreen()).then(()=>'went fullscreen',e=>'refused')`) == 'refused');
  X('move', Math.round(fr[0] + fb[0]), Math.round(fr[1] + fb[1]), 'click', 1, 'sleep', 1200, 'key', 'Escape', 'sleep', 1200);
  fr = JSON.parse(await ev(`JSON.stringify((()=>{const r=__W.body.querySelector('.fh').getBoundingClientRect();return[r.left,r.top,r.right,r.bottom]})())`));
  p = shot('fullscreen_exit', [[4, 4], [fr[2] - 10, fr[3] - 10]]);
  ok('...and goes back into its Oracle window', !green(p[0]) && green(p[1]), p);

  ok('Esc leaves fullscreen', await inTab(u => u.includes('/flash'), '!document.fullscreenElement'));

  // --- the desktop's own fullscreen (Media Player, games): fills the screen, hides real tabs, Esc leaves
  await ev(`(()=>{const b=document.createElement('button');b.id='fsb';b.style.cssText='position:fixed;left:900px;top:20px;width:80px;height:40px;z-index:2147483646';b.onclick=()=>{const v=document.createElement('div');v.id='fsv';v.style.background='rgb(255,0,255)';document.body.appendChild(v);v.requestFullscreen()};document.body.appendChild(b);return 1})()`);
  X('move', 940, 40, 'click', 1, 'sleep', 1000);
  p = shot('desk_fullscreen', [[4, 4], [1260, 780], [fr[0] + 60, fr[1] + 60]]);
  const mag = q => q[0] > 200 && q[1] < 60 && q[2] > 200;
  ok('the desktop can show something fullscreen, over real tabs', mag(p[0]) && mag(p[1]) && mag(p[2]) && await ev('document.fullscreenElement&&document.fullscreenElement.id')=='fsv', p);
  X('key', 'Escape', 'sleep', 800);
  ok('...and Esc leaves it', await ev('!document.fullscreenElement') && green(shot('desk_fullscreen_exit', [[fr[2] - 10, fr[3] - 10]])[0]));
  await ev(`document.getElementById('fsb').remove();document.getElementById('fsv').remove();1`);

  // --- display scaling (the desktop page is zoomed): the page still fits the tab area
  await ev(`__W.el.style.left='20px';__W.el.style.top='10px';__W.el.style.width='700px';__W.el.style.height='450px';document.documentElement.style.zoom='1.25';1`); await wait(800);
  fr = JSON.parse(await ev(`JSON.stringify((()=>{const r=__W.body.querySelector('.fh').getBoundingClientRect();return[r.left,r.top,r.right,r.bottom]})())`));
  const z = await ev(`JSON.stringify((()=>{const r=__W.body.querySelector('.fh').getBoundingClientRect(),e=document.elementFromPoint((r.left+r.right)/2,(r.top+r.bottom)/2);return [e&&e.className]})())`);
  p = shot('zoom125', [[fr[0] + 4, fr[3] - 4], [fr[2] - 4, fr[3] - 4], [fr[0] + 4, fr[1] - 10], [fr[2] + 8, fr[3] - 30]]);
  console.log('zoomed tab area', JSON.stringify(fr), z);
  ok('with display scaling the page still sits exactly in the tab area', green(p[0]) && green(p[1]) && !green(p[2]) && !green(p[3]), p);
  await ev(`document.documentElement.style.zoom='';1`); await wait(400);

  // --- downloads go to C:\DOWNLOADS through Oracle's checks, with the site's cookies
  await ev(`__W.body.querySelector('.ad').value='http://site.test:8099/files';__W.body.querySelector('.ad').dispatchEvent(new KeyboardEvent('keydown',{key:'Enter'}));1`);
  await tabUntil(u => u.includes('/files'), 'document.readyState=="complete"');
  await inTab(u => u.includes('/files'), `document.getElementById('f').click();1`, true);
  ok('a download from a real tab is saved in C:\\DOWNLOADS (signed in: the cookie came along)', await until(`!!(fs['C:'].DOWNLOADS&&fs['C:'].DOWNLOADS['NOTES.TXT']=='notes for alice')`, 10000), await ev(`JSON.stringify(DLS.slice(-1))`));
  await inTab(u => u.includes('/files'), `document.getElementById('b').click();1`, true);
  ok('a download over the size limit is stopped, with a reason', await until(`DLS.length&&DLS[DLS.length-1].n=='BIG.ZIP'&&DLS[DLS.length-1].st=='blocked'`, 10000), await ev(`JSON.stringify(DLS.slice(-1))`));

  // --- without the browser window: what asked for it opens in Oracle; the sign-in settings say so
  const nw = await ev('wins.length');
  await ev(`window.open('http://site.test:8099/');1`);
  ok('a new browser window asked for by the desktop opens in Oracle', await until(`wins.length==${nw + 1}&&wins[wins.length-1].tb.title.includes('Oracle')`, 5000), await ev(`JSON.stringify(wins.map(w=>w.tb.title))`));
  await ev(`wins[wins.length-1].close();1`);
  ok('Settings › Oracle sign-in says sign-ins work in real tabs', await ev(`(()=>{const c=document.createElement('div');signinPanel(c);return c.textContent.includes('real browser tabs now')})()`));

  // --- cookies are kept, and each account has its own
  await ev(`__W.close();1`); await wait(500);
  ok('closing Oracle closes its real tabs', await ev('RT.size==0') && !(await list()).some(t => t.type == 'webview' && t.url.includes('site.test')));
  await ev(`browser('http://site.test:8099/me');window.__W=wins[wins.length-1];1`);
  ok('the sign-in is still there in a new Oracle window', await tabUntil(u => u.includes('/me'), `document.getElementById('me')&&document.getElementById('me').textContent=='signed in as alice'`));
  await ev(`__W.close();window.__U=USER;USER={...USER,n:'bob'};browser('http://site.test:8099/me');window.__W=wins[wins.length-1];1`);
  ok('another account does not share the cookies', await tabUntil(u => u.includes('/me'), `document.getElementById('me')&&document.getElementById('me').textContent=='not signed in'`));
  await ev(`__W.close();USER=__U;1`);

  console.log(`RESULT ${R.filter(Boolean).length}/${R.length}`);
  process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(2) });
