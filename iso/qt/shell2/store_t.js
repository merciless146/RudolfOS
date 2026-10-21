// App Store 1.2.5: Linux apps over Wi-Fi (the download question, Metered networks, retried downloads) and the Games
// tab, in the desktop page with a stand-in rudolfd, in Chromium. node store_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path'), {execSync} = require('child_process');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.mjs': 'text/javascript', '.html': 'text/html'};
// what the stand-in rudolfd says; the tests change it
const M = {link: 'wifi', ssid: 'Home', renderer: null, download: 182e6, runtime: 1.1e9, calls: []};
const api = (a, q) => {
  M.calls.push(a + (q ? '?' + q : ''));
  if (a == 'apps/status') return {ok: true, flatpak: true, persistent: true, free: 40e9};
  if (a == 'apps/catalog') return {ok: true, apps: [{id: 'org.prismlauncher.PrismLauncher', name: 'Prism Launcher', summary: 'Minecraft launcher', verified: true}, {id: 'org.libretro.RetroArch', name: 'RetroArch', summary: 'Emulators', verified: true}]};
  if (a == 'apps/installed') return {ok: true, apps: []};
  if (a == 'apps/info') return {ok: true, description: 'An app.', permissions: {shared: 'network'}};
  if (a == 'apps/size' || a == 'native/size') return {ok: true, download: M.download, installed: 500e6, runtime: 'org.freedesktop.Platform/x86_64/24.08', runtimeDownload: M.runtime, link: M.link, dev: M.link == 'wifi' ? 'wlan0' : 'eth0'};
  if (a == 'wifi/status') return {ok: true, raw: M.link == 'wifi' ? `adapter=wlan0\nwpa_state=COMPLETED\nssid=${M.ssid}\nip_address=192.168.1.9\nsaved=0\t${M.ssid}\tany\t[CURRENT]` : 'adapter='};
  if (a == 'system') return {ok: true, renderer: M.renderer, display: M.renderer ? 'gpu' : 'gpu-nomodeset'};
  if (a == 'native/status') return {ok: true, flatpak: true, persistent: true, installed: false, free: 40e9};
  if (a == 'job') return {ok: true, running: false, status: 0, name: 'install', log: 'Done.'};
  return {ok: true};
};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs))) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(8096, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  // rudolfd's install command: a download that stops is tried again (three tries in all)
  const src = fs.readFileSync(path.join(ROOT, 'iso/sys/rudolfd.c'), 'utf8');
  const fp = src.match(/#define FP_INSTALL ("[^\n]*") \\\n\t("[^\n]*")/).slice(1, 3).map(x => JSON.parse(x)).join('');
  const tmp = fs.mkdtempSync('/tmp/fpt'); fs.writeFileSync(tmp + '/flatpak', '#!/bin/sh\nn=$(cat ' + tmp + '/n 2>/dev/null || echo 0); n=$((n+1)); echo $n > ' + tmp + '/n; [ $n -ge ${OKAT:-3} ]\n', {mode: 0o755});
  const runFp = okat => { try { fs.unlinkSync(tmp + '/n') } catch (e) {} let out, rc = 0; try { out = execSync(`sh -c '${fp.replace(/%s/, 'x.y.Z').replace(/sleep 10/, 'sleep 0')}'`, {env: {PATH: tmp + ':/usr/bin:/bin', OKAT: String(okat)}}).toString() } catch (e) { out = String(e.stdout); rc = e.status } return {rc, out, tries: +fs.readFileSync(tmp + '/n', 'utf8')} };
  let r = runFp(3); ok('a download that stops twice finishes on the third try', r.rc == 0 && r.tries == 3 && (r.out.match(/Trying again/g) || []).length == 2 && /Done\.$/.test(r.out.trim()), r);
  r = runFp(9); ok('...and fails after three', r.rc != 0 && r.tries == 3, r);
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  p.on('pageerror', e => console.log('page error:', e.message));
  await p.goto('http://127.0.0.1:8096/'); await p.waitForFunction(() => typeof appStore == 'function' && typeof gamesTab == 'function', null, {timeout: 30000});
  await p.evaluate(() => { try { svj('bd95setup', 1) } catch (e) {} ['oobe', 'login'].forEach(i => { const o = document.getElementById(i); o && o.remove() }); wins.slice().forEach(w => w.close()); window.elevate = async () => true });
  // the question itself
  const notes = await p.evaluate(() => [appDownloadNote({ok: true, download: 182e6, runtimeDownload: 1.1e9, installed: 5e8, link: 'wifi'}, 'Home', false, 40e9), appDownloadNote({ok: true, download: 182e6, runtimeDownload: 1.1e9, link: 'wired'}, '', false, 0), appDownloadNote({ok: true, download: 100e6, runtimeDownload: 0, link: 'wifi'}, 'Home', false, 0), appDownloadNote({ok: true, download: 100e6, runtimeDownload: 0, link: 'wifi'}, 'Phone', true, 0), appDownloadNote({ok: true, link: 'none'}, '', false, 0), appDownloadNote({ok: true, download: 3e9, runtimeDownload: 0, installed: 6e9, link: 'wifi'}, 'Home', false, 4e9)]);
  ok('Wi-Fi, 1.3 GB: asks, with the size, the network and the free space', notes[0] && /About 1\.3 GB to download over Wi-Fi \(Home\), including the 1\.1 GB runtime/.test(notes[0].text) && /40\.0 GB is free/.test(notes[0].text), notes[0]);
  ok('a network cable: no question', notes[1] === null);
  ok('Wi-Fi, 100 MB: no question', notes[2] === null);
  ok('a metered network, 100 MB: asks', notes[3] && /marked metered/.test(notes[3].text), notes[3]);
  ok('not connected: says so and stops', notes[4] && notes[4].stop);
  ok('...and says when it will not fit', notes[5] && /more than the 4\.0 GB free/.test(notes[5].text), notes[5]);
  // Games tab, drawing with the processor
  await p.evaluate(() => { window.__S = appStore(); return 1 });
  const tabs = await p.evaluate(() => [...__S.body.querySelectorAll('.tmtabs button')].map(b => b.textContent).join());
  ok('the App Store has a Games tab after Get apps', tabs == 'Get apps,Games,Linux apps,Installed,Built-in apps', tabs);
  await p.evaluate(() => __S.body.querySelector(".tmtabs [data-t='games']").click());
  await p.waitForFunction(() => __S.body.querySelectorAll('.gmc').length >= 5, null, {timeout: 10000}).catch(() => 0);
  await p.waitForFunction(() => /to download/.test(__S.body.textContent), null, {timeout: 10000}).catch(() => 0);   // the sizes arrive after the cards
  let g = await p.evaluate(() => ({note: !__S.body.querySelector('.gmnote').hidden && __S.body.querySelector('.gmnote').textContent, cards: [...__S.body.querySelectorAll('.stc')].map(c => c.querySelector('b').textContent + ' | ' + [...c.querySelectorAll('.gmlab')].map(x => x.textContent).join() + ' | ' + ((c.querySelector('.gmsz') || {}).textContent || ''))}));
  ok('games: Roblox, Steam, Prism, RetroArch and the built-in ones', ['Roblox (Sober)', 'Steam', 'Prism Launcher (Minecraft)', 'RetroArch', 'Emu', 'Quake Engine', 'Board Games', 'Card Games'].every(n => g.cards.some(c => c.startsWith(n + ' |'))), g.cards);
  ok('...each labelled', g.cards.every(c => /Needs the graphics card|Runs anywhere/.test(c)) && /^Roblox.*Needs the graphics card/.test(g.cards[0]) && g.cards.some(c => /^RetroArch \| Runs anywhere/.test(c)), g.cards);
  ok('...the real download size from Flathub', g.cards.some(c => /^Steam .*1\.3 GB to download/.test(c)), g.cards);
  ok('drawing with the processor: the note, pointing to Task Manager', /draws with the processor/.test(g.note || '') && /Task Manager/.test(g.note), g.note);
  M.renderer = 'NV174 (RTX 3080)';
  await p.evaluate(() => __S.body.querySelector(".tmtabs [data-t='games']").click()); await wait(1200);
  ok('...and no note when the graphics card draws', await p.evaluate(() => __S.body.querySelector('.gmnote').hidden));
  // Roblox over Wi-Fi: the consent, then the download question; "Not now" installs nothing
  M.calls = [];
  const getRoblox = async answer => { await p.evaluate(() => { const c = [...__S.body.querySelectorAll('.natc')][0]; [...c.querySelectorAll('button')].find(b => b.textContent == 'Get').click() }); await wait(300);
    await p.evaluate(() => { const d = document.querySelector('.natdlg'); d.querySelector('input').click(); d.querySelector('.g').click() }); await wait(800);
    const q = await p.evaluate(() => { const d = document.querySelector('.natdlg'); return d ? d.querySelector('h3').textContent + ' | ' + d.querySelector('p') .textContent : null });
    if (q) await p.evaluate(a => { const d = document.querySelector('.natdlg'); d.querySelector(a ? '.g' : '.c').click() }, answer); await wait(800); return q };
  let qn = await getRoblox(false);
  ok('Roblox over Wi-Fi: asks before the 1.3 GB download', qn && /^Download Roblox \(Sober\)\?/.test(qn) && /1\.3 GB to download over Wi-Fi \(Home\)/.test(qn), qn);
  ok('..."Not now" downloads nothing', !M.calls.some(c => c.startsWith('native/install')), M.calls);
  await p.evaluate(() => __S.body.querySelector(".tmtabs [data-t='games']").click()); await wait(1200);
  qn = await getRoblox(true);
  ok('..."Download" starts it', M.calls.some(c => c.startsWith('native/install')), M.calls.filter(c => /native/.test(c)));
  // the same over a network cable: no question
  M.link = 'wired'; M.calls = [];
  await p.evaluate(() => __S.body.querySelector(".tmtabs [data-t='games']").click()); await wait(1200);
  qn = await getRoblox(true);
  ok('over a network cable it just starts', qn === null && M.calls.some(c => c.startsWith('native/install')), qn);
  // a Linux game (Prism) on a metered network: the Linux apps page, its consent, then the question
  M.link = 'wifi'; M.ssid = 'Phone'; M.download = 80e6; M.runtime = 0; M.calls = [];
  await p.evaluate(() => { appSetMetered('Phone', true); __S.body.querySelector(".tmtabs [data-t='games']").click() }); await wait(1200);
  await p.evaluate(() => [...__S.body.querySelectorAll('.gmc')].find(c => /Prism/.test(c.textContent)).querySelector('button').click()); await wait(1200);
  await p.evaluate(() => [...__S.body.querySelectorAll('.lxact button')].find(b => b.textContent == 'Get').click()); await wait(500);
  await p.evaluate(() => { const m = document.querySelector('.rmodal'); const g = [...m.querySelectorAll('button')].find(b => /Install|Get/.test(b.textContent)); g.click() }); await wait(1000);
  qn = await p.evaluate(() => { const d = document.querySelector('.natdlg'); return d ? d.querySelector('p').textContent : null });
  ok('a metered network asks even for 80 MB', qn && /80 MB to download on Phone, which is marked metered/.test(qn), qn);
  await p.evaluate(() => document.querySelector('.natdlg .g').click()); await wait(1000);
  ok('...and installs when told to', M.calls.some(c => c.startsWith('apps/install?id=org.prismlauncher')), M.calls);
  // Settings › Network › Saved networks: the Metered switch
  const mt = await p.evaluate(async () => { appSetMetered('Phone', false); const d = document.createElement('div'); document.body.appendChild(d); wifiPanel(d); await new Promise(r => setTimeout(r, 1500));
    const c = d.querySelector('.wfmt input'); if (!c) return null; const before = c.checked; c.click(); const after = appMetered().includes('Phone'); d.remove(); return [before, after] });
  ok('Saved networks have a Metered switch that is kept', mt && mt[0] === false && mt[1] === true, mt);
  await b.close(); srv.close();
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
