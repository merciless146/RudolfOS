// 1.4.4 part 6: one clock (taskbar, calendar, lock screen, screen saver, widgets) in the time zone and format of System
// Settings; quick settings without volume, brightness and battery; the battery on the taskbar; System Settings › Power
// and battery (in System health since 1.5.6, with battery care at 70, 80, 90 or 100 %); with a stand-in rudolfd, in a browser whose own zone is UTC. node taskbar144_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 400) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.mjs': 'text/javascript', '.html': 'text/html'};
const M = {calls: [], ev: [], errors: [], bat: {name: 'BAT0', capacity: 45, status: 'Charging', energy_now: 30000000, energy_full: 40000000, energy_full_design: 42000000, power_now: 8000000, cycle_count: 120}, ac: true, set: {mode_ac: 'balanced', mode_bat: 'balanced', saver_at: 20, saver_now: 0, screen_ac: 10, screen_bat: 5, sleep_ac: 0, sleep_bat: 15, lid_ac: 'sleep', lid_bat: 'sleep', button: 'ask', care: 100}};
const status = () => ({ok: true, ac: M.ac, battery: M.bat, can: {profile: true, profileChoices: 'quiet balanced performance', epp: true, care: true, lid: true, backlight: true, wifiSave: false}, now: {src: M.ac ? 'ac' : 'bat', saver: false, mode: 'balanced', profile: 'balanced', epp: 'balance_performance', screen: 10, sleep: 0, care: M.nowcare ?? M.set.care}, set: M.set});
const api = (a, q) => {
  M.calls.push(a + (q ? '?' + q : ''));
  if (a == 'power/status') return status();
  if (a == 'power/set') { for (const [k, v] of new URLSearchParams(q)) M.set[k] = isNaN(+v) ? v : +v; return status() }
  if (a == 'events') { const e = M.ev.join('\n'); M.ev = []; return {ok: true, events: e} }
  if (a == 'info') return {ok: true, backlight: false, live: 0, boot: 'uefi', kernel: 'test'};
  if (a == 'volume') return {ok: true, volume: 50, muted: false};
  if (a == 'net') return {ok: true, interfaces: [{name: 'wlan0', state: 'up', ip: '192.168.1.2/24', wireless: true, virtual: false, default: true, ssid: 'Home', rx: 0, tx: 0}], gateway: '', dns: '', wifiTools: true};
  if (a == 'stats') return {ok: true, battery: [Object.assign({type: 'Battery', scope: 'System'}, M.bat), {name: 'hidpp_battery_0', type: 'Battery', scope: 'Device', capacity: -1, status: 'Discharging'}, {name: 'ACAD', type: 'Mains', capacity: -1, online: 1}], cpu: 1, memTotal: 8e9, memAvailable: 4e9};
  return {ok: true};
};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', async () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/events/wait') for (let i = 0; i < 20 && !M.ev.length; i++) await new Promise(r => setTimeout(r, 100));
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5) == 'events/wait' ? 'events' : u.slice(5), qs))) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(8098, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  const ctx = await b.newContext({timezoneId: 'UTC'}); const p = await ctx.newPage(); p.on('pageerror', e => { M.errors.push(e.message); console.log('page error:', e.stack.slice(0, 300)) });
  await p.goto('http://127.0.0.1:8098/'); await p.waitForFunction(() => typeof clockParts == 'function' && typeof launch == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';window.elevate=async()=>true;return 1})()");
  // System Settings: Tokyo (UTC+9); the system clock at 20:30 UTC, already the next day in Tokyo
  await q("(()=>{S.tz='Asia/Tokyo';const n=new Date();const t=Date.UTC(n.getUTCFullYear(),n.getUTCMonth(),n.getUTCDate(),20,30,0);S.doff=t-Date.now();window.__sys=1})()");
  for (const h24 of [true, false]) {
    await q(`S.h24=${h24};clk2()`); await wait(200);
    const want = h24 ? '05:30' : '5:30 AM';
    const bar = await q("$('#tray').childNodes[0].textContent");
    await q("$('#tray').click()"); await wait(300);
    const cal = await q("[document.querySelector('#calp .cbig').textContent,document.querySelector('#calp .cdate').textContent,document.querySelector('#calp td.tod')&&document.querySelector('#calp td.tod').textContent]");
    await q("document.getElementById('calp').remove()");
    const tok = await q("(()=>{const d=new Date(sysNow().getTime()+9*3600e3);return [d.getUTCDate(),d.toLocaleDateString('en-US',{timeZone:'UTC',weekday:'long'})]})()");
    ok(`1. ${h24 ? '24-hour' : '12-hour'}: the taskbar says ${want} (Tokyo, not the browser's UTC 20:30)`, bar.startsWith(want.split(' ')[0]) && (h24 || /AM/.test(bar)), bar);
    ok('...the calendar shows the same time and Tokyo\'s date, and marks Tokyo\'s day as today', cal[0] == want && cal[1].includes(tok[1]) && cal[2] == String(tok[0]), [cal, tok]);
    ok('...the screen saver\'s time is the same', await q("ssTime()") == want);
  }
  await q("window.__lk=1;lockScreen()"); await wait(300);
  ok('2. the lock screen shows the same time', await q("document.querySelector('#lockov .lkclock')&&document.querySelector('#lockov .lkclock').textContent") == '5:30 AM', await q("document.querySelector('#lockov .lkclock')&&document.querySelector('#lockov .lkclock').textContent"));
  await q("(()=>{const o=document.getElementById('lockov');o&&o.remove();LOCKED=false})()");
  await q("S.doff=0;S.tz='UTC';clk2()");
  // quick settings
  await q("qsToggle()"); await wait(800);
  ok('3. quick settings: no volume, brightness or battery; the network line', await q("(()=>{const p=document.getElementById('qsp');return !!p&&!p.querySelector('.qv')&&!p.querySelector('.qb')&&!/🔋/.test(p.textContent)&&/Wi-Fi — connected \\(Home\\)/.test(p.querySelector('.qsinfo').textContent)})()"), await q("document.getElementById('qsp')&&document.getElementById('qsp').innerText"));
  await q("qsClose()");
  // the battery on the taskbar
  await p.waitForFunction(() => document.getElementById('bat') && document.getElementById('bat').style.display != 'none', null, {timeout: 5000}).catch(() => {});
  const bat = () => q("(()=>{const b=document.getElementById('bat');if(!b||b.style.display=='none')return null;const r=b.querySelectorAll('rect')[2];return{t:b.textContent,fill:r&&r.getAttribute('fill'),bolt:!!b.querySelector('path'),title:b.title,next:b.nextElementSibling&&b.nextElementSibling.id}})()");
  let x = await bat();
  ok('4. the battery next to the workspace buttons: 45 %, green, charging (bolt), the time to full', x && x.t == '45%' && x.fill == '#22c55e' && x.bolt && x.next == 'wsw' && /Battery 45%, charging · 1 h 15 min to full/.test(x.title), x);
  M.bat = Object.assign({}, M.bat, {capacity: 15, status: 'Discharging'}); M.ac = false; M.ev.push('power battery'); await wait(1200); x = await bat();
  ok('...unplugged ("power battery" event): 15 %, amber, no bolt, the time left', x && x.t == '15%' && x.fill == '#f59e0b' && !x.bolt && /left/.test(x.title), x);
  M.bat = Object.assign({}, M.bat, {capacity: 8}); await q("pwrPoll()"); x = await bat();
  ok('...8 %: red', x && x.fill == '#ef4444', x);
  M.bat = null; await q("pwrPoll()"); x = await bat();
  ok('...no battery: no icon', x === null, x);
  ok('5. a device\'s battery at -1 % is not the computer\'s (Task Manager too)', await q("batOwn([{type:'Battery',scope:'System',capacity:45},{type:'Battery',scope:'Device',capacity:-1},{type:'Mains',capacity:-1}]).length") == 1);
  M.bat = {name: 'BAT0', capacity: 45, status: 'Charging', energy_full: 40000000, energy_full_design: 42000000, cycle_count: 120}; M.ac = true; await q("pwrPoll()");
  // Power and battery
  await q("document.getElementById('bat').click()"); await wait(1500);
  ok('6. the battery on the taskbar opens Settings › System health, with Power and battery between the summary and the events', await q("(()=>{const d=document.getElementById('pwr31');if(!d||!SW||SW.sec!='health')return false;const s=d.parentElement.querySelector(':scope > .hls'),e=d.parentElement.querySelector(':scope > .hlev');return !!s&&!!e&&!!(s.compareDocumentPosition(d)&4)&&!!(d.compareDocumentPosition(e)&4)})()"), await q("SW&&SW.sec"));
  ok('...and no longer in System Settings', await q("(async()=>{SW.show('sys',1);await new Promise(r=>setTimeout(r,800));const n=!document.querySelector('.pwr31');SW.show('health',1);await new Promise(r=>setTimeout(r,1200));return n&&!!document.querySelector('.pwr31')})()"));
  ok('...Battery care offers 70, 80, 90 and 100 %', await q("[...document.querySelectorAll('.pwr31 select[data-k=care] option')].map(o=>o.value).join()") == '70,80,90,100', await q("[...document.querySelectorAll('.pwr31 select[data-k=care] option')].map(o=>o.value).join()"));
  ok('...Settings search: "battery" finds System health', await q("SETPAGES.some(([k,t,kw])=>k=='health'&&kw.includes('battery'))&&!SETPAGES.some(([k,t,kw])=>k=='sys'&&kw.includes('battery'))"));
  const t = await q("document.querySelector('.pwr31')&&document.querySelector('.pwr31').innerText");
  ok('...Power and battery: battery and its health, power mode, battery saver, screen off, sleep, lid, power button, battery care', t && /45 %, charging/.test(t) && /health 95 %, 120 charge cycles/.test(t) && /Power mode/i.test(t) && /Battery saver/i.test(t) && /screen off/i.test(t) && /Sleep after/i.test(t) && /lid/i.test(t) && /power button/i.test(t) && /Battery care/i.test(t), t);
  for (const [k, v] of [['mode_bat', 'saver'], ['screen_bat', '2'], ['lid_ac', 'nothing'], ['button', 'shutdown'], ['care', '80'], ['care', '70'], ['care', '90']]) {
    M.calls = []; await q(`(()=>{const s=document.querySelector('.pwr31 select[data-k=${k}]');s.value='${v}';s.dispatchEvent(new Event('change',{bubbles:true}))})()`); await wait(400);
    ok(`...${k} = ${v} goes to rudolfd (power/set)`, M.calls.includes(`power/set?${k}=${v}`) && String(M.set[k]) == v, M.calls) }
  M.calls = []; await q("(()=>{const s=document.querySelector('.pwr31 .pwsn');s.checked=true;s.dispatchEvent(new Event('change',{bubbles:true}))})()"); await wait(400);
  ok('...battery saver on now', M.calls.includes('power/set?saver_now=1'), M.calls);
  M.nowcare = 80; await q("(async()=>{SW.show('health',1)})()"); await wait(1200);
  ok('...a battery that keeps another limit than the one chosen (90, reports 80) says so', /reports a limit of 80 %/.test(await q("(document.querySelector('.pwr31 .pwcn')||{}).textContent||''")), await q("document.querySelector('.pwr31')&&document.querySelector('.pwr31').innerText"));
  M.nowcare = undefined;
  ok('7. User Settings no longer has Sleep: a link to System health › Power and battery, which opens it', await q("(async()=>{SW.show('usr',1);await new Promise(r=>setTimeout(r,800));if(document.getElementById('usp'))return false;const a=[...SW.el.querySelectorAll('.msg a')].find(x=>/System health › Power and battery/.test(x.textContent));if(!a)return false;a.click();await new Promise(r=>setTimeout(r,1500));return SW.sec=='health'&&!!document.getElementById('pwr31')})()"));
  ok('no page error', !M.errors.length, M.errors);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
