// 1.6.2 (docs/plan-1.6.2.md Part 14): Native's app windows (each a page of its own in WebKit) have the desktop page's policy too: no
// script from another site, and Lua on the page's own copy (lib/fengari-web.js). BlockLua Builder's window: the Prompt's Lua, Blitzen's
// sandboxed Lua, and a script of another site refused (the owner's log on the Vivobook: cdnjs's fengari-web.js asked for in every window).
//   SAME_SCRIPT=noremote162_t.js sh same_t.sh OUT '' [RUDOLF-NATIVE]     (same_t.sh starts same_rd.py and Xvfb)
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
// an answer that takes a while: started into window.__nr, read until it is there
async function nawait(k, js, ms) { await neval(k, `(window.__nr=undefined,Promise.resolve((async()=>{${js}})()).then(v=>window.__nr=String(v),e=>window.__nr='ERR '+e),'started')`);
  let v = null; await until(async () => { v = await neval(k, "String(window.__nr)"); return v != null && v != 'undefined' }, ms); return v }
(async () => {
  ST.state(D, {}, {['bd95pins:' + ACCT]: ['explorer', 'blocklua', 'taskmgr']});
  const env = Object.assign({}, process.env, {DISPLAY: DSP, HOME: D, RUDOLF_NATIVE_STATE: SF, RUDOLF_NATIVE_TEST_EVAL: TEV, LIBGL_ALWAYS_SOFTWARE: '1', TZ: 'UTC', RUDOLF_WX_BASE: BASE, RUDOLF_WX_GEO: BASE});
  for (const k of ['http_proxy', 'https_proxy', 'HTTP_PROXY', 'HTTPS_PROXY', 'all_proxy', 'ALL_PROXY']) delete env[k];
  const lg = fs.openSync(NLOG, 'w'), NP = cp.spawn(BIN, ['--size=' + W + 'x' + H, '--draw=soft', '--base=' + BASE], {env, stdio: ['ignore', lg, lg], cwd: D});
  const log = () => { try { return fs.readFileSync(NLOG, 'utf8') } catch (e) { return '' } };
  try {
    await until(() => nst('signin') == '1', 40000); await wait(2000);
    xdo("type --delay 40 '" + PW.replace(/'/g, "'\\''") + "'"); xdo('key Return');
    ok('signed in', await until(() => nst('user') == 'Ann' && nst('signin') == '0', 30000));
    await until(() => /^svc ready/m.test(stx()), 40000); await wait(2000);
    const h = nhit('pin blocklua'); if (h) xdo('mousemove ' + h.join(' ') + ' click 1');
    ok('BlockLua Builder\'s window opens', await until(() => /^client .* app=blocklua /m.test(stx()), 20000), h);
    await wait(1500);
    const meta = await neval('blocklua', "String(!!document.querySelector('meta[http-equiv=\"Content-Security-Policy\"]')&&/script-src 'self'/.test(document.querySelector('meta[http-equiv=\"Content-Security-Policy\"]').content))");
    ok('its page has the desktop page\'s policy', meta == 'true', meta);
    ok('no script tag of another site in it', (await neval('blocklua', "String(![...document.scripts].some(s=>/^https?:/.test(s.src)&&!s.src.startsWith(location.origin)))")) == 'true');
    const lua = await nawait('blocklua', "let o='';await runLua('print(\"hi \"..(6*7))',{write:s=>o+=s,print:s=>o+=s+'\\n'},'T');return o.trim()+' '+[...document.scripts].some(s=>/\\/lib\\/fengari-web\\.js$/.test(s.src))", 20000);
    ok('Lua runs in the app window on the page\'s own copy (lib/fengari-web.js)', lua == 'hi 42 true', lua);
    const bx = await nawait('blocklua', "const r=await blzRunLuaBox('print(\"box \"..(2+3))',{auto:1});return (r.out||r.error||'').trim()", 25000);
    ok('Blitzen\'s sandboxed Lua runs in the app window', bx == 'box 5', bx);
    const csp = await nawait('blocklua', "return await new Promise(r=>{let v=null;addEventListener('securitypolicyviolation',e=>{v=e.violatedDirective},{once:true});const s=document.createElement('script');s.src='https://cdn.jsdelivr.net/npm/fengari-web@0.1.4/dist/fengari-web.js';s.onload=()=>r('loaded');s.onerror=()=>r('refused '+v);document.head.appendChild(s);setTimeout(()=>r('none'),5000)})", 15000);
    ok('a script of another site is refused in the app window (script-src)', /^refused script-src/.test(csp), csp);
    ok('Native\'s log names no refused script of another site (cdnjs, jsDelivr, unpkg) but the one the test asked for', !/cdnjs|unpkg/.test(log()), log().split('\n').filter(l => /cdnjs|jsdelivr|unpkg/.test(l)).slice(0, 4).join(' | '));
  } catch (e) { ok('ran', false, e.stack) }
  NP.kill(); await wait(500);
  console.log(R.filter(Boolean).length + '/' + R.length + ' passed'); process.exit(R.every(Boolean) ? 0 : 1);
})();
