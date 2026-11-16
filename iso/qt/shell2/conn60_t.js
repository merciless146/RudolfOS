// 1.6.0 (plan Part 3, stage A item 3): the connection on the desktop: Settings › Mobile network's Connection (connected
// and in use, or kept in reserve behind Wi-Fi; Disconnect, Connect), Connect automatically, quick settings' 📶 Mobile
// data (only with a modem whose SIM is unlocked), and the notices when mobile data takes over and when Wi-Fi is back
// (rudolfd's mobile/notes). A stand-in rudolfd.   node conn60_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www'); const PORT = 8513;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
let M = [{id: 0, maker: 'Quectel', model: 'EG25-G', state: 'connected', failed: '', locked: '', sim: true, simOp: '310260', simName: 'T-Mobile', operator: 'T-Mobile', reg: 'home', signal: '70', tech: 'lte', iface: 'wwan0'}];
let route = 'wlan0', notes = [{n: 1, t: 'old', b: 'old'}], calls = [], set = {autoconnect: true, off: false, roaming: false, apn: '', apnuser: '', apnpass: false, mode: 'auto', modes: ['auto']};
const api = (a, qs, m) => { const p = new URLSearchParams(qs);
  if (a == 'mobile/status') return {ok: true, running: true, modems: M, notes: [], route, cellular: route == 'wwan0'};
  if (a == 'mobile/connect') { calls.push({a, qs}); M[0].state = p.get('on') == '1' ? 'connected' : 'registered'; M[0].iface = p.get('on') == '1' ? 'wwan0' : ''; return {ok: true, message: p.get('on') == '1' ? 'Connected' : 'Disconnected'} }
  if (a == 'mobile/notes') { const since = +p.get('since') || 0; return {ok: true, notes: notes.filter(n => n.n > since), last: notes.length ? notes[notes.length - 1].n : 0} }
  if (a == 'mobile/settings') { if (m == 'POST') { calls.push({a, qs}); set[p.get('k')] = p.get('v') == '1'; return {ok: true, message: 'saved'} } return {ok: true, sim: true, simOp: '310260', simName: 'T-Mobile', auto: {name: 'T-Mobile', apn: 'fast.t-mobile.com', user: '', source: 'database'}, ...set} }
  if (a == 'mobile/airplane') return {ok: true, on: false};
  return {ok: true} };
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/a/')) { const f = path.join(WWW, u); if (fs.existsSync(f)) { s.writeHead(200); return s.end(fs.readFileSync(f)) } }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(api(u.slice(5), qs || '', q.method))) }
    s.writeHead(404); s.end() }) }).listen(PORT, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 1000}});
  const errs = []; p.on('pageerror', e => { if (!/volUpd/.test(e.stack || '')) errs.push(e.message) });
  await p.goto(`http://127.0.0.1:${PORT}/`); await p.waitForFunction(() => typeof mobConn == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x'}];USER=ACCTS[0];
    window.__nt=[];const on=notify;notify=function(t,b){__nt.push(t+' | '+b);return on.apply(this,arguments)};return 1})()`);
  const open = async () => { await q("(()=>{let d=document.getElementById('mt');if(d)d.remove();d=document.createElement('div');d.id='mt';document.body.appendChild(d);mobPanelS(d);return 1})()"); await wait(1200) };
  const t = () => q("document.getElementById('mt').textContent");
  await open(); let x = await t();
  ok('1. Settings › Mobile network: connected, kept in reserve while Wi-Fi carries the traffic; Disconnect', /Connected: kept in reserve while Wi-Fi carries the traffic/.test(x) && (await q("document.querySelector('#mt .mobcb').textContent")) == 'Disconnect', x);
  route = 'wwan0'; await open(); x = await t();
  ok('2. ...Wi-Fi gone: connected and in use', /Connected and in use/.test(x), x);
  await q("document.querySelector('#mt .mobcb').click()"); await wait(1500); x = await t();
  ok('3. Disconnect: rudolfd asked (on=0), said it stays off until Connect; the page shows Connect', calls.some(c => c.a == 'mobile/connect' && /id=0&on=0/.test(c.qs)) && (await q("__nt.join('\\n')")).includes('stays off until you connect again') && /Not connected/.test(x) && (await q("document.querySelector('#mt .mobcb').textContent")) == 'Connect', x);
  await q("document.querySelector('#mt .mobcb').click()"); await wait(1500);
  ok('...Connect: asked (on=1)', calls.some(c => c.a == 'mobile/connect' && /on=1/.test(c.qs)) && (await q("document.querySelector('#mt .mobcb').textContent")) == 'Disconnect');
  x = await t();
  ok('4. Connect automatically: on by default, saying when (home network; abroad only with roaming), Wi-Fi first', await q("document.querySelector('#mt .mobautoc').checked") && /On the SIM's home network; abroad only when roaming is allowed/.test(x) && /takes over within seconds/.test(x), x);
  await q("document.querySelector('#mt .mobautoc').click()"); await wait(600);
  ok('...turned off: sent', calls.some(c => /k=autoconnect&v=0/.test(c.qs)) && set.autoconnect === false, calls);
  await q("qsToggle()"); await wait(1000);
  let tiles = await q("[...document.querySelectorAll('#qsp .qst span')].map(s=>s.textContent)");
  ok("5. quick settings: 📶 Mobile data (the tenth), on while connected", tiles.length == 10 && tiles[9] == 'Mobile data' && await q("document.querySelector('#qsp .qmob').classList.contains('on')"), tiles);
  calls = []; await q("document.querySelector('#qsp .qmob').click()"); await wait(800);
  ok('...pressed: disconnected (on=0), the switch off', calls.some(c => c.a == 'mobile/connect' && /on=0/.test(c.qs)) && await q("!document.querySelector('#qsp .qmob').classList.contains('on')"), calls);
  await q("document.querySelector('#qsp .qmob').click()"); await wait(800);
  ok('...pressed again: connected (on=1)', calls.some(c => /on=1/.test(c.qs)) && await q("document.querySelector('#qsp .qmob').classList.contains('on')"), calls);
  await q("qsClose()"); M[0] = {...M[0], locked: 'sim-pin', state: 'locked'}; await q("qsToggle()"); await wait(1000);
  tiles = await q("[...document.querySelectorAll('#qsp .qst span')].map(s=>s.textContent)");
  ok('6. a SIM still locked: no Mobile data switch (nine)', tiles.length == 9, tiles); await q("qsClose()");
  await wait(6000); const before = await q("__nt.length");
  notes.push({n: 2, t: '📶 Using mobile data', b: 'The Wi-Fi connection is gone: mobile data carries the traffic now (metered: updates and other large downloads wait).'}); await wait(6500);
  const nt = await q("__nt.slice(" + before + ").join('\\n')");
  ok('7. the notice that mobile data took over, shown within seconds (the old ones not again)', nt.includes('📶 Using mobile data | The Wi-Fi connection is gone') && !nt.includes('old'), nt);
  notes.push({n: 3, t: '📶 Back on Wi-Fi', b: 'Mobile data stays connected, unused, in case Wi-Fi drops again.'}); await wait(6500);
  ok('...and when Wi-Fi is back', (await q("__nt.join('\\n')")).includes('📶 Back on Wi-Fi'));
  ok('8. no page errors', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
