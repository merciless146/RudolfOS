// 1.4.8: Rudolf = Runtime Driver Open Linux Framework (the L was Lua) on the sign-in screen, About, VER, the Prompt,
// and in a C:\README.TXT saved by an older version. node name48_t.js [CHROME]
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 300) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8099, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage();
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto('http://127.0.0.1:8099/'); await p.waitForFunction(() => typeof nameFix == 'function' && typeof fs != 'undefined' && fs['C:'], null, {timeout: 30000});
  const q = s => p.evaluate(s);
  const html = await q("document.documentElement.innerHTML");
  ok('1. nowhere on the page says "Open Lua Framework" except the one-time fix itself', (html.match(/Open Lua Framework/g) || []).length == 2, (html.match(/.{40}Open Lua Framework.{20}/g) || []));
  ok('2. the sign-in screen says Runtime Driver Open Linux Framework', await q("(document.getElementById('login')||document.body).textContent.includes('Runtime Driver Open Linux Framework')"));
  await q("fs['C:']['README.TXT']='Welcome to Rudolf OS!\\n\\nRudolf stands for Runtime Driver Open Lua Framework.\\n\\nMy own note.';1");
  const fixed = await q("[nameFix(),fs['C:']['README.TXT']]");
  ok('3. a README.TXT saved by an older version is updated, the person\'s own text kept', fixed[0] === true && /Open Linux Framework/.test(fixed[1]) && !/Open Lua/.test(fixed[1]) && /My own note/.test(fixed[1]), fixed);
  ok('4. ...once (nothing to do the second time)', await q("nameFix()") === false);
  await q("(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});USER=USER||ACCTS[0];return 1})()");
  const about = await q("(()=>{const h=HELP.find(h=>h[0]=='about');return h?h[2]():''})()");
  ok('5. About Rudolf OS says Runtime Driver Open Linux Framework', /Runtime Driver Open Linux Framework/.test(about), about.slice(0, 200));
  ok('6. no page errors', !errs.length, errs);
  console.log(`${R.filter(x => x).length}/${R.length} passed`); await b.close(); srv.close(); process.exit(R.every(x => x) ? 0 : 1);
})();
