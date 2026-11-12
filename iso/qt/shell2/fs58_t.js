// 1.5.8: the desktop's fullscreen stand-in (shell26.js fsStandIn; Oracle's tabs use Chromium's real fullscreen) against a page that takes untouched
// copies of the fullscreen functions from an empty frame of its own, as YouTube's player does. Before, that reached
// Chromium's real fullscreen, which Controlled Frame puts straight back (YouTube went back to its size at once).
// node fs58_t.js [CHROME] [SHELL_JS]
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 300) : '')) };
const src = fs.readFileSync(process.argv[3] || path.join(__dirname, '../../../desktop/shell26.js'), 'utf8');
const fn = /\nfunction fsStandIn\(notify\) \{[\s\S]*?\n\}\n/.exec(src)[0];
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const p = await (await b.newContext()).newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.setContent(`<body><div id=player style="width:300px;height:200px;background:#000"><video id=v></video></div><button id=go>fs</button></body>`);
  await p.evaluate(`window.__real=0;document.addEventListener('fullscreenchange',()=>{if(document.fullscreenElement&&!document.fullscreenElement.classList.contains('rudolf-fs'))__real++});window.__asks=0;${fn};fsStandIn(on=>{__asks++;return true})`);
  // the page's trick: clean functions from a new empty frame
  await p.evaluate(`document.getElementById('go').onclick=()=>{const f=document.createElement('iframe');f.style.display='none';document.body.appendChild(f);
    const clean=f.contentWindow.Element.prototype.requestFullscreen;window.__cleanExit=f.contentWindow.Document.prototype.exitFullscreen;window.__cleanEl=Object.getOwnPropertyDescriptor(f.contentWindow.Document.prototype,'fullscreenElement').get;
    window.__p=clean.call(document.getElementById('player')).then(()=>'ok',e=>'no: '+e.message)}`);
  await p.click('#go'); const r = await p.evaluate('window.__p'); await p.waitForTimeout(100);
  const st = await p.evaluate(`({cls:document.getElementById('player').classList.contains('rudolf-fs'),el:document.fullscreenElement&&document.fullscreenElement.id,clean:(__cleanEl.call(document)||{}).id,asks:__asks,real:__real,w:document.getElementById('player').getBoundingClientRect().width})`);
  ok('1. fullscreen through a clean copy from an empty frame: the stand-in fills the page (not Chromium\'s real one)', r == 'ok' && st.cls && st.el == 'player' && st.asks == 1 && st.real == 0 && st.w > 600, {r, st});
  ok('...the clean copy of fullscreenElement says so too', st.clean == 'player', st);
  await p.evaluate("__cleanExit.call(document)"); await p.waitForTimeout(100);
  ok('2. leaving through the clean copy: back to its size', await p.evaluate("!document.fullscreenElement&&!document.getElementById('player').classList.contains('rudolf-fs')"));
  await p.evaluate("document.getElementById('go').onclick=()=>document.getElementById('v').webkitEnterFullscreen()"); await p.click('#go'); await p.waitForTimeout(100);
  ok('3. a video\'s own webkitEnterFullscreen: the stand-in', await p.evaluate("document.fullscreenElement&&document.fullscreenElement.id=='v'&&__real==0"));
  ok('no page error', !errs.length, errs);
  await b.close(); const k = R.filter(Boolean).length; console.log(`${k}/${R.length} passed`); process.exit(k == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
