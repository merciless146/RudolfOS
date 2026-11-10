// 1.5.7 (plan stage 2): Blitzen's reference material (blitzen-docs.json, built with the desktop from docs/ and the
// CHANGELOG, and the Help pages) found by words, and this computer's facts (rudolfd's ai/facts and the Event Viewer).
// node blitzdiag57_t.js [CHROME]  (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 600) : '')) };
const T = {'.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm', '.json': 'application/json'};
const srv = http.createServer((q, s) => { const u = decodeURIComponent(q.url.split('?')[0]);
  const f = path.join(WWW, u == '/' ? 'index.html' : path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
  s.writeHead(200, {'content-type': T[path.extname(f)] || 'application/octet-stream'}); fs.createReadStream(f).pipe(s) }).listen(8164, '127.0.0.1');
(async () => {
  const j = JSON.parse(fs.readFileSync(path.join(WWW, 'blitzen-docs.json'), 'utf8'));
  ok('1. blitzen-docs.json is built with the desktop: the plans and the CHANGELOG, in sections of at most 200 words', j.version && j.docs.length > 150 && j.docs.some(d => d.s == 'CHANGELOG.md') && j.docs.some(d => d.s == 'docs/plan-1.5.6.md') && j.docs.every(d => d.t.split(' ').length <= 200), j.docs.length);
  ok('2. ...without the release secrets\' names or the development link', !j.docs.some(d => /RPK_SIGNING_KEY|RUDOLF_SB_KEY|RPK_LIBRARY_KEY|devlink/i.test(d.t)));
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await (await b.newContext()).newPage();
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.addInitScript(() => {
    window.RUDOLFD = {token: 'x', base: '', boot: 'uefi', media: 'disk', live: 0, recovery: 0}; window.FACTS = 0;
    const f0 = window.fetch;
    window.fetch = (u, o) => { u = String(u); const J = x => Promise.resolve(new Response(JSON.stringify(Object.assign({ok: true}, x)), {headers: {'content-type': 'application/json'}}));
      const q = new URL(u, location.href);
      if (q.pathname == '/api/ai/facts') { FACTS++; return J({version: '1.5.7', uptime: 120, health: '{"k":"crash","m":"the browser restarted"}\n', rudolfd: 'rudolfd: started\n', page: null}) }
      if (u.includes('/api/')) return J({items: [], apps: [], windows: []});
      return f0(u, o) } });
  const q = s => p.evaluate(s);
  await p.goto('http://127.0.0.1:8164/'); await p.waitForFunction(() => typeof bzDocs == 'function' && typeof HELP != 'undefined', null, {timeout: 30000});
  const n = await q('bzDocs().then(d=>d.length)');
  ok('3. bzDocs: the documents and the Help pages', n > j.docs.length && await q("BZD.docs.some(d=>d.help=='privacy')"), n);
  const top = s => q(`bzSearch(${JSON.stringify(s)},3).map(d=>d.s+' | '+d.h+' | '+d.t.slice(0,80))`);
  let r = await top('the battery stops charging at 60 % instead of 70 %');
  ok('4. a question about the battery limit finds where it is described', r.length && /battery/i.test(r[0]), r);
  r = await top('why does the computer show a white screen and go back to the sign-in screen when I close Steam?');
  ok('5. the white screens with Steam find the watchdog\'s fix', r.some(x => /screen white|white screen|watchdog/i.test(x)), r);
  r = await top('how do I turn on the screen reader');
  ok('6. a Help page is found for an everyday question', r.some(x => /^Help: Accessibility/.test(x)), r);
  ok('7. nothing for words no document has', (await q("bzSearch('zzqx wqqv',3).length")) == 0);
  const f = await q("(async()=>{SLOG.sys.push({t:Date.now()-1000,m:'Signed in as Ann'});SLOG.fw.push({t:Date.now()-9*864e5,m:'old'});return await bzFacts(3)})()");
  ok('8. bzFacts: rudolfd\'s facts and the Event Viewer\'s recent events', f.version == '1.5.7' && /browser restarted/.test(f.health) && /sys Signed in as Ann/.test(f.events) && !/old/.test(f.events), f);
  ok('9. search and facts are quick (the index is built once)', await q("(()=>{const t=performance.now();for(let i=0;i<20;i++)bzSearch('downloads folder not found after update',4);return (performance.now()-t)/20<100})()"));
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
