// 1.5.6: a Linux app (Flatpak) put on the desktop shows its own icon, as in the Start menu, not the penguin
// (found on the Vivobook with Discord).   node lxdesk_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 400) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8142, '127.0.0.1');
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 760}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.addInitScript(() => {
    window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0};
    // a 2x2 PNG for apps/icon
    const png = Uint8Array.from(atob('iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAFElEQVR4nGNk+M/wn4GBgYGJgQEAHwQCAV0gWB0AAAAASUVORK5CYII='), c => c.charCodeAt(0));
    window.T = {icon: []};
    const f0 = window.fetch;
    window.fetch = (u, o) => { u = String(u);
      if (u.includes('/api/apps/icon')) { T.icon.push(new URL(u, location.href).searchParams.get('id')); return Promise.resolve(new Response(png, {headers: {'content-type': 'image/png'}})) }
      if (u.includes('/api/')) return Promise.resolve(new Response('{"ok":true,"items":[],"apps":[]}', {headers: {'content-type': 'application/json'}}));
      return f0(u, o) } });
  await p.goto('http://127.0.0.1:8142/'); await p.waitForFunction(() => typeof lxRegister == 'function' && typeof deskMakeIcon == 'function' && FSLOADED && fs['C:'] && fs['C:'].USERS, null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});USER=USER||ACCTS[0];LX.inst=[{id:'com.discordapp.Discord',name:'Discord',launchers:[{desktop:'com.discordapp.Discord',name:'Discord',icon:'com.discordapp.Discord',mime:'',wmclass:'discord',cats:''}]}];lxRegister();return 1})()");
  await q("deskMakeIcon('lx:com.discordapp.Discord')"); await p.waitForTimeout(800);
  const r = await q("(()=>{const el=DESK.querySelector('.ic[data-app=\"lx:com.discordapp.Discord\"]');const im=el&&el.querySelector('b img.lxapp');return {has:!!el,img:!!im,txt:el&&el.querySelector('b').textContent,w:im&&im.getBoundingClientRect().width}})()");
  ok('1. Discord\'s desktop shortcut shows its own icon (from apps/icon), not the penguin', r.has && r.img && r.txt === '' && r.w >= 30 && r.w <= 60, r);
  ok('2. ...asked for Discord\'s icon', (await q("T.icon")).includes('com.discordapp.Discord'));
  ok('3. no page errors', !errs.length, errs);
  await b.close(); srv.close();
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
