/* ===== Rudolf OS 1.6.3: Oracle frees the memory of tabs you have not looked at for a while (docs/plan-1.6.3.md Part 4) =====
   Each of Oracle's real tabs keeps its own page and process, also when it has not been looked at for hours: measured with
   iso/qt/native/oramem63_t.js (a real Chromium, Oracle's tabs as the shell makes them), about 40 MB a stand-in news page and
   more for real sites. A tab that is not its window's current one, and has not been for S.oraFree minutes (Settings › Oracle ›
   Memory: 15 minutes, 30 minutes (the default), 1 hour, 2 hours or Never), is freed as game mode frees one (gamesleep59.js):
   its page is closed, its address, title and how far it was scrolled are kept (💤 on the tab), and choosing it loads it
   again where it was. A tab playing sound or video, or holding text typed and not sent, is never freed (the shell's "busy"
   question; no answer in 2 s counts as busy), nor a game's full-screen tab. When memory runs low (the memory guard's warning)
   the background tabs are freed at once, before the guard has to close a page. Nothing else changes: each tab keeps its own
   process (site isolation), Chromium's sandbox, private and Tor windows' separation and the encrypted folders are as they
   were. The desktops: Chromium and Native (the same shell); Lite's WebKit tabs answer the same question, and come back at
   their address (WebKit's tabs do not say how far they were scrolled). */
const OFREE_CH = [15, 30, 60, 120, 0];
if (S.oraFree === undefined || !OFREE_CH.includes(+S.oraFree)) S.oraFree = 30;
const OFREE = {seen: new Map(), busy: false};
const ofreeMin = () => OFREE_CH.includes(+S.oraFree) ? +S.oraFree : 30;
/* now: free every background tab that may sleep (memory is low); otherwise only those not looked at for the chosen time */
async function ofreeTick(now) {
  if (OFREE.busy || typeof RT == 'undefined' || typeof rtBusy != 'function') return 0;
  const min = ofreeMin(); if (!min) return 0;
  const t0 = Date.now(); let n = 0; OFREE.busy = true;
  try {
    for (const [tid, w] of [...RT]) {
      if (!w || typeof w.sleep != 'function') continue;
      let shown = true; try { shown = !!(w.shown && w.shown()) } catch (e) {}
      if (shown) { OFREE.seen.set(tid, t0); continue }
      if (!OFREE.seen.has(tid)) { OFREE.seen.set(tid, t0); if (!now) continue }   /* (a tab's clock starts when it is first seen) */
      if (!now && t0 - OFREE.seen.get(tid) < min * 60000) continue;
      if (typeof GAME_FS != 'undefined' && GAME_FS.has(tid)) continue;
      const b = await rtBusy(tid); if (b.m || b.f) continue;
      if (!RT.has(tid)) continue;
      try { if (w.shown && w.shown()) continue } catch (e) { continue }   /* (chosen while it was asked) */
      if (w.sleep(b.y || 0)) { n++; OFREE.seen.delete(tid) }
    }
    for (const k of [...OFREE.seen.keys()]) if (!RT.has(k)) OFREE.seen.delete(k);
  } finally { OFREE.busy = false }
  return n;
}
setInterval(() => { ofreeTick(false).catch(() => {}) }, 60000);
/* memory is low: the guard's warning ("memory warn PID MB"). In game mode gamesleep59.js frees them for the game, and says so */
if (typeof evOn == 'function') evOn(t => {
  if (!/^memory warn \d+ \d+$/m.test(String(t))) return;
  try { if (typeof gameMode == 'function' && gameMode()) return } catch (e) {}
  ofreeTick(true).then(n => { if (n) notify('Oracle freed memory', `Memory was low, so ${n == 1 ? 'a tab you were not looking at was' : n + ' tabs you were not looking at were'} freed (💤). Choose one to load it again.`, {app: 'Oracle', quiet: true}) }).catch(() => {});
});
/* Settings › Oracle › Memory */
function ofreeRow(c) {
  const box = document.createElement('div'); box.className = 'ofreebox';
  const opt = (v, t) => `<option value='${v}'${ofreeMin() == v ? ' selected' : ''}>${t}</option>`;
  box.innerHTML = `<h4 class='sh4'>Memory</h4><div class='r2'><label for='ofreesel'>Free tabs you have not looked at for</label><div><select id='ofreesel'>${opt(15, '15 minutes')}${opt(30, '30 minutes')}${opt(60, '1 hour')}${opt(120, '2 hours')}${opt(0, 'Never')}</select>
<div class='msg'>A tab that is not the one shown in its window closes its page after that time and keeps its address (💤 on the tab); choosing it loads it again where it was. Tabs playing sound or video, or holding text you have typed and not sent, are never freed. When memory runs low, the tabs you are not looking at are freed at once (unless this is Never).</div></div></div>`;
  c.appendChild(box);
  /* (kept at once, as the page's other Oracle choices: its Apply is web search's) */
  box.querySelector('#ofreesel').onchange = e => { const v = +e.target.value; if (!OFREE_CH.includes(v)) return; S.oraFree = v; if (typeof SC == 'object' && SC) SC.oraFree = v; persistS();
    try { logEvt('sys', `${USER ? USER.n : 'Someone'} set Oracle to free tabs not looked at for ${e.target.selectedOptions[0].textContent.toLowerCase()}`) } catch (x) {} };
}
if (typeof signinPanel == 'function') { const _sp = signinPanel; signinPanel = function (c) { const r = _sp.apply(this, arguments); try { ofreeRow(c) } catch (e) {} return r } }
