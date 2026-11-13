// 1.5.9: Search in Settings: the box at the top of Settings, its drop-down narrowing as each letter is typed, each
// result with its place underneath, and a result opening its page with the setting shown. A stand-in rudolfd.
//   node setsearch59_t.js [CHROME]          the test
//   node setsearch59_t.js --write [CHROME]  also writes desktop/settings-index.json (what the build puts in the page),
//                                            from every page as it shows here; then rebuild (build19.py)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www'), IDX = path.join(__dirname, '../../../desktop/settings-index.json');
const WRITE = process.argv.includes('--write'), CH = process.argv.slice(2).find(a => !a.startsWith('--'));
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const srv = http.createServer((q, s) => { const [u0] = q.url.split('?'); const u = decodeURIComponent(u0);
  q.on('data', () => {}); q.on('end', () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'});
      return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0};</script>')) }
    if (u.startsWith('/a/')) { const f = path.join(WWW, u); if (fs.existsSync(f)) { s.writeHead(200); return s.end(fs.readFileSync(f)) } }
    if (u.startsWith('/api/')) { if (u == '/api/events/wait') return setTimeout(() => { s.writeHead(200); s.end('{"ok":true,"events":[]}') }, 20000);
      s.writeHead(200, {'content-type': 'application/json'}); return s.end('{"ok":true}') }
    s.writeHead(404); s.end() }) }).listen(8282, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: CH || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 900}});
  const errs = []; p.on('pageerror', e => { if (/\bss[A-Z]\w*|settings/.test(e.stack || '')) errs.push(e.message + ' ' + (e.stack || '').split('\n')[1]) });   // (other pages' errors with this bare stand-in are theirs)
  await p.goto('http://127.0.0.1:8282/'); await p.waitForFunction(() => typeof ssAttach == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});ACCTS=[{n:'Ann',r:'admin',h:'x'}];USER=ACCTS[0];return 1})()`);
  await q('settings()'); await wait(800);
  const top = await q("(()=>{const b=SW.body.firstElementChild;return b&&b.classList.contains('ssbar')&&b.querySelector('input.ssq').placeholder})()");
  ok('1. a search box at the top of Settings', top == 'Search settings', top);
  // every page shown once: what Search learns from each (and, with --write, the index the build puts in the page)
  const keys = await q('Object.keys(SW.SEC)');
  for (const k of keys) { await q(`SW.show('${k}',1)`); await wait(1700) }
  const live = await q('JSON.parse(JSON.stringify(SSLIVE))');
  if (WRITE) { const o = {}; for (const k of keys) o[k] = live[k] || []; fs.writeFileSync(IDX, JSON.stringify(o, null, 0).replace(/\],\[/g, '],\n[').replace(/":\[/g, '":\n[') + '\n'); console.log('wrote ' + IDX + ' (' + keys.map(k => o[k].length).reduce((a, x) => a + x, 0) + ' entries)') }
  const idx = fs.existsSync(IDX) ? JSON.parse(fs.readFileSync(IDX, 'utf8')) : {};
  const miss = keys.filter(k => (live[k] || []).length && !(idx[k] && idx[k].length)), stale = [];
  for (const k of keys) for (const e of live[k] || []) if (!(idx[k] || []).some(x => x[0] == e[0] && x[1] == e[1])) stale.push(k + ': ' + e[0]);
  const tot = keys.reduce((a, k) => a + (live[k] || []).length, 0);
  ok('2. desktop/settings-index.json has every page that shows settings here, and nearly all of what they show (else: --write)', !miss.length && stale.length <= Math.max(3, tot * 0.05), {miss, stale: stale.slice(0, 20), tot});
  await q("SW.show('disp',1)"); await wait(600);
  const type = async v => { await p.fill('.ssbar .ssq', v); await wait(150);
    return q("[...document.querySelectorAll('.ssbar .ssr')].map(r=>[r.querySelector('.sst').textContent,r.querySelector('.ssp').textContent])") };
  const r1 = await type('w'), r2 = await type('wi'), r3 = await type('wi-f'), r4 = await type('wifi');
  ok('3. typing shows the drop-down, each result with its place underneath ("Settings › …")', r2.length > 0 && await q("!document.querySelector('.ssbar .ssdd').hidden") && r2.every(x => /^Settings( › |$)/.test(x[1])), r2.slice(0, 5));
  const key = x => x.join('|'), sub = (a, bb) => a.every(x => bb.some(y => key(y) == key(x)));
  ok('4. each letter only takes results away (process of elimination)', r2.length <= r1.length && sub(r2, r1) && r3.length <= r2.length && sub(r3, r2), [r1.length, r2.length, r3.length]);
  ok('5. "wifi" finds the Wi-Fi settings, the Network page among them', r4.some(x => /Wi-Fi/.test(x[0])) && r4.some(x => /Network Settings/.test(x[0]) && x[1] == 'Settings'), r4.slice(0, 8));
  const bt = await type('blue');
  ok('6. a category: "blue" lists the Bluetooth page first', bt.length && /Bluetooth/.test(bt[0][0]) && bt[0][1] == 'Settings', bt.slice(0, 3));
  const md = await q("document.querySelector('.ssbar .ssr .sst').innerHTML");
  ok('7. ...the typed letters in bold', /<b>Blue<\/b>/.test(md), md);
  // a setting deeper in a page: opened, scrolled to, lit
  const deep = await q("(()=>{const L=ssAll(SW.SEC).filter(e=>!e.page&&!e.h&&e.s&&e.k=='acc');return L.length?L[Math.floor(L.length/2)]:null})()");
  const r5 = await type(deep.t);
  ok('8. a setting: found by its name, with its page and section underneath', r5.some(x => x[0].replace(/^\S+ /, '') == deep.t || x[0] == deep.t) && r5.some(x => x[1] == 'Settings › Accessibility › ' + deep.s || x[1].startsWith('Settings › ') && x[1].endsWith(' › ' + deep.s)), {deep, r5: r5.slice(0, 5)});
  const i5 = r5.findIndex(x => x[0] == deep.t && x[1].endsWith(' › ' + deep.s));
  for (let i = 0; i < i5; i++) await p.keyboard.press('ArrowDown');
  await p.keyboard.press('Enter'); await wait(900);
  const op = await q(`(()=>{const h=[...SW.body.querySelectorAll('.sc .sshit')];return [SW.sec,h.map(x=>ssClean(x)),document.querySelector('.ssbar .ssdd').hidden]})()`);
  ok('9. ...Enter opens its page with the setting lit, and the drop-down closes', op[0] == 'acc' && op[1].includes(deep.t) && op[2], {op, deep});
  const nn = await type('zzqx');
  ok('10. nothing matches: said so', !nn.length && /No settings match “zzqx”/.test(await q("document.querySelector('.ssbar .ssdd').textContent")));
  await type('sound'); await p.keyboard.press('Escape'); await wait(100);
  ok('11. Esc closes the drop-down (and Settings stays open)', await q("document.querySelector('.ssbar .ssdd').hidden&&wins.includes(SW)"));
  const r6 = await type('sound'); await p.click('.ssbar .ssr:nth-child(1)'); await wait(700);
  ok('12. a click opens a result (Sound: Volume Controls)', await q("SW.sec") == 'vol', [r6.slice(0, 2), await q('SW.sec')]);
  ok('13. no page errors from Search or Settings', errs.length == 0, errs);
  await b.close(); srv.close();
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
