#!/usr/bin/env node
/* Rudolf OS 1.6.3 (docs/plan-1.6.3.md Part 4): which process shows each of Oracle's tabs, for a tab's tooltip. rudolf-launch (built
   from iso/sys/rudolf-launch.c) with a real Chromium and a shell of its own (an Isolated Web App made here with a test key, mkiwa.py),
   whose page holds tabs as the shell does (a <controlledframe> each, in an account's persist: partition) on stand-in sites
   (127.0.0.1:8726 under made-up names):
     big.test/big#part   a page that holds some 150 MB (its address with a #part: matched without it)
     s.test/one, s.test/two   two pages of one site (Chromium may give them one process: said, not required)
     f.test/frame         a page with a frame of another site (ad.test/ad, which holds some 80 MB: its own process)
   Asked as Task Manager or a tooltip asks (the WANT file touched), rudolf-launch writes each frame's process and each process's
   Pss and RssAnon; checked against what the pages hold: the big page's process holds over 120 MB and more than the small ones';
   the other site's frame is in a process of its own under its page, and holds over 60 MB; the addresses are hashed, never written.
   Both ways rudolf-launch runs: the desktop's Chromium (OUTF, beside the pages) and Oracle's own (--oracle: a file of its own,
   gone when it exits). RUDOLF_TABMEM_DIR keeps the files in the test's folder.
     node oratabmem63_t.js [RUDOLF-LAUNCH]      (needs a display: xvfb-run; CHROME: Playwright's Chromium by default) */
const fs = require('fs'), os = require('os'), path = require('path'), http = require('http'), cp = require('child_process');
const REPO = path.resolve(__dirname, '../../..'), PORT = 8726;
const T = fs.mkdtempSync(path.join(/\/snap\//.test(process.env.CHROME || '') ? os.homedir() : os.tmpdir(), 'oratabmem63-'));
const wait = ms => new Promise(r => setTimeout(r, ms));
let fails = 0; const ok = (c, m, d) => { console.log((c ? 'PASS ' : 'FAIL ') + m + (!c && d !== undefined ? ' - ' + JSON.stringify(d).slice(0, 700) : '')); if (!c) fails++ };
const fnv = u => { u = u.split('#')[0]; let h = 0x811c9dc5; for (let i = 0; i < u.length; i++) { h ^= u.charCodeAt(i) & 0xff; h = Math.imul(h, 0x01000193) >>> 0 } return h };
const CHROME = process.env.CHROME || require('playwright').chromium.executablePath();
let LAUNCH = process.argv[2];
if (!LAUNCH) { LAUNCH = path.join(T, 'rudolf-launch'); cp.execFileSync('gcc', ['-O2', '-Wall', '-o', LAUNCH, path.join(REPO, 'iso/sys/rudolf-launch.c')]) }
const U = {big: `http://big.test:${PORT}/big#part`, one: `http://s.test:${PORT}/one`, two: `http://s.test:${PORT}/two`, frame: `http://f.test:${PORT}/frame`, ad: `http://ad.test:${PORT}/ad`};
const hold = mb => `<script>window.__k=new Uint8Array(${mb}*1048576);for(let i=0;i<__k.length;i+=4096)__k[i]=1+i%250;</script>`;
const PAGES = {'/big': '<title>Big</title>Big page' + hold(150), '/one': '<title>One</title>One', '/two': '<title>Two</title>Two',
  '/frame': `<title>Frame</title>A page with a frame<iframe src="${U.ad}" width=300 height=200></iframe>`, '/ad': '<title>Ad</title>Ad' + hold(80)};
(async () => {
  const srv = http.createServer((q, s) => { const b = PAGES[q.url.split('?')[0]]; s.writeHead(b ? 200 : 404, {'Content-Type': 'text/html'}); s.end(b || 'no') }).listen(PORT, '127.0.0.1');
  const app = path.join(T, 'app'); fs.mkdirSync(path.join(app, '.well-known'), {recursive: true});
  fs.copyFileSync(path.join(REPO, 'iso/shell/icon.png'), path.join(app, 'icon.png'));
  fs.writeFileSync(path.join(app, '.well-known/manifest.webmanifest'), JSON.stringify({name: 'Tab memory test', short_name: 'Tabs', version: '1.0.0',
    start_url: '/', display: 'standalone', icons: [{src: '/icon.png', sizes: '256x256', type: 'image/png'}],
    permissions_policy: {'controlled-frame': ['self'], 'cross-origin-isolated': ['self']}}));
  /* the tabs as the shell makes them: a controlledframe each in the account's partition, the last one shown */
  const urls = [U.big, U.one, U.two, U.frame];
  fs.writeFileSync(path.join(app, 'index.html'), `<!doctype html><meta charset="utf-8"><title>Tabs</title><style>controlledframe{position:absolute;inset:0;width:100%;height:100%;border:0}</style><body><script src="/tabs.js"></script></body>`);
  fs.writeFileSync(path.join(app, 'tabs.js'), `${JSON.stringify(urls)}.forEach((u,k,a)=>{const f=document.createElement('controlledframe');f.setAttribute('partition','persist:web-a1');f.style.visibility=k==a.length-1?'visible':'hidden';document.body.appendChild(f);f.src=u});`);   /* (an Isolated Web App runs no inline script) */
  const key = path.join(T, 'test.key'), swbn = path.join(T, 'app.swbn');
  const id = cp.execFileSync('python3', [path.join(REPO, 'iso/systools/mkiwa.py'), 'key', key]).toString().trim().split(/\s/)[0];
  cp.execFileSync('python3', [path.join(REPO, 'iso/systools/mkiwa.py'), 'build', app, swbn, key]);
  const env = Object.assign({}, process.env, {RUDOLF_TABMEM_DIR: T}, process.env.DBG ? {RUDOLF_LAUNCH_DEBUG: "1"} : {}); for (const k of ['http_proxy', 'https_proxy', 'HTTP_PROXY', 'HTTPS_PROXY']) delete env[k];
  const chromeArgs = prof => [CHROME, '--user-data-dir=' + path.join(T, prof), '--password-store=basic', '--enable-features=IsolatedWebApps,IsolatedWebAppDevMode',
    '--install-isolated-web-app-from-file=' + swbn, '--no-first-run', '--no-default-browser-check', `--host-resolver-rules=MAP *.test 127.0.0.1`]
    .concat(process.getuid && process.getuid() == 0 || !/\/snap\//.test(CHROME) ? ['--no-sandbox'] : []);
    /* (sandboxed with the Chromium snap, as on Rudolf OS: its own profile lets it; Playwright's Chromium runs without, as Playwright runs it,
       Ubuntu 24.04 refusing it the user namespaces its sandbox needs; and a root test machine) */
  const want = () => { const w = path.join(T, 'rudolf-tabmem.want'); fs.writeFileSync(w, '1'); const t = new Date(); fs.utimesSync(w, t, t) };
  /* the file's frames once a round begun after asked has been written (up to 60 s: the shell and its tabs start) */
  async function frames(file, since) {
    let j = null;
    for (let i = 0; i < 60; i++) { want(); await wait(1000);
      try { j = JSON.parse(fs.readFileSync(file, 'utf8')) } catch (e) { j = null }
      if (j && j.frames && j.ftime >= since && urls.every(u => j.frames.some(f => f.h == fnv(u) && f.m)) && j.frames.some(f => f.h == fnv(U.ad))) {
        await wait(3000); want(); await wait(3000); try { j = JSON.parse(fs.readFileSync(file, 'utf8')) } catch (e) {} return j }   /* (and once more, the pages settled) */
    }
    return j;
  }
  function check(mode, j, raw) {
    ok(!!(j && j.frames), `${mode}: rudolf-launch wrote each frame's process`, j);
    if (!j || !j.frames) return;
    const main = u => j.frames.find(f => f.h == fnv(u) && f.m), mem = p => (j.mem || {})[p] || [-1, -1];
    const B = main(U.big), O = main(U.one), W = main(U.two), F = main(U.frame), A = j.frames.find(f => f.h == fnv(U.ad));
    ok(B && O && W && F && A, `${mode}: every tab found by its address's hash (the big one with its #part)`, j.frames);
    if (!(B && O && W && F && A)) return;
    ok(!/test:8726/.test(raw), `${mode}: no address written in the clear`);
    const live = p => { try { return fs.readFileSync(`/proc/${p}/cmdline`, 'utf8').includes('--type=renderer') } catch (e) { return false } };
    ok([B, O, W, F, A].every(f => live(f.p)), `${mode}: each one's process is a live Chromium renderer`, [B, O, W, F, A].map(f => f.p));
    console.log(`FOUND ${mode} (${/\/snap\//.test(CHROME) ? 'sandboxed' : 'no sandbox'}): big ${B.p} (${mem(B.p)[0]} kB Pss, ${mem(B.p)[1]} kB RssAnon), one ${O.p}, two ${W.p}${O.p == W.p ? ' (one process for the site)' : ' (a process each)'}, frame ${F.p}, its other site's frame ${A.p} (${mem(A.p)[0]} kB)`);
    ok(mem(B.p)[0] > 120 * 1024 && mem(B.p)[0] > mem(O.p)[0] + 100 * 1024, `${mode}: the big page's process holds over 120 MB, more than a small page's`, [mem(B.p), mem(O.p)]);
    ok(mem(B.p)[1] > 120 * 1024, `${mode}: its RssAnon is read too`, mem(B.p));
    ok(A.up == F.f && !A.m && A.p != F.p, `${mode}: the frame of another site is under its page, in a process of its own`, [A, F]);
    ok(mem(A.p)[0] > 60 * 1024 && mem(F.p)[0] < mem(A.p)[0], `${mode}: and its process holds what that frame holds (over 60 MB)`, [mem(A.p), mem(F.p)]);
    ok(Object.values(j.mem).every(v => Array.isArray(v) && v.length == 2 && v.every(Number.isFinite)), `${mode}: each process's numbers are numbers`, j.mem);
  }
  // ---- the desktop's Chromium (the session's way)
  {
    const t0 = Math.floor(Date.now() / 1000);
    const p = cp.spawn(LAUNCH, [id, ...chromeArgs('kiosk')], {env, stdio: ['ignore', 'ignore', 'pipe']}); let err = ''; p.stderr.on('data', d => err += d);
    const f = path.join(T, 'rudolf-tabmem.json'); const j = await frames(f, t0);
    check('the desktop\'s Chromium', j, fs.existsSync(f) ? fs.readFileSync(f, 'utf8').replace(/"url":"[^"]*"/g, '') : '');
    ok(j && Array.isArray(j.pages), 'the desktop\'s Chromium: the pages\' JavaScript memory (Task Manager) is still written beside them', j && j.pages);
    p.kill('SIGTERM'); await new Promise(r => { p.on('exit', r); setTimeout(r, 15000) });
    if (fails) { if (process.env.DBG) fs.writeFileSync('/tmp/claude-0/dbg-' + 'kiosk' + '.log', err); console.log(err.split('\n').filter(l => !/^\[|dbus|Fontconfig/i.test(l)).slice(-15).join('\n')) }
  }
  // ---- Oracle's own Chromium (--oracle: Native, the encrypted setting, the Tor windows)
  {
    const ctl = path.join(T, 'ctl'); cp.execFileSync('mkfifo', [ctl]);
    const t0 = Math.floor(Date.now() / 1000);
    const p = cp.spawn(LAUNCH, ['--oracle', ctl, id, ...chromeArgs('oracle')], {env, stdio: ['ignore', 'pipe', 'pipe']}); let err = ''; p.stderr.on('data', d => err += d); p.stdout.on('data', () => {});
    await wait(2500); let fd = -1; try { fd = fs.openSync(ctl, fs.constants.O_WRONLY | fs.constants.O_NONBLOCK); fs.writeSync(fd, 'open 1000 700 0 0 app=browser\n'); fs.closeSync(fd) } catch (e) { ok(false, 'Oracle\'s own Chromium: rudolf-launch is running (its control FIFO open)', String(e)) }
    const f = path.join(T, `rudolf-tabmem-o${p.pid}.json`); const j = await frames(f, t0);
    check('Oracle\'s own Chromium', j, fs.existsSync(f) ? fs.readFileSync(f, 'utf8') : '');
    p.kill('SIGTERM'); await new Promise(r => { p.on('exit', r); setTimeout(r, 15000) }); await wait(500);
    ok(!fs.existsSync(f), 'Oracle\'s own Chromium: its file is gone when it exits');
    if (fails) { if (process.env.DBG) fs.writeFileSync('/tmp/claude-0/dbg-' + 'oracle' + '.log', err); console.log(err.split('\n').filter(l => !/^\[|dbus|Fontconfig/i.test(l)).slice(-15).join('\n')) }
  }
  srv.close(); fs.rmSync(T, {recursive: true, force: true});
  console.log(fails ? `FAIL ${fails}` : 'PASS'); process.exit(fails ? 1 : 0);
})().catch(e => { console.log('FAIL ' + (e.stack || e)); try { fs.rmSync(T, {recursive: true, force: true}) } catch (x) {} process.exit(1) });
