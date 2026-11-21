// 1.6.1 (docs/plan-1.6.1.md Part 10, docs/plan-oracle.md item 2, its second half): Oracle's session (desktop/orasession61.js), kept in
// the account's own folder (rudolfd's home/* stood in: a folder in memory). With a stood-in shell (rudolf-webkit's protocol): each Oracle
// window's file (.rudolf/oracle-session/W.json) follows its tabs and which one is in front; 1.6.0's copy in the settings (S.otabs) is not
// written (and an old one goes); a window closed by its person is marked closed; signing out marks the open ones ended; a private window
// has no file. At the next sign-in, with "Reopen my tabs next time": every window back, the first one's tabs asleep but the one in
// front, the others in windows of their own, the old files gone; after an unexpected end without it: "Restore your tabs?" once, which
// brings them; after a sign-out without it: nothing offered; only closed windows: the last one closed. An account whose files are not
// on the disk keeps 1.6.0's way.   node orasession61_t.js [CHROME]   (build iso/www/index.html first)
const {chromium} = require('playwright'); const http = require('http'), fs = require('fs'), path = require('path');
const ROOT = path.join(__dirname, '../../..'), WWW = path.join(ROOT, 'iso/www');
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 500) : '')) };
const HOMEF = new Map();   /* the account's folder: path -> text */
const home = (what, qs, body) => { const p = qs.get('path') || '';
  if (what == 'list') { const L = [...HOMEF.keys()].filter(k => k.startsWith(p + '/') && !k.slice(p.length + 1).includes('/')); if (!L.length && ![...HOMEF.keys()].some(k => k.startsWith(p + '/'))) return [404, {ok: false, error: 'no such folder'}]; return [200, {ok: true, items: L.map(k => ({p: k, d: 0, s: HOMEF.get(k).length, m: 0, v: '1'})), more: false}] }
  if (what == 'read') return HOMEF.has(p) ? [200, HOMEF.get(p)] : [404, {ok: false, error: 'not found'}];
  if (what == 'write') { HOMEF.set(p, body); return [200, {ok: true, v: '2'}] }
  if (what == 'delete') { HOMEF.delete(p); return [200, {ok: true}] }
  return [200, {ok: true}] };
const srv = http.createServer((q, s) => { const [u0, qs0] = q.url.split('?'); const u = decodeURIComponent(u0); let body = '';
  q.on('data', d => body += d); q.on('end', async () => {
    if (u == '/' || u == '/index.html') { s.writeHead(200, {'content-type': 'text/html'}); return s.end(fs.readFileSync(path.join(WWW, 'index.html'), 'utf8').replace('<head>', '<head><script>window.RUDOLFD={token:"t",base:"",boot:"uefi",media:"disk",live:0,recovery:0,engine:"webkit"};window.__out=[];window.webkit={messageHandlers:{rudolfShell:{postMessage:m=>__out.push(m)}}};</script>')) }
    if (u.startsWith('/api/home/')) { const [c, r] = home(u.slice(10), new URLSearchParams(qs0 || ''), body); s.writeHead(c, {'content-type': typeof r == 'string' ? 'application/octet-stream' : 'application/json'}); return s.end(typeof r == 'string' ? r : JSON.stringify(r)) }
    if (u == '/api/events/wait') await new Promise(r => setTimeout(r, 800));
    if (u.startsWith('/api/')) { s.writeHead(200, {'content-type': 'application/json'}); return s.end(JSON.stringify(u == '/api/events' || u == '/api/events/wait' ? {ok: true, events: ''} : {ok: true})) }
    const f = path.join(WWW, path.normalize(u)); if (!f.startsWith(WWW) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { s.writeHead(404); return s.end() }
    s.writeHead(200); fs.createReadStream(f).pipe(s) }) }).listen(8616, '127.0.0.1');
const prep = "(()=>{try{svj('bd95setup',1)}catch(e){}['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});wins.slice().forEach(w=>w.close());USER=USER||ACCTS[0];USER.r='admin';USER.id=USER.id||'utest';USER.disk=1;HOME=HOME||{};HOMELOCKED=false;HD.acct=uid();window.__net=false;window.__sys=false;window.__notes=[];notify=(t,b,o)=>{__notes.push({t,b,o})};return uid()})()";
const DIR = '.rudolf/oracle-session/';
const files = () => [...HOMEF.entries()].filter(([k]) => k.startsWith(DIR)).map(([k, v]) => Object.assign(JSON.parse(v), {f: k.slice(DIR.length)}));
(async () => {
  const b = await chromium.launch({executablePath: process.argv[2] || undefined, args: ['--disable-gpu']});
  const p = await (await b.newContext({viewport: {width: 1280, height: 800}})).newPage(); const errs = []; p.on('pageerror', e => errs.push(e.stack.slice(0, 300)));
  await p.goto('http://127.0.0.1:8616/'); await p.waitForFunction(() => typeof browser == 'function' && typeof oraSession == 'function', null, {timeout: 30000});
  const q = s => p.evaluate(s); await q(prep);
  await q("__rwkIn({rudolfShell:'hello',version:'webkit-1.6.1',caps:['tabs']})"); await p.waitForTimeout(200);
  const OW = "wins.filter(w=>w.app=='browser'&&w.el.isConnected)";
  const tabsOf = w => q(`[...${w}.body.querySelectorAll('.otab')].map(d=>d.textContent.replace(/✕$/,''))`);
  // a session: its file follows the window
  await q("S.oreopen=true;S.otabs=['https://old.example/'];persistS();oraSsLoad()"); await p.waitForTimeout(300);
  ok('0. with the files kept in the account\'s folder, 1.6.0\'s copy in the settings (S.otabs) is removed', (await q('S.otabs')) === undefined && !/old\.example/.test(await q("localStorage.getItem('bd95set:'+uid())||''")));
  await q("browser('https://one.example/')"); await p.waitForTimeout(300);
  const W1 = `${OW}[0]`;
  await q(`${W1}.oNewTab('https://two.example/')`); await q(`${W1}.oNewTab('https://three.example/')`); await p.waitForTimeout(200);
  await q(`${W1}.body.querySelectorAll('.otab')[1].click()`); await p.waitForTimeout(1500);
  let F = files();
  ok('1. the window\'s file in the account\'s folder: its three tabs, the second in front', F.length == 1 && F[0].tabs.map(x => x.u).join(' ') == 'https://one.example/ https://two.example/ https://three.example/' && F[0].ci == 1 && !F[0].closed, F);
  ok('2. ...and nothing in the settings or the page\'s storage names them (Oracle\'s history apart, which is its own)', !(await q("JSON.stringify(S.otabs||null)+JSON.stringify(Object.keys(localStorage).filter(k=>k!='bd95hist').map(k=>localStorage.getItem(k)))")).includes('three.example'));
  await q("oraclePrivate?oraclePrivate('https://priv.example/'):browser('https://priv.example/',{priv:1})"); await p.waitForTimeout(1500);
  ok('3. a private window has no file', !files().some(o => JSON.stringify(o).includes('priv.example')) && files().length == 1);
  await q(`${OW}.find(w=>w.priv).close()`);
  // a second window, then signing out: both marked ended
  await q("browser('https://four.example/')"); await p.waitForTimeout(1500);
  ok('4. a second window, a file of its own', files().length == 2);
  await q("userLogout()"); await p.waitForTimeout(500);
  F = files(); ok('5. signing out: the open windows\' files say so (ended), written at once', F.length == 2 && F.every(o => o.ended && !o.closed), F.map(o => [o.f, o.ended, o.closed]));
  // the next sign-in, after a sign-out, with "Reopen my tabs next time"
  await q(prep); await q("S.oreopen=true;oraSsLoad()"); await p.waitForTimeout(500);
  const opens0 = await q("__out.filter(m=>m.op=='open').length");
  await q("browser()"); await p.waitForTimeout(800);
  const ws = await q(`${OW}.length`), t1 = await tabsOf(`${OW}[0]`), t2 = ws > 1 ? await tabsOf(`${OW}[1]`) : [];
  ok('6. the first Oracle window after signing in: both windows back', ws == 2 && t1.length == 3 && t2.some(x => /four/.test(x)), [ws, t1, t2]);
  ok('7. ...the first one\'s tabs asleep (💤) but the one that was in front, which alone loads', t1.filter(x => /^💤/.test(x)).length == 2 && !/^💤/.test(t1[1]) && (await q(`__out.filter(m=>m.op=='open').slice(${opens0}).map(m=>m.url)`)).filter(u => /one|two|three/.test(u)).join() == 'https://two.example/', [t1, await q(`__out.filter(m=>m.op=='open').slice(${opens0}).map(m=>m.url)`)]);
  await q(`${OW}[0].body.querySelectorAll('.otab')[2].click()`); await p.waitForTimeout(200);
  ok('8. an asleep tab clicked: it loads then', (await q("__out.filter(m=>m.op=='open').pop().url")) == 'https://three.example/' && !/^💤/.test((await tabsOf(`${OW}[0]`))[2]));
  await p.waitForTimeout(1500); F = files();
  ok('9. the old files are gone; the windows now have new ones', F.length == 2 && F.every(o => !o.ended) && F.some(o => o.tabs.length == 3), F.map(o => [o.f, o.tabs.length]));
  // a window closed by its person: marked closed
  await q(`${OW}[1].close()`); await p.waitForTimeout(400);
  ok('10. a window closed: its file marked closed', files().filter(o => o.closed).length == 1);
  // an unexpected end: the next sign-in without the setting offers "Restore your tabs?"
  await q(`${OW}.forEach(w=>w.close())`); await p.waitForTimeout(300); HOMEF.clear();
  HOMEF.set(DIR + 'wa1b2c3.json', JSON.stringify({v: 1, at: 1, tabs: [{u: 'https://crash1.example/', title: 'C1'}, {u: 'https://crash2.example/', title: 'C2'}], ci: 0}));
  await q("S.oreopen=false;__notes.length=0;oraSsLoad()"); await p.waitForTimeout(400);
  await q("browser()"); await p.waitForTimeout(1300);
  const note = await q("__notes.find(n=>n.t=='Restore your tabs?')");
  ok('11. after an unexpected end, without the setting: the start page, and "Restore your tabs?" offered', !!note && /2 tabs/.test(note.b) && (await tabsOf(`${OW}[0]`)).length == 1, [note, await tabsOf(`${OW}[0]`)]);
  await q("__notes.find(n=>n.t=='Restore your tabs?').o.action()"); await p.waitForTimeout(400);
  const t3 = await tabsOf(`${OW}[0]`);
  ok('12. ...taken: its tabs in that window, the untouched start page gone', t3.length == 2 && t3.every(x => /C1|C2|crash/.test(x)), t3);
  await q(`${OW}.forEach(w=>w.close())`); await q("browser()"); await p.waitForTimeout(900);
  ok('13. offered once only', (await q("__notes.filter(n=>n.t=='Restore your tabs?').length")) == 1);
  // after a sign-out, without the setting: nothing offered
  await q(`${OW}.forEach(w=>w.close())`); HOMEF.clear(); HOMEF.set(DIR + 'wd4e5f6.json', JSON.stringify({v: 1, at: 1, ended: 1, tabs: [{u: 'https://e.example/', title: 'E'}], ci: 0}));
  await q("S.oreopen=false;__notes.length=0;oraSsLoad()"); await p.waitForTimeout(300); await q("browser()"); await p.waitForTimeout(1200);
  ok('14. after a sign-out without the setting: the start page, nothing offered', (await q("__notes.length")) == 0 && (await tabsOf(`${OW}[0]`)).length == 1);
  // only windows closed by their person: the last one closed
  await q(`${OW}.forEach(w=>w.close())`); HOMEF.clear();
  HOMEF.set(DIR + 'wold000.json', JSON.stringify({v: 1, at: 100, closed: 1, tabs: [{u: 'https://older.example/', title: 'Older'}], ci: 0}));
  HOMEF.set(DIR + 'wnew000.json', JSON.stringify({v: 1, at: 200, closed: 1, tabs: [{u: 'https://newer.example/', title: 'Newer'}], ci: 0}));
  await q("S.oreopen=true;oraSsLoad()"); await p.waitForTimeout(300); await q("browser()"); await p.waitForTimeout(600);
  const t4 = await tabsOf(`${OW}[0]`);
  ok('15. every window closed last time: the last one closed comes back', (await q(`${OW}.length`)) == 1 && t4.length == 1 && /Newer|newer/.test(t4[0]), t4);
  // an account whose files are not on the disk: 1.6.0's way
  await q(`${OW}.forEach(w=>w.close())`); await q("USER.disk=0;S.oreopen=true;delete S.otabs"); await q("browser('https://nodisk.example/')"); await p.waitForTimeout(300); await q(`${OW}[0].close()`); await p.waitForTimeout(200);
  ok('16. files not on the disk: 1.6.0\'s copy in the settings, as before', JSON.stringify(await q('S.otabs')) == '["https://nodisk.example/"]');
  ok('no page errors', !errs.length, errs);
  await b.close(); srv.close();
  const n = R.filter(Boolean).length; console.log(`${n}/${R.length} passed`); process.exit(n == R.length ? 0 : 1);
})().catch(e => { console.error(e); process.exit(1) });
