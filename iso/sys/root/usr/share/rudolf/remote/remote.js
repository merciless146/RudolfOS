/* Rudolf OS 1.5.0: the remote-access viewer (served by rudolf-remote). Signs in, then draws the screen from the
   WebSocket (changed 64x64 tiles, zlib-compressed BGRX) and sends the keyboard and mouse back. */
'use strict';
const $ = id => document.getElementById(id);
const show = id => { for (const s of ['signin', 'wait', 'bye', 'view']) $(s).hidden = s != id };
let ws = null, W = 0, H = 0, ctx = null, chain = Promise.resolve(), ended = false;

async function start() {
  let st = {paired: false};
  try { st = await (await fetch('/status', {cache: 'no-store'})).json() } catch (e) {}
  $('codeRow').hidden = !!st.paired; $('code').required = !st.paired;
  show('signin'); ($('codeRow').hidden ? $('user') : $('code')).focus();
}
$('f').onsubmit = async e => {
  e.preventDefault(); $('msg').textContent = ''; $('go').disabled = true;
  const b = new URLSearchParams({code: $('code').value.trim(), user: $('user').value.trim(), pass: $('pass').value});
  let j = {};
  try { const r = await fetch('/login', {method: 'POST', body: b, headers: {'content-type': 'application/x-www-form-urlencoded'}}); j = await r.json() } catch (x) { j = {error: 'The computer could not be reached.'} }
  $('pass').value = ''; $('go').disabled = false;
  if (!j.ok) { $('msg').textContent = j.error || 'It did not work.'; if (j.needCode) { $('codeRow').hidden = false; $('code').focus() } return }
  $('who').textContent = $('user').value.trim() + ' on Rudolf OS';
  connect();
};
function connect() {
  ended = false; show('wait'); $('waitMsg').textContent = 'Connecting…';
  ws = new WebSocket('wss://' + location.host + '/ws'); ws.binaryType = 'arraybuffer';
  ws.onmessage = ev => {
    if (typeof ev.data == 'string') {
      let m = {}; try { m = JSON.parse(ev.data) } catch (e) {}
      if (m.t == 'wait') { show('wait'); $('waitMsg').textContent = m.m }
      else if (m.t == 'go') { show('view'); $('c').focus() }
      else if (m.t == 'bye') { ended = true; $('byeMsg').textContent = m.m || 'The session ended.'; show('bye') }
      return;
    }
    const d = ev.data; chain = chain.then(() => frame(d)).catch(e => console.warn(e));
  };
  ws.onclose = () => { if (!ended) { $('byeMsg').textContent = 'The connection was closed.'; show('bye') } ws = null };
}
const send = s => { if (ws && ws.readyState == 1) ws.send(s) };
async function inflate(u8) {
  const s = new Blob([u8]).stream().pipeThrough(new DecompressionStream('deflate'));
  return new Uint8Array(await new Response(s).arrayBuffer());
}
async function frame(buf) {
  const v = new DataView(buf), u = new Uint8Array(buf);
  if (u[0] == 83) {   /* 'S': the screen's size */
    W = v.getUint16(1); H = v.getUint16(3); const c = $('c'); c.width = W; c.height = H; ctx = c.getContext('2d'); return;
  }
  if (u[0] != 84 || !ctx) return;   /* 'T': tiles */
  const n = v.getUint16(1); let o = 3; const jobs = [];
  for (let i = 0; i < n; i++) {
    const x = v.getUint16(o), y = v.getUint16(o + 2), w = v.getUint16(o + 4), h = v.getUint16(o + 6), l = v.getUint32(o + 8);
    const z = u.subarray(o + 12, o + 12 + l); o += 12 + l;
    jobs.push(inflate(z).then(px => {
      const im = ctx.createImageData(w, h), d = im.data;
      for (let p = 0; p < w * h * 4; p += 4) { d[p] = px[p + 2]; d[p + 1] = px[p + 1]; d[p + 2] = px[p]; d[p + 3] = 255 }
      ctx.putImageData(im, x, y);
    }));
  }
  await Promise.all(jobs); send('a');
}
/* ---- input */
const c = $('c');
const pos = e => { const r = c.getBoundingClientRect(); return [Math.round((e.clientX - r.left) * W / r.width), Math.round((e.clientY - r.top) * H / r.height)] };
let mv = null;
c.addEventListener('pointermove', e => { mv = pos(e); if (!mv.q) { requestAnimationFrame(() => { if (mv) send('m ' + mv[0] + ' ' + mv[1]); mv = null }); mv.q = 1 } });
const btn = b => b == 0 ? 1 : b == 1 ? 2 : b == 2 ? 3 : b == 3 ? 8 : 9;
c.addEventListener('pointerdown', e => { c.focus(); c.setPointerCapture(e.pointerId); const p = pos(e); send('m ' + p[0] + ' ' + p[1]); send('b ' + btn(e.button) + ' 1'); e.preventDefault() });
c.addEventListener('pointerup', e => { const p = pos(e); send('m ' + p[0] + ' ' + p[1]); send('b ' + btn(e.button) + ' 0'); e.preventDefault() });
c.addEventListener('contextmenu', e => e.preventDefault());
let wacc = 0;
c.addEventListener('wheel', e => {
  e.preventDefault(); wacc += e.deltaMode == 1 ? e.deltaY * 33 : e.deltaY;
  while (Math.abs(wacc) >= 50) { const b = wacc > 0 ? 5 : 4; send('b ' + b + ' 1'); send('b ' + b + ' 0'); wacc -= Math.sign(wacc) * 50 }
}, {passive: false});
const KS = {Enter: 0xff0d, Backspace: 0xff08, Tab: 0xff09, Escape: 0xff1b, Delete: 0xffff, Home: 0xff50, ArrowLeft: 0xff51, ArrowUp: 0xff52, ArrowRight: 0xff53, ArrowDown: 0xff54,
  PageUp: 0xff55, PageDown: 0xff56, End: 0xff57, Insert: 0xff63, ContextMenu: 0xff67, CapsLock: 0xffe5, NumLock: 0xff7f, ScrollLock: 0xff14, Pause: 0xff13, PrintScreen: 0xff61,
  ShiftLeft: 0xffe1, ShiftRight: 0xffe2, ControlLeft: 0xffe3, ControlRight: 0xffe4, AltLeft: 0xffe9, AltRight: 0xfe03, MetaLeft: 0xffeb, MetaRight: 0xffec};
const down = new Map();
function keysym(e) {
  if (KS[e.code]) return KS[e.code]; if (KS[e.key]) return KS[e.key];
  const f = /^F(\d{1,2})$/.exec(e.key); if (f) return 0xffbd + +f[1];
  if ([...e.key].length == 1) { const cp = e.key.codePointAt(0); return cp < 0x100 ? cp : 0x1000000 + cp }
  return 0;
}
addEventListener('keydown', e => {
  if ($('view').hidden) return; const k = down.get(e.code) || keysym(e); if (!k) return;
  down.set(e.code, k); send('k ' + k + ' 1'); e.preventDefault();
});
addEventListener('keyup', e => {
  if ($('view').hidden) return; const k = down.get(e.code) || keysym(e); down.delete(e.code); if (!k) return;
  send('k ' + k + ' 0'); e.preventDefault();
});
/* keys held when the window loses focus are let go on the computer */
addEventListener('blur', () => { for (const k of down.values()) send('k ' + k + ' 0'); down.clear() });
$('end').onclick = () => { ended = true; if (ws) ws.close(); $('byeMsg').textContent = 'You disconnected.'; show('bye') };
$('cancel').onclick = () => { ended = true; if (ws) ws.close(); start() };
$('again').onclick = () => start();
$('fit').onclick = () => $('wrap').classList.toggle('fit');
$('full').onclick = () => { if (document.fullscreenElement) document.exitFullscreen(); else $('view').requestFullscreen().catch(() => {}) };
$('wrap').classList.add('fit');
start();
