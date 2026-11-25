// 1.6.3 Part 8, item 1 ("Paint app does not prompt user to save after unsaved changes are made to images"): Paint asks
// "Save changes to NAME?" (Save, Don't save, Cancel) before New or closing its window when the picture has changed since it
// was opened or saved, as WordPro and Notepad do; nothing is asked when nothing changed.
//   Chromium mode (Playwright): close unchanged (no question), drawn then ✕ (asked; Cancel keeps it; Don't save closes it),
//     drawn then ✕ › Save (the Save As name, the file written, the window closed), New (asked; Cancel keeps the drawing,
//     Don't save clears it), a saved picture opened again (unchanged: no question; changed: asked with its name).
//   Native: the same window's frame ✕ unchanged (closed), changed (asked in the page; Cancel keeps the window; Don't save
//     closes it), New (asked).
// Run by same_t.sh with SAME_SCRIPT=paintsave163_t.js:
//   SAME_SCRIPT=paintsave163_t.js SAME_PORT=8743 SAME_DSP=:90 sh same_t.sh OUT
const {chromium} = require('playwright'); const fs = require('fs'), path = require('path'), cp = require('child_process');
const [BIN, D, OUT, PORT, DSP] = process.argv.slice(2);
const W = 1920, H = 1080, BASE = 'http://127.0.0.1:' + PORT;
const KEYS = JSON.parse(fs.readFileSync(path.join(D, 'keys.json'))), PW = KEYS.Ann.pw;
const ST = require('./same_state.js');
const wait = ms => new Promise(r => setTimeout(r, ms));
const env0 = Object.assign({}, process.env, {DISPLAY: DSP, LC_ALL: 'C.UTF-8'});
const xdo = a => cp.execSync('xdotool ' + a, {env: env0}).toString();
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + JSON.stringify(d).slice(0, 700) : '')) };
fs.mkdirSync(OUT, {recursive: true});
/* ---- Chromium mode */
async function chromiumMode() {
  ST.state(D, {}, {});
  const br = await chromium.launch(); const ctx = await br.newContext({viewport: {width: W, height: H}, timezoneId: 'UTC'}); await ctx.route(u => !/^https?:\/\/127\.0\.0\.1/.test(u.href), r => r.abort());
  const p = await ctx.newPage(); const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto(BASE + '/'); await p.waitForFunction(() => typeof userLogin == 'function' && getComputedStyle(document.getElementById('login')).display != 'none', null, {timeout: 60000});
  await wait(1000); await p.focus('#lp'); await p.keyboard.type(PW); await p.keyboard.press('Enter');
  await p.waitForFunction(() => USER && getComputedStyle(document.getElementById('login')).display == 'none', null, {timeout: 60000}); await wait(2500);
  const open = async pth => { await p.evaluate(pth => { wins.slice().forEach(w => { w._force = true; w.close() }); document.querySelectorAll('.pask').forEach(o => o.remove()); pth ? paint(pth) : launch('paint') }, pth || null); await wait(800);
    return p.evaluate(() => { const W = wins[wins.length - 1]; W.el.style.left = '300px'; W.el.style.top = '120px'; return true }) };
  const pw = () => p.evaluate(() => { const W = wins[wins.length - 1], c = W.el.querySelector('canvas').getBoundingClientRect(), x = W.el.querySelector('.tb .cl').getBoundingClientRect(), n = W.el.querySelector('.pt .cl').getBoundingClientRect();
    return {cv: [c.left, c.top], x: [x.left + x.width / 2, x.top + x.height / 2], nw: [n.left + n.width / 2, n.top + n.height / 2], n: wins.length} });
  const draw = async () => { const g = await pw(); await p.mouse.move(g.cv[0] + 40, g.cv[1] + 40); await p.mouse.down(); await p.mouse.move(g.cv[0] + 160, g.cv[1] + 120, {steps: 8}); await p.mouse.up(); await wait(200) };
  const asked = () => p.evaluate(() => { const o = document.querySelector('.pask'); return o ? o.querySelector('.dqa').textContent : '' });
  const answer = b => p.click('.pask .' + {save: 'y', discard: 'n', cancel: 'c'}[b]);
  const inked = () => p.evaluate(() => { const W = wins[wins.length - 1], c = W.el.querySelector('canvas'), d = c.getContext('2d').getImageData(0, 0, c.width, c.height).data; for (let i = 0; i < d.length; i += 4) if (d[i] < 200) return true; return false });
  const nwin = () => p.evaluate(() => wins.length);
  // 1. unchanged: closed at once
  await open(); let g = await pw(); let n0 = await nwin(); await p.mouse.click(g.x[0], g.x[1]); await wait(500);
  ok('Chromium mode: 1. Paint unchanged closes without a question', !(await asked()) && (await nwin()) == n0 - 1, {asked: await asked()});
  // 2. drawn: asked; Cancel keeps it; Don't save closes it
  await open(); await draw(); g = await pw(); n0 = await nwin(); await p.mouse.click(g.x[0], g.x[1]); await wait(500); const q2 = await asked();
  ok('Chromium mode: 2. drawn, ✕ asks "Save changes to Untitled?" and the window stays', q2 == 'Save changes to Untitled?' && (await nwin()) == n0, {q2});
  if (q2) { await answer('cancel'); await wait(300) }
  ok('Chromium mode: 3. ...Cancel keeps the window and the drawing', !(await asked()) && (await nwin()) == n0 && await inked());
  g = await pw(); await p.mouse.click(g.x[0], g.x[1]); await wait(500); if (await asked()) { await answer('discard'); await wait(500) }
  ok('Chromium mode: 4. ...✕ again › Don\'t save closes it', (await nwin()) == n0 - 1);
  // 3. drawn: Save writes the file and closes
  await open(); await draw(); g = await pw(); n0 = await nwin(); await p.mouse.click(g.x[0], g.x[1]); await wait(500); if (await asked()) { await answer('save'); await wait(500) }
  const nd = await p.evaluate(() => { const W = wins.find(w => /Save As/.test(w.el.textContent.slice(0, 60))); if (!W) return false; W.body.querySelector('input').value = 'PAINT163.PNG'; W.body.querySelector('button').click(); return true }); await wait(800);
  const saved = await p.evaluate(() => { const v = node(['C:', 'PAINT163.PNG']); return typeof v == 'string' && v.startsWith('data:image/png') });
  ok('Chromium mode: 5. drawn, ✕ › Save asks the name, writes the picture and closes the window', nd && saved && (await nwin()) == n0 - 1, {nd, saved, n: await nwin(), n0});
  // 4. New: asked; Cancel keeps the drawing; Don't save clears it
  await open(); await draw(); g = await pw(); await p.mouse.click(g.nw[0], g.nw[1]); await wait(400); const q4 = await asked();
  if (q4) { await answer('cancel'); await wait(300) }
  ok('Chromium mode: 6. drawn, New asks, and Cancel keeps the drawing', q4 == 'Save changes to Untitled?' && await inked(), {q4});
  await p.mouse.click(g.nw[0], g.nw[1]); await wait(400); if (await asked()) { await answer('discard'); await wait(300) }
  ok('Chromium mode: 7. ...New › Don\'t save clears it', !(await inked()));
  g = await pw(); n0 = await nwin(); await p.mouse.click(g.x[0], g.x[1]); await wait(500);
  ok('Chromium mode: 8. ...a new picture unchanged closes without a question', !(await asked()) && (await nwin()) == n0 - 1, {asked: await asked()});
  // 5. a saved picture: unchanged, no question; changed, asked with its name
  await open(['C:', 'PAINT163.PNG']); await wait(600); g = await pw(); n0 = await nwin(); await p.mouse.click(g.x[0], g.x[1]); await wait(500);
  ok('Chromium mode: 9. a saved picture opened and closed unchanged: no question', !(await asked()) && (await nwin()) == n0 - 1, {asked: await asked()});
  await open(['C:', 'PAINT163.PNG']); await wait(600); await draw(); g = await pw(); await p.mouse.click(g.x[0], g.x[1]); await wait(500); const q5 = await asked();
  ok('Chromium mode: 10. ...changed: "Save changes to PAINT163.PNG?"', q5 == 'Save changes to PAINT163.PNG?', {q5});
  if (q5) await answer('discard');
  ok('Chromium mode: no errors', !errs.length, errs);
  await br.close();
}
/* ---- Native */
let NP = null, TEV = null;
function nlines(k) { try { return fs.readFileSync(path.join(D, 'state'), 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } }
const nst = k => { const l = nlines(k)[0]; return l ? l.slice(k.length + 1) : null };
const nhit = k => { const l = nlines('hit').find(x => x.startsWith('hit ' + k + ' ')); if (!l) return null; const a = l.split(' ').slice(3).map(Number); return [a[0] + a[2] / 2, a[1] + a[3] / 2] };
const mrow = k => { const l = nlines('mrow').find(x => x.split(' ')[1] == k); if (!l) return null; const a = l.split(' ').slice(2).map(Number); return [a[0] + 40, a[1] + a[3] / 2] };
async function until(f, ms) { const t = Date.now(); while (Date.now() - t < (ms || 20000)) { if (await f()) return true; await wait(200) } return false }
const nclick = (c, b) => { if (c) xdo('mousemove ' + Math.round(c[0]) + ' ' + Math.round(c[1]) + ' click ' + (b || 1)) };
async function menuOpen() { if (nst('menu') != '1') nclick(nhit('start -')); return until(() => nst('menu') == '1' && nlines('mrow').length > 5, 6000) }
async function menuRow(k) { await menuOpen(); const r0 = nlines('mrow')[0].split(' ').slice(2).map(Number);
  for (let i = 0; i < 40 && !mrow(k); i++) { xdo('mousemove ' + (r0[0] + 60) + ' ' + (r0[1] + 100) + ' click 5'); await wait(150) } return mrow(k) }
function TEVAL() { if (!TEV) { TEV = path.join(D, 'teval'); try { fs.unlinkSync(TEV) } catch (e) {} cp.execSync('mkfifo ' + TEV) } return TEV }
async function neval(k, js) {
  const o = TEV + '.out', got = () => { try { return fs.readFileSync(o, 'utf8').split('\n').filter(x => x.startsWith(k + ' ')) } catch (e) { return [] } }, n0 = got().length;
  const fd = fs.openSync(TEV, fs.constants.O_RDWR | fs.constants.O_NONBLOCK); fs.writeSync(fd, k + ' ' + js.replace(/\n/g, ' ') + '\n'); fs.closeSync(fd);
  await until(() => got().length > n0, 8000); const r = got()[n0]; return r == null ? null : r.slice(k.length + 1);
}
const client = () => { const l = nlines('client').find(x => / app=paint /.test(x)); if (!l) return null; const a = l.split(' '); return {x: +a[3], y: +a[4], cw: +a[5], ch: +a[6]} };
const frameX = c => [c.x + c.cw + 2 - 1 - 8 - 12, c.y + 1 + 7 + 11];
/* (a change: the Fill tool poured on the canvas, as a click with it does) */
const FILL = `(()=>{const W=wins[wins.length-1];W.body.querySelector('[data-tool=fill]').click();const c=W.body.querySelector('canvas'),r=c.getBoundingClientRect();W.body.querySelector('.pal i[data-c="#ed1c24"]').click();c.dispatchEvent(new PointerEvent('pointerdown',{bubbles:true,clientX:r.left+20,clientY:r.top+20,button:0,pointerId:1}));return 1})()`;
const ASKED = `(()=>{const o=document.querySelector('.pask');return o?o.querySelector('.dqa').textContent:'-'})()`;
async function native() {
  ST.state(D, {}, {}); try { fs.unlinkSync(path.join(D, 'session')) } catch (e) {}
  const env = Object.assign({}, env0, {HOME: D, RUDOLF_NATIVE_STATE: path.join(D, 'state'), RUDOLF_NATIVE_TEST_EVAL: TEVAL(), LIBGL_ALWAYS_SOFTWARE: '1', TZ: 'UTC'});
  const lg = fs.openSync(path.join(OUT, 'native.log'), 'a'); NP = cp.spawn(BIN, ['--size=' + W + 'x' + H, '--draw=soft', '--base=' + BASE], {env, stdio: ['ignore', lg, lg], cwd: D});
  await until(() => nst('signin') == '1', 40000); await wait(2000); xdo("type --delay 40 '" + PW.replace(/'/g, "'\\''") + "'"); xdo('key Return');
  await until(() => nst('user') == 'Ann' && nst('signin') == '0', 30000); await wait(3000);
  const open = async () => { nclick(await menuRow('paint')); return until(() => client(), 30000).then(r => wait(2500).then(() => r)) };
  // 1. unchanged: the frame's ✕ closes it
  let o = await open(); nclick(frameX(client())); const g1 = await until(() => !client(), 10000);
  ok('Native: 1. Paint unchanged: the frame\'s ✕ closes it without a question', o && g1);
  // 2. changed: asked; Cancel keeps it; Don't save closes it
  o = await open(); await neval('paint', FILL); await wait(300); nclick(frameX(client())); await wait(1500); const q2 = await neval('paint', ASKED);
  ok('Native: 2. changed, the frame\'s ✕ asks "Save changes to Untitled?" and the window stays', o && q2 == 'Save changes to Untitled?' && !!client(), {q2});
  await neval('paint', `(()=>{document.querySelector('.pask .c').click();return 1})()`); await wait(800);
  ok('Native: 3. ...Cancel keeps the window', !!client() && (await neval('paint', ASKED)) == '-');
  await wait(5500);   /* (a second ✕ within 5 s is the same request; after, rudolf-native asks again) */
  nclick(frameX(client())); await wait(1500); await neval('paint', `(()=>{const b=document.querySelector('.pask .n');if(b)b.click();return b?1:0})()`);
  ok('Native: 4. ...✕ again › Don\'t save closes it', await until(() => !client(), 10000));
  // 3. New asks
  o = await open(); await neval('paint', FILL); await wait(300); await neval('paint', `(()=>{wins[wins.length-1].body.querySelector('.pt .cl').click();return 1})()`); await wait(600);
  const q3 = await neval('paint', ASKED);
  ok('Native: 5. changed, New asks', q3 == 'Save changes to Untitled?', {q3});
  NP.kill();
}
(async () => {
  try { await chromiumMode() } catch (e) { ok('Chromium mode did the steps', 0, e.stack) }
  try { await native() } catch (e) { ok('Native did the steps', 0, e.stack); if (NP) NP.kill() }
  console.log(R.filter(x => x).length + '/' + R.length + ' passed'); process.exit(R.every(x => x) ? 0 : 1);
})();
