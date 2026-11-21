/* ===== Rudolf OS 1.6.1 (docs/plan-1.6.1.md Part 10, docs/plan-oracle.md item 2, its second half): Oracle's session =====
   ☰ › Reopen my tabs next time (1.6.0 Part 21) becomes "continue where you left off": the first Oracle window after signing in
   brings back every Oracle window you had, its tabs asleep (💤, loaded when clicked) but the one that was in front, so starting is
   quick. After Oracle or the desktop ended unexpectedly (the memory guard, a crash, the power), the next Oracle window offers
   "Restore your tabs?" once, whatever the setting.
   - Where it is kept: in the account's own encrypted home folder (rudolfd home/*, .rudolf/oracle-session/WINDOW.json, locked with
     the folder at sign-out), one small file per Oracle window, so Native's windows (a page each) never write over each other.
     Nothing goes into the page's shared storage: with files kept this way, 1.6.0's copy in the settings (S.otabs) is no longer
     written, and an old one is removed. Accounts whose files are not on the disk keep 1.6.0's way (S.otabs), unchanged.
   - Never private or Tor windows, never Guest.
   - Each window's file says which tabs it has and which is in front; written a second after a change. A window closed by its
     person is marked closed; signing out marks the open ones as ended by sign-out. At the next sign-in: windows not marked at all
     ended unexpectedly; if there are none, the last window closed is the session.
   browser() calls oraSession({...}) once per window. ===== */
const ORA_SS = {pend: null, crash: false, ending: false, asked: false};
const ORA_SS_DIR = '.rudolf/oracle-session';
const oraSsOn = () => { try { return typeof hdOn == 'function' && hdOn() && typeof USER != 'undefined' && USER && USER.r != 'guest' } catch (e) { return false } };
/* (the 1.6.0 copy in the settings is not written while the files are kept this way: base_v24.html oraKeep asks) */
function oraSsOwnsTabs() { return oraSsOn() }
const oraSsPath = id => ORA_SS_DIR + '/' + id + '.json';
async function oraSsWrite(id, o) { try { await rd('home/write?' + hdQ() + '&path=' + encodeURIComponent(oraSsPath(id)) + '&mkdirs=1', {method: 'POST', body: JSON.stringify(o)}) } catch (e) {} }
async function oraSsDel(id) { try { await rd('home/delete?' + hdQ() + '&path=' + encodeURIComponent(oraSsPath(id)), {method: 'POST'}) } catch (e) {} }
/* at sign-in: what the last session left (read once; the files stay until a window takes them) */
async function oraSsLoad() {
  ORA_SS.pend = null; ORA_SS.crash = false; ORA_SS.asked = false; ORA_SS.ending = false; if (!oraSsOn()) return;
  if (Array.isArray(S.otabs)) { delete S.otabs; try { persistS() } catch (e) {} }   /* (1.6.0's copy outside the folder: gone) */
  let L = []; try { L = ((await rd('home/list?' + hdQ() + '&path=' + encodeURIComponent(ORA_SS_DIR))).items || []).filter(x => !x.d && /\.json$/.test(x.p || '')) } catch (e) { return }
  const all = [];
  for (const x of L.slice(0, 40)) { const id = String(x.p).split('/').pop().replace(/\.json$/, ''); if (!/^[a-z0-9-]{4,40}$/.test(id)) continue;
    try { const o = JSON.parse(new TextDecoder().decode(await hdRead(oraSsPath(id)))); if (o && Array.isArray(o.tabs) && o.tabs.length) all.push(Object.assign(o, {id})); else oraSsDel(id) } catch (e) {} }
  if (!all.length) return;
  const open = all.filter(o => !o.closed && !o.ended), signed = all.filter(o => !o.closed && o.ended);
  const take = open.length ? open : signed.length ? signed : [all.filter(o => o.closed).sort((a, b) => (b.at || 0) - (a.at || 0))[0]];
  ORA_SS.crash = open.length > 0; ORA_SS.pend = {wins: take.sort((a, b) => (a.at || 0) - (b.at || 0)), ids: all.map(o => o.id)};
}
(function () { const o = window.__afterLogin; window.__afterLogin = () => { o && o(); oraSsLoad().catch(() => {}) } })();
/* signing out: the open windows' files say so first (written before the folder is locked) */
function oraSsEnd() { ORA_SS.ending = true; try { (window.__oraSsAll || []).forEach(f => { try { f(true) } catch (e) {} }) } catch (e) {} }
if (typeof userLogout == 'function') { const _lo = userLogout; userLogout = function () { oraSsEnd(); return _lo.apply(this, arguments) } }
{ const _ol = window.__oaLogout; window.__oaLogout = () => { oraSsEnd(); return _ol ? _ol() : 1 } }
window.__oraSsAll = [];
function oraSession(c) {
  const W = c.W; if (c.priv || c.tor) return null;
  const id = 'w' + Date.now().toString(36) + '-' + Math.random().toString(36).slice(2, 8);
  let last = '', tm = 0;
  const snap = () => { const L = (c.tabs() || []).map(t => ({u: String(t.ru || (t.h && t.h[t.i]) || '').slice(0, 2000), title: String(t.title || '').slice(0, 200)})).filter(x => /^(https?:|about:|file:\/\/\/)/i.test(x.u) && !/^about:(blank|home)/.test(x.u)).slice(0, 50);
    return {v: 1, at: Date.now(), tabs: L, ci: Math.max(0, L.findIndex(x => x.u === (c.cur() || '')))} };
  /* written a second after a change; at once when signing out (ended) */
  const save = now => { if (!oraSsOn() || (ORA_SS.ending && !now)) return; const o = snap(); if (now === true) o.ended = 1;
    const k = JSON.stringify([o.tabs, o.ci, o.ended]); if (k === last) return; last = k; clearTimeout(tm);
    const go = () => { if (o.tabs.length) oraSsWrite(id, o); else oraSsDel(id) };
    if (now) go(); else tm = setTimeout(go, 1000) };
  window.__oraSsAll.push(save);
  const changed = () => save(false);
  /* the window closed by its person: marked closed (the last one closed is the session next time) */
  const closed = () => { window.__oraSsAll = window.__oraSsAll.filter(f => f !== save); clearTimeout(tm); if (!oraSsOn() || ORA_SS.ending) return;
    const o = snap(); if (!o.tabs.length) { oraSsDel(id); return } o.closed = 1; oraSsWrite(id, o) };
  /* the first Oracle window: the last session's windows back (this one the first of them), the old files removed */
  const bring = p => { const ws = p.wins; ORA_SS.pend = null; p.ids.forEach(oraSsDel);
    const first = ws[0]; first.tabs.slice(0, 12).forEach((x, k) => c.add(x.u, x.title, k !== (first.ci | 0)));
    c.pick(Math.min(first.ci | 0, Math.min(first.tabs.length, 12) - 1));
    ws.slice(1, 6).forEach(w => { const urls = w.tabs.map(x => x.u).slice(0, 12); if (!urls.length) return;
      if (typeof ONEAPP == 'object' && ONEAPP && typeof oaPost == 'function' && typeof oaOut == 'function' && oaOut() && !(ONEAPP.q && ONEAPP.q.get('enc') == '1')) oaPost({op: 'untor', url: urls.join('\n')});
      else if (typeof ONEAPP == 'object' && ONEAPP && ONEAPP.q && ONEAPP.q.get('enc') == '1' && typeof rdPost == 'function') rdPost('orawin/open?' + urls.map(u => 'url=' + encodeURIComponent(u)).join('&')).catch(() => {});
      else { ORA_SS.next = urls.slice(1); try { browser(urls[0]) } finally { ORA_SS.next = null } } });
    save(false) };
  /* called once the window is made, before its first tab: true when it has taken the session's tabs */
  const restore = () => { const p = ORA_SS.pend; if (!p || !oraSsOn()) return false;
    if (typeof S == 'object' && S.oreopen) { bring(p); return true }
    if (ORA_SS.crash && !ORA_SS.asked) { ORA_SS.asked = true; const n = p.wins.reduce((a, w) => a + w.tabs.length, 0);
      setTimeout(() => notify('Restore your tabs?', 'Oracle did not close properly last time. Bring back its ' + n + ' tab' + (n == 1 ? '' : 's') + '?', {app: 'Oracle', toast: 15000,
        action: () => { if (!W.el.isConnected || ORA_SS.pend !== p) return; W.focus(); const k0 = (c.tabs() || []).length; bring(p); c.dropHome(k0) }}), 800) }
    return false };
  /* (a window opened by bring: the other tabs of that window) */
  const opened = () => { const n = ORA_SS.next; ORA_SS.next = null; if (Array.isArray(n) && n.length) setTimeout(() => n.forEach(u => { try { W.oNewTab(u) } catch (e) {} }), 0) };
  return {changed, closed, restore, opened, id};
}
