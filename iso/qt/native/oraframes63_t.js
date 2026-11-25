#!/usr/bin/env node
/* Rudolf OS 1.6.3 (docs/plan-1.6.3.md Part 7): what Chromium's own password manager and extensions do in Oracle's tabs.
   Oracle's tabs are Controlled Frames (<controlledframe>) of the shell, an Isolated Web App (iso/shell), in every Chromium
   mode: the Chromium desktop's own Chromium, and Oracle's own Chromium on Native and with "Keep my websites encrypted with
   my account" (rudolf-native.c oracle_open, rudolfd's orawin: the same shell). This test asks a real Chromium, side by side:
   - a plain Chromium tab (Chromium's own window): the password manager reads the sign-in form (its log in
     chrome://password-manager-internals names the site) and an extension's content script runs;
   - a Controlled Frame of an Isolated Web App, with the same profile, the same site and the same extension: what then?
   It passes when Chromium behaves as Rudolf OS's plan now says (in a Controlled Frame neither runs), and fails with a plain
   message the day a Chromium release changes that, so Part 7 can be looked at again.
   A small Isolated Web App of its own (mkiwa.py, a test key made here; nothing of the real shell's key), a stand-in sign-in
   page on 127.0.0.1:8723, a stand-in extension. Needs a display (DISPLAY: xvfb-run) and Playwright; CHROME is the Chromium
   (Playwright's by default; the snap's /snap/bin/chromium on the runner, the one the releases ship).
     node oraframes63_t.js            (prints FOUND lines, then PASS or FAIL) */
const fs = require('fs'), os = require('os'), path = require('path'), http = require('http'), {execFileSync} = require('child_process');
const {chromium} = require('playwright');
const REPO = path.resolve(__dirname, '../../..'), PORT = 8723;
const CHROME = process.env.CHROME || '';
/* (the snap sees the runner's home folder, not its /tmp) */
const T = fs.mkdtempSync(path.join(/\/snap\//.test(CHROME) ? os.homedir() : os.tmpdir(), 'oraframes63-'));
let fails = 0; const ok = (c, m) => { console.log((c ? 'ok   ' : 'FAIL ') + m); if (!c) fails++ };
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  // the Isolated Web App: one Controlled Frame, an account's partition as the shell's tabs have
  const app = path.join(T, 'app'); fs.mkdirSync(path.join(app, '.well-known'), {recursive: true});
  fs.copyFileSync(path.join(REPO, 'iso/shell/icon.png'), path.join(app, 'icon.png'));
  fs.writeFileSync(path.join(app, '.well-known/manifest.webmanifest'), JSON.stringify({name: 'Frames test', short_name: 'Frames', version: '1.0.0',
    start_url: '/', display: 'standalone', icons: [{src: '/icon.png', sizes: '256x256', type: 'image/png'}],
    permissions_policy: {'controlled-frame': ['self'], 'cross-origin-isolated': ['self']}}));
  fs.writeFileSync(path.join(app, 'index.html'), `<!doctype html><meta charset="utf-8"><title>Frames test</title>
<controlledframe id="f" partition="persist:acct1" src="http://127.0.0.1:${PORT}/login.html" style="width:800px;height:500px"></controlledframe>`);
  const key = path.join(T, 'test.key'), swbn = path.join(T, 'app.swbn');
  const id = execFileSync('python3', [path.join(REPO, 'iso/systools/mkiwa.py'), 'key', key]).toString().trim().split(/\s/)[0];
  execFileSync('python3', [path.join(REPO, 'iso/systools/mkiwa.py'), 'build', app, swbn, key]);
  // the stand-in extension: a content script in every frame of every page
  const ext = path.join(T, 'ext'); fs.mkdirSync(ext);
  fs.writeFileSync(path.join(ext, 'manifest.json'), JSON.stringify({manifest_version: 3, name: 'Stand-in', version: '1.0',
    content_scripts: [{matches: ['<all_urls>'], js: ['cs.js'], all_frames: true, run_at: 'document_start'}]}));
  fs.writeFileSync(path.join(ext, 'cs.js'), "document.documentElement.setAttribute('data-standin','ran')");
  // the sign-in page
  const srv = http.createServer((q, s) => {
    if (q.method == 'POST') { q.resume(); q.on('end', () => { s.writeHead(303, {Location: '/done.html'}); s.end() }); return }
    s.writeHead(200, {'Content-Type': 'text/html'});
    s.end(q.url.startsWith('/done') ? '<!doctype html><title>Signed in</title>Signed in' :
      '<!doctype html><title>Sign in</title><form action="/signin" method="post"><input name="user" id="user" autocomplete="username"><input type="password" name="pw" id="pw" autocomplete="current-password"><button id="go">Sign in</button></form>');
  }).listen(PORT, '127.0.0.1');
  const args = ['--password-store=basic', '--enable-features=IsolatedWebApps,IsolatedWebAppDevMode', '--install-isolated-web-app-from-file=' + swbn,
    '--disable-extensions-except=' + ext, '--load-extension=' + ext];
  if (process.getuid && process.getuid() == 0) args.push('--no-sandbox');   /* (a root test machine only: the runner is not root) */
  const ctx = await chromium.launchPersistentContext(path.join(T, 'profile'), Object.assign({headless: false, args}, CHROME ? {executablePath: CHROME} : {}));
  const ver = ctx.browser() ? ctx.browser().version() : '';
  const internals = await ctx.newPage(); await internals.goto('chrome://password-manager-internals');
  const pmLines = async () => (await internals.evaluate(() => document.body.innerText)).split('\n').filter(l => l.includes('127.0.0.1:' + PORT)).length;
  const signIn = async ev => { await ev("document.getElementById('user').value='alice';document.getElementById('pw').value='made-up-test-only';document.getElementById('go').click()"); await wait(3000) };
  // 1. a plain tab
  const tab = await ctx.newPage(); await tab.goto(`http://127.0.0.1:${PORT}/login.html`); await wait(1500);
  const tabExt = await tab.evaluate(() => document.documentElement.getAttribute('data-standin'));
  await signIn(c => tab.evaluate(c => eval(c), c));
  const tabPm = await pmLines();
  console.log(`FOUND Chromium ${ver}, a plain tab: the extension's content script ${tabExt == 'ran' ? 'ran' : 'did not run'}; the password manager's log names the site ${tabPm} times`);
  ok(tabExt == 'ran', 'a plain tab: the stand-in extension runs (the test itself works)');
  ok(tabPm > 0, 'a plain tab: Chromium\'s password manager reads the sign-in form (the test itself works)');
  await tab.close(); await internals.reload(); await wait(1000);
  // 2. the Controlled Frame
  let iwa; for (let i = 0; i < 20 && !iwa; i++) {
    const s = await ctx.newCDPSession(internals); try { await s.send('Target.createTarget', {url: 'isolated-app://' + id + '/'}) } catch (e) {}
    await wait(1500); iwa = ctx.pages().find(p => p.url().startsWith('isolated-app://'));
  }
  ok(!!iwa, 'the Isolated Web App opens (Controlled Frames are available)');
  if (iwa) {
    await wait(3000);
    const ex = c => iwa.evaluate(c => document.getElementById('f').executeScript({code: c}), c);
    const frExt = (await ex("document.documentElement.getAttribute('data-standin')"))[0];
    const form = (await ex("!!document.getElementById('pw')"))[0];
    ok(form, 'the Controlled Frame shows the sign-in page');
    await signIn(ex);
    const frPm = await pmLines();
    console.log(`FOUND Chromium ${ver}, a Controlled Frame: the extension's content script ${frExt == 'ran' ? 'ran' : 'did not run'}; the password manager's log names the site ${frPm} times`);
    ok(frExt != 'ran', 'a Controlled Frame: no extension\'s content script runs (if this fails, Chromium now runs extensions in Controlled Frames: look at Part 7 again)');
    ok(frPm == 0, 'a Controlled Frame: Chromium\'s password manager does not see the form (if this fails, Chromium now offers it in Controlled Frames: look at Part 7 again)');
  }
  await ctx.close(); srv.close();
  fs.rmSync(T, {recursive: true, force: true});
  console.log(fails ? `FAIL ${fails}` : 'PASS');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.log('FAIL ' + (e.stack || e)); try { fs.rmSync(T, {recursive: true, force: true}) } catch (x) {} process.exit(1) });
