/* ===== V26 devices and updates.
   - Plugged-in devices: rudolfd passes on devplug's events ("dev {...}" lines). A notification names each device;
     USB drives and SD cards are mounted by Rudolf OS and the notification opens them in File Explorer.
   - Settings > Devices (drives, USB devices, keyboards, mice, touchpads) and Settings > Mouse and touchpad.
   - After an update, a notification says what changed; "What's new" shows CHANGELOG.md (RCHANGES). ===== */
const DEV_ICON = {'Keyboard': '⌨️', 'Mouse': '🖱️', 'Keyboard and mouse': '⌨️', 'Input device': '🖱️', 'Game controller': '🎮', 'Audio device': '🎧', 'Camera': '📷',
  'Phone or camera': '📱', 'Printer': '🖨️', 'Bluetooth adapter': '🔵', 'Wi-Fi adapter': '📶', 'Network adapter': '🌐', 'Smart card reader': '💳', 'USB drive': '🔌', 'USB device': '🔌'};
const devEsc = s => String(s == null ? '' : s).replace(/[&<>"']/g, c => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'}[c]));
const devDisk = d => ((/^(mmcblk\d+|nvme\d+n\d+)/.exec(d) || /^(sd[a-z]+)/.exec(d) || [d, d])[1]);
const devDriveName = v => v.label || (v.model && v.model.trim()) || (/^mmcblk/.test(v.dev) ? 'SD card' : 'USB drive');
let DEV_SEEN = false;

/* the events since the last look; the first look after start-up only takes note (devices that were already there) */
function devEvents(text) {
  const lines = String(text || '').split('\n').filter(l => l.startsWith('dev '));
  if (!DEV_SEEN) { DEV_SEEN = true; if (lines.length) dispatchEvent(new CustomEvent('rudolf-drive', {detail: {a: 'refresh'}})); return }
  for (const l of lines) { let v; try { v = JSON.parse(l.slice(4)) } catch (e) { continue } devEvent(v) }
}

function devOpenDrive(v) {
  const label = devDriveName(v) + (v.bytes ? ' (' + fmtB(v.bytes) + ')' : '');
  explorer({remote: {dev: v.dev, root: v.path, path: v.path, label}});
}

function devEvent(v) {
  if (v.t == 'drive') {
    const name = devDriveName(v), sd = /^mmcblk/.test(v.dev);
    if (v.a == 'add') notify((sd ? '💾 ' : '🔌 ') + name + ' is connected', (v.bytes ? fmtB(v.bytes) + ' · ' : '') + (v.readonly ? 'read only · ' : '') + 'click to open it in File Explorer', {app: 'Devices', action: () => devOpenDrive(v)});
    else if (v.a == 'eject') notify('⏏ ' + name + ' can be removed', 'It is safe to unplug it now.', {app: 'Devices', toast: 4000});
    else notify(name + ' was removed', 'Next time, use ⏏ Eject in File Explorer first so no file is left half-written.', {app: 'Devices', quiet: true, toast: 6000});
    dispatchEvent(new CustomEvent('rudolf-drive', {detail: v}));
  } else if (v.t == 'usb') {
    const ic = DEV_ICON[v.kind] || '🔌';
    if (v.a == 'add') notify(ic + ' ' + v.kind + ' connected', v.name + (v.driver ? '' : ' — Rudolf OS has no driver for this device yet, so it may not work.'), {app: 'Devices', kind: v.driver ? 'info' : 'warn'});
    else notify(ic + ' ' + v.kind + ' disconnected', v.name, {app: 'Devices', quiet: true, toast: 3000});
    dispatchEvent(new CustomEvent('rudolf-usb', {detail: v}));
  }
}

/* ---------- Settings > Devices ---------- */
function devPanel(c) {
  c.innerHTML = `<h3>Devices</h3><p class='msg'>What is plugged into this computer. USB drives and SD cards open in File Explorer by themselves; use ⏏ Eject before unplugging one.</p><div class='dvx'>Loading…</div><p><button class='dvr'>Refresh</button></p>`;
  const box = c.querySelector('.dvx');
  if (!RD) { box.textContent = 'Devices are shown on Rudolf OS itself (bootable disc or USB stick).'; return }
  const draw = async () => {
    let d; try { d = await rd('devices') } catch (e) { box.textContent = e.message; return }
    const sec = (t, rows) => `<h4 style='margin:12px 0 4px'>${t}</h4>` + (rows.length ? `<table class='dvt' style='width:100%;border-collapse:collapse'>${rows.join('')}</table>` : `<div class='msg'>None</div>`);
    const drives = d.drives.map(v => `<tr><td style='width:28px'>${/^mmcblk/.test(v.dev) ? '💾' : '🔌'}</td><td><b>${devEsc(devDriveName(v))}</b><br><small>${devEsc(fmtB(v.bytes))} · ${devEsc(v.fs)}${v.readonly ? ' · read only' : ''}${v.model ? ' · ' + devEsc(v.model) : ''}</small></td>
      <td style='text-align:right;white-space:nowrap'><button data-open='${devEsc(v.dev)}'>Open</button> <button data-eject='${devEsc(devDisk(v.dev))}'>⏏ Eject</button></td></tr>`);
    const usb = d.usb.filter(v => v.kind != 'USB drive' || !d.drives.length).map(v => `<tr><td>${DEV_ICON[v.kind] || '🔌'}</td><td><b>${devEsc(v.name)}</b><br><small>${devEsc(v.kind)} · ${devEsc(v.id)}${v.driver ? '' : ' · <span style="color:#b45309">no driver in Rudolf OS</span>'}</small></td><td></td></tr>`);
    const inp = d.inputs.map(v => `<tr><td>${{keyboard: '⌨️', mouse: '🖱️', touchpad: '👆', touchscreen: '📱'}[v.kind] || '🖱️'}</td><td><b>${devEsc(v.name)}</b><br><small>${devEsc(v.kind)}</small></td><td style='text-align:right'>${v.kind == 'touchpad' || v.kind == 'mouse' ? "<button data-mouse='1'>Settings</button>" : ''}</td></tr>`);
    box.innerHTML = sec('Drives and SD cards', drives) + sec('USB devices', usb) + sec('Keyboards, mice and touchpads', inp);
    box.querySelectorAll('[data-open]').forEach(b => b.onclick = () => { const v = d.drives.find(x => x.dev == b.dataset.open); v && devOpenDrive(v) });
    box.querySelectorAll('[data-eject]').forEach(b => b.onclick = async () => { b.disabled = true; try { await rdPost('eject?dev=' + b.dataset.eject); setTimeout(draw, 800) } catch (e) { b.disabled = false; notify('Eject', e.message, {kind: 'error'}) } });
    box.querySelectorAll('[data-mouse]').forEach(b => b.onclick = () => { const W = c.closest('.win'); const n = W && W.querySelector(".sn div[data-k='mouse']"); n && n.click() });
  };
  c.querySelector('.dvr').onclick = draw;
  const live = () => { if (!c.isConnected) { removeEventListener('rudolf-drive', live); removeEventListener('rudolf-usb', live); return } setTimeout(draw, 600) };
  addEventListener('rudolf-drive', live); addEventListener('rudolf-usb', live);
  draw();
}

/* ---------- Settings > Mouse and touchpad ---------- */
const PTR_DEF = {speed: 1, accel: 1, tap: true, natural: false, typing: true};
const ptrGet = () => Object.assign({}, PTR_DEF, S.ptr || {});
function pointerApply() {
  if (!RD) return;
  const p = ptrGet();
  rdPost(`pointer?speed=${p.speed}&accel=${p.accel}&tap=${p.tap ? 1 : 0}&natural=${p.natural ? 1 : 0}&typing=${p.typing ? 1 : 0}`).catch(() => {});
}
function mousePanel(c) {
  const p = ptrGet();
  c.innerHTML = `<h3>Mouse and touchpad</h3>
   <div class='r2' style='display:grid;grid-template-columns:200px 1fr;gap:10px;align-items:center;max-width:560px'>
    <label for='ptsp'>Pointer speed</label><span><input type='range' id='ptsp' min='0.3' max='3' step='0.1' style='width:220px'> <output class='ptspv'></output></span>
    <label for='ptac'>Acceleration (faster when you move quickly)</label><span><input type='range' id='ptac' min='1' max='3' step='0.25' style='width:220px'> <output class='ptacv'></output></span>
   </div>
   <h4 style='margin:16px 0 6px'>Touchpad</h4><div class='pttp msg'></div>
   <label class='chk'><input type='checkbox' id='pttap'> Tap to click (two fingers: right click, three: middle click)</label><br>
   <label class='chk'><input type='checkbox' id='ptnat'> Natural scrolling (content follows your fingers)</label><br>
   <label class='chk'><input type='checkbox' id='pttyp'> Ignore the touchpad while typing</label>
   <p class='msg'>Two fingers scroll up, down and sideways. On a clickpad, press with two fingers or in the bottom-right corner for a right click.</p>
   <p><button class='ptdef'>Restore defaults</button></p>`;
  const q = s => c.querySelector(s);
  const show = () => { const v = ptrGet(); q('#ptsp').value = v.speed; q('#ptac').value = v.accel; q('.ptspv').textContent = (+v.speed).toFixed(1) + '×'; q('.ptacv').textContent = v.accel == 1 ? 'off' : (+v.accel).toFixed(2) + '×'; q('#pttap').checked = v.tap; q('#ptnat').checked = v.natural; q('#pttyp').checked = v.typing };
  const save = () => { S.ptr = {speed: +q('#ptsp').value, accel: +q('#ptac').value, tap: q('#pttap').checked, natural: q('#ptnat').checked, typing: q('#pttyp').checked}; persistS(); show(); pointerApply() };
  ['#ptsp', '#ptac'].forEach(s => q(s).oninput = save);
  ['#pttap', '#ptnat', '#pttyp'].forEach(s => q(s).onchange = save);
  q('.ptdef').onclick = () => { S.ptr = Object.assign({}, PTR_DEF); persistS(); show(); pointerApply() };
  show();
  if (RD) rd('devices').then(d => { const t = d.inputs.filter(i => i.kind == 'touchpad'); q('.pttp').textContent = t.length ? 'Found: ' + t.map(i => i.name).join(', ') : 'No touchpad found on this computer. These settings apply when one is connected.' }).catch(() => {});
  else q('.pttp').textContent = 'These settings apply on Rudolf OS itself.';
}

/* ---------- What's new (CHANGELOG.md) ---------- */
function changesParse() {
  const out = []; let cur = null;
  for (const l of String(typeof RCHANGES == 'string' ? RCHANGES : '').split('\n')) {
    const h = /^## (\S+)\s*(?:\(([^)]*)\))?/.exec(l);
    if (h) { cur = {ver: h[1], date: h[2] || '', summary: '', items: []}; out.push(cur); continue }
    if (!cur) continue;
    if (/^Summary:/.test(l)) cur.summary = l.replace(/^Summary:\s*/, '');
    else if (/^- /.test(l)) cur.items.push(l.slice(2));
  }
  return out;
}
function changesHtml(only) {
  const vs = changesParse().filter(v => !only || only.includes(v.ver));
  if (!vs.length) return `<p>Rudolf OS ${devEsc(RVER)}</p>`;
  return vs.map(v => `<h3 style='margin:14px 0 4px'>Rudolf OS ${devEsc(v.ver)}${v.date ? ` <small style='font-weight:normal'>· ${devEsc(v.date)}</small>` : ''}${v.ver == RVER ? ` <small style='font-weight:normal'>· this version</small>` : ''}</h3>
    <ul>${v.items.map(i => { const m = /^(Fixed|New|Security|Changed):\s*/.exec(i); return `<li>${m ? `<b>${m[1]}:</b> ` + devEsc(i.slice(m[0].length)) : devEsc(i)}</li>` }).join('')}</ul>`).join('');
}
function whatsNew() {
  if (typeof helpApp == 'function') return helpApp('new');
  const W = appWin(null, "What's new", '🆕', 640, 520); W.body.style.cssText = 'padding:12px 18px;overflow:auto;background:var(--wbg,#fff)'; W.body.innerHTML = changesHtml(); return W;
}

/* ---------- the notice after an update (once per account and release) ---------- */
let UPD_FOR = null;
async function updNotice() {
  if (!USER || USER.r == 'guest' || UPD_FOR == USER.id) return;
  UPD_FOR = USER.id;
  let st = null; if (RD) { try { st = await rd('sys/status') } catch (e) {} }
  const cur = st && st.current && st.current.serial ? st.current : null;
  const key = cur ? 'r' + cur.serial : 'v' + RVER, ver = cur && cur.version ? cur.version : RVER;
  const before = S.seenRel;
  if (before == key) return;
  S.seenRel = key; persistS();
  // the first time this account sees a release: only announce it when the system was updated (not a new install)
  if (!before && !(st && st.booted && st.booted != 'disc')) return;
  const mine = changesParse().find(v => ver.startsWith(v.ver + '.') || ver == v.ver);
  const what = (cur && cur.notes && !/^(desktop update|security fixes|Linux |Chromium )/.test(cur.notes)) ? cur.notes : mine ? mine.summary : (cur && cur.notes) || '';
  notify('Rudolf OS was updated to ' + ver, (what ? what + ' · ' : '') + "click to see what's new.", {app: 'Update', action: whatsNew, toast: 9000});
}

/* applied after sign-in and whenever settings change */
(function () { const o = applySet; applySet = function () { const r = o.apply(this, arguments); try { if (USER) { pointerApply(); setTimeout(updNotice, 4000) } } catch (e) {} return r } })();
