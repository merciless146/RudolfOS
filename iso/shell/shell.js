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
   Tabs (see TABS below): desktop -> shell {rudolfShell: 'tab', op, tid, ...}; shell -> desktop {rudolfShell: 'tabev',
   tid, ev, ...}. */
'use strict';
const DESK_URL = 'http://127.0.0.1:8080/';
const DESK_ORIGIN = 'http://127.0.0.1:8080';
const VERSION = '1.5.1';   /* 1.5.1: controllers (the gamepad permission) for cloud games in real tabs */
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
  starting.hidden = false; setTimeout(() => { desk.src = DESK_URL }, 2000);
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
     open {url, part, adopt?}   a new tab (part: whose tab, so each Rudolf OS account has its own cookies);
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
     unfs                       (1.5.8) leave full screen, whichever tab has it (Task Manager and the power dialog come first)
     ruffle {cfg}               Ruffle for Flash content in pages: {on, urls, config} (as the extension's settings)
   shell -> desktop {rudolfShell: 'tabev', tid, ev, ...}
     nav {url, back, fwd}       the page's address changed (also inside the page); back/fwd: its own history
     load {loading}             title {title}       focus       close (the page closed itself)
     keys {k}                   a shortcut pressed inside the page: ct cw cn cl cr f5 back fwd z+ z- z0 ntab ptab af4
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
const PAGE_JS = `(()=>{const N=${JSON.stringify(NONCE)},say=o=>{try{console.debug(N+JSON.stringify(o))}catch(e){}};
const SUB={F20:'w',F21:'t',F22:'n',F23:'q'};
const UI=new Set(['CKeyH','CKeyJ','CKeyD','CKeyU','CKeyS','CKeyO','CSKeyN','CSKeyT','CSKeyW','CSKeyO','CSKeyB','CSDelete','CSKeyA','CSKeyM','ASKeyI','SEscape','F1','F7','AHome']);
addEventListener('keydown',e=>{const c=e.ctrlKey||e.metaKey,k=SUB[e.code]&&e.ctrlKey?SUB[e.code]:e.key;let a='';
 if(c&&!e.altKey&&!e.shiftKey&&/^[twlrn]$/i.test(k))a='c'+k.toLowerCase();else if(c&&k=='q')a='none';else if(k=='F5')a='f5';
 else if(e.altKey&&!c&&(k=='ArrowLeft'||k=='ArrowRight'))a=k=='ArrowLeft'?'back':'fwd';else if(e.altKey&&!c&&(k=='F4'||e.code=='F20'))a='af4';
 else if(c&&!e.altKey&&(k=='+'||k=='='||k=='-'||k=='0'))a='z'+(k=='='?'+':k);else if(c&&k=='Tab')a=e.shiftKey?'ptab':'ntab';
 else if((c&&(e.code=='KeyR'||e.code=='F5'))||(e.shiftKey&&!c&&e.code=='F5'))a='f5';
 if(a){e.preventDefault();e.stopImmediatePropagation();if(a!='none')say({k:a})}
 /* 1.5.6: Chromium's own pages, windows and bubbles (history, downloads, bookmarks, its task manager, view source,
    save and open dialogs, help) never open from a website: the page still sees the key */
 else if(e.isTrusted&&UI.has((c?'C':'')+(e.shiftKey?'S':'')+(e.altKey?'A':'')+e.code))e.preventDefault()},true);
addEventListener('click',e=>{if(!e.isTrusted)return;const a=e.target&&e.target.closest&&e.target.closest('a[href^="tel:" i]');if(!a)return;e.preventDefault();e.stopImmediatePropagation();say({tel:String(a.getAttribute('href')).slice(4,80)})},true);
let t=null;const tt=()=>{if(document.title!==t){t=document.title;say({title:t})}};
addEventListener('DOMContentLoaded',tt);addEventListener('load',tt);setInterval(tt,1500)})()`;
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

function tabEvents(t) {
  const f = t.f;
  const navState = async () => {
    let back = false, fwd = false;
    try { back = await f.canGoBack(); fwd = await f.canGoForward() } catch (e) {}
    toTab(t.tid, 'nav', {url: t.url, back, fwd});
  };
  f.addEventListener('loadcommit', e => { if (e.isTopLevel) { t.url = e.url; navState() } });
  f.addEventListener('loadstart', e => {
    if (!e.isTopLevel) return;
    const why = fwWhy(e.url || '');
    if (why) { f.stop(); if (!(t.blk && t.blk.startsWith(e.url + ' ') && Date.now() - +t.blk.split(' ')[1] < 3000)) toTab(t.tid, 'blocked', {url: e.url, why, main: true}); if (/^https?:/i.test(t.url) && t.url !== e.url && !fwWhy(t.url)) f.back().catch(() => {}); return }
    toTab(t.tid, 'load', {loading: true});
  });
  f.addEventListener('loadstop', () => { toTab(t.tid, 'load', {loading: false}); navState() });
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
   and frames turned back), unless it is dark already. The desktop sends {op: 'dark', on}. */
let DARK = false;
const DARK_CSS = 'html{filter:invert(.9) hue-rotate(180deg)!important;background:#fff!important}img,video,picture,canvas,iframe,embed,object,svg image,[style*="background-image"]{filter:invert(1) hue-rotate(180deg)!important}';
const darkJS = on => `(()=>{const id='rudolf-dark';let s=document.getElementById(id);if(!${on}){if(s)s.remove();return}
 if(!s){s=document.createElement('style');s.id=id;s.textContent=${JSON.stringify(DARK_CSS)};(document.head||document.documentElement).appendChild(s)}
 const chk=()=>{try{const L=c=>{const m=/rgba?\\(([^)]+)\\)/.exec(c||'');if(!m)return null;const v=m[1].split(',').map(Number);if(v.length>3&&v[3]<.5)return null;return(.2126*v[0]+.7152*v[1]+.0722*v[2])/255};
  let l=document.body?L(getComputedStyle(document.body).backgroundColor):null;if(l==null)l=L(getComputedStyle(document.documentElement).backgroundColor);if(l!=null&&l<.4&&s.isConnected)s.remove()}catch(e){}};
 if(document.readyState=='loading')document.addEventListener('DOMContentLoaded',chk,{once:true});else chk()})()`;
const darken = t => { if (DARK) t.f.executeScript({code: darkJS(true)}).catch(() => {}) };
function makeTab(tid, part) {
  const f = document.createElement('controlledframe');
  f.className = 'tab';
  f.setAttribute('partition', 'persist:web-' + String(part || 'x').replace(/[^a-z0-9_-]/gi, '').slice(0, 40));
  f.setAttribute('allowtransparency', 'off');
  document.body.appendChild(f);
  const t = {tid, part, f, url: '', r: null};
  TABS.set(tid, t);
  f.addEventListener('loadcommit', e => { if (e.isTopLevel) darken(t) });
  f.addEventListener('contentload', () => darken(t));
  try { f.addContentScripts([{name: 'oracle', urlPatterns: ['http://*/*', 'https://*/*'], js: {code: PAGE_JS}, runAt: 'document-start', allFrames: false},
    {name: 'flash', urlPatterns: ['http://*/*', 'https://*/*'], js: {code: FLASH_JS}, runAt: 'document-idle', allFrames: true}]).catch(e => note('content scripts:', String(e))) } catch (e) { note('content scripts:', String(e)) }
  try {
    const i = f.request.createWebRequestInterceptor({urlPatterns: ['*://*/*'], blocking: true,
      resourceTypes: ['main-frame', 'sub-frame', 'stylesheet', 'script', 'image', 'font', 'object', 'xmlhttprequest', 'ping', 'csp-report', 'media', 'websocket', 'webbundle', 'other']});
    i.addEventListener('beforerequest', e => {
      const u = e.request && e.request.url || '', why = fwWhy(u);
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
const TAB_OPS = {
  open: m => {
    let t = m.adopt && TABS.get(String(m.adopt));
    if (t) { TABS.delete(t.tid); t.tid = String(m.tid); TABS.set(t.tid, t); return }   // the page's new window
    if (TABS.has(String(m.tid)) || TABS.size >= 60) return;
    t = makeTab(String(m.tid), String(m.part || 'x'));
    if (/^https?:/i.test(m.url || '')) { t.url = m.url; t.f.src = m.url }
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
  place: (m, t) => place(t, rect(m.r), Array.isArray(m.vis) ? m.vis.map(rect).filter(Boolean) : null, m.z),
};
function onTab(m) {
  if (m.op == 'fw') { const r = m.rules || {}; FW = {on: !!r.on, mode: String(r.mode || 'open'), block: [].concat(r.block || []).map(String), trust: [].concat(r.trust || []).map(String)}; return }
  if (m.op == 'dialog') { const d = DLG.get(m.did); DLG.delete(m.did); if (d) try { m.ok ? d.ok(String(m.text == null ? '' : m.text)) : d.cancel() } catch (e) {} return }
  if (m.op == 'blur') { desk.focus(); return }
  if (m.op == 'unfs') { if (document.fullscreenElement) document.exitFullscreen().catch(() => {}); return }   // 1.5.8: Task Manager, the power dialog
  if (m.op == 'dark') { const on = !!m.on; if (on === DARK) return; DARK = on; for (const t of TABS.values()) t.f.executeScript({code: darkJS(on)}).catch(() => {}); return }
  if (m.op == 'ruffle') { const r = m.cfg || {}; RUFFLE = {on: !!r.on, urls: [].concat(r.urls || []).filter(u => /^https?:\/\//.test(u)).slice(0, 4), config: Object.assign({}, r.config || {})}; return }
  if (m.op == 'perm') { const p = PERM.get(m.pid); PERM.delete(m.pid); if (p) try { m.allow ? p.allow() : p.deny() } catch (e) {} return }
  if (m.op == 'clear') {
    const f = document.createElement('controlledframe'); f.setAttribute('partition', 'persist:web-' + String(m.part || 'x').replace(/[^a-z0-9_-]/gi, '').slice(0, 40)); f.style.display = 'none'; document.body.appendChild(f);
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
};
addEventListener('message', e => {
  if (e.origin !== DESK_ORIGIN || e.source !== desk.contentWindow) return;
  const m = e.data; if (!m || typeof m.rudolfShell !== 'string' || !Object.hasOwn(handlers, m.rudolfShell)) return;
  handlers[m.rudolfShell](m, r => toDesk(r));
});

desk.src = DESK_URL;
