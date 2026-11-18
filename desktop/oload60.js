/* ===== 1.6.0 (the owner, 2026-11-18): Oracle's loading line =====
   "Can Oracle have the LitePoint line/dot animation moving from left to right at the bottom of its bookmarks bar as a
   loading progress visual?" ... "It will go into 1.6 in the same beta build". Along the bottom edge of the bookmarks bar
   (.obm; the address bar's row when the bookmarks bar is hidden), for the tab in front: LitePoint's point of light moves
   left to right as the page loads, the line lit behind it. Its progress is the page's own where the browser tells it (the
   shell's tabev "progress", from the Controlled Frame's loadprogress), otherwise an estimate that slows as it nears the
   end. Loaded: the point runs to the right end and the line fades; stopped or failed: it fades where it stopped.
   LitePoint's own colour and brightness (lpCols, lpBr, lpPaint); Reduce motion (lpOff): a plain line that only fills.
   Started and ended by Oracle's own status line ("Loading…" / anything else), so frames (Lite, files, about: pages) and
   real tabs (the shell) are the same; on Standard and Native alike (the same page). */
const OLP={on:new Set(),raf:0};
function olpState(W){if(!W.__olp){const host=W.body;const c=document.createElement('canvas');c.className='olp';c.setAttribute('aria-hidden','true');
  W.__olp={c,host,p:0,shown:0,target:0,t0:0,state:'idle',fade:1,real:-1,err:false}}return W.__olp}
/* where it is drawn: the bottom edge of the bookmarks bar, or of the address bar's row */
function olpPlace(W){const L=W.__olp,b=W.body.querySelector('.obm'),bar=b&&b.offsetHeight>2&&getComputedStyle(b).display!='none'?b:W.body.querySelector('.ob');if(!bar)return false;
 if(L.c.parentElement!==bar){if(getComputedStyle(bar).position=='static')bar.style.position='relative';bar.appendChild(L.c)}
 const w=bar.clientWidth,h=16,d=Math.min(2,devicePixelRatio||1);L.c.style.cssText=`position:absolute;left:0;bottom:-8px;width:${w}px;height:${h}px;pointer-events:none;z-index:3`;
 if(L.c.width!=Math.round(w*d)||L.c.height!=Math.round(h*d)){L.c.width=Math.round(w*d);L.c.height=Math.round(h*d)}const x=L.c.getContext('2d');x.setTransform(d,0,0,d,0,0);return{x,w,h}}
function olpStart(W){const L=olpState(W);L.state='loading';L.t0=performance.now();L.shown=0;L.real=-1;L.err=false;L.fade=1;OLP.on.add(W);olpGo()}
function olpEnd(W,err){const L=W.__olp;if(!L||L.state!='loading')return;L.state=err?'failed':'done';L.err=!!err;L.tEnd=performance.now();olpGo()}
/* the page's own progress (0..1), where the browser tells it */
function olpProgress(W,p){const L=W.__olp;if(!L||L.state!='loading'||!isFinite(p))return;L.real=Math.max(L.real,Math.min(1,Math.max(0,p)))}
/* LitePoint's colours; on a light bookmarks bar (a light theme), white would not show: the accent colour instead */
function olpCols(W,col){const L=W.__olp,bar=L.c.parentElement;if(!bar)return col;if(L.bgAt!==bar){L.bgAt=bar;let c=getComputedStyle(bar).backgroundColor,m=/rgba?\(([\d.]+),\s*([\d.]+),\s*([\d.]+)(?:,\s*([\d.]+))?/.exec(c);
  if(m&&m[4]!==undefined&&+m[4]<.3){const b=getComputedStyle(W.body).backgroundColor;m=/rgba?\(([\d.]+),\s*([\d.]+),\s*([\d.]+)/.exec(b)||m}
  L.light=!!m&&(.2126*m[1]+.7152*m[2]+.0722*m[3])/255>.62}
 if(!L.light)return col;const hex=String(S.accent||'#2563eb');const v=/^#([0-9a-f]{6})$/i.test(hex)?hex:'#2563eb',g=[1,3,5].map(i=>parseInt(v.slice(i,i+2),16));return{glow:g,core:v}}
function olpGo(){if(!OLP.raf)OLP.raf=requestAnimationFrame(olpFrame)}
function olpFrame(now){OLP.raf=0;const col=typeof lpCols=='function'?lpCols():{glow:[205,228,255],core:'#f2f7ff'},br=typeof lpBr=='function'?lpBr():1,plain=typeof lpOff=='function'&&lpOff();
 for(const W of[...OLP.on]){const L=W.__olp;if(!W.el||!W.el.isConnected){OLP.on.delete(W);continue}const P=olpPlace(W);if(!P){continue}const{x,w,h}=P;x.clearRect(0,0,w,h);
  /* where it should be: the page's own progress, or an estimate (half in 2 s, slowing toward 90 %) */
  const est=.9*(1-Math.exp(-(now-L.t0)/2900));
  if(L.state=='loading')L.target=Math.max(L.real>=0?L.real:0,L.real>=0?Math.min(L.real+.05,.97):est);
  else if(L.state=='done')L.target=1;
  L.shown+=plain?(L.target-L.shown):(L.target-L.shown)*(L.state=='done'?.22:.12);if(Math.abs(L.target-L.shown)<.002)L.shown=L.target;
  if(L.state!='loading'&&(L.state=='failed'||L.shown>=.999)){L.fade-=plain?.2:.06;if(L.fade<=0){L.state='idle';L.c.remove();OLP.on.delete(W);continue}}
  const X=Math.max(0,Math.min(w,w*L.shown)),y=h/2,a=L.fade*(L.err?.8:1),c2=olpCols(W,col);
  if(plain){x.save();x.globalAlpha=a*Math.max(.35,br);x.fillStyle=c2.core;x.fillRect(0,y-1,X,2);x.restore();continue}
  if(typeof lpPaint=='function'&&br>0){x.save();x.globalAlpha=a;lpPaint(x,{L:[[0,y,X,y,1]],P:L.state=='loading'||L.shown<.999?[[X,y,.55,L.err?1:0]]:[]},c2,br);x.restore()}
  else{x.save();x.globalAlpha=a;x.fillStyle=c2.core;x.fillRect(0,y-1,X,2);x.restore()}}
 if(OLP.on.size)OLP.raf=requestAnimationFrame(olpFrame)}
/* Oracle's windows (browser() returns nothing: the new ones are found after it): its status line says when the tab in
   front starts and stops loading */
function olpHook(W){if(!W||W.__olpHook||!W.body)return;const st=W.body.querySelector('.stat');if(!st||!W.body.querySelector('.obm'))return;W.__olpHook=1;
 let was=false;const look=()=>{const t=st.textContent||'',on=/^Loading/.test(t);if(on&&!was)olpStart(W);else if(!on&&was)olpEnd(W,/could not be loaded|stopped working|Blocked/.test(t));was=on};
 new MutationObserver(look).observe(st,{childList:true,characterData:true,subtree:true});look()}
{const _b=browser;browser=function(){const r=_b.apply(this,arguments);try{wins.forEach(olpHook)}catch(e){console.warn(e)}return r}}
/* the real tabs' own progress (the shell): its tab's area is in the Oracle window it belongs to */
addEventListener('message',e=>{if(typeof SHELL=='undefined'||!SHELL||e.source!==SHELL)return;const m=e.data;if(!m||m.rudolfShell!='tabev'||m.ev!='progress'||typeof RT=='undefined')return;
 const t=RT.get(m.tid),el=t&&t.el,w=el&&el.closest&&el.closest('.win');if(!w||!el.offsetParent)return;const W=wins.find(x=>x.el===w);if(W)olpProgress(W,+m.p)});
