// 2026-11-15 (docs/plan-1.6.0.md Part 14): Settings › Privacy › Saved passwords for apps: the account's keyring and its
// Flatpak apps' secrets (GET keyring/list), each named (never as HTML), Remove after asking (POST keyring/forget), Remove
// all; nobody signed in, the keyring not running, nothing saved.   node keyring60_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const M = {forgot: [], signedIn: true, keyring: true, items: [
  {id: '/org/freedesktop/secrets/collection/login/1', label: 'Claude Safe Storage', app: 'Claude', schema: 'org.freedesktop.Secret.Generic', kind: 'item'},
  {id: '/org/freedesktop/secrets/collection/login/2', label: '<img src=x onerror="window.__pwn=1">', app: '', schema: 'chrome_libsecret_os_crypt_password_v2', kind: 'item'},
  {id: 'app:org.example.Chat', label: 'org.example.Chat', app: 'org.example.Chat', kind: 'flatpak'}]};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', async () => {
    const J = (o, c) => { s.writeHead(c || 200, {'content-type': 'application/json'}); s.end(JSON.stringify(o)) };
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u == '/api/events/wait') { await new Promise(r => setTimeout(r, 800)); s.writeHead(200); return s.end('{"ok":true,"events":""}') }
    if (u == '/api/keyring/list') return J({ok: true, signedIn: M.signedIn, keyring: M.keyring, items: M.signedIn ? M.items : []});
    if (u == '/api/keyring/forget' && q.method == 'POST') { const id = new URLSearchParams(qs).get('id'); M.forgot.push(id); if (id.includes('/2')) return J({ok: false, error: 'the keyring asks for a prompt to remove it'}, 500); M.items = M.items.filter(x => x.id != id); return J({ok: true}) }
    if (u.startsWith('/api/')) return J({ok: true});
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8345, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined});
  const p = await b.newPage({viewport: {width: 1280, height: 900}}); const errs = []; p.on('pageerror', e => errs.push(String(e.stack || e).slice(0, 300)));
  await p.goto('http://127.0.0.1:8345/'); await p.waitForFunction(() => typeof krSection == 'function' && typeof settings == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  const open = async () => { await q("(()=>{wins.slice().forEach(w=>w.close());settings('priv');return 1})()"); await wait(900) };
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});USER=USER||ACCTS[0];return 1})()");
  await open();
  const rows = await q("[...document.querySelectorAll('.pkr60 tbody tr')].map(r=>[...r.cells].map(c=>c.textContent))");
  ok('1. Privacy › Saved passwords for apps: each item, who saved it and what', rows.length == 3 && rows[0][0] == 'Claude' && rows[0][1] == 'Claude Safe Storage' && rows[1][0] == 'Chromium-based app (its saved logins)' && rows[2][0] == 'org.example.Chat (Flatpak app)', rows);
  ok('...after the camera, before App permissions', await q("(()=>{const k=document.querySelector('.pkr60');return !!k&&k.previousElementSibling&&k.previousElementSibling.classList.contains('pcam')})()"));
  ok('...an app\'s label shown as text, never run as HTML', !(await q('window.__pwn')) && /<img/.test(rows[1][1]), rows[1]);
  await q("document.querySelectorAll('.pkr60 .pkr')[0].click()"); await wait(300);
  ok('2. Remove asks first, naming it', /Remove "Claude Safe Storage"\? Claude will be signed out/.test(await q("(document.querySelector('.dqa')||{}).textContent||''")));
  await q("[...document.querySelectorAll('.rmodal button')].find(b=>b.textContent=='Cancel').click()"); await wait(200);
  ok('...Cancel: nothing removed', M.forgot.length == 0);
  await q("document.querySelectorAll('.pkr60 .pkr')[0].click()"); await wait(300); await q("[...document.querySelectorAll('.rmodal button')].find(b=>b.textContent=='Remove').click()"); await wait(700);
  ok('...Remove: that one forgotten (keyring/forget with its id), the list read again', M.forgot[0] == '/org/freedesktop/secrets/collection/login/1' && (await q("document.querySelectorAll('.pkr60 tbody tr').length")) == 2 && /Removed/.test(await q("document.querySelector('.pkr60 .pkm').textContent")), [M.forgot, await q("document.querySelector('.pkr60 .pkm').textContent")]);
  await q("document.querySelector('.pkr60 .pka').click()"); await wait(300); await q("[...document.querySelectorAll('.rmodal button')].find(b=>b.textContent=='Remove all').click()"); await wait(900);
  ok('3. Remove all: each asked for; one the keyring would not remove is said, the others gone', M.forgot.includes('app:org.example.Chat') && M.forgot.some(x => x.endsWith('/2')) && /1 could not be removed/.test(await q("document.querySelector('.pkr60 .pkm').textContent")) && (await q("document.querySelectorAll('.pkr60 tbody tr').length")) == 1, [M.forgot, await q("document.querySelector('.pkr60 .pkm').textContent")]);
  M.items = []; await open();
  ok('4. nothing saved: said, Remove all off', /No app has saved a password yet/.test(await q("document.querySelector('.pkr60 tbody').textContent")) && await q("document.querySelector('.pkr60 .pka').disabled"));
  M.keyring = false; await open();
  ok('...the keyring not running: said', /not running now/.test(await q("document.querySelector('.pkr60 tbody').textContent")));
  M.signedIn = false; await open();
  ok('...nobody signed in: said', /Sign in to an account/.test(await q("document.querySelector('.pkr60 tbody').textContent")));
  ok('5. no page errors', !errs.length, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
