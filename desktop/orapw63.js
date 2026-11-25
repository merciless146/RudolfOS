/* ===== Rudolf OS 1.6.3: Oracle's own password manager (docs/plan-1.6.3.md Part 7, step 1; the owner's "Option C", 2026-11-24) =====
   Chromium's own password manager never reaches Oracle's tabs (Controlled Frames: #684), so Oracle has its own, the same on the
   Chromium desktop, Native and Lite:
   - The shell's script in the tab's top frame, in an isolated world (iso/shell/shell.js PW_JS; Lite: rudolf-webkit's "rudolf"
     world), on https: pages only, tells Oracle when a password field has the keyboard (pwfocus) and, when a sign-in is sent, the
     user name and password (pwsave; between the page and the shell under a one-time pad, so Chromium's log never holds them).
     The shell checks the origin is the tab's own top-level https: origin before telling.
   - "Save your password for example.com?" (Save, Never for this site, Not now) in Oracle's own bar, outside the page (the page
     cannot cover, read or click it). Kept for that exact origin, in the account's keyring (rudolfd oracle/pw/*: rudolf-portal
     --webpw as the account's Linux user), locked with the account.
   - Filled only after a click in that bar ("🔑 Fill"), only into the tab's top frame and only while it is still at that origin
     (the shell checks, then the page); never by itself, never into a frame of another site, never on http:.
   - "Use a strong password" for a new password: 20 characters from the system's random source, shown before it is used.
   - Guest, private and Tor windows never save and are never offered a fill: they keep nothing.
   - ☰ › Passwords: the sites and user names; Show and Copy after the account's password; change; remove; the sites marked Never.
   ===== */
const OPW = {bars: new WeakMap(), quiet: new Map()};
if (!Array.isArray(S.opwNever)) S.opwNever = [];
const opwHost = o => { try { return new URL(o).host } catch (e) { return String(o || '') } };
const opwSafeOrigin = o => typeof o == 'string' && /^https:\/\/[a-z0-9.-]{1,253}(:\d{1,5})?$/.test(o) && !/^https:\/\/[.-]/.test(o);
/* the window may keep and fill passwords: Rudolf OS itself, a signed-in account (not Guest), not a private or Tor window */
const opwAllowed = W => typeof RD != 'undefined' && !!RD && typeof USER != 'undefined' && !!USER && USER.r != 'guest' && !!W && !W.priv && !W.tor;
/* a look-alike: a host written with letters of other alphabets (punycode, xn--) */
const opwPuny = o => /(^|\.)xn--/.test(opwHost(o));
/* a strong password: 20 characters, letters of both cases, digits and signs, each kind at least once (rejection sampling) */
function opwStrong() {
  const A = 'ABCDEFGHJKLMNPQRSTUVWXYZ', a = 'abcdefghijkmnopqrstuvwxyz', d = '23456789', s = '-_.!?@#%+=', all = A + a + d + s;
  const pick = set => { const lim = 256 - 256 % set.length, b = new Uint8Array(1); for (;;) { crypto.getRandomValues(b); if (b[0] < lim) return set[b[0] % set.length] } };
  for (;;) { let p = ''; for (let i = 0; i < 20; i++) p += pick(all);
    if (/[A-Z]/.test(p) && /[a-z]/.test(p) && /[2-9]/.test(p) && /[-_.!?@#%+=]/.test(p)) return p }
}
const opwList = async () => { try { const j = await rd('oracle/pw/list'); return Array.isArray(j.items) ? j.items.filter(x => x && opwSafeOrigin(x.origin) && typeof x.user == 'string') : [] } catch (e) { return [] } };
const opwBody = (...L) => L.join('\n');
/* Oracle's bar for a tab, under the bookmarks bar (or the find bar): one at a time per window; it goes when its tab is not shown */
function opwBar(w, tid) {
  const W = w.W; if (!W || !W.body) return null;
  let b = OPW.bars.get(W);
  if (!b || !b.isConnected) {
    b = document.createElement('div'); b.className = 'opwbar'; b.setAttribute('role', 'region'); b.setAttribute('aria-label', 'Passwords');
    const at = W.body.querySelector('.ofind') || W.body.querySelector('.obm'); if (at) at.after(b); else W.body.prepend(b);
    OPW.bars.set(W, b);
  }
  clearInterval(b._iv); b.hidden = false; b.dataset.tid = tid; b.innerHTML = '';
  b._iv = setInterval(() => { const t = RT.get(tid); let on = false; try { on = !!(t && t.shown()) } catch (e) {} if (!t || !b.isConnected) { opwHide(b); return } b.hidden = !on }, 500);
  return b;
}
function opwHide(b) { if (!b) return; clearInterval(b._iv); b.hidden = true; b.innerHTML = ''; delete b.dataset.tid }
const opwBtn = (t, f, pri) => { const e = document.createElement('button'); e.textContent = t; if (pri) e.className = 'pri'; e.onclick = f; return e };
const opwText = (t, cls) => { const e = document.createElement('span'); e.textContent = t; if (cls) e.className = cls; return e };
/* a sign-in sent: "Save your password for HOST?" (or Update), unless it is kept already as it is, or the site is marked Never */
async function opwSave(tid, w, o) {
  if (!opwAllowed(w.W) || !opwSafeOrigin(o.origin) || typeof o.pw != 'string' || !o.pw) return;
  if (S.opwNever.includes(o.origin)) return;
  const user = String(o.user || '').slice(0, 200), pw = o.pw.slice(0, 500), L = await opwList();
  const had = L.some(x => x.origin == o.origin && x.user == user);
  if (had) { try { const g = await rdPost('oracle/pw/get', opwBody(o.origin, user)); if (g && g.pw === pw) return } catch (e) {} }
  const b = opwBar(w, tid); if (!b) return;
  const host = opwHost(o.origin);
  b.append(opwText('🔑'), opwText((had ? 'Update your saved password for ' : 'Save your password for ') + host + (user ? ' (' + user + ')' : '') + '?', 'opwq'));
  if (opwPuny(o.origin)) b.append(opwText('This address is written with letters of other alphabets (' + host + '): check it is the site you meant.', 'opwwarn'));
  b.append(opwBtn(had ? 'Update' : 'Save', async () => {
    try { await rdPost('oracle/pw/set', opwBody(o.origin, user, pw)); b.innerHTML = ''; b.append(opwText('🔑 Saved in your keyring, for ' + host + ' only.', 'opwq')); setTimeout(() => { if (b.dataset.tid == tid) opwHide(b) }, 3000);
      try { logEvt('sys', `${USER.n} saved a password for ${host} in Oracle`) } catch (e) {} }
    catch (e) { b.innerHTML = ''; b.append(opwText('🔑 Not saved: ' + e.message, 'opwerr'), opwBtn('Close', () => opwHide(b))) } }, true),
    opwBtn('Never for this site', () => { if (!S.opwNever.includes(o.origin)) { S.opwNever.push(o.origin); S.opwNever = S.opwNever.slice(-500); persistS() } opwHide(b) }),
    opwBtn('Not now', () => opwHide(b)));
}
/* a password field has the keyboard: the saved accounts for this exact origin, and a strong password for a new one; nothing is
   filled until Fill is clicked here */
async function opwFocus(tid, w, o) {
  if (!opwAllowed(w.W) || !opwSafeOrigin(o.origin)) return;
  const k = tid + ' ' + o.origin; if (Date.now() - (OPW.quiet.get(k) || 0) < 60000) return;
  const L = (await opwList()).filter(x => x.origin == o.origin);
  if (!L.length && !o.newpw) return;
  const b = opwBar(w, tid); if (!b) return;
  const host = opwHost(o.origin);
  b.append(opwText('🔑'));
  if (L.length) {
    b.append(opwText('Sign in to ' + host + ' as', 'opwq'));
    const sel = document.createElement('select'); sel.className = 'opwsel'; sel.setAttribute('aria-label', 'Saved account');
    L.forEach((x, i) => { const op = document.createElement('option'); op.value = i; op.textContent = x.user || '(no user name)'; sel.appendChild(op) });
    b.append(sel, opwBtn('Fill', async () => {
      const x = L[+sel.value]; if (!x) return;
      let g; try { g = await rdPost('oracle/pw/get', opwBody(x.origin, x.user)) } catch (e) { b.innerHTML = ''; b.append(opwText('🔑 ' + e.message, 'opwerr'), opwBtn('Close', () => opwHide(b))); return }
      opwFill(tid, x.origin, x.user, g.pw, false, b); g = null }, true));
  }
  if (o.newpw) b.append(opwBtn('Use a strong password', () => {
    const p = opwStrong(); b.innerHTML = '';
    b.append(opwText('🔑 A strong password for ' + host + ':', 'opwq'), opwText(p, 'opwgen'),
      opwBtn('Use it', () => opwFill(tid, o.origin, '', p, true, b), true), opwBtn('Another', () => { opwHide(b); opwFocus(tid, w, Object.assign({}, o, {again: 1})) }), opwBtn('Not now', () => opwHide(b)));
  }));
  b.append(opwBtn('Not now', () => { OPW.quiet.set(k, Date.now()); opwHide(b) }));
}
/* the fill: told to the shell, which checks the tab is still at that origin (and the page checks again) */
const OPW_FILL = new Map(); let OPW_N = 0;
function opwFill(tid, origin, user, pw, gen, b) {
  const id = ++OPW_N; OPW_FILL.set(id, b);
  rtSend('pwfill', tid, {id, origin, user, pw, gen: !!gen});
  setTimeout(() => OPW_FILL.delete(id), 10000);
}
function opwFilled(o) {
  const b = OPW_FILL.get(+o.id); OPW_FILL.delete(+o.id); if (!b) return;
  if (o.r == 'ok') { opwHide(b); return }
  b.innerHTML = ''; b.append(opwText(o.r == 'origin' ? '🔑 The page is no longer that site: nothing was filled.' : '🔑 No password field to fill on this page.', 'opwerr'), opwBtn('Close', () => opwHide(b)));
}
/* the shell's events for a tab (tabs26.js) */
function opwEvent(tid, w, ev, m) {
  if (ev == 'pwsave') opwSave(tid, w, {origin: String(m.origin || ''), user: typeof m.user == 'string' ? m.user : '', pw: typeof m.pw == 'string' ? m.pw : ''});
  else if (ev == 'pwfocus') opwFocus(tid, w, {origin: String(m.origin || ''), newpw: !!m.newpw});
  else if (ev == 'pwfilled') opwFilled({id: +m.id || 0, r: String(m.r || '')});
}
/* the account's own password, asked again before a saved password is shown or copied */
function opwAsk(reason) {
  return new Promise(res => {
    const o = document.createElement('div'); o.className = 'rmodal';
    o.innerHTML = `<div class='rmbox' role='alertdialog' aria-label='Your password'><h3>🔑 Your password</h3><p class='rmr'></p><div class='r2'><label>Password</label><input class='ep' autocomplete='off'></div><div class='rmmsg'></div><div class='rmb'><button class='en'>Cancel</button> <button class='ey pri'>OK</button></div></div>`;
    o.querySelector('.rmr').textContent = reason; o.querySelector('label').textContent = 'Password of ' + USER.n;
    const P = typeof maskInput == 'function' ? maskInput(o.querySelector('.ep')) : {get: () => o.querySelector('.ep').value};
    document.body.appendChild(o); setTimeout(() => o.querySelector('.ep').focus(), 30);
    const done = v => { o.remove(); res(v) };
    const ok = async () => { if (await pwCheck(USER, P.get())) done(true); else { o.querySelector('.rmmsg').textContent = 'That is not the password.'; } };
    o.querySelector('.en').onclick = () => done(false); o.querySelector('.ey').onclick = ok;
    o.querySelector('.ep').addEventListener('keydown', e => { if (e.key == 'Enter') ok(); if (e.key == 'Escape') done(false) });
  });
}
/* ☰ › Passwords */
async function opwWindow() {
  if (window.__opwW && wins.includes(__opwW)) { __opwW.focus(); __opwW.draw(); return __opwW }
  const W = window.__opwW = win('Passwords - Oracle', '🔑', 620, 440); W.body.classList.add('opwwin');
  W.body.innerHTML = `<div class='opwtop'><b>Oracle's saved passwords</b><span class='msg'>Kept in your account's keyring, locked when you sign out. Each one is offered only on its own site, after you click Fill.</span></div><div class='opwl'></div><h4 class='sh4'>Never saved for</h4><div class='opwn'></div>`;
  const L = W.body.querySelector('.opwl'), Nv = W.body.querySelector('.opwn');
  const draw = W.draw = async () => {
    if (!opwAllowed({})) { L.textContent = typeof USER != 'undefined' && USER && USER.r == 'guest' ? 'Guest keeps no passwords.' : 'Saved passwords need Rudolf OS itself and a signed-in account.'; Nv.textContent = ''; return }
    L.textContent = 'Loading…'; const items = await opwList(); L.innerHTML = '';
    if (!items.length) L.textContent = 'No passwords saved yet. When you sign in to a site in Oracle, it asks whether to save the password.';
    items.sort((a, b) => a.origin.localeCompare(b.origin) || a.user.localeCompare(b.user)).forEach(x => {
      const r = document.createElement('div'); r.className = 'opwrow';
      const pwv = opwText('••••••••', 'opwpw');
      r.append(opwText(opwHost(x.origin), 'opwsite'), opwText(x.user || '(no user name)', 'opwuser'), pwv);
      const reveal = async copy => { if (!(await opwAsk((copy ? 'Copy' : 'Show') + ' the password for ' + opwHost(x.origin) + '.'))) return;
        try { const g = await rdPost('oracle/pw/get', opwBody(x.origin, x.user)); if (copy) { await navigator.clipboard.writeText(g.pw); notify('Password copied', 'For ' + opwHost(x.origin) + '.', {toast: 2500}) } else { pwv.textContent = g.pw; setTimeout(() => { pwv.textContent = '••••••••' }, 30000) } }
        catch (e) { notify('Oracle', e.message, {kind: 'error'}) } };
      r.append(opwBtn('Show', () => reveal(false)), opwBtn('Copy', () => reveal(true)),
        opwBtn('Change', async () => { if (!(await opwAsk('Change the password for ' + opwHost(x.origin) + '.'))) return;
          const v = prompt('The new password for ' + opwHost(x.origin) + (x.user ? ' (' + x.user + ')' : '') + ':'); if (!v) return;
          try { await rdPost('oracle/pw/set', opwBody(x.origin, x.user, v.slice(0, 500))); draw() } catch (e) { notify('Oracle', e.message, {kind: 'error'}) } }),
        opwBtn('Remove', async () => { if (!confirm('Remove the saved password for ' + opwHost(x.origin) + (x.user ? ' (' + x.user + ')' : '') + '?')) return;
          try { await rdPost('oracle/pw/del', opwBody(x.origin, x.user)); draw() } catch (e) { notify('Oracle', e.message, {kind: 'error'}) } }));
      L.appendChild(r);
    });
    Nv.innerHTML = ''; if (!S.opwNever.length) Nv.textContent = 'No sites.';
    S.opwNever.forEach(o => { const r = document.createElement('div'); r.className = 'opwrow'; r.append(opwText(opwHost(o), 'opwsite'), opwBtn('Remove', () => { S.opwNever = S.opwNever.filter(x => x != o); persistS(); draw() })); Nv.appendChild(r) });
  };
  draw(); return W;
}
{ const o = window.__oracleMenu; window.__oracleMenu = x => { const r = o ? o(x) : []; return r.concat([['Passwords', () => opwWindow()]]) } }
(function () { const s = document.createElement('style');
  s.textContent = '.opwbar{display:flex;flex-wrap:wrap;gap:6px;align-items:center;padding:4px 8px;background:#eff6ff;border-bottom:1px solid var(--bd);font-size:12px}.opwbar[hidden]{display:none}.opwbar button{padding:1px 9px;font-size:12px}'
    + '.opwbar .opwwarn,.opwbar .opwerr{color:#b91c1c}.opwbar .opwgen,.opwwin .opwpw{font-family:monospace;user-select:all}.odark .opwbar{background:#0f172a;border-color:#374151;color:#e5e7eb}'
    + '.opwwin{padding:10px;overflow:auto;display:block}.opwwin .opwtop{display:flex;flex-direction:column;gap:2px;margin-bottom:8px}.opwwin .opwrow{display:flex;gap:6px;align-items:center;padding:4px 0;border-bottom:1px solid var(--bd)}'
    + '.opwwin .opwsite{flex:1 1 30%;font-weight:600;overflow-wrap:anywhere}.opwwin .opwuser{flex:1 1 30%;overflow-wrap:anywhere}.opwwin .opwpw{flex:0 0 120px;overflow-wrap:anywhere}';
  document.head.appendChild(s) })();
