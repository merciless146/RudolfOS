// 1.5.6: Chromium's own shortcuts inside the shell (host.sh start fresh first): with real X keys (xin), none of them
// closes, reloads, covers or opens a window over the desktop, whether the keyboard is on the desktop (keys56.js) or on
// the shell itself (shell.js). Alt+F4 closed the desktop on the Vivobook; here, before 1.5.6, F5 and Ctrl+Shift+R
// reloaded it. Ctrl+W, T, N and Q arrive raw here (on Rudolf OS rudolf-display turns them into F20..F23 first).
//   node keysiwa_t.js XIN
const {list, evalIn} = require('./cdp.js');
const {execFileSync} = require('child_process');
const XIN = process.argv[2]; const X = (...a) => execFileSync(XIN, a.map(String), {env: Object.assign({}, process.env, {DISPLAY: ':77'})});
const R = []; const ok = (n, c, d) => { R.push(!!c); console.log((c ? 'PASS ' : 'FAIL ') + n + (d !== undefined && !c ? ' - ' + String(typeof d == 'string' ? d : JSON.stringify(d)).slice(0, 600) : '')) };
const combos = ['F5', 'ctrl+r', 'ctrl+shift+r', 'shift+F5', 'ctrl+F5', 'ctrl+h', 'ctrl+j', 'shift+Escape', 'F1', 'alt+Home', 'ctrl+shift+o', 'ctrl+shift+Delete',
  'ctrl+p', 'ctrl+s', 'ctrl+o', 'ctrl+d', 'ctrl+f', 'ctrl+u', 'F12', 'ctrl+shift+i', 'ctrl+shift+j', 'ctrl+shift+n', 'ctrl+n', 'ctrl+t', 'ctrl+shift+w', 'ctrl+w', 'ctrl+F4', 'alt+F4'];
(async () => {
  const desk = async () => { const l = await list(); return {t: l.find(x => x.url.startsWith('http://127.0.0.1:8080')), n: l.filter(x => /^(page|app|webview|other)$/.test(x.type)).length} };
  const bad = [];
  for (const c of combos) {
    let d = await desk(); if (!d.t) { bad.push([c, 'no desktop before']); break }
    await evalIn(d.t, "window.__mark=1;if(!window.__kl){window.__kl=1;window.__keys=[];addEventListener('keydown',e=>__keys.push(e.code+(e.defaultPrevented?'!':'')),true);document.addEventListener('visibilitychange',()=>__keys.push('VIS:'+document.visibilityState))}__keys.length=0;1");
    X('move', 640, 400, 'click', 1, 'sleep', 300);
    const parts = c.split('+'), mods = parts.slice(0, -1).map(m => ({ctrl: 'Control_L', shift: 'Shift_L', alt: 'Alt_L'}[m]));
    X(...mods.flatMap(m => ['kdown', m]), 'key', parts.at(-1), ...[...mods].reverse().flatMap(m => ['kup', m]), 'sleep', 1500);
    const d2 = await desk();
    const st = d2.t ? await evalIn(d2.t, "JSON.stringify({mark:window.__mark===1,vis:document.visibilityState,keys:(window.__keys||[]).join(' ')})") : null;
    const s = st ? JSON.parse(st) : null;
    if (!s || !s.mark || s.vis != 'visible' || d2.n != d.n || /(^|\s)(Key\w|F\d+|Delete|Home|Escape)(\s|$)/.test(s.keys)) bad.push([c, s, d.n + '->' + d2.n]);
    if (!d2.t) break;
    if (d2.n > d.n) X('key', 'Escape', 'sleep', 500);
  }
  ok(`none of ${combos.length} Chromium shortcuts closes, reloads, covers or opens a window over the desktop (and the desktop stops each one it sees)`, !bad.length, bad);
  console.log(`${R.filter(x => x).length}/${R.length} passed`); process.exit(R.every(x => x) ? 0 : 1);
})().catch(e => { console.log('ERROR', e); process.exit(1) });
