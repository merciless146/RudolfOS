// 1.5.9 (plan 15.2): safeHtml (desktop/sanitize59.js) against the known ways of getting script past an HTML cleaner,
// each shown in the desktop's page with a canary that must never be called; and what documents need kept.
//   node sanitize59_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 900) : '')) };
const srv = http.createServer((q, s) => { const u = q.url.split('?')[0];
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};window.__hit=[];window.__pwn=function(id){window.__hit.push(String(id))};</script>')) }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000); s.writeHead(200, {'content-type': 'application/json'}); return s.end('{"ok":true}') }
    s.writeHead(404); s.end() }) }).listen(8304, '127.0.0.1');
const c = id => `top.__pwn('${id}')`;
const V = {
  'img onerror': `<img src=x onerror="${c('a')}">`,
  'a slash for the space': `<img src=x/onerror=${c('b')}>`,
  'upper case': `<IMG SRC=x ONERROR="${c('c')}">`,
  'a tab and a newline': `<img\tsrc=x\nonerror="${c('d')}">`,
  'entities in the handler': `<img src=x onerror="&#116;op.__pwn('e')">`,
  'svg onload': `<svg onload="${c('f')}"></svg>`,
  'svg animate setting an href': `<svg><a><animate attributeName=href values="javascript:${c('g')}"/><text x=20 y=20>x</text></a></svg>`,
  'svg set': `<svg><a><set attributeName="href" to="javascript:${c('h')}"/><text>x</text></a></svg>`,
  'script': `<script>${c('i')}</script>`,
  'script split by a comment': `<scr<!-- -->ipt>${c('j')}</script>`,
  'a nested script tag': `<scr<script>ipt>${c('k')}</scr</script>ipt>`,
  'iframe srcdoc': `<iframe srcdoc="<script>${c('l')}</script>"></iframe>`,
  'object data': `<object data="javascript:${c('m')}"></object>`,
  'embed': `<embed src="javascript:${c('n')}">`,
  'details ontoggle': `<details open ontoggle="${c('o')}">`,
  'body onload': `<body onload="${c('p')}">`,
  'input autofocus onfocus': `<input autofocus onfocus="${c('q')}">`,
  'video source onerror': `<video><source onerror="${c('r')}"></video>`,
  'math with a javascript href': `<math><mtext><a href="javascript:${c('s')}">x</a></mtext></math>`,
  'mXSS through noscript': `<noscript><p title="</noscript><img src=x onerror=${c('t')}>">`,
  'mXSS through svg and style': `<svg></p><style><a id="</style><img src=1 onerror=${c('u')}>">`,
  'form formaction': `<form><button formaction="javascript:${c('v')}">x</button></form>`,
  'meta refresh': `<meta http-equiv="refresh" content="0;url=javascript:${c('w')}">`,
  'base href': `<base href="javascript:${c('x')}//">`,
  'link import': `<link rel=import href="data:text/html,<script>${c('y')}</script>">`,
  'marquee onstart': `<marquee onstart="${c('z')}">x</marquee>`,
  'foreignObject': `<svg><foreignObject><img src=x onerror="${c('fo')}"></foreignObject></svg>`,
  'template content': `<template><img src=x onerror="${c('tp')}"></template>`,
  'srcset': `<img srcset="javascript:${c('ss')} 1x">`,
};
const LINKS = { 'javascript: link': `<a href="javascript:${c('L1')}">x</a>`, 'javascript: with spaces and case': `<a href=" JaVaScRiPt:${c('L2')}">x</a>`,
  'javascript: with a tab inside': `<a href="java\tscript:${c('L3')}">x</a>`, 'entity-encoded javascript:': `<a href="&#106;avascript:${c('L4')}">x</a>`,
  'vbscript:': `<a href="vbscript:x">x</a>`, 'data: text/html': `<a href="data:text/html,<script>${c('L5')}</script>">x</a>`, 'data: SVG picture': `<img src="data:image/svg+xml,<svg onload='${c('L6')}'/>">` };
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  await p.goto('http://127.0.0.1:8304/'); await p.waitForFunction(() => typeof safeHtml == 'function', null, {timeout: 30000});
  const run = async h => p.evaluate(h => new Promise(r => { const out = safeHtml(h); const d = document.createElement('div'); d.innerHTML = out; document.body.appendChild(d);
    setTimeout(() => { const links = [...d.querySelectorAll('[href],[src],[action],[formaction],[data],[xlink\\:href]')].flatMap(e => [...e.attributes].filter(a => /href|src|action|data/.test(a.name)).map(a => a.value));
      const live = d.querySelectorAll('script').length + [...d.querySelectorAll('*')].filter(e => [...e.attributes].some(a => /^on/i.test(a.name))).length;
      d.remove(); r({out, hit: window.__hit.splice(0), links, live}) }, 400) }), h);
  const bad = [];
  for (const [n, h] of Object.entries(V)) { const r = await run(h); if (r.hit.length || r.live || r.links.some(v => /script:/i.test(v.replace(/[\s\u0000-\u001f]/g, '')))) bad.push(n + ': ' + r.out) }
  ok('1. ' + Object.keys(V).length + ' known tricks for running script: none runs, no handler or script left', !bad.length, bad);
  const badL = [];
  for (const [n, h] of Object.entries(LINKS)) { const r = await run(h); if (r.hit.length || r.links.some(v => /^\s*(java|vb)\s*script:|^data:(text|image\/svg)/i.test(v.replace(/[\s\u0000-\u001f]/g, '')))) badL.push(n + ': ' + r.out) }
  ok('2. links and sources that run script (javascript:, vbscript:, data: pages and SVG): removed', !badL.length, badL);
  const keep = await run(`<h1 style="color:#c00">Title</h1><p><b>bold</b> <i>it</i> <u>u</u> <a href="https://example.com/x">link</a> <a href="#n">anchor</a> <a href="mailto:a@b.c">mail</a></p><table class="wp-t" data-x="1"><tr><td colspan="2" style="background:#eee">cell</td></tr></table><img src="data:image/png;base64,iVBORw0KGgo=" alt="pic" width="20"><ol><li>one</li></ol><div class="wp-fnl" contenteditable="false">note</div>`);
  ok('3. what documents need is kept: headings, styles, bold, links, anchors, mail, tables, classes, data-, pictures, lists',
    /<h1 style="color:#c00">Title<\/h1>/.test(keep.out) && /href="https:\/\/example.com\/x"/.test(keep.out) && /href="#n"/.test(keep.out) && /mailto:/.test(keep.out) && /colspan="2"/.test(keep.out) && /class="wp-t"/.test(keep.out) && /data-x="1"/.test(keep.out) && /src="data:image\/png;base64/.test(keep.out) && /<li>one<\/li>/.test(keep.out) && /wp-fnl/.test(keep.out), keep.out);
  ok('4. plain text stays as it is', (await run('just some text & more')).out == 'just some text &amp; more');
  ok('5. the canary works (a check of the test)', await p.evaluate(() => new Promise(r => { const d = document.createElement('div'); d.innerHTML = `<img src=x onerror="top.__pwn('self')">`; document.body.appendChild(d); setTimeout(() => { d.remove(); r(window.__hit.splice(0).includes('self')) }, 300) })));
  // WordPro itself: a document with the tricks opened, and pasted
  await p.evaluate(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x'}];USER=ACCTS[0];return 1})()`);
  await p.evaluate(h => { node(['C:']).trick = {}; node(['C:', 'trick'])['T.DOC'] = h; fsSave(); wordpro(['C:', 'trick', 'T.DOC']) }, Object.values(V).join('') + Object.values(LINKS).join('')); await p.waitForTimeout(900);
  const wp = await p.evaluate(() => ({hit: window.__hit.splice(0), js: [...document.querySelectorAll('.win [href]')].filter(a => /javascript:/i.test(a.getAttribute('href'))).length}));
  ok('6. WordPro opening a document with all of them: nothing runs, no javascript: link', !wp.hit.length && !wp.js, wp);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
