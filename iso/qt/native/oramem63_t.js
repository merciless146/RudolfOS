#!/usr/bin/env node
/* Rudolf OS 1.6.3 (docs/plan-1.6.3.md Part 4): Oracle's memory with 1, 3 and 6 tabs, and what freeing its background tabs saves.
   Oracle's tabs are Controlled Frames of the shell (iso/shell) in every Chromium mode, one Chromium process per site (site
   isolation, kept). This asks a real Chromium: an Isolated Web App of its own (mkiwa.py, a test key made here) whose page
   holds the tabs as the shell does (a <controlledframe> each, in an account's persist: partition), each tab a website of its
   own; the memory is the proportional set size (Pss, /proc/PID/smaps_rollup) of every process of that Chromium, after the
   pages have settled.
     before: 1, 3 and 6 tabs, every one alive (as Oracle keeps them today, also when they have not been looked at for hours)
     after:  the same 3 and 6 tabs with the background ones freed as desktop/orasleep63.js frees them (the shell's "close":
             the frame removed, its address kept by Oracle), and a freed tab chosen again (loaded at its address)
   SITES=real: real websites (the runner); otherwise stand-in sites on 127.0.0.1:8724 under made-up names (a.test, b.test,
   ..., each its own site and so its own process), each page with some text, pictures and script as a news page has.
   Needs a display (xvfb-run) and Playwright; CHROME is the Chromium (Playwright's by default; /snap/bin/chromium on the runner).
   Prints the readings (MB) and PASS when a freed background tab saves memory; FAIL otherwise. */
const fs = require('fs'), os = require('os'), path = require('path'), http = require('http'), {execFileSync} = require('child_process');
const {chromium} = require('playwright');
const REPO = path.resolve(__dirname, '../../..'), PORT = 8724, CHROME = process.env.CHROME || '';
const REAL = process.env.SITES == 'real';
const SITES = REAL ? ['https://en.wikipedia.org/wiki/Reindeer', 'https://github.com/torvalds/linux', 'https://www.bbc.com/news',
  'https://www.theguardian.com/international', 'https://developer.mozilla.org/en-US/docs/Web/JavaScript', 'https://www.youtube.com/']
  : ['a', 'b', 'c', 'd', 'e', 'f'].map(x => `http://${x}.test:${PORT}/`);
const T = fs.mkdtempSync(path.join(/\/snap\//.test(CHROME) ? os.homedir() : os.tmpdir(), 'oramem63-'));
const wait = ms => new Promise(r => setTimeout(r, ms));
let fails = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) fails++ };
const MB = k => (k / 1024).toFixed(1);
/* the processes of this Chromium: the one whose command line names its profile folder and every process under it */
function pss() {
  const prof = path.join(T, 'profile'), kids = {}, cl = {};
  for (const d of fs.readdirSync('/proc')) { if (!/^\d+$/.test(d)) continue;
    try { cl[d] = fs.readFileSync(`/proc/${d}/cmdline`, 'utf8'); const st = fs.readFileSync(`/proc/${d}/stat`, 'utf8'); const pp = st.slice(st.lastIndexOf(')') + 2).split(' ')[1]; (kids[pp] = kids[pp] || []).push(d) } catch (e) {} }
  const top = Object.keys(cl).filter(d => cl[d].includes('--user-data-dir=' + prof) && !cl[d].includes('--type='));
  const all = new Set(), st = [...top]; while (st.length) { const d = st.pop(); if (all.has(d)) continue; all.add(d); st.push(...(kids[d] || [])) }
  let tot = 0, n = 0;
  for (const d of all) { if (/crashpad_handler/.test((cl[d] || '').split('\0')[0])) continue;
    try { const m = /^Pss:\s+(\d+) kB/m.exec(fs.readFileSync(`/proc/${d}/smaps_rollup`, 'utf8')); if (m) { tot += +m[1]; n++ } } catch (e) {} }
  return {kb: tot, n};
}
/* a stand-in news page: text, a few pictures (canvas-made, so nothing is fetched), a script holding some data and a timer */
const page = h => `<!doctype html><meta charset="utf-8"><title>${h}</title><h1>${h}</h1>` +
  Array.from({length: 60}, (_, i) => `<p>Paragraph ${i} of ${h}: ` + 'Rudolf reads the news. '.repeat(30) + '</p>').join('') +
  `<div id="pics"></div><script>const d=[];for(let i=0;i<200000;i++)d.push({i,s:'item '+i});
for(let k=0;k<8;k++){const c=document.createElement('canvas');c.width=800;c.height=450;const x=c.getContext('2d');for(let j=0;j<400;j++){x.fillStyle='hsl('+(j*7%360)+',60%,50%)';x.fillRect(Math.random()*800,Math.random()*450,40,40)}
const im=new Image();im.src=c.toDataURL();im.width=400;document.getElementById('pics').appendChild(im)}
setInterval(()=>{d[Math.floor(Math.random()*d.length)].s+='.'},1000);</script>`;
(async () => {
  const srv = REAL ? null : http.createServer((q, s) => { s.writeHead(200, {'Content-Type': 'text/html'}); s.end(page(String(q.headers.host || '').split(':')[0])) }).listen(PORT, '127.0.0.1');
  const app = path.join(T, 'app'); fs.mkdirSync(path.join(app, '.well-known'), {recursive: true});
  fs.copyFileSync(path.join(REPO, 'iso/shell/icon.png'), path.join(app, 'icon.png'));
  fs.writeFileSync(path.join(app, '.well-known/manifest.webmanifest'), JSON.stringify({name: 'Memory test', short_name: 'Memory', version: '1.0.0',
    start_url: '/', display: 'standalone', icons: [{src: '/icon.png', sizes: '256x256', type: 'image/png'}],
    permissions_policy: {'controlled-frame': ['self'], 'cross-origin-isolated': ['self']}}));
  /* the tabs as the shell makes them (makeTab): a controlledframe in the account's partition, the shown one visible */
  fs.writeFileSync(path.join(app, 'index.html'), `<!doctype html><meta charset="utf-8"><title>Memory test</title><style>controlledframe{position:absolute;inset:0;width:100%;height:100%;border:0}</style><body></body>`);
  const key = path.join(T, 'test.key'), swbn = path.join(T, 'app.swbn');
  const id = execFileSync('python3', [path.join(REPO, 'iso/systools/mkiwa.py'), 'key', key]).toString().trim().split(/\s/)[0];
  execFileSync('python3', [path.join(REPO, 'iso/systools/mkiwa.py'), 'build', app, swbn, key]);
  const args = ['--password-store=basic', '--enable-features=IsolatedWebApps,IsolatedWebAppDevMode', '--install-isolated-web-app-from-file=' + swbn,
    '--disk-cache-size=67108864', '--no-first-run', '--no-default-browser-check'];
  if (!REAL) args.push(`--host-resolver-rules=MAP *.test 127.0.0.1`);
  if (process.getuid && process.getuid() == 0) args.push('--no-sandbox');   /* (a root test machine only: the runner is not root) */
  const ctx = await chromium.launchPersistentContext(path.join(T, 'profile'), Object.assign({headless: false, viewport: {width: 1280, height: 800}, args}, CHROME ? {executablePath: CHROME} : {}));
  const ver = ctx.browser() ? ctx.browser().version() : '';
  const first = ctx.pages()[0] || await ctx.newPage();
  let iwa; for (let i = 0; i < 20 && !iwa; i++) {
    const s = await ctx.newCDPSession(first); try { await s.send('Target.createTarget', {url: 'isolated-app://' + id + '/'}) } catch (e) {}
    await wait(1500); iwa = ctx.pages().find(p => p.url().startsWith('isolated-app://'));
  }
  ok(!!iwa, 'the Isolated Web App opens');
  if (!iwa) process.exit(1);
  await first.close().catch(() => {});
  const q = (f, a) => iwa.evaluate(f, a);
  /* open tab k (its site), shown or in the background; wait for it to load */
  const open = (k, shown) => q(([u, k, shown]) => new Promise(res => {
    const f = document.createElement('controlledframe'); f.id = 't' + k; f.setAttribute('partition', 'persist:web-acct1');
    f.style.visibility = shown ? 'visible' : 'hidden'; document.body.appendChild(f);
    let done = false; const end = () => { if (!done) { done = true; res(true) } };
    f.addEventListener('loadstop', end); setTimeout(end, 30000); f.src = u;
  }), [SITES[k], k, shown]);
  const show = k => q(k => { for (const f of document.querySelectorAll('controlledframe')) f.style.visibility = f.id == 't' + k ? 'visible' : 'hidden' }, k);
  const free = k => q(k => { const f = document.getElementById('t' + k); if (f) f.remove() }, k);
  const closeAll = () => q(() => { for (const f of document.querySelectorAll('controlledframe')) f.remove() });
  const settle = async () => { await wait(REAL ? 25000 : 12000); return pss() };
  await wait(3000); const base = await settle();
  console.log(`FOUND Chromium ${ver}, ${REAL ? 'real websites' : 'stand-in sites'}; no tab: ${MB(base.kb)} MB in ${base.n} processes`);
  const R = {};
  for (const n of [1, 3, 6]) {
    for (let k = 0; k < n; k++) await open(k, k == n - 1);
    const all = await settle(); R[n] = {all};
    console.log(`FOUND ${n} tab${n > 1 ? 's' : ''}, every one alive: ${MB(all.kb)} MB in ${all.n} processes (${MB(all.kb - base.kb)} MB more than no tab)`);
    if (n > 1) {
      for (let k = 0; k < n - 1; k++) await free(k);
      const fr = await settle(); R[n].freed = fr;
      const per = (all.kb - fr.kb) / (n - 1);
      console.log(`FOUND ${n} tabs, the ${n - 1} background ones freed: ${MB(fr.kb)} MB in ${fr.n} processes (${MB(all.kb - fr.kb)} MB less: ${MB(per)} MB a background tab)`);
      ok(per > 10 * 1024, `${n} tabs: each freed background tab saves more than 10 MB (${MB(per)} MB)`);
      // a freed tab chosen again: loaded at its address
      await open(0, true); await show(0);
      const back = await q(() => new Promise(res => { const f = document.getElementById('t0'); f.executeScript({code: 'location.href+" "+document.title'}).then(r => res(r[0])).catch(e => res('error ' + e)) }));
      ok(back && back.startsWith(SITES[0].replace(/\/$/, '')), `${n} tabs: a freed tab chosen again loads at its address (${String(back).slice(0, 80)})`);
    }
    await closeAll(); await wait(4000);
  }
  await ctx.close(); if (srv) srv.close();
  fs.rmSync(T, {recursive: true, force: true});
  console.log(JSON.stringify({chromium: ver, sites: REAL ? 'real' : 'stand-in', base: base.kb, r: Object.fromEntries(Object.entries(R).map(([n, v]) => [n, {all: v.all.kb, freed: v.freed ? v.freed.kb : null}]))}));
  console.log(fails ? `FAIL ${fails}` : 'PASS');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.log('FAIL ' + (e.stack || e)); try { fs.rmSync(T, {recursive: true, force: true}) } catch (x) {} process.exit(1) });
