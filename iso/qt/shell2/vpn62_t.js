// 1.6.2 (docs/plan-1.6.2.md Part 33): the VPN in the page (vpn62.js). Settings › Network › VPN and the taskbar's network
// pop-up against a stand-in rudolfd that keeps the VPNs as rudolfd and its helper answer (GET vpn: never a key; the
// policies and who may): a file imported (its name from the file, an administrator's password sent for rudolfd to
// check; a file that is not WireGuard's refused in the page; the key never kept in the page or its storage),
// connected and disconnected from Settings and from the pop-up (its row after the adapters, its state there too),
// "Administrators only" asking for a password, "Always on" with no way off, a child's account, another account's VPN
// ("Ann's VPN is on for everyone on this computer"), renamed and removed.
//   node vpn62_t.js [CHROME]   (build iso/www/index.html first; port 8630)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www'), PORT = 8630;
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 600) : '')) };
const KEY = 'cGFnZS10ZXN0OiBub3QgYSBrZXkgb2YgYW55dGhpbmch';   // (32 bytes of text in base64: "page-test: not a key of anything!")
const CONF = `[Interface]\nPrivateKey = ${KEY}\nAddress = 10.66.0.2/32\nDNS = 10.66.0.1\n\n[Peer]\nPublicKey = cGFnZS10ZXN0OiB0aGUgc2VydmVyJ3Mgc3RhbmQtaW4h\nAllowedIPs = 0.0.0.0/0\nEndpoint = vpn.example.com:51820\n`;
const M = {vpns: [], active: null, me: {name: 'Ann', admin: true, child: false, signedIn: true}, posts: [], bodies: [], n: 0};
const may = (v, off) => { if (off && v.policy == 'always') return 'This VPN is always on: nobody can turn it off. An administrator can change that in Settings › Network › VPN.';
  if (off && M.me.child) return 'Your account has parental controls: it cannot turn a VPN off.'; if (v.policy == 'everyone' || M.me.admin) return null; return off ? 'Only an administrator can turn this VPN off.' : 'Only an administrator can turn this VPN on.' };
const list = () => ({ok: true, wireguard: true, active: M.active, me: M.me, vpns: M.vpns.map(v => { const a = may(v, 0), b = may(v, 1);
  return Object.assign({}, v, {on: M.active == v.id, state: M.active == v.id ? 'connected' : 'off', mayOn: !a, mayOff: !b, why: b || a || ''}) })});
const api = (a, q, method, body) => {
  if (method == 'POST' && a.startsWith('vpn')) { M.posts.push(a + (q ? '?' + q : '')); M.bodies.push(body) }
  const J = (() => { try { return JSON.parse(body || '{}') } catch (e) { return {} } })();
  if (a == 'vpn') return [200, list()];
  if (a == 'vpn/add') { if (J.n != 'Ann' || J.p != 'pw-ann') return [403, {ok: false, error: 'Wrong password.'}];
    const id = 'v0000000' + (++M.n); M.vpns.push({id, name: J.name, type: 'wireguard', policy: 'everyone', server: 'vpn.example.com:51820', addresses: ['10.66.0.2/32'], dns: ['10.66.0.1'], whole: true, by: ''}); return [200, {ok: true, id, notes: []}] }
  if (a == 'vpn/on') { const id = new URLSearchParams(q).get('id'), v = M.vpns.find(x => x.id == id); const cur = M.vpns.find(x => x.id == M.active && x.id != id);
    const adm = J.n == 'Ann' && J.p == 'pw-ann'; const no = may(v, 0) && !(adm && v.policy == 'admins') ? may(v, 0) : cur && may(cur, 1) && !(adm && cur.policy == 'admins') ? may(cur, 1) : null;
    if (no) return [403, {ok: false, needAdmin: /Only an administrator/.test(no), error: no}];
    M.active = id; v.by = M.me.name; return [200, {ok: true, state: 'connected', message: "Connected: this computer's traffic goes through the VPN."}] }
  if (a == 'vpn/off') { const v = M.vpns.find(x => x.id == M.active); if (!v) return [200, {ok: true}]; const adm = J.n == 'Ann' && J.p == 'pw-ann';
    const no = may(v, 1); if (no && !(adm && v.policy == 'admins')) return [403, {ok: false, needAdmin: /Only an administrator/.test(no), error: no}]; M.active = null; v.by = ''; return [200, {ok: true}] }
  if (a == 'vpn/set') { if (J.n != 'Ann' || J.p != 'pw-ann') return [403, {ok: false, error: 'Wrong password.'}]; const v = M.vpns.find(x => x.id == J.id); if (J.name) v.name = J.name; if (J.policy) v.policy = J.policy; return [200, {ok: true}] }
  if (a == 'vpn/remove') { if (J.n != 'Ann' || J.p != 'pw-ann') return [403, {ok: false, error: 'Wrong password.'}]; M.vpns = M.vpns.filter(x => x.id != J.id); if (M.active == J.id) M.active = null; return [200, {ok: true}] }
  if (a == 'net') return [200, {ok: true, interfaces: [{name: 'wlan0', state: 'up', ip: '192.168.1.20/24', default: true, wireless: true, ssid: 'Home', rssi: -50}], gateway: '192.168.1.1', dns: '192.168.1.1', wifiTools: false}];
  if (a == 'events') return [200, {ok: true, events: ''}];
  if (a == 'info') return [200, {ok: true, backlight: false, live: 0, boot: 'uefi', kernel: 'test'}];
  return [200, {ok: true}];
};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0); let body = '';
  q.on('data', d => { body += d }); q.on('end', async () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t62",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/events/wait') await new Promise(r => setTimeout(r, 800));
    if (u.startsWith('/api/')) { const [c, j] = api(u.slice(5) == 'events/wait' ? 'events' : u.slice(5), qs, q.method, body); s.writeHead(c, {'content-type': 'application/json'}); return s.end(JSON.stringify(j)) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(PORT, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const p = await (await b.newContext({viewport: {width: 1280, height: 900}})).newPage(); const errs = []; p.on('pageerror', e => errs.push(String(e.stack || e).slice(0, 300)));
  await p.goto('http://127.0.0.1:' + PORT + '/'); await p.waitForFunction(() => typeof vpnGet == 'function' && typeof net31Pop == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  /* signed in as Ann (an administrator); the administrator's prompt answers Ann's name and password (elevate is core25.js's: its own test has it) */
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0]||{n:'Ann',r:'admin'};USER.r='admin';USER.n='Ann';window.__el=[];window.elevate=async(r,o)=>{__el.push(r);return o&&o.cred?{n:'Ann',p:'pw-ann'}:true};window.__n=[];const o=notify;notify=function(t,m,op){__n.push([t,m]);return o.apply(this,arguments)};return 1})()");
  await q("settings('net')"); await p.waitForSelector('.vpn62 h4', {timeout: 8000});
  let t = await q("document.querySelector('.vpn62').innerText");
  ok('1. Settings › Network › VPN: what it is, none yet, Add a VPN for an administrator', /A VPN for the whole computer/.test(t) && /No VPN yet/.test(t) && !(await q("document.querySelector('.vpna').disabled")), t);
  // the add dialog: a file that is not WireGuard's, then the real one from a file
  await p.click('.vpna'); await p.waitForSelector('.vpnf');
  await q("document.querySelector('.vpnpd').open=true"); await p.fill('.vpnt', 'client\nremote vpn.example.com 1194\n'); await p.fill('.vpnn', 'X'); M.posts = [];
  await p.click('.rmbox .y'); await wait(300);
  ok('2. a file that is not WireGuard\'s is refused in the page (nothing sent)', /not a WireGuard file/.test(await q("document.querySelector('.vpnm').textContent")) && !M.posts.length, await q("document.querySelector('.vpnm').textContent"));
  await p.fill('.vpnt', ''); await p.fill('.vpnn', '');
  const cf = path.join(require('os').tmpdir(), 'Home VPN.conf'); fs.writeFileSync(cf, CONF); await p.setInputFiles('.vpnf', cf); await wait(300);
  ok('3. ...its name taken from the file\'s', await q("document.querySelector('.vpnn').value") == 'Home VPN', await q("document.querySelector('.vpnn').value"));
  await p.click('.rmbox .y'); await p.waitForSelector('.vpnc', {timeout: 5000}); await wait(300);
  const ab = JSON.parse(M.bodies[M.bodies.length - 1] || '{}');
  ok('4. added: POST vpn/add with the administrator\'s name and password for rudolfd to check, the name and the file', M.posts.includes('vpn/add') && ab.n == 'Ann' && ab.p == 'pw-ann' && ab.name == 'Home VPN' && ab.conf.includes('[Interface]') && (await q("__el.join('|')")).includes('Add the VPN Home VPN'), ab);
  const leak = await q(`(()=>{const k='${KEY}';return [document.documentElement.innerHTML.includes(k),JSON.stringify(localStorage).includes(k),[...document.querySelectorAll('textarea,input')].some(e=>(e.value||'').includes(k))]})()`);
  ok('5. the key is nowhere in the page afterwards: not in the page, its storage or a field', leak.every(x => !x), leak);
  t = await q("document.querySelector('.vpnc').innerText");
  ok('6. listed: its name, Off, its server, address and name server; Connect', /Home VPN/.test(t) && /Off/.test(t) && /vpn\.example\.com:51820/.test(t) && /10\.66\.0\.2\/32/.test(t) && /name lookups through 10\.66\.0\.1/.test(t) && await q("document.querySelector('.vpnb').textContent") == 'Connect', t);
  M.posts = []; await p.click('.vpnb'); await wait(1200);
  t = await q("document.querySelector('.vpnc').innerText");
  ok('7. Connect: POST vpn/on for it; Connected, on for everyone on this computer (turned on by Ann)', M.posts.includes('vpn/on?id=v00000001') && /Connected/.test(t) && /On for everyone on this computer \(turned on by Ann\)/.test(t) && await q("document.querySelector('.vpnb').textContent") == 'Disconnect', [M.posts, t]);
  // the taskbar's network pop-up
  await q("document.querySelectorAll('.rwin,.win').forEach(w=>{});net31Pop()"); await p.waitForSelector('.np31 .npvpn', {timeout: 5000});
  let pop = await q("(()=>{const p=document.querySelector('.np31');return {nps:p.querySelector('.nps').textContent,rows:[...p.querySelectorAll('.npr')].map(r=>[r.className,r.children[0].textContent,r.children[1].textContent,r.children[2].textContent])}})()");
  ok('8. the pop-up: the VPN after the adapters, its state (Connected, the row in use), and the header says so', /^Connected.* · VPN: Home VPN$/.test(pop.nps) && pop.rows.length == 2 && pop.rows[1][0].includes('on') && pop.rows[1][1] == '🔒' && pop.rows[1][2] == 'VPN · Home VPN' && pop.rows[1][3] == 'Connected', pop);
  M.posts = []; await p.click('.np31 .npvpn'); await wait(1000);
  pop = await q("[...document.querySelectorAll('.np31 .npvpn')].map(r=>r.className+'|'+r.children[2].textContent)");
  ok('9. a click on its row turns it off (no question: Ann turned it on, Everyone); the row says Off', M.posts.includes('vpn/off') && pop.length == 1 && pop[0] == 'npr npvpn|Off', [M.posts, pop]);
  M.posts = []; await p.click('.np31 .npvpn'); await wait(1000);
  ok('10. ...and on again', M.posts.includes('vpn/on?id=v00000001') && (await q("document.querySelector('.np31 .npvpn').children[2].textContent")) == 'Connected', M.posts);
  await q("document.querySelector('.np31')&&document.querySelector('.np31').remove()");
  // another account turned it on: Bob, Everyone
  M.me = {name: 'Bob', admin: false, child: false, signedIn: true}; await q("USER.r='user';USER.n='Bob';vpnGet()"); await wait(600);
  ok('11. a standard account: the administrator\'s parts hidden, Add a VPN not offered', await q("document.querySelector('.vpna').disabled") && await q("document.querySelector('.vpnad').hidden") && /Only an administrator can add/.test(await q("document.querySelector('.vpn62').innerText")));
  await q("net31Pop()"); await p.waitForSelector('.np31 .npvpn'); M.posts = []; await p.click('.np31 .npvpn'); await wait(800);
  ok('12. Ann\'s VPN from Bob\'s pop-up: not turned off there, Settings › Network opened to ask first', !M.posts.includes('vpn/off') && !(await q("!!document.querySelector('.np31')")) && await q("!!document.querySelector('.vpn62')"), M.posts);
  await p.click('.vpnb'); await p.waitForSelector('.vpnatx', {timeout: 3000});
  t = await q("document.querySelector('.vpnatx').textContent");
  ok('13. Disconnect: "Ann\'s VPN is on for everyone on this computer", asked first', /^Ann's VPN is on for everyone on this computer\./.test(t), t);
  M.posts = []; await p.click('.rmbox .y'); await wait(1000);
  ok('14. ...turned off when Bob says so', M.posts.includes('vpn/off') && /Off/.test(await q("document.querySelector('.vpnc .vpnst').textContent")), M.posts);
  // Administrators only, then Always on, then a child's account
  M.vpns[0].policy = 'admins'; await q("vpnGet()"); await wait(500); M.posts = []; await q("__el=[]"); await p.click('.vpnb'); await wait(1000);
  ok('15. Administrators only, Bob connects: an administrator\'s password asked for and sent with it', M.posts.includes('vpn/on?id=v00000001') && JSON.parse(M.bodies[M.bodies.length - 1]).n == 'Ann' && (await q("__el.join()")).includes('Turn on the VPN Home VPN'), [M.posts, await q("__el")]);
  M.vpns[0].policy = 'always'; await q("vpnGet()"); await wait(500);
  ok('16. Always on: Disconnect is not offered, and why is said', await q("document.querySelector('.vpnb').disabled") && /always on: nobody can turn it off/.test(await q("document.querySelector('.vpnc').innerText")));
  M.vpns[0].policy = 'everyone'; M.me = {name: 'Kim', admin: false, child: true, signedIn: true}; await q("USER.n='Kim';vpnGet()"); await wait(500);
  ok('17. a child account (parental controls): Disconnect is not offered, and why is said', await q("document.querySelector('.vpnb').disabled") && /parental controls/.test(await q("document.querySelector('.vpnc').innerText")));
  // Ann again: who may (the select), renamed, removed
  M.me = {name: 'Ann', admin: true, child: false, signedIn: true}; await q("USER.r='admin';USER.n='Ann';vpnGet()"); await wait(500); M.posts = [];
  await p.selectOption('.vpnp', 'admins'); await wait(800);
  ok('18. who may turn it on and off: set by an administrator (POST vpn/set with the password)', M.posts.includes('vpn/set') && JSON.parse(M.bodies[M.bodies.length - 1]).policy == 'admins' && M.vpns[0].policy == 'admins', M.posts);
  await p.click('.vpnr'); await p.waitForSelector('.rmodal .t'); await p.fill('.rmodal .t', 'Work'); await p.click('.rmodal .ok'); await wait(800);
  ok('19. renamed', M.vpns[0].name == 'Work' && /Work/.test(await q("document.querySelector('.vpnc .vpnnm').textContent")));
  await p.click('.vpnx'); await p.waitForSelector('.vpnatx'); await p.click('.rmbox .y'); await wait(1000);
  ok('20. removed (asked first, then the password)', M.posts.includes('vpn/remove') && !M.vpns.length && /No VPN yet/.test(await q("document.querySelector('.vpn62').innerText")), M.posts);
  ok('21. no page error', !errs.length, errs);
  await b.close(); srv.close(); try { fs.unlinkSync(cf) } catch (e) {}
  const k = R.filter(Boolean).length; console.log(`${k}/${R.length} passed`); process.exit(k == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
