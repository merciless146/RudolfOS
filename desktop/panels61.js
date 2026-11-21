/* ===== 1.6.1 (docs/plan-1.6.1.md Part 2): the mobile build's three panels: the groundwork only =====
   The phone's home screen will be the three workspaces as three panels, swiped left and right as on Android (2.0's
   phone build: docs/plan-2.0.md, "The phone build"). What is here, on the mobile build only:
   - the flag: rudolfd's RUDOLFD.mobile, which it sets when the system layer says it is the mobile build
     (/etc/rudolf/build-kind is "mobile"; no build writes it yet). Without it nothing here runs: the desktop builds (and
     a plain browser) look and work as before;
   - the layout model: a panel is a workspace shown full screen (the tablet layout of 1.5.8, each window filling the
     screen, kept on), panels 1 to NWS in a row, the shown one WS; pnSwipe decides from a finger's way whether it was a
     swipe to the next panel (to the left) or the one before (to the right), as the native shell's own does (pn_swipe,
     rudolf-native.c);
   - switching: a horizontal swipe on the desktop or on a window's title bar, and the keyboard's workspace keys
     (Ctrl+Alt+arrows) as before; the panel slides in from the side it comes from. A finger, a pen or the pointer alike:
     rudolf-display gives a touch screen to X as the pointer (XTest), so on Rudolf OS a finger's swipe reaches the page as
     the mouse's (a browser with touch gives it as touch).
   Not here (2.0's): the status bar, the notification shade, the app grid, the panels' own home screen, the lock screen. */
const PANELS=(()=>{try{return !ONEAPP&&!!(window.RUDOLFD&&RUDOLFD.mobile)}catch(e){return false}})();
/* the model (pure: also used by the tests) */
const PN_EDGE=24;
function pnSwipe(dx,dy,ms,w){const far=Math.max(48,(w||0)*0.15);if(!(ms>=0)||ms>900||Math.abs(dx)<far||Math.abs(dx)<Math.abs(dy)*1.5)return 0;return dx<0?1:-1}
function pnTarget(cur,d,n){const t=cur+d;return t<1||t>n?cur:t}
(function(){if(!PANELS)return;
 const H=document.documentElement;H.classList.add('panels');
 /* each panel full screen: the tablet layout, whatever Settings › Display › Tablet mode says */
 if(typeof tabletOn=='function'){tabletOn=function(){return true};try{tabletApply()}catch(e){}}
 const st=document.createElement('style');st.textContent='@keyframes pnInL{from{transform:translateX(-36%);opacity:.3}to{transform:none;opacity:1}}@keyframes pnInR{from{transform:translateX(36%);opacity:.3}to{transform:none;opacity:1}}html.panels #desk.pnl>*{animation:pnInL .22s ease-out}html.panels #desk.pnr>*{animation:pnInR .22s ease-out}html.panels #desk{touch-action:pan-y}html.panels #desk .win>.tb{touch-action:pan-y}';document.head.appendChild(st);
 /* the panel slides in from the side it comes from, whatever switched it (a swipe, the keys, the taskbar's numbers) */
 const D=document.getElementById('desk');
 if(typeof wsGo=='function'){const g0=wsGo;wsGo=function(n){const was=WS;const r=g0.apply(this,arguments);if(D&&WS!=was){D.classList.remove('pnl','pnr');void D.offsetWidth;D.classList.add(WS>was?'pnr':'pnl');clearTimeout(D.__pnT);D.__pnT=setTimeout(()=>D.classList.remove('pnl','pnr'),300)}return r}}
 /* a finger's swipe on the desktop or a title bar (not from the screen's right or bottom edge: those are the tablet
    layout's own, for the notifications and Start) */
 let s=null;
 const where=t=>{if(!t||!t.closest)return false;if(t===D)return true;if(t.closest('button,input,select,textarea,a'))return false;return !!t.closest('.win>.tb')||(!!t.closest('.ic')&&!!t.closest('#desk'))};
 addEventListener('pointerdown',e=>{if(!e.isPrimary||e.button>0){s=null;return}
  if(e.clientX>=innerWidth-PN_EDGE||e.clientY>=innerHeight-PN_EDGE||!where(e.target)){s=null;return}
  s={x:e.clientX,y:e.clientY,t:performance.now(),id:e.pointerId}},{capture:true,passive:true});
 addEventListener('pointerup',e=>{if(!s||e.pointerId!=s.id)return;const d=pnSwipe(e.clientX-s.x,e.clientY-s.y,performance.now()-s.t,innerWidth);s=null;
  if(d){const n=pnTarget(WS,d,NWS);if(n!=WS)wsGo(n)}},{capture:true,passive:true});
 addEventListener('pointercancel',()=>{s=null},{capture:true,passive:true})})();
