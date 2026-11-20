/* ===== Rudolf OS 1.6.0: "Keep my websites encrypted with my account" (docs/plan-1.6.0.md Part 22; the owner, 2026-11-20: "For
   standard, it will be off by default. For Lite and Native it will be on by default.") =====
   Settings › Oracle. On: Oracle's websites (sign-ins, site data, cache, saved site passwords) live in the account's own encrypted
   folder (/persist/oracle/ID, the home folder's key), locked at sign-out. Each account's own choice, kept by rudolfd inside its
   encrypted home folder (desktop/owenc), so it cannot be changed with the drive alone; turning it off asks for the account's
   password and is noted in System health. Only for accounts whose files are on the disk. Applies the next time Oracle opens. */
function oweHasRD() { return typeof RD != 'undefined' && !!RD }
function OWE_ENG() { return oweHasRD() ? (RD.engine == 'native' ? 'native' : RD.engine == 'webkit' ? 'lite' : 'std') : 'std' }
const OWE_DEF = {std: false, native: true, lite: true};
/* the desktops where "on" is built (the rest show the choice, kept for when they have it) */
const OWE_BUILT = {native: true, lite: true};
async function oweGet() { try { return await rd('desktop/owenc?acct=' + encodeURIComponent(USER.id)) } catch (e) { return {ok: false, error: e.message} } }
function oweOn(st) { const eng = OWE_ENG(); return st && st.on != null ? !!st.on : OWE_DEF[eng] }
function oweRow(c) {
  if (typeof USER == 'undefined' || !USER || USER.r == 'guest' || !USER.id) return;
  const box = document.createElement('div'); box.className = 'owebox';
  box.innerHTML = "<h4 class='sh4'>Website data</h4><div class='r2'><label>Encryption</label><div><label class='ck'><input type='checkbox' class='owec'> Keep my websites encrypted with my account</label><div class='msg owem'></div><div class='msg owen'></div></div></div>";
  c.appendChild(box);
  const ck = box.querySelector('.owec'), m = box.querySelector('.owem'), n = box.querySelector('.owen'), eng = OWE_ENG();
  const say = (on, st) => {
    if (!st || !st.ok || !st.open) { ck.disabled = true; ck.checked = false;
      m.textContent = USER.disk ? 'Your files are not open on this computer\'s disk right now, so this cannot be changed.' : 'Only for accounts whose files are kept on this computer\'s disk.'; return }
    ck.disabled = false; ck.checked = on;
    m.textContent = on ? 'On: Oracle\'s websites (your sign-ins, site data, cache and saved site passwords) are encrypted with your account\'s key and locked when you sign out.'
      : eng == 'std' ? 'Off: Oracle\'s websites are kept in this computer\'s shared browser profile, and they are not encrypted: someone with this computer\'s drive could read them.'
      : 'Off: Oracle\'s websites are kept in this computer\'s shared browser data, and they are not encrypted: someone with this computer\'s drive could read them.';
    n.textContent = (!OWE_BUILT[eng] ? 'On this desktop, Oracle keeps its websites the shared way until a later update; your choice is kept for then. ' : '')
      + 'A change applies the next time Oracle opens, and websites ask you to sign in again. It protects your websites while you are signed out; anyone using your account while it is signed in can still use them.';
  };
  oweGet().then(st => say(oweOn(st), st));
  ck.onchange = async () => {
    const want = ck.checked; ck.disabled = true;
    let body = {acct: USER.id, on: want};
    if (!want) { const p = await askText('Turn off encrypted websites: your password', true); if (p == null) { ck.checked = true; ck.disabled = false; return } body.p = p }
    try { await rd('desktop/owenc', {method: 'POST', body: JSON.stringify(body)});
      try { logEvt('sys', `${USER.n} ${want ? 'turned on' : 'turned off'} keeping websites encrypted`) } catch (e) {}
      notify('Oracle', want ? 'Your websites will be kept encrypted with your account from the next time Oracle opens.' : 'Your websites will be kept the shared way from the next time Oracle opens.', {app: 'Settings'});
    } catch (e) { notify('Oracle', e.message || 'That did not work.', {kind: 'error'}) }
    const st = await oweGet(); say(oweOn(st), st); oweLite(st);
  };
}
if (typeof signinPanel == 'function') { const _sp = signinPanel; signinPanel = function (c) { const r = _sp.apply(this, arguments); try { if (oweHasRD()) oweRow(c) } catch (e) {} return r } }
/* Lite: rudolf-webkit keeps each account's websites in a WebKit context of its own; it is told where this account's go (its encrypted
   folder, or "" for the shared way) after signing in and whenever the choice changes (from its next tab), and to let the context go
   at sign-out, before the home folder (and with it the Oracle folder) is locked */
async function oweLite(st) {
  try { if (OWE_ENG() != 'lite' || typeof rtOn != 'function' || !rtOn() || !USER || USER.r == 'guest' || !USER.id) return;
    st = st || await oweGet(); const on = oweOn(st) && st && st.ok && st.open && st.web;
    shellSend({rudolfShell: 'tab', op: 'webdir', part: rtPart(), dir: on ? st.web : '', id: 0}) } catch (e) {}
}
(function () { const o = window.__afterLogin; window.__afterLogin = () => { o && o(); oweLite() } })();
if (typeof homeUnload == 'function') { const _hu = homeUnload; homeUnload = function () {
  try { if (OWE_ENG() == 'lite' && typeof rtOn == 'function' && rtOn() && USER && USER.r != 'guest' && USER.id) shellSend({rudolfShell: 'tab', op: 'drop', part: rtPart(), id: 0}) } catch (e) {}
  return _hu.apply(this, arguments) } }
