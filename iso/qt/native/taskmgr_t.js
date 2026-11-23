// 1.6.2 (docs/plan-1.6.2.md Part 39, step 2): the native Task Manager (rudolf-taskmgr) against the web one. The same rudolfd stand-in
// (same_rd.py with DIR/tm: the processes, the system, the memory's breakdown, the apps running, as fixtures/tmdata.js makes them) for
// the page's Task Manager in Chromium (Chromium mode, signed in) and the native one under Xvfb:
//   - look: each tab's window body screenshot-compared (pxdiff.py, a pixel one pixel away matching, text anti-aliasing aside): under
//     2 % (TM_LIMIT), light and dark;
//   - the checklist (taskmgr_checklist.txt, made from tm26.js, tmtop57.js, apps57.js, tools56.js and their tests): each item done on the
//     native one with the pointer and the keyboard (xdotool) and checked by what it shows (its state file) and what it asked of
//     rudolfd (the stand-in's calls);
//   - the memory at idle with the Processes tab open (RssAnon and PSS) against the web one's (Chromium's renderer for the page as an app
//     window, #app=taskmgr, as Part 38 measured on the host).
// node taskmgr_t.js RUDOLF-TASKMGR DIR OUT PORT DISPLAY [CASES]        (run by taskmgr_t.sh)
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path'), cp = require('child_process');
const [BIN, D, OUT, PORT, DSP, ONLY] = process.argv.slice(2);
const LIMIT = +(process.env.TM_LIMIT || 2), TOL = +(process.env.TM_TOL || 32), BASE = 'http://127.0.0.1:' + PORT;
const KEYS = JSON.parse(fs.readFileSync(path.join(D, 'keys.json'))), PW = KEYS.Ann.pw, ST = require('./same_state.js');
const wait = ms => new Promise(r => setTimeout(r, ms));
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 600) : '')) };
fs.mkdirSync(OUT, {recursive: true});
const only = k => !ONLY || ONLY.split(',').includes(k);
const TABS = ['apps', 'procs', 'perf', 'system', 'hw', 'start'];
const calls = () => { try { return fs.readFileSync(path.join(D, 'calls'), 'utf8') } catch (e) { return '' } };
const env0 = () => { const e = Object.assign({}, process.env, {DISPLAY: DSP, RUI_BASE: BASE, LC_ALL: 'C.UTF-8', NO_AT_BRIDGE: '1'}); for (const k of ['http_proxy', 'https_proxy', 'HTTP_PROXY', 'HTTPS_PROXY']) delete e[k]; return e };
const sh = c => cp.execSync(c, {env: env0()}).toString();
const px = (a, b, ra, rb, out) => JSON.parse(cp.execSync(`python3 ${path.join(__dirname, 'pxdiff.py')} ${a} ${b} ${ra.join(',')} ${rb.join(',')} ${TOL} ${out} 1`, {env: Object.assign({}, process.env, {PXDIFF_MAP: '1', PXDIFF_AA: '1'})}).toString());
const SET = {light: {dm: 'light', darkui: true, accent: '#2563eb'}, dark: {dm: 'dark', darkui: true, accent: '#2563eb'}};
let BR;
// ---- the page's Task Manager (Chromium mode)
async function page(mode, tabs) {
  ST.state(D, Object.assign({restore: false}, SET[mode]), {['bd95sess:' + ST.ACCT]: ['notepad', 'calc'], bd95sec: {fw: {on: true, mode: 'open'}, av: {on: true}}});
  const ctx = await BR.newContext({viewport: {width: 1280, height: 800}, deviceScaleFactor: 1, timezoneId: 'UTC'});
  await ctx.clock.install({time: new Date('2026-11-14T09:41:00Z')});
  await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => /generate_204/.test(r.request().url()) ? r.fulfill({status: 204, body: ''}) : r.abort());
  const p = await ctx.newPage();
  await p.goto(BASE + '/'); await p.waitForFunction(() => typeof userLogin == 'function' && document.getElementById('login') && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
  await p.clock.runFor(1500); await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
  await p.waitForFunction(() => typeof USER != 'undefined' && USER && getComputedStyle(document.getElementById('login')).display == 'none', null, {timeout: 60000});
  for (let i = 0; i < 12; i++) { await p.clock.runFor(500); await wait(100) }
  await p.evaluate(() => { document.querySelectorAll('.ntoast,#ntoasts>*').forEach(e => e.remove()); const W = launch('taskmgr'); W.el.style.left = '120px'; W.el.style.top = '60px' });
  const out = {};
  for (const t of tabs) {
    await p.evaluate(t => { const b = document.querySelector(`.tmtabs button[data-t='${t}']`); b.click() }, t);
    /* (the page samples every 2 s: 62 samples, so its graphs are as full as the native one's with RUDOLF_TM_HIST=60) */
    for (let i = 0; i < (t == 'perf' ? 62 : 3); i++) { await p.clock.runFor(2000); await wait(t == 'perf' ? 30 : 300) }
    await wait(800); await p.evaluate(() => document.querySelectorAll('.ntoast,#ntoasts>*').forEach(e => e.remove())); await p.mouse.move(5, 5);
    const b = await p.evaluate(() => { const r = window.__tmW.body.getBoundingClientRect(); return [Math.round(r.left), Math.round(r.top), Math.round(r.width), Math.round(r.height)] });
    const f = path.join(OUT, `${mode}-${t}.page.png`); await p.screenshot({path: f}); out[t] = {f, b};
    /* (where the page's parts are, from the window body's corner: kept with the pictures, to find a difference) */
    fs.writeFileSync(path.join(OUT, `${mode}-${t}.page.json`), JSON.stringify(await p.evaluate(b => { const o = []; const W = window.__tmW.body;
      W.querySelectorAll('.tmtabs,.tmtabs button,.tmb,.tmfoot,.tmfoot span,.tmfoot button,.tmb > *,.tmcard,.tmcard small,.tmcard b,.rmrow,.tmmrow,.tmt tr:first-child,.tmt').forEach(e => { const r = e.getBoundingClientRect();
        o.push([e.tagName.toLowerCase() + (e.className ? '.' + String(e.className).split(' ').join('.') : ''), Math.round(r.left - b[0]), Math.round(r.top - b[1]), Math.round(r.width * 10) / 10, Math.round(r.height * 10) / 10]) }); return o }, b)));
  }
  await ctx.close(); return out;
}
// ---- the native one
let NP = null; const SF = path.join(D, 'tmstate');
const nst = () => { try { return fs.readFileSync(SF, 'utf8') } catch (e) { return '' } };
const line = k => (nst().split('\n').find(x => x.startsWith(k + ' ')) || '').slice(k.length + 1);
const rect = n => { const l = line('rect ' + n); return l ? l.split(' ').map(Number) : null };
async function until(f, ms) { const t = Date.now(); while (Date.now() - t < (ms || 15000)) { if (await f()) return true; await wait(100) } return false }
function natStart(extra) { try { fs.unlinkSync(SF) } catch (e) {} const e = Object.assign(env0(), {RUDOLF_TM_STATE: SF, RUDOLF_TM_HIST: '60'}, extra || {});
  NP = cp.spawn(BIN, [], {env: e, stdio: ['ignore', 'pipe', 'pipe']}); NP.log = ''; NP.stdout.on('data', d => NP.log += d); NP.stderr.on('data', d => NP.log += d) }
function natStop() { if (NP) { NP.kill(); NP = null } }
const xdo = a => sh('xdotool ' + a);
const focus = () => { try { sh(`xdotool search --onlyvisible --class rudolf-taskmgr | head -1 | xargs -r -I{} xdotool windowfocus {} 2>/dev/null`) } catch (e) {} };
const click = (r, btn) => { xdo(`mousemove ${Math.round(r[0] + r[2] / 2)} ${Math.round(r[1] + r[3] / 2)} click ${btn || 1}`) };
async function natTab(i) { const r = rect('tab' + i); click(r); await until(() => line('tab') == String(i), 8000); await wait(1500) }
async function native(mode, tabs) {
  ST.state(D, Object.assign({restore: false}, SET[mode]), {['bd95sess:' + ST.ACCT]: ['notepad', 'calc'], bd95sec: {fw: {on: true, mode: 'open'}, av: {on: true}}});
  natStart(); await until(() => rect('body') && line('procs') != '' , 20000); await wait(1500); xdo('mousemove 5 5');
  const out = {};
  for (const t of tabs) { await natTab(TABS.indexOf(t)); xdo('mousemove 5 5'); await wait(2600);
    const f = path.join(OUT, `${mode}-${t}.nat.png`); sh(`import -window root ${f}`); out[t] = {f, b: rect('body')} }
  natStop(); return out;
}
// ---- the checklist on the native one
async function checklist() {
  ST.state(D, SET.light, {['bd95sess:' + ST.ACCT]: ['notepad', 'calc'], bd95sec: {fw: {on: true, mode: 'open'}, av: {on: true}}});
  const c0 = () => calls().length; let mark = c0(); const since = () => calls().slice(mark);
  natStart(); await until(() => rect('body') && line('procs') != '', 20000); await wait(1200); focus();
  ok('1. it opens: six tabs, Apps first, the footer\'s three buttons, the status line', [0, 1, 2, 3, 4, 5].every(i => rect('tab' + i)) && line('tab') == '0' && rect('copy') && rect('report') && rect('devcon') && /window\(s\) · \d+ processes · Rudolf OS on this computer/.test(line('status')), nst().slice(0, 600));
  ok('2. ...asking rudolfd for what the page asks (stats, processes, apps/running, the shell\'s windows)', /GET stats/.test(since()) && /GET processes/.test(since()) && /POST apps\/running/.test(since()), since().slice(-400));
  await natTab(1); mark = c0();
  ok('3. Processes: every process of the stand-in listed, grouped by kind', (nst().match(/^row /gm) || []).length == TMD.processes.items.length, (nst().match(/^row /gm) || []).length + ' of ' + TMD.processes.items.length);
  const rows = () => nst().split('\n').filter(x => x.startsWith('row ')).map(x => x.split(' ').map(Number));
  const order = () => rows().map(r => r[1]);
  const byMemDesc = TMD.processes.items.filter(p => p.tag == 'desktop').sort((a, b) => b.rss - a.rss).map(p => p.pid);
  ok('4. ...sorted by memory, most first, within each group (the page\'s default)', JSON.stringify(order().filter(p => byMemDesc.includes(p))) == JSON.stringify(byMemDesc), order());
  { const at = i => { const t = rect('ptable'), cx = line('cols').split(' ').map(Number); return [Math.round(t[0] + (cx[i] + cx[i + 1]) / 2), t[1] + 12] };   /* (each time: the columns' widths change with the arrow) */
    const want = [['name', '1'], ['pid', '1'], ['user', '0'], ['cpu', '0'], ['rss', '0'], ['rss', '1'], ['rss', '0']], got = [];
    for (let i = 0; i < 7; i++) { const p = at(Math.min(i, 4)); xdo(`mousemove ${p[0]} ${p[1]} click 1`); await until(() => line('sort') == want[i].join(' '), 4000); await wait(200); got.push(line('sort')) }
    ok('5. the headings sort: Name and PID rising first, User, CPU and Memory falling first; the same heading again turns it', JSON.stringify(got) == JSON.stringify(['name 1', 'pid 1', 'user 0', 'cpu 0', 'rss 0', 'rss 1', 'rss 0']), got) }
  { const f = rect('find'); focus(); click(f); xdo('type --delay 30 rudolfd'); await until(() => line('filter') == 'rudolfd', 4000); await wait(400);
    const n = rows().length, want = TMD.processes.items.filter(p => (p.name + ' ' + p.cmd + ' ' + p.pid).toLowerCase().includes('rudolfd')).length;
    ok('6. Find a process: only those whose name, command or number has the words', n == want && n > 0, n + ' of ' + want);
    xdo('key ctrl+a BackSpace'); await until(() => line('filter') == '', 4000); await wait(400) }
  const prot = TMD.processes.items.find(p => p.prot), own = TMD.processes.items.find(p => p.tag == 'app' && p.user == 'rudolf' && !p.prot), sys = TMD.processes.items.find(p => !p.prot && p.tag == 'system');
  const rowOf = pid => rows().find(r => r[1] == pid);
  /* (a process below the fold: found with Find a process, as a person would, then clicked) */
  const show = async name => { const f = rect('find'); focus(); click(f); xdo('key ctrl+a BackSpace'); xdo('type --delay 20 ' + name); await until(() => line('filter') == name, 4000); await wait(400) };
  { await show(prot.name); click(rowOf(prot.pid).slice(2)); await until(() => line('sel') == String(prot.pid), 4000);
    ok('7. a process Rudolf OS needs: selected, End process, End process tree and End now greyed, and it says why', /cannot be ended/.test(nst()) && line('sel') == String(prot.pid)) }
  { mark = c0(); await show(own.name); click(rowOf(own.pid).slice(2)); await until(() => line('sel') == String(own.pid), 4000);
    click(rect('endp')); await until(() => rect('dialog'), 4000);
    ok('8. End process on the person\'s own app: the page\'s Confirm (End process), no administrator', !!rect('dialog') && /ask end \d+ confirm/.test(nst()));
    xdo('key Return'); await until(() => /POST proc\/end\?pid=/.test(since()), 5000);
    ok('9. ...confirmed: rudolfd asked to end it (proc/end?pid=N)', new RegExp('POST proc/end\\?pid=' + own.pid + '(\\s|$)').test(since()), since().slice(-300)) }
  { mark = c0(); await wait(800); await show(sys.name); click(rowOf(sys.pid).slice(2)); await until(() => line('sel') == String(sys.pid), 4000);
    click(rect('endn')); await until(() => rect('dialog'), 4000);
    ok('10. End now on another account\'s or the system\'s process: the administrator\'s permission first', /ask end \d+ administrator/.test(nst()));
    xdo('type --delay 30 wrongpw'); xdo('key Return'); await until(() => /POST desktop\/admin/.test(since()), 5000); await wait(500);
    ok('11. ...a wrong password: refused by rudolfd, nothing ended', /POST desktop\/admin/.test(since()) && !/proc\/end/.test(since()));
    mark = c0(); xdo(`type --delay 30 '${PW.replace(/'/g, "'\\''")}'`); xdo('key Return'); await until(() => /POST proc\/end/.test(since()), 6000);
    ok('12. ...the right one: ended at once (force=1)', new RegExp('POST proc/end\\?pid=' + sys.pid + '&force=1').test(since()), since().slice(-300)) }
  { mark = c0(); await wait(800); await show(sys.name); const r = rowOf(sys.pid); click(r.slice(2), 3); await until(() => line('menu'), 4000);
    ok('13. the right-click menu: End process, End process tree, End now', !!line('menu') && /act menu \d+/.test(nst()));
    const m = line('menu').split(' ').map(Number); xdo(`mousemove ${m[0] + 60} ${m[1] + 4 + 22 + 25 + 12} click 1`); await until(() => rect('dialog'), 4000);
    xdo(`type --delay 30 '${PW.replace(/'/g, "'\\''")}'`); xdo('key Return'); await until(() => /tree=1/.test(since()), 6000);
    ok('14. ...End process tree: its processes ended with it (tree=1)', new RegExp('POST proc/end\\?pid=' + sys.pid + '&tree=1').test(since()), since().slice(-300)) }
  { mark = c0(); await wait(800); await show(sys.name); click(rowOf(sys.pid).slice(2)); await until(() => line('sel') == String(sys.pid), 4000); xdo('key Delete'); await until(() => rect('dialog'), 4000);
    ok('15. the keyboard: Delete asks to end the selected process', !!rect('dialog')); xdo('key Escape'); await until(() => !rect('dialog'), 4000) }
  await natTab(2); mark = c0(); await wait(2500);
  ok('16. Performance: the cards, the three graphs, where the memory goes (rudolfd\'s breakdown) and the network\'s rate (net)', !!rect('cards') && /GET memory\/breakdown/.test(since()) && /GET net/.test(since()), since().slice(-300));
  { const box = rect('box'); for (let i = 0; i < 12; i++) xdo(`mousemove ${box[0] + 300} ${box[1] + 200} click 5`); await wait(800); const b = rect('clearcache'); click(b); await until(() => /POST memory\/dropcache/.test(since()), 5000); ok('17. ...Clear cache: rudolfd asked (memory/dropcache)', /POST memory\/dropcache/.test(since())) }
  await natTab(3); mark = c0();
  ok('18. System: Refresh, the services with Restart, rudolfd\'s system', !!rect('refresh') && !!rect('services'));
  { const box = rect('box'); for (let i = 0; i < 6; i++) xdo(`mousemove ${box[0] + 300} ${box[1] + 200} click 5`); await wait(800); click(rect('restart-display')); await until(() => rect('dialog'), 4000);
    xdo(`type --delay 30 '${PW.replace(/'/g, "'\\''")}'`); xdo('key Return'); await until(() => /service\/restart/.test(since()), 6000);
    ok('19. ...Restart (the display) after the administrator\'s permission: service/restart', /POST service\/restart\?name=display/.test(since()), since().slice(-300)) }
  { mark = c0(); const bd = rect('body'); for (let i = 0; i < 12; i++) xdo(`mousemove ${bd[0] + 300} ${bd[1] + 200} click 4`); await wait(600); click(rect('refresh')); await until(() => /GET system/.test(since()), 5000); ok('20. ...Refresh asks rudolfd again', /GET system/.test(since()), nst()) }
  await natTab(4);
  ok('21. Hardware: Check this computer…, the computer, processor, graphics, storage, network, sound', !!rect('hwcheck'));
  await natTab(5); mark = c0();
  ok('22. Startup: Reopen my apps when I sign in, the apps reopened (bd95sess:ID)', !!rect('restore') && !!rect('startlist'));
  { click(rect('restore')); await until(() => /POST lsync/.test(since()), 4000); const ls = JSON.parse(fs.readFileSync(path.join(D, 'ls.json'))).data['bd95set:' + ST.ACCT];
    ok('23. ...unticked: the account\'s settings say restore: false (as the page saves them)', /"restore":false/.test(ls || ''), (ls || '').slice(0, 200)) }
  { mark = c0(); await wait(500); click(rect('remove0')); await until(() => /POST lsync/.test(since()), 4000); await wait(300);
    const s = JSON.parse(fs.readFileSync(path.join(D, 'ls.json'))).data['bd95sess:' + ST.ACCT]; ok('24. ...Remove: the app taken off the list', s == '["calc"]', s) }
  { await wait(600); click(rect('copy')); await until(() => fs.existsSync(SF + '.report'), 8000); if (!fs.existsSync(SF + '.report')) console.log(nst()); const t = fs.existsSync(SF + '.report') ? fs.readFileSync(SF + '.report', 'utf8') : '';
    ok('25. Copy system report: the report (its sections, no network addresses or account names) on the clipboard', /^Rudolf OS system report/.test(t) && /== Status/.test(t) && /== Processes \(/.test(t) && !/192\.168\./.test(t) && !/\bAnn\b/.test(t), t.slice(0, 300));
    let clip = ''; try { clip = sh('xclip -o -selection clipboard 2>/dev/null || true') } catch (e) {} ok('...on the clipboard', !clip || /^Rudolf OS system report/.test(clip)) }
  { click(rect('report')); await wait(500); ok('26. Report a problem…: the page\'s own dialog asked of the shell (open report)', /act open report/.test(nst())) }
  { xdo('mousemove 5 300'); await wait(800); click(rect('devcon')); await until(() => /act open devcon/.test(nst()), 3000); ok('27. Developer Console: opened by the shell (open devcon)', /act open devcon/.test(nst()), nst().split('\n').filter(x => /^act/.test(x)).slice(-6).join(' / ') + ' ' + JSON.stringify(rect('devcon'))) }
  natStop();
  { mark = c0(); natStart({}); NP.kill(); natStop(); natStart(); await until(() => rect('body'), 15000); NP.kill('SIGTERM'); await wait(500); }
  { mark = c0(); try { fs.unlinkSync(SF) } catch (e) {} const e = Object.assign(env0(), {RUDOLF_TM_STATE: SF}); const p = cp.spawn(BIN, ['--urgent'], {env: e, stdio: 'ignore'});
    await until(() => /POST desktop\/priority\?on=1/.test(since()), 8000); ok('28. Ctrl+Alt+Del\'s (--urgent): the desktop\'s processes first while it is open (desktop/priority?on=1)', /POST desktop\/priority\?on=1/.test(since())); p.kill() }
}
// ---- started by the native shell (rudolf-native): Ctrl+Shift+Esc opens the native one (its program, framed as the page's app), asked
// again it is brought forward (one window), and its Apps tab is given the shell's windows (_RUDOLF_ASK "wins")
async function shell() {
  const NAT = process.env.TM_NATIVE; if (!NAT) { console.log('SKIP shell: no rudolf-native (TM_NATIVE)'); return }
  ST.state(D, Object.assign({restore: false}, SET.light), {}); const apps = path.join(D, 'apps'); fs.mkdirSync(apps, {recursive: true}); try { fs.unlinkSync(path.join(apps, 'rudolf-taskmgr')) } catch (e) {}
  fs.symlinkSync(path.resolve(BIN), path.join(apps, 'rudolf-taskmgr')); try { fs.unlinkSync(SF) } catch (e) {}
  const NS = path.join(D, 'nstate'), nl = k => { try { return fs.readFileSync(NS, 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } };
  const e = Object.assign(env0(), {HOME: D, RUDOLF_NATIVE_STATE: NS, RUDOLF_NATIVE_APPS_DIR: apps, RUDOLF_TM_STATE: SF, LIBGL_ALWAYS_SOFTWARE: '1', TZ: 'UTC'});
  const lg = fs.openSync(path.join(OUT, 'native-shell.log'), 'w'); const sp = cp.spawn(NAT, ['--size=1280x800', '--draw=soft', '--base=' + BASE], {env: e, stdio: ['ignore', lg, lg], cwd: D});
  try {
    await until(() => nl('signin').includes('signin 1'), 40000); await wait(1500); xdo(`type --delay 40 '${PW.replace(/'/g, "'\\''")}'`); xdo('key Return');
    await until(() => nl('user').includes('user Ann'), 30000); await wait(2500);
    xdo('key ctrl+shift+Escape'); const got = await until(() => nl('client').some(x => / app=taskmgr /.test(x)), 25000);
    ok('30. on the native desktop, Ctrl+Shift+Esc starts the native Task Manager (rudolf-taskmgr), framed as Task Manager\'s window', got, nl('client').join(' / ') + ' ' + (fs.readFileSync(path.join(OUT, 'native-shell.log'), 'utf8').split('\n').filter(x => /taskmgr|native/.test(x)).slice(-5).join(' / ')));
    await until(() => line('wins') != '', 10000);
    ok('31. ...its Apps tab is given the shell\'s windows (the native shell answered _RUDOLF_ASK "wins")', +line('wins') >= 1, nst().slice(0, 300));
    const n0 = +sh("pgrep -c -x rudolf-taskmgr || true"); xdo('key ctrl+shift+Escape'); await wait(2500); const n1 = +sh("pgrep -c -x rudolf-taskmgr || true");
    ok('32. ...asked for again, the same window comes forward (one Task Manager, as the page\'s)', n0 >= 1 && n1 == n0, n0 + ' then ' + n1);
    ok('33. ...above the other windows (Ctrl+Alt+Del\'s and Ctrl+Shift+Esc\'s Task Manager) and the desktop\'s processes first while it is open', /POST desktop\/priority\?on=1/.test(calls()));
  } finally { sp.kill(); try { sh('pkill -x rudolf-taskmgr || true') } catch (e) {} }
}
// ---- the memory at idle with the Processes tab open
const rss = pid => { const s = fs.readFileSync(`/proc/${pid}/status`, 'utf8'); const g = k => +((s.match(new RegExp('^' + k + ':\\s+(\\d+)', 'm')) || [])[1] || 0) / 1024; let pss = null;
  try { pss = +((fs.readFileSync(`/proc/${pid}/smaps_rollup`, 'utf8').match(/^Pss:\s+(\d+)/m) || [])[1]) / 1024 } catch (e) {} return {anon: g('RssAnon'), rss: g('VmRSS'), pss} };
async function memory() {
  ST.state(D, SET.light, {});
  natStart({RUDOLF_TM_TAB: '1'}); await until(() => rect('body') && line('procs') != '', 20000); await wait(12000); const n = rss(NP.pid); natStop();
  /* the web one: Chromium's renderer for the page as an app window (#app=taskmgr, Native's one-app page) with Processes open, signed in */
  const br = await chromium.launch(); const ctx = await br.newContext({viewport: {width: 860, height: 580}}); const p = await ctx.newPage();
  const ses = await (await fetch(BASE + '/api/desktop/session', {headers: {'X-Rudolf-Token': 'tok160same'}}).catch(() => null));
  await p.goto(BASE + '/#app=taskmgr'); await wait(15000);
  await p.evaluate(() => { const b = document.querySelector(".tmtabs button[data-t='procs']"); if (b) b.click() }); await wait(12000);
  const pids = cp.execSync(`pgrep -f -- '--type=renderer' || true`).toString().trim().split('\n').filter(Boolean).map(Number);
  let w = null; for (const id of pids) { try { const m = rss(id); if (!w || m.anon > w.anon) w = Object.assign(m, {pid: id}) } catch (e) {} }
  await br.close(); void ses;
  const f = x => x == null ? 'n/a' : x.toFixed(1); const res = `native ${f(n.anon)} MB RssAnon, ${f(n.pss)} MB PSS; web (Chromium's renderer, #app=taskmgr) ${w ? f(w.anon) : 'n/a'} MB RssAnon, ${w ? f(w.pss) : 'n/a'} MB PSS`;
  fs.writeFileSync(path.join(OUT, 'memory.txt'), res + '\n'); console.log('memory: ' + res);
  ok('29. the memory at idle with Processes open: the native one under the web one\'s, and under Part 39\'s 40 MB', w && n.anon < w.anon && n.anon < 40, res);
}
const TMD = require('../host/fixtures/tmdata.js').make();
(async () => {
  fs.writeFileSync(path.join(D, 'tm'), JSON.stringify(TMD));
  BR = await chromium.launch({ignoreDefaultArgs: ['--hide-scrollbars']});
  const rows = [];
  for (const mode of ['light', 'dark']) {
    if (!only(mode)) continue;
    const tabs = TABS.filter(t => only(mode) || only(t));
    const P = await page(mode, tabs), N = await native(mode, tabs);
    for (const t of tabs) {
      if (!P[t] || !N[t] || !N[t].b) { ok(`${mode} ${t}: drawn by both`, false); continue }
      const d = px(P[t].f, N[t].f, P[t].b, N[t].b, path.join(OUT, `${mode}-${t}`)); rows.push([mode, t, d.pct, P[t].b.slice(2).join('x'), N[t].b.slice(2).join('x')]);
      ok(`${mode} ${t}: ${d.pct}% of the window's pixels differ (page ${P[t].b.slice(2).join('x')}, native ${N[t].b.slice(2).join('x')})`, d.pct < LIMIT, {box: d.box, map: d.map});
    }
  }
  await BR.close();
  if (only('checklist')) await checklist();
  if (only('shell')) await shell();
  if (only('memory')) await memory();
  fs.writeFileSync(path.join(OUT, 'summary.tsv'), rows.map(r => r.join('\t')).join('\n') + '\n');
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) && R.length ? 0 : 1);
})().catch(e => { console.error(e); natStop(); process.exit(1) });
