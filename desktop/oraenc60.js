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
const OWE_BUILT = {native: true, lite: true, std: true};
async function oweGet() { try { return await rd('desktop/owenc?acct=' + encodeURIComponent(USER.id)) } catch (e) { return {ok: false, error: e.message} } }
/* 1.6.1 (docs/plan-1.6.1.md Part 20): what keeps the shared browser profile at this start (rudolfd's webkey: tpm, none, failed,
   off), for "Off" on the desktops whose Oracle uses it (Standard; Native's Oracle in Rudolf's Chromium) */
async function oweKey() { try { return await rd('webkey') } catch (e) { return null } }
function oweOffText(eng, wk) {
  const where = 'Off: Oracle\'s websites are kept in this computer\'s shared browser ' + (eng == 'std' ? 'profile' : 'data');
  const st = wk && wk.state;
  if (eng == 'lite' || !st || st == 'off') return where + ', and they are not encrypted: someone with this computer\'s drive could read them.';
  if (st == 'tpm') return where + ', encrypted with a key kept in this computer\'s security chip (TPM) and tied to its Secure Boot: this computer\'s drive alone cannot read them, nor can another system started on it. Anyone who can sign in on this computer can still use them.'
    + (wk.why && !/^the first time/.test(wk.why) ? ' A new key was made at this start (' + wk.why.replace(/ \(.*$/, '') + '): what was kept under the previous one could not be read, so websites asked you to sign in again.' : '');
  if (st == 'none') return where + ', with the browser\'s built-in key only: this computer has no TPM security chip (or it is turned off in its firmware\'s settings), so someone with this computer\'s drive could read them.';
  return where + ', with the browser\'s built-in key at this start: the TPM security chip did not give its key' + (wk.why ? ' (' + wk.why + ')' : '') + ', so someone with this computer\'s drive could read what is kept now.';
}
function oweOn(st) { const eng = OWE_ENG(); return st && st.on != null ? !!st.on : OWE_DEF[eng] }
let OWE_WK = null;
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
    m.textContent = on ? 'On: Oracle\'s websites (your sign-ins, site data, cache and saved site passwords) are encrypted with your account\'s key and locked when you sign out.' + (eng == 'std' ? ' On this desktop Oracle then opens in a window of its own, over the desktop, and uses a little more memory while it is open.' : '')
      : oweOffText(eng, OWE_WK);
    n.textContent = (!OWE_BUILT[eng] ? 'On this desktop, Oracle keeps its websites the shared way until a later update; your choice is kept for then. ' : '')
      + 'A change applies the next time Oracle opens, and websites ask you to sign in again. It protects your websites while you are signed out; anyone using your account while it is signed in can still use them.';
  };
  Promise.all([oweGet(), oweKey()]).then(([st, wk]) => { OWE_WK = wk; say(oweOn(st), st) });
  ck.onchange = async () => {
    const want = ck.checked; ck.disabled = true;
    let body = {acct: USER.id, on: want};
    if (!want) { const p = await askText('Turn off encrypted websites: your password', true); if (p == null) { ck.checked = true; ck.disabled = false; return } body.p = p }
    try { await rd('desktop/owenc', {method: 'POST', body: JSON.stringify(body)});
      try { logEvt('sys', `${USER.n} ${want ? 'turned on' : 'turned off'} keeping websites encrypted`) } catch (e) {}
      notify('Oracle', want ? 'Your websites will be kept encrypted with your account from the next time Oracle opens.' : 'Your websites will be kept the shared way from the next time Oracle opens.', {app: 'Settings'});
    } catch (e) { notify('Oracle', e.message || 'That did not work.', {kind: 'error'}) }
    const st = await oweGet(); say(oweOn(st), st); oweLite(st); OWE_ST = st;
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
/* Standard, turned on: Oracle in a Chromium of its own (rudolfd's orawin), its profile in the account's encrypted folder, as
   Standard's Tor windows are. Every Oracle window the desktop would open goes there; at sign-out it is closed (its tabs saved)
   before the folders are locked. Only when the account turned it on (Standard's default is off). */
let OWE_ST = null;
const oweStdOn = () => { try { return OWE_ENG() == 'std' && !(typeof ONEAPP == 'object' && ONEAPP) && USER && USER.r != 'guest' && USER.id && OWE_ST && OWE_ST.ok && OWE_ST.open && OWE_ST.on === true } catch (e) { return false } };
async function oweStdOpen(urls, priv) {
  const hx = u => [...new Uint8Array(u)].map(x => x.toString(16).padStart(2, '0')).join('');
  if (typeof FKRAW == 'undefined' || !FKRAW) throw new Error('your account\'s key is not open');
  await rdPost('orawin/session', JSON.stringify({id: USER.id, n: USER.n, r: USER.r == 'admin' ? 'admin' : 'user', fk: hx(FKRAW)}));
  const q = (urls || []).filter(u => typeof u == 'string' && /^(https?:\/\/|about:|file:\/\/\/)/i.test(u) && u.length < 2000).slice(0, 12).map(u => 'url=' + encodeURIComponent(u));
  if (priv) q.push('priv=1');
  await rdPost('orawin/open' + (q.length ? '?' + q.join('&') : ''));
}
if (typeof browser == 'function') { const _br = browser; browser = function (u, bo) {
  if (oweStdOn() && !(bo && bo.adopt)) { oweStdOpen(u ? [u] : [], !!(bo && bo.priv)).catch(e => notify('Oracle', 'Oracle could not open: ' + (e.message || e), {kind: 'error'})); return null }
  return _br.apply(this, arguments) } }
(function () { const o = window.__afterLogin; window.__afterLogin = () => { o && o(); if (OWE_ENG() == 'std' && !(typeof ONEAPP == 'object' && ONEAPP)) oweGet().then(st => { OWE_ST = st }).catch(() => {}) } })();
if (typeof homeUnload == 'function') { const _hu2 = homeUnload; homeUnload = function () {
  try { if (OWE_ENG() == 'std' && !(typeof ONEAPP == 'object' && ONEAPP) && typeof HD == 'object' && HD && OWE_ST && OWE_ST.on === true) { HD.chain = HD.chain.then(() => rdPost('orawin/close').catch(() => {})) } } catch (e) {}
  OWE_ST = null; return _hu2.apply(this, arguments) } }
