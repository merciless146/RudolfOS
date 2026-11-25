// 1.6.3 Part 8, item 4 ("Desktop icon removal/changes still take several minutes on Native"): on Native, each change to the
// desktop's icons timed from the action to the shell drawing it (its state file's icons), as Chromium mode's one page shows it
// at once: an app's icon removed (its menu's Remove from desktop), a folder made (the desktop's menu › New folder) and renamed
// (its menu › Rename), a text document made, a file deleted from another window (File Explorer's page, as its Delete does), a
// file made by another window, and an app's icon put back (Desktop icons…'s tick), the icons' size changed by another window.
// Each must show within 1 s (DESK_LIMIT ms), and a window's change must not take away the icons another page made.
// DESK_DISK=1: the account's files on the disk, as the owner's (same_rd.py's home/*, DIR/diskhome); DESK_SLOW=MS: a busy computer
// (each picture the catalogue draws MS late in the hidden page and File Explorer's window: the catalogue some 20 s late, as the
// owner's Vivobook gave it 192 s late while Steam started). The times go to OUT/deskicons163.json.
// Run by same_t.sh with SAME_SCRIPT=deskicons163_t.js:
//   SAME_SCRIPT=deskicons163_t.js SAME_PORT=8744 SAME_DSP=:91 sh same_t.sh OUT
const fs = require('fs'), path = require('path'), cp = require('child_process');
const [BIN, D, OUT, PORT, DSP] = process.argv.slice(2);
const W = 1920, H = 1080, BASE = 'http://127.0.0.1:' + PORT;
const KEYS = JSON.parse(fs.readFileSync(path.join(D, 'keys.json'))), PW = KEYS.Ann.pw;
const ST = require('./same_state.js'), ACCT = ST.ACCT;
const LIMIT = +(process.env.DESK_LIMIT || 1000), DISK = process.env.DESK_DISK == '1', SLOW = +(process.env.DESK_SLOW || 0);
const wait = ms => new Promise(r => setTimeout(r, ms));
const env0 = Object.assign({}, process.env, {DISPLAY: DSP, LC_ALL: 'C.UTF-8'});
const xdo = a => cp.execSync('xdotool ' + a, {env: env0}).toString();
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 700) : '')) };
fs.mkdirSync(OUT, {recursive: true});
let NP = null, TEV = null;
function nlines(k) { try { return fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } }
const nst = k => { const l = nlines(k)[0]; return l ? l.slice(k.length + 1) : null };
async function until(f, ms) { const t = Date.now(); while (Date.now() - t < (ms || 20000)) { if (await f()) return true; await wait(50) } return false }
function TEVAL() { if (!TEV) { TEV = path.join(D, 'teval'); try { fs.unlinkSync(TEV) } catch (e) {} cp.execSync('mkfifo ' + TEV) } return TEV }
async function neval(k, js) {
  const o = TEV + '.out', got = () => { try { return fs.readFileSync(o, 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } }, n0 = got().length;
  const fd = fs.openSync(TEV, fs.constants.O_RDWR | fs.constants.O_NONBLOCK); fs.writeSync(fd, k + ' ' + js.replace(/\n/g, ' ') + '\n'); fs.closeSync(fd);
  await until(() => got().length > n0, 8000); const r = got()[n0]; return r == null ? null : r.slice(k.length + 1);
}
/* the icons the shell draws: [name, kind, the file's path or ''] */
const icons = () => nlines('dicon').map(l => { const a = l.split(' '), t = a.slice(7).join(' ').split('|'); return {k: t[0], kind: t[1], n: t.slice(2).join('|'), c: +a[3], r: +a[4], w: +a[5]} });
const has = n => icons().some(i => i.n.toUpperCase() == n.toUpperCase());
const at = n => { const i = icons().find(x => x.n.toUpperCase() == String(n).toUpperCase()); return i ? [i.c + i.w / 2, i.r + 24] : null };
const menuRow = label => { const l = nlines('cmitem').find(x => x.endsWith('|' + label)); return l ? l.split(' ').slice(1, 3).map(Number) : null };
const T = {};
async function timed(name, act, done, ms) { const t0 = Date.now(); await act(); const g = await until(done, ms || +(process.env.DESK_WAIT || 240000)); T[name] = g ? Date.now() - t0 : null;
  ok(name + ': shown within ' + LIMIT + ' ms (' + (g ? T[name] + ' ms' : 'not in ' + ((ms || +(process.env.DESK_WAIT || 240000)) / 1000) + ' s') + ')', g && T[name] <= LIMIT, {icons: icons().map(i => i.n)}) }
async function rightRow(q, label) { xdo('mousemove ' + Math.round(q[0]) + ' ' + Math.round(q[1]) + ' click 3'); await until(() => menuRow(label), 6000); await wait(150); const r = menuRow(label); if (r) xdo('mousemove ' + r[0] + ' ' + r[1] + ' click 1'); else xdo('key Escape'); return !!r }
async function main() {
  ST.state(D, {}, {['bd95deskadd:' + ACCT]: ['taskmgr']}); try { fs.unlinkSync(path.join(D, 'session')) } catch (e) {}
  if (DISK) fs.writeFileSync(path.join(D, 'diskhome'), '1');   /* (the account's files on the disk, as the owner's: same_rd.py's home/*) */
  const env = Object.assign({}, env0, {HOME: D, RUDOLF_NATIVE_STATE: path.join(D, 'state'), RUDOLF_NATIVE_TEST_EVAL: TEVAL(), LIBGL_ALWAYS_SOFTWARE: '1', TZ: 'UTC'});
  const lg = fs.openSync(path.join(OUT, 'native.log'), 'a'); NP = cp.spawn(BIN, ['--size=' + W + 'x' + H, '--draw=soft', '--base=' + BASE], {env, stdio: ['ignore', lg, lg], cwd: D});
  await until(() => nst('signin') == '1', 40000); await wait(2000); xdo("type --delay 40 '" + PW.replace(/'/g, "'\\''") + "'"); xdo('key Return');
  await until(() => nst('user') == 'Ann' && nst('signin') == '0', 30000); await until(() => has('Task Manager'), 30000); await wait(8000);
  if (DISK) { const dk = () => { try { return fs.readdirSync(path.join(D, 'dh')).some(a => fs.existsSync(path.join(D, 'dh', a, 'Desktop'))) } catch (e) { return false } };
    const on = await until(dk, 60000); ok('the account\'s files are on the disk (the stand-in rudolfd\'s home/*)', on, {dh: (() => { try { return fs.readdirSync(path.join(D, 'dh')) } catch (e) { return [] } })()}); await wait(5000) }
  /* (a busy computer: each picture the catalogue draws takes SLOW ms in the shell's hidden page and File Explorer's window, so the
     catalogue comes some 20 s late, as the owner's Vivobook gave it 192 s late while Steam started) */
  const slow = async k => { if (SLOW) await neval(k, `(()=>{if(!window.__slow){window.__slow=1;const o=oaPng;oaPng=function(){const a=arguments;return new Promise(r=>setTimeout(r,${SLOW})).then(()=>o.apply(this,a))}}return 1})()`) };
  await slow('do-svc');
  // 1. an app's icon removed
  await timed('1. Task Manager\'s icon removed (Remove from desktop)', () => rightRow(at('Task Manager'), 'Remove from desktop'), () => !has('Task Manager'));
  await wait(1500);
  // 2. a folder made, and renamed
  const n0 = new Set(icons().map(i => i.n));
  await timed('2. a folder made (New folder)', () => rightRow([1300, 500], 'New folder'), () => icons().some(i => !n0.has(i.n)));
  const made = (icons().find(i => !n0.has(i.n)) || {}).n; await wait(1500);
  if (nlines('dialog').length) { xdo('key Escape'); await wait(800) }   /* (the page may ask its name at once: kept as it is) */
  const fn = (icons().find(i => !n0.has(i.n)) || {}).n || made;
  await rightRow(at(fn), 'Rename'); await until(() => nlines('dialog').length, 6000); await wait(400); xdo('key ctrl+a'); xdo("type --delay 30 'Plans163'");
  await timed('3. the folder renamed (Rename: from its Enter)', async () => xdo('key Return'), () => has('Plans163'));
  await wait(1500);
  // 3. a text document made
  const n1 = new Set(icons().map(i => i.n));
  await timed('4. a text document made (New text document)', () => rightRow([1300, 600], 'New text document'), () => icons().some(i => !n1.has(i.n)));
  await wait(1500); if (nlines('dialog').length) { xdo('key Escape'); await wait(800) }
  // 4. changes from another window: File Explorer's page writes and deletes in the Desktop folder
  xdo('mousemove 1300 500 click 3'); await until(() => menuRow('Open File Explorer'), 6000); await wait(150); { const r = menuRow('Open File Explorer'); xdo('mousemove ' + r[0] + ' ' + r[1] + ' click 1') }
  await until(() => nlines('client').some(l => / app=explorer /.test(l)), 30000); await wait(3500); await slow('explorer');
  await timed('5. a file made by File Explorer\'s window', () => neval('explorer', `(()=>{const d=deskFiles();d['NOTE163.TXT']='hello';fsSave();EXPL.forEach(f=>f());return 1})()`), () => has('NOTE163.TXT'));
  await wait(2500); ok('5b. ...and the other icons stay (the folder and the text document the desktop made): the window that changed its files gave the others too', has('Plans163') && has('NEW TEXT DOCUMENT.TXT'), {icons: icons().map(i => i.n)});
  await wait(1500);
  await timed('6. a file deleted by File Explorer\'s window', () => neval('explorer', `(()=>{const d=deskFiles();delete d['NOTE163.TXT'];fsSave();EXPL.forEach(f=>f());return 1})()`), () => !has('NOTE163.TXT'));
  await wait(1500);
  ok('7a. the folder is on the desktop before it is deleted', has('Plans163'), {icons: icons().map(i => i.n)});
  await timed('7. the folder deleted by File Explorer\'s window', () => neval('explorer', `(()=>{const d=deskFiles();delete d['PLANS163'];delete d['Plans163'];fsSave();EXPL.forEach(f=>f());return 1})()`), () => !has('Plans163') && !has('PLANS163'));
  await wait(1500);
  // 5. an app's icon put back from another window (Add to desktop, as Desktop icons…'s tick does)
  await timed('8. Task Manager\'s icon put back (Add to desktop, from File Explorer\'s window)', () => neval('explorer', `(()=>{deskAdd('taskmgr');return 1})()`), () => has('Task Manager'));
  await wait(1500);
  // 6. the icons' size changed in another window (Settings' Display Properties saves it: persistS, the shell told)
  const w0 = (icons()[0] || {}).w;
  await timed('9. the icons\' size changed by another window (125 %)', () => neval('explorer', `(()=>{S.is=125;if(SC)SC.is=125;persistS();return 1})()`), () => (icons()[0] || {}).w > w0);
  NP.kill();
}
(async () => {
  try { await main() } catch (e) { ok('Native did the steps', 0, e.stack); if (NP) NP.kill() }
  fs.writeFileSync(path.join(OUT, 'deskicons163.json'), JSON.stringify(T, null, 1)); try { fs.copyFileSync(path.join(D, 'calls'), path.join(OUT, 'calls.txt')) } catch (e) {} console.log('times (ms): ' + JSON.stringify(T));
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
