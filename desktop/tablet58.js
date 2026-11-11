/* ===== Rudolf OS 1.5.8 (plan 8.1): the tablet layout, for x86 tablets and handhelds (Surface Go, Steam Deck, ROG
   Ally, Legion Go) and for the browser build on a phone or tablet.
   - Chosen by itself when the screen is touch only (a touchscreen and no mouse or touchpad), or by hand in Settings ›
     Display › Tablet mode (Automatic, On, Off).
   - Bigger targets (the touch mode), the taskbar as a dock (icons only, larger), each window full screen, one at a
     time (the one in front; the others wait behind it, on the dock), no moving or resizing.
   - Swipe up from the bottom edge for the Start menu, in from the right edge for the notifications.
   - Portrait and landscape: the layout follows the screen's shape.
   The on-screen keyboard (a11y25.js) opens on a tap in a text box, as in the touch mode. Rotation from the
   accelerometer and the checks on tablets and in QEMU follow in 1.5.9. ===== */
Object.assign(DEFS.disp,{tablet:'auto'});if(!DKEYS.includes('tablet'))DKEYS.push('tablet');if(S.tablet===undefined)S.tablet='auto';if(SC&&SC.tablet===undefined)SC.tablet='auto';
const tabletAuto=()=>matchMedia('(pointer:coarse)').matches&&!matchMedia('(any-pointer:fine)').matches;
function tabletOn(){return S.tablet=='on'||(S.tablet!='off'&&tabletAuto())}
function tabletApply(){const H=document.documentElement,on=tabletOn();const was=H.classList.contains('tablet');H.classList.toggle('tablet',on);
 H.classList.toggle('touch',on||!!S.touch);H.classList.toggle('portrait',innerHeight>innerWidth);
 if(on!=was)dispatchEvent(new CustomEvent('rudolf-tablet',{detail:{on}}))}
addEventListener('resize',()=>tabletApply());
['(pointer:coarse)','(any-pointer:fine)'].forEach(m=>{try{matchMedia(m).addEventListener('change',()=>tabletApply())}catch(e){}});
(function(){if(typeof themeApply!='function')return;const o=themeApply;themeApply=function(){const r=o.apply(this,arguments);tabletApply();return r}})();
tabletApply();
/* the dock: each taskbar button shows its app's icon (its picture, or the first character of its text) */
const tabIc=t=>{t=String(t||'').trim();try{const g=new Intl.Segmenter(undefined,{granularity:'grapheme'}).segment(t)[Symbol.iterator]().next().value;return g?g.segment:''}catch(e){return [...t][0]||''}};
function tabletDock(){const T=document.getElementById('tasks');if(!T)return;T.querySelectorAll('button').forEach(b=>{const i=b.querySelector('img.ico')?'':tabIc(b.textContent);if(b.dataset.ic!==i)b.dataset.ic=i})}
(function(){const T=document.getElementById('tasks');if(!T)return;new MutationObserver(()=>tabletDock()).observe(T,{childList:true,subtree:true,characterData:true});tabletDock()})();
/* swipes from the screen's edges */
(function(){let st=null;const EDGE=24,FAR=60;
 addEventListener('pointerdown',e=>{if(!tabletOn()||e.pointerType=='mouse'){st=null;return}
  const fromB=e.clientY>=innerHeight-EDGE,fromR=e.clientX>=innerWidth-EDGE;st=fromB||fromR?{x:e.clientX,y:e.clientY,b:fromB,r:fromR,id:e.pointerId}:null},{capture:true,passive:true});
 addEventListener('pointermove',e=>{if(!st||e.pointerId!=st.id)return;
  if(st.b&&st.y-e.clientY>FAR){st=null;const m=document.getElementById('menu');if(m&&m.style.display!='flex'){const s=document.getElementById('start');s&&s.click()}}
  else if(st.r&&!st.b&&st.x-e.clientX>FAR){st=null;if(!document.getElementById('ncp')&&typeof ncToggle=='function')ncToggle()}},{capture:true,passive:true});
 addEventListener('pointerup',()=>{st=null},{capture:true,passive:true});addEventListener('pointercancel',()=>{st=null},{capture:true,passive:true})})();
/* Settings › Display › Tablet mode */
(function(){if(typeof dispPanelS!='function')return;const o=dispPanelS;dispPanelS=function(c,W){o(c,W);
 const d=sp(c,`<h4 class='sh4'>Tablet mode</h4><div class='r2'><label for='dtab'>Tablet mode</label><select id='dtab'>${optH('auto','Automatic (on when there is a touchscreen and no mouse or touchpad)',S.tablet||'auto')}${optH('on','On',S.tablet)}${optH('off','Off',S.tablet)}</select></div>
  <div class='msg dtabm'></div>`);
 const msg=()=>{d.querySelector('.dtabm').textContent=tabletOn()?'Tablet mode is on: each window fills the screen, the taskbar is a dock, swipe up from the bottom for Start and in from the right for notifications.':'Tablet mode is off.'};
 d.querySelector('#dtab').onchange=e=>{S.tablet=e.target.value;if(SC)SC.tablet=S.tablet;persistS();tabletApply();msg()};msg()}})();
