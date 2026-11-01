// 1.4.4 part 2 (osd31.js): the volume and brightness keys' indicator, the system volume on the taskbar and in
// Settings, brightness on the taskbar and in Display Properties, events/wait; with a stand-in rudolfd.
// node osd_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 300) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.mjs': 'text/javascript', '.html': 'text/html'};
const M = {calls: [], ev: [], vol: 40, muted: false, bl: 30, rd: true, waits: 0};
const api = (a, q) => {
  M.calls.push(a + (q ? '?' + q : '')); const P = new URLSearchParams(q || '');
  if (a == 'info') return {ok: true, backlight: true, live: 0, boot: 'uefi', kernel: 'test'};
  if (a == 'volume') { if (P.get('set')) M.vol = +P.get('set'), M.muted = false; if (P.get('muted')) M.muted = P.get('muted') == '1';
    if (P.get('step') == 'up') M.vol = Math.min(100, M.vol + 5), M.muted = false; if (P.get('step') == 'down') M.vol = Math.max(0, M.vol - 5);
    return {ok: true, volume: M.vol, muted: M.muted} }
  if (a == 'brightness') { if (P.get('value')) M.bl = +P.get('value'); return {ok: true, brightness: M.bl} }
  if (a == 'brightness/step') { M.bl = Math.min(100, M.bl + (P.get('d') == 'up' ? 5 : -5)); M.ev.push('osd brightness ' + M.bl); return {ok: true, brightness: M.bl} }
  if (a == 'events') { const e = M.ev.join('\n'); M.ev = []; return {ok: true, events: e} }
  return {ok: true};
};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', async () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); const h = fs.readFileSync(path.join(WWW, 'index.html'), 'utf8');
      return s.end(M.rd ? h.replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>') : h) }
    if (u == '/api/events/wait') { M.waits++; for (let i = 0; i < 30 && !M.ev.length; i++) await new Promise(r => setTimeout(r, 100)); }
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5) == 'events/wait' ? 'events' : u.slice(5), qs))) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(8094, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  const open = async () => { const p = await b.newPage(); p.on('pageerror', e => console.log('page error:', e.stack.slice(0, 400)));
    await p.goto('http://127.0.0.1:8094/'); await p.waitForFunction(() => typeof sysVolPop == 'function' && typeof launch == 'function', null, {timeout: 30000});
    await p.evaluate("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';window.elevate=async()=>true;window.__offs=0;A.off=()=>{__offs++};return 1})()");
    return p };
  const p = await open(), q = s => p.evaluate(s);
  await p.waitForFunction(() => document.getElementById('vol').title == 'Volume: 40%' && !!document.getElementById('bri'), null, {timeout: 15000}).catch(() => {});
  ok('1. the taskbar\'s volume icon shows the system volume (40 %), not the desktop mix', await q("$('#vol').title") == 'Volume: 40%', await q("$('#vol').title"));
  ok('2. a brightness icon next to it, with the screen\'s brightness', await q("!!$('#bri')&&$('#vol').nextElementSibling.id=='bri'&&$('#bri').title") == 'Brightness: 30%', await q("$('#bri')&&$('#bri').title"));
  ok('3. events come through events/wait', M.waits > 0 && !M.calls.some(c => c == 'events'), M.waits);
  M.vol = 45; M.ev.push('osd volume 45'); const t0 = Date.now();
  await p.waitForFunction(() => { const o = document.getElementById('osd'); return o && o.style.display != 'none' && /45%/.test(o.textContent) }, null, {timeout: 5000}).catch(() => {});
  const dt = Date.now() - t0;
  ok('4. a volume key press: the indicator shows 45 % at once (well within the old 2.5 s check)', await q("$('#osd')&&$('#osd').textContent") == '45%' && dt < 1500, [await q("$('#osd')&&$('#osd').textContent"), dt]);
  ok('...and the taskbar icon follows', await q("$('#vol').title") == 'Volume: 45%');
  await wait(1800);
  ok('...the indicator goes after 1.5 s', await q("$('#osd').style.display") == 'none');
  M.ev.push('osd volume muted'); await wait(700);
  ok('5. mute: the indicator says Muted and the icon is crossed out', await q("$('#osd').textContent") == 'Muted' && await q("$('#vol').title") == 'Volume: muted' && await q("$('#vol').dataset.lv") == '0');
  M.ev.push('osd brightness 60'); await wait(700);
  ok('6. a brightness key press: the indicator shows 60 % and the brightness icon follows', await q("$('#osd').textContent") == '60%' && await q("$('#bri').title") == 'Brightness: 60%');
  M.ev.push('osd brightness none'); await wait(700);
  ok('...no adjustable backlight: the indicator says so', /cannot be changed/.test(await q("$('#osd').textContent")));
  M.ev.push('osd mic muted'); await wait(700);
  ok('7. the microphone key: Microphone off', await q("$('#osd').textContent") == 'Microphone off');
  M.ev.push('power ac'); await wait(700);
  ok('8. "power ac" (the charger, part 6) does not shut the computer down', await q("__offs") == 0);
  M.ev.push('power 123456'); await wait(700);
  ok('...the power button ("power <ms>") still does', await q("__offs") == 1);
  // the volume popup
  M.muted = false; await q("sysVolGet()"); await q("$('#vol').click()"); await wait(400);
  ok('9. the volume popup is the system volume\'s', await q("!!document.querySelector('.svp')&&document.querySelector('.svp input[type=range]').value") == '45');
  M.calls = []; await q("(()=>{const r=document.querySelector('.svp input[type=range]');r.value=70;r.dispatchEvent(new Event('input',{bubbles:true}))})()"); await wait(500);
  ok('...its slider sets the system volume (volume?set=70)', M.calls.includes('volume?set=70') && M.vol == 70, M.calls);
  M.ev.push('osd volume 70'); await wait(700);
  ok('...and the indicator stays away while the popup is open', await q("$('#osd').style.display") == 'none');
  await q("(()=>{const k=document.querySelector('.svp input[type=checkbox]');k.checked=true;k.dispatchEvent(new Event('change',{bubbles:true}))})()"); await wait(500);
  ok('...Mute mutes it', M.muted && await q("$('#vol').title") == 'Volume: muted');
  await q("document.querySelector('.svp').remove()");
  M.calls = []; await p.hover('#vol'); await p.mouse.wheel(0, -100); await wait(500);
  ok('10. the mouse wheel over the volume icon: volume?step=up', M.calls.includes('volume?step=up') && await q("$('#vol').title") == 'Volume: 75%', [M.calls, await q("$('#vol').title")]);
  M.calls = []; await p.hover('#bri'); await p.mouse.wheel(0, 100); await wait(800);
  ok('11. the mouse wheel over the brightness icon: brightness/step?d=down', M.calls.includes('brightness/step?d=down'), M.calls);
  await q("$('#bri').click()"); await wait(400);
  M.calls = []; await q("(()=>{const r=document.querySelector('.bvp input[type=range]');r.value=80;r.dispatchEvent(new Event('input',{bubbles:true}))})()"); await wait(500);
  ok('12. the brightness popup\'s slider sets the brightness (brightness?value=80)', M.calls.includes('brightness?value=80') && await q("$('#bri').title") == 'Brightness: 80%', M.calls);
  await q("document.querySelector('.bvp').remove()");
  // Settings
  await q("settings('vol')"); await wait(1200);
  ok('13. Settings › Volume Controls: System volume, above the desktop\'s own sounds', await q("(()=>{const d=document.querySelector('.sysv');return !!d&&d.style.display!='none'&&d.querySelector('input[type=range]').value=='75'&&/All desktop sounds/.test(d.parentElement.textContent)})()"));
  M.ev.push('osd volume 20'); M.vol = 20; await wait(700);
  ok('...its slider follows the keys', await q("document.querySelector('.sysv input[type=range]').value") == '20');
  M.calls = []; await q("(()=>{const r=document.querySelector('.sysv input[type=range]');r.value=55;r.dispatchEvent(new Event('input',{bubbles:true}))})()"); await wait(500);
  ok('...and sets the same volume as the keys', M.calls.includes('volume?set=55') && await q("$('#vol').title") == 'Volume: 55%', M.calls);
  await q("wins.slice().forEach(w=>w.close())"); await q("settings('disp')"); await wait(1200);
  ok('14. Settings › Display Properties: Brightness', await q("!!document.querySelector('.briv')&&document.querySelector('.briv input[type=range]').value") == '80');
  M.ev.push('osd brightness 35'); M.bl = 35; await wait(700);
  ok('...it follows the keys', await q("document.querySelector('.briv input[type=range]').value") == '35');
  await p.close();
  // the browser version: as before
  M.rd = false; const p2 = await b.newPage(); await p2.goto('http://127.0.0.1:8094/'); await p2.waitForFunction(() => typeof launch == 'function', null, {timeout: 30000}); await wait(1500);
  await p2.evaluate("(()=>{['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});$('#vol').click()})()"); await wait(400);
  ok('15. the browser version: no brightness icon, the volume popup is the desktop mix', await p2.evaluate("!$('#bri')&&!!document.querySelector('.vp')&&!document.querySelector('.svp')"));
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
