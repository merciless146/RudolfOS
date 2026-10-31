// 1.4.4 part 4 (settings31.js): the Settings layout, with a stand-in rudolfd. Every page opens without an error; each
// moved control is on its new page and changes the same saved value; Pointer speed reaches rudolfd's pointer and the
// in-page mouse capture; natural scrolling and horizontal speed are gone from Accessibility.
// node settings144_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.mjs': 'text/javascript', '.html': 'text/html'};
const M = {calls: [], rd: true, errors: []};
const api = (a, q) => {
  M.calls.push(a + (q ? '?' + q : ''));
  if (a == 'net') return {ok: true, interfaces: [{name: 'wlan0', state: 'up', mac: 'aa', ip: '192.168.1.20/24', wireless: true, rx: 1, tx: 1, virtual: false, default: true, ssid: 'Home'}], gateway: '192.168.1.1', dns: '1.1.1.1', wifiTools: true};
  if (a == 'wifi/status') return {ok: true, raw: 'adapter=wlan0\nwpa_state=COMPLETED\nssid=Home'};
  if (a == 'info') return {ok: true, backlight: true, live: 0, boot: 'uefi', kernel: 'test', vendor: 'V', product: 'P', cpu: 'C', threads: 4, memTotal: 8e9, media: 'disk', persistTotal: 1e10, persistFree: 5e9};
  if (a == 'volume') return {ok: true, volume: 50, muted: false};
  if (a == 'brightness') return {ok: true, brightness: 40};
  if (a == 'time/sync') return {ok: true, message: 'The clock is set: 2026-10-30 18:00.'};
  if (a == 'decktest') return {ok: true, deck: {on: false, cpus: 8, memkb: 8e6}};
  if (a == 'devices') return {ok: true, inputs: [{kind: 'touchpad', name: 'Test touchpad'}], usb: [], drives: []};
  if (a == 'sys/status' || a == 'pkg/status') return {__status: 404, ok: false, error: 'not in this test'};
  if (a == 'events') return {ok: true, events: ''};
  if (a == 'disks') return {ok: true, disks: []};
  if (a == 'display') return {ok: true, setting: 'games', active: 'gpu', xorg: true};
  return {ok: true};
};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', async () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); const h = fs.readFileSync(path.join(WWW, 'index.html'), 'utf8');
      return s.end(M.rd ? h.replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>') : h) }
    if (u == '/api/events/wait') await new Promise(r => setTimeout(r, 1500));
    if (u.startsWith('/api/')) { const j = api(u.slice(5) == 'events/wait' ? 'events' : u.slice(5), qs); s.writeHead(j.__status || 200, {'content-type': 'application/json'}); return s.end(JSON.stringify(j)) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(8096, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  const open = async () => { const p = await b.newPage(); p.on('pageerror', e => { M.errors.push(e.message); console.log('page error:', e.stack.slice(0, 400)) });
    await p.goto('http://127.0.0.1:8096/'); await p.waitForFunction(() => typeof settings == 'function' && typeof launch == 'function', null, {timeout: 30000});
    await p.evaluate("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';window.elevate=async()=>true;return 1})()");
    return p };
  const p = await open(), q = s => p.evaluate(s);
  const page = async k => { await q(`settings('${k}')`); await q(`SW.show('${k}',1)`); await wait(900); return q("document.querySelector('.sc').innerText") };
  const vis = id => q(`(()=>{const e=document.getElementById('${id}');return !!e&&!!e.offsetParent})()`);
  const keys = await q("Object.keys(SW?{}:{})");
  // every page opens
  await q("settings('disp')"); await wait(800);
  const secs = await q("[...document.querySelectorAll('.sn [data-k]')].map(d=>d.dataset.k)");
  M.errors = []; const blank = [];
  for (const k of secs) { const n0 = M.errors.length; const t = await page(k); if (!t || t.length < 20) blank.push(k); if (M.errors.length > n0) console.log('errors on page', k) }
  ok('1. every Settings page opens without an error (' + secs.length + ' pages)', !M.errors.length && !blank.length, [M.errors, blank]);
  ok('...the Oracle page is called "Oracle"', await q("[...document.querySelectorAll('.sn [data-k]')].find(d=>d.dataset.k=='web').textContent.includes('Oracle')&&!/sign-in/.test([...document.querySelectorAll('.sn [data-k]')].find(d=>d.dataset.k=='web').textContent)"));
  // 16, 17, 18: Accessibility
  let t = await page('acc');
  ok('2. Accessibility: natural scrolling and horizontal speed are gone', !(await q("!!document.getElementById('ans')||!!document.getElementById('ahs')")) && !/Horizontal speed|Natural scrolling/i.test(t));
  ok('...pointer speed, primary button, double-click, scroll speed and single click are not shown there', !(await vis('msens')) && !(await vis('mhand')) && !(await vis('mdbl')) && !(await vis('mscroll')) && !(await vis('msingle')));
  ok('...cursor size and colour and the pointer helpers stay, under "Pointer and cursor"', await vis('csize') && await vis('ccol') && await vis('clocate') && /Pointer and cursor/i.test(t));
  ok('...Language is not there', !(await vis('alg')) && !/Language and region/i.test(t));
  const h4 = await q("[...document.querySelectorAll('.sc h4')].filter(h=>h.offsetParent).map(h=>h.textContent.trim())");
  ok('24. Vision, Hearing and Voice control groups (no "Hearing and speech", no "Hearing and voice")', ['Vision', 'Hearing', 'Voice control'].every(x => h4.includes(x)) && !h4.includes('Hearing and speech') && !h4.includes('Hearing and voice') && !h4.includes('Color vision'), h4);
  const grp = id => q(`(()=>{const e=document.getElementById('${id}');let r=e;while(r&&!(r.classList&&r.classList.contains('r2')))r=r.parentElement;const h=r&&r.previousElementSibling;return h&&h.textContent.trim()})()`);
  ok('...colour vision, text-to-speech and the screen reader in Vision; visual sound alerts, live captions and caption style in Hearing', await grp('acb') == 'Vision' && await grp('att') == 'Vision' && await grp('asr') == 'Vision' && await grp('avc') == 'Hearing' && await grp('alc') == 'Hearing' && await grp('acs') == 'Hearing' && await grp('avo') == 'Voice control', [await grp('acb'), await grp('att'), await grp('avc'), await grp('acs')]);
  // mouse page
  await page('mouse');
  ok('18. Mouse and touchpad: primary button, double-click speed, scroll speed, single-click opening', await vis('mpb') && await vis('mpd') && await vis('mps') && await vis('mpo'));
  await q("(()=>{const e=document.getElementById('mpb');e.value='left';e.dispatchEvent(new Event('change'))})()");
  ok('...the primary button changes the same saved value (S.mhand, kept)', await q("S.mhand=='left'&&SC.mhand=='left'&&JSON.parse(localStorage.getItem('bd95set')).mhand=='left'&&document.documentElement.classList.contains('lefthand')"));
  await q("(()=>{const e=document.getElementById('mpb');e.value='right';e.dispatchEvent(new Event('change'))})()");
  await q("(()=>{const e=document.getElementById('mpd');e.value=600;e.dispatchEvent(new Event('input'))})()");
  ok('...double-click speed (S.mdbl) is used by the desktop', await q("S.mdbl==600&&dblMs()==600"));
  M.calls = []; await q("(()=>{const e=document.getElementById('ptsp');e.value=2;e.dispatchEvent(new Event('input'))})()"); await wait(400);
  ok('17. one Pointer speed: rudolfd\'s pointer gets it', M.calls.some(c => /^pointer\?speed=2&/.test(c)), M.calls);
  ok('...and games that capture the mouse follow it (S.msens, qScale)', await q("S.msens==msensOf(2)&&Math.abs(ptrF()/2-1)<0.2"), await q("[S.msens,ptrF()]"));
  ok('...and it is kept (it was lost at the next start before 1.4.4)', await q("JSON.parse(localStorage.getItem('bd95set')).ptr.speed==2"));
  // 19, 20: System
  t = await page('sys');
  ok('19. System Settings › Date, time and language: Language', /Date, time and language/i.test(t) && await vis('slg'));
  await q("(()=>{const e=document.getElementById('slg');e.value='es';e.dispatchEvent(new Event('change'))})()"); await wait(300);
  ok('...it changes S.lang, and System\'s Apply keeps it', await q("S.lang=='es'&&secDirty('sys')"));
  await q("[...document.querySelectorAll('.sc .sfoot button')].find(b=>b.classList.contains('sfv')).click()"); await wait(300); await q("document.querySelector('.sdlg button:last-child').click()"); await wait(300);
  ok('...Revert puts it back', await q("S.lang=='en'||S.lang==undefined"), await q("S.lang"));
  M.calls = []; await q("document.querySelector('.syslang .nts').click()"); await wait(500);
  ok('20. Set the clock from the internet is there (time/sync)', M.calls.includes('time/sync'), M.calls);
  // 21: Network → Oracle
  t = await page('net');
  ok('21. Network Settings: no Web search, no Downloads, no Clock', !(await q("[...document.querySelectorAll('.sc h4')].some(h=>h.offsetParent&&/Web search|Downloads|^Clock$/.test(h.textContent))")) && /Oracle/i.test(t));
  t = await page('web');
  ok('...the Oracle page: real tabs, then Web search and Downloads', /Real browser tabs/i.test(t) && /Web search/i.test(t) && /Downloads/i.test(t) && /Largest download/i.test(t), t.slice(0, 300));
  ok('...without the network page\'s other sections', !/Network hardware|Add a connection|Manual address|Wi-Fi/i.test(t.replace(/Wi-Fi\)/g, '')), t);
  await q("(()=>{const s=[...document.querySelectorAll('.orsearch select[data-k=prov]')][0];s.value='ddg';s.dispatchEvent(new Event('change',{bubbles:true}));s.dispatchEvent(new Event('input',{bubbles:true}))})()"); await wait(300);
  await q("[...document.querySelectorAll('.orsearch .sfoot button')].find(b=>b.classList.contains('sfa')).click()"); await wait(300); await q("document.querySelector('.sdlg button:last-child').click()"); await wait(500);
  ok('...its Apply sets the search service (NETC.search.prov)', await q("NETC.search.prov") == 'ddg', await q("NETC.search.prov"));
  // 23: For developers
  t = await page('disp');
  ok('23. Display Properties: no Cyberdeck test', !/Cyberdeck/i.test(t));
  ok('25. one Taskbar group with its position and grouping', await q("(()=>{const h=[...document.querySelectorAll('.sc h4')].filter(h=>h.textContent.trim()=='Taskbar');return h.length==1&&h[0].nextElementSibling.querySelector('#tb')&&h[0].nextElementSibling.querySelector('#dtg')?1:0})()") == 1);
  ok('...and the brightness (part 2)', await vis('osd') || await q("!!document.querySelector('.briv')"));
  t = await page('upd');
  ok('...Update and recovery: no "Update from a file"', !/Update from a file|Update the page/i.test(t));
  t = await page('about');
  ok('...About › For developers: the desktop page from a file and the Cyberdeck test', /For developers/i.test(t) && /Update the page from a file/i.test(t) && /Cyberdeck test/i.test(t), t.slice(-500));
  // 26
  t = await page('stor');
  ok('26. Storage: one Backup group, with when to use it', (t.match(/Backup/gi) || []).length >= 1 && /Back up everything/i.test(t));
  // 27
  t = await page('vol');
  ok('27. Volume Controls: System volume, then the desktop sounds', /System volume/i.test(t) && /Desktop sounds/i.test(t) && !/Master volume/i.test(t));
  // search
  const sr = async w => q(`startSearch(${JSON.stringify(w)}).filter(r=>r.g=='Settings').map(r=>r.t)`);
  ok('Settings search finds the moved controls on their new pages', (await sr('double-click')).some(x => /Mouse/.test(x)) && (await sr('language')).some(x => /^System/.test(x)) && (await sr('web search')).some(x => /^Oracle/.test(x)) && (await sr('cyberdeck')).some(x => /About/.test(x)), [await sr('double-click'), await sr('language'), await sr('web search')]);
  ok('no page error along the way', !M.errors.length, M.errors);
  await p.close();
  // the browser version
  M.rd = false; M.errors = []; const p2 = await open();
  await p2.evaluate("settings('disp')"); await wait(800);
  const s2 = await p2.evaluate("[...document.querySelectorAll('.sn [data-k]')].map(d=>d.dataset.k)");
  for (const k of s2) { await p2.evaluate(`SW.show('${k}',1)`); await wait(500) }
  await p2.evaluate("SW.show('net',1)"); await wait(500);
  ok('the browser version: every page opens, Network Settings keeps its connections', !M.errors.length && await p2.evaluate("[...document.querySelectorAll('.sc h4')].some(h=>h.offsetParent&&/^Connections$/.test(h.textContent))"), M.errors);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
