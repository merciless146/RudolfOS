/* ===== V26 screen resolution and frame rate (Settings › Display, Rudolf OS 1.1.2) =====
   rudolfd's screen API (/usr/lib/rudolf/screenctl). A new setting restarts the display (a few seconds, the
   desktop page reloads); afterwards the desktop asks "Keep this?" and goes back by itself after 15 seconds, and
   screenctl does the same after 90 seconds if the screen shows nothing at all.
   - software display: the desktop is drawn at the chosen size and rudolf-display scales it to the screen,
     keeping its shape (black bars); the screen itself refreshes about 60 times a second (firmware mode)
   - Xorg: the monitor's own mode of that size and refresh rate, or the nearest one scaled with black bars ===== */
const SCR_RES = ['1920x1080', '1366x768', '1280x720', '1024x576'];
const SCR_FPS = [120, 90, 60, 30];
const scrX = r => String(r).replace('x', '×');
/* the refresh rates the monitor offers at a size (Xorg), or null when not known */
function scrRates(st, res) {
  if (!st || st.display != 'gpu' || !st.modes || !st.modes.modes) return null;
  const [w, h] = (res == 'auto' ? st.modes.native : res).split('x').map(Number);
  const m = st.modes.modes.find(m => m.w == w && m.h == h) || st.modes.modes.find(m => st.modes.native == m.w + 'x' + m.h);
  return m ? m.hz : null;
}
/* is a frame rate available at a resolution, and if not, why */
function scrFpsWhy(st, res, f) {
  if (!st) return '';
  if (st.display != 'gpu') return f > 60 ? 'this screen is driven by the firmware, which refreshes it about 60 times a second; the graphics-card display (Settings › Display › Graphics) can go faster' : '';
  const hz = scrRates(st, res);
  if (!hz || f == 60) return '';
  return hz.some(x => Math.abs(x - f) <= 2) ? '' : 'the monitor does not offer ' + f + ' Hz at this resolution';
}
function screenUi(resBox, fpsBox) {
  if (!RD) { resBox.innerHTML = `<div class='msg'>${screen.width}×${screen.height} at ${devicePixelRatio * 100}% · set by your device</div>`; fpsBox.innerHTML = `<div class='msg'>Set by your device.</div>`; return }
  resBox.innerHTML = `<div class='msg'>Loading…</div>`; fpsBox.innerHTML = '';
  rd('screen').then(st => {
    const panel = st.display == 'gpu' && st.modes ? st.modes.native : st.actual.panel || (st.actual.w + 'x' + st.actual.h);
    const native = (r) => st.display == 'gpu' && st.modes ? st.modes.modes.some(m => m.w + 'x' + m.h == r) : r == panel;
    resBox.innerHTML = `<select id='dres' aria-describedby='dresm'>${optH('auto', `Automatic (${scrX(panel)}, the screen’s own)`, st.res)}${SCR_RES.map(r => optH(r, scrX(r) + (native(r) ? '' : ' (scaled)'), st.res)).join('')}</select>
      <div class='msg' id='dresm'></div>`;
    fpsBox.innerHTML = `<select id='dfps' aria-describedby='dfpsm'></select><div class='msg' id='dfpsm'></div>
      <div class='rmrow' style='justify-content:flex-start;margin-top:6px'><button class='dsap pri' disabled>Apply</button><span class='msg dsmsg'></span></div>`;
    const R = resBox.querySelector('#dres'), F = fpsBox.querySelector('#dfps'), A = fpsBox.querySelector('.dsap');
    const a = st.actual, now = a.w ? `Now ${scrX(a.w + 'x' + a.h)}${a.hz ? ' at ' + Math.round(a.hz) + (st.display == 'gpu' ? ' Hz' : ' frames a second') : ''}${a.kind == 'scaled' ? `, scaled to the ${scrX(a.panel)} screen with black bars where the shapes differ` : ''}.` : '';
    resBox.querySelector('#dresm').textContent = now + (st.persistent ? '' : ' Not installed: the setting lasts until Rudolf OS restarts.');
    const fill = () => {
      const keep = F.value || String(st.fps);
      F.innerHTML = SCR_FPS.map(f => { const why = scrFpsWhy(st, R.value, f); return `<option value='${f}'${why ? ' disabled' : ''} title='${why}'>${f} fps${why ? ' (not available)' : f == 60 ? ' (default)' : f == 30 ? ' (saves power)' : ''}</option>` }).join('');
      F.value = [...F.options].some(o => o.value == keep && !o.disabled) ? keep : '60';
      const off = SCR_FPS.filter(f => scrFpsWhy(st, R.value, f));
      fpsBox.querySelector('#dfpsm').textContent = off.length ? off.join(' and ') + ' fps: ' + scrFpsWhy(st, R.value, off[0]) + '.' : '';
      A.disabled = R.value == st.res && F.value == String(st.fps);
    };
    R.onchange = fill; F.onchange = fill; fill();
    A.onclick = () => sdlg({body: resBox.closest('.win') || document.body}, 'Change the display?', `The screen goes dark for a few seconds while the display starts again with ${R.value == 'auto' ? 'the screen’s own resolution' : scrX(R.value)} at ${F.value} fps. Your files are saved; open Linux apps and the browser window for sign-in sites are closed. If the picture does not come back, wait: the previous setting returns by itself.`,
      [['Cancel'], ['Change', async () => {
        try { if (typeof fsFlush == 'function') fsFlush() } catch (e) {}
        try { await rdPost(`screen?res=${R.value}&fps=${F.value}`); fpsBox.querySelector('.dsmsg').textContent = 'Starting the display again…' }
        catch (e) { fpsBox.querySelector('.dsmsg').textContent = e.message }
      }, 1]]);
  }).catch(e => { resBox.innerHTML = `<div class='msg'>${e.message}</div>` });
}
/* after a change: keep it, or go back after 15 seconds */
async function screenPending() {
  let st; try { st = await rd('screen') } catch (e) { return }
  if (!st.pending || document.getElementById('scrkeep')) return;
  const o = document.createElement('div'); o.id = 'scrkeep'; o.className = 'rmodal'; o.style.zIndex = 2147483000;
  o.innerHTML = `<div class='rmbox' role='alertdialog' aria-modal='true' aria-labelledby='scrkh'><h3 id='scrkh' style='margin-top:0'>Keep this display setting?</h3>
    <p>${scrX(st.actual.w + 'x' + st.actual.h)}${st.actual.hz ? ' at ' + Math.round(st.actual.hz) + (st.display == 'gpu' ? ' Hz' : ' fps') : ''}. The previous setting comes back in <b class='scrn'>15</b> seconds.</p>
    <div class='rmrow'><button class='scrb'>Go back</button><button class='scrk pri'>Keep</button></div></div>`;
  document.body.appendChild(o); o.querySelector('.scrk').focus();
  let n = 15; const iv = setInterval(() => { n--; o.querySelector('.scrn').textContent = n; if (n <= 0) back() }, 1000);
  const done = () => { clearInterval(iv); o.remove() };
  const back = () => { done(); rdPost('screen/revert').catch(() => {}) };
  o.querySelector('.scrb').onclick = back;
  o.querySelector('.scrk').onclick = () => { done(); rdPost('screen/keep').then(() => notify('Display', 'The new setting is kept.', {toast: 3000})).catch(() => {}) };
}
if (RD) setTimeout(screenPending, 2500);
