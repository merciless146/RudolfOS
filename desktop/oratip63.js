/* ===== Rudolf OS 1.6.3: each of Oracle's tabs says in its tooltip how much memory it uses (docs/plan-1.6.3.md Part 4) =====
   The owner (2026-11-24 22:14 CST): "Can we have Oracle show the memory usage of a tab in the tooltip when the mouse is held over
   that tab?" The tab's tooltip (its title and its address) gains a line for its memory, the same on Native, Chromium and Lite,
   honest about what can be known:
     a tab alone in its process   that process's memory as Task Manager counts it (Pss), with the frames of other sites the page
                                  holds (their own processes: site isolation) said apart
     tabs sharing a process       "Shares a process with 2 other tabs: 180 MB together"
     a freed tab (💤)              none until it is chosen
     Oracle's own pages           part of Oracle's page
   Chromium does not say which process shows which tab: rudolf-launch finds it (a trace of the shell's frames, each with its
   process; the addresses hashed) and reads each process's Pss, while Task Manager or a tooltip asks; rudolfd's processes reply
   passes it on ("pages" for the desktop's Chromium, "oframes" for Oracle's own: Native, the encrypted setting, Tor windows).
   Measured when the tooltip opens (the mouse rested on the tab), again every 3 seconds while it shows, never while none shows.
   Lite: WebKit does not say which of its page processes shows which tab, so the tooltip says so and gives Lite's page processes
   together. ===== */
const OTIP = {el: null, tab: null, timer: 0, poll: 0, gen: 0, data: null, at: 0};
/* the address's hash as rudolf-launch makes it (FNV-1a of the address without its #part) */
const otipHash = u => { u = String(u || '').split('#')[0]; let h = 0x811c9dc5; for (let i = 0; i < u.length; i++) { h ^= u.charCodeAt(i) & 0xff; h = Math.imul(h, 0x01000193) >>> 0 } return h };
const otipMB = kb => kb >= 1048576 ? (kb / 1048576).toFixed(1) + ' GB' : Math.max(1, Math.round(kb / 1024)) + ' MB';
const otipN = (n, one, many) => n + ' ' + (n == 1 ? one : many);
/* the measurements in a processes reply that are fresh (taken at or after `since`, in seconds): [{frames, mem}] */
function otipSources(j, since) {
  const out = [], add = s => { if (s && Array.isArray(s.frames) && +s.ftime >= since) out.push({frames: s.frames, mem: s.mem && typeof s.mem == 'object' ? s.mem : {}}) };
  if (j) { add(j.pages); if (Array.isArray(j.oframes)) j.oframes.forEach(add) }
  return out;
}
/* what a real tab at `url` uses, from fresh measurements (otipSources) and the processes' list (rudolfd's, for a process whose Pss
   could not be read): a line of text */
function otipCalc(url, src, items) {
  const F = [], mem = {};
  src.forEach(s => { s.frames.forEach(f => { if (f && +f.p > 0 && typeof f.f == 'string') F.push({h: +f.h >>> 0, p: +f.p, m: !!f.m, f: f.f, up: typeof f.up == 'string' ? f.up : ''}) });
    for (const k in s.mem) { const v = s.mem[k]; if (Array.isArray(v)) mem[+k] = [+v[0], +v[1]] } });
  const rss = {}; (items || []).forEach(p => { if (p && +p.pid > 0) rss[+p.pid] = +p.rss });
  const kb = p => { const v = mem[p]; if (v && v[0] >= 0) return v[0]; if (v && v[1] >= 0) return v[1]; return rss[p] > 0 ? rss[p] / 1024 : 0 };
  const H = otipHash(url), mains = F.filter(f => f.m && f.h == H);
  if (!mains.length) return 'Memory: not measured yet';
  if (new Set(mains.map(f => f.p)).size > 1) return 'Memory: not told apart: this address is open in more than one tab';
  /* each frame's tab: its own page, or the page it is inside (frames of other sites) */
  const by = new Map(F.map(f => [f.f, f])), root = f => { for (let i = 0; f && !f.m && i < 32; i++) f = by.get(f.up); return f && f.m ? f.f : null };
  const pidsOf = new Map(), tabsIn = new Map();
  F.forEach(f => { const r = root(f); if (!r) return; if (!pidsOf.has(r)) pidsOf.set(r, new Set()); pidsOf.get(r).add(f.p);
    if (!tabsIn.has(f.p)) tabsIn.set(f.p, new Set()); tabsIn.get(f.p).add(r) });
  /* the tabs it shares a process with, and theirs */
  const group = new Set(mains.map(f => f.f)), todo = [...group];
  while (todo.length) { const r = todo.pop(); for (const p of pidsOf.get(r) || []) for (const o of tabsIn.get(p) || []) if (!group.has(o)) { group.add(o); todo.push(o) } }
  const pids = new Set(); group.forEach(r => (pidsOf.get(r) || []).forEach(p => pids.add(p)));
  const total = [...pids].reduce((a, p) => a + kb(p), 0);
  if (group.size > 1) return `Memory: shares a process with ${otipN(group.size - 1, 'other tab', 'other tabs')}: ${otipMB(total)} together`;
  const page = kb(mains[0].p), r0 = mains[0].f, others = F.filter(f => !f.m && f.p != mains[0].p && root(f) == r0).length;
  return 'Memory: ' + otipMB(total) + (total > page ? ` (the page ${otipMB(page)}; ${otipN(others, 'frame', 'frames')} from other sites ${otipMB(total - page)})` : '');
}
/* Lite: what WebKit's page processes use together (resident memory: rudolfd's list) */
function otipLite(items) {
  const w = (items || []).filter(p => p && /^WebKitWebProc/.test(String(p.name || '')));
  const kb = w.reduce((a, p) => a + (+p.rss > 0 ? p.rss / 1024 : 0), 0);
  return 'Memory: Lite\'s WebKit does not say which page process shows which tab' + (w.length ? `; its ${otipN(w.length, 'page process', 'page processes')} (the desktop's own among them) use ${otipMB(kb)} together` : '');
}
const otipLiteOn = () => typeof RD != 'undefined' && RD && RD.engine == 'webkit';
/* the line for a tab now: from what is known without asking, or null (to be measured) */
function otipNow(x) {
  if (!x) return '';
  if (x.slept) return 'Memory: none until you choose it (💤 freed)';
  const u = x.ru || (x.h && x.h[x.i]) || '';
  if (!x.real || !/^https?:/i.test(u)) return 'Memory: part of Oracle\'s own page';
  if (typeof RD == 'undefined' || !RD) return 'Memory: measured on Rudolf OS itself';
  return null;
}
function otipText(x) {
  const u = x.ru || (x.h && x.h[x.i]) || '', a = /^about:home/.test(u) ? '' : u;
  return {t: x.title || 'New tab', a: a.length > 300 ? a.slice(0, 300) + '…' : a};
}
function otipDraw(d, x, line) {
  let e = OTIP.el;
  if (!e) { e = OTIP.el = document.createElement('div'); e.className = 'otip'; e.setAttribute('role', 'tooltip'); e.id = 'otip63';
    e.innerHTML = '<b></b><span class="otipa"></span><span class="otipm"></span>'; document.body.appendChild(e) }
  const {t, a} = otipText(x);
  e.children[0].textContent = t; e.children[1].textContent = a; e.children[1].hidden = !a; e.children[2].textContent = line;
  e.hidden = false;
  const r = d.getBoundingClientRect(), w = Math.min(380, innerWidth - 16);
  e.style.maxWidth = w + 'px'; e.style.left = Math.max(8, Math.min(r.left, innerWidth - w - 8)) + 'px'; e.style.top = (r.bottom + 4) + 'px';
  const h = e.getBoundingClientRect(); if (h.bottom > innerHeight - 4) e.style.top = Math.max(4, r.top - h.height - 4) + 'px';
}
function otipHide() {
  clearTimeout(OTIP.timer); clearTimeout(OTIP.poll); OTIP.timer = OTIP.poll = 0; OTIP.gen++; OTIP.tab = null;
  if (OTIP.el) OTIP.el.hidden = true;
}
/* while the tooltip shows: ask (rudolfd's processes: Task Manager's own reply, which has rudolf-launch measure), until a
   measurement taken after it opened is there (up to 8 s), then every 3 s */
async function otipMeasure(d, x, gen, since, tries) {
  if (gen != OTIP.gen) return;
  if (!d.isConnected) { otipHide(); return }   /* (the bar was drawn again) */
  const now = otipNow(x); if (now != null) { otipDraw(d, x, now); return }
  let j = null; try { j = await rd('processes') } catch (e) {}
  if (gen != OTIP.gen) return;
  if (!d.isConnected) { otipHide(); return }
  let line;
  if (otipLiteOn()) line = j ? otipLite(j.items) : 'Memory: not known (Rudolf OS did not answer)';
  else { const src = otipSources(j, since);
    if (src.length) { OTIP.data = j; OTIP.at = since; line = otipCalc(x.ru || x.h[x.i], src, j.items) }
    else if (tries < 8) line = 'Memory: measuring…';
    else { const old = otipSources(j, since - 30); line = old.length ? otipCalc(x.ru || x.h[x.i], old, j.items) : 'Memory: not known (Chromium did not say)' } }
  otipDraw(d, x, line);
  const fresh = !/measuring/.test(line);
  OTIP.poll = setTimeout(() => otipMeasure(d, x, gen, fresh ? Math.floor(Date.now() / 1000) - 1 : since, fresh ? 0 : tries + 1), fresh ? 3000 : 1000);
}
/* a tab of Oracle's bar (base_v24.html sync: each tab drawn): its tooltip is this, not the browser's own. (It takes the pointer, so
   that what covers a real tab (tabs26.js rtCover, by the topmost element) finds it: the page is cut around it, as for a menu.) */
function oTabTip(d, x) {
  if (!d || !x) return;
  const {t, a} = otipText(x); d.setAttribute('aria-label', t + (a ? ', ' + a : '')); d.setAttribute('aria-describedby', 'otip63');
  /* (the browser's own tooltip, from the tab's title, not shown while the mouse is on it; the title stays otherwise) */
  const back = () => { if (d.dataset.ot != null) { d.title = d.dataset.ot; delete d.dataset.ot } };
  d.addEventListener('mouseenter', () => { otipHide(); if (d.hasAttribute('title')) { d.dataset.ot = d.title; d.removeAttribute('title') } const gen = OTIP.gen; OTIP.tab = x;
    OTIP.timer = setTimeout(() => { if (gen != OTIP.gen || !d.isConnected) return; const n = otipNow(x); otipDraw(d, x, n != null ? n : 'Memory: measuring…');
      if (n == null) otipMeasure(d, x, gen, Math.floor(Date.now() / 1000), 0) }, 600) });
  d.addEventListener('mouseleave', () => { otipHide(); back() }); d.addEventListener('pointerdown', otipHide);
}
{ const st = document.createElement('style');
  st.textContent = '.otip{position:fixed;z-index:2147483000;display:flex;flex-direction:column;gap:2px;padding:6px 9px;border:1px solid var(--bd,#9ca3af);border-radius:6px;background:var(--wbg,#fff);color:var(--fg,#111827);font-size:12px;line-height:1.35;box-shadow:0 4px 14px rgba(0,0,0,.18);overflow-wrap:anywhere}.otip[hidden]{display:none}.otip .otipa{color:#6b7280}.otip .otipm{margin-top:2px}';
  document.head.appendChild(st) }
addEventListener('blur', otipHide);
