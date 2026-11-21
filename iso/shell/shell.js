/* Rudolf OS shell (an Isolated Web App, installed by Chromium's policy and started by the session).
   The desktop page runs unchanged in a full-screen Controlled Frame; Oracle's website tabs are Controlled Frames
   too, placed over Oracle's windows, so websites are real, first-party browser pages (signing in works).
   GPL-2.0-or-later.

   Messages (window.postMessage), desktop <-> shell. Only the desktop frame (http://127.0.0.1:8080) is listened to.
     shell -> desktop  {rudolfShell: 'hello', version, caps}       after every load of the desktop, every second until
                                                                   the desktop answers {rudolfShell: 'ready'}
                       {rudolfShell: 'newwindow', url}              the desktop asked for a new browser window
     desktop -> shell  {rudolfShell: 'ping', id}  ->  {rudolfShell: 'pong', id}
                       {rudolfShell: 'lock', id}  ->  {rudolfShell: 'locked', id, ok}   pointer capture for games: the
                       pointer cannot be captured inside the desktop's frame, so the shell captures it on the frame
                       and sends {rudolfShell: 'mm', dx, dy}, {'mb', b, down}, {'mw', dy}; {'lockchange', locked}
                       {rudolfShell: 'unlock'}
                       {rudolfShell: 'oa', op: 'title', t} | {rudolfShell: 'oa', op: 'close'}   (1.5.9) Oracle's own window
                       on the native desktop (opened at #app=browser&...): the window's title, and closing it with the app
   Tabs (see TABS below): desktop -> shell {rudolfShell: 'tab', op, tid, ...}; shell -> desktop {rudolfShell: 'tabev',
   tid, ev, ...}. */
'use strict';
const DESK_URL = 'http://127.0.0.1:8080/';
const DESK_ORIGIN = 'http://127.0.0.1:8080';
const VERSION = '1.5.9';   /* 1.5.1: controllers (the gamepad permission) for cloud games in real tabs; 1.5.9: Oracle's window on the native desktop */
/* 1.5.9 (docs/plan-1.5.9.md 12.3c): on the native desktop each of Oracle's windows is this shell, opened at
   #app=browser&...: the # goes on to the desktop page, which opens Oracle alone in it (one-app mode) with these real tabs */
const DESK_HASH = /^#app=[a-z0-9_-]{1,40}(&[A-Za-z0-9&=._~%+-]*)?$/.test(location.hash) ? location.hash : '';
const DESK_SRC = DESK_URL + DESK_HASH;
const CAPS = ['tabs', 'dark'];
const desk = document.getElementById('desk');
const starting = document.getElementById('starting');

const toDesk = m => { try { desk.contentWindow.postMessage(m, DESK_ORIGIN) } catch (e) {} };
/* notes for the desktop's shell log (SHELL_LOG there): what went wrong, for support */
const note = (...a) => toDesk({rudolfShell: 'log', m: a.map(x => typeof x == 'string' ? x : JSON.stringify(x)).join(' ').slice(0, 500)});


/* permissions the desktop may have; it asks the person itself where that matters (camera, screen sharing) */
const ALLOW = new Set(['media', 'fullscreen', 'pointerLock', 'download', 'geolocation']);
desk.addEventListener('permissionrequest', e => {
  try { ALLOW.has(e.permission) ? e.request.allow() : e.request.deny() } catch (x) {}
});
/* the desktop opens websites itself (Oracle, the browser window); anything else that asks for a window goes there */
desk.addEventListener('newwindow', e => {
  const url = e.targetUrl || '';
  try { e.window.discard() } catch (x) {}
  if (/^https?:/i.test(url)) toDesk({rudolfShell: 'newwindow', url});
});
let helloTimer = null, ready = false;
desk.addEventListener('loadstop', () => {
  starting.hidden = true; ready = false; desk.focus();
  clearInterval(helloTimer);
  const hello = () => { if (ready) clearInterval(helloTimer); else toDesk({rudolfShell: 'hello', version: VERSION, caps: CAPS}) };
  hello(); helloTimer = setInterval(hello, 1000);
});
/* the desktop page started again (reload, crash): its tabs are gone with it */
desk.addEventListener('loadcommit', e => { if (e.isTopLevel) tabsReset() });
/* rudolfd may not be listening yet right after start-up: try again */
desk.addEventListener('loadabort', e => {
  if (e.isTopLevel === false) return;
  starting.hidden = false; setTimeout(() => { desk.src = DESK_SRC }, 2000);
});
addEventListener('focus', () => { if (!document.activeElement || document.activeElement === document.body) desk.focus() });
/* 1.5.6: a key that reaches the shell itself (the keyboard is on neither the desktop nor a tab) never lets Chromium
   act: Ctrl+Shift+R reloaded the desktop from here (the host test, in this shell), and F5, Alt+F4, Ctrl+H, Shift+Esc
   and the others do what desktop/keys56.js lists. The key is stopped, and the keyboard goes back to the desktop */
const SHELL_KEYS = new Set(['CKeyW', 'CKeyT', 'CKeyN', 'CKeyR', 'CKeyS', 'CKeyO', 'CKeyP', 'CKeyH', 'CKeyJ', 'CKeyD', 'CKeyF', 'CKeyU', 'CKeyQ', 'CKeyG',
  'CKeyE', 'CKeyK', 'CKeyL', 'CF4', 'CF5', 'CTab', 'CPageUp', 'CPageDown', 'CSKeyW', 'CSKeyN', 'CSKeyT', 'CSKeyR', 'CSKeyQ', 'CSKeyB', 'CSKeyO', 'CSKeyA',
  'CSKeyM', 'CSKeyG', 'CSKeyJ', 'CSKeyI', 'CSKeyC', 'CSDelete', 'CSTab', 'AF4', 'AHome', 'AKeyD', 'AKeyF', 'AKeyE', 'ASKeyI', 'SEscape', 'SF5',
  'F1', 'F3', 'F5', 'F6', 'F7', 'F10', 'F11', 'F12', 'CF20', 'CF21', 'CF22', 'CF23', 'AF20']);
addEventListener('keydown', e => {
  if (!e.isTrusted || document.activeElement === desk || (document.activeElement && document.activeElement.tagName === 'CONTROLLEDFRAME')) return;
  const n = (e.ctrlKey ? 'C' : '') + (e.shiftKey ? 'S' : '') + (e.altKey ? 'A' : '') + e.code;
  if (/^C?S?A?Digit[1-9]$/.test(n) && e.ctrlKey || SHELL_KEYS.has(n)) { e.preventDefault(); desk.focus() }
}, true);

/* pointer capture for games (Quake): movement, buttons and the wheel go to the desktop while captured */
let captured = false;
document.addEventListener('pointerlockchange', () => { captured = document.pointerLockElement === desk; lockSum = [0, 0]; toDesk({rudolfShell: 'lockchange', locked: captured}) });
let lockDebug = 0, lockSum = [0, 0];   /* 'lock' with debug: the first moves go to the shell's notes, as the shell sees them */
document.addEventListener('mousemove', e => {
  if (lockDebug > 0) { lockDebug--; note('move', e.movementX, e.movementY, e.screenX, e.screenY, e.isTrusted, document.pointerLockElement === desk) }
  if (!captured) return;
  /* While the pointer is captured, Chromium moves the hidden pointer back to the middle of the screen now and then and
     reports that as one movement undoing everything since the last time; a game would jump back. That movement is
     left out: it exactly cancels the movement collected since the pointer was last put back (and is not small). */
  const dx = e.movementX, dy = e.movementY;
  if (dx === -lockSum[0] && dy === -lockSum[1] && Math.abs(dx) + Math.abs(dy) >= 24) { lockSum = [0, 0]; return }
  lockSum = [lockSum[0] + dx, lockSum[1] + dy];
  toDesk({rudolfShell: 'mm', dx, dy});
});
document.addEventListener('mousedown', e => { if (captured) { e.preventDefault(); toDesk({rudolfShell: 'mb', b: e.button, down: true}) } });
document.addEventListener('mouseup', e => { if (captured) toDesk({rudolfShell: 'mb', b: e.button, down: false}) });
document.addEventListener('wheel', e => { if (captured) toDesk({rudolfShell: 'mw', dy: e.deltaY}) }, {passive: true});


/* ===== TABS: Oracle's website tabs =====
   desktop -> shell {rudolfShell: 'tab', op, tid, ...}
     open {url, part, adopt?}   a new tab (part: whose tab, so each Rudolf OS account has its own cookies; a part
                                beginning "priv-" is a private window's, kept in memory only);
                                adopt: a tab the shell already made for a page's new window (tabev 'newwin')
     go {url} | back | forward | reload | stop | zoom {z} | focus | close
     busy {id}                  (1.5.9) may the tab sleep (game mode)? answered by tabev busy {id, m, f}
     notice {text, s, btn?, id?} (1.5.9) a notice inside the page for s seconds, over its full screen too (memory warning);
                                with btn, a button that answers tabev notice {id}
     place {r: [x, y, w, h] | null, vis: [[x, y, w, h], ...] | null, z}
                                where the tab is on the screen (null: not shown); vis: the parts of it not covered
                                by the desktop (menus, other windows), relative to r; null when nothing covers it
     dialog {did, ok, text}     the answer to a page's alert/confirm/prompt
     perm {pid, allow}          the answer to a page asking for the camera, the microphone or the location
     fw {rules}                 the firewall (all tabs): {on, mode: open|trusted|lock, block: [], trust: []}
     clear {part}               forget a partition's cookies, storage and cache
     blur                       keyboard focus back to the desktop
     block {cfg}                (1.6.1) ad and tracker blocking (all tabs): {on, b, tp, ex, off}; tabev adblock {n} the page's count
     unfs                       (1.5.8) leave full screen, whichever tab has it (Task Manager and the power dialog come first)
     ruffle {cfg}               Ruffle for Flash content in pages: {on, urls, config} (as the extension's settings)
     find {text, mc, back, again, id} | findstop | findsel {id}   (1.6.1) find in page; answered by tabev find {id, n, i} or {id, sel}
   shell -> desktop {rudolfShell: 'tabev', tid, ev, ...}
     nav {url, back, fwd}       the page's address changed (also inside the page); back/fwd: its own history
     load {loading}             title {title}       focus       close (the page closed itself)
     progress {p}               how far the page has loaded, 0..1 (1.6.0: Oracle's loading line)
     keys {k}                   a shortcut pressed inside the page: ct cw cn cl cr f5 back fwd z+ z- z0 ntab ptab af4 cpn (Ctrl+Shift+N)
                                (Ctrl+W, T, N, Q arrive as Ctrl+F20..F23, Alt+F4 as Alt+F20: see rudolf-display's desk_key; af4 closes the window)
     newwin {url, ntid, popup}  the page opened a window: a new tab ntid already holds it (opener kept)
     dialog {did, kind, text, def}   perm {pid, perm, url}   blocked {url, why, main}   gone {why}
     fs {on}                    the page is fullscreen: its frame fills the screen
     flash                      Ruffle was loaded for Flash content in the page
     download {url, started}    the page started a download (Chromium saves it; the desktop files it)
     busy {id, m, f}            (1.5.9) m: sound or video is playing; f: text is typed into the page and not sent
     notice {id}                (1.5.9) the button of a notice was pressed */
const TABS = new Map();
const NONCE = 'rudolf' + crypto.getRandomValues(new Uint32Array(2)).join('');
let FW = {on: false}, RUFFLE = {on: false, urls: [], config: {}};
const toTab = (tid, ev, o) => toDesk(Object.assign({rudolfShell: 'tabev', tid, ev}, o || {}));
const hostOf = u => { try { return new URL(u).hostname.toLowerCase() } catch (e) { return '' } };
const hostIn = (h, l) => (l || []).some(x => h == x || h.endsWith('.' + x));
/* the firewall's answer for an address: null (allowed) or why not */
function fwWhy(u) {
  if (!/^(https?|wss?):/i.test(u) || !FW.on) return null;
  const h = hostOf(u);
  if (FW.mode == 'lock') return 'All internet access is blocked by the firewall.';
  if (hostIn(h, FW.block)) return 'This site is on the blocked list.';
  if (FW.mode == 'trusted' && !hostIn(h, FW.trust)) return 'This site is not on the trusted list.';
  return null;
}
/* inside every page (isolated world): Oracle's shortcuts, and the title */
const PAGE_JS = `(()=>{const R=document.documentElement;if(window.__rudolfPJ||R&&R.hasAttribute('data-rudolf-pj'))return;window.__rudolfPJ=1;if(R)R.setAttribute('data-rudolf-pj','');
const N=${JSON.stringify(NONCE)},say=o=>{try{console.debug(N+JSON.stringify(o))}catch(e){}};
const SUB={F20:'w',F21:'t',F22:'n',F23:'q'};
const UI=new Set(['CKeyH','CKeyJ','CKeyD','CKeyU','CKeyS','CKeyO','CSKeyN','CSKeyT','CSKeyW','CSKeyO','CSKeyB','CSDelete','CSKeyA','CSKeyM','ASKeyI','SEscape','F1','F7','AHome']);
addEventListener('keydown',e=>{const c=e.ctrlKey||e.metaKey,k=SUB[e.code]&&e.ctrlKey?SUB[e.code]:e.key;let a='';
 if(c&&e.shiftKey&&!e.altKey&&e.code=='KeyN')a='cpn';else if(c&&!e.altKey&&!e.shiftKey&&/^[twlrn]$/i.test(k))a='c'+k.toLowerCase();else if(c&&k=='q')a='none';else if(k=='F5')a='f5';
 else if(e.altKey&&!c&&(k=='ArrowLeft'||k=='ArrowRight'))a=k=='ArrowLeft'?'back':'fwd';else if(e.altKey&&!c&&(k=='F4'||e.code=='F20'))a='af4';
 else if(c&&!e.altKey&&(k=='+'||k=='='||k=='-'||k=='0'))a='z'+(k=='='?'+':k);else if(c&&k=='Tab')a=e.shiftKey?'ptab':'ntab';
 else if((c&&(e.code=='KeyR'||e.code=='F5'))||(e.shiftKey&&!c&&e.code=='F5'))a='f5';
 if(a){e.preventDefault();e.stopImmediatePropagation();if(a!='none')say({k:a})}
 /* 1.5.6: Chromium's own pages, windows and bubbles (history, downloads, bookmarks, its task manager, view source,
    save and open dialogs, help) never open from a website: the page still sees the key */
 else if(e.isTrusted&&UI.has((c?'C':'')+(e.shiftKey?'S':'')+(e.altKey?'A':'')+e.code))e.preventDefault()},true);
/* 1.6.1 (Part 10): find in page: Ctrl+F, F3, Shift+F3, Ctrl+G and Ctrl+Shift+G open Oracle's find bar, before the page sees them, as Chrome does */
addEventListener('keydown',e=>{if(!e.isTrusted)return;const c=e.ctrlKey||e.metaKey;let a='';if(c&&!e.shiftKey&&!e.altKey&&e.code=='KeyF')a='cf';else if(!c&&!e.altKey&&e.code=='F3')a=e.shiftKey?'sf3':'f3';else if(c&&!e.altKey&&e.code=='KeyG')a=e.shiftKey?'sf3':'f3';
 if(a){e.preventDefault();e.stopImmediatePropagation();say({k:a})}},true);
addEventListener('click',e=>{if(!e.isTrusted)return;const a=e.target&&e.target.closest&&e.target.closest('a[href^="tel:" i]');if(!a)return;e.preventDefault();e.stopImmediatePropagation();say({tel:String(a.getAttribute('href')).slice(4,80)})},true);
let t=null;const tt=()=>{if(document.title!==t){t=document.title;say({title:t})}};
addEventListener('DOMContentLoaded',tt);addEventListener('load',tt);setInterval(tt,1500)})()`;
/* 1.6.1 (the owner, 2026-11-20): in every frame of a page, where a link (or a form's submit button) goes, while the mouse is
   over it or it has the keyboard's focus: Oracle's status line says "Goes To: <address>" (tabs26.js hover). Only the
   address is sent ('' when it leaves); javascript: links are not shown. */
const HOVER_JS = `(()=>{if(window.__rudolfHov)return;window.__rudolfHov=1;const N=${JSON.stringify(NONCE)},say=o=>{try{console.debug(N+JSON.stringify(o))}catch(e){}};let hv='';
const at=t=>{if(!t||!t.closest)return'';const a=t.closest('a[href],area[href]');if(a){const h=a.href;return typeof h=='string'?h:(h&&h.animVal?new URL(h.animVal,document.baseURI).href:'')}
 const b=t.closest('button,input[type=submit],input[type=image]');if(b&&b.form&&(b.type=='submit'||b.type=='image'))return b.hasAttribute('formaction')?b.formAction:b.form.action;return''};
const set=u=>{if(/^javascript:/i.test(u))u='';u=String(u).slice(0,2000);if(u!==hv){hv=u;say({hov:u})}};
addEventListener('mouseover',e=>set(at(e.target)),true);addEventListener('focusin',e=>set(at(e.target)),true);
addEventListener('mouseout',e=>{if(!e.relatedTarget)set('')},true);addEventListener('pagehide',()=>set(''))})()`;
/* in every frame of a page: Flash content (an <embed> or <object> with a .swf) asks for Ruffle */
const FLASH_SEL = 'embed[src*=".swf" i],embed[type*="shockwave" i],embed[type*="futuresplash" i],object[data*=".swf" i],object[type*="shockwave" i],object[classid*="d27cdb6e" i],param[name="movie" i][value*=".swf" i]';
const FLASH_JS = `(()=>{const N=${JSON.stringify(NONCE)},S=${JSON.stringify(FLASH_SEL)};let done=false,mo=null;
const go=()=>{if(done)return;done=true;if(mo)mo.disconnect();try{console.debug(N+JSON.stringify({flash:location.href}))}catch(e){}};
if(document.querySelector(S))return go();mo=new MutationObserver(()=>{if(document.querySelector(S))go()});
mo.observe(document.documentElement,{childList:true,subtree:true,attributes:true,attributeFilter:['src','data','type','value']});setTimeout(()=>mo&&mo.disconnect(),60000)})()`;
/* Ruffle, loaded into the page's own world (a script element) so it can replace the Flash content; the same
   copy of Ruffle and settings as the desktop's Flash Player (the desktop sends them) */
const ruffleJS = () => `(()=>{if(!document.querySelector(${JSON.stringify(FLASH_SEL)})||document.documentElement.dataset.rudolfRuffle)return;document.documentElement.dataset.rudolfRuffle='1';
const C=${JSON.stringify(Object.assign({unmuteOverlay: 'visible', warnOnUnsupportedContent: false}, RUFFLE.config))},U=${JSON.stringify(RUFFLE.urls)};
const cfg=document.createElement('script');cfg.textContent='window.RufflePlayer=window.RufflePlayer||{};window.RufflePlayer.config=Object.assign('+JSON.stringify(C)+',window.RufflePlayer.config||{});';(document.head||document.documentElement).appendChild(cfg);cfg.remove();
let i=0;const next=()=>{if(i>=U.length)return;const s=document.createElement('script');s.src=U[i++];s.onerror=()=>{s.remove();next()};(document.head||document.documentElement).appendChild(s)};next()})()`;

/* 1.6.1 (docs/plan-1.6.1.md Part 10): ad and tracker blocking (desktop/orablock61.js). The desktop sends the lists {on, b, tp, ex, off}
   (op block): whole domains blocked (b), blocked from other sites only (tp), never blocked (ex), and the sites the person turned it off
   for. A request of a tab's page (never the page itself) to such a domain is cancelled here, in the firewall's interceptor, and
   counted: tabev adblock {n}, the count of the page in the tab. Nothing leaves this computer: the lists are checked here. A private
   window's tabs ("priv-" partitions: private and Tor windows) always block. */
let BLOCK = {on: false, b: new Set(), tp: new Set(), ex: new Set(), off: new Set()};
const SLD2 = new Set(['co.uk', 'org.uk', 'ac.uk', 'gov.uk', 'com.au', 'net.au', 'org.au', 'co.nz', 'co.jp', 'ne.jp', 'or.jp', 'co.kr', 'com.br', 'com.mx', 'com.ar', 'com.cn', 'com.tw', 'com.tr', 'co.in', 'co.za', 'com.sg', 'com.hk', 'co.il']);
const siteOf = h => { const p = String(h || '').split('.'); if (p.length <= 2 || /^\d+$/.test(p[p.length - 1])) return h; const l2 = p.slice(-2).join('.'); return SLD2.has(l2) ? p.slice(-3).join('.') : l2 };
const inSet = (h, S) => { if (!h || !S.size) return false; for (let x = h; ;) { if (S.has(x)) return true; const i = x.indexOf('.'); if (i < 0) return false; x = x.slice(i + 1) } };
function blockWhy(t, u, ty) {
  if (/main/i.test(ty) || !/^(https?|wss?):/i.test(u)) return false;
  const priv = /^priv-/.test(String(t.part || '')); if (!BLOCK.on && !priv) return false;
  const top = hostOf(t.url); if (!priv && top && inSet(top, BLOCK.off)) return false;
  const h = hostOf(u); if (!h || inSet(h, BLOCK.ex)) return false;
  return inSet(h, BLOCK.b) || (inSet(h, BLOCK.tp) && siteOf(h) !== siteOf(top));
}
function tabBlock(t, e, u) {
  if (!blockWhy(t, u, String(e.request.type || e.request.resourceType || ''))) return false;
  e.preventDefault(); t.bn = (t.bn || 0) + 1;
  if (!t.bnT) t.bnT = setTimeout(() => { t.bnT = 0; toTab(t.tid, 'adblock', {n: t.bn}) }, 250);
  return true;
}
function tabEvents(t) {
  const f = t.f;
  const navState = async () => {
    let back = false, fwd = false;
    try { back = await f.canGoBack(); fwd = await f.canGoForward() } catch (e) {}
    toTab(t.tid, 'nav', {url: t.url, back, fwd});
  };
  /* (1.6.0) PAGE_JS in every page of the tab: the content scripts reached only the first tab of an account's partition,
     so in Oracle's other website tabs Ctrl+T, Ctrl+W and the other keys did nothing (and their titles were not told);
     PAGE_JS runs once in a page, whichever gives it first */
  f.addEventListener('loadcommit', e => { if (e.isTopLevel) { t.url = e.url; navState(); if (/^https?:/i.test(e.url || '')) f.executeScript({code: PAGE_JS}).catch(() => {}); f.executeScript({code: HOVER_JS, allFrames: true}).catch(() => {}) } });
  f.addEventListener('loadstart', e => {
    if (!e.isTopLevel) return;
    const why = fwWhy(e.url || '');
    if (why) { f.stop(); if (!(t.blk && t.blk.startsWith(e.url + ' ') && Date.now() - +t.blk.split(' ')[1] < 3000)) toTab(t.tid, 'blocked', {url: e.url, why, main: true}); if (/^https?:/i.test(t.url) && t.url !== e.url && !fwWhy(t.url)) f.back().catch(() => {}); return }
    toTab(t.tid, 'load', {loading: true});
  });
  f.addEventListener('loadstop', () => { toTab(t.tid, 'load', {loading: false}); navState(); if (/^https?:/i.test(t.url || '')) f.executeScript({code: HOVER_JS, allFrames: true}).catch(() => {}) });   // (1.6.1: frames added while it loaded)
  /* 1.6.0: how far the page has loaded (0..1), for Oracle's loading line (oload60.js) */
  f.addEventListener('loadcommit', e => { if (e.isTopLevel) { t.bn = 0; clearTimeout(t.bnT); t.bnT = 0; toTab(t.tid, 'adblock', {n: 0}) } });   // (1.6.1: a new page's count of blocked requests)
  f.addEventListener('loadprogress', e => { if (e.isTopLevel !== false) toTab(t.tid, 'progress', {p: Math.max(0, Math.min(1, +e.progress || 0))}) });
  f.addEventListener('loadabort', e => {
    if (e.isTopLevel === false || /ERR_ABORTED|ERR_BLOCKED_BY_CLIENT/.test(String(e.reason))) return;   // stopped by the person, or by the firewall (reported already)
    toTab(t.tid, 'load', {loading: false, error: String(e.reason || ''), url: e.url});
  });
  f.addEventListener('consolemessage', e => {
    const s = String(e.message || ''); if (!s.startsWith(NONCE)) return;
    let o; try { o = JSON.parse(s.slice(NONCE.length)) } catch (x) { return }
    if (typeof o.k == 'string') toTab(t.tid, 'keys', {k: o.k.slice(0, 8)});
    else if (typeof o.flash == 'string') { if (RUFFLE.on && RUFFLE.urls.length) { f.executeScript({code: ruffleJS(), allFrames: true}).catch(() => {}); toTab(t.tid, 'flash') } }
    else if (typeof o.title == 'string') toTab(t.tid, 'title', {title: o.title.slice(0, 300)});
    else if (typeof o.hov == 'string') toTab(t.tid, 'hover', {url: o.hov.slice(0, 2000)});   // (1.6.1: where a link goes, for the status line)
    else if (typeof o.notice == 'number') toTab(t.tid, 'notice', {id: o.notice});
    else if (typeof o.tel == 'string') toTab(t.tid, 'tel', {num: o.tel.slice(0, 80)});   // (1.5.9, plan 7.3: a phone number clicked)   // (1.5.9: its button was pressed, by a person: isTrusted)
  });
  f.addEventListener('focus', () => toTab(t.tid, 'focus'));
  f.addEventListener('close', () => toTab(t.tid, 'close'));
  /* (1.5.8: a page that ends while full screen, as the memory guard ended a game's on the Vivobook, leaves full screen:
     its frame stayed over everything with Chromium's sad-face icon, and Task Manager opened behind it) */
  f.addEventListener('exit', e => { if (FS_TAB === t || document.fullscreenElement === f) document.exitFullscreen().catch(() => {}); if (e.reason != 'normal') toTab(t.tid, 'gone', {why: String(e.reason || '')}) });
  f.addEventListener('unresponsive', () => toTab(t.tid, 'gone', {why: 'unresponsive'}));
  f.addEventListener('newwindow', e => {
    const url = e.targetUrl || '';
    if (!/^(https?:|about:blank)/i.test(url) || TABS.size >= 60) { try { e.window.discard() } catch (x) {} return }
    const nt = makeTab('n' + (++NEWN) + '-' + t.tid, t.part);
    try { e.window.attach(nt.f) } catch (x) { nt.f.src = url }
    nt.url = url;
    toTab(t.tid, 'newwin', {url, ntid: nt.tid, popup: e.windowOpenDisposition == 'new_popup' || e.windowOpenDisposition == 'new_window'});
  });
  f.addEventListener('dialog', e => {
    const did = ++DLGN; DLG.set(did, e.dialog);
    try { e.preventDefault() } catch (x) {}
    toTab(t.tid, 'dialog', {did, kind: e.messageType, text: String(e.messageText || '').slice(0, 2000), def: String(e.defaultPromptText || '')});
  });
  f.addEventListener('permissionrequest', e => {
    const p = e.permission;
    if (p == 'download') { const u = e.request && e.request.url || ''; try { e.request.allow() } catch (x) {} toTab(t.tid, 'download', {url: /^https?:/i.test(u) ? u : t.url, started: true}); return }
    if (p == 'fullscreen' || p == 'pointerLock') { try { e.request.allow() } catch (x) {} return }
    if (p == 'media' || p == 'geolocation') {
      try { e.preventDefault() } catch (x) {}
      const pid = ++PERMN; PERM.set(pid, e.request);
      toTab(t.tid, 'perm', {pid, perm: p, url: e.request && e.request.url || t.url});
      return;
    }
    try { e.request.deny() } catch (x) {}
  });
}
/* ---- downloads: Chromium saves the file (with the site's cookies, straight to disk, any size) in its download folder,
   Downloads/.incoming (set by policy); the desktop checks it there and files it (dl27.js). The shell only says that
   a download started, so Oracle can show it at once. */
let NEWN = 0, DLGN = 0, PERMN = 0;
const DLG = new Map(), PERM = new Map();

/* (1.2.2) dark pages, when Oracle asks for them: each page is turned dark as it loads (inverted, with pictures, videos
   and frames turned back), unless it is dark already. The desktop sends {op: 'dark', on}.
   (1.6.0) A layer that is turned back is turned back once: inside another one it has no filter of its own, and it has
   no backdrop-filter, whose backdrop (the page below, already inverted by <html>'s filter) its own filter would turn
   back as well. Either drew the video under it as a negative: YouTube's progress bar while the mouse is on it (the
   Vivobook, 2026-11-15). And whether the page is dark already is read without the style: its white on <html> made a
   dark page whose colour is on <html> (not on <body>) look light. (iso/qt/host/darkvideo60_t.sh) */
let DARK = false;
const DARK_CSS = 'html{filter:invert(.9) hue-rotate(180deg)!important;background:#fff!important}img,video,picture,canvas,iframe,embed,object,svg image,[style*="background-image"]{filter:invert(1) hue-rotate(180deg)!important;backdrop-filter:none!important}:is(img,video,picture,canvas,iframe,embed,object,svg image,[style*="background-image"]) :is(img,video,picture,canvas,iframe,embed,object,svg image,[style*="background-image"]){filter:none!important}';
const darkJS = on => `(()=>{const id='rudolf-dark';let s=document.getElementById(id);if(!${on}){if(s)s.remove();return}
 if(!s){s=document.createElement('style');s.id=id;s.textContent=${JSON.stringify(DARK_CSS)};(document.head||document.documentElement).appendChild(s)}
 const chk=()=>{try{const L=c=>{const m=/rgba?\\(([^)]+)\\)/.exec(c||'');if(!m)return null;const v=m[1].split(',').map(Number);if(v.length>3&&v[3]<.5)return null;return(.2126*v[0]+.7152*v[1]+.0722*v[2])/255};
  const sh=s.sheet;if(sh)sh.disabled=true;let l=document.body?L(getComputedStyle(document.body).backgroundColor):null;if(l==null)l=L(getComputedStyle(document.documentElement).backgroundColor);if(sh)sh.disabled=false;if(l!=null&&l<.4&&s.isConnected)s.remove()}catch(e){}};
 if(document.readyState=='loading')document.addEventListener('DOMContentLoaded',chk,{once:true});else chk()})()`;
const darken = t => { if (DARK) t.f.executeScript({code: darkJS(true)}).catch(() => {}) };
/* whose cookies and site storage: an account's are kept on the disk (persist:); a private window's (1.5.9, "priv-…")
   are in memory only, and a new private session gets a new name, so nothing of an earlier one is seen again */
const partName = p => { const n = 'web-' + String(p || 'x').replace(/[^a-z0-9_-]/gi, '').slice(0, 40); return /^priv-/.test(String(p || '')) ? n : 'persist:' + n };
function makeTab(tid, part) {
  const f = document.createElement('controlledframe');
  f.className = 'tab';
  f.setAttribute('partition', partName(part));
  f.setAttribute('allowtransparency', 'off');
  document.body.appendChild(f);
  const t = {tid, part, f, url: '', r: null};
  TABS.set(tid, t);
  f.addEventListener('loadcommit', e => { if (e.isTopLevel) darken(t) });
  f.addEventListener('contentload', () => darken(t));
  /* (1.6.0) registering the content scripts takes a moment: a tab opened with its address (a link's new tab, Oracle's
     oNewTab) waits for them before its first page (t.ready). (They reach only an account's first tab in Chromium 153: see
     tabEvents' loadcommit, which gives PAGE_JS to every page.) */
  t.ready = Promise.resolve();
  try { t.ready = f.addContentScripts([{name: 'oracle', urlPatterns: ['http://*/*', 'https://*/*'], js: {code: PAGE_JS}, runAt: 'document-start', allFrames: false},
    {name: 'flash', urlPatterns: ['http://*/*', 'https://*/*'], js: {code: FLASH_JS}, runAt: 'document-idle', allFrames: true},
    {name: 'hover', urlPatterns: ['http://*/*', 'https://*/*'], js: {code: HOVER_JS}, runAt: 'document-start', allFrames: true}]).catch(e => note('content scripts:', String(e))) } catch (e) { note('content scripts:', String(e)) }
  try {
    const i = f.request.createWebRequestInterceptor({urlPatterns: ['*://*/*'], blocking: true,
      resourceTypes: ['main-frame', 'sub-frame', 'stylesheet', 'script', 'image', 'font', 'object', 'xmlhttprequest', 'ping', 'csp-report', 'media', 'websocket', 'webbundle', 'other']});
    i.addEventListener('beforerequest', e => {
      const u = e.request && e.request.url || '', why = fwWhy(u);
      if (!why && tabBlock(t, e, u)) return;   // (1.6.1: an ad or a tracker)
      if (!why) return;
      e.preventDefault();
      const ty = String(e.request.type || e.request.resourceType || '');
      const main = !ty || /main/i.test(ty);
      if (main) t.blk = u + ' ' + Date.now();
      toTab(t.tid, 'blocked', {url: u, why, main, type: ty});
    });
  } catch (e) {}
  tabEvents(t);
  return t;
}
function tabsReset() { for (const t of TABS.values()) t.f.remove(); TABS.clear(); DLG.clear(); PERM.clear() }

/* where a tab is shown: a fixed box over Oracle's window, with the parts the desktop covers cut away */
function place(t, r, vis, zi) {
  const s = t.f.style;
  t.r = r; t.vis = vis;
  if (t.pfs) return;
  if (!r || r[2] < 1 || r[3] < 1 || (vis && !vis.length)) { s.visibility = 'hidden'; s.clipPath = ''; return }
  s.left = r[0] + 'px'; s.top = r[1] + 'px'; s.width = r[2] + 'px'; s.height = r[3] + 'px';
  s.zIndex = String(10 + (zi | 0));
  s.clipPath = vis ? `path('${vis.map(([x, y, w, h]) => `M${x} ${y}h${w}v${h}h${-w}z`).join('')}')` : '';
  s.visibility = 'visible';
}
/* ---- fullscreen (1.5.8). A page in a tab goes fullscreen the real way: Chromium asks the shell (permissionrequest,
   allowed above) and the tab's frame then fills the screen, with Chromium's own "press Esc" notice. Chromium asks only
   when the shell's permissions policy lets websites use fullscreen (the manifest: "fullscreen": ["self", "*"]); with
   "self" alone it said no by itself, at once, so a video went back to its size (YouTube on the Vivobook, 2026-11-11).
   (Before, pages had a stand-in put into them, which a site with Trusted Types, such as YouTube, refuses.)
   While a tab is fullscreen the shell does not move or cut it; afterwards it goes back over Oracle's window. */
let FS_TAB = null;
document.addEventListener('fullscreenchange', () => {
  const el = document.fullscreenElement;
  const t = el ? [...TABS.values()].find(x => x.f === el) || null : null;
  if (FS_TAB && FS_TAB !== t) { const o = FS_TAB; FS_TAB = null; o.pfs = false; toTab(o.tid, 'fs', {on: false}); place(o, o.r, o.vis) }
  if (t && FS_TAB !== t) { FS_TAB = t; t.pfs = true; t.f.style.clipPath = ''; t.f.style.visibility = 'visible'; t.f.focus(); toTab(t.tid, 'fs', {on: true}) }
});
const num = (v, d) => Number.isFinite(+v) ? +v : d;
const rect = r => Array.isArray(r) && r.length == 4 ? r.map(v => Math.round(num(v, 0))) : null;
/* (1.5.9) a notice shown inside a page for S seconds: a popover (the top layer: above the page's full-screen element) */
const noticeJS = (text, sec, btn, id) => `(()=>{try{const o=document.getElementById('rudolf-notice');o&&o.remove();const d=document.createElement('div');d.id='rudolf-notice';d.setAttribute('popover','manual');
d.style.cssText='position:fixed;inset:auto auto 24px 50%;transform:translateX(-50%);margin:0;max-width:560px;padding:12px 18px;border:0;border-radius:10px;background:rgba(20,20,24,.92);color:#fff;font:15px/1.4 system-ui,sans-serif;box-shadow:0 6px 24px rgba(0,0,0,.5);z-index:2147483647';
d.textContent=${JSON.stringify('⚠ ')}+${JSON.stringify(text)};
${btn ? `const b=document.createElement('button');b.textContent=${JSON.stringify(btn)};b.style.cssText='margin-left:12px;padding:4px 12px;border:0;border-radius:6px;background:#fff;color:#111;font:inherit;cursor:pointer';
b.onclick=e=>{if(!e.isTrusted)return;try{console.debug(${JSON.stringify(NONCE)}+JSON.stringify({notice:${Number(id) || 0}}))}catch(x){}d.remove()};d.appendChild(b);` : ''}
(document.body||document.documentElement).appendChild(d);try{d.showPopover()}catch(e){}setTimeout(()=>d.remove(),${sec * 1000})}catch(e){}})()`;
/* (1.5.9) what the page is doing that sleeping would lose: media playing, text typed and not sent */
const BUSY_JS = `JSON.stringify({m:[...document.querySelectorAll('video,audio')].some(x=>!x.paused&&!x.muted&&x.readyState>2),
f:[...document.querySelectorAll('input,textarea')].some(x=>!x.disabled&&/^(text|search|email|url|tel|number|password|textarea|)$/.test(x.type||'')&&x.value&&x.value!==x.defaultValue)||[...document.querySelectorAll('[contenteditable]')].some(x=>x.isContentEditable&&(x.textContent||'').trim().length>0&&x.contains(document.activeElement))})`;
/* 1.6.1 (docs/plan-1.6.1.md Part 10): find in page. Oracle's find bar (desktop/orafind61.js) asks {text, mc, back, again, id};
   the Controlled Frame's own find does it (Chromium's highlighting, every frame), its answer (and findupdate's) sent as tabev find
   {id, n, i}. Without it (or if it fails), FIND_FN in the page's isolated world: the same walker as Oracle's own pages
   (orafind61.js oraFindFn; oraclefind61_t.js checks that they are the same). findstop clears; findsel asks for the selected text. */
const FIND_FN = function (o) {
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
};
const findJs = (t, o) => t.f.executeScript({code: '(' + FIND_FN.toString() + ')(' + JSON.stringify(o) + ')'}).then(r => { const x = r && r[0]; if (x && typeof x == 'object') toTab(t.tid, 'find', Object.assign({id: o.id | 0}, x)) }).catch(() => {});
function tabFind(t, m) {
  const text = String(m.text || '').slice(0, 500), id = m.id | 0, o = {text, mc: !!m.mc, back: !!m.back, again: !!m.again, id};
  if (!text) return tabFindStop(t);
  if (t.fnat !== false && typeof t.f.find == 'function') {
    try {
      if (!t.fev) { t.fev = 1; t.f.addEventListener('findupdate', e => { const q = t.fq; if (q && t.fnat && e.searchText === q.text && !e.canceled) toTab(t.tid, 'find', {id: q.id, n: e.numberOfMatches | 0, i: e.activeMatchOrdinal | 0}) }) }
      /* (a new search, not the next match, when the case choice changed for the same words) */
      if (t.fq && t.fq.text === text && t.fq.mc !== o.mc) try { t.f.stopFinding('clear') } catch (e) {}
      t.fq = {text, mc: o.mc, id}; let got = false;
      const done = r => { if (got || !r || typeof r != 'object' || r.canceled) return; got = true; t.fnat = true; toTab(t.tid, 'find', {id, n: r.numberOfMatches | 0, i: r.activeMatchOrdinal | 0}) };
      const p = t.f.find(text, {backward: o.back, matchCase: o.mc}, done);
      if (p && typeof p.then == 'function') p.then(done, () => { if (!got) { t.fnat = false; findJs(t, o) } });
      /* (no answer at all: the walker) */
      setTimeout(() => { if (!got && t.fq && t.fq.id === id && t.fnat !== true) { t.fnat = false; findJs(t, Object.assign({}, o, {again: false})) } }, 3000);
      return;
    } catch (e) { t.fnat = false }
  }
  findJs(t, o);
}
function tabFindStop(t) { t.fq = null; if (t.fnat && typeof t.f.stopFinding == 'function') try { t.f.stopFinding('keep') } catch (e) {} t.f.executeScript({code: '(' + FIND_FN.toString() + ')({stop:1})'}).catch(() => {}) }
const TAB_OPS = {
  open: m => {
    let t = m.adopt && TABS.get(String(m.adopt));
    if (t) { TABS.delete(t.tid); t.tid = String(m.tid); TABS.set(t.tid, t); return }   // the page's new window
    if (TABS.has(String(m.tid)) || TABS.size >= 60) return;
    t = makeTab(String(m.tid), String(m.part || 'x'));
    if (/^https?:/i.test(m.url || '')) { const tt = t; tt.url = m.url; tt.ready.then(() => { if (TABS.get(tt.tid) === tt && tt.url === m.url && !tt.f.src) tt.f.src = m.url }) }
  },
  go: (m, t) => { if (/^https?:/i.test(m.url || '')) { t.url = m.url; t.f.src = m.url } },
  back: (m, t) => t.f.back().catch(() => {}),
  forward: (m, t) => t.f.forward().catch(() => {}),
  reload: (m, t) => t.f.reload(),
  stop: (m, t) => t.f.stop(),
  zoom: (m, t) => t.f.setZoom(Math.max(.25, Math.min(5, num(m.z, 1)))).catch(() => {}),
  focus: (m, t) => t.f.focus(),
  close: (m, t) => { t.f.remove(); TABS.delete(t.tid) },
  /* 1.5.9 (plan 9.2b): a notice inside the page, in the top layer, so it shows over the page's own full screen too */
  notice: (m, t) => { t.f.executeScript({code: noticeJS(String(m.text || '').slice(0, 300), Math.max(3, Math.min(30, num(m.s, 12))), m.btn ? String(m.btn).slice(0, 40) : '', m.id)}).catch(() => {}) },
  /* 1.5.9 (game mode, plan 9.2): may this tab sleep? Sound playing, or text typed into a form, says no */
  busy: async (m, t) => {
    let a = false, r = {};
    try { a = !!(await t.f.getAudioState()) } catch (e) {}
    try { const x = await t.f.executeScript({code: BUSY_JS}); r = JSON.parse((x && x[0]) || '{}') } catch (e) { r = {f: true} }
    toTab(t.tid, 'busy', {id: m.id, m: a || !!r.m, f: !!r.f});
  },
  find: (m, t) => tabFind(t, m),   // (1.6.1: find in page)
  findstop: (m, t) => tabFindStop(t),
  findsel: (m, t) => { t.f.executeScript({code: '(' + FIND_FN.toString() + ')({sel:1})'}).then(r => { const x = r && r[0]; if (x && typeof x.sel == 'string') toTab(t.tid, 'find', {id: m.id | 0, sel: x.sel.slice(0, 200)}) }).catch(() => {}) },
  place: (m, t) => place(t, rect(m.r), Array.isArray(m.vis) ? m.vis.map(rect).filter(Boolean) : null, m.z),
};
function onTab(m) {
  if (m.op == 'block') { const c = m.cfg || {}, set = l => new Set([].concat(l || []).map(x => String(x).toLowerCase()).filter(x => /^[a-z0-9.-]{1,253}$/.test(x)));
    BLOCK = {on: !!c.on, b: set(c.b), tp: set(c.tp), ex: set(c.ex), off: set(c.off)}; return }   // (1.6.1: ad and tracker blocking)
  if (m.op == 'fw') { const r = m.rules || {}; FW = {on: !!r.on, mode: String(r.mode || 'open'), block: [].concat(r.block || []).map(String), trust: [].concat(r.trust || []).map(String)}; return }
  if (m.op == 'dialog') { const d = DLG.get(m.did); DLG.delete(m.did); if (d) try { m.ok ? d.ok(String(m.text == null ? '' : m.text)) : d.cancel() } catch (e) {} return }
  /* (1.6.1: blurred first: after a click into a tab the shell still had the desktop as its focused element while the keyboard was the
     tab's, so focus() did nothing and what was typed next went on to the website, not to Oracle's find bar or address box) */
  if (m.op == 'blur') { desk.blur(); desk.focus(); return }
  if (m.op == 'unfs') { if (document.fullscreenElement) document.exitFullscreen().catch(() => {}); return }   // 1.5.8: Task Manager, the power dialog
  if (m.op == 'dark') { const on = !!m.on; if (on === DARK) return; DARK = on; for (const t of TABS.values()) t.f.executeScript({code: darkJS(on)}).catch(() => {}); return }
  if (m.op == 'ruffle') { const r = m.cfg || {}; RUFFLE = {on: !!r.on, urls: [].concat(r.urls || []).filter(u => /^https?:\/\//.test(u)).slice(0, 4), config: Object.assign({}, r.config || {})}; return }
  if (m.op == 'perm') { const p = PERM.get(m.pid); PERM.delete(m.pid); if (p) try { m.allow ? p.allow() : p.deny() } catch (e) {} return }
  if (m.op == 'clear') {
    const f = document.createElement('controlledframe'); f.setAttribute('partition', partName(m.part)); f.style.display = 'none'; document.body.appendChild(f);
    const done = ok => { f.remove(); toDesk({rudolfShell: 'cleared', id: m.id, ok}) };
    try { f.clearData({since: 0}, {appcache: true, cache: true, cookies: true, fileSystems: true, indexedDB: true, localStorage: true, persistentCookies: true, sessionCookies: true, webSQL: true}).then(() => done(true), () => done(false)) } catch (e) { done(false) }
    return;
  }
  const t = TABS.get(String(m.tid));
  if (m.op == 'open') return TAB_OPS.open(m);
  if (t && TAB_OPS[m.op]) try { TAB_OPS[m.op](m, t) } catch (e) {}
}

const handlers = {
  ready: () => { ready = true; clearInterval(helloTimer) },
  lock: async (m, reply) => { if (m.debug) lockDebug = 20; try { await desk.requestPointerLock(); reply({rudolfShell: 'locked', id: m.id, ok: true}) } catch (e) { reply({rudolfShell: 'locked', id: m.id, ok: false, error: String(e && e.message || e)}) } },
  unlock: () => { if (document.pointerLockElement) document.exitPointerLock() },
  ping: (m, reply) => reply({rudolfShell: 'pong', id: m.id}),
  tab: m => onTab(m),
  /* 1.5.9 (12.3c): Oracle's own window on the native desktop: its title is the page's, and it closes with the app */
  oa: m => { if (!DESK_HASH) return; if (m.op === 'title') document.title = String(m.t || '').slice(0, 200) || 'Rudolf OS'; else if (m.op === 'close') window.close() },
};
addEventListener('message', e => {
  if (e.origin !== DESK_ORIGIN || e.source !== desk.contentWindow) return;
  const m = e.data; if (!m || typeof m.rudolfShell !== 'string' || !Object.hasOwn(handlers, m.rudolfShell)) return;
  handlers[m.rudolfShell](m, r => toDesk(r));
});

desk.src = DESK_SRC;
