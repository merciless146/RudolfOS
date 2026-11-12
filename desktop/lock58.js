/* ===== 1.5.8 (lock58): how the lock screen and the sign-in page look, and the shutting-down screen.
   - Display Properties › Lock and sign-in screen (for the whole computer, as the screen saver: the sign-in page shows
     before anyone has signed in): the background (the desktop's, a picture of your own, or a colour), its blur and
     darkening, the clock (large, small or none), where the box sits, and whether the account's picture and name show.
     The desktop's background is remembered when the screen locks or someone signs out, for the sign-in page.
   - The shutting-down screen (shutdownDlg in core25.js): a full screen "Shutting down…" or "Restarting…" with
     Rudolf OS's portrait, shown at once; the shutdown tone plays to the end while files are saved, then the system's
     own safe shutdown (/usr/lib/rudolf/shutdown) closes the apps and turns the power off.
   ===== */
const LK_DEF = {bg: 'desktop', color: '#1e3a8a', blur: 0, dim: 30, clock: 'large', pos: 'center', user: true};
const LK_PICK = 'bd95lkpic', LK_LAST = 'bd95lklast';
const lkConf = () => Object.assign({}, LK_DEF, SYSC.lk || {});
/* the desktop's background now (an image or a colour), to remember for the sign-in page */
function lkDesktopBg() {
  const cs = getComputedStyle(document.body), img = cs.backgroundImage, col = cs.backgroundColor;
  return img && img != 'none' ? {img} : col && col != 'rgba(0, 0, 0, 0)' ? {col} : null;
}
function lkRemember() { if (!USER) return; const b = lkDesktopBg(); if (b && (!b.img || b.img.length < 2e6)) svj(LK_LAST, b) }
/* the background layers, behind the screen's own content */
function lkApply(el, kind) {
  if (!el) return; const c = lkConf();
  el.querySelectorAll(':scope > .lkbgl').forEach(x => x.remove()); el.style.isolation = 'isolate';
  let bg = null;
  if (c.bg == 'picture') { const p = ld(LK_PICK, null); if (p) bg = {img: 'url("' + p + '")'} }
  else if (c.bg == 'color') bg = {col: c.color};
  else bg = (kind == 'lock' && USER && lkDesktopBg()) || ld(LK_LAST, null);
  if (bg) {
    const b = document.createElement('div'); b.className = 'lkbgl';
    b.style.cssText = `position:absolute;inset:${c.blur ? -3 * c.blur : 0}px;z-index:-2;pointer-events:none;background:${bg.col || '#0b1020'} ${bg.img ? bg.img + ' center/cover no-repeat' : ''};${c.blur ? `filter:blur(${c.blur}px);` : ''}`;
    el.prepend(b);
  }
  const d = document.createElement('div'); d.className = 'lkbgl'; d.style.cssText = `position:absolute;inset:0;z-index:-1;pointer-events:none;background:rgba(0,0,0,${(c.dim || 0) / 100})`;
  el.prepend(d);
  /* where the box sits */
  const P = {center: ['center', 'center', ''], left: ['center', 'flex-start', '0 0 0 8vw'], bottom: ['flex-end', 'flex-start', '0 0 8vh 8vw']}[c.pos] || ['center', 'center', ''];
  if (kind == 'lock') { el.style.justifyContent = P[0]; el.style.alignItems = P[1]; el.style.padding = P[2] }
  else { el.style.alignItems = P[0]; el.style.justifyContent = P[1]; el.style.padding = P[2] }
  /* the clock */
  const ck = el.querySelector('.lkclock'), dt = el.querySelector('.lkdate');
  if (ck) { ck.style.display = c.clock == 'off' ? 'none' : ''; ck.style.fontSize = c.clock == 'small' ? '36px' : '' }
  if (dt) { dt.style.display = c.clock == 'off' ? 'none' : ''; dt.style.fontSize = c.clock == 'small' ? '14px' : '' }
  /* the account's picture and name (the lock screen) */
  el.querySelectorAll('.lkav,.lkn').forEach(x => x.style.display = c.user ? '' : 'none');
}
/* the sign-in page gets a clock too (not before: it had none) */
function lkLoginClock() {
  const L = $('#login'); if (!L) return; let ck = L.querySelector(':scope > .lkck');
  if (!ck) { ck = document.createElement('div'); ck.className = 'lkck'; ck.style.cssText = 'position:absolute;top:6vh;left:0;right:0;text-align:center;pointer-events:none;text-shadow:0 2px 8px #0008'; ck.innerHTML = "<div class='lkclock'></div><div class='lkdate'></div>"; L.appendChild(ck) }
  const tick = () => { if (L.style.display == 'none') return; const c = clockParts(); ck.querySelector('.lkclock').textContent = c.time; ck.querySelector('.lkdate').textContent = c.longNoYear };
  tick(); clearInterval(window.__lkLoginIv); window.__lkLoginIv = setInterval(tick, 1000);
}
{ const _lock = lockScreen; lockScreen = function () { lkRemember(); _lock.apply(this, arguments); lkApply(document.getElementById('lockov'), 'lock') } }
{ const _show = showLogin; showLogin = function () { lkRemember(); const r = _show.apply(this, arguments); lkLoginClock(); lkApply($('#login'), 'login'); return r } }
setTimeout(() => { try { lkLoginClock(); lkApply($('#login'), 'login') } catch (e) {} }, 0);

/* Display Properties › Lock and sign-in screen */
(function () { const o = dispPanelS; dispPanelS = function (c, W) { o(c, W); const adm = isAdmin(), D = adm ? '' : ' disabled', k = lkConf();
  const d = document.createElement('div'); d.className = 'lk58';
  const opt = (v, t, cur) => `<option value='${v}'${v == cur ? ' selected' : ''}>${t}</option>`;
  d.innerHTML = `<h4 class='sh4'>Lock and sign-in screen</h4><div class='r2'>
  <label for='lkbg'>Background</label><div><select id='lkbg'${D}>${opt('desktop', 'The desktop’s background', k.bg)}${opt('picture', 'A picture of your own', k.bg)}${opt('color', 'A colour', k.bg)}</select>
   <span class='lkpicrow'> <button class='lkpic'${D}>Choose a picture…</button></span><span class='lkcolrow'> <input type='color' id='lkcol' value='${k.color}'${D}></span></div>
  <label for='lkblur'>Blur</label><div><input type='range' id='lkblur' min='0' max='20' value='${k.blur}'${D}> <span class='lkbv'></span></div>
  <label for='lkdim'>Darken</label><div><input type='range' id='lkdim' min='0' max='80' value='${k.dim}'${D}> <span class='lkdv'></span></div>
  <label for='lkck'>Clock</label><select id='lkck'${D}>${opt('large', 'Large', k.clock)}${opt('small', 'Small', k.clock)}${opt('off', 'None', k.clock)}</select>
  <label for='lkpos'>Position</label><select id='lkpos'${D}>${opt('center', 'In the middle', k.pos)}${opt('left', 'On the left', k.pos)}${opt('bottom', 'Bottom left', k.pos)}</select>
  <label>Account</label><div><label class='ck'><input type='checkbox' id='lkuser'${k.user ? ' checked' : ''}${D}> Show the account’s picture and name when locked</label></div>
  <label></label><div><button class='lkprev'>Preview</button></div></div>
  <div class='msg'>For everyone on this computer, on the lock screen and the sign-in page.${adm ? '' : ' Only administrators can change it.'}</div>`;
  const at = c.querySelector(':scope > .sfoot'); if (at) at.before(d); else c.appendChild(d);
  const q = s => d.querySelector(s);
  const show = () => { const bg = q('#lkbg').value; q('.lkpicrow').style.display = bg == 'picture' ? '' : 'none'; q('.lkcolrow').style.display = bg == 'color' ? '' : 'none'; q('.lkbv').textContent = q('#lkblur').value + ' px'; q('.lkdv').textContent = q('#lkdim').value + ' %' };
  const save = () => { SYSC.lk = {bg: q('#lkbg').value, color: q('#lkcol').value, blur: +q('#lkblur').value, dim: +q('#lkdim').value, clock: q('#lkck').value, pos: q('#lkpos').value, user: q('#lkuser').checked}; sysSave(); show(); try { lkApply($('#login'), 'login') } catch (e) {} };
  ['#lkbg', '#lkcol', '#lkblur', '#lkdim', '#lkck', '#lkpos', '#lkuser'].forEach(s => q(s).addEventListener(s == '#lkblur' || s == '#lkdim' ? 'input' : 'change', e => { e.stopPropagation(); if (adm) save() }));
  show();
  /* a picture of your own: scaled to the screen and kept as a JPEG */
  q('.lkpic').onclick = e => { e.preventDefault(); const f = document.createElement('input'); f.type = 'file'; f.accept = 'image/*';
    f.onchange = () => { const file = f.files && f.files[0]; if (!file) return; const im = new Image(), u = URL.createObjectURL(file);
      im.onload = () => { const s = Math.min(1, 1920 / im.width, 1080 / im.height), cv = document.createElement('canvas'); cv.width = Math.round(im.width * s); cv.height = Math.round(im.height * s);
        cv.getContext('2d').drawImage(im, 0, 0, cv.width, cv.height); URL.revokeObjectURL(u);
        if (!svj(LK_PICK, cv.toDataURL('image/jpeg', .85))) return notify('Lock and sign-in screen', 'The picture is too large to keep.', {kind: 'error'});
        q('#lkbg').value = 'picture'; save(); notify('Lock and sign-in screen', 'Picture set.', {quiet: true}) };
      im.onerror = () => { URL.revokeObjectURL(u); notify('Lock and sign-in screen', 'That file is not a picture.', {kind: 'error'}) }; im.src = u };
    f.click() };
  /* a preview: what the lock screen will look like; any key or click ends it */
  q('.lkprev').onclick = e => { e.preventDefault(); if (document.getElementById('lkprevov')) return;
    const p = document.createElement('div'); p.id = 'lkprevov'; p.style.cssText = 'position:fixed;inset:0;z-index:1000009;color:#e2e8f0;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:6px;background:radial-gradient(circle at 30% 20%,#1e3a8a,#020617);cursor:pointer';
    p.innerHTML = `<div class='lkclock'></div><div class='lkdate'></div><div class='lkbox'><div class='lkav'>👤</div><div class='lkn'></div><div class='msg' style='opacity:.8'>Preview: click or press a key to close</div></div>`;
    p.querySelector('.lkn').textContent = USER ? USER.n : ''; const cp = clockParts(); p.querySelector('.lkclock').textContent = cp.time; p.querySelector('.lkdate').textContent = cp.longNoYear;
    document.body.appendChild(p); lkApply(p, 'lock');
    const end = () => { p.remove(); removeEventListener('keydown', end, true) }; p.onclick = end; setTimeout(() => addEventListener('keydown', end, true), 50) };
} })();

/* ---------- the shutting-down screen ---------- */
/* how long a sound plays, from its notes ([frequency, start, length, …]); the orchestral voices are about 2.4 s */
function sfxLen(n) { try { const p = PAT[n]; if (Array.isArray(p) && p.length && Array.isArray(p[0])) return Math.max(...p.map(x => (+x[1] || 0) + (+x[2] || 0))) } catch (e) {} return 2.4 }
function shutScreen(a) {
  let o = document.getElementById('shutov'); if (o) return o;
  o = document.createElement('div'); o.id = 'shutov'; o.setAttribute('role', 'status'); o.setAttribute('aria-live', 'polite');
  o.style.cssText = 'position:fixed;inset:0;z-index:1000020;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:14px;background:radial-gradient(circle at 50% 40%,#16213d,#04060c 75%);color:#e2e8f0;font-family:Tahoma,Arial,sans-serif;cursor:none';
  const logo = ($('#login .rlogo') || $('#start img') || {}).src || '';
  o.innerHTML = `${logo ? `<img alt='' src='${logo}' style='width:112px;height:112px;opacity:.95'>` : ''}<div class='shutt' style='font-size:24px'></div><div class='shutm' style='font-size:14px;opacity:.75;min-height:1.2em'></div><div class='shutd' style='letter-spacing:6px;font-size:22px;opacity:.8'></div>`;
  o.querySelector('.shutt').textContent = a == 'reboot' ? 'Restarting…' : 'Shutting down…';
  document.body.appendChild(o); let i = 0; o.__iv = setInterval(() => { o.querySelector('.shutd').textContent = '•••'.slice(0, i++ % 4) }, 350);
  return o;
}
function shutScreenSay(t) { const o = document.getElementById('shutov'); if (o) o.querySelector('.shutm').textContent = t }
function shutScreenGone() { const o = document.getElementById('shutov'); if (o) { clearInterval(o.__iv); o.remove() } }
