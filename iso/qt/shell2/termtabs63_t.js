// 1.6.3 (the owner, 2026-11-28): the Command Prompt and the Linux terminal are one app, and the way between them is the
// tab bar's ＋ (Ctrl+Shift+T), the Start menu's Terminal/Command Prompt, Ctrl+Alt+T and prompt() -- a .BAT, a Lua program,
// rpk. Two things were broken: a window kept the "no Linux" answer it was born with (at sign-in the encrypted folder may
// not be open yet, the status call fails then, and the ＋ gave another Command Prompt tab for the window's whole life,
// with nothing said -- and with no Debian set up there was nothing ever offered), and every way in opened a second
// Terminal window, so Ctrl+Alt+T and the taskbar could reach the one whose tabs were somewhere else.
// node termtabs63_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../..', 'iso/www'), PORT = 8763;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 900) : '')) };
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }
  const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200); fs.createReadStream(f).pipe(s) }).listen(PORT, '127.0.0.1');
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const p = await (await b.newContext({viewport: {width: 1280, height: 800}})).newPage(); const errs = [];
  p.on('pageerror', e => errs.push(String(e.message).slice(0, 200)));
  // the desktop with a daemon behind it (a browser alone has none, and then Linux cannot be offered at all)
  await p.addInitScript(() => { window.RUDOLFD = {token: 'test-token'} });
  await p.route('**/api/**', r => r.fulfill({status: 200, contentType: 'application/json', body: '{"ok":true}'}));
  await p.goto(`http://127.0.0.1:${PORT}/`);
  await p.waitForFunction(() => typeof terminalApp == 'function' && typeof prompt == 'function', null, {timeout: 30000});
  // rudolfd's Debian and terminal answers, given by the test; `st` is what debian/status says, `fails` the calls that fail
  await p.evaluate(() => {
    try { svj('bd95setup', 1) } catch (e) {} ['oobe', 'login'].forEach(i => { const o = document.getElementById(i); o && o.remove() }); USER = USER || ACCTS[0];
    window.__st = {installed: true, persist: true, version: '13', size: 0, free: 0};
    window.__fails = 0; window.__calls = 0;
    window.lxApi = async (u) => {
      if (u.startsWith('debian/status')) { window.__calls++; if (window.__fails > 0) { window.__fails--; throw new Error('rudolfd is not ready') } return {...window.__st} }
      if (u.startsWith('debian/apps')) return {apps: []};
      if (u.startsWith('term/open')) return {id: 't1'};
      if (u.startsWith('term/read')) { await new Promise(r => setTimeout(r, 150)); return {data: null, off: 0} }
      if (u.startsWith('term/')) return {ok: true};
      return {ok: true};
    };
  });
  const wait = ms => p.waitForTimeout(ms);
  const state = () => p.evaluate(() => { const W = [...document.querySelectorAll('.win')].filter(x => x.querySelector('.dterm')).pop();
    if (!W) return null; const b = W.querySelector('.body');
    return { wins: document.querySelectorAll('.win').length, termWins: document.querySelectorAll('.dterm').length,
      tabs: [...b.querySelectorAll('.dttab')].map(x => x.textContent),
      shown: [...b.querySelectorAll('.dtbox')].filter(x => getComputedStyle(x).display != 'none').map(x => x.className),
      keys: getComputedStyle(b.querySelector('.dtkeys')).display,
      focus: document.activeElement && document.activeElement.closest('.dtbox') ? document.activeElement.closest('.dtbox').className : 'none' } });
  const menu = async () => { await p.evaluate(() => document.querySelector('.dterm .dtnew').click()); await wait(900);
    return p.evaluate(() => [...document.querySelectorAll('.cmenu .cmi')].map(d => ({t: d.textContent, off: d.classList.contains('dis')}))) };
  const choose = async row => { await p.evaluate(i => document.querySelectorAll('.cmenu .cmi')[i].click(), row); await wait(1000) };

  // ---- 1. Linux set up: the Terminal opens on Linux, and one window holds both shells
  await p.evaluate(() => { terminalApp() }); await wait(1200);
  let s = await state();
  ok('the Terminal opens one window, on Linux when Linux is set up', s && s.termWins == 1 && s.tabs.length == 1 && /Debian/.test(s.tabs[0]), s);
  let m = await menu();
  ok('＋ offers both shells', m.length == 2 && /Command Prompt \(C:\)/.test(m[0].t) && /Linux \(Debian\)/.test(m[1].t) && !m[1].off, m);
  await choose(0);   // Command Prompt (C:)
  s = await state();
  ok('the Command Prompt is a tab in that same window, shown and ready to type in', s && s.termWins == 1 && s.tabs.length == 2 && /dtcmd/.test(s.shown[0] || '') && s.keys == 'none', s);
  await p.evaluate(() => [...document.querySelectorAll('.dterm .dttab')].find(t => /Debian/.test(t.textContent)).click()); await wait(400);
  s = await state();
  ok('and the Debian tab switches back to the Linux terminal', s && s.tabs.length == 2 && !/dtcmd/.test(s.shown[0] || '') && s.keys == 'flex', s);

  // ---- 2. every way in comes to the window that is open: prompt() (.BAT, Lua, rpk), then the Terminal again (Ctrl+Alt+T)
  await p.evaluate(() => { window.__P = prompt() }); await wait(700);
  s = await state();
  ok('prompt() adds a Command Prompt tab to the window that is open, not a second Terminal', s && s.termWins == 1 && s.tabs.length == 3 && /dtcmd/.test(s.focus), s);
  const hasP = await p.evaluate(() => !!(window.__P && window.__P.cwd));
  ok('and the prompt it returns is the new tab\'s own', hasP);
  await p.evaluate(() => { terminalApp() }); await wait(500);
  s = await state();
  ok('the Terminal opened again (Ctrl+Alt+T, the Start menu) focuses the one window', s && s.termWins == 1 && s.tabs.length == 3, s);

  // ---- 3. Debian not set up: the menu says so and offers to set it up, instead of another Command Prompt tab
  await p.evaluate(() => { window.__st = {installed: false} }); await wait(8500);   // the remembered "yes" holds 8 s
  m = await menu();
  ok('without Debian, ＋ offers "Set up Linux (Debian)…" and not a Linux tab', m.length == 2 && /Command Prompt \(C:\)/.test(m[0].t) && /Set up Linux/.test(m[1].t) && !m[1].off, m);
  await choose(1);
  const setup = await p.evaluate(() => { const w = [...document.querySelectorAll('.win')].find(x => /Linux terminal with Debian/.test(x.textContent)); return w ? w.textContent.replace(/\s+/g, ' ').slice(0, 120) : '' });
  ok('choosing it opens the Debian setup page', /Linux terminal with Debian 13/.test(setup), setup);

  // ---- 4. the lock this fixes: the status call fails while the window opens, so it opens on the Command Prompt -- and
  //         the way to Linux is still there once rudolfd answers (before, ＋ gave another Command Prompt tab for good)
  await p.evaluate(() => { [...document.querySelectorAll('.win')].forEach(w => w.querySelector('.cl') && w.querySelector('.cl').click()) }); await wait(400);
  await p.evaluate(() => { window.__st = {installed: true}; window.__fails = 4 });   // the retry is one more call: four failures settle it
  await p.evaluate(() => { terminalApp() }); await wait(1500);
  s = await state();
  ok('with the status call failing, the Terminal opens on the Command Prompt', s && s.termWins == 1 && s.tabs.length == 1 && /C:/.test(s.tabs[0]), s);
  await p.evaluate(() => { window.__fails = 0 });          // rudolfd answers now
  m = await menu();
  ok('and ＋ asks again, so the way to Linux is there once rudolfd answers', m.length == 2 && /Linux \(Debian\)/.test(m[1].t) && !m[1].off, m);
  await choose(1);
  s = await state();
  ok('choosing it adds the Debian tab to that same window', s && s.termWins == 1 && s.tabs.length == 2 && /Debian/.test(s.tabs[1]), s);

  // ---- 5. Ctrl+Shift+T is the same menu
  await p.evaluate(() => { const u = termOpen(); (u ? u.W.body : document.body).dispatchEvent(new KeyboardEvent('keydown', {key: 'T', ctrlKey: true, shiftKey: true, bubbles: true})) });
  await wait(900);
  m = await p.evaluate(() => [...document.querySelectorAll('.cmenu .cmi')].map(d => d.textContent));
  ok('Ctrl+Shift+T opens the same two ways', m.length == 2 && /Command Prompt/.test(m[0]) && /Linux/.test(m[1]), m);

  ok('no page errors', !errs.length, errs);
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); await b.close(); srv.close(); process.exit(R.every(Boolean) ? 0 : 1)
})().catch(e => { console.log('FAIL ' + e.message); process.exit(1) });
