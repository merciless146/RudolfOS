/* ===== Rudolf OS 1.6.2: the taskbar at the left or the right, its pop-ups and menus beside it =====
   The owner, 2026-11-22: "Some of the taskbar menus and visuals when set to the left and right side options need
   correction." With the taskbar at the left or the right (Display Properties › Taskbar):
   - Quick settings, the notifications and the calendar (.flyout: #qsp, #ncp, #calp) opened in the screen's bottom-right
     corner, as for a taskbar at the bottom: over a taskbar at the right, across the screen from one at the left. They now
     open beside their button as the network's and the sound's pop-ups do (osd31.js flyPlace): right of a taskbar at the
     left, left of one at the right, level with the button's top, kept on the screen.
   - A right-click menu (cmenu: the taskbar's own, its buttons', the desktop's) opened at the pointer and could lie over the
     taskbar: it is moved off it, beside it.
   - The desktop (its icons) is placed beside the taskbar by --bw, which the clock set once a minute: when the taskbar grew
     (the pinned apps coming in, a wider clock), the icons stayed under it until the next minute. --bw now follows the
     taskbar's width as it changes.
   rudolf-native.c places its own the same way (fl_place_over for these pop-ups; cm_open off the taskbar). */
function tbSide(){const c=document.body.classList;return c.contains('left')?1:c.contains('right')?2:0}
function tbSideBw(){const b=document.getElementById('bar');return b?b.getBoundingClientRect().width:0}
/* a pop-up beside its taskbar button, as flyPlace does (without its own closing: these close as they always have) */
function tbSidePlace(p,B){if(!p||!B||!p.isConnected)return;const s=tbSide();if(!s){p.style.left=p.style.top='';return}
 const r=B.getBoundingClientRect(),w=p.offsetWidth,h=p.offsetHeight;let x=s==1?r.right+8:r.left-w-8,y=r.top;
 p.style.right='auto';p.style.bottom='auto';
 p.style.left=Math.max(4,Math.min(innerWidth-w-4,x))+'px';p.style.top=Math.max(4,Math.min(innerHeight-h-4,y))+'px'}
const TBSIDE_P={qsp:'qsb',ncp:'ncb',calp:'tray'};
function tbSideFly(){for(const id in TBSIDE_P){const p=document.getElementById(id);if(p)tbSidePlace(p,document.getElementById(TBSIDE_P[id]))}}
/* a menu kept off the taskbar */
function tbSideMenu(){const s=tbSide();if(!s||typeof CMENU=='undefined'||!CMENU||!CMENU.el)return;const m=CMENU.el,bw=tbSideBw(),r=m.getBoundingClientRect();
 if(s==1&&r.left<bw+4)m.style.left=(bw+4)+'px';if(s==2&&r.right>innerWidth-bw-4)m.style.left=Math.max(2,innerWidth-bw-4-r.width)+'px'}
/* the desktop's inset following the taskbar's width */
function tbSideBwSet(){const b=document.getElementById('bar');if(b)document.body.style.setProperty('--bw',(b.offsetWidth+4)+'px')}
(function(){
 if(typeof cmenu=='function')cmenu=__after(cmenu,function(){tbSideMenu()},'tbside62.js');
 const mo=new MutationObserver(ms=>{for(const m of ms)for(const n of m.addedNodes)if(n.nodeType==1&&TBSIDE_P[n.id]){const p=n;tbSidePlace(p,document.getElementById(TBSIDE_P[p.id]));
  /* (and again as its content settles: the notifications' list, the calendar's month) */
  if(window.ResizeObserver){const ro=new ResizeObserver(()=>{if(!p.isConnected){ro.disconnect();return}tbSidePlace(p,document.getElementById(TBSIDE_P[p.id]))});ro.observe(p)}}});
 mo.observe(document.body,{childList:true});
 const b=document.getElementById('bar');if(b&&window.ResizeObserver)new ResizeObserver(()=>{tbSideBwSet();tbSideFly()}).observe(b);
 addEventListener('resize',tbSideFly)})();
