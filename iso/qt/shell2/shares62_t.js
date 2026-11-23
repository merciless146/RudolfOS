// 1.6.2 (docs/plan-1.6.2.md Part 35): network shares in File Explorer (desktop/shares62.js and its hooks in files25.js) with a
// stand-in rudolfd: the Network place, the computers and NAS boxes found (their names as text, never HTML), signing in to one
// (a wrong password said, then the right one), its shared folders, one opened as a folder (its files listed; copied into
// Rudolf OS, copied onto it, dropped onto the desktop), "Remember the password" asked for, the password in no request's
// address and nowhere in the page afterwards, the open share in the places, Disconnect, the recent shares reconnected only when
// clicked (none asked for at start), an address typed (\\server\share, smb://server/share), an SMB 1 server said plainly.
// Run twice: in Chromium as Standard's page, and in WebKit (Native's engine, WebKitGTK) as Native's app window
// (index.html#app=explorer, the same page alone); then the two Network views compared part by part (the same places, cards,
// texts and buttons, in the same order, of the same sizes within 15 %).
//   node shares62_t.js      (build iso/www first; port 8650, or SHARES62_PORT; SHARES62_WEBKIT=0 for Chromium only)
const pw = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 900) : '')) };
const T = {'.wasm': 'application/wasm', '.js': 'text/javascript', '.html': 'text/html', '.webp': 'image/webp', '.jpg': 'image/jpeg', '.woff2': 'font/woff2', '.woff': 'font/woff'};
const PORT = +process.env.SHARES62_PORT || 8650, URL0 = 'http://127.0.0.1:' + PORT + '/';
const PW = 'Pw-' + Math.random().toString(36).slice(2, 10) + ',x=1';   // (made for this run)
const SP = '/run/rudolf/shares/1001/0123456789abcdef';
const M = {calls: [], bodies: {}, open: [], kept: false, files: {[SP + '/onserver.txt']: 'from the NAS\n'}};
const reset = () => Object.assign(M, {calls: [], bodies: {}, open: [], kept: false, files: {[SP + '/onserver.txt']: 'from the NAS\n'}});
const FOUND = {ok: true, servers: [{name: 'Nas62', ip: '192.168.62.2', host: 'nas62.local', how: 'mdns', group: ''}, {name: 'WIN<b>62', ip: '192.168.62.5', host: 'WIN<b>62', how: 'wsd', group: 'HOME62'}]};
const srv = http.createServer((q, s) => { const [u0, qs] = q.url.split('?'); const u = decodeURIComponent(u0); const sp = new URLSearchParams(qs || '');
  const bs = []; q.on('data', d => bs.push(d)); q.on('end', () => { const body = Buffer.concat(bs).toString();
    const J = (x, st) => { s.writeHead(st || 200, {'content-type': 'application/json'}); s.end(JSON.stringify(x)) };
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/api/')) { const a = u.slice(5); const c = q.method + ' ' + a + (qs ? '?' + qs : ''); M.calls.push(c); if (body) M.bodies[c] = body;
      if (a == 'events/wait') return setTimeout(() => J({ok: true, events: []}), 20000);
      if (a == 'desktop/session') return M.session ? J(M.session) : J({ok: false, error: 'no native session'}, 404);
      if (a == 'disks') return J({ok: true, disks: []});
      if (a == 'shares/found') return setTimeout(() => J(FOUND), 300);
      if (a == 'shares/open' && q.method == 'GET') return J({ok: true, uid: 1001, shares: M.open});
      if (a == 'shares/list') { const [us, p] = body.split('\n'); if (us == 'ann' && p == PW) return J({ok: true, dialect: '3.1.1', shares: [{name: 'docs', remark: 'Ann <i>docs</i>'}, {name: 'pub', remark: ''}]});
        return J({ok: false, error: 'The name or password is not right, or this name may not open it.', why: 'password'}, 401) }
      if (a == 'shares/open') { const [us, p] = body.split('\n'); const srvn = sp.get('server'), sh = sp.get('share');
        if (/old62/.test(srvn)) return J({ok: false, error: 'old62 speaks only SMB 1, which is old and unsafe, so Rudolf OS never uses it. Turn on SMB 2 or 3 on it (on a NAS, in its file services settings).', why: 'smb1'}, 426);
        const good = sp.get('guest') == '1' || (us == 'ann' && (p == PW || (!p && M.kept)));
        if (!good) return J({ok: false, error: 'The name or password is not right, or this name may not open it.', why: 'password'}, 401);
        if (sp.get('keep') == '1') M.kept = true;
        const sh1 = {id: '0123456789abcdef', server: srvn, share: sh, user: us || '-', ver: '3', encrypted: true, guest: false, ip: sp.get('ip') || '', path: SP};
        M.open = [sh1]; return J({ok: true, id: sh1.id, path: SP, server: srvn, share: sh, ip: sh1.ip, ver: '3', encrypted: true, kept: M.kept, already: false}) }
      if (a == 'shares/close') { M.open = []; return J({ok: true, message: 'disconnected'}) }
      if (a == 'fs/list') { const p = sp.get('path'); const items = Object.keys(M.files).filter(f => f.startsWith(p + '/') && !f.slice(p.length + 1).includes('/')).map(f => ({name: f.slice(p.length + 1), dir: false, size: M.files[f].length, mtime: 1795000000000}));
        return J({ok: true, items}) }
      if (a == 'fs/read') { const f = M.files[sp.get('path')]; if (f == null) return J({ok: false, error: 'not found'}, 404); s.writeHead(200, {'content-type': 'application/octet-stream'}); return s.end(f) }
      if (a == 'fs/write') { M.files[sp.get('path')] = body; return J({ok: true, message: 'saved', mtime: Date.now(), size: body.length}) }
      if (a == 'fs/hash') { const f = M.files[sp.get('path')] || ''; return J({ok: true, sha256: require('crypto').createHash('sha256').update(f).digest('hex'), size: f.length}) }
      if (a == 'fs/tree') return J({ok: true, items: []});
      if (a == 'home') return J({ok: true, supported: false, accounts: []});
      if (/^sys\//.test(a)) return J({ok: false, error: 'not in this test'}, 404);
      return J({ok: true, message: 'done'}) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }) }).listen(PORT, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
async function until(pg, js, ms) { const t = Date.now() + (ms || 8000); for (;;) { let v; try { v = await pg.evaluate(js) } catch (e) { v = null } if (v || Date.now() > t) return v; await wait(150) } }
const VIEW = `(()=>{const W=[...document.querySelectorAll('.win')].find(w=>w.querySelector('.xp25'));if(!W)return null;const b=W.querySelector('.xp25');
  const r=e=>{const x=e.getBoundingClientRect();return [Math.round(x.width),Math.round(x.height)]};
  return {side:[...b.querySelectorAll('.xside .xsh,.xside .xsi')].map(e=>e.textContent.trim()),cards:[...b.querySelectorAll('.xnc')].map(e=>e.textContent.trim()),
   sizes:[...b.querySelectorAll('.xnc,.xna,.xngo,.xnrf')].map(r),addr:b.querySelector('.ad').value,stat:b.querySelector('.stat').textContent,
   rows:[...b.querySelectorAll('tr[data-k]')].map(e=>e.textContent),bar:[...b.querySelectorAll('.xrb button')].filter(x=>x.offsetParent).map(x=>x.textContent),
   html:[...b.querySelectorAll('.xnc')].map(e=>e.innerHTML).join('')}})()`;
const DLG = `(()=>{const o=[...document.querySelectorAll('.rmodal')].pop();if(!o)return null;return {t:o.querySelector('h3').textContent,w:(o.querySelector('.nw')||{}).textContent||'',btn:[...o.querySelectorAll('.owb')].map(b=>b.textContent),html:o.innerHTML}})()`;
async function flow(pg, tag) {
  reset(); const view = () => pg.evaluate(VIEW);
  await until(pg, `!!(document.querySelector('.xp25'))`, 15000);
  await pg.evaluate(`(()=>{localStorage.removeItem(netRecK());NET.open=[];NET.found=null;const W=[...document.querySelectorAll('.win')].find(w=>w.querySelector('.xp25'));W&&W.refresh&&W.refresh()})()`); M.calls = [];
  let v = await view();
  ok(`${tag} 1. File Explorer's places: Network, after Drives (nothing asked of the network yet: no shares/found, no reconnection at start)`, v && v.side.includes('Network') && v.side.includes('🌐Network') && !M.calls.some(c => /shares\/(found|list)|POST shares\/open/.test(c)), [v && v.side, M.calls]);
  await pg.evaluate(`[...document.querySelectorAll('.xsi')].find(e=>e.textContent.trim()=='🌐Network').click()`);
  v = await until(pg, `(${VIEW.slice(0, -2)})()&&document.querySelectorAll('.xnc').length>=2&&(${VIEW})`);
  ok(`${tag} 2. the Network view: an address box, the computers and NAS boxes found (shares/found), a Windows computer's name with "<b>" in it shown as text`, v && v.cards.length == 2 && /Nas62NAS or computer · 192\.168\.62\.2/.test(v.cards[0]) && /WIN<b>62Windows computer · 192\.168\.62\.5 · HOME62/.test(v.cards[1]) && /WIN&lt;b&gt;62/.test(v.html) && v.addr == 'Network' && M.calls.includes('GET shares/found'), v);
  // a NAS box: signing in (a wrong password, then the right one), its shares
  await pg.evaluate(`document.querySelector('.xnc').click()`); let d = await until(pg, DLG);
  ok(`${tag} 3. a NAS box clicked: the sign-in box (name, password, guest, remember, More options: domain and SMB 2.1)`, d && /Connect to \\\\nas62\.local/.test(d.t) && /class="np" type="password"/.test(d.html) && /nk/.test(d.html) && /n21/.test(d.html), d);
  const fill = (u, p, keep) => pg.evaluate(`(()=>{const o=[...document.querySelectorAll('.rmodal')].pop();o.querySelector('.nu').value=${JSON.stringify(u)};o.querySelector('.np').value=${JSON.stringify(p)};o.querySelector('.nk').checked=${!!keep};o.querySelector('.o').click()})()`);
  await fill('ann', 'wrong'); d = await until(pg, `(()=>{const d=${DLG};return d&&d.w?d:null})()`);
  ok(`${tag} 4. a wrong password: said in the box, which asks again`, d && /not right/.test(d.w), d);
  await fill('ann', PW, true); d = await until(pg, `(()=>{const d=${DLG};return d&&d.btn.length?d:null})()`);
  ok(`${tag} 5. the right one: its shared folders (shares/list over SMB 3), a remark shown as text`, d && d.btn.length == 2 && /docs — Ann <i>docs<\/i>/.test(d.btn[0]) && /&lt;i&gt;/.test(d.html) && M.calls.some(c => c == 'POST shares/list?server=nas62.local&ip=192.168.62.2'), d);
  await pg.evaluate(`[...document.querySelectorAll('.rmodal .owb')][0].click()`);
  v = await until(pg, `(()=>{const v=${VIEW};return v&&v.rows.length?v:null})()`);
  const oc = M.calls.find(c => /^POST shares\/open\?/.test(c)) || '';
  ok(`${tag} 6. docs opened as a folder: its files, its address \\\\nas62.local\\docs, encrypted said; Remember asked for (keep=1)`, v && v.rows.some(r => /onserver\.txt/.test(r)) && v.addr == '\\\\nas62.local\\docs' && v.side.includes('🔒\\\\nas62.local\\docs') && /server=nas62\.local&ip=192\.168\.62\.2&share=docs&keep=1$/.test(oc), [v, oc]);
  ok(`${tag} 7. the password only in the requests' bodies (name, password, domain), never in an address; nowhere in the page or its storage afterwards`, M.bodies[oc] == `ann\n${PW}\n\n` && !M.calls.some(c => c.includes(PW.slice(0, 8))) && !(await pg.evaluate(`(document.documentElement.outerHTML+JSON.stringify(localStorage)+[...document.querySelectorAll('input')].map(i=>i.value).join('')).includes(${JSON.stringify(PW)})`)), M.bodies);
  ok(`${tag} 8. the open share in the places (🔒 \\\\nas62.local\\docs) and Disconnect in its bar`, v && v.side.some(x => x == '🔒\\\\nas62.local\\docs') && v.bar.includes('⏏ Disconnect'), v);
  // copied each way, and onto the desktop
  await pg.evaluate(`(()=>{const tr=[...document.querySelectorAll('tr[data-k]')].find(t=>/onserver/.test(t.textContent));tr.click();[...document.querySelectorAll('.xrb button')].find(b=>/Copy selected into Rudolf OS/.test(b.textContent)).click()})()`);
  const got = await until(pg, `(()=>{const d=node(downloadsPath());return d&&Object.keys(d).find(k=>/ONSERVER/i.test(k))?String(d[Object.keys(d).find(k=>/ONSERVER/i.test(k))]):null})()`, 10000);
  ok(`${tag} 9. copied into Rudolf OS (Downloads): read from the share's folder (fs/read)`, /from the NAS/.test(got || '') && M.calls.some(c => c.startsWith('GET fs/read?path=' + encodeURIComponent(SP + '/onserver.txt'))), [got, M.calls.filter(c => /fs\//.test(c))]);
  await until(pg, `[...document.querySelectorAll('.xrb button')].some(b=>/Copy Rudolf OS clipboard here/.test(b.textContent))&&!/Copying/.test((document.querySelector('.xp25 .stat')||{}).textContent)`);
  await pg.evaluate(`(()=>{const p=homePath('DOCUMENTS');node(p)['LETTER.TXT']='to the NAS';fsSave();FC.op='copy';FC.items=[[...p,'LETTER.TXT']];FC.r=null;try{fcPut()}catch(e){}[...document.querySelectorAll('.xrb button')].find(b=>/Copy Rudolf OS clipboard here/.test(b.textContent)).click()})()`);
  const put = await until(pg, `(()=>1)()`, 100) && await (async () => { for (let i = 0; i < 40; i++) { const k = Object.keys(M.files).find(f => /letter\.txt$/i.test(f) && f.startsWith(SP)); if (k) return k + '=' + M.files[k]; await wait(200) } return null })();
  ok(`${tag} 10. copied onto the share (Paste): written into its folder (fs/write)`, /letter\.txt=to the NAS$/i.test(put || ''), [put, M.calls.filter(c => /fs\//.test(c))]);
  await pg.evaluate(`(()=>{const W=[...document.querySelectorAll('.win')].find(w=>w.querySelector('.xp25'));dkPut({dragR:{r:{dev:'share:0123456789abcdef',root:${JSON.stringify(SP)},path:${JSON.stringify(SP)},label:'\\\\\\\\nas62.local\\\\docs',ro:false},items:[{name:'onserver.txt',dir:false,size:13,mtime:1795000000000}]}},homePath('DESKTOP'),true,null)})()`).catch(e => null);
  const dk = await until(pg, `(()=>{const d=node(homePath('DESKTOP'));return d&&Object.keys(d).find(k=>/ONSERVER/i.test(k))||null})()`, 10000);
  ok(`${tag} 11. dragged onto the desktop (the desktop's drop of a drive pane's items): the file is on the desktop`, !!dk, dk);
  // Disconnect; the recent share, reconnected only when clicked, with the password kept
  if (!(await until(pg, `[...document.querySelectorAll('.xrb button')].some(b=>/Disconnect/.test(b.textContent))`, 4000))) {
    if (process.env.SH62_DBG) console.log('DBG before Disconnect', JSON.stringify(await view()));
    await pg.evaluate(`[...document.querySelectorAll('.xsi')].find(e=>/^🔒/.test(e.textContent.trim())).click()`);   /* (the pane went elsewhere: the share opened again from the places) */
    await until(pg, `[...document.querySelectorAll('.xrb button')].some(b=>/Disconnect/.test(b.textContent))`) }
  await pg.evaluate(`[...document.querySelectorAll('.xrb button')].find(b=>/Disconnect/.test(b.textContent)).click()`);
  v = await until(pg, `(()=>{const v=${VIEW};return v&&!v.bar.length&&!v.side.some(x=>/nas62/.test(x))?v:null})()`);
  ok(`${tag} 12. Disconnect: shares/close, the pane back home, gone from the places`, v && M.calls.some(c => c == 'POST shares/close?id=0123456789abcdef'), [v, M.calls.slice(-4)]);
  const rec = await pg.evaluate(`localStorage.getItem(netRecK())`);
  ok(`${tag} 13. the account's recent shares kept (server, share, name; no password)`, /"server":"nas62\.local","share":"docs","user":"ann"/.test(rec || '') && !(rec || '').includes(PW), rec);
  await pg.evaluate(`[...document.querySelectorAll('.xsi')].find(e=>e.textContent.trim()=='🌐Network').click()`);
  v = await until(pg, `(()=>{const v=${VIEW};return v&&v.cards.some(c=>/not connected/.test(c))?v:null})()`);
  ok(`${tag} 14. Network: "Recent shares" with \\\\nas62.local\\docs, not connected (nothing opened by itself)`, v && /\\\\nas62\.local\\docsAs ann · not connected/.test(v.cards[0]) && M.calls.filter(c => /^POST shares\/open/.test(c)).length == 1, v);
  const n0 = M.calls.length; await pg.evaluate(`[...document.querySelectorAll('.xnc')][0].click()`);
  v = await until(pg, `(()=>{const v=${VIEW};return v&&v.rows.length?v:null})()`);
  const rc = M.calls.slice(n0).find(c => /^POST shares\/open/.test(c));
  ok(`${tag} 15. clicked: reconnected with the kept password (none typed, no box asked)`, v && rc && M.bodies[rc] == 'ann\n\n\n' && !(await pg.evaluate(DLG)), [rc, rc && M.bodies[rc]]);
  // an address typed; an SMB 1 server
  await pg.evaluate(`(()=>{const a=[...document.querySelectorAll('.xp25 .ad')].pop();a.value='smb://old62/old';a.dispatchEvent(new KeyboardEvent('keydown',{key:'Enter'}))})()`);
  d = await until(pg, DLG);
  ok(`${tag} 16. smb://old62/old typed in the address bar: the sign-in box for \\\\old62\\old`, d && /\\\\old62\\old/.test(d.t), d);
  await pg.evaluate(`(()=>{const o=[...document.querySelectorAll('.rmodal')].pop();o.querySelector('.ng').checked=true;o.querySelector('.ng').dispatchEvent(new Event('change'));o.querySelector('.o').click()})()`);
  v = await until(pg, `(()=>{const v=${VIEW};return v&&/SMB 1/.test(v.stat)?v:null})()`);
  ok(`${tag} 17. ...as a guest: the server speaks only SMB 1, said plainly in the status line`, v && /old62 speaks only SMB 1, which is old and unsafe/.test(v.stat), v && v.stat);
  await pg.evaluate(`(()=>{const a=[...document.querySelectorAll('.xp25 .ad')].pop();a.value='\\\\\\\\WIN62\\\\Users';a.dispatchEvent(new KeyboardEvent('keydown',{key:'Enter'}))})()`);
  d = await until(pg, DLG); await pg.evaluate(`[...document.querySelectorAll('.rmodal')].pop().querySelector('.c').click()`);
  ok(`${tag} 18. \\\\WIN62\\Users typed: the sign-in box for it (Cancel: nothing asked)`, d && /\\\\WIN62\\Users/.test(d.t) && !M.calls.some(c => /WIN62/.test(c)), d);
  await pg.evaluate(`[...document.querySelectorAll('.xsi')].find(e=>e.textContent.trim()=='🌐Network').click()`);
  return await until(pg, `(()=>{const v=${VIEW};return v&&v.cards.length>=3?v:null})()`);
}
(async () => {
  const errs = []; const watch = p => p.on('pageerror', e => errs.push(e.message + ' @ ' + String(e.stack).split('\n').slice(1, 3).join(' ')));
  const setup = async (p) => { await p.waitForFunction(() => typeof acctNew == 'function' && typeof explorer == 'function', null, {timeout: 30000});
    return p.evaluate(`(async()=>{svj('bd95setup',1);['oobe'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
      const a=await acctNew('Ann','pw-one','admin');ACCTS=[a];saveUser();USER=a;await userLogin(a,'pw-one');fsSave();await new Promise(r=>setTimeout(r,800));
      const hex=u=>[...new Uint8Array(u)].map(x=>x.toString(16).padStart(2,'0')).join('');return {id:a.id,pk:hex(await pbkBits('pw-one',a.ks,256)),fk:hex(FKRAW)}})()`) };
  // Standard's page, in Chromium
  const b = await pw.chromium.launch(); const ctx = await b.newContext({viewport: {width: 1200, height: 820}});
  let p = await ctx.newPage(); watch(p); await p.goto(URL0); const acct = await setup(p);
  await p.evaluate('explorer()'); const vs = await flow(p, 'Standard');
  M.session = {ok: true, id: acct.id, n: 'Ann', r: 'admin', pk: acct.pk, fk: acct.fk};
  // Native's app window: the same page alone; in WebKit (Native draws its windows with WebKitGTK) unless asked not to
  let vn = null;
  const wk = process.env.SHARES62_WEBKIT != '0' ? await pw.webkit.launch().catch(e => { console.log('NOTE: no WebKit here: ' + e.message.split('\n')[0]); return null }) : null;
  const ctx2 = wk ? await wk.newContext({viewport: {width: 1200, height: 820}}) : ctx;
  p = await ctx2.newPage(); watch(p);
  reset(); if (wk) { await p.goto(URL0); await setup(p); await p.goto('about:blank') }
  await p.goto(URL0 + '#app=explorer'); await p.waitForFunction(() => typeof explorer == 'function' && typeof ONEAPP != 'undefined' && ONEAPP && USER, null, {timeout: 30000});
  await p.waitForSelector('.win.oamain', {timeout: 20000});
  vn = await flow(p, wk ? 'Native (WebKit)' : 'Native');
  // Native against Standard: the Network view part by part
  const same = vs && vn && JSON.stringify(vs.cards) == JSON.stringify(vn.cards) && JSON.stringify(vs.side) == JSON.stringify(vn.side) && vs.sizes.length == vn.sizes.length &&
    vs.sizes.every((s, i) => Math.abs(s[1] - vn.sizes[i][1]) <= Math.max(6, s[1] * 0.15)) &&
    /* (widths: the buttons' own; the address box and the cards fill the window, which is the app's whole window on Native:
       they differ by the windows' difference alone) */
    [1, 2].every(i => Math.abs(vs.sizes[i][0] - vn.sizes[i][0]) <= Math.max(8, vs.sizes[i][0] * 0.15)) &&
    [0, ...vs.sizes.map((x, i) => i).slice(3)].every(i => Math.abs((vn.sizes[i][0] - vs.sizes[i][0]) - (vn.sizes[0][0] - vs.sizes[0][0])) <= 4);
  ok(`Native and Standard: the same Network view (the places, the recent shares and the computers found, in the same order, with the same texts; each part's size within 15 %)`, same, vs && vn ? {cards: [vs.cards, vn.cards], side: vs.side.join() == vn.side.join() ? 'same' : [vs.side, vn.side], sizes: [vs.sizes, vn.sizes]} : [vs, vn]);
  ok('no page error', !errs.length, errs);
  await b.close(); if (wk) await wk.close(); srv.close(); const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
