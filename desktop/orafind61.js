/* ===== Rudolf OS 1.6.1 (docs/plan-1.6.1.md Part 10, docs/plan-oracle.md item 1): Oracle's find in page =====
   Ctrl+F, F3 (Shift+F3 back), Ctrl+G, or ☰ › Find in page… opens a bar under the bookmarks bar: the text box, "3 of 17", up and
   down, Match case and ✕. Enter goes to the next match, Shift+Enter to the previous one, Escape closes the bar. Text selected on
   the page fills the box. The bar belongs to its tab: switching tabs hides it, and switching back shows it with the same text.
   - Real tabs (Standard, Native, Standard's encrypted Oracle): the shell's tab op "find" (iso/shell/shell.js: the Controlled
     Frame's own find, as Chromium highlights matches; a script walking the page's text if that is not there); Lite: rudolf-webkit's
     WebKitFindController. Both answer with tabev "find" {id, n, i}.
   - Oracle's own pages (about: pages, files on C:, srcdoc): ORACLE_FIND_JS below, added to each, the same walker (CSS highlights,
     never the page's HTML changed). Websites in plain frames (no shell): "Find is not available for this page".
   browser() calls oraFind({W, T, tabs}) once per window; the window's menu gets "Find in page…" through __oracleMenu. ===== */
/* the walker, run in a page: {text, mc, again, back} -> {n, i} (i from 1); {stop: 1} clears; {sel: 1} -> {sel: the selected text}.
   (iso/shell/shell.js FIND_FN is the same function for real tabs without the Controlled Frame's find: oraclefind61_t.js checks both) */
function oraFindFn(o) {
  const F = window.__rudolfFind || (window.__rudolfFind = {q: null, mc: false, r: [], i: -1});
  const HL = typeof CSS != 'undefined' && CSS.highlights && typeof Highlight == 'function';
  const clear = () => { try { if (HL) { CSS.highlights.delete('rudolf-find'); CSS.highlights.delete('rudolf-find-cur') } else if (F.r.length) getSelection().removeAllRanges() } catch (e) {} F.r = []; F.i = -1; F.q = null };
  if (o.stop) { clear(); return {n: 0, i: 0} }
  if (o.sel) { let s = ''; try { s = String(getSelection() || '').trim() } catch (e) {} return {sel: s.length <= 200 && !/\n/.test(s) ? s : ''} }
  const q = String(o.text || '').slice(0, 500); if (!q) { clear(); return {n: 0, i: 0} }
  if (F.q !== q || F.mc !== !!o.mc || !o.again) {
    clear(); F.q = q; F.mc = !!o.mc;
    /* the visible text nodes, joined; a block's edge (or a hidden part) breaks a match, as in Chromium's find */
    const root = document.body || document.documentElement, nodes = [], blk = new Map();
    const blockOf = el => { let e = el; while (e && e !== root) { let b = blk.get(e); if (b === undefined) { let d = ''; try { d = getComputedStyle(e).display } catch (x) {} b = !/^(inline|contents)/.test(d); blk.set(e, b) } if (b) return e; e = e.parentElement } return root };
    if (root) { const tw = document.createTreeWalker(root, NodeFilter.SHOW_TEXT, {acceptNode: n => { const p = n.parentElement;
        if (!p || /^(SCRIPT|STYLE|NOSCRIPT|TEMPLATE|TEXTAREA|SELECT|OPTION)$/.test(p.tagName) || !n.data) return NodeFilter.FILTER_REJECT;
        if (p.checkVisibility && !p.checkVisibility({visibilityProperty: true})) return NodeFilter.FILTER_REJECT;
        return NodeFilter.FILTER_ACCEPT }});
      for (let n = tw.nextNode(), k = 0; n && k < 20000; n = tw.nextNode(), k++) nodes.push(n) }
    let txt = '', at = [], prev = null;
    for (const n of nodes) { const b = blockOf(n.parentElement); if (prev && b !== prev) txt += '\u0000'; prev = b; at.push([txt.length, n]); txt += n.data }
    const hay = F.mc ? txt : txt.toLowerCase(), nd = F.mc ? q : q.toLowerCase(), pos = (k) => { let lo = 0, hi = at.length - 1; while (lo < hi) { const m = (lo + hi + 1) >> 1; if (at[m][0] <= k) lo = m; else hi = m - 1 } return at[lo] };
    for (let k = hay.indexOf(nd); k >= 0 && F.r.length < 1000; k = hay.indexOf(nd, k + nd.length)) {
      try { const a = pos(k), b = pos(k + nd.length - 1), r = document.createRange(); r.setStart(a[1], k - a[0]); r.setEnd(b[1], k + nd.length - b[0]); F.r.push(r) } catch (e) {} }
    F.i = F.r.length ? (o.back ? F.r.length - 1 : 0) : -1;
    if (HL && F.r.length) { try { if (!document.getElementById('rudolf-find-css')) { const s = document.createElement('style'); s.id = 'rudolf-find-css';
        s.textContent = '::highlight(rudolf-find){background-color:#fde047;color:#111}::highlight(rudolf-find-cur){background-color:#f97316;color:#000}'; (document.head || document.documentElement).appendChild(s) }
      CSS.highlights.set('rudolf-find', new Highlight(...F.r)) } catch (e) {} }
  } else if (F.r.length) F.i = (F.i + (o.back ? F.r.length - 1 : 1)) % F.r.length;
  const r = F.r[F.i];
  if (r) { try { if (HL) CSS.highlights.set('rudolf-find-cur', new Highlight(r)); else { const s = getSelection(); s.removeAllRanges(); s.addRange(r) } } catch (e) {}
    try { const el = r.startContainer.parentElement, b = r.getBoundingClientRect(); if (b.top < 0 || b.bottom > innerHeight || b.left < 0 || b.right > innerWidth) el.scrollIntoView({block: 'center', inline: 'nearest'}) } catch (e) {} }
  return {n: F.r.length, i: F.i + 1};
}
/* in each of Oracle's own pages (sandboxed, scripts allowed): the walker, asked by the window ({rfind: o, id}); Ctrl+F, F3 and
   Ctrl+G pressed in the page go to the window (it cannot see a frame's keys) */
const ORACLE_FIND_JS = '<scr' + 'ipt>(function(){var F=' + oraFindFn.toString().replace(/<\/(script)/gi, '<\\/$1') + ';addEventListener("message",function(e){var d=e.data;if(e.source!==parent||!d||!d.rfind)return;var r;try{r=F(d.rfind)}catch(x){r={n:0,i:0}}parent.postMessage({rfound:r,id:d.id},"*")});'
  + 'addEventListener("keydown",function(e){if(!e.isTrusted)return;var c=e.ctrlKey||e.metaKey,a="";if(c&&!e.shiftKey&&!e.altKey&&e.code=="KeyF")a="cf";else if(!c&&!e.altKey&&e.code=="F3")a=e.shiftKey?"sf3":"f3";else if(c&&!e.altKey&&e.code=="KeyG")a=e.shiftKey?"sf3":"f3";if(a){e.preventDefault();e.stopPropagation();parent.postMessage({rfkey:a},"*")}},true)})()</scr' + 'ipt>';
/* which tab belongs to which window's find (for the menu, which is told the tab) */
const ORA_FIND_TABS = new WeakMap();
function oraFind(c) {
  const W = c.W, T = c.T, q = s => W.body.querySelector(s);
  const bar = document.createElement('div'); bar.className = 'ofind'; bar.hidden = true; bar.setAttribute('role', 'search');
  bar.innerHTML = "<input class='ofq' spellcheck='false' placeholder='Find in page' aria-label='Find in page'><span class='ofn' aria-live='polite'></span><button class='ofp' title='Previous match (Shift+Enter)' aria-label='Previous match'>▲</button><button class='ofx' title='Next match (Enter)' aria-label='Next match'>▼</button><label class='ofl'><input type='checkbox' class='ofc'> Match case</label><button class='ofz' title='Close (Esc)' aria-label='Close the find bar'>✕</button>";
  const obm = q('.obm'); if (obm) obm.after(bar); else W.body.prepend(bar);
  const inp = bar.querySelector('.ofq'), cnt = bar.querySelector('.ofn'), mc = bar.querySelector('.ofc');
  let ID = 0, tmr = 0;
  const st = t => t.ff || (t.ff = {open: false, q: '', mc: false, n: 0, i: 0, msg: ''});
  /* how this tab can be searched: 'rt' (a real tab), 'own' (Oracle's own page), '' (a website in a frame: not possible) */
  const how = t => t.real ? (t.rt ? 'rt' : '') : t.fr && t.fr.hasAttribute('srcdoc') && !t.fr.hasAttribute('src') ? 'own' : '';
  const show = t => { const f = st(t); cnt.textContent = f.msg || (f.q ? (f.n ? f.i + ' of ' + f.n : 'No results') : ''); cnt.classList.toggle('ofno', !!f.q && !f.n && !f.msg); inp.classList.toggle('ofno', !!f.q && !f.n && !f.msg) };
  const send = (t, o) => { const k = how(t), f = st(t); f.id = ++ID;
    if (k == 'rt') rtCmd(t.rt, o.stop ? 'findstop' : o.sel ? 'findsel' : 'find', Object.assign({id: f.id}, o));
    else if (k == 'own') { try { t.fr.contentWindow.postMessage({rfind: o, id: f.id}, '*') } catch (e) {} }
    return k };
  const run = (back, again) => { const t = T(); if (!t) return; const f = st(t); f.q = inp.value; f.mc = mc.checked;
    if (!f.q) { f.n = f.i = 0; f.msg = ''; send(t, {stop: 1}); show(t); return }
    f.msg = how(t) ? '' : 'Find is not available for this page';
    if (!f.msg) send(t, {text: f.q, mc: f.mc, back: !!back, again: !!again}); show(t) };
  const open = () => { const t = T(); if (!t) return; const f = st(t); f.open = true; sync();
    if (typeof rtBlur == 'function' && t.real) rtBlur();
    inp.focus(); inp.select(); setTimeout(() => { if (document.activeElement !== inp && f.open && t === T()) { inp.focus(); inp.select() } }, 60);   /* (again once the shell has given the keyboard back) */
    if (how(t)) send(t, {sel: 1}) };
  const close = () => { const t = T(); if (!t) return; const f = st(t); f.open = false; if (f.q) send(t, {stop: 1}); f.n = f.i = 0; sync();
    try { if (t.real && t.rt) rtCmd(t.rt, 'focus'); else W.el.focus() } catch (e) {} };
  /* (called after the window's own sync: a tab switched, opened, or its page changed. A tab whose address changed has lost its old
     matches; with its bar open it is searched again once the new page is there) */
  const sync = () => { const t = T(); (c.tabs() || []).forEach(x => ORA_FIND_TABS.set(x, api)); if (!t) { bar.hidden = true; return } const f = st(t);
    const u = t.h && t.h[t.i]; if (f.u !== u) { const was = f.u; f.u = u; if (was !== undefined && f.q) { f.n = f.i = 0; if (f.open) setTimeout(() => { if (t === T() && f.open && f.u === u) run(false, false) }, 700) } }
    bar.hidden = !f.open; if (inp.value !== f.q) inp.value = f.q; mc.checked = f.mc; show(t) };
  inp.addEventListener('input', () => { clearTimeout(tmr); tmr = setTimeout(() => run(false, false), 120) });
  inp.addEventListener('keydown', e => { if (e.key == 'Enter') { e.preventDefault(); clearTimeout(tmr); const t = T(); const f = t && st(t); run(e.shiftKey, !!(f && f.q === inp.value && f.mc === mc.checked && f.n)) }
    else if (e.key == 'Escape') { e.preventDefault(); e.stopPropagation(); close() } });
  mc.onchange = () => run(false, false);
  bar.querySelector('.ofx').onclick = () => run(false, true);
  bar.querySelector('.ofp').onclick = () => run(true, true);
  bar.querySelector('.ofz').onclick = close;
  /* an answer: from the shell or rudolf-webkit (tabev find), or from one of Oracle's own pages */
  const found = (t, o) => { if (!t || !o) return; const f = st(t);
    if (typeof o.sel == 'string') { if (o.sel && f.open && t === T() && !inp.value) { inp.value = o.sel; inp.select(); run(false, false) } return }
    if (o.id && o.id !== f.id) return;
    f.n = Math.max(0, o.n | 0); f.i = Math.max(0, Math.min(f.n, o.i | 0)); if (t === T()) show(t) };
  const onm = e => { if (!W.el.isConnected) { removeEventListener('message', onm); return } const d = e.data; if (!d || !(d.rfound || d.rfkey)) return; const t = (c.tabs() || []).find(x => x.fr && x.fr.contentWindow === e.source); if (!t) return;
    if (d.rfkey) { if (t === T()) key(d.rfkey) } else found(t, Object.assign({id: d.id}, d.rfound)) };
  addEventListener('message', onm);
  const key = a => { if (a == 'cf') { open(); return true } if (a == 'f3' || a == 'sf3') { const t = T(); if (t && !st(t).open) { open(); return true } run(a == 'sf3', true); return true } return false };
  /* Ctrl+F, F3, Shift+F3, Ctrl+G and Ctrl+Shift+G in Oracle's own window (its toolbar, its address box) */
  W.el.addEventListener('keydown', e => { if (!e.isTrusted) return; const cc = e.ctrlKey || e.metaKey; let a = '';
    if (cc && !e.shiftKey && !e.altKey && e.code == 'KeyF') a = 'cf'; else if (!cc && !e.altKey && e.code == 'F3') a = e.shiftKey ? 'sf3' : 'f3'; else if (cc && !e.altKey && e.code == 'KeyG') a = e.shiftKey ? 'sf3' : 'f3';
    if (a) { e.preventDefault(); e.stopPropagation(); key(a) } });
  const api = {open, close, key, sync, found, bar};
  return api;
}
/* ☰ › Find in page… (the window's menu is told its tab) */
{ const o = window.__oracleMenu; window.__oracleMenu = x => { const a = x && x.tab && ORA_FIND_TABS.get(x.tab), r = o ? o(x) : [];
  return a ? [['Find in page…', () => a.open()]].concat(r) : r } }
(function () { const s = document.createElement('style'); s.textContent = ".ofind{display:flex;gap:4px;align-items:center;padding:3px 6px;background:#f9fafb;border-bottom:1px solid var(--bd);font-size:12px}.ofind[hidden]{display:none}"
  + ".ofind .ofq{flex:0 1 260px;min-width:80px;border-radius:12px;padding:3px 10px}.ofind .ofq.ofno{background:#fee2e2}.ofind .ofn{min-width:72px;color:#4b5563}.ofind .ofn.ofno{color:#b91c1c}"
  + ".ofind button{padding:1px 8px;font-size:12px}.ofind .ofl{display:flex;gap:3px;align-items:center;margin-left:6px}.ofind .ofz{margin-left:auto}"
  + ".odark .ofind{background:#0f172a;border-color:#374151;color:#e5e7eb}.odark .ofind input.ofq{background:#1f2937;color:#f3f4f6;border-color:#374151}.odark .ofind .ofq.ofno{background:#7f1d1d}.odark .ofind button{background:#1f2937;color:#e5e7eb;border-color:#374151}.odark .ofind .ofn{color:#9ca3af}"
  + ".opriv .ofind{background:#2e1065!important;color:#ddd6fe}";
  (document.head || document.documentElement).appendChild(s) })();
