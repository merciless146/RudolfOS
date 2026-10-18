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
const VERSION = '1.2.0';
const CAPS = ['tabs'];
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
     place {r: [x, y, w, h] | null, vis: [[x, y, w, h], ...] | null, z}
                                where the tab is on the screen (null: not shown); vis: the parts of it not covered
                                by the desktop (menus, other windows), relative to r; null when nothing covers it
     dialog {did, ok, text}     the answer to a page's alert/confirm/prompt
     perm {pid, allow}          the answer to a page asking for the camera, the microphone or the location
     fw {rules}                 the firewall (all tabs): {on, mode: open|trusted|lock, block: [], trust: []}
     clear {part}               forget a partition's cookies, storage and cache
     blur                       keyboard focus back to the desktop
     ruffle {cfg}               Ruffle for Flash content in pages: {on, urls, config} (as the extension's settings)
   shell -> desktop {rudolfShell: 'tabev', tid, ev, ...}
     nav {url, back, fwd}       the page's address changed (also inside the page); back/fwd: its own history
     load {loading}             title {title}       focus       close (the page closed itself)
     keys {k}                   a shortcut pressed inside the page: ct cw cn cl cr f5 back fwd z+ z- z0 ntab ptab
                                (Ctrl+W, T, N, Q arrive as Ctrl+F20..F23: see rudolf-display's desk_key)
     newwin {url, ntid, popup}  the page opened a window: a new tab ntid already holds it (opener kept)
     dialog {did, kind, text, def}   perm {pid, perm, url}   blocked {url, why, main}   gone {why}
     fs {on}                    the page is "fullscreen" (the stand-in, see fsStandIn): its frame fills the screen
     flash                      Ruffle was loaded for Flash content in the page
     download {url, name, data | big | why}   a file the page downloads (data: a data: URL), see download() */
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
addEventListener('keydown',e=>{const c=e.ctrlKey||e.metaKey,k=SUB[e.code]&&e.ctrlKey?SUB[e.code]:e.key;let a='';
 if(c&&!e.altKey&&!e.shiftKey&&/^[twlrn]$/i.test(k))a='c'+k.toLowerCase();else if(c&&k=='q')a='none';else if(k=='F5')a='f5';
 else if(e.altKey&&!c&&(k=='ArrowLeft'||k=='ArrowRight'))a=k=='ArrowLeft'?'back':'fwd';
 else if(c&&!e.altKey&&(k=='+'||k=='='||k=='-'||k=='0'))a='z'+(k=='='?'+':k);else if(c&&k=='Tab')a=e.shiftKey?'ptab':'ntab';
 if(a){e.preventDefault();e.stopImmediatePropagation();if(a!='none')say({k:a})}},true);
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
    else if (typeof o.pfs == 'boolean') pageFullscreen(t, o.pfs, !!o.top, String(o.href || ''));
  });
  f.addEventListener('focus', () => toTab(t.tid, 'focus'));
  f.addEventListener('close', () => toTab(t.tid, 'close'));
  f.addEventListener('exit', e => { if (e.reason != 'normal') toTab(t.tid, 'gone', {why: String(e.reason || '')}) });
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
    if (p == 'download') { try { e.request.deny() } catch (x) {} download(t, e.request && e.request.url || ''); return }
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
/* ---- downloads go to Rudolf OS's own Downloads folder (C:\DOWNLOADS), through the desktop's checks (file types,
   antivirus, the size limit), not to Chromium's. The page fetches the file itself, so the site's cookies come with
   it (a file behind a sign-in); the shell collects it and hands it to the desktop. When the page cannot fetch it
   (another site's file without permission), the desktop tries on its own. */
const DL_MAX = 1.5 * 1048576;
let DLN = 0;
function download(t, url) {
  if (!/^(https?|blob|data):/i.test(url)) return;
  const id = ++DLN, send = o => toTab(t.tid, 'download', Object.assign({url: /^https?:/i.test(url) ? url : t.url}, o));
  const code = `(()=>{const W=window.__rudolfDl=window.__rudolfDl||{};W[${id}]={wait:1};(async()=>{try{
const r=await fetch(${JSON.stringify(url)},{credentials:'include'});if(!r.ok)throw new Error('HTTP '+r.status);
let n=+r.headers.get('content-length')||0,name='';const cd=r.headers.get('content-disposition')||'',m=/filename\\*=UTF-8''([^;]+)|filename="?([^";]+)"?/i.exec(cd);
if(m)try{name=decodeURIComponent(m[1]||m[2])}catch(e){name=m[1]||m[2]}
if(n>${DL_MAX}){W[${id}]={ok:false,big:true,name};return}const b=await r.blob();if(b.size>${DL_MAX}){W[${id}]={ok:false,big:true,name};return}
const d=await new Promise((res,rej)=>{const f=new FileReader();f.onload=()=>res(f.result);f.onerror=rej;f.readAsDataURL(b)});W[${id}]={ok:true,name,data:d}}
catch(e){W[${id}]={ok:false,why:String(e&&e.message||e)}}})();return 1})()`;
  t.f.executeScript({code}).then(() => {
    const until = Date.now() + 120000;
    const poll = () => t.f.executeScript({code: `(()=>{const W=window.__rudolfDl||{},r=W[${id}];if(r&&!r.wait)delete W[${id}];return r||null})()`}).then(r => {
      const v = r && r[0];
      if (v && !v.wait) return send(v.ok ? {name: String(v.name || ''), data: v.data} : {name: String(v.name || ''), big: !!v.big, why: String(v.why || '')});
      if (Date.now() < until) setTimeout(poll, 400); else send({why: 'timeout'});
    }, () => send({why: 'page'}));
    poll();
  }, () => send({why: 'page'}));
}
let NEWN = 0, DLGN = 0, PERMN = 0;
const DLG = new Map(), PERM = new Map();

function makeTab(tid, part) {
  const f = document.createElement('controlledframe');
  f.className = 'tab';
  f.setAttribute('partition', 'persist:web-' + String(part || 'x').replace(/[^a-z0-9_-]/gi, '').slice(0, 40));
  f.setAttribute('allowtransparency', 'off');
  document.body.appendChild(f);
  const t = {tid, part, f, url: '', r: null};
  TABS.set(tid, t);
  try { f.addContentScripts([{name: 'oracle', urlPatterns: ['http://*/*', 'https://*/*'], js: {code: PAGE_JS}, runAt: 'document-start', allFrames: false},
    {name: 'flash', urlPatterns: ['http://*/*', 'https://*/*'], js: {code: FLASH_JS}, runAt: 'document-idle', allFrames: true},
    {name: 'fullscreen', urlPatterns: ['http://*/*', 'https://*/*'], js: {code: FS_JS}, runAt: 'document-start', allFrames: true}]).catch(() => {}) } catch (e) {}
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
/* ---- fullscreen. Controlled Frame does not let a page go fullscreen (Chromium puts it straight back), so pages get
   a stand-in: requestFullscreen() makes the element fill the page (the class rudolf-fs), document.fullscreenElement
   and the fullscreenchange events behave as usual, Esc leaves; and the shell makes the page's frame fill the screen.
   The stand-in runs in the page's own world (a script element, so a page whose Content Security Policy forbids
   inline scripts keeps its normal, non-fullscreen view). It may only go fullscreen right after a click or a key
   press, as with the real thing. Same code as the desktop's (shell26.js). */
function fsStandIn(notify) {
  if (window.__rudolfFs) return; window.__rudolfFs = 1;
  const D = Document.prototype, E = Element.prototype; let cur = null;
  const css = document.createElement('style');
  css.textContent = '.rudolf-fs{position:fixed!important;inset:0!important;width:100vw!important;height:100vh!important;max-width:none!important;max-height:none!important;min-width:0!important;min-height:0!important;margin:0!important;padding:0!important;z-index:2147483647!important;transform:none!important;border:0!important;border-radius:0!important;box-sizing:border-box!important;background:#000}video.rudolf-fs{object-fit:contain!important}html.rudolf-fs-on,html.rudolf-fs-on body{overflow:hidden!important}';
  const fire = el => { for (const n of ['fullscreenchange', 'webkitfullscreenchange']) try { (el && el.isConnected ? el : document).dispatchEvent(new Event(n, {bubbles: true, composed: true})) } catch (e) {} };
  const enter = function () {
    const el = this;
    if (!(el instanceof Element)) return Promise.reject(new TypeError('Not an element'));
    if (cur !== el && !notify(true)) return Promise.reject(new TypeError('Permissions check failed'));
    if (!css.isConnected) (document.head || document.documentElement).appendChild(css);
    if (cur && cur !== el) cur.classList.remove('rudolf-fs');
    cur = el; el.classList.add('rudolf-fs'); document.documentElement.classList.add('rudolf-fs-on');
    setTimeout(() => fire(el), 0); return Promise.resolve();
  };
  const exit = function () {
    if (!cur) return Promise.reject(new TypeError('Document not active'));
    const el = cur; cur = null; el.classList.remove('rudolf-fs'); document.documentElement.classList.remove('rudolf-fs-on');
    notify(false); setTimeout(() => fire(el), 0); return Promise.resolve();
  };
  E.requestFullscreen = enter; E.webkitRequestFullscreen = E.webkitRequestFullScreen = function () { enter.call(this).catch(() => {}) };
  D.exitFullscreen = exit; D.webkitExitFullscreen = D.webkitCancelFullScreen = function () { exit.call(this).catch(() => {}) };
  for (const k of ['fullscreenElement', 'webkitFullscreenElement', 'webkitCurrentFullScreenElement']) Object.defineProperty(D, k, {configurable: true, get() { return cur }});
  for (const k of ['fullscreen', 'webkitIsFullScreen']) Object.defineProperty(D, k, {configurable: true, get() { return !!cur }});
  for (const k of ['fullscreenEnabled', 'webkitFullscreenEnabled']) Object.defineProperty(D, k, {configurable: true, get() { return true }});
  addEventListener('keydown', e => { if (cur && e.key == 'Escape') { e.preventDefault(); e.stopImmediatePropagation(); exit.call(document) } }, true);
  setInterval(() => { if (cur && !cur.isConnected) exit.call(document).catch(() => {}) }, 1000);
}
/* in every frame of a page, before the page's own scripts: the stand-in, and its link to the shell. A page cannot
   fake going fullscreen: the request is only passed on while the page has a click or key press to show for it. */
const FS_JS = `(()=>{const N=${JSON.stringify(NONCE)};
const s=document.createElement('script');s.textContent='('+${JSON.stringify(fsStandIn.toString())}+')(on=>{const e=new CustomEvent("rudolf-fs",{detail:on,cancelable:true});document.dispatchEvent(e);return !e.defaultPrevented})';
(document.head||document.documentElement).appendChild(s);s.remove();
document.addEventListener('rudolf-fs',e=>{const on=e.detail===true;if(on&&!(navigator.userActivation&&navigator.userActivation.isActive)){e.preventDefault();return}
 try{console.debug(N+JSON.stringify({pfs:on,top:window===top,href:location.href}))}catch(x){}},true)})()`;
/* the page's frame fills the screen while the page is "fullscreen"; a frame inside the page that went fullscreen
   (a video player from another site) is made to fill the page too */
function pageFullscreen(t, on, top, href) {
  if (!top) {
    let origin = ''; try { origin = new URL(href).origin } catch (e) {}
    t.f.executeScript({allFrames: true, code: `(()=>{const o=${JSON.stringify(origin)},on=${on};document.querySelectorAll('iframe').forEach(f=>{let fo='';try{fo=new URL(f.src,location.href).origin}catch(e){}if(fo!==o)return;
      if(on){if(f.dataset.rudolfFs===undefined)f.dataset.rudolfFs=f.getAttribute('style')||'';f.setAttribute('style','position:fixed!important;inset:0!important;width:100vw!important;height:100vh!important;max-width:none!important;max-height:none!important;z-index:2147483647!important;border:0!important;margin:0!important;transform:none!important')}
      else if(f.dataset.rudolfFs!==undefined){f.setAttribute('style',f.dataset.rudolfFs);delete f.dataset.rudolfFs}})})()`}).catch(() => {});
  }
  if (on === !!t.pfs) return;
  t.pfs = on; toTab(t.tid, 'fs', {on});
  const s = t.f.style;
  if (on) {
    Object.assign(s, {left: '0', top: '0', width: '100%', height: '100%', clipPath: '', zIndex: '2147483000', visibility: 'visible'});
    t.f.focus(); fsHint();
  } else place(t, t.r, t.vis);
}
/* "Press Esc to leave full screen", for a few seconds */
let fsHintT = 0;
function fsHint() {
  const h = document.getElementById('fshint'); h.hidden = false; clearTimeout(fsHintT);
  fsHintT = setTimeout(() => { h.hidden = true }, 3500);
}
const num = (v, d) => Number.isFinite(+v) ? +v : d;
const rect = r => Array.isArray(r) && r.length == 4 ? r.map(v => Math.round(num(v, 0))) : null;
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
  place: (m, t) => place(t, rect(m.r), Array.isArray(m.vis) ? m.vis.map(rect).filter(Boolean) : null, m.z),
};
function onTab(m) {
  if (m.op == 'fw') { const r = m.rules || {}; FW = {on: !!r.on, mode: String(r.mode || 'open'), block: [].concat(r.block || []).map(String), trust: [].concat(r.trust || []).map(String)}; return }
  if (m.op == 'dialog') { const d = DLG.get(m.did); DLG.delete(m.did); if (d) try { m.ok ? d.ok(String(m.text == null ? '' : m.text)) : d.cancel() } catch (e) {} return }
  if (m.op == 'blur') { desk.focus(); return }
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
