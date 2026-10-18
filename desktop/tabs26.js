/* ===== V26 Oracle's real browser tabs (Rudolf OS 1.2) =====
   Inside the shell (shell26.js), a website in an Oracle tab is a real browser page: a Controlled Frame that the
   shell keeps over the tab's area of the Oracle window. The desktop says where that area is, many times a second,
   and which parts of it are covered by the desktop (menus, other windows, dialogs), so those parts are cut out of
   the page and the desktop shows through. Each Rudolf OS account has its own cookies and site storage.
   rtOn() is false outside the shell; Oracle then shows websites in frames as in 1.1. ===== */
const RT = new Map();          // tid -> {el (the tab's area), shown() -> bool, z() -> number, h (Oracle's handlers)}
let RT_N = 0, RT_RAF = 0, RT_FW = '';
const rtOn = () => !!(SHELL && SHELL_CAPS.includes('tabs'));
/* whose tab: each account gets its own partition (cookies, site storage) */
function rtPart() {
  const n = String(typeof USER != 'undefined' && USER && USER.n || 'guest').toLowerCase();
  let h = 0x811c9dc5; for (let i = 0; i < n.length; i++) { h ^= n.charCodeAt(i); h = Math.imul(h, 0x01000193) >>> 0 }
  return 'u' + h.toString(36);
}
const rtSend = (op, tid, o) => shellSend(Object.assign({rudolfShell: 'tab', op, tid}, o || {}));
/* a new real tab. h: {nav(url, back, fwd), load(on, error), title(t), focus(), close(), keys(k), newwin(url, ntid, popup),
   gone(why), dialog(kind, text, def) -> Promise<{ok, text}>, perm(perm, url) -> Promise<bool>, blocked(url, why), fs(on), flash(),
   download(url, {name, data, big, why})} */
function rtOpen(url, where, h, adopt) {
  const tid = 'o' + (++RT_N) + '-' + Date.now().toString(36);
  RT.set(tid, Object.assign({h, last: ''}, where));
  rtFw(true); rtRuffle();
  rtSend('open', tid, {url, part: rtPart(), adopt});
  rtLoop();
  return tid;
}
const rtGo = (tid, url) => rtSend('go', tid, {url});
const rtCmd = (tid, op, o) => rtSend(op, tid, o);
function rtClose(tid) { if (!RT.has(tid)) return; RT.delete(tid); rtSend('close', tid) }

/* ---- what covers a tab. Candidates are the desktop's top-level pieces (windows, the taskbar, the Start menu,
   menus, notifications, dialogs) and what the tab's own window puts over it; a candidate that overlaps the tab
   covers it when the topmost element at a few points of the overlap belongs to it. */
const rtR = r => [Math.round(r.left), Math.round(r.top), Math.round(r.right) - Math.round(r.left), Math.round(r.bottom) - Math.round(r.top)];
function rtSub(list, o) {
  const out = [], ox2 = o[0] + o[2], oy2 = o[1] + o[3];
  for (const [x, y, w, h] of list) {
    const x2 = x + w, y2 = y + h;
    if (o[0] >= x2 || ox2 <= x || o[1] >= y2 || oy2 <= y) { out.push([x, y, w, h]); continue }
    if (o[1] > y) out.push([x, y, w, o[1] - y]);
    if (oy2 < y2) out.push([x, oy2, w, y2 - oy2]);
    const ty = Math.max(y, o[1]), by = Math.min(y2, oy2);
    if (o[0] > x) out.push([x, ty, o[0] - x, by - ty]);
    if (ox2 < x2) out.push([ox2, ty, x2 - ox2, by - ty]);
  }
  return out;
}
function rtCover(el) {
  const R = rtR(el.getBoundingClientRect());
  if (R[2] < 2 || R[3] < 2) return null;
  const own = el.closest('.win'), cands = new Set();
  for (const c of document.body.children) { cands.add(c); for (const d of c.children) cands.add(d) }
  const dk = document.getElementById('desk'); if (dk) for (const c of dk.children) cands.add(c);
  if (own) for (const c of own.querySelector('.body') ? own.querySelector('.body').children : []) cands.add(c);
  const occ = [];
  for (const c of cands) {
    if (c === el || c.contains(el) || el.contains(c) || /^(SCRIPT|STYLE|LINK|META)$/.test(c.tagName)) continue;
    const r = c.getBoundingClientRect();
    const x = Math.max(R[0], Math.floor(r.left)), y = Math.max(R[1], Math.floor(r.top)), x2 = Math.min(R[0] + R[2], Math.ceil(r.right)), y2 = Math.min(R[1] + R[3], Math.ceil(r.bottom));
    if (x2 - x < 1 || y2 - y < 1) continue;
    const ix = Math.min(2, (x2 - x) / 4), iy = Math.min(2, (y2 - y) / 4);
    const pts = [[(x + x2) / 2, (y + y2) / 2], [x + ix, y + iy], [x2 - ix, y + iy], [x + ix, y2 - iy], [x2 - ix, y2 - iy]];
    if (pts.some(([px, py]) => { const hit = document.elementFromPoint(px, py); return hit && c.contains(hit) })) occ.push([x, y, x2 - x, y2 - y]);
  }
  let vis = [R];
  for (const o of occ) vis = rtSub(vis, o);
  return {r: R, vis: occ.length ? vis.filter(v => v[2] > 0 && v[3] > 0).map(([x, y, w, h]) => [x - R[0], y - R[1], w, h]) : null};
}
/* every frame while there are tabs: tell the shell where each one is, when that changed */
function rtLoop() {
  if (RT_RAF || !RT.size) return;
  const step = () => {
    RT_RAF = 0; if (!RT.size) return;
    if (!document.hidden) for (const [tid, t] of RT) {
      let c = null;
      try { if (t.shown() && !SHELL_FS) c = rtCover(t.el) } catch (e) {}
      const z = c ? t.z() : 0;
      const k = JSON.stringify([c, z]);
      if (k !== t.last) { t.last = k; rtSend('place', tid, c ? {r: c.r, vis: c.vis, z} : {r: null}) }
    }
    RT_RAF = requestAnimationFrame(step);
  };
  RT_RAF = requestAnimationFrame(step);
}
/* the firewall applies to every request a tab makes, so the shell gets its rules */
function rtFw(force) {
  if (!SHELL || typeof SECC == 'undefined' || !window.__sys) return;
  const f = SECC.fw, k = JSON.stringify(f);
  if (k === RT_FW && !force) return;
  RT_FW = k; shellSend({rudolfShell: 'tab', op: 'fw', rules: {on: !!f.on, mode: f.mode, block: f.block, trust: f.trust}});
}
setInterval(() => { if (RT.size) { rtFw(); rtRuffle() } }, 2000);
/* Ruffle for Flash content in real tabs: the same settings the "Ruffle for Oracle" extension gets (oext25.js) */
let RT_RUF = '';
function rtRuffle() {
  if (!SHELL || typeof oextWant != 'function') return;
  let c; try { c = oextWant() } catch (e) { return }
  const k = JSON.stringify(c); if (k === RT_RUF) return;
  RT_RUF = k; shellSend({rudolfShell: 'tab', op: 'ruffle', cfg: c});
}

/* a page's alert, confirm or prompt, shown over its Oracle window */
function rtDialog(W, kind, text, def) {
  return new Promise(res => {
    const o = document.createElement('div'); o.className = 'dlg sdlg'; o.setAttribute('role', 'alertdialog');
    o.innerHTML = `<div style='max-width:420px'><div class='sdm' style='white-space:pre-wrap;word-break:break-word;max-height:50vh;overflow:auto'></div>${kind == 'prompt' ? "<input class='rtp' style='width:100%;margin-top:10px;box-sizing:border-box'>" : ''}<div style='text-align:right;margin-top:14px;display:flex;gap:6px;justify-content:flex-end'>${kind == 'alert' ? '' : "<button class='rtc'>Cancel</button>"}<button class='rto' style='background:var(--ac);color:#fff'>OK</button></div></div>`;
    o.querySelector('.sdm').textContent = text;
    const inp = o.querySelector('.rtp'); if (inp) inp.value = def || '';
    const done = ok => { o.remove(); res({ok, text: inp ? inp.value : ''}) };
    o.querySelector('.rto').onclick = () => done(true);
    if (o.querySelector('.rtc')) o.querySelector('.rtc').onclick = () => done(false);
    o.addEventListener('keydown', e => { if (e.key == 'Enter') done(true); else if (e.key == 'Escape') done(kind == 'alert') });
    W.body.style.position = 'relative'; W.body.appendChild(o);
    (inp || o.querySelector('.rto')).focus();
  });
}
/* a page asking for the camera, the microphone or the location: asked once per site while Oracle is open */
const RT_PERM = new Map();
function rtPerm(W, perm, url) {
  let host = ''; try { host = new URL(url).host } catch (e) {}
  const k = perm + ' ' + host;
  if (RT_PERM.has(k)) return Promise.resolve(RT_PERM.get(k));
  const what = perm == 'media' ? 'use your camera or microphone' : 'know your location';
  return new Promise(res => sdlg(W, 'Allow ' + (host || 'this site') + '?', (host || 'This site') + ' wants to ' + what + '.',
    [['Block', () => { RT_PERM.set(k, false); res(false) }], ['Allow', () => { RT_PERM.set(k, true); res(true) }, 1]]));
}

/* events from the shell */
addEventListener('message', e => {
  if (!SHELL || e.source !== SHELL) return;
  const m = e.data; if (!m || m.rudolfShell != 'tabev') return;
  const t = RT.get(m.tid); if (!t) return;
  const h = t.h, ev = m.ev;
  try {
    if (ev == 'nav' && /^https?:/i.test(m.url || '')) h.nav(m.url, !!m.back, !!m.fwd);
    else if (ev == 'load') h.load(!!m.loading, m.error || '');
    else if (ev == 'title') h.title(String(m.title || ''));
    else if (ev == 'focus') h.focus();
    else if (ev == 'close') h.close();
    else if (ev == 'keys') h.keys(String(m.k || ''));
    else if (ev == 'newwin' && typeof m.ntid == 'string') h.newwin(String(m.url || ''), m.ntid, !!m.popup);
    else if (ev == 'gone') h.gone(String(m.why || ''));
    else if (ev == 'fs') h.fs && h.fs(!!m.on);
    else if (ev == 'blocked') { if (m.main) h.blocked(String(m.url || ''), String(m.why || '')) }
    else if (ev == 'flash') h.flash && h.flash();
    else if (ev == 'download') h.download && h.download(String(m.url || ''), {name: String(m.name || ''), data: typeof m.data == 'string' && m.data.startsWith('data:') ? m.data : '', big: !!m.big, why: String(m.why || '')});
    else if (ev == 'dialog') h.dialog(String(m.kind || 'alert'), String(m.text || ''), String(m.def || '')).then(r => rtSend('dialog', m.tid, {did: m.did, ok: r.ok, text: r.text}));
    else if (ev == 'perm') h.perm(String(m.perm || ''), String(m.url || '')).then(a => rtSend('perm', m.tid, {pid: m.pid, allow: a}));
  } catch (x) { console.warn('real tab event', ev, x) }
});
/* keyboard focus back to the desktop (Ctrl+L in a real tab goes to Oracle's address bar) */
function rtBlur() { if (SHELL) shellSend({rudolfShell: 'tab', op: 'blur'}) }
