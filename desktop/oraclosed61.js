/* ===== Rudolf OS 1.6.1 (docs/plan-1.6.1.md Part 10, docs/plan-oracle.md item 2): Oracle's closed tabs =====
   Ctrl+Shift+T, or ☰ › Reopen closed tab, reopens the last closed tab at its place, with Oracle's back and forward list for it;
   pressed again, the one before. ☰ › Recently closed… lists them (the last 25; a closed window is one entry that brings its tabs
   back, in a window of their own). Kept in memory only, never written anywhere: the list lasts while Oracle is open (closing
   Oracle's last window forgets it, as on Native, where Oracle's Chromium closes with its last window) and is forgotten at sign-out
   (Standard's userLogout, Native's __oaLogout). Each account's own: an entry is reopened only by the account that closed it.
   Private and Tor windows keep their own list, which goes with the window; nothing of theirs enters the shared one.
   On Native (and Standard's encrypted Oracle) each Oracle window is a page of its own: a closed tab stays in its window's page; a
   closed window's entry goes to the other Oracle windows' pages (a BroadcastChannel, in memory, inside Oracle's Chromium), and is
   reopened there through the native desktop (op untor: a normal Oracle window with those tabs) or rudolfd's orawin/open.
   browser() calls oraClosed({...}) once per window. ===== */
const ORA_CL = {l: [], n: 0, off: false};   /* the shared list (newest last) */
const ORA_CL_MAX = 25, ORA_CL_TABS = new WeakMap();
const oraClUid = () => { try { return typeof USER != 'undefined' && USER ? String(USER.id || USER.n || '') : '' } catch (e) { return '' } };
const oraClMulti = () => typeof ONEAPP == 'object' && !!ONEAPP;   /* (one Oracle window per page: Native, the encrypted Oracle) */
let ORA_CL_BC = null;
try { if (typeof BroadcastChannel == 'function') { ORA_CL_BC = new BroadcastChannel('rudolf-oracle-closed');
  ORA_CL_BC.onmessage = e => { const m = e.data || {}; if (!oraClMulti() || ORA_CL.off) return;
    if (m.op == 'add' && m.e && m.e.uid && m.e.uid === oraClUid() && !ORA_CL.l.some(x => x.id === m.e.id)) { ORA_CL.l.push(m.e); ORA_CL.l = ORA_CL.l.slice(-ORA_CL_MAX) }
    else if (m.op == 'del') ORA_CL.l = ORA_CL.l.filter(x => x.id !== m.id);
    else if (m.op == 'clear') ORA_CL.l = [];
    else if (m.op == 'hello' && ORA_CL.l.length) ORA_CL_BC.postMessage({op: 'all', l: ORA_CL.l.filter(x => x.uid === m.uid)});
    else if (m.op == 'all' && Array.isArray(m.l)) { m.l.forEach(x => { if (x && x.uid === oraClUid() && !ORA_CL.l.some(y => y.id === x.id)) ORA_CL.l.push(x) }); ORA_CL.l.sort((a, b) => a.at - b.at); ORA_CL.l = ORA_CL.l.slice(-ORA_CL_MAX) } } } } catch (e) { ORA_CL_BC = null }
const oraClSay = m => { try { if (ORA_CL_BC && oraClMulti()) ORA_CL_BC.postMessage(m) } catch (e) {} };
/* forgotten at sign-out, and not taken again until someone signs in */
function oraClForget() { ORA_CL.l = []; ORA_CL.off = true; oraClSay({op: 'clear'}) }
if (typeof userLogout == 'function') { const _lo = userLogout; userLogout = function () { oraClForget(); return _lo.apply(this, arguments) } }
if (typeof userLogin == 'function') { const _li = userLogin; userLogin = async function () { const r = await _li.apply(this, arguments); ORA_CL.l = ORA_CL.l.filter(x => x.uid === oraClUid()); ORA_CL.off = false; return r } }
{ const _ol = window.__oaLogout; window.__oaLogout = () => { oraClForget(); return _ol ? _ol() : 1 } }
/* an entry kept: a tab {h, i, title, zoom, pos} or a window {tabs: [{u, title}]} */
const oraClHost = u => { try { return new URL(u).host } catch (e) { return '' } };
const oraClWorth = u => !!u && !/^about:(home|blank|newtab)/.test(u);
function oraClosed(c) {
  const W = c.W, own = [];   /* (a private or Tor window's own list) */
  const priv = !!(c.priv || c.tor);
  const list = () => priv ? own : ORA_CL.l.filter(x => x.uid === oraClUid());
  const keep = e => { e.at = Date.now(); e.id = 'c' + (++ORA_CL.n) + '-' + Math.random().toString(36).slice(2, 8);
    if (priv) { own.push(e); own.splice(0, Math.max(0, own.length - ORA_CL_MAX)); return }
    if (ORA_CL.off || !oraClUid()) return; e.uid = oraClUid(); ORA_CL.l.push(e); ORA_CL.l = ORA_CL.l.slice(-ORA_CL_MAX); oraClSay({op: 'add', e}) };   /* (Native: the other windows' pages too) */
  const drop = e => { if (priv) { const k = own.indexOf(e); if (k >= 0) own.splice(k, 1); return } ORA_CL.l = ORA_CL.l.filter(x => x !== e); oraClSay({op: 'del', id: e.id}) };
  const tabEntry = (t, pos) => { const h = (t.h || []).slice(), i = Math.max(0, Math.min(h.length - 1, t.i | 0)), u = h[i];
    if (!oraClWorth(u)) return null; const a = Math.max(0, i - 49); return {k: 'tab', h: h.slice(a, a + 50).map(x => String(x).slice(0, 2000)), i: i - a, title: String(t.title || oraClHost(u) || u).slice(0, 200), zoom: +t.zoom || 1, pos} };
  /* a tab closed (not the window's last: that closes the window) */
  const tabClosed = (t, pos) => { const e = tabEntry(t, pos); if (e) keep(e) };
  /* the window closed: one entry for its tabs; with one tab, a tab's (reopened as a tab of another window) */
  const winClosed = tabs => { if (priv) return;
    const L = (tabs || []).map((t, k) => tabEntry(t, k)).filter(Boolean); if (!L.length || ORA_CL.off || !oraClUid()) return;
    if (!oraClMulti()) { const others = typeof wins != 'undefined' && wins.some(w => w !== W && w.app == 'browser' && !w.priv && !w.tor && w.el && w.el.isConnected);
      if (!others) { ORA_CL.l = ORA_CL.l.filter(x => x.uid !== oraClUid()); return } }   /* (Oracle's last window: the list goes with it) */
    const e = L.length == 1 ? L[0] : {k: 'win', tabs: L.map(x => ({u: x.h[x.i], title: x.title})).slice(0, 12), title: L.length + ' tabs'};
    keep(e) };
  /* reopened: a tab in this window (c.add), a window as a window of its own */
  const reopen = e => { e = e || list().slice(-1)[0]; if (!e) { c.say('No recently closed tabs'); return false } drop(e);
    if (e.k == 'tab') { c.add(e.h, e.i, e.title, e.zoom, e.pos); return true }
    const urls = (e.tabs || []).map(x => x.u).filter(u => typeof u == 'string' && /^(https?:\/\/|about:|file:\/\/\/)/i.test(u)).slice(0, 12); if (!urls.length) return false;
    if (oraClMulti() && typeof oaPost == 'function' && typeof oaOut == 'function' && oaOut() && !(ONEAPP.q && ONEAPP.q.get('enc') == '1')) { oaPost({op: 'untor', url: urls.join('\n')}); return true }
    if (oraClMulti() && ONEAPP.q && ONEAPP.q.get('enc') == '1' && typeof rdPost == 'function') { rdPost('orawin/open?' + urls.map(u => 'url=' + encodeURIComponent(u)).join('&')).catch(() => {}); return true }
    ORA_CL.next = urls.slice(1); try { browser(urls[0]) } finally { setTimeout(() => { ORA_CL.next = null }, 0) } return true };
  /* ☰ › Recently closed…: a list under the menu button, newest first */
  const menu = () => { const L = list().slice().reverse().slice(0, 15); W.body.querySelectorAll('.odd').forEach(x => x.remove());
    const d = document.createElement('div'); d.className = 'odd ocl'; d.setAttribute('role', 'menu');
    if (!L.length) { const r = document.createElement('div'); r.className = 'oclno'; r.textContent = 'No recently closed tabs'; d.appendChild(r) }
    L.forEach(e => { const r = document.createElement('div'); r.setAttribute('role', 'menuitem');
      const u = e.k == 'tab' ? e.h[e.i] : '', ic = e.k == 'win' ? '🗗 ' : '↺ ';
      r.textContent = ic + (e.k == 'win' ? 'Window: ' + e.tabs.map(x => x.title).slice(0, 3).join(', ') + (e.tabs.length > 3 ? '…' : '') + ' (' + e.tabs.length + ' tabs)' : e.title);
      r.title = e.k == 'win' ? e.tabs.map(x => x.u).join('\n') : u; r.onclick = () => { d.remove(); reopen(e) }; d.appendChild(r) });
    W.body.style.position = 'relative'; W.body.appendChild(d);
    const kill = ev => { if (!d.contains(ev.target)) { d.remove(); document.removeEventListener('pointerdown', kill, true) } }; setTimeout(() => document.addEventListener('pointerdown', kill, true), 0) };
  /* Ctrl+Shift+T in Oracle's own window (on Standard it comes as Ctrl+Shift+F21 from rudolf-display, turned back into T by shell26.js) */
  W.el.addEventListener('keydown', e => { if ((e.ctrlKey || e.metaKey) && e.shiftKey && !e.altKey && e.code == 'KeyT') { e.preventDefault(); e.stopPropagation(); reopen() } });
  const api = {tabClosed, winClosed, reopen, menu, list, sync: () => (c.tabs() || []).forEach(x => ORA_CL_TABS.set(x, api)),
    /* (a window opened by reopen: the closed window's other tabs) */
    opened: () => { const n = ORA_CL.next; ORA_CL.next = null; if (Array.isArray(n) && n.length) setTimeout(() => n.forEach(u => { try { W.oNewTab(u) } catch (e) {} }), 0) }};
  return api;
}
/* a new Oracle page on Native: the other windows' list (they answer on the channel) */
if (oraClMulti()) setTimeout(() => oraClSay({op: 'hello', uid: oraClUid()}), 3000);
/* ☰ › Reopen closed tab, Recently closed… */
{ const o = window.__oracleMenu; window.__oracleMenu = x => { const a = x && x.tab && ORA_CL_TABS.get(x.tab), r = o ? o(x) : [];
  return a ? r.concat([['Reopen closed tab (Ctrl+Shift+T)', () => a.reopen()], ['Recently closed…', () => a.menu()]]) : r } }
(function () { const s = document.createElement('style'); s.textContent = '.odd.ocl{max-width:420px}.odd.ocl>div{white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.odd.ocl .oclno{color:#6b7280;cursor:default}';
  (document.head || document.documentElement).appendChild(s) })();
