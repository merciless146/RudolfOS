/* ===== Rudolf OS 1.6.1 (docs/plan-1.6.1.md Part 25; the owner, 2026-11-21): progress on a minimized window's taskbar button =====
   "When a window is minimized but has a task ongoing (downloads, page-loading, request sending, etc) can it's taskbar button show an
   inset loading feature that fills the button as if it were a loading bar from the left edge to the right edge? This loading feature
   is green by default but can be set to other colors." For work with no real percentage the owner chose a creeping fill.
   - winProgress(W, v[, src]): v a number 0-100 (a real percentage), 'busy' (no percentage: the fill creeps towards 90 % and completes
     when the work ends) or null (done). src names what reports it ('app' when left out), so several things can report on one window:
     a percentage wins over 'busy', several percentages are averaged.
   - Found by itself while a window is minimized: a <progress> or [role=progressbar] inside its body (with aria-valuenow, or a value:
     that percentage; without: 'busy'; one at 100 % is finished), and an Oracle window loading a page (W.oLoading).
   - The fill: 3 px inside the button's edge, rounded, behind the picture and the title (which stay as they are), half see-through
     (more opaque with high contrast), in every taskbar button style, at the bottom, top, left or right. Only while the window is
     minimized: restoring it hides it, minimizing again shows where the work is now. When the work ends: to 100 %, then it fades.
   - Its colour: Display Properties › Taskbar › Progress colour (S.tbpcol): Green (the default), Blue, Orange, Purple, Red, the accent
     colour or any colour.
   - The native desktop (the owner's rule: as on Standard): each app's window is a page of its own, whose taskbar is rudolf-native's;
     the page tells the shell its windows' progress (oneapp59.js oaPost, op "prog"; Oracle's in Chromium through rudolf-launch, which
     adds its window's place), and the shell draws the same fill while the window is minimized (rudolf-native.c tbp_*). ===== */
Object.assign(DEFS.disp,{tbpcol:'green'});
['tbpcol'].forEach(k=>{if(!DKEYS.includes(k))DKEYS.push(k);if(S[k]===undefined)S[k]=DEFS.disp[k];if(SC&&SC[k]===undefined)SC[k]=cln(S[k])});
const TBP_COLS=[['green','Green','#16a34a'],['blue','Blue','#2563eb'],['orange','Orange','#ea580c'],['purple','Purple','#9333ea'],['red','Red','#dc2626']];
/* the creeping fill: 90 % × (1 − e^(−t/6 s)); the end: 0.3 s to 100 %, then 0.5 s fading (rudolf-native.c draws the same) */
const TBP_MAX=90,TBP_TAU=6000,TBP_FULL=300,TBP_FADE=500;
const TBP={m:new Map(),inp:0,t:0,sent:null,sentAt:0,sendT:0};
function tbpColour(){const v=String(S.tbpcol||'green');if(v=='accent')return S.accent||'#2563eb';const p=TBP_COLS.find(x=>x[0]==v);if(p)return p[2];return /^#[0-9a-f]{6}$/i.test(v)?v.toLowerCase():'#16a34a'}
function tbpApply(){try{document.documentElement.style.setProperty('--tbpc',tbpColour())}catch(e){}}
const tbpCreep=ms=>TBP_MAX*(1-Math.exp(-Math.max(0,ms)/TBP_TAU));
/* what one window shows now: {f: 0-100, end: 0-1 (fading), num} or null */
function tbpNorm(v){if(v=='busy')return'busy';if(v===null||v===undefined||v===false||v==='')return null;const n=+v;return isFinite(n)?Math.max(0,Math.min(100,n)):null}
function winProgress(W,v,src){if(!W||typeof W!='object')return;src=String(src||'app');v=tbpNorm(v);let st=TBP.m.get(W);
 if(v==null){if(!st||!(src in st.src))return;delete st.src[src]}
 else{if(!st){st={src:{},b0:0,end:0,last:0};TBP.m.set(W,st)}if(st.end){st.end=0;st.b0=0}st.src[src]=v}
 tbpKick()}
function tbpState(W){const st=TBP.m.get(W);if(!st)return null;const now=Date.now(),vs=Object.values(st.src);
 if(!vs.length){if(!st.end)st.end=now;const t=now-st.end;if(t>=TBP_FULL+TBP_FADE){TBP.m.delete(W);return null}return{f:100,end:t<TBP_FULL?0:(t-TBP_FULL)/TBP_FADE,num:1,v:null}}
 const nums=vs.filter(x=>x!='busy');
 if(nums.length){st.b0=0;const f=nums.reduce((a,b)=>a+b,0)/nums.length;st.last=f;return{f,end:0,num:1,v:Math.round(f)}}
 if(!st.b0)st.b0=now;return{f:tbpCreep(now-st.b0),end:0,num:0,v:'busy'}}
/* ---- found by itself: progress bars inside a window's body, an Oracle window loading */
function tbpSeen(el,stop){for(let e=el;e&&e!==stop;e=e.parentElement){if(e.hidden||e.getAttribute('aria-hidden')=='true')return false;const d=e.style&&e.style.display;if(d=='none')return false;try{if(getComputedStyle(e).display=='none')return false}catch(x){}}return true}
function tbpScan(W){const b=W&&W.body;if(!b||!b.querySelectorAll)return null;let n=0,sum=0,busy=0;
 b.querySelectorAll('progress,[role=progressbar]').forEach(e=>{if(e.classList.contains('tbpf')||!tbpSeen(e,b))return;let p=null;
  if(e.tagName=='PROGRESS'){if(e.hasAttribute('value')){const mx=+e.max||1;p=(+e.value||0)/mx*100}}
  else if(e.hasAttribute('aria-valuenow')){const lo=+(e.getAttribute('aria-valuemin')||0),hi=+(e.getAttribute('aria-valuemax')||100),v=+e.getAttribute('aria-valuenow');if(isFinite(v)&&hi>lo)p=(v-lo)/(hi-lo)*100}
  if(p==null){busy++;return}if(p>=100)return;n++;sum+=Math.max(0,p)});
 if(n)return sum/n;if(busy)return'busy';
 try{if(typeof W.oLoading=='function'&&W.oLoading())return'busy'}catch(e){}
 return null}
const tbpNative=()=>typeof ONEAPP!='undefined'&&!!ONEAPP&&typeof oaPost=='function';
function tbpAuto(){const ws=typeof wins!='undefined'?wins:[];
 /* (on the native desktop the shell minimizes the page's window, so its page looks every time: the shell draws it only while minimized) */
 const nat=tbpNative();ws.forEach(W=>{if(nat||W.min)winProgress(W,tbpScan(W),'auto');else winProgress(W,null,'auto')})}
/* ---- the window a piece of work belongs to: the one in front when it was asked for (a click or a key in the last 15 s), else none */
addEventListener('pointerdown',()=>{TBP.inp=Date.now()},true);addEventListener('keydown',()=>{TBP.inp=Date.now()},true);
function tbpOwner(){if(Date.now()-TBP.inp>15000)return null;const ws=typeof wins!='undefined'?wins:[];
 if(tbpNative()&&typeof OA=='object'&&OA.main)return OA.main;
 return ws.find(o=>o.el&&o.el.classList.contains('act')&&!o.min&&o.app!='xfer')||null}
/* ---- drawn: the fill inside the button (a span the button's own text may replace: put back) */
function tbpBtnState(b){const ws=typeof wins!='undefined'?wins:[],o=ws.find(x=>x.tb===b);if(!o)return null;
 const reps=b.classList.contains('grp')&&o.app?ws.filter(x=>x.app==o.app&&x.ws==o.ws):[o];
 for(const x of reps){if(!x.min)continue;const s=tbpState(x);if(s)return s}return null}
function tbpPaint(b,s){let f=b.querySelector(':scope > .tbpf');
 if(!s){if(f)f.remove();b.classList.remove('tbpon');return}
 if(!f){f=document.createElement('span');f.className='tbpf';f.setAttribute('aria-hidden','true');b.prepend(f)}
 b.classList.add('tbpon');f.classList.toggle('tbpnum',!!s.num);f.style.width=`calc((100% - 6px) * ${(s.f/100).toFixed(4)})`;f.style.opacity=s.end?String(+(1-s.end).toFixed(3)):''}
function tbpDraw(){const T=document.getElementById('tasks');let live=0;if(T)T.querySelectorAll(':scope > button').forEach(b=>{if(b.classList.contains('lxtb'))return;const s=tbpBtnState(b);if(s&&(!s.num||s.end||s.v==null))live=1;tbpPaint(b,s)});return live}
/* ---- the native desktop: the page's windows together, told to the shell when it changes */
function tbpTell(){if(!tbpNative())return;const ws=typeof wins!='undefined'?wins:[];let n=0,sum=0,busy=0,any=0;
 ws.forEach(W=>{const st=TBP.m.get(W);if(!st)return;const vs=Object.values(st.src);if(!vs.length)return;any=1;vs.forEach(v=>{if(v=='busy')busy++;else{n++;sum+=v}})});
 const v=n?String(Math.round(sum/n)):busy?'busy':'';if(v===TBP.sent)return;
 const now=Date.now();if(v&&TBP.sent&&v!='busy'&&TBP.sent!='busy'&&now-TBP.sentAt<250){clearTimeout(TBP.sendT);TBP.sendT=setTimeout(tbpTell,260);return}
 TBP.sent=v;TBP.sentAt=now;oaPost({op:'prog',v})}
function tbpTick(){clearTimeout(TBP.t);TBP.t=0;
 /* (finished on a window not minimized, or closed: nothing to show) */
 for(const [W,st] of [...TBP.m]){if(!W.el||!W.el.isConnected||!Object.keys(st.src).length&&!W.min)TBP.m.delete(W)}
 let live=0;try{live=tbpDraw()}catch(e){}try{tbpTell()}catch(e){}if(TBP.m.size)TBP.t=setTimeout(tbpTick,live?100:400)}
function tbpKick(){clearTimeout(TBP.t);TBP.t=setTimeout(tbpTick,0)}
setInterval(()=>{try{tbpAuto()}catch(e){}},1000);
{const T=document.getElementById('tasks');if(T)new MutationObserver(()=>{if(TBP.m.size)tbpKick()}).observe(T,{childList:true,subtree:true})}
/* restored or minimized: at once (base_v24's focus and minimize) */
document.addEventListener('click',e=>{if(e.target&&e.target.closest&&e.target.closest('#bar,.win .tb'))setTimeout(tbpKick,0)},true);
{const st=document.createElement('style');st.textContent=`
#bar #tasks button.tbpon,.mbar [id^=mtasks] button.tbpon{position:relative;isolation:isolate}
.tbpf{position:absolute;left:3px;top:3px;bottom:3px;width:0;border-radius:3px;background:color-mix(in srgb,var(--tbpc,#16a34a) 50%,transparent);z-index:-1;pointer-events:none;display:block;max-width:calc(100% - 6px)}
html.hc .tbpf{background:color-mix(in srgb,var(--tbpc,#16a34a) 65%,transparent)}
.tbpf.tbpnum{transition:width .35s linear}
html.rmotion .tbpf{transition:none!important}
.tbprow{display:flex;flex-wrap:wrap;gap:6px 10px;align-items:center}.tbprow input[type=color]{width:42px;height:26px;padding:0 2px}`;document.head.appendChild(st)}
themeApply=__after(themeApply,function(){tbpApply()},'tbprog61.js');
/* Display Properties › Taskbar: Progress colour (a step of Display Properties' list: hooks61.js, Part 24) */
(function(){dispPanelS=__after(dispPanelS,function(c,W){const tg=c.querySelector('.tbgrp');if(!tg||c.querySelector('#dtbpc'))return;
 const cur=String(S.tbpcol||'green'),custom=!TBP_COLS.some(x=>x[0]==cur)&&cur!='accent';
 const l=document.createElement('label');l.htmlFor='dtbpc';l.textContent='Progress colour';
 const d=document.createElement('div');d.innerHTML=`<div class='tbprow'><select id='dtbpc'>${TBP_COLS.map(([k,n])=>optH(k,k=='green'?n+' (as it comes)':n,custom?'':cur)).join('')}${optH('accent','The accent colour',cur)}${optH('custom','Any colour…',custom?'custom':'')}</select> <input type='color' id='dtbpcc' aria-label='The colour'></div>
  <div class='msg'>The fill on a minimized window's taskbar button while it is doing something (a download, a page loading, a copy, an installation).</div>`;
 tg.append(l,d);const s=d.querySelector('#dtbpc'),cc=d.querySelector('#dtbpcc');cc.value=custom?cur.toLowerCase():tbpColour();
 const sync=()=>{cc.hidden=s.value!='custom'};
 s.onchange=()=>{if(s.value=='custom'){cc.hidden=false;liveSet(c,'tbpcol',cc.value)}else{liveSet(c,'tbpcol',s.value);cc.value=tbpColour()}sync();tbpApply()};
 cc.oninput=()=>{if(s.value!='custom')s.value='custom';liveSet(c,'tbpcol',cc.value);sync();tbpApply()};sync()},'tbprog61.js')})();
/* ---- the sources: the work each module does, on the window it belongs to (the rest find their own <progress>: tbpScan) */
/* the File Explorer copies and moves (xfer61.js xfJob): on the window they were asked in; the progress window is found by its bar */
if(typeof xfJob=='function'){const o=xfJob;xfJob=function(kind,n,opt){const j=o.apply(this,arguments),W=tbpOwner(),src='xfer'+Math.random().toString(36).slice(2,7);if(!W)return j;
 const tick=()=>{if(j.end||!W.el||!W.el.isConnected){clearInterval(t);winProgress(W,null,src);return}winProgress(W,j.total?j.bytes/j.total*100:j.N?j.i/j.N*100:'busy',src)};
 const t=setInterval(tick,400);const f=j.finish;j.finish=function(){const r=f.apply(this,arguments);tick();return r};tick();return j}}
/* the jobs rudolfd runs for a window (the App Store's and Packages' installs and updates, Linux apps, system updates, extensions): no
   percentage, so the creeping fill, on the window that asked; one started by itself (the daily update check) has none */
function tbpJob(o,k){return async function(){const W=tbpOwner(),src=k+Math.random().toString(36).slice(2,7);if(W)winProgress(W,'busy',src);try{return await o.apply(this,arguments)}finally{if(W)winProgress(W,null,src)}}}
if(typeof pkgJob=='function')pkgJob=tbpJob(pkgJob,'pkg');
if(typeof sysuJob=='function')sysuJob=tbpJob(sysuJob,'sysu');
if(typeof sxJobWait=='function')sxJobWait=tbpJob(sxJobWait,'sx');
if(typeof lxJobWait=='function')lxJobWait=tbpJob(lxJobWait,'lx');
/* a text message being sent (messages60.js): on the Messages window */
if(typeof msgSend=='function'){const o=msgSend;msgSend=async function(){const W=typeof MSG=='object'&&MSG.W;if(W)winProgress(W,'busy','sms');try{return await o.apply(this,arguments)}finally{if(W)winProgress(W,null,'sms')}}}
/* a document being printed (print25.js): from Print until the queue has it, on the window that printed it */
if(typeof prJSON=='function'){const o=prJSON;prJSON=async function(p){const W=/^print\/job/.test(String(p||''))?tbpOwner():null;if(W)winProgress(W,'busy','print');try{return await o.apply(this,arguments)}finally{if(W)winProgress(W,null,'print')}}}
if(typeof prRaster=='function'){const o=prRaster;prRaster=async function(){const W=tbpOwner();if(W)winProgress(W,'busy','print');try{return await o.apply(this,arguments)}finally{if(W)winProgress(W,null,'print')}}}
/* the downloads (dl27.js): Oracle's, on the Oracle window they were started in (the one in front then), else Oracle's last used one;
   a percentage when every one arriving says its size, else the creeping fill */
const TBP_DLW=new WeakMap();
if(typeof dlNative=='function'){const o=dlNative;dlNative=function(){const r=o.apply(this,arguments);try{const W=tbpOwner();if(W&&r)TBP_DLW.set(r,W)}catch(e){}return r}}
setInterval(()=>{if(typeof DLN!='object'||!DLN||!Array.isArray(DLN.pend))return;const ws=typeof wins!='undefined'?wins:[],ora=ws.filter(x=>x.app=='browser');
 const by=new Map();DLN.pend.forEach(r=>{if(r.st!='wait'&&r.st!='check'&&r.st!='approve')return;let W=TBP_DLW.get(r);if(!W||!ws.includes(W))W=(typeof MRU!='undefined'&&MRU.find(x=>x.app=='browser'))||ora.at(-1)||(tbpNative()&&typeof OA=='object'?OA.main:null);if(!W)return;
  if(!by.has(W))by.set(W,[]);by.get(W).push(r)});
 ws.forEach(W=>{const rs=by.get(W);if(!rs){winProgress(W,null,'dl');return}const all=rs.every(r=>r.st=='wait'&&+r.tot>0);
  winProgress(W,all?rs.reduce((a,r)=>a+Math.min(+r.sz||0,+r.tot),0)/rs.reduce((a,r)=>a+(+r.tot),0)*100:'busy','dl')})},1000);
/* (the native desktop's idle windows, rest61.js: a window with work in progress does not rest) */
if(typeof restBlock=='function'){const o=restBlock;restBlock=function(){const r=o.apply(this,arguments);if(r)return r;for(const st of TBP.m.values())if(Object.keys(st.src).length)return 'work in progress';return''}}
tbpApply();
