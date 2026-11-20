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
  // the native desktop (1.6.0, the owner's native rule): an account deleted in a WebKit window, Oracle's Chromium told
  await q("(async()=>{ACCTS.push(await acctNew('gone','g1','user'));saveUser();return 1})()");
  const goneId = await q("ACCTS.find(a=>a.n=='gone').id"), gonePart = 'a' + goneId.replace(/[^a-z0-9]/gi, '');
  await q("window.NM=[];window.__S0=SHELL;SHELL=null;window.webkit={messageHandlers:{rudolfNative:{postMessage:m=>NM.push(m)}}};M.length=0;rtForget(ACCTS.find(a=>a.n=='gone'));1"); await wait(300);
  const lst = await q("JSON.parse(localStorage.getItem('bd95webclear')||'[]')");
  ok('12. native: an account deleted in a window of its own: its websites listed for Oracle (by ID and the old name-key) and Oracle told', lst.includes(gonePart) && lst.includes(await q("rtPartOld('gone')")) && await q("NM.some(m=>m.op=='webclear')") && !(await q("M.length")), {lst, nm: await q("NM")});
  await q("delete window.webkit;SHELL=__S0;M.length=0;1");
  const n12 = await q("__oaWebClearIn()");
  const cl12 = await q("M.filter(m=>m.op=='clear').map(m=>m.part)");
  ok('13. ...Oracle\'s page (its Chromium) clears them, and the list is emptied', n12 == 2 && cl12.includes(gonePart) && await q("localStorage.getItem('bd95webclear')") === null, {n12, cl12});
  ok('14. ...nothing again the next time', await q("__oaWebClearIn()") == 0);
  await q("(()=>{USER={n:'Guest',r:'guest'};window.__rtGuest=null;return 1})()"); const gp = await q("rtPart()");
  await q("window.SAVED=0;const o=window.__oraSave;window.__oraSave=()=>{SAVED++;o&&o()};M.length=0;1");
  await q("__oaLogout()");
  ok('15. native sign-out (rudolf-launch "logout"): Oracle\'s tabs saved, Guest\'s websites cleared', await q("SAVED") == 1 && (await q("M.filter(m=>m.op=='clear').map(m=>m.part)")).includes(gp) && await q("window.__rtGuest") === null, {gp, M: await q("M")});
  await q("USER=null;1");
  const src = f => fs.readFileSync(path.join(__dirname, '../../sys', f), 'utf8');
  const nat = src('rudolf-native.c'), lch = src('rudolf-launch.c');
  ok('16. the native shell: signing out tells Oracle (only Oracle, not the Tor windows) "logout"; a window\'s "webclear" passed on', /oracle_only\("logout\\n"\)/.test(nat) && /op, "webclear"\)\) oracle_only\("webclear\\n"\)/.test(nat));
  ok('17. rudolf-launch: "webclear" and "logout" run in every desktop page; Chromium closes after "logout"', /__oaWebClearIn&&__oaWebClearIn\(\)/.test(lch) && /__oaLogout&&__oaLogout\(\)/.test(lch) && /o_close_at = 0; o_nq = 0; send_cmd\("Browser.close"/.test(lch));
  // Part 22 item 5: Settings › Oracle › "Keep my websites encrypted with my account"
  await q(`(()=>{window.__H0=oweHasRD;window.__E0=OWE_ENG;window.ENG='std';oweHasRD=()=>true;OWE_ENG=()=>ENG;window.OWC={on:null,open:true,web:'/persist/oracle/x'};window.OWP=[];
    window.__rd0=rd;rd=async(p,o)=>{if(p.startsWith('desktop/owenc?'))return Object.assign({ok:true},OWC);if(p=='desktop/owenc'){const b=JSON.parse(o.body);OWP.push(b);if(b.on===false&&b.p!='pw1'){throw new Error('The password is incorrect.')}OWC.on=b.on;return{ok:true}}return __rd0(p,o)};
    USER=ACCTS.find(a=>a.n=='bob')||ACCTS[0];USER.disk=true;return 1})()`);
  const panel = async () => { await q("(()=>{document.querySelectorAll('.owetest').forEach(x=>x.remove());const c=document.createElement('div');c.className='owetest';document.body.appendChild(c);signinPanel(c);return 1})()"); await wait(400);
    return q("(()=>{const c=document.querySelector('.owetest .owec');return c?{on:c.checked,dis:c.disabled,msg:document.querySelector('.owetest .owem').textContent,note:document.querySelector('.owetest .owen').textContent}:null})()") };
  let pv = await panel();
  ok('18. Settings › Oracle on Standard: "Keep my websites encrypted with my account", off by default (the owner), said what off means', pv && !pv.on && !pv.dis && /shared browser profile/.test(pv.msg) && /later update/.test(pv.note), pv);
  await q("ENG='native';1"); pv = await panel();
  ok('19. ...on Native: on by default, said what on means (locked when you sign out)', pv && pv.on && /locked when you sign out/.test(pv.msg) && !/later update/.test(pv.note), pv);
  await q("window.__askT=askText;askText=async()=> 'wrong';document.querySelector('.owetest .owec').click();1"); await wait(500);
  ok('20. turning it off asks for the password: a wrong one is refused, it stays on', (await q("OWP.length==1&&OWP[0].on===false&&OWP[0].p=='wrong'&&OWC.on===null")) && (await q("document.querySelector('.owetest .owec').checked")), await q('OWP'));
  await q("askText=async()=> 'pw1';document.querySelector('.owetest .owec').click();1"); await wait(500);
  ok('21. ...the right one turns it off (kept by rudolfd, in the account\'s encrypted folder)', await q("OWC.on===false&&!document.querySelector('.owetest .owec').checked"), await q('OWP'));
  await q("askText=async()=>{window.__asked=1;return null};document.querySelector('.owetest .owec').click();1"); await wait(500);
  ok('22. ...turning it on again needs no password', await q("OWC.on===true&&!window.__asked&&OWP[OWP.length-1].p===undefined"), await q('OWP'));
  await q("OWC.open=false;1"); pv = await panel();
  ok('23. an account whose files are not open on the disk: greyed, with why', pv && pv.dis && /not open on this computer/.test(pv.msg), pv);
  await q("askText=__askT;rd=__rd0;oweHasRD=__H0;OWE_ENG=__E0;USER=null;document.querySelectorAll('.owetest').forEach(x=>x.remove());1");
  const srcN = fs.readFileSync(path.join(__dirname, '../../sys/rudolf-native.c'), 'utf8'), srcD = fs.readFileSync(path.join(__dirname, '../../sys/rudolfd-desktop.inc'), 'utf8');
  ok('24. Native: Oracle\'s profile in rudolfd\'s "web" folder; the old shared-profile partitions removed once; sign-out waits for Oracle to close', /oracle_web_profile\(L, n\)/.test(srcN) && /--user-data-dir=%s", prof/.test(srcN) && /if \(fresh\) oracle_old_parts_rm/.test(srcN) && /while \(OR_RUN && g_get_monotonic_time\(\) < until\)/.test(srcN));
  ok('25. rudolfd: "web" given only when the account did not turn it off; turning off checks the password and is logged', (srcD.match(/owenc_get\(s\.id\) != 0/g) || []).length == 2 && /turned off keeping websites encrypted/.test(srcD) && /dpw_check\(h, pw\) == 0; snprintf\(nm/.test(srcD));
  ok('no page error', !errs.length, errs);
  await b.close(); srv.close(); const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
