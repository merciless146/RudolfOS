/* 1.5.7: Task Manager opened by its shortcut (Ctrl+Alt+Del, or Ctrl+Shift+Esc) comes up above every window and is
   kept there ("Always on top", focused, restored if it was minimized), and the desktop's processes get the processor
   and the disk first while it is open, so an app or a game that is using up the computer can still be found and
   ended. rudolf-display already did that at the key; closing Task Manager gives the desktop its usual share again.
   The right-click menu and the Start menu open it as before. GPL-2.0-or-later. */
function tmPriority(on){if(!RD)return Promise.resolve(null);return rd('desktop/priority?on='+(on?1:0),{method:'POST'}).catch(()=>null)}
/* 1.5.8: nothing full screen may hide Task Manager or the power dialog: a game's tab ended by the memory guard while
   full screen stayed there (Chromium's sad-face icon), and Ctrl+Alt+Del's Task Manager opened behind it (the Vivobook,
   2026-11-12; only the power button got out). The shell leaves full screen for a tab ('unfs'); the page for itself. */
function fsLeave(){try{if(typeof rtSend=='function')rtSend('unfs','')}catch(e){}try{if(document.fullscreenElement)document.exitFullscreen().catch(()=>{})}catch(e){}}
{const _sd=shutdownDlg;shutdownDlg=function(){fsLeave();return _sd.apply(this,arguments)}}
function tmUrgent(){
 fsLeave();
 const W=launch('taskmgr');if(!W||!W.el)return W;
 if(W.min||W.el.style.display=='none')W.focus();
 W.el.classList.add('ontop','tmurgent');W.focus();
 tmPriority(1);
 if(!W.__tmTop){W.__tmTop=1;const c=W.close;W.close=(...a)=>{W.el.classList.remove('ontop','tmurgent');tmPriority(0);return c.apply(W,a)}}
 return W}
/* the shortcut (in a web browser, and wherever rudolf-display does not catch it) */
(function(){const s=SHORT.find(x=>x.id=='taskmgr');if(s)s.fn=tmUrgent})();
/* from rudolf-display: the shell's own "taskmgr" handler is replaced, so Task Manager is not opened twice */
window.__tmEvent=tmUrgent;
/* left on (the page was reloaded with Task Manager open): back to the usual share */
if(RD)setTimeout(()=>{if(window.__tmW&&wins.includes(__tmW))return;rd('desktop/priority').then(j=>{if(j&&j.on)tmPriority(0)}).catch(()=>{})},4000);
/* a window opened later (an app starting, an error) stays under the windows that are always on top: before, it came
   up over them until they were clicked */
{const _w57=win;win=function(...a){const W=_w57.apply(this,a);const up=()=>{try{wins.forEach(o=>{if(o!==W&&o.el.classList.contains('ontop'))o.el.style.zIndex=++z})}catch(e){}};up();setTimeout(up,0);return W}}
