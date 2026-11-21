/* ===== Rudolf OS 1.6.1 (docs/plan-1.6.1.md Part 23 C): idle windows give their memory back (the native desktop) =====
   rudolf-native unloads the page of an app's window that has been minimized or covered for a while (Settings › System, Idle
   windows) and loads it again when the window is shown (rest_tick, appwin_rest, appwin_wake). Before, it asks the page
   (__oaRest): only an app on REST_APPS, which can give back where it was and say whether it has unsaved changes, and only when
   nothing would be lost now: no unsaved change, no dialog, no sound or video playing, no recording or call, no request still
   going (a download, an upload, a transfer: xfer61.js), no job polled lately (installs, updates, a stick being made), not full
   screen. Its state comes back in the address (&rest=JSON): the files open in Notepad's tabs, the current one, the caret and
   the scroll; File Explorer's folder; and every scrolled box of the window. The window's size and place are the shell's (the
   window is never closed). An app that cannot say all of this is not on the list (Oracle, the games and emulators, the
   office apps, Settings, the Terminal...). Nothing here on Standard or Lite. ===== */
const REST={on:!!(ONEAPP&&oaNative()),inflight:0,last:0,job:0,xf:0,ctx:new Set(),pcs:new Set(),tracks:new Set()};
/* (what the page has going: requests in flight, a job's polling, transfers, sound, the camera or microphone, calls) */
if(REST.on){
 /* (a job polled: rudolfd's job (lxJobWait: installs, the Debian apps, a stick, a copy of a drive), an update being installed, a transfer's
    pieces (xfer61.js: fs/copy, fs/move, fs/hash, a piece of fs/write)) */
 const JOB=/\/api\/(job(\?|$)|pkg\/(install|remove|update)|debian\/(install|remove|update)|lx\/(install|remove)|updates?\/(install|apply|download|now)|stick|mkstick|drivecopy|xfer|fs\/(copy|move|hash)|fs\/write\?.*part=|home\/hash)/;
 const seen=u=>{try{if(JOB.test(String(u)))REST.job=Date.now()}catch(e){}};
 const F=window.fetch;window.fetch=function(u){seen(u&&u.url||u);REST.inflight++;const p=F.apply(this,arguments);p.then(()=>REST.inflight--,()=>REST.inflight--);return p};
 const O=XMLHttpRequest.prototype.open,SND=XMLHttpRequest.prototype.send;
 XMLHttpRequest.prototype.open=function(m,u){this.__ru=u;return O.apply(this,arguments)};
 XMLHttpRequest.prototype.send=function(){seen(this.__ru);if(this.__rest)return SND.apply(this,arguments);this.__rest=1;REST.inflight++;let done=0;const end=()=>{if(!done){done=1;REST.inflight--}};
  this.addEventListener('loadend',end);try{return SND.apply(this,arguments)}catch(e){end();throw e}};
 for(const n of ['AudioContext','webkitAudioContext'])if(typeof window[n]=='function'){const C=window[n];window[n]=function(...a){const c=new C(...a);REST.ctx.add(c);return c};window[n].prototype=C.prototype}
 if(typeof RTCPeerConnection=='function'){const C=RTCPeerConnection;window.RTCPeerConnection=function(...a){const c=new C(...a);REST.pcs.add(c);return c};window.RTCPeerConnection.prototype=C.prototype}
 if(navigator.mediaDevices){for(const n of ['getUserMedia','getDisplayMedia'])if(typeof navigator.mediaDevices[n]=='function'){const G=navigator.mediaDevices[n].bind(navigator.mediaDevices);
  navigator.mediaDevices[n]=async(...a)=>{const s=await G(...a);try{s.getTracks().forEach(t=>REST.tracks.add(t))}catch(e){}return s}}}
 if(typeof xfRun=='function'){const X=xfRun;xfRun=async function(){REST.xf++;try{return await X.apply(this,arguments)}finally{REST.xf--}}}
}
/* what keeps the page from resting now ('' nothing) */
function restBlock(){
 if(document.querySelector('body>.rmodal,body>.natdlg'))return 'a dialog';
 if(document.fullscreenElement||document.documentElement.classList.contains('oafs'))return 'full screen';
 if([...document.querySelectorAll('audio,video')].some(m=>!m.paused&&!m.ended))return 'playing sound or video';
 if([...REST.ctx].some(c=>c.state=='running'))return 'playing sound';
 if([...REST.tracks].some(t=>t.readyState=='live'))return 'recording';
 if([...REST.pcs].some(c=>c.connectionState!='closed'&&c.signalingState!='closed'))return 'in a call';
 if(REST.xf>0)return 'a transfer';
 try{if(typeof DLN=='object'&&(DLN.busy||DLN.ing>0||DLN.pend.some(x=>/^(wait|check|approve|run|go)$/.test(x.st))))return 'a download'}catch(e){}
 if(REST.inflight>0)return 'a request still going';
 if(Date.now()-REST.job<20000)return 'a job running';
 return ''}
/* every box of the window scrolled (its place among the window's elements), and back */
function restScroll(W){const L=[],all=[W.body,...W.body.querySelectorAll('*')];all.forEach((e,i)=>{if((e.scrollTop>0||e.scrollLeft>0)&&L.length<24)L.push([i,e.scrollTop,e.scrollLeft])});return L}
function restScrollSet(W,L){if(!Array.isArray(L))return;const all=[W.body,...W.body.querySelectorAll('*')];L.forEach(([i,t,l])=>{const e=all[i];if(e){e.scrollTop=t;e.scrollLeft=l}})}
/* the apps that can rest: save(W) gives their state or {no: why}; ready(W) once their window has drawn; load(W, state) */
const REST_APPS={
 notepad:{save(W){const T=W.dtabs,ta=W.body.querySelector('textarea');if(!T||!ta)return{no:'not ready'};
   for(let k=0;k<T.tabs.length;k++){const t=T.tabs[k],c=k==T.i?ta.value:t.content;if(c!==t.saved)return{no:'unsaved changes'}}
   const tabs=T.tabs.filter(t=>t.p).map(t=>t.p.join('\\')),cur=T.tabs[T.i];
   return{tabs,i:cur&&cur.p?tabs.indexOf(cur.p.join('\\')):-1,ss:ta.selectionStart,se:ta.selectionEnd,top:ta.scrollTop}},
  ready(W){const T=W.dtabs;return !!(T&&W.body.querySelector('textarea')&&(!USER||USER.r=='guest'||(typeof HOME=='object'&&HOME)))},
  load(W,st){const T=W.dtabs,ta=W.body.querySelector('textarea');(st.tabs||[]).forEach(p=>{try{T.open(resolve(String(p),['C:']))}catch(e){}});
   if(st.i>=0&&st.tabs[st.i])try{T.open(resolve(String(st.tabs[st.i]),['C:']))}catch(e){}
   try{ta.setSelectionRange(+st.ss||0,+st.se||0);ta.scrollTop=+st.top||0}catch(e){}}},
 explorer:{save(W){const ad=W.body.querySelector('.ad');if(!ad)return{no:'not ready'};
   if(W.body.querySelectorAll('.xtabs .xtab').length>1)return{no:'several tabs'};if(W.body.querySelector('.xpanes.split'))return{no:'two panes'};
   const p=String(ad.value||'');if(!/^[A-Z]:(\\|$)/.test(p))return{no:'a drive or a folder of the computer'};
   if(typeof FC=='object'&&(FC.op||(FC.items&&FC.items.length)))return{no:'files cut or copied'};return{path:p}},
  ready(W){const ad=W.body.querySelector('.ad');return !!(ad&&ad.value&&W.body.querySelector('.xpanes')&&(!USER||USER.r=='guest'||(typeof HOME=='object'&&HOME)))},
  load(W,st){const ad=W.body.querySelector('.ad');if(!ad||!st.path||ad.value==st.path)return;ad.value=st.path;ad.dispatchEvent(new KeyboardEvent('keydown',{key:'Enter',bubbles:true}))}}};
window.__oaRest=()=>{const J=o=>JSON.stringify(o);try{if(!REST.on)return J({ok:0,why:'not the native desktop'});const A=REST_APPS[ONEAPP.k],W=OA.main;
 if(!A)return J({ok:0,why:'this app cannot be restored'});if(!W||!W.el||!W.el.isConnected||OA.done)return J({ok:0,why:'no window'});
 if(OA.restoring)return J({ok:0,why:'still being restored'});
 const b=restBlock();if(b)return J({ok:0,why:b});const st=A.save(W);if(!st||st.no)return J({ok:0,why:st&&st.no||'cannot be restored'});
 st.sc=restScroll(W);const s=J(st);if(s.length>5500)return J({ok:0,why:'too much to keep'});
 /* (the files and settings written before the page goes, as when it closes) */
 try{fsFlush()}catch(e){}try{window.__rudolfLsFlush&&window.__rudolfLsFlush()}catch(e){}
 return J({ok:1,st:s})}catch(e){return J({ok:0,why:String(e&&e.message||e)})}};
/* (loaded again: its window back where it was, once the app has drawn it) */
if(REST.on&&ONEAPP.q.get('rest')){let st=null;try{st=JSON.parse(ONEAPP.q.get('rest'))}catch(e){}const A=REST_APPS[ONEAPP.k];
 if(st&&A){OA.restoring=1;const t0=Date.now(),go=()=>{const W=OA.main;if(W&&W.el&&W.el.isConnected&&A.ready(W)){try{A.load(W,st)}catch(e){console.warn('rest',e)}
   setTimeout(()=>{try{restScrollSet(W,st.sc)}catch(e){}OA.restoring=0;OA.restored=1},400);return}
  if(Date.now()-t0<15000)setTimeout(go,100);else OA.restoring=0};setTimeout(go,50)}}
/* Settings › System › Idle windows (what the native desktop does; shown on every engine, so Search in Settings finds it): on or off, and after how long; not chosen yet: on when the computer has
   under 6 GB of memory (the shell decides the same way: rudolf-native.c rest_on). Saved at once, as the desktop engine is. */
async function restPanel61(c){if(typeof RD=='undefined'||!RD)return;const nat=RD.engine=='native';const d=document.createElement('div');d.className='rest61';c.appendChild(d);
 let small=true;try{const s=await rd('stats');if(s&&s.memTotal)small=s.memTotal<6*1073741824}catch(e){}
 const on=typeof S.idlerest=='boolean'?S.idlerest:small,min=[5,10,20,30,60].includes(+S.idlemin)?+S.idlemin:10;
 d.innerHTML=`<h4 class='sh4'>Idle windows</h4><div class='r2'><label for='ir61'>Idle windows</label><div><label class='ck'><input type='checkbox' id='ir61'> Give an app's memory back when its window has been minimized or covered for a while</label>
<div style='margin-top:6px'><label for='im61'>After</label> <select id='im61'>${[5,10,20,30,60].map(m=>`<option value='${m}'>${m} minutes</option>`).join('')}</select></div>
<div class='msg'>${nat?'':'On the native desktop (Settings › System health › Desktop engine: Native). '}Notepad and File Explorer only: the app's page is unloaded and loads again where it was when you show the window (its files, folder and scroll). Never with unsaved changes, a dialog, sound or video playing, a call, a download or a transfer. Oracle, Linux, Debian, Flatpak and Wine apps are never touched. ${small?'On by default on this computer (under 6 GB of memory).':'Off by default on this computer (6 GB of memory or more).'}</div></div></div>`;
 const ck=d.querySelector('#ir61'),sel=d.querySelector('#im61');ck.checked=on;sel.value=String(min);sel.disabled=!on;
 const save=()=>{S.idlerest=ck.checked;S.idlemin=+sel.value;if(typeof SC=='object'&&SC){SC.idlerest=S.idlerest;SC.idlemin=S.idlemin}sel.disabled=!ck.checked;try{persistS()}catch(e){console.warn(e)}};
 ck.onchange=save;sel.onchange=save}
{const _s=sysPanelS;sysPanelS=function(c,W){const r=_s.apply(this,arguments);try{restPanel61(c)}catch(e){console.warn(e)}return r}}
