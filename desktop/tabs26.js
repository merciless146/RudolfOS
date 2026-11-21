/* ===== V26 Oracle's real browser tabs (Rudolf OS 1.2) =====
   Inside the shell (shell26.js), a website in an Oracle tab is a real browser page: a Controlled Frame that the
   shell keeps over the tab's area of the Oracle window. The desktop says where that area is, many times a second,
   and which parts of it are covered by the desktop (menus, other windows, dialogs), so those parts are cut out of
   the page and the desktop shows through. Each Rudolf OS account has its own cookies and site storage.
   rtOn() is false outside the shell; Oracle then shows websites in frames as in 1.1. ===== */
const RT = new Map();          // tid -> {el (the tab's area), shown() -> bool, z() -> number, h (Oracle's handlers)}
let RT_N = 0, RT_RAF = 0, RT_FW = '';
const rtOn = () => !!(SHELL && SHELL_CAPS.includes('tabs'));
/* whose tab: each account gets its own partition (cookies, site storage).
   1.6.0 (plan-1.6.0 Part 21): by the account's ID, not its name (a deleted account made again with the same name does not
   get its websites); Guest's in memory only ("priv-…"), new for each Guest session and cleared at its sign-out. */
function rtPartOld(name) {   /* (before 1.6.0: a hash of the account's name) */
  const n = String(name || 'guest').toLowerCase();
  let h = 0x811c9dc5; for (let i = 0; i < n.length; i++) { h ^= n.charCodeAt(i); h = Math.imul(h, 0x01000193) >>> 0 }
  return 'u' + h.toString(36);
}
function rtPart() {
  const u = typeof USER != 'undefined' ? USER : null;
  if (u && u.r == 'guest') { if (!window.__rtGuest) window.__rtGuest = Math.random().toString(36).slice(2, 10); return 'priv-g' + window.__rtGuest }
  if (u && u.id) return 'a' + String(u.id).replace(/[^a-z0-9]/gi, '').slice(0, 32);
  return rtPartOld(u && u.n);
}
/* an account's websites forgotten (deleted account), and the pre-1.6.0 ones of every account (once: everyone starts fresh) */
const rtClear = part => { try { if (!part) return; if (typeof rtOn == 'function' && rtOn()) shellSend({rudolfShell: 'tab', op: 'clear', part, id: 0});
  else if (typeof oaNative == 'function' && oaNative()) rtWebClearAdd(part) } catch (e) {} };
/* 1.6.0 (plan-1.6.0 Part 21; the owner: "Rudolf OS on the native desktop should appear and function exactly as it does in Chromium
   mode"): on the native desktop an account is deleted in a window of its own (WebKit), while Oracle's tabs and their websites are
   Oracle's Chromium's. The partitions to clear are listed in the storage the pages share (rudolfd's copy, RT_WCK) and Oracle told
   (op "webclear"); Oracle's page clears them when told, and when it starts (Oracle closed at the time). */
const RT_WCK = 'bd95webclear';
function rtWebClearAdd(part) {
  try { let l = JSON.parse(localStorage.getItem(RT_WCK) || '[]'); if (!Array.isArray(l)) l = []; if (!l.includes(part)) l.push(part);
    localStorage.setItem(RT_WCK, JSON.stringify(l.slice(-60)));
    Promise.resolve(window.__rudolfLsFlush && window.__rudolfLsFlush()).catch(() => {}).then(() => { try { oaPost({op: 'webclear'}) } catch (e) {} }) } catch (e) {}
}
window.__oaWebClearIn = async () => {
  if (typeof rtOn != 'function' || !rtOn()) return 0; let l = null;
  try { if (typeof RD != 'undefined' && RD) { const r = await fetch('/api/lsync?key=' + RT_WCK, {headers: {'X-Rudolf-Token': RD.token}, cache: 'no-store'}); const j = r.ok ? await r.json() : null;
    if (j && typeof j.v == 'string') l = JSON.parse(j.v) } } catch (e) {}
  if (!l) try { l = JSON.parse(localStorage.getItem(RT_WCK) || 'null') } catch (e) {}
  if (!Array.isArray(l) || !l.length) return 0;
  l.filter(p => typeof p == 'string').forEach(p => shellSend({rudolfShell: 'tab', op: 'clear', part: p, id: 0}));
  try { localStorage.removeItem(RT_WCK); window.__rudolfLsFlush && window.__rudolfLsFlush() } catch (e) {}
  return l.length;
};
/* (Oracle's page starting: what was listed while it was closed, once its tabs can start) */
{ let n = 0; const t = setInterval(() => { try { if (typeof rtOn == 'function' && rtOn()) { clearInterval(t); window.__oaWebClearIn() } else if (++n > 90) clearInterval(t) } catch (e) { clearInterval(t) } }, 2000); }
/* the account signing out on the native desktop (rudolf-launch "logout", before Oracle's Chromium closes): as Standard's sign-out
   (core25.js userLogout) for Oracle: its tabs saved for "the tabs from last time", Guest's websites cleared */
window.__oaLogout = () => {
  try { window.__oraSave && window.__oraSave() } catch (e) {}
  try { if (typeof USER != 'undefined' && USER && USER.r == 'guest' && window.__rtGuest) { rtClear('priv-g' + window.__rtGuest); window.__rtGuest = null } } catch (e) {}
  try { typeof fsFlush == 'function' && fsFlush() } catch (e) {}
  try { window.__rudolfLsFlush && window.__rudolfLsFlush() } catch (e) {}
  return 1;
};
function rtForget(a) { if (!a) return; if (a.id) rtClear('a' + String(a.id).replace(/[^a-z0-9]/gi, '').slice(0, 32)); rtClear(rtPartOld(a.n)) }
const rtSend = (op, tid, o) => shellSend(Object.assign({rudolfShell: 'tab', op, tid}, o || {}));
/* a new real tab. h: {nav(url, back, fwd), load(on, error), title(t), focus(), close(), keys(k), newwin(url, ntid, popup),
   gone(why), dialog(kind, text, def) -> Promise<{ok, text}>, perm(perm, url) -> Promise<bool>, blocked(url, why), fs(on), flash(),
   download(url, {name, data, big, why})} */
function rtOpen(url, where, h, adopt) {
  const tid = 'o' + (++RT_N) + '-' + Date.now().toString(36);
  RT.set(tid, Object.assign({h, last: ''}, where));
  rtFw(true); rtRuffle();
  rtSend('open', tid, {url, part: where && where.priv && typeof rtPrivPart == 'function' ? rtPrivPart() : rtPart(), adopt});   // 1.5.9: a private window's tabs: private59.js
  rtLoop();
  return tid;
}
const rtGo = (tid, url) => rtSend('go', tid, {url});
const rtCmd = (tid, op, o) => rtSend(op, tid, o);
function rtClose(tid) { if (!RT.has(tid)) return; RT.delete(tid); rtSend('close', tid) }
/* 1.5.9 (game mode, plan 9.2): may a tab sleep? {m: sound or video playing, f: text typed and not sent}; no answer
   within 2 s counts as busy (the tab stays) */
const RT_BUSY = new Map(); let RT_BN = 0;
/* 1.5.9: a notice inside a tab's page, over its full screen too; with a button, f(tid) runs when a person presses it */
const RT_NOTICE = new Map(); let RT_NN = 0;
function rtNotice(tid, text, sec, btn, f) { const o = {text, s: sec || 12}; if (btn && f) { o.btn = btn; o.id = ++RT_NN; RT_NOTICE.set(o.id, f); setTimeout(() => RT_NOTICE.delete(o.id), (o.s + 5) * 1000) } rtSend('notice', tid, o) }
function rtBusy(tid) {
  if (!RT.has(tid)) return Promise.resolve({m: true, f: true});
  const id = ++RT_BN;
  return new Promise(res => { RT_BUSY.set(id, res); rtSend('busy', tid, {id}); setTimeout(() => { if (RT_BUSY.has(id)) { RT_BUSY.delete(id); res({m: true, f: true, late: true}) } }, 2000) });
}

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
    else if (ev == 'load') { if (/ERR_UNKNOWN_URL_SCHEME/.test(m.error || '') && typeof lxStoreLinkId == 'function' && lxStoreLinkId(m.url)) { lxStoreOpen(lxStoreLinkId(m.url)); try { rtSend('back', m.tid, {}) } catch (x) {} h.load(false, '') }
      /* 1.6.0: a link of a Linux app's own scheme (claude://... after signing in on the site): to that app, after asking (links60.js) */
      else if (/ERR_UNKNOWN_URL_SCHEME/.test(m.error || '') && typeof lkLinkOk == 'function' && lkLinkOk(m.url)) lkTab(t, m)
      else h.load(!!m.loading, m.error || '') }
    else if (ev == 'title') h.title(String(m.title || ''));
    else if (ev == 'hover') { if (h.hover) h.hover(String(m.url || '').slice(0, 2000)) }   /* (1.6.1: where a link goes: "Goes To: …") */
    else if (ev == 'focus') h.focus();
    else if (ev == 'find') { if (h.find) h.find({id: +m.id || 0, n: +m.n || 0, i: +m.i || 0, sel: typeof m.sel == 'string' ? m.sel.slice(0, 200) : undefined}) }   /* (1.6.1: find in page, orafind61.js) */
    else if (ev == 'close') h.close();
    else if (ev == 'keys') h.keys(String(m.k || ''));
    else if (ev == 'newwin' && typeof m.ntid == 'string') h.newwin(String(m.url || ''), m.ntid, !!m.popup);
    else if (ev == 'gone') h.gone(String(m.why || ''));
    else if (ev == 'notice') { const w = RT_NOTICE.get(+m.id); if (w) { RT_NOTICE.delete(+m.id); w(m.tid) } }   // 1.5.9: a notice's button
    else if (ev == 'tel') { if (typeof telOpen == 'function') telOpen(m.num) }   // 1.5.9 (plan 7.3): a phone number clicked in a tab
    else if (ev == 'busy') { const w = RT_BUSY.get(m.id); if (w) { RT_BUSY.delete(m.id); w({m: !!m.m, f: !!m.f}) } }
    else if (ev == 'fs') h.fs && h.fs(!!m.on);
    else if (ev == 'adblock') { if (h.adblock) h.adblock(+m.n || 0) }   /* (1.6.1: ads and trackers blocked, orablock61.js) */
    else if (ev == 'blocked') { if (m.main) h.blocked(String(m.url || ''), String(m.why || '')) }
    else if (ev == 'flash') h.flash && h.flash();
    else if (ev == 'still') rtStill(t, m.img);   /* (1.6.1, Lite: a covered tab's still picture) */
    else if (ev == 'download') h.download && h.download(String(m.url || ''), {name: String(m.name || ''), data: typeof m.data == 'string' && m.data.startsWith('data:') ? m.data : '', big: !!m.big, why: String(m.why || ''), started: !!m.started});
    else if (ev == 'dialog') h.dialog(String(m.kind || 'alert'), String(m.text || ''), String(m.def || '')).then(r => rtSend('dialog', m.tid, {did: m.did, ok: r.ok, text: r.text}));
    else if (ev == 'perm') h.perm(String(m.perm || ''), String(m.url || '')).then(a => rtSend('perm', m.tid, {pid: m.pid, allow: a}));
  } catch (x) { console.warn('real tab event', ev, x) }
});
/* 1.6.1 (Lite, docs/plan-1.6.1.md Part 3): while the desktop covers part of a tab (a menu, another window), rudolf-webkit
   sends a still picture of its page, shown here in the tab's place, under the desktop's menus and windows (a WebKit page
   cannot be cut on the screen); null: the live page is back. Standard's shell cuts its pages and never sends it. */
function rtStill(t, img) {
  const el = t && t.el; if (!el) return;
  const ok = typeof img == 'string' && img.length < 8e6 && /^data:image\/jpeg;base64,[A-Za-z0-9+/=]+$/.test(img);
  el.style.backgroundImage = ok ? 'url("' + img + '")' : ''; el.style.backgroundSize = ok ? '100% 100%' : ''; el.style.backgroundRepeat = ok ? 'no-repeat' : '';
}
/* keyboard focus back to the desktop (Ctrl+L in a real tab goes to Oracle's address bar) */
function rtBlur() { if (SHELL) shellSend({rudolfShell: 'tab', op: 'blur'}) }
/* 1.6.0 (plan-1.6.0 Part 21, the owner: everyone starts fresh): once on this computer, the websites every account had
   before (their name-keyed partitions) are forgotten; each account that had them is told once, in Oracle's words */
function rtFresh160() {
  if (typeof rtOn != 'function' || !rtOn() || typeof SYSC != 'object' || typeof ACCTS == 'undefined') return;
  if (!SYSC.web160) { const had = ACCTS.filter(a => a.r != 'guest').map(a => a.id || a.n); ACCTS.forEach(a => rtClear(rtPartOld(a.n))); rtClear(rtPartOld('guest'));
    SYSC.web160 = 1; SYSC.web160had = had; try { sysSave() } catch (e) {} }
  const u = typeof USER != 'undefined' ? USER : null;
  if (u && u.r != 'guest' && Array.isArray(SYSC.web160had) && SYSC.web160had.includes(u.id || u.n) && !S.web160) {
    S.web160 = 1; try { persistS() } catch (e) {}
    notify('Oracle', 'Websites are now kept separately for each account. Sign in to websites again.', {app: 'Oracle'}) }
}
(function () { const o = window.__afterLogin; window.__afterLogin = () => { o && o(); setTimeout(() => { try { rtFresh160() } catch (e) {} }, 3000) } })();
