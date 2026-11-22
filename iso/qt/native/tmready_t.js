// 1.6.2 Part 14 (the owner's Vivobook, 2026-11-22: "taskmgr's page is not ready after 2500 ms", then "never said it was ready"): Task
// Manager opened while the shell's hidden page (do-bg) is busy (there: its desktop/apps took 192 s, with Steam starting). Every app's
// page was opened by window.open from the first page with an address, the hidden one, so it shared that page's web process and waited
// for it: Task Manager, the app for a desktop that is stuck, came up as a blank cover. Its page must say it is ready (the window shown
// with its rows) while the hidden page is busy; and another app's page still opens beside an app's page as before (memory).
//   SAME_SCRIPT=tmready_t.js sh same_t.sh OUT '' [RUDOLF-NATIVE]     (same_t.sh starts same_rd.py and Xvfb)
const fs = require('fs'), path = require('path'), cp = require('child_process');
const [BIN, D, OUT, PORT, DSP] = process.argv.slice(2);
const W = 1920, H = 1080, BASE = 'http://127.0.0.1:' + PORT;
const KEYS = JSON.parse(fs.readFileSync(path.join(D, 'keys.json'))), PW = KEYS.Ann.pw;
const ST = require('./same_state.js'), ACCT = ST.ACCT;
const wait = ms => new Promise(r => setTimeout(r, ms));
const xdo = a => cp.execSync('xdotool ' + a, {env: Object.assign({}, process.env, {DISPLAY: DSP, LC_ALL: 'C.UTF-8'})});
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(d).slice(0, 400) : '')) };
fs.mkdirSync(OUT, {recursive: true});
const SF = path.join(D, 'state'), NLOG = path.join(OUT, 'native.log');
const stx = () => { try { return fs.readFileSync(SF, 'utf8') } catch (e) { return '' } };
const nst = k => { const l = stx().split('\n').find(x => x.startsWith(k + ' ')); return l ? l.slice(k.length + 1) : null };
const nhit = k => { const l = stx().split('\n').find(x => x.startsWith('hit ' + k + ' ')); if (!l) return null; const a = l.split(' ').slice(3).map(Number); return [Math.round(a[0] + a[2] / 2), Math.round(a[1] + a[3] / 2)] };
async function until(f, ms) { const t = Date.now(); while (Date.now() - t < ms) { if (await f()) return true; await wait(100) } return false }
let TEV = path.join(D, 'teval'); try { fs.unlinkSync(TEV) } catch (e) {} cp.execSync('mkfifo ' + TEV);
async function neval(k, js) {
  const o = TEV + '.out', got = () => { try { return fs.readFileSync(o, 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } }, n0 = got().length;
  const fd = fs.openSync(TEV, fs.constants.O_RDWR | fs.constants.O_NONBLOCK); fs.writeSync(fd, k + ' ' + js + '\n'); fs.closeSync(fd);
  await until(() => got().length > n0, 8000); const r = got()[n0]; return r == null ? null : r.slice(k.length + 1);
}
(async () => {
  ST.state(D, {}, {['bd95pins:' + ACCT]: ['explorer', 'notepad', 'calc', 'settings', 'taskmgr']});
  const env = Object.assign({}, process.env, {DISPLAY: DSP, HOME: D, RUDOLF_NATIVE_STATE: SF, RUDOLF_NATIVE_TEST_EVAL: TEV, LIBGL_ALWAYS_SOFTWARE: '1', TZ: 'UTC', RUDOLF_WX_BASE: BASE, RUDOLF_WX_GEO: BASE});
  for (const k of ['http_proxy', 'https_proxy', 'HTTP_PROXY', 'HTTPS_PROXY', 'all_proxy', 'ALL_PROXY']) delete env[k];
  const lg = fs.openSync(NLOG, 'w'), NP = cp.spawn(BIN, ['--size=' + W + 'x' + H, '--draw=soft', '--base=' + BASE], {env, stdio: ['ignore', lg, lg], cwd: D});
  const log = () => { try { return fs.readFileSync(NLOG, 'utf8') } catch (e) { return '' } };
  try {
    await until(() => nst('signin') == '1', 40000); await wait(2000);
    xdo("type --delay 40 '" + PW.replace(/'/g, "'\\''") + "'"); xdo('key Return');
    ok('signed in', await until(() => nst('user') == 'Ann' && nst('signin') == '0', 30000));
    ok('the hidden page is ready', await until(() => /^svc ready/m.test(stx()), 40000), stx().split('\n').filter(l => /^svc /.test(l)).join(' '));
    await wait(3000);
    // the hidden page busy for 20 s (as its 192 s desktop/apps on the Vivobook), then Task Manager opened from its pin
    const busy = await neval('do-svc', "(setTimeout(()=>{const t=Date.now();while(Date.now()-t<20000);},50),'busy')");
    ok('the hidden page made busy', busy == 'busy', busy);
    await wait(300); const t0 = Date.now(), h = nhit('pin taskmgr'); if (h) xdo('mousemove ' + h.join(' ') + ' click 1');
    const shown = await until(() => /^client .* app=taskmgr /m.test(stx()), 12000), ms = Date.now() - t0;
    const rows = shown ? await neval('taskmgr', "String(document.querySelectorAll('.tmtabs button').length)") : null;
    console.log('Task Manager: its window shown ' + (shown ? ms + ' ms' : 'not within 12 s') + ' after its click, with the hidden page busy; its tabs: ' + rows);
    ok('Task Manager\'s page says it is ready while the hidden page is busy (under 2.5 s, not covered)', shown && ms < 2500 && !/taskmgr's page is not ready/.test(log()), ms + ' ms; ' + log().split('\n').filter(l => /taskmgr/.test(l)).join(' | '));
    ok('Task Manager\'s page answers while the hidden page is busy', rows == '6', rows);
    await until(async () => (await neval('do-svc', "'free'")) == 'free', 30000); await wait(1500);   /* (the hidden page free again) */
    // another app's window still opens (and draws) as before
    const h2 = nhit('pin notepad'); if (h2) xdo('mousemove ' + h2.join(' ') + ' click 1');
    ok('Notepad\'s window opens after', await until(() => /^client .* app=notepad /m.test(stx()), 15000));
    ok('nothing said a page was not ready', !/page is not ready|never said it was ready/.test(log()), log().split('\n').filter(l => /not ready|never said/.test(l)).join(' | '));
  } catch (e) { ok('ran', false, e.stack) }
  NP.kill(); await wait(500);
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})();
