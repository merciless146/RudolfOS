// 1.5.4 (games30.js): Steam's card in App Store › Games says whether Proton has Vulkan on this computer (and why not),
// what anti-cheat allows, and how much space is free for games; game mode holds notifications while a Linux game is
// full screen. Graphics as rudolfd reports them (system: gpu,
// renderer) for: an AMD laptop, a computer drawing with the processor, Haswell, an NVIDIA card before Turing (nouveau
// without NVK), Turing with NVK (Zink), an old Radeon, a virtual machine. The desktop page with a stand-in rudolfd.
//   node steamcard_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
const G = {
  amd: {gpu: 'card1 0x1002:0x1506 amdgpu', renderer: 'AMD Radeon 610M (radeonsi, mendocino, LLVM 17.0.6, DRM 3.64, 6.18.0-rudolf)'},
  soft: {gpu: 'card0 0x1002:0x1506 amdgpu', renderer: ''},
  hsw: {gpu: 'card0 0x8086:0x0416 i915', renderer: 'Mesa Intel(R) HD Graphics 4600 (HSW GT2)'},
  tgl: {gpu: 'card0 0x8086:0x9a49 i915', renderer: 'Mesa Intel(R) Xe Graphics (TGL GT2)'},
  pascal: {gpu: 'card0 0x10de:0x1c82 nouveau', renderer: 'NV137'},
  turing: {gpu: 'card0 0x10de:0x1f08 nouveau', renderer: 'zink Vulkan 1.3(NVIDIA GeForce RTX 2060 (NVK TU106))'},
  radeon: {gpu: 'card0 0x1002:0x6779 radeon', renderer: 'AMD CAICOS (DRM 2.50.0 / 6.18.0-rudolf)'},
  vm: {gpu: 'card0 0x1af4:0x1050 virtio-pci', renderer: 'virgl (Mesa)'},
};
let mode = 'amd';
const api = a => {
  if (a == 'system') return Object.assign({ok: true}, G[mode]);
  if (a == 'apps/status') return {ok: true, flatpak: true, persistent: true, free: 52e9};
  if (a == 'apps/installed') return {ok: true, apps: []};
  if (a == 'apps/catalog') return {ok: true, apps: []};
  if (a == 'events/wait') return {ok: true, events: []};
  return {ok: true} };
const srv = http.createServer((q, s) => { const [u0] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5)))) }
    s.writeHead(404); s.end() }) }).listen(8096, '127.0.0.1');
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto('http://127.0.0.1:8096/'); await p.waitForFunction(() => typeof gamesTab == 'function', null, {timeout: 30000});
  const card = async m => { mode = m; return p.evaluate(async () => { let d = document.getElementById('gmt'); if (d) d.remove(); d = document.createElement('div'); d.id = 'gmt'; document.body.appendChild(d);
    await gamesTab(d, () => {}, null); const c = [...d.querySelectorAll('.gmc')].find(x => x.querySelector('b').textContent == 'Steam');
    const n = c && c.querySelector('.gmst'); return n ? {vk: (n.querySelector('.gmvk') || {}).textContent || '', ac: n.querySelector('.gmac').textContent, sp: n.querySelector('.gmsp').textContent, cloud: !!n.querySelector('.gmcloud')} : null }) };
  let c = await card('amd');
  ok('1. AMD: Vulkan, yes', c && /Vulkan: yes/.test(c.vk), c);
  ok('2. anti-cheat in one line, with cloud gaming for the others', c && /Easy Anti-Cheat/.test(c.ac) && /Valorant/.test(c.ac) && c.cloud, c);
  ok('3. the space free for games, and library folders on other drives', c && /52(\.0)? GB free/.test(c.sp) && /library folder/.test(c.sp), c);
  for (const [m, re, n] of [['soft', /draws with the processor/, '4. drawing with the processor'], ['hsw', /older than Broadwell/, '5. Intel before Broadwell'],
      ['pascal', /Older NVIDIA cards/, '6. NVIDIA without NVK'], ['radeon', /radeon driver/, '7. an old Radeon'], ['vm', /virtual machine/, '8. a virtual machine']]) {
    c = await card(m); ok(n + ': "Needs a graphics card with Vulkan", and why', c && /Needs a graphics card with Vulkan/.test(c.vk) && re.test(c.vk), c) }
  c = await card('tgl'); ok('9. Intel Tiger Lake: Vulkan, yes', c && /Vulkan: yes/.test(c.vk), c);
  c = await card('turing'); ok('10. NVIDIA Turing on NVK: Vulkan, yes', c && /Vulkan: yes/.test(c.vk), c);
  const gm = await p.evaluate(() => { const r = []; LX.mode = 'win 1a00003'; LX.wins = [{win: '1a00003', shown: true, fullscreen: true}]; r.push(gameMode());
    LX.wins = [{win: '1a00003', shown: true, fullscreen: false}]; r.push(gameMode()); LX.wins = [{win: '1a00003', shown: true, fullscreen: true}]; LX.mode = 'desktop'; r.push(gameMode()); return r });
  ok('11. game mode: while a Linux app\'s full-screen window is in front (a Steam game), not when it is windowed or behind the desktop', gm[0] === true && gm[1] === false && gm[2] === false, gm);
  ok('12. no page errors', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1)
})().catch(e => { console.log('FAIL ' + e.stack); process.exit(1) });
