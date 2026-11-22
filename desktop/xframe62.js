/* ===== Rudolf OS 1.6.2 (docs/plan-1.6.2.md Part 29): Rudolf's title bar on Standard's windows from outside the page =====
   The owner, 2026-11-21: every window from outside the page (the encrypted Oracle, Oracle's Tor windows, Linux apps) gets
   Rudolf's title bar on Standard, as Native's window manager frames them all. Each such window (rudolfd's apps/windows, the
   list the taskbar's buttons are made from) gets a window of the page's own (win(): its title bar, buttons, edges, menu,
   styles and colours, the workspaces and monitors, exactly as Rudolf's windows), and the app's X window is placed in it:
   rudolf-display puts the window shown where the frame's inside is (apps/show with the place: "win HEX X Y W H").
   - The frame that is the page's active window has its app shown in it; another window or the desktop clicked, the app goes
     behind the page (apps/desktop, as before) and its frame keeps its place, its inside showing the app's icon and name
     ("Click to show it"); a click on the frame brings the app back.
   - Its title bar's buttons: minimize (the app behind the page), maximize and restore, close (the app asked to close, as its
     own close button does: wmcmd close); moving and sizing from every edge; the window's menu (workspaces, always on top);
     the taskbar button is the app's own (linux26.js's), which minimizes and restores it.
   - Full-screen windows and games have no frame and keep the whole screen (rudolf-display); the browser window for sign-in
     sites keeps its 🌐 button.
   - New windows start maximized, as Standard has always fitted them, unless the app's frame was last left at a size.
   - Nothing the page did not have: the windows' ids, classes and titles from apps/windows; the X window's place is the
     frame's own. Never shown over the lock screen, the screen saver or a prompt of the page's. ===== */
const XF={map:new Map(),busy:0,next:'',sent:'',sendT:0,pollT:0,lastX:null,raf:0,obs:null,IN:0};
function xfOn(){return !!(RD&&!(typeof ONEAPP!='undefined'&&ONEAPP)&&USER)}
/* nothing may come over the page now: the lock screen, the screen saver, a prompt of the page's */
function xfBlocked(){return !USER||(typeof LOCKED!='undefined'&&LOCKED)||(typeof SAVER!='undefined'&&SAVER)||!!document.querySelector('#lockov,#saver,.rmodal')}
/* the height of Rudolf's title bar in the screen's pixels (rudolf-display's strip for a window not framed yet: web/strut's bar) */
function xfBarPx(){if(!xfOn())return 0;let tb=document.querySelector('#desk .win>.tb');let probe=null;
 if(!tb){probe=document.createElement('div');probe.className='win';probe.style.cssText='position:absolute;left:-9999px;top:0;width:300px;visibility:hidden';probe.appendChild(document.createElement('div')).className='tb';probe.firstChild.textContent='x';document.getElementById('desk').appendChild(probe);tb=probe.firstChild}
 const h=tb.getBoundingClientRect().height;if(probe)probe.remove();return Math.max(0,Math.min(120,Math.round(h*(devicePixelRatio||1))))}
/* (Part 29's left part) the frame's top edge above the app's window in the screen's pixels: its title bar and edges (xfRect's top
   less the frame's), for rudolf-display's own drag of an app (its frame's top not above the desktop's) */
function xfFramePx(){if(!xfOn())return 0;let el=document.querySelector('#desk .win.xfr'),probe=null;
 if(!el){probe=el=document.createElement('div');el.className='win xfr';el.style.cssText='position:absolute;left:-9999px;top:0;width:300px;height:200px;visibility:hidden';
  el.appendChild(document.createElement('div')).className='tb';el.firstChild.textContent='x';el.appendChild(document.createElement('div')).className='body';document.getElementById('desk').appendChild(el)}
 const t=el.querySelector('.body').getBoundingClientRect().top-el.getBoundingClientRect().top;if(probe)probe.remove();return Math.max(0,Math.min(160,Math.round(t*(devicePixelRatio||1))))}
/* the frame's inside (the app's place) in the X screen's pixels: below Rudolf's title bar, inside the frame's 1 px edge, as Native
   frames a window (_NET_FRAME_EXTENTS 1 1 35 1); the sizing edges reach 3 px beyond the frame, where only the page has pixels
   (XF.IN: px kept free inside as well, none) */
function xfRect(F){const r=F.W.body.getBoundingClientRect(),k=devicePixelRatio||1,i=XF.IN;
 const g=[Math.round((r.left+i)*k),Math.round(r.top*k),Math.round((r.width-2*i)*k),Math.round((r.height-i)*k)];return g[2]>=40&&g[3]>=40?g:null}
function xfLabel(w){const a=lxAppFor(w);return w.title||(a&&a.name)||w.class||'Linux app'}
function xfKey(w){const a=lxAppFor(w);return 'xf:'+String(a?a.app:(w.class||w.instance||'app')).toLowerCase()}
function xfIcon(F,w){const sp=F.W.el.querySelector('.tb>span'),ph=F.W.body.querySelector('.xfpi'),c=String(w.class||'').toLowerCase(),a=lxAppFor(w);
 [sp,ph].forEach(el=>{if(!el)return;if(c=='rudolftor'&&typeof torSvg=='function')el.innerHTML=torSvg(16);else if(c=='rudolforacle')el.textContent='🌐️';else if(a)lxImg(el,a.app,a.icon);else el.textContent='🐧'})}
/* a frame for an app window */
function xfMake(w){const was=LAUNCHING;let W;LAUNCHING=xfKey(w);
 try{W=win('','🐧',760,520)}finally{LAUNCHING=was}
 const id=w.win,F={id,W,full:0,gone:0,label:''};W.xf=F;W.app=xfKey(w);
 W.el.classList.add('xfr');W.el.dataset.xwin=id;
 if(W.tb&&W.tb.parentNode)W.tb.remove();W.tb=document.createElement('button');   /* (its taskbar button is linux26.js's: xfButton) */
 const ph=document.createElement('div');ph.className='xfph';ph.setAttribute('aria-hidden','true');
 ph.appendChild(document.createElement('span')).className='xfpi';ph.appendChild(document.createElement('span')).className='xfpt';
 const m=ph.appendChild(document.createElement('span'));m.className='xfpm';m.textContent='Click to show it';W.body.appendChild(ph);
 W.onclose=()=>{if(F.gone)return true;lxApi('apps/close?win='+id).then(()=>setTimeout(lxWinPoll,400)).catch(()=>{});return false};
 XF.map.set(id,F);xfTitle(F,w);xfIcon(F,w);
 /* (a window's last size for this app, kept by shell25.js as for Rudolf's windows; else maximized, as Standard fitted it) */
 const g=typeof geoK=='function'?(ld(geoK(),{})||{})[W.app]:null;if(!(g&&g.w>=200&&g.h>=120)&&!W.el.classList.contains('max'))W.el.querySelector('.mx').click();
 if(!XF.obs)XF.obs=new MutationObserver(xfSoon);XF.obs.observe(W.el,{attributes:true,attributeFilter:['style','class']});
 return F}
function xfTitle(F,w){const t=xfLabel(w);if(t==F.label)return;F.label=t;F.W.el.querySelector('.t').textContent=t;F.W.el.setAttribute('aria-label',t);F.W.body.querySelector('.xfpt').textContent=t}
/* the app's taskbar button (linux26.js's) is the frame's: minimizes and restores it, its menu as before */
function xfButton(F){const b=document.querySelector(`#tasks .lxtb[data-win="${F.id}"]`);if(!b)return;F.W.tb=b;if(b.__xf)return;b.__xf=1;
 b.onclick=e=>{e.stopPropagation();const W=F.W;if(!W.min&&W.el.classList.contains('act')&&XF.lastX==F.id)W.el.querySelector('.mn').click();else W.focus()}}
function xfDrop(F){F.gone=1;XF.map.delete(F.id);try{F.W.close()}catch(e){}if(F.W.el.isConnected)F.W.el.remove()}
/* no frame is the active window (the desktop, a game or the browser window in front) */
function xfDeact(){for(const F of XF.map.values())F.W.el.classList.remove('act')}
/* a menu of the page's (the frame's own, from its title bar or its taskbar button) over the app's place: the app waits
   behind the page until it closes, so the menu can be seen */
function xfCovered(F){const b=F.W.body.getBoundingClientRect();return [...document.querySelectorAll('.ctx')].some(m=>{const r=m.getBoundingClientRect();return r.width&&r.left<b.right&&r.right>b.left&&r.top<b.bottom&&r.bottom>b.top})}
/* the frame whose app should be shown now */
function xfWant(){if(xfBlocked())return null;for(const F of XF.map.values()){const el=F.W.el;if(el.classList.contains('act')&&!F.W.min&&el.style.display!='none'&&el.isConnected)return xfCovered(F)?null:F}return null}
function xfSoon(){if(XF.raf)return;XF.raf=requestAnimationFrame(()=>{XF.raf=0;xfReconcile()})}
/* rudolf-display told what the page wants: the active frame's app shown in it, or the desktop when a frame's app was shown */
function xfReconcile(){if(!xfOn())return;const xs=XF.map.get(XF.lastX);if(xs&&xs.full)return;   /* (full screen: rudolf-display's) */
 const F=xfWant();let q='';
 if(F&&!F.full){const g=xfRect(F);if(g)q='apps/show?win='+F.id+'&x='+g[0]+'&y='+g[1]+'&w='+g[2]+'&h='+g[3]+(F.W.el.classList.contains('max')?'&max=1':'')}
 else if(xs)q='apps/desktop';
 if(!q||q==XF.sent)return;XF.sent=q;XF.lastX=F&&q.startsWith('apps/show')?F.id:'';xfSend(q);xfAskTick()}
function xfSend(q){if(XF.busy){XF.next=q;return}XF.busy=1;
 lxApi(q).catch(()=>{XF.sent=''}).finally(()=>{XF.busy=0;XF.sendT=Date.now();if(XF.next){const n=XF.next;XF.next='';if(n!=q)xfSend(n)}})}
/* each time the taskbar's list of windows is read (lxWinPoll, every 1.5 s and after each action): frames made, kept current,
   closed; a change made elsewhere (a new window, Super+Tab, Super+Esc, the saver, a prompt) followed */
function xfSync(){if(!xfOn()){if(XF.map.size)for(const F of [...XF.map.values()])xfDrop(F);return}
 const list=(LX.wins||[]).filter(w=>!w.web),fresh=new Set(),cur=!XF.busy&&XF.pollT>XF.sendT;   /* (cur: the list asked for after the page's last word) */
 for(const F of [...XF.map.values()])if(!list.some(w=>w.win==F.id))xfDrop(F);
 for(const w of list){let F=XF.map.get(w.win);if(!F){F=xfMake(w);fresh.add(w.win)}xfTitle(F,w);xfButton(F);
  const full=!!w.fullscreen;if(full!=!!F.full){F.full=full;F.W.el.classList.toggle('xffull',full)}}
 const sh=list.find(w=>w.shown),xs=sh?sh.win:'';
 if(XF.map.size&&typeof webStrut=='function'){const fp=xfFramePx();if(fp!=XF.framePx){XF.framePx=fp;webStrut()}}   /* (Part 29's left part: the title bar's style changed) */
 /* what rudolf-display shows changed elsewhere (read after the page's last word, so not one of its own on the way) */
 if(cur&&(xs!==XF.lastX||fresh.has(xs))){XF.lastX=xs;XF.sent='';
  const F=XF.map.get(xs);if(F){if(F.W.min||!F.W.el.classList.contains('act'))F.W.focus()}else xfDeact()}
 xfReconcile()}
(function(){/* (when each list was asked for: one asked before the page's last word may not have it yet) */
 lxApi=(function(o){return function(p){const t=Date.now(),r=o.apply(this,arguments);return p==='apps/windows'?r.then(v=>{XF.pollT=t;try{xfAsks(v&&v.asks)}catch(e){console.warn('xframe asks',e)}return v}):r}})(lxApi);
 lxTaskbar=__after(lxTaskbar,function(){try{xfSync()}catch(e){console.warn('xframe',e)}},'xframe62.js');
 /* a workspace changed, a window focused, closed or minimized: the taskbar is drawn again (shell25.js) */
 tbRefreshNow=__after(tbRefreshNow,function(){if(XF.map.size)xfSoon()},'xframe62.js');
 /* a press anywhere but on a frame or its taskbar button: no frame is the active window any more (linux26.js sends the
    desktop forward as before) */
 addEventListener('pointerdown',e=>{if(!XF.map.size)return;const t=e.target;if(t.closest&&(t.closest('.xfr')||t.closest('.lxtb')))return;xfDeact();xfSoon()},true);
 addEventListener('resize',()=>{if(XF.map.size)xfSoon()});
 new MutationObserver(()=>{if(XF.map.size)xfSoon()}).observe(document.body,{childList:true});   /* (a menu, the lock screen, a prompt: opened or closed) */
 /* (Force quit in a frame's menu: the app's, as on its taskbar button) */
 if(typeof fqWinItem=='function'){const o=fqWinItem;fqWinItem=function(W){if(W&&W.xf&&typeof fqXItem=='function')return fqXItem(W.xf.id,W.xf.label);return o.apply(this,arguments)}}
 const st=document.createElement('style');st.textContent=`.xfr>.body{background:#f3f4f6;overflow:hidden;position:relative}
.xfph{position:absolute;inset:0;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:8px;color:#4b5563;font-size:14px;pointer-events:none}
.xfph .xfpi{font-size:40px;line-height:1}.xfph .xfpi img{width:56px;height:56px}.xfph .xfpm{font-size:12px;opacity:.75}
.xfr>.tb>span:first-child img{width:16px;height:16px;vertical-align:-3px}.xfr.xffull{visibility:hidden}`;document.head.appendChild(st)})();
/* ===== 1.6.2 (docs/plan-1.6.2.md Part 29, its left part): an app's own maximize and minimize buttons and a drag of its own title
   bar or edges, as on Native. rudolf-display does what the app asks as Native's window manager does it (the drag: the app's window
   moved and sized at once, by Native's rules, which are the page's own) and asks the page (apps/windows' "asks"):
   - "at": the app's window's new place: its frame follows (the frame's inside where the app is); at the drag's end the monitor it
     was let go on, as a drag of Rudolf's title bar (mon61.js);
   - "max": maximize, restore or toggle the frame, as its □ (the frame's state goes back with its place: apps/show's max=1, and
     rudolf-display marks the app's window maximized, so the app shows its restore button);
   - "min": minimize it, as its _.
   Asked again quickly while an app is shown (every 250 ms) and during its drag (every 40 ms); asks from before the page was loaded
   are not done again. ===== */
XF.askN=-1;XF.drag='';XF.dragT=0;XF.askT=0;
function xfAsks(a){if(!Array.isArray(a))return;const top=a.reduce((m,x)=>Math.max(m,+x.n||0),0);
 if(XF.askN<0){XF.askN=top;return}
 for(const x of a){if(!(+x.n>XF.askN))continue;XF.askN=+x.n;const F=XF.map.get(String(x.win));if(!F||F.full)continue;
  try{if(x.k=='at')xfAskAt(F,x);else if(x.k=='max')xfAskMax(F,+x.act);else if(x.k=='min')xfAskMin(F)}catch(e){console.warn('xframe ask',e)}}
 if(XF.drag&&Date.now()-XF.dragT>3000)XF.drag='';xfAskTick()}
/* the frame placed so that its inside is g (the X screen's pixels), as xfRect reads it */
function xfInside(F,g){const el=F.W.el,k=devicePixelRatio||1,zf=parseFloat(document.documentElement.style.zoom)||1,r=F.W.body.getBoundingClientRect(),i=XF.IN;
 const d=[(g[0]/k-i-r.left)/zf,(g[1]/k-r.top)/zf,(g[2]/k+2*i-r.width)/zf,(g[3]/k+i-r.height)/zf];if(d.every(v=>Math.abs(v)<.5))return;
 const cs=getComputedStyle(el);el.style.left=(el.offsetLeft+d[0])+'px';el.style.top=(el.offsetTop+d[1])+'px';el.style.width=(parseFloat(cs.width)+d[2])+'px';el.style.height=(parseFloat(cs.height)+d[3])+'px'}
function xfAskAt(F,x){if(F.W.el.classList.contains('max')||F.W.min)return;const g=[+x.x,+x.y,+x.w,+x.h];if(!g.every(Number.isFinite))return;
 xfInside(F,g);XF.drag=x.end?'':F.id;XF.dragT=Date.now();
 if(x.end&&typeof MON_ON!='undefined'&&MON_ON&&typeof monDrop=='function')setTimeout(()=>monDrop(F.W),0)}
function xfAskMax(F,act){const W=F.W,on=W.el.classList.contains('max'),want=act==2?!on:act==1;if(want==on||W.min)return;W.focus();W.el.querySelector('.mx').click()}
function xfAskMin(F){if(!F.W.min)F.W.el.querySelector('.mn').click()}
/* asked again soon while an app is shown in its frame (quickly while it is dragged) */
function xfAskTick(){clearTimeout(XF.askT);XF.askT=0;if(!xfOn()||!XF.map.size)return;const sh=XF.map.get(XF.lastX),d=XF.drag?40:sh&&!sh.full?250:0;if(!d)return;
 XF.askT=setTimeout(()=>{XF.askT=0;lxApi('apps/windows').catch(()=>{}).finally(()=>{if(!XF.askT)xfAskTick()})},d)}
