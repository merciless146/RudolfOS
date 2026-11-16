/* 1.6.0: Oracle's dark pages over a playing video (YouTube's progress bar, the Vivobook, 2026-11-15: a rectangle of the
   video drawn as a negative while the mouse is on the bar). The shell's own dark-pages script (iso/shell/shell.js) is
   run in a page with videos and the kinds of layers a player puts over them; each video, seen through each layer, must
   look as it does without dark pages: its brightness follows the original's, not its negative's (a dark scrim over
   the video turns light, as every colour of the page does, so a little less than 1).
     node darkvideo60.js DIR PORT [chromium flags...]      (DIR has v.webm; run by darkvideo60_t.sh)
   CHROME: another Chromium (a release's, from its chromium.sfs); SHELLJS: another shell.js (an older one, to compare) */
const {chromium} = require('playwright'); const http = require('http'); const fs = require('fs'); const path = require('path');
const [DIR, PORT, ...FLAGS] = process.argv.slice(2);
const SHELL = fs.readFileSync(process.env.SHELLJS || path.join(__dirname, '../../shell/shell.js'), 'utf8');
const m = /\nlet DARK = false;\n([\s\S]*?)\nconst darken =/.exec(SHELL);
if (!m) { console.log('FAIL the dark-pages script was not found in shell.js'); process.exit(1) }
const darkJS = new Function(m[1] + '\nreturn darkJS')();
/* each case: a player (a paused video) with something over its left half, or around the video */
const CASES = {
  plain: '<div class=o style="background:rgba(255,255,255,.15)"></div>',
  gradient: '<div class=o style="background:linear-gradient(transparent,rgba(0,0,0,.5))"></div>',
  'backdrop-filter': '<div class=o style="backdrop-filter:blur(1px);background:rgba(0,0,0,.1)"></div>',
  'mix-blend-mode': '<div class=o style="mix-blend-mode:screen;background:rgba(40,40,40,.5)"></div>',
  'inline background + backdrop-filter': '<div class=o style="background-image:linear-gradient(transparent,rgba(0,0,0,.2));backdrop-filter:blur(1px)"></div>',
  'inline background + mix-blend-mode': '<div class=o style="background-image:linear-gradient(rgba(30,30,30,.4),rgba(30,30,30,.4));mix-blend-mode:screen"></div>',
  'canvas + backdrop-filter': '<canvas class=o style="backdrop-filter:blur(1px)"></canvas>',
  'svg + backdrop-filter': '<svg class=o style="backdrop-filter:blur(1px)"><image href="t.png" width=10 height=10 /></svg>',
  'video inside an inline background': 'WRAP',
};
const page = dark => `<!doctype html><html><head><style>body{margin:0;background:#fff}
.p{position:relative;width:256px;height:144px;display:inline-block;margin:6px;background:#000;overflow:hidden}
.p video{width:100%;height:100%;display:block}.o{position:absolute;left:0;top:0;width:50%;height:100%;display:block}
</style></head><body>${Object.entries(CASES).map(([k, v], i) => v == 'WRAP'
  ? `<div class=p id=c${i}><div style="background-image:linear-gradient(#000,#000);width:100%;height:100%"><video src=v.webm muted preload=auto></video></div></div>`
  : `<div class=p id=c${i}><video src=v.webm muted preload=auto></video>${v}</div>`).join('')}</body></html>`;
const T = {webm: 'video/webm', png: 'image/png', html: 'text/html'};
const srv = http.createServer((q, r) => {
  const u = q.url.split('?')[0];
  if (u == '/p.html') { r.setHeader('content-type', 'text/html'); return r.end(page()) }
  if (u == '/darkpage.html') { r.setHeader('content-type', 'text/html'); return r.end('<!doctype html><html style="background:#111"><body style="margin:0;color:#ddd"><p>a dark page with a clear body</p></body></html>') }
  if (u == '/lightpage.html') { r.setHeader('content-type', 'text/html'); return r.end('<!doctype html><html><body style="margin:0;background:#fff"><p>a light page</p></body></html>') }
  const f = path.join(DIR, path.basename(u)); if (!fs.existsSync(f)) { r.statusCode = 404; return r.end() }
  r.setHeader('content-type', T[f.split('.').pop()] || 'application/octet-stream'); r.end(fs.readFileSync(f));
}).listen(+PORT, '127.0.0.1');
let P = 0, F = 0; const ok = (n, c, why) => { if (c) { P++; console.log('PASS ' + n) } else { F++; console.log('FAIL ' + n + (why ? ' - ' + why : '')) } };
const corr = (a, b) => { const n = a.length, ma = a.reduce((s, x) => s + x, 0) / n, mb = b.reduce((s, x) => s + x, 0) / n;
  let sab = 0, sa = 0, sb = 0; for (let i = 0; i < n; i++) { sab += (a[i] - ma) * (b[i] - mb); sa += (a[i] - ma) ** 2; sb += (b[i] - mb) ** 2 } return sab / Math.sqrt(sa * sb || 1) };
(async () => {
  const b = await chromium.launch(process.env.CHROME ? {executablePath: process.env.CHROME, args: [...FLAGS, '--no-sandbox']} : {args: FLAGS});
  console.log('Chromium ' + b.version()); const p = await b.newPage({viewport: {width: 1200, height: 700}});
  const shot = async () => { const buf = await p.screenshot(); return buf };
  const lum = async (buf, boxes) => p.evaluate(async ([b64, boxes]) => {
    const im = new Image(); im.src = 'data:image/png;base64,' + b64; await im.decode();
    const c = document.createElement('canvas'); c.width = im.width; c.height = im.height; const x = c.getContext('2d'); x.drawImage(im, 0, 0);
    return boxes.map(([l, t, w, h]) => { const d = x.getImageData(l, t, w, h).data, o = []; for (let i = 0; i < d.length; i += 4 * 7) o.push(.2126 * d[i] + .7152 * d[i + 1] + .0722 * d[i + 2]); return o });
  }, [buf.toString('base64'), boxes]);
  await p.goto(`http://127.0.0.1:${PORT}/p.html`);
  await p.evaluate(async () => { for (const v of document.querySelectorAll('video')) { await new Promise(r => { if (v.readyState >= 2) r(); else v.addEventListener('loadeddata', r, {once: true}) }); v.currentTime = 3.3; await new Promise(r => v.addEventListener('seeked', r, {once: true})) } });
  await p.waitForTimeout(300);
  const boxes = await p.evaluate(() => [...document.querySelectorAll('.p')].map(e => { const r = e.getBoundingClientRect(); return [[r.x + 4, r.y + 4, r.width / 2 - 8, r.height - 8], [r.x + r.width / 2 + 4, r.y + 4, r.width / 2 - 8, r.height - 8]] }));
  const flat = boxes.flat();
  const L0 = await lum(await shot(), flat);
  await p.evaluate(darkJS(true)); await p.waitForTimeout(300);
  ok('0. the dark-pages style is in the page', await p.evaluate(() => !!document.getElementById('rudolf-dark')));
  const b1 = await shot(); if (process.env.SHOT) fs.writeFileSync(process.env.SHOT, b1);
  const L1 = await lum(b1, flat);
  Object.keys(CASES).forEach((k, i) => {
    const under = corr(L0[2 * i], L1[2 * i]), beside = corr(L0[2 * i + 1], L1[2 * i + 1]);
    ok(`${i + 1}. ${k}: the video is not a negative (under ${under.toFixed(2)}, beside ${beside.toFixed(2)})`, under > .5 && beside > .5,
      under < 0 || beside < 0 ? 'drawn as a negative' : 'changed');
  });
  await p.evaluate(darkJS(false)); await p.waitForTimeout(200);
  const L2 = await lum(await shot(), flat);
  ok(`${Object.keys(CASES).length + 1}. dark pages off again: as before`, L0.every((a, i) => corr(a, L2[i]) > .98));
  /* the check that leaves dark pages alone: it must not read the white that dark pages itself gives <html> */
  for (const [u, want, n] of [['darkpage.html', false, 'a dark page with a clear <body> (the colour on <html>) is left as it is'], ['lightpage.html', true, 'a light page is turned dark']]) {
    await p.goto(`http://127.0.0.1:${PORT}/${u}`); await p.evaluate(darkJS(true)); await p.waitForTimeout(200);
    ok(`${Object.keys(CASES).length + (want ? 3 : 2)}. ${n}`, (await p.evaluate(() => !!document.getElementById('rudolf-dark'))) === want);
  }
  await b.close(); srv.close();
  console.log(`${P}/${P + F} passed`); process.exit(F ? 1 : 0);
})().catch(e => { console.log('FAIL ' + e); process.exit(1) });
