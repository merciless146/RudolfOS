// 1.6.2 (docs/plan-1.6.2.md Part 14; the owner's Vivobook, 2026-11-22: "Refused to execute https://cdnjs.cloudflare.com/.../fengari-web.js
// as script because X-Content-Type-Options: nosniff"): no script from another site loads into the desktop's page or an app window. The page
// holds rudolfd's token, so such a script would have the whole system API. Lua (the Prompt, BlockLua Builder, Blitzen's sandbox) runs on the
// page's own copy (lib/fengari-web.js); Ruffle plays in its own sandboxed frame (lib/ruffle-frame.html); the page's Content-Security-Policy
// refuses any other site's script. Every request to another site is refused here and noted.   node noremote162_t.js [CHROME] [PORT]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path'), cp = require('child_process');
const WWW = path.join(__dirname, '../../../iso/www'), PORT = +(process.argv[3] || 8621), BASE = 'http://127.0.0.1:' + PORT;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 600) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.mjs': 'text/javascript', '.html': 'text/html', '.css': 'text/css', '.json': 'application/json', '.webp': 'image/webp', '.png': 'image/png', '.woff2': 'font/woff2', '.woff': 'font/woff', '.txt': 'text/plain'};
const CSP = "object-src 'none'; base-uri 'self'; script-src 'self' 'unsafe-inline' 'unsafe-eval' blob:; worker-src 'self' blob:";   // (rudolfd's, serve_index)
// a stand-in for a copy of Ruffle on this computer (as host.sh's): a player that says what it was given
const RUFFLE = "window.RufflePlayer={newest(){return{createPlayer(){const d=document.createElement('div');d.className='ruf';d.load=o=>{d.textContent='played '+(o.data?o.data.byteLength+' bytes':o.url);return Promise.resolve()};return d}}}};";
const got = [];
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]); got.push(u);
  if (u == '/lib/ruffle/ruffle.js') { s.writeHead(200, {'content-type': 'text/javascript', 'x-content-type-options': 'nosniff'}); return s.end(RUFFLE) }
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  const h = {'content-type': T[path.extname(f)] || 'application/octet-stream', 'x-content-type-options': 'nosniff'}; if (u == '/') h['content-security-policy'] = CSP;
  s.writeHead(200, h); fs.createReadStream(f).pipe(s) }).listen(PORT, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const nr = cp.spawnSync('python3', [path.join(__dirname, '../../systools/noremote.py')], {encoding: 'utf8'});
  ok('1. noremote.py: no script of another site in the sources or the built pages (index.html, a/*.html, a/*.js)', nr.status == 0, nr.stdout.slice(-600));
  const pages = ['index.html', ...fs.readdirSync(path.join(WWW, 'a')).filter(f => f.endsWith('.html')).map(f => 'a/' + f)];
  ok('2. every built page has the policy (' + pages.length + ' pages)', pages.every(f => fs.readFileSync(path.join(WWW, f), 'utf8').slice(0, 20000).includes('<meta http-equiv="Content-Security-Policy" content="' + CSP + '">')), pages);
  ok('3. Fengari ships with its licence (MIT)', fs.existsSync(path.join(WWW, 'lib/fengari-web.js')) && /MIT|Permission is hereby granted/.test(fs.readFileSync(path.join(WWW, 'lib/fengari-LICENSE.txt'), 'utf8')));
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const ctx = await b.newContext({viewport: {width: 1366, height: 768}});
  const outside = [], viol = [];
  await ctx.route('**/*', r => { const u = r.request().url(); if (/^(https?:\/\/127\.0\.0\.1:|data:|blob:|about:)/.test(u)) return r.continue(); outside.push(r.request().resourceType() + ' ' + u); return r.abort() });
  const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message));
  p.on('console', m => { if (/Content Security Policy|Refused to (load|execute)/i.test(m.text())) viol.push(m.text()) });
  await p.goto(BASE + '/'); await p.waitForFunction(() => typeof runLua == 'function' && typeof rufFrame == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});return 1})()`); await wait(800);
  ok('4. Fengari is not loaded at start (no script tag of another site in the page)', await q("typeof fengari=='undefined'&&![...document.scripts].some(s=>/^https?:/.test(s.src)&&!s.src.startsWith(location.origin))"));
  const lua = await q("(async()=>{let o='';await runLua('print(\"hi \"..(6*7))',{write:s=>o+=s,print:s=>o+=s+'\\n'},'T');return o})()");
  ok('5. Lua (the Prompt\'s runLua) runs on the page\'s own copy', lua.trim() == 'hi 42' && got.includes('/lib/fengari-web.js'), lua);
  await q("(()=>{blocklua();return 1})()"); await wait(800); await q("(()=>{const w=[...document.querySelectorAll('.win')].find(w=>/BlockLua/.test(w.querySelector('.t').textContent));w.querySelector('.body .r').click();return 1})()"); await wait(2500);
  const blk = await q("[...document.querySelectorAll('.win')].map(w=>w.textContent).join(' | ')");
  ok('6. BlockLua Builder\'s Run runs its program (no "could not be loaded")', !/could not be loaded|Lua error/.test(blk) && await q("[...document.querySelectorAll('.win .t')].some(t=>/Prompt/.test(t.textContent))"), blk.slice(0, 400));
  const bx = await q("(async()=>{const r=await blzRunLuaBox('print(\"box \"..(2+3))',{auto:1});return JSON.stringify(r)})()");
  ok('7. Blitzen\'s Lua (its sandbox) runs on the page\'s own copy', /box 5/.test(bx), bx);
  // the policy: a script of another site is refused, and nothing asks for it
  const n0 = outside.length;
  const csp = await q(`new Promise(r=>{let v=null;addEventListener('securitypolicyviolation',e=>{v=e.blockedURI+' '+e.violatedDirective},{once:true});const s=document.createElement('script');s.src='https://cdn.jsdelivr.net/npm/fengari-web@0.1.4/dist/fengari-web.js';s.onload=()=>r('loaded');s.onerror=()=>r('refused: '+v);document.head.appendChild(s);setTimeout(()=>r('none'),4000)})`);
  ok('8. the page refuses a script from another site (its policy, script-src)', /^refused: https:\/\/cdn\.jsdelivr\.net.* script-src/.test(csp) && !outside.slice(n0).some(x => /^script/.test(x)), csp);
  const imp = await q("import('https://unpkg.com/x.mjs').then(()=>'loaded',e=>'refused')");
  ok('9. ...and an import() from another site', imp == 'refused' && !outside.slice(n0).some(x => /^script/.test(x)), imp);
  const wk = await q("new Promise(r=>{try{const w=new Worker(URL.createObjectURL(new Blob([\"try{importScripts('https://unpkg.com/x.js');postMessage('loaded')}catch(e){postMessage('refused')}\"])));w.onmessage=e=>r(e.data);setTimeout(()=>r('none'),4000)}catch(e){r('no worker: '+e)}})");
  ok('10. ...and a worker\'s importScripts() from another site', wk == 'refused' && !outside.slice(n0).some(x => /^script/.test(x)), wk);
  // Ruffle: in its own sandboxed frame, never in this page; given the file
  await q(`(()=>{MMC.set.rufUrl='${BASE}/lib/ruffle/ruffle.js';MMC.plug.ruffle=Object.assign(MMC.plug.ruffle||{},{inst:1,en:1});window.__FW=flashPlayer();return !!__FW})()`);
  const swf = fs.readFileSync(path.join(__dirname, 'blue.swf'));
  const fl = await q(`(async()=>{const u=new Uint8Array([${[...swf].join(',')}]);const r=await __FW.loadSWF(u,'BLUE.SWF');return r})()`);
  await wait(500);
  const ff = p.frames().find(f => /\/lib\/ruffle-frame\.html$/.test(f.url()));
  const inner = ff ? await ff.evaluate("JSON.stringify({o:origin,t:(document.querySelector('.ruf')||{}).textContent,ls:(()=>{try{localStorage.length;return 'yes'}catch(e){return 'no'}})(),tok:(()=>{try{return typeof parent.RUDOLFD}catch(e){return 'blocked'}})()})") : null;
  const fa = await q("(()=>{const f=__FW.body.querySelector('iframe');return f?f.getAttribute('sandbox'):null})()");
  ok('11. the Flash Player plays in Ruffle\'s own frame: a sandbox (allow-scripts only), no origin, no storage, no reach into the page', fl === true && fa == 'allow-scripts' && inner && JSON.parse(inner).o == 'null' && JSON.parse(inner).ls == 'no' && JSON.parse(inner).tok == 'blocked', {fl, fa, inner});
  ok('12. ...given the file (' + swf.length + ' bytes); Ruffle never ran in the desktop\'s page', inner && JSON.parse(inner).t == 'played ' + swf.length + ' bytes' && await q("typeof RufflePlayer=='undefined'"), inner);
  // Oracle's own Flash page (a page Oracle draws, so with the desktop's policy): Ruffle in a frame inside it
  await q(`(()=>{const r=rufPage('blue.swf - Flash',{url:'${BASE}/blue.swf'});const f=document.createElement('iframe');f.id='orf';f.setAttribute('sandbox','allow-scripts');f.style.cssText='width:300px;height:200px';f.srcdoc=r.html;document.body.appendChild(f);return 1})()`);
  await wait(2000);
  const of = p.frames().filter(f => /\/lib\/ruffle-frame\.html$/.test(f.url())).pop();
  const ot = of && of !== ff ? await of.evaluate("(document.querySelector('.ruf')||{}).textContent||''") : null;
  ok('13. Oracle\'s own Flash page plays through Ruffle\'s frame (it cannot load Ruffle itself under the policy)', ot == 'played ' + BASE + '/blue.swf', ot);
  const chk = await q("rufLoad().then(()=>'ok',e=>'err '+e.message)");
  ok('14. Settings › Multimedia\'s check loads Ruffle in a hidden frame, and removes it', chk == 'ok' && await q("![...document.querySelectorAll('body>iframe')].some(f=>/ruffle-frame/.test(f.src))"), chk);
  ok('15. no script or worker of another site was asked for (the network check\'s generate_204 is not a script), and the policy refused nothing the page needs', !outside.some(x => /^(script|worker|sharedworker)/.test(x)) && viol.every(v => /cdn\.jsdelivr\.net|unpkg\.com/.test(v)), {outside, viol});
  ok('16. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.log('FAIL ran - ' + e.stack); process.exit(1) });
