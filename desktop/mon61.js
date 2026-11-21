/* ===== Rudolf OS 1.6.1 (plan Part 1): several monitors, on the three workspaces. Decided by the owner on 2026-11-15: "These
   workspaces will be the basis for Rudolf being able to support up to three monitors/displays."
   - Up to three monitors (rudolf-display arranges them side by side, the main one at the left: /run/rudolf/monitors, passed on by
     rudolfd's screen request as "monitors"), each showing one of the three workspaces, with the wallpaper and a taskbar of its own.
   - The main monitor is the desktop as with one: the taskbar with the tray, the desktop's icons and widgets, the Start menu, the
     notifications. The page's own box (body) is the main monitor's (innerWidth and innerHeight too), so all of that stays there.
   - Each other monitor: the wallpaper, and a taskbar of its own with the start button, the pinned apps, that workspace's windows,
     the workspaces and the clock (the main taskbar's CSS, its rules copied for these: the same styles and colours).
   - A window is in a workspace, and shows on the monitor showing it (moved there: "Move to workspace N" moves it to that monitor);
     a window dragged onto another monitor goes into that monitor's workspace. Maximised, it fills its own monitor.
   - A workspace's button on a monitor's taskbar shows it there (the monitor that showed it takes this one's); Ctrl+Alt+arrows and a
     window's focus go to the workspace where it is shown, or show it on the monitor in use.
   - A monitor unplugged: its workspace is no longer shown (switched to from the taskbar, as with one monitor) and its windows go
     with it; each window is kept inside the monitor it shows on, never off the screen.
   - Settings › Display › Displays: the monitors, which is the main one, their order from the left and which workspace each shows,
     kept for this set of monitors (screenctl arrange).
   - The sign-in and lock screens, the screen saver and the shut-down screen cover every monitor.
   Nothing changes with one monitor, and nothing in an app's own window (one-app mode, oneapp59.js). rudolf-native does the same
   (iso/sys/rudolf-native.c, "several monitors"). ===== */
var MONS=[],MON_ON=false,MON_KEY='';   /* (MONS: the main one first, then from the left: {name,x,y,w,h,ws,main} in the X screen's pixels) */
const MON_OA=/[#&]app=/.test(location.hash),MON_IDS=['bar','start','pins','tasks','wsw','tray'];
const monU=()=>(devicePixelRatio||1)*(parseFloat(document.documentElement.style.zoom)||1);
const monOfWs=n=>MON_ON?MONS.findIndex(m=>m.ws==n):-1;
/* a monitor's place against the main one, and its size, in the page's units (the zoom of Display scaling counted) */
function monBox(k){const m=MONS[k],o=MONS[0],u=monU();return{x:(m.x-o.x)/u,y:(m.y-o.y)/u,w:m.w/u,h:m.h/u}}
/* the page's width and height: the main monitor's, with several */
(function(){if(MON_OA)return;for(const k of['innerWidth','innerHeight']){const d=Object.getOwnPropertyDescriptor(window,k);if(!d||!d.get||!d.configurable)continue;
 Object.defineProperty(window,k,{configurable:true,enumerable:d.enumerable,get(){if(MON_ON&&MONS[0])return(k=='innerWidth'?MONS[0].w:MONS[0].h)/(devicePixelRatio||1);return d.get.call(window)},set(v){}})}})();
/* the list as rudolfd gives it (or the tests): kept to three, the main one first; workspaces each once */
function monNorm(list){let L=(Array.isArray(list)?list:[]).filter(m=>m&&m.w>0&&m.h>0).slice(0,3).map(m=>({name:String(m.name||'monitor').slice(0,40),x:+m.x||0,y:+m.y||0,w:+m.w,h:+m.h,ws:+m.ws||0,main:!!m.main}));
 L.sort((a,b)=>a.x-b.x||a.y-b.y);const mi=Math.max(0,L.findIndex(m=>m.main));if(L.length)L.unshift(L.splice(mi,1)[0]);
 const used=new Set();L.forEach(m=>{if(m.ws<1||m.ws>NWS||used.has(m.ws))m.ws=0;else used.add(m.ws)});L.forEach(m=>{if(!m.ws){for(let i=1;i<=NWS;i++)if(!used.has(i)){m.ws=i;used.add(i);break}}});
 L.forEach((m,i)=>m.main=!i);return L}
/* new monitors: the workspace in use stays in use when it is still shown, else the main monitor's; the windows kept on their monitors.
   The same monitors again (a size changed elsewhere): the workspaces each shows now are kept */
function monSet(list,key){if(MON_OA)return;const L=monNorm(list),geo=a=>a.map(m=>[m.name,m.x,m.y,m.w,m.h].join(':')).join(',');
 if(MONS.length&&geo(L)==geo(MONS))L.forEach((m,i)=>m.ws=MONS[i].ws);
 MONS=L;MON_KEY=key||MONS.map(m=>m.name).sort().join('+');const was=MON_ON;MON_ON=MONS.length>1;if(!MON_ON&&!was)return;
 if(MON_ON&&monOfWs(WS)<0)WS=MONS[0].ws;monApply()}
/* ---- the page laid out for the monitors */
let MON_CSS=null,MON_RULES=null,MON_RT=0;
function monApply(){const H=document.documentElement;H.classList.toggle('mmon',MON_ON);if(!MON_CSS){MON_CSS=document.createElement('style');MON_CSS.id='monst';document.head.appendChild(MON_CSS)}
 const D=$('#desk');document.querySelectorAll('.mwall,.mbar,.mcover').forEach(e=>{if(!MON_ON||+e.dataset.k>=MONS.length)e.remove()});
 if(!MON_ON){MON_CSS.textContent='';if(MON_RULES)MON_RULES.textContent='';if(MON_MAXC)MON_MAXC.textContent='';wins.forEach(o=>{o.el.classList.remove('mon1','mon2');o.el.style.display=o.ws==WS&&!o.min?'flex':'none'});monKeep();tbRefresh();if(typeof scaleVars=='function')scaleVars();return}
 const o=MONS[0],u=monU();let css=`html.mmon{background:#000!important}html.mmon body{position:fixed;left:${o.x/u}px;top:${o.y/u}px;width:${o.w/u}px;height:${o.h/u}px;contain:layout;overflow:visible}`;
 for(let k=1;k<MONS.length;k++){const b=monBox(k);css+=`.win.mon${k}{translate:${b.x}px ${b.y}px}`+
  `.mwall[data-k='${k}'],.mcover[data-k='${k}']{left:${b.x}px;top:${b.y}px;width:${b.w}px;height:${b.h}px}`}
 css+=`.mwall,.mcover{position:absolute}.mcover{display:none;z-index:1000003;pointer-events:auto;background:radial-gradient(circle at 30% 20%,#1e3a8a,#0b1020 70%)}html.mlock .mcover{display:block}html.mdark .mcover{background:#000}`;
 MON_CSS.textContent=css;
 for(let k=1;k<MONS.length;k++){let w=document.querySelector(`.mwall[data-k='${k}']`);if(!w){w=document.createElement('div');w.className='mwall';w.dataset.k=k;w.setAttribute('aria-hidden','true');document.body.insertBefore(w,document.body.firstChild)}
  let c=document.querySelector(`.mcover[data-k='${k}']`);if(!c){c=document.createElement('div');c.className='mcover';c.dataset.k=k;c.setAttribute('aria-hidden','true');document.body.appendChild(c)}}
 monWall();monRules();monCover();wins.forEach(monWin);monKeep();if(typeof scaleVars=='function')scaleVars();tbRefresh()}
/* the desk's area on monitor K: the monitor less its taskbar (the bar as wide, or as high, as the main one's), in the page's units */
function monDesk(k){const D=$('#desk'),b=monBox(k),o=monBox(0);return{w:b.w-(o.w-(D?D.clientWidth:o.w)),h:b.h-(o.h-(D?D.clientHeight:o.h))}}
function monWall(){const bg=document.body.style.background;document.querySelectorAll('.mwall').forEach(w=>w.style.background=bg)}
/* the main taskbar's rules, copied for the other monitors' (#bar → :is(#mbar1,#mbar2), the same weight), again when they change */
function monRules(){clearTimeout(MON_RT);MON_RT=setTimeout(monRulesNow,30)}
/* (from the style sheets' own text: the CSSOM's text loses a shorthand with var() that a later longhand overrides, as
   #tasks button.on's background:var(--ac) followed by background-image:none) */
function monCssWalk(t,emit,wrap){let i=0;const n=t.length;
 const close=a=>{let d=0;for(let k=a;k<n;k++){const c=t[k];if(c=='"'||c=="'"){const e=t.indexOf(c,k+1);if(e<0)return n;k=e;continue}if(c=='{')d++;else if(c=='}'&&--d==0)return k}return n};
 while(i<n){const b=t.indexOf('{',i);if(b<0)break;const sel=t.slice(i,b).trim(),e=close(b);
  if(sel[0]=='@'){if(/^@(media|supports)/.test(sel))monCssWalk(t.slice(b+1,e),emit,x=>wrap(sel+'{'+x+'}'))}else emit(sel,t.slice(b+1,e),wrap);i=e+1}}
function monRulesNow(){if(!MON_ON)return;if(!MON_RULES){MON_RULES=document.createElement('style');MON_RULES.id='monbar';document.head.appendChild(MON_RULES)}
 const re=/#(bar|start|pins|tasks|wsw|tray)(?![\w-])/,reg=/#(bar|start|pins|tasks|wsw|tray)(?![\w-])/g,ks=[];for(let k=1;k<MONS.length;k++)ks.push(k);const out=[];
 const map=sel=>sel.replace(reg,(_,id)=>':is('+ks.map(k=>'#m'+id+k).join(',')+')');
 document.querySelectorAll('style').forEach(st=>{if(st===MON_RULES||st===MON_CSS||st===MON_MAXC)return;const t=st.textContent.replace(/\/\*[\s\S]*?\*\//g,'');if(!re.test(t))return;
  monCssWalk(t,(sel,body,wrap)=>{if(re.test(sel))out.push(wrap(map(sel)+'{'+body+'}'))},x=>x)});
 MON_RULES.textContent=out.join('\n')}
/* the sign-in and lock screens, the screen saver and shut-down's screen: the other monitors covered */
function monCover(){const H=document.documentElement,L=$('#login'),off=$('#off'),sv=$('#saver');const lk=!!(L&&getComputedStyle(L).display!='none')||(typeof LOCKED!='undefined'&&!!LOCKED)||!!$('#lockov'),dk=!!(off&&getComputedStyle(off).display!='none')||!!(sv&&sv.isConnected);
 H.classList.toggle('mlock',MON_ON&&(lk||dk));H.classList.toggle('mdark',MON_ON&&dk&&!lk)}
/* ---- the windows */
function monWin(W){if(!W||!W.el)return;const k=monOfWs(W.ws);W.el.classList.toggle('mon1',k==1);W.el.classList.toggle('mon2',k==2);
 if(MON_ON)W.el.style.display=k>=0&&!W.min?'flex':'none'}
/* every window inside the monitor it shows on (the main one for a workspace not shown): at least its title bar within reach */
function monKeep(){wins.forEach(W=>{const e=W.el;if(!e||e.classList.contains('max'))return;const k=Math.max(0,monOfWs(W.ws)),d=MON_ON?monDesk(k):{w:($('#desk')||e.parentElement).clientWidth,h:($('#desk')||e.parentElement).clientHeight};if(!(d.w>0&&d.h>0))return;
 const w=e.offsetWidth,h=e.offsetHeight;if(w>d.w-8&&w>200)e.style.width=Math.max(200,d.w-8)+'px';if(h>d.h-8&&h>120)e.style.height=Math.max(120,d.h-8)+'px';
 const l=e.offsetLeft,t=e.offsetTop,nw=e.offsetWidth,nh=e.offsetHeight;if(l>d.w-60||l+nw<60)e.style.left=Math.max(0,Math.min(d.w-nw-4,l))+'px';if(t>d.h-40||t<0)e.style.top=Math.max(0,Math.min(d.h-nh-4,t))+'px'})}
/* a workspace shown on monitor K (its taskbar's button): the monitor that showed it takes K's */
function monShow(k,n){if(!MON_ON||!MONS[k])return wsGo(n);n=Math.max(1,Math.min(NWS,n));const j=monOfWs(n),was=MONS[k].ws;
 if(j<0){MONS[k].ws=n}else if(j!=k){MONS[j].ws=was;MONS[k].ws=n}
 WS=n;wins.forEach(monWin);monKeep();tbRefresh();const top=MRU.find(o=>o.ws==WS&&!o.min);top&&top.focus();if(j!=k)notify('Workspace '+n,'',{quiet:true,toast:1200})}
(function(){if(MON_OA)return;
 const g=wsGo;wsGo=function(n){if(!MON_ON)return g(n);n=Math.max(1,Math.min(NWS,n));if(n==WS){tbRefresh();return}
  const j=monOfWs(n);if(j>=0){WS=n;tbRefresh();const top=MRU.find(o=>o.ws==WS&&!o.min);top&&top.focus();return}
  monShow(Math.max(0,monOfWs(WS)),n)};
 const mv=wsMove;wsMove=function(W,n){if(!MON_ON)return mv(W,n);const k0=monOfWs(W.ws);W.ws=Math.max(1,Math.min(NWS,n));monWin(W);
  if(k0>=0&&monOfWs(W.ws)>=0)monKeep();tbRefresh()};
 /* the main taskbar shows the main monitor's workspace; then the other monitors' taskbars */
 const tr=tbRefreshNow;tbRefreshNow=function(){if(!MON_ON)return tr();const w=WS;WS=MONS[0].ws;try{tr()}finally{WS=w}monBars()};
 const _w=win;win=function(...a){const W=_w(...a);if(MON_ON){monWin(W);setTimeout(()=>{monKeep();tbRefresh()},0)}return W};
 /* the main taskbar's workspaces: shown on the main monitor */
 document.addEventListener('click',e=>{if(!MON_ON)return;const b=e.target.closest&&e.target.closest('#wsw > button');if(!b||!/^[1-3]$/.test(b.textContent))return;e.stopImmediatePropagation();e.preventDefault();monShow(0,+b.textContent)},true);
 /* a window brought from one monitor onto another by its title bar: into that monitor's workspace, where it was let go */
 document.addEventListener('pointerup',e=>{if(!MON_ON)return;const el=e.target.closest&&e.target.closest('.win');if(!el||!e.target.closest('.tb'))return;const W=wins.find(o=>o.el===el);if(W)setTimeout(()=>monDrop(W),0)},true);
 /* the start button on another monitor: the menu there; the main one's: back on the main monitor */
 document.addEventListener('pointerdown',e=>{if(!MON_ON)return;const s=e.target.closest&&e.target.closest('#start');if(s)monPop(0)},true);
 applySet=__after(applySet,function(){if(MON_ON){monWall();monRules()}},"mon61.js");
 new MutationObserver(ms=>{if(!MON_ON)return;const mine=n=>n===MON_RULES||n===MON_CSS||n===MON_MAXC||(n&&n.parentNode&&(n.parentNode===MON_RULES||n.parentNode===MON_CSS||n.parentNode===MON_MAXC));
  if(ms.some(m=>!mine(m.target)&&!(m.type=='childList'&&[...m.addedNodes,...m.removedNodes].every(mine))))monRules()}).observe(document.head,{childList:true,subtree:true,characterData:true});
 /* the monitor in use: the one clicked on (its wallpaper, its taskbar; a window's focus says so itself) */
 document.addEventListener('pointerdown',e=>{if(!MON_ON||!e.target.closest)return;if(e.target.closest('.win'))return;const t=e.target.closest('.mbar,.mwall');const k=t?+t.dataset.k:e.target.closest('#bar,#desk')?0:-1;
  if(k>=0&&MONS[k]&&WS!=MONS[k].ws){WS=MONS[k].ws;tbRefresh()}},true);
 const lo=A.logoff;A.logoff=()=>{lo();if(MON_ON){MONS.forEach((m,i)=>m.ws=m.ws0||i+1);WS=MONS[0].ws;monApply()}};
 setInterval(()=>{if(MON_ON)monCover()},500);
 addEventListener('resize',()=>{clearTimeout(monFetch.t);monFetch.t=setTimeout(monFetch,600)});
 setTimeout(monFetch,1500)})();
function monDrop(W){const el=W.el;if(!el||el.style.display=='none'||el.classList.contains('max'))return;const r=el.getBoundingClientRect(),cx=r.left+r.width/2,cy=r.top+Math.min(16,r.height/2);
 let j=-1;for(let k=0;k<MONS.length;k++){const a=k?document.querySelector(`.mwall[data-k='${k}']`):document.body;if(!a)continue;const q=a.getBoundingClientRect();if(cx>=q.left&&cx<q.right&&cy>=q.top&&cy<q.bottom)j=k}
 const k=monOfWs(W.ws);if(j<0||j==k||k<0)return;const a=monBox(k),b=monBox(j);el.style.left=el.offsetLeft+a.x-b.x+'px';el.style.top=el.offsetTop+a.y-b.y+'px';
 W.ws=MONS[j].ws;WS=W.ws;monWin(W);monKeep();tbRefresh()}
/* a pop-up opened from monitor K's taskbar (the Start menu, a window's menu, the calendar): moved onto that monitor, its taskbar's side kept */
function monPop(k){const m=$('#menu');if(!m)return;if(!MON_ON||!k){m.style.translate='';return}const b=monBox(k),o=monBox(0),side=S.tb||'bottom';
 m.style.translate=`${b.x+(side=='right'?b.w-o.w:0)}px ${b.y+(side=='bottom'?b.h-o.h:0)}px`}
function monPopNew(k,before){if(!k)return;const b=monBox(k),o=monBox(0),side=S.tb||'bottom';const dx=b.x+(side=='right'?b.w-o.w:0),dy=b.y+(side=='bottom'?b.h-o.h:0);
 document.querySelectorAll('.ctx,.flyout').forEach(e=>{if(!before.has(e))e.style.translate=`${dx}px ${dy}px`})}
function monAt(k,fn){const before=new Set(document.querySelectorAll('.ctx,.flyout'));fn();setTimeout(()=>monPopNew(k,before),0);setTimeout(()=>monPopNew(k,before),60)}
/* ---- the other monitors' taskbars: the start button, the pinned apps, the workspace's windows, the workspaces, the clock */
/* a maximised window fills its monitor's desk (the desk's size known once the taskbar is laid out: again at each refresh) */
let MON_MAXC=null;function monMaxCss(){if(!MON_MAXC){MON_MAXC=document.createElement('style');MON_MAXC.id='monmax';document.head.appendChild(MON_MAXC)}let t='';
 for(let k=1;k<MONS.length;k++){const d=monDesk(k);if(d.w>0&&d.h>0)t+=`.win.max.mon${k}{width:${d.w-6}px!important;height:${d.h-6}px!important}`}if(MON_MAXC.textContent!=t)MON_MAXC.textContent=t}
function monBars(){if(!MON_ON)return;monMaxCss();const B=$('#bar');for(let k=1;k<MONS.length;k++){let bar=document.getElementById('mbar'+k);
  if(!bar){bar=document.createElement('div');bar.id='mbar'+k;bar.className='mbar';bar.dataset.k=k;bar.setAttribute('role','toolbar');bar.setAttribute('aria-label','Taskbar, display '+(k+1));
   bar.innerHTML=`<button id='mstart${k}' title='Start' aria-label='Start'></button><div id='mpins${k}' role='toolbar' aria-label='Pinned apps'></div><div id='mtasks${k}' aria-label='Open windows'></div><div id='mwsw${k}' role='toolbar' aria-label='Workspaces'></div><div id='mtray${k}' class='sunk'></div>`;
   document.body.appendChild(bar);const st=bar.querySelector('#mstart'+k);
   st.onclick=()=>{monPop(k);const n=MONS[k].ws;if(WS!=n){WS=n;tbRefresh()}startToggle()};
   bar.querySelector('#mtray'+k).onclick=()=>{const t=$('#tray');if(t)monAt(k,()=>t.click())};
   bar.oncontextmenu=e=>{if(e.target.closest('button'))return;e.preventDefault();if(typeof barMenu=='function'){const b=monBox(k);monAt(k,()=>barMenu(e.clientX-b.x,e.clientY-b.y))}}}
  /* the main bar's look: its classes and its own style (the place is this monitor's) */
  bar.className=(B.className+' mbar').trim();const box=monBox(k),side=S.tb||'bottom',bw=B.offsetWidth,bh=B.offsetHeight;
  const pl=side=='top'?[box.x,box.y,box.w,bh]:side=='left'?[box.x,box.y,bw,box.h]:side=='right'?[box.x+box.w-bw,box.y,bw,box.h]:[box.x,box.y+box.h-bh,box.w,bh];
  bar.style.cssText=B.style.cssText+`;left:${pl[0]}px;top:${pl[1]}px;width:${side=='left'||side=='right'?'max-content':pl[2]+'px'};height:${side=='left'||side=='right'?pl[3]+'px':''};right:auto;bottom:auto;`+(side=='left'||side=='right'?'':'box-sizing:'+getComputedStyle(B).boxSizing);
  if(side!='left'&&side!='right'){const cs=getComputedStyle(B),pad=parseFloat(cs.paddingLeft)+parseFloat(cs.paddingRight)+(cs.boxSizing=='border-box'?0:0);bar.style.width=(cs.boxSizing=='border-box'?pl[2]:pl[2]-pad)+'px'}
  const st=bar.querySelector('#mstart'+k),S0=$('#start');if(S0&&st.dataset.h!=S0.innerHTML){st.innerHTML=S0.innerHTML;st.dataset.h=S0.innerHTML;st.className=S0.className}
  /* the pinned apps: the main bar's, a click opening the app (or its window) in this monitor's workspace */
  const P=bar.querySelector('#mpins'+k),P0=$('#pins');P.innerHTML='';P.__src=P0?P0.innerHTML:'';if(P0)[...P0.children].forEach(b0=>{const b=b0.cloneNode(true);const n=MONS[k].ws;if(!b.querySelector('img')&&S.icons!=='emoji'&&typeof icImg=='function'){const im=icImg(b.textContent.trim());if(im){b.textContent='';b.appendChild(im)}}
   b.onclick=()=>{WS=n;b0.click();tbRefresh()};b.oncontextmenu=e=>{e.preventDefault();e.stopPropagation();monAt(k,()=>b0.dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,clientX:e.clientX-box.x,clientY:e.clientY-box.y})))};P.appendChild(b)});
  /* this workspace's windows (one button each), as the main bar's buttons */
  const T=bar.querySelector('#mtasks'+k),n=MONS[k].ws;T.innerHTML='';wins.forEach(o=>{if(o.ws!=n||!o.tb)return;const b=o.tb.cloneNode(true);b.__o=o;b.__src=o.tb.innerHTML;b.style.display='';delete b.dataset.count;b.classList.remove('grp');
   b.classList.toggle('on',o.el.classList.contains('act')&&!o.min);b.setAttribute('aria-pressed',o.el.classList.contains('act')&&!o.min?'true':'false');
   b.onclick=()=>{if(o.min||!o.el.classList.contains('act'))o.focus();else o.el.querySelector('.mn').click();tbRefresh()};
   b.oncontextmenu=e=>{e.preventDefault();e.stopPropagation();monAt(k,()=>winMenu(o,e.clientX-box.x,e.clientY-box.y))};T.appendChild(b)});
  /* the workspaces: this monitor's lit */
  const Wv=bar.querySelector('#mwsw'+k);Wv.innerHTML='';for(let i=1;i<=NWS;i++){const b=document.createElement('button');b.textContent=i;b.title='Workspace '+i+(wins.some(o=>o.ws==i)?'':' (empty)');b.setAttribute('aria-label','Workspace '+i);
   if(i==n)b.classList.add('on');if(wins.some(o=>o.ws==i))b.classList.add('has');b.onclick=()=>monShow(k,i);Wv.appendChild(b)}
  {const ov=document.createElement('button');ov.textContent='▦';ov.title='Show all windows (Super+Tab)';ov.setAttribute('aria-label','Show all windows');ov.onclick=()=>{WS=n;overview()};Wv.appendChild(ov)}
  /* the clock, as the tray's */
  const C=bar.querySelector('#mtray'+k),C0=$('#tray');if(C0&&C.innerHTML!=C0.innerHTML){C.innerHTML=C0.innerHTML;C.title=C0.title}if(C0)C.style.cssText=C0.style.cssText}}
setInterval(()=>{if(!MON_ON)return;const C0=$('#tray');
 /* (what the main taskbar's buttons show changed since: a window's new title, the pinned apps' pictures) */
 if(document.querySelector('.mbar')&&([...document.querySelectorAll('.mbar [id^=mtasks] > button')].some(b=>b.__o&&b.__o.tb&&(b.__src!=b.__o.tb.innerHTML||b.classList.contains('on')!=(b.__o.el.classList.contains('act')&&!b.__o.min)))||[...document.querySelectorAll('.mbar [id^=mpins]')].some(P=>$('#pins')&&P.__src!=$('#pins').innerHTML)))monBars();document.querySelectorAll('.mbar .sunk').forEach(C=>{if(C0&&C.innerHTML!=C0.innerHTML){C.innerHTML=C0.innerHTML;C.title=C0.title}})},1000);
/* ---- the monitors from rudolfd (screenctl status's "monitors", from rudolf-display), when the page starts and when its size changes */
async function monFetch(){if(MON_OA||typeof RD=='undefined'||!RD)return;try{const st=await rd('screen');const m=st&&st.monitors;if(m&&Array.isArray(m.monitors)){m.monitors.forEach(x=>x.ws0=x.ws);monSet(m.monitors,m.key);MONS.forEach(x=>{const f=m.monitors.find(y=>y.name==x.name);x.ws0=f?f.ws:x.ws})}}catch(e){}}
/* ---- Settings › Display › Displays */
(function(){if(typeof dispPanelS!='function')return;dispPanelS=__after(dispPanelS,function(c,W){const d=sp(c,`<h4 class='sh4'>Displays</h4><div id='dmons'></div>`);monUi(d.querySelector('#dmons'))},"mon61.js");})();
function monUi(box){if(!box)return;let L=MONS.map(m=>({...m}));const draw=()=>{if(!box.isConnected)return;
 if(L.length<2){box.innerHTML=`<div class='msg'>One display${L[0]?' ('+(+L[0].w)+'×'+(+L[0].h)+')':''}. Plug in another monitor (up to three) and each shows one of the three workspaces, with its own wallpaper and taskbar.</div>`;return}
 box.innerHTML=`<div class='msg'>${L.length} displays, side by side from the left. The main display has the tray, the desktop’s icons and the Start menu; each display shows one of the three workspaces. Kept for this set of displays.</div>`+
  `<div class='dmonpic' aria-hidden='true' style='display:flex;gap:6px;align-items:flex-end;margin:8px 0'>${L.map((m,i)=>`<div style='width:${Math.round(m.w/40)}px;height:${Math.round(m.h/40)}px;border:2px solid ${i?'var(--bd,#999)':'var(--ac,#2563eb)'};border-radius:4px;display:flex;align-items:center;justify-content:center;font-weight:600'>${+m.ws}</div>`).join('')}</div>`+
  `<div class='r2'>${L.map((m,i)=>`<label for='dmon${i}'>Display ${i+1}${i?'':' (main)'}</label><div><b class='dmonn'></b> · ${+m.w}×${+m.h} · <select id='dmon${i}' aria-label='Workspace shown on display ${i+1}'>${[1,2,3].map(n=>optH(n,'Workspace '+n,m.ws)).join('')}</select>`+
   ` <button class='dmonl' data-i='${i}'${i?'':' disabled'} title='${i==1?'Make this the main display':'Move to the left'}'>${i==1?'Make main':'◀ Left'}</button></div>`).join('')}</div>`+
  `<div><button class='pri' id='dmonsave'>Keep this arrangement</button> <span class='msg' id='dmonm'></span></div>`;
 box.querySelectorAll('.dmonn').forEach((e,i)=>e.textContent=L[i].name);
 const cur=()=>L.map((m,i)=>({...m,ws:+box.querySelector('#dmon'+i).value}));
 box.querySelectorAll('select').forEach((s,i)=>s.onchange=()=>{const v=+s.value,j=L.findIndex((m,x)=>x!=i&&+box.querySelector('#dmon'+x).value==v);if(j>=0)box.querySelector('#dmon'+j).value=L[i].ws;L.forEach((m,x)=>m.ws=+box.querySelector('#dmon'+x).value)});
 box.querySelectorAll('.dmonl').forEach(b=>b.onclick=()=>{const i=+b.dataset.i;if(i<1)return;const c2=cur();[c2[i-1],c2[i]]=[c2[i],c2[i-1]];monSave(c2,box,draw)});
 box.querySelector('#dmonsave').onclick=()=>monSave(cur(),box,()=>{L=MONS.map(m=>({...m}));fetchL()})};
 /* (the list as rudolfd has it: an app's own window on the native desktop has no monitors of its own) */
 const fetchL=()=>{if(typeof RD=='undefined'||!RD)return;rd('screen').then(st=>{const m=st&&st.monitors;if(m&&Array.isArray(m.monitors)){L=monNorm(m.monitors);if(MON_ON)L.forEach(x=>{const y=MONS.find(z=>z.name==x.name);if(y)x.ws=y.ws});draw()}}).catch(()=>{})};
 draw();fetchL();box.__monDraw=draw}
async function monSave(list,box,redraw){const spec=list.map(m=>m.name+'='+m.ws).join(','),msg=box.querySelector('#dmonm');
 /* shown now: the workspaces each display shows (the order follows when rudolf-display has moved them) */
 if(!MON_OA&&MON_ON){list.forEach(m=>{const x=MONS.find(y=>y.name==m.name);if(x){x.ws=m.ws;x.ws0=m.ws}});if(monOfWs(WS)<0)WS=MONS[0].ws;monApply()}
 if(typeof RD=='undefined'||!RD){if(msg)msg.textContent='Shown now (kept on the installed Rudolf OS).';return}
 try{await rd('screen/arrange?set='+encodeURIComponent(spec),{method:'POST'});if(msg)msg.textContent='Kept for these displays.';setTimeout(()=>{monFetch().then(()=>redraw&&redraw())},1500)}catch(e){if(msg)msg.textContent=e.message}}
