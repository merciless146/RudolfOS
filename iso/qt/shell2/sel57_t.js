// 1.5.7 (asked on 2026-11-10): the selection highlight in desktop windows is the Start menu's: the accent colour with
// white writing, or black on a light accent (--acfg); hovering tints the row and keeps the window's writing colour.
// Before, light bars (#e5e7eb, #dbe6ff, #dbeafe) sat under the dark theme's light writing (Settings, File Explorer).
// Checks the contrast (WCAG) of selected and hovered rows in Settings, File Explorer, Task Manager and the other lists,
// in the light and the dark theme, with a blue, a yellow and a very light accent.
// node sel57_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 600) : '')) };
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }
  const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200); fs.createReadStream(f).pipe(s) }).listen(8179, '127.0.0.1');
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const p = await (await b.newContext({viewport: {width: 1280, height: 800}})).newPage(); const errs = []; p.on('pageerror', e => errs.push(e.stack.slice(0,400)));
  await p.goto('http://127.0.0.1:8179/'); await p.waitForFunction(() => typeof settings == 'function' && typeof themeApply == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';window.netPopFill=()=>{};return 1})()");
  // contrast of an element's writing against what is drawn behind it (translucent backgrounds composited upwards)
  await q(`window.__cr=el=>{const P=c=>{let m=/color\\(srgb ([^)]+)\\)/.exec(c);if(m){const v=m[1].split(/[ \\/]+/).filter(Boolean).map(Number);return[v[0]*255,v[1]*255,v[2]*255,v[3]==null?1:v[3]]}
      m=/rgba?\\(([^)]+)\\)/.exec(c);if(!m)throw new Error('colour '+c);const v=m[1].split(/[ ,\\/]+/).filter(Boolean).map(Number);return[v[0],v[1],v[2],v[3]==null?1:v[3]]};
    const layers=[];for(let e=el;e;e=e.parentElement){const c=P(getComputedStyle(e).backgroundColor);if(c[3]>0)layers.push(c);if(c[3]>=1)break}
    let bg=[255,255,255];for(let i=layers.length-1;i>=0;i--){const[r,g,b,a]=layers[i];bg=[r*a+bg[0]*(1-a),g*a+bg[1]*(1-a),b*a+bg[2]*(1-a)]}
    const fg=P(getComputedStyle(el).color);const L=c=>{const x=c.slice(0,3).map(v=>{v/=255;return v<=.03928?v/12.92:Math.pow((v+.055)/1.055,2.4)});return .2126*x[0]+.7152*x[1]+.0722*x[2]};
    const a=L(fg),bb=L(bg);return +((Math.max(a,bb)+.05)/(Math.min(a,bb)+.05)).toFixed(2)};1`);
  const theme = (dark, ac) => q(`(()=>{S.dm=${dark ? "'dark'" : "'light'"};S.darkui=true;window.dynResolve=()=>({mode:${dark ? "'dark'" : "'light'"}});S.accent='${ac}';themeApply();return document.documentElement.classList.contains('dark')==${dark}})()`);
  // the lists drawn the way the apps draw them (classes from settings25, files25, tm26, base: .xt, .np, linux26 .lxit, the calendar)
  await q(`(()=>{const W=document.createElement('div');W.className='win act';W.id='selwin';W.style.cssText='position:fixed;left:680px;top:0;width:600px;height:500px;z-index:999999';
    W.innerHTML="<div class='body'><div class='xt'><div class='on'>Folder on</div><div>Folder</div></div><div class='np'><div class='npr on'>Network on</div><div class='npr'>Network</div></div>"+
     "<div class='lxit on'>Linux app on</div><div class='lxit'>Linux app</div><table class='tmt'><tbody><tr class='on'><td>proc on</td></tr><tr><td>proc</td></tr></tbody></table>"+
     "<table class='calbig'><tr><td class='sel'><div class='cd'>12</div></td></tr></table><div class='xp25'><table><tbody><tr class='s'><td>file on</td></tr><tr><td>file</td></tr></tbody></table></div></div>";
    document.body.appendChild(W);return 1})()`);
  await q("settings('disp')"); await p.waitForTimeout(400);
  const sets = [['light', false, '#2563eb'], ['dark', false, '#ca8a04'], ['dark', true, '#2563eb'], ['dark', true, '#ca8a04'], ['dark', true, '#fde68a'], ['light', false, '#fde68a']];
  for (const [nm, dark, ac] of sets) {
    ok(`theme ${nm} (accent ${ac}) applied`, await theme(dark, ac));
    const fg = await q("getComputedStyle(document.documentElement).getPropertyValue('--acfg').trim()");
    ok(`...the writing on the accent is ${fg}`, fg == (['#ca8a04', '#fde68a'].includes(ac) ? '#000000' : '#ffffff'), fg);
    const sel = await q(`(()=>{const out={};for(const s of ['#selwin .xt div.on','#selwin .npr.on','#selwin .lxit.on','#selwin .tmt tr.on td','#selwin .calbig td.sel','#selwin .xp25 tr.s td','.sg .sn div.on']){const e=document.querySelector(s);out[s]=e?__cr(e):'missing'}return out})()`);
    for (const [k, v] of Object.entries(sel)) ok(`  selected ${k}: contrast ${v} (at least 4.5)`, typeof v == 'number' && v >= 4.5, v);
    // hover: Settings' page list and the rows in the test window
    for (const s of ['.sg .sn div:not(.on)', '#selwin .xt div:not(.on)', '#selwin .npr:not(.on)', '#selwin .lxit:not(.on)', '#selwin .tmt tr:not(.on) td', '#selwin .xp25 tr:not(.s) td']) {
      await p.hover(s); const v = await q(`__cr(document.querySelector(${JSON.stringify(s)}))`);
      const bg = await q(`getComputedStyle(document.querySelector(${JSON.stringify(s)})).backgroundColor`);
      const fgc = await q(`getComputedStyle(document.querySelector(${JSON.stringify(s)})).color`);
      ok(`  hovered ${s}: contrast ${v} (at least 4.5), tinted (${bg})`, v >= 4.5 && bg != 'rgba(0, 0, 0, 0)', {v, bg, fgc});
    }
  }
  // the Start menu keeps its look: accent with the --acfg writing
  await theme(true, '#2563eb');
  const m = await q("(()=>{const r=[...document.styleSheets].flatMap(s=>{try{return[...s.cssRules]}catch(e){return[]}}).map(r=>r.cssText).filter(t=>t.includes('#menu li:hover')&&t.includes('--acfg'));return r.length})()");
  ok('the Start menu uses the same rule (accent, --acfg writing)', m > 0, m);
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
