/* ===== Rudolf OS 1.6.1 (docs/plan-1.6.1.md Part 10, docs/plan-oracle.md item 5): Oracle's ad and tracker blocking =====
   A shield 🛡 in the address box with the number blocked on this page. Its menu: "Blocking ads and trackers on example.com", Turn off
   (on) for this site, Report a broken site (a note in System health: nothing is sent anywhere), Settings… Settings › Privacy › Ads and
   trackers in Oracle: on (the default) or off, the sites it is off for, and sites of your own to block. Private and Tor windows always
   block. Everything is checked on this computer: the lists come with Rudolf OS, and nothing about the pages you visit leaves it.
   - The list (ORA_BLOCK_LIST, desktop/orablock61.json, made by iso/systools/blocklist.py from filter lists in Adblock Plus syntax):
     Rudolf's own starter list (iso/systools/blocklists/rudolf.txt); EasyList and EasyPrivacy, in the same syntax, can be compiled
     with it. Whole domains only: blocked (b), blocked from other sites only (tp), never blocked (ex).
   - Real tabs (Standard, Native, the encrypted Oracle): the shell's request interceptor (the firewall's) cancels a request to such a
     domain (never the page itself) and counts it (tabev adblock {n}); Lite: rudolf-webkit compiles the same lists into WebKit's own
     content blocker for each tab, and its page script counts what the page's elements asked for from them.
   - Websites in plain frames (no shell): not possible; the shield says so. ===== */
const ORA_BLOCK_LIST = null;   /* (desktop/build19.py puts desktop/orablock61.json here) */
const ORA_BLOCK_DOM = /^[a-z0-9]([a-z0-9-]*[a-z0-9])?(\.[a-z0-9]([a-z0-9-]*[a-z0-9])?)+$/;
const oraBlockSet = () => { const o = typeof S == 'object' && S.oblock && typeof S.oblock == 'object' ? S.oblock : {};
  return {on: o.on !== false, off: Array.isArray(o.off) ? o.off.filter(x => ORA_BLOCK_DOM.test(x)).slice(0, 500) : [], mine: Array.isArray(o.mine) ? o.mine.filter(x => ORA_BLOCK_DOM.test(x)).slice(0, 2000) : []} };
function oraBlockSave(o) { if (typeof S != 'object') return; S.oblock = {on: o.on !== false, off: [...new Set(o.off || [])], mine: [...new Set(o.mine || [])]}; try { persistS() } catch (e) {} oraBlockSend(true) }
/* what the shell (or rudolf-webkit) is told: the lists, the switch, the sites it is off for */
function oraBlockCfg() { const L = ORA_BLOCK_LIST || {b: [], tp: [], ex: []}, o = oraBlockSet();
  return {on: o.on, b: (L.b || []).concat(o.mine), tp: L.tp || [], ex: L.ex || [], off: o.off, v: L.v || 0} }
let ORA_BLOCK_SENT = '';
function oraBlockSend(force) {
  try { if (typeof rtOn != 'function' || !rtOn()) return; const c = oraBlockCfg(), k = JSON.stringify([c.on, c.off, c.b.length, c.tp.length, c.ex.length, c.v, oraBlockSet().mine]);
    if (k === ORA_BLOCK_SENT && !force) return; ORA_BLOCK_SENT = k; shellSend({rudolfShell: 'tab', op: 'block', cfg: c}) } catch (e) {}
}
/* (sent before the first tab, again whenever the shell says hello: a shell started again knows nothing) */
if (typeof rtOpen == 'function') { const _ro = rtOpen; rtOpen = function () { oraBlockSend(); return _ro.apply(this, arguments) } }
addEventListener('message', e => { const m = e.data; if (m && m.rudolfShell == 'hello') ORA_BLOCK_SENT = '' });
setInterval(() => { try { if (typeof RT != 'undefined' && RT.size) oraBlockSend() } catch (e) {} }, 2000);
const oraBlockHost = u => { try { return new URL(u).hostname.toLowerCase() } catch (e) { return '' } };
const oraBlockIn = (h, L) => (L || []).some(x => h == x || h.endsWith('.' + x));
const ORA_BLOCK_TABS = new WeakMap();
function oraBlock(c) {
  const W = c.W, T = c.T, q = s => W.body.querySelector(s), priv = !!(c.priv || c.tor);
  const b = document.createElement('button'); b.className = 'oblk'; b.title = 'Ads and trackers blocked on this page'; b.setAttribute('aria-label', 'Ads and trackers'); b.hidden = true;
  const st = q('.ob .st'); if (st) st.before(b); else q('.ob').appendChild(b);
  const urlOf = t => t && t.h ? t.h[t.i] || '' : '';
  const state = t => { const u = urlOf(t), h = oraBlockHost(u), o = oraBlockSet();
    if (!/^https?:/i.test(u)) return null;
    if (!t.real) return {h, n: 0, why: 'frame'};
    if (priv) return {h, n: t.bn || 0, why: 'priv'};
    if (!o.on) return {h, n: 0, why: 'off'};
    if (oraBlockIn(h, o.off)) return {h, n: 0, why: 'site'};
    return {h, n: t.bn || 0, why: ''} };
  const sync = () => { (c.tabs() || []).forEach(x => ORA_BLOCK_TABS.set(x, api)); const t = T(), s = t && state(t);
    if (t && t.bnU !== urlOf(t)) { t.bnU = urlOf(t); if (!t.real) t.bn = 0 }
    b.hidden = !s; if (!s) return; const on = !s.why || s.why == 'priv';
    b.textContent = '🛡' + (on && s.n ? ' ' + (s.n > 99 ? '99+' : s.n) : ''); b.classList.toggle('oblkoff', !on);
    b.title = on ? (s.n ? s.n + ' ad and tracker request' + (s.n == 1 ? '' : 's') + ' blocked on this page' : 'Blocking ads and trackers') : s.why == 'frame' ? 'Blocking is not possible for this page' : 'Blocking is off ' + (s.why == 'site' ? 'for this site' : 'in Settings') };
  const count = (t, n) => { t.bn = Math.max(0, n | 0); t.bnU = urlOf(t); if (t === T()) sync() };
  /* the shield's menu */
  const menu = () => { const t = T(), s = t && state(t); if (!s) return; W.body.querySelectorAll('.odd').forEach(x => x.remove());
    const d = document.createElement('div'); d.className = 'odd oblkm'; d.setAttribute('role', 'menu');
    const row = (txt, f, cls) => { const r = document.createElement('div'); r.textContent = txt; if (cls) r.className = cls; if (f) { r.setAttribute('role', 'menuitem'); r.onclick = () => { d.remove(); f() } } d.appendChild(r); return r };
    row(s.why == 'frame' ? 'Blocking is not possible here: this website is shown in a frame' : (s.why == 'site' || s.why == 'off' ? 'Blocking is off on ' : 'Blocking ads and trackers on ') + s.h, null, 'oblkh');
    if (!s.why || s.why == 'priv') row(s.n ? s.n + ' blocked on this page' : 'Nothing blocked on this page yet', null, 'oblkn');
    if (s.why == 'priv') row(c.tor ? 'Tor windows always block' : 'Private windows always block', null, 'oblkn');
    else if (s.why == '' || s.why == 'site') row(s.why == 'site' ? 'Turn on for this site' : 'Turn off for this site', () => { const o = oraBlockSet(), dom = s.h.replace(/^www\./, '');
      o.off = s.why == 'site' ? o.off.filter(x => !(s.h == x || s.h.endsWith('.' + x))) : o.off.concat([dom]); oraBlockSave(o); if (t.rt) rtCmd(t.rt, 'reload'); sync() });
    if (s.why != 'frame') row('Report a broken site', () => { try { if (typeof hlNote == 'function') hlNote('firewall', 'Oracle: ' + s.h + ' may not work with ad and tracker blocking (' + (s.why ? 'blocking ' + (s.why == 'priv' ? 'on' : 'off') : s.n + ' blocked') + '). Noted here only: nothing was sent.') } catch (e) {}
      notify('Oracle', 'Noted in System health. Nothing was sent anywhere. You can turn blocking off for ' + s.h + ' from the shield.', {app: 'Oracle'}) });
    row('Settings…', () => { try { settings('priv') } catch (e) {} });
    W.body.style.position = 'relative'; W.body.appendChild(d);
    const kill = ev => { if (!d.contains(ev.target) && ev.target !== b) { d.remove(); document.removeEventListener('pointerdown', kill, true) } }; setTimeout(() => document.addEventListener('pointerdown', kill, true), 0) };
  b.onclick = e => { e.stopPropagation(); const o = W.body.querySelector('.oblkm'); if (o) { o.remove(); return } menu() };
  const api = {sync, count, menu};
  return api;
}
/* Settings › Privacy › Ads and trackers in Oracle */
function oraBlockPanel(c) {
  const box = document.createElement('div'); box.className = 'oblkbox'; c.appendChild(box);
  const L = ORA_BLOCK_LIST || {b: [], tp: []}, n = (L.b || []).length + (L.tp || []).length;
  const h = (tag, txt, cls) => { const e = document.createElement(tag); if (txt != null) e.textContent = txt; if (cls) e.className = cls; return e };
  const draw = () => { const o = oraBlockSet(); box.replaceChildren();
    box.append(h('h4', 'Ads and trackers in Oracle', 'sh4'));
    const lb = h('label', null, 'ck'), cb = h('input'); cb.type = 'checkbox'; cb.className = 'oblkon'; cb.checked = o.on; lb.append(cb, ' Block ads and trackers on websites'); box.append(lb);
    cb.onchange = () => { const x = oraBlockSet(); x.on = cb.checked; oraBlockSave(x); draw() };
    box.append(h('p', 'Rudolf OS\'s own list blocks ' + n + ' ad and tracker services' + ((L.src || []).length > 1 ? ' (' + L.src.join(', ') + ')' : '') + '. Everything is checked on this computer: nothing about the websites you visit is sent anywhere. Private and Tor windows always block. Pages loaded again use the change.', 'msg'));
    const list = (title, items, cls, rm) => { box.append(h('div', title, 'oblkt')); const u = h('div', null, cls); if (!items.length) u.append(h('div', 'None', 'msg'));
      items.forEach(x => { const r = h('div', null, 'oblkr'), x1 = h('button', '✕'); x1.title = 'Remove ' + x; x1.setAttribute('aria-label', 'Remove ' + x); x1.onclick = () => { rm(x); draw() }; r.append(h('span', x), x1); u.append(r) }); box.append(u) };
    list('Sites where blocking is off (from the shield)', o.off, 'oblkoffl', x => { const y = oraBlockSet(); y.off = y.off.filter(z => z != x); oraBlockSave(y) });
    list('Sites you block too', o.mine, 'oblkmine', x => { const y = oraBlockSet(); y.mine = y.mine.filter(z => z != x); oraBlockSave(y) });
    const add = h('div', null, 'oblkadd'), inp = h('input'), go = h('button', 'Block'); inp.placeholder = 'a site, such as ads.example.com'; inp.setAttribute('aria-label', 'A site to block'); inp.spellcheck = false;
    const m = h('span', '', 'msg'); add.append(inp, go, m); box.append(add);
    go.onclick = () => { let v = String(inp.value || '').trim().toLowerCase().replace(/^[a-z]+:\/\//, '').replace(/[\/:?#].*$/, '').replace(/^\*\./, '');
      if (!ORA_BLOCK_DOM.test(v)) { m.textContent = 'That is not a site\'s name.'; return } const y = oraBlockSet(); if (!y.mine.includes(v)) y.mine.push(v); oraBlockSave(y); draw() };
    inp.onkeydown = e => { if (e.key == 'Enter') go.onclick() } };
  draw();
}
if (typeof privPanel == 'function') { const _pp = privPanel; privPanel = function (c) { const r = _pp.apply(this, arguments); try { oraBlockPanel(c) } catch (e) { console.warn('orablock61', e) } return r } }
(function () { const s = document.createElement('style'); s.textContent = '.ob .oblk{padding:2px 6px;white-space:nowrap}.ob .oblk[hidden]{display:none}.ob .oblk.oblkoff{opacity:.55}.odd.oblkm .oblkh{font-weight:600;cursor:default}.odd.oblkm .oblkn{color:#6b7280;cursor:default}'
  + '.oblkbox .oblkt{margin-top:8px;font-weight:600}.oblkbox .oblkr{display:flex;gap:8px;align-items:center}.oblkbox .oblkr button{padding:0 6px}.oblkbox .oblkadd{display:flex;gap:6px;align-items:center;margin-top:6px}';
  (document.head || document.documentElement).appendChild(s) })();
