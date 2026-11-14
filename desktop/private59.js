/* ===== Rudolf OS 1.5.9: private browsing in Oracle (asked for on 2026-11-14)
   A private window (Oracle's ☰ › New private tab, Ctrl+Shift+N, or right-click Oracle's icon › New private tab on the
   taskbar, the Start menu, the desktop or an open Oracle window's taskbar button):
   - its tabs are real browser pages with cookies and site storage of their own, kept in memory only (a part named
     "priv-…": the shell gives it a partition without persist:, rudolf-webkit an ephemeral context), so the account's
     sign-ins are not seen there and nothing done there stays; when the last private window closes, its data is
     cleared and the next private window starts afresh (a new name);
   - Oracle keeps no history of it (browser()'s hadd), and the session does not reopen it;
   - bookmarks made and files downloaded are kept, as in other browsers; the private start page says so.
   New private tab while a private window is open adds a tab to it. ===== */
let PRIV_ID = null; const PRIV_W = new Set();
/* the part for a private window's tabs: the account's, made private, and new for each private session */
function rtPrivPart() { if (!PRIV_ID) PRIV_ID = Math.random().toString(36).slice(2, 8); return ('priv-' + rtPart() + '-' + PRIV_ID).slice(0, 40) }
function privOpened(W) { PRIV_W.add(W) }
function privClosed(W) {
  PRIV_W.delete(W); if (PRIV_W.size || !PRIV_ID) return;
  const part = rtPrivPart(); PRIV_ID = null;
  try { if (typeof rtOn == 'function' && rtOn()) shellSend({rudolfShell: 'tab', op: 'clear', part, id: 0}) } catch (e) {}
}
/* New private tab: a tab in the private window already open, else a new private window */
function oraclePrivate(u) {
  const W = [...PRIV_W].filter(w => w.el && w.el.isConnected).pop();
  if (W) { W.focus(); if (W.oNewTab) W.oNewTab(u); return W }
  browser(u || null, {priv: 1});
}
/* the private window's start page (browser()'s about:home in a private window) */
function privHome(doc, esc) {
  const real = typeof rtOn == 'function' && rtOn();
  return doc('Private browsing', `<div style='max-width:560px;margin:10px auto;line-height:1.55'>
 <div style='font-size:46px;text-align:center'>🕶️</div>
 <h2 style='text-align:center;margin:.2em 0 .6em'>You are browsing privately</h2>
 <p>Oracle forgets the pages you visit in this window: they are not kept in History, and when you close the last
 private window their cookies and site data go too. Websites you are signed in to in other windows do not know you here.</p>
 <p><b>Kept:</b> bookmarks you make and files you download.</p>
 <p><b>Still seen:</b> the websites you visit, your network (at school or work, its administrator) and your internet provider can still see what you visit.</p>
 ${real ? '' : `<p style='color:#b45309'>Websites here are shown in frames (Oracle's own tabs are not running), and frames use the desktop's storage: they are not private.</p>`}
 <p style='color:#6b7280'>Type an address or search in the box above. <b>Ctrl+Shift+N</b> opens another private tab.</p></div>`);
}
/* the app's own entries in its right-click menus (a "jump list"): Oracle's New private tab */
const APP_JUMPS = {browser: () => [['New private tab', () => { try { $('#menu').style.display = 'none' } catch (e) {} oraclePrivate() }, true, {icon: '🕶️', key: 'Ctrl+Shift+N'}]]};
let CM_APP = null;
{const _cm = cmenu; cmenu = function (x, y, items, o) {
  let k = CM_APP; CM_APP = null;
  if (!k && o && o.title) k = Object.keys(APP_JUMPS).find(a => APPS[a] && APPS[a].name == o.title) || null;
  if (k && APP_JUMPS[k] && Array.isArray(items)) {
    const add = APP_JUMPS[k]().filter(j => !items.some(i => Array.isArray(i) && i[0] == j[0]));
    if (add.length) { items = items.slice(); const at = items.findIndex(i => Array.isArray(i)); items.splice(at < 0 ? 0 : at + 1, 0, ...add) }
  }
  return _cm.call(this, x, y, items, o)
}}
/* the taskbar button of an open window, and a desktop icon: their menus say which app they are */
{const _wm = winMenu; winMenu = function (W, x, y) { CM_APP = W && W.app || null; try { return _wm.apply(this, arguments) } finally { CM_APP = null } }}
{const _im = iconMenu; iconMenu = function (d, x, y) { try { CM_APP = typeof deskAppOf == 'function' ? deskAppOf(d) || null : null } catch (e) { CM_APP = null } try { return _im.apply(this, arguments) } finally { CM_APP = null } }}
/* the look: a dark violet toolbar and tabs, as other browsers mark a private window */
{const st = document.createElement('style'); st.textContent = `.opriv .otabs{background:#2e1065!important}.opriv .otab{background:#4c1d95!important;color:#ede9fe!important}.opriv .otab.on{background:#6d28d9!important;color:#fff!important}
.opriv .ob{background:#3b0764!important;color:#f5f3ff}.opriv .ob button{color:#f5f3ff}.opriv .ob input{background:#1e1b4b!important;color:#f5f3ff!important;border-color:#6d28d9!important}
.opriv .obm{background:#2e1065!important}.opriv .obm button{color:#ddd6fe!important}.opriv .stat{background:#2e1065!important;color:#c4b5fd!important}
.opv{display:inline-flex;align-items:center;gap:4px;padding:2px 8px;margin:0 4px;border-radius:999px;background:#7c3aed;color:#fff;font-size:12px;font-weight:600;white-space:nowrap}`; document.head.appendChild(st)}
