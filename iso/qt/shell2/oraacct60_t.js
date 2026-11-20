// 1.6.0 (plan-1.6.0 Part 21): Oracle's websites per account by its ID (not its name), Guest's in memory and cleared at
// its sign-out, a deleted account's forgotten, everyone's pre-1.6.0 websites forgotten once with a notice to the accounts
// that had them, and "Reopen my tabs next time". A stand-in shell records what the desktop asks of it.
//   node oraacct60_t.js [CHROME]   (build iso/www first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const WWW = path.join(__dirname, '../../../iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 500) : '')) };
const srv = http.createServer((q, s) => { s.writeHead(200, {'content-type': 'text/html'}); s.end(fs.readFileSync(path.join(WWW, 'index.html'))) }).listen(8146, '127.0.0.1');
const wait = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined}); const p = await b.newPage({viewport: {width: 1280, height: 800}});
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto('http://127.0.0.1:8146/'); await p.waitForFunction(() => typeof rtPart == 'function' && typeof acctNew == 'function' && IDB && IDB.db, null, {timeout: 30000});
  const q = s => p.evaluate(s);
  await q(`(async()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});
    window.M=[];SHELL={postMessage:m=>M.push(m)};SHELL_CAPS=['tabs'];
    window.IN=async(n,pw)=>{const a=ACCTS.find(x=>x.n==n);if(USER)userLogout();USER=a;await userLogin(a,pw);return true};
    window.OUT=async()=>{fsFlush();await homeChain;userLogout();await homeChain;USER=null};
    delete SYSC.web160;delete SYSC.web160had;
    ACCTS.length=0;ACCTS.push({n:'boss',h:await pwMake('b1'),r:'admin'});saveUser();return 1})()`);
  await q("IN('boss','b1')"); await q("(async()=>{ACCTS.push(await acctNew('kid','k1','user'));saveUser()})()");
  const ids = await q("ACCTS.map(a=>a.id)");
  ok('1. the partition is the account\'s ID, not its name', await q("rtPart()") == 'a' + ids[0].replace(/[^a-z0-9]/gi, '') && !/u[0-9a-z]+$/.test(await q("rtPart()")), await q("rtPart()"));
  await wait(3600);
  const cl = await q("M.filter(m=>m.op=='clear').map(m=>m.part)");
  const olds = await q("['boss','kid','guest'].map(rtPartOld)");
  ok('2. once: every account\'s pre-1.6.0 websites forgotten (boss, kid, guest)', olds.every(x => cl.includes(x)), {cl, olds});
  const n1 = await q("NC.filter(n=>/kept separately for each account/.test((n.t||'')+' '+(n.b||''))).length");
  ok('3. ...and the account that had them is told once', n1 == 1, await q("NC.slice(0,3)"));
  await q("M.length=0;OUT()"); await q("IN('boss','b1')"); await wait(3600);
  ok('4. not again at the next sign-in', !(await q("M.some(m=>m.op=='clear')")) && await q("NC.filter(n=>/kept separately/.test((n.t||'')+' '+(n.b||''))).length") <= 1);
  // a new account made after the update: no notice
  await q("(async()=>{ACCTS.push(await acctNew('new','n1','user'));saveUser();await OUT()})()"); await q("IN('new','n1')"); await wait(3600);
  ok('5. an account made after the update is not told', await q("NC.filter(n=>/kept separately/.test((n.t||'')+' '+(n.b||''))).length") == 0);
  // Guest
  await q("OUT()"); await q("(()=>{USER={n:'Guest',r:'guest'};return 1})()");
  const g1 = await q("rtPart()"); const g1b = await q("rtPart()");
  await q("M.length=0;userLogout();USER=null"); const gcl = await q("M.filter(m=>m.op=='clear').map(m=>m.part)");
  await q("USER={n:'Guest',r:'guest'}"); const g2 = await q("rtPart()"); await q("userLogout();USER=null");
  ok('6. Guest: in memory (priv-), the same through its session, cleared at sign-out, new the next time', /^priv-g/.test(g1) && g1 == g1b && gcl.includes(g1) && g2 != g1, {g1, g2, gcl});
  // a deleted account's websites
  await q("IN('boss','b1')"); await q("M.length=0");
  const kidPart = 'a' + ids[1].replace(/[^a-z0-9]/gi, '');
  await q(`(()=>{const a=ACCTS.find(x=>x.n=='kid');rtForget(a);return 1})()`);
  ok('7. a deleted account\'s websites forgotten', await q("M.filter(m=>m.op=='clear').map(m=>m.part)").then(l => l.includes(kidPart)), await q("M"));
  // reopen tabs
  await q("S.oreopen=true;persistS()");
  await q("browser('https://example.com/a')"); await wait(400);
  await q("(()=>{const W=wins.filter(o=>/Oracle/.test(o.title||o.el.textContent)).pop();W.oNewTab('https://example.org/b')})()"); await wait(400);
  await q("(()=>{const W=wins.filter(o=>/Oracle/.test(o.title||o.el.textContent)).pop();W.close?W.close():W.el.querySelector('.x,.close').click()})()"); await wait(500);
  const kept = await q("S.otabs");
  ok('8. closing Oracle keeps its tabs when "Reopen my tabs next time" is on', Array.isArray(kept) && kept.includes('https://example.com/a') && kept.includes('https://example.org/b'), kept);
  await q("M.length=0;browser()"); await wait(600);
  const opened = await q("M.filter(m=>m.op=='open').map(m=>m.url)");
  ok('9. ...and the next Oracle window opens them', opened.includes('https://example.com/a') && opened.includes('https://example.org/b'), opened);
  const menu = await q("__oracleMenu({url:''}).map(x=>x[0])");
  ok('10. Oracle\'s ☰ menu: ✓ Reopen my tabs next time', menu.includes('✓ Reopen my tabs next time'), menu);
  await q("M.length=0;browser(null,{priv:1})"); await wait(500);
  ok('11. a private window does not reopen them', !(await q("M.filter(m=>m.op=='open').map(m=>m.url)")).includes('https://example.com/a'));
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
