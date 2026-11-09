/* ===== 1.5.6: Chromium's own shortcuts never act on the desktop =====
   The desktop is a page in Chromium (kiosk), and Chromium acts on its shortcuts unless the page stops it. Alt+F4
   closed the desktop's window on the Vivobook (back to the sign-in screen); tried one by one in Chromium's kiosk mode
   (Xvfb, a real X keyboard), many more did damage:
   - closed it: Ctrl+W, Ctrl+Shift+W, Ctrl+F4, Alt+F4 (rudolf-display also hands these over as F20, see shell26.js);
   - reloaded it (every window lost): F5, Ctrl+R, Ctrl+Shift+R, Ctrl+F5, Shift+F5;
   - put a browser page or window over it: Ctrl+T, Ctrl+N, Ctrl+Shift+N, Ctrl+H (history), Ctrl+J (downloads),
     Ctrl+Shift+O (bookmarks), Ctrl+Shift+Delete, F1 (help), Alt+Home, Ctrl+S and Ctrl+O (file windows),
     Shift+Esc (Chromium's task manager), F12, Ctrl+Shift+I, J and C (developer tools);
   - opened Chromium's bubbles and bars over it: Ctrl+P, Ctrl+F, Ctrl+D, Ctrl+Shift+A, Ctrl+Shift+M, F7.
   Every one of them is stopped from the page (preventDefault, which Chromium honours for all of them; checked the
   same way), and so are Chromium's other tab, address-bar and menu keys (Ctrl+Tab, Ctrl+1…9, Ctrl+L, Alt+D, F6,
   F10, F11…), which do nothing useful in the desktop. The desktop's own handlers still see the keys (Notepad's
   Ctrl+S, Oracle's Ctrl+T, Find's Ctrl+F): only Chromium's action is stopped. Chromium's developer tools (F12,
   Ctrl+Shift+I, J, C) stay with Settings › Apps › "Show developer tools in the Start menu". Same-origin frames
   (Oracle's pages without the shell, the apps in frames) get the same guard. */
window.__keys56=1;
const KEYS56=(()=>{const s=new Set();
 'KeyW KeyT KeyN KeyR KeyS KeyO KeyP KeyH KeyJ KeyD KeyF KeyU KeyQ KeyG KeyE KeyK KeyL F4 F5 Tab PageUp PageDown Digit1 Digit2 Digit3 Digit4 Digit5 Digit6 Digit7 Digit8 Digit9'.split(' ').forEach(k=>s.add('C'+k));
 'KeyW KeyN KeyT KeyR KeyQ KeyB KeyO KeyA KeyM KeyG KeyJ KeyI KeyC Delete Tab'.split(' ').forEach(k=>s.add('CS'+k));
 ['F4','Home','KeyD','KeyF','KeyE'].forEach(k=>s.add('A'+k));s.add('ASKeyI');s.add('SEscape');s.add('SF5');
 ['F1','F3','F5','F6','F7','F10','F11','F12'].forEach(k=>s.add(k));
 return s})();
const KEYS56DEV=new Set(['F12','CSKeyI','CSKeyJ','CSKeyC']);
function keys56Name(e){return(e.ctrlKey?'C':'')+(e.shiftKey?'S':'')+(e.altKey?'A':'')+e.code}
function keys56Guard(e){if(!e.isTrusted||e.metaKey)return;const n=keys56Name(e);if(!KEYS56.has(n))return;
 if(KEYS56DEV.has(n)&&typeof devToolsShown=='function'&&devToolsShown())return;
 e.preventDefault()}
/* on the document, in the capture phase: a frame's first window (about:blank) is kept for its page, and a listener put
   on that window did not run once the page loaded (checked in Chromium); each document is guarded once */
function keys56Attach(w){try{const d=w&&w.document;if(!d||d.__keys56g)return;d.__keys56g=1;d.addEventListener('keydown',keys56Guard,true)}catch(e){}}
keys56Attach(window);window.addEventListener('keydown',keys56Guard,true);   /* the desktop itself: first of all */
/* same-origin frames: when they load (cross-origin ones cannot be reached, and a frame's page cannot reload the desktop) */
{const hook=f=>{const a=()=>{try{keys56Attach(f.contentWindow)}catch(e){}};a();f.addEventListener('load',a)};
 document.querySelectorAll('iframe').forEach(hook);
 new MutationObserver(ms=>ms.forEach(m=>m.addedNodes.forEach(n=>{if(n.nodeType!=1)return;if(n.tagName=='IFRAME')hook(n);else if(n.querySelectorAll)n.querySelectorAll('iframe').forEach(hook)}))).observe(document.documentElement,{childList:true,subtree:true})}
