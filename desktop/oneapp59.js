/* ===== Rudolf OS 1.5.9 (plan Part 12, step 1): the page's one-app mode =====
   index.html#app=NAME (an app's key, such as notepad or settings; &file=C:\PATH opens a file with it): the page starts
   that app alone, its window filling the page, without the taskbar, the desktop's icons, widgets or wallpaper. The
   native desktop engine (rudolf-native) shows each app this way, in a WebKit window of its own with its own frame.
   - Signing in: with a native session (rudolfd's desktop/session: the account rudolf-native signed in, with its keys)
     there is no sign-in screen; without one (the page opened by hand in a browser) the usual sign-in, then the app.
   - The app's window has no title bar of its own (the native frame has one); its title is the page's title.
   - Closing it (its ✕, or the native frame's ✕ through window.__oneAppClose(), which asks the app as its ✕ does, so
     unsaved work is asked about) writes the files and settings first, then closes the page; where a script cannot close
     it (a browser tab) the page says "This app is closed", with Open again.
   - Another app asked for opens in a window of its own (a rudolfNative message to the native shell); in a browser it
     opens in the same page, as before.
   - (1.6.0 Part 12) #app=do-NAME: the start menu's other rows (Run..., the Programs folder, Emu, Blitzen): the page's own
     action for that row, whose first window is the main window, as an app's.
   - The windows of the last session are not reopened (each app window would reopen them), and only Settings writes the
     accounts (all app windows share the same storage). ===== */
const ONEAPP=(()=>{try{const h=String(location.hash||'').replace(/^#/,'');if(!h)return null;const q=new URLSearchParams(h),k=q.get('app');
 return k&&/^[a-z0-9_:.+-]{1,127}$/i.test(k)?{k,file:q.get('file')||'',w:+q.get('w')||0,q}:null}catch(e){return null}})();   /* (2026-11-16: also the Library's, Linux's and Windows' programs: pkg:NAME, lx:ID.desktop, lx:deb-ID) */
const OA={main:null,want:0,opened:0,done:0,login:0};
let OA_FSIN=0;   /* (the files being read again because another page saved them: oaFsReload) */
/* (1.6.0 Part 12, item 12) the native desktop's service page (#app=do-svc, svc60.js): the background work of the whole desktop, never shown */
const OA_SVC=!!(ONEAPP&&ONEAPP.k=='do-svc');
/* (the sign-in screen hidden from the first paint in an app's window: shown again, base_v24.html's head) */
function oaLoginUnhide(){try{document.documentElement.classList.remove('oalg')}catch(e){}}
if(!ONEAPP)oaLoginUnhide();
/* (1.6.0 Part 12, item 1) an Explorer window's files dragged into another window when the windows are pages of their own: FC.drag
   is the page's own, so it goes with the drag as text/x-rudolf-paths and is taken back on the drop (a desktop file dropped on a
   window: oaFs's op=drop) */
if(ONEAPP){
 const T='text/x-rudolf-paths',ours=e=>e.dataTransfer&&[...e.dataTransfer.types].includes(T);let lent=0;
 document.addEventListener('dragstart',e=>{try{if(typeof FC=='object'&&FC.drag&&FC.drag.length&&e.dataTransfer)e.dataTransfer.setData(T,JSON.stringify(FC.drag))}catch(x){}});
 document.addEventListener('dragenter',e=>{if(!ours(e)||typeof FC!='object')return;if(!lent&&!FC.drag){FC.drag=[];lent=1}else if(lent)lent++},true);
 document.addEventListener('dragover',e=>{if(lent&&ours(e))try{e.dataTransfer.dropEffect=e.ctrlKey?'copy':'move'}catch(x){}},true);   /* (WebKit gives a drag from another page none) */
 document.addEventListener('dragleave',e=>{if(lent&&--lent==0&&FC.drag&&!FC.drag.length)FC.drag=null},true);
 document.addEventListener('drop',e=>{if(lent&&ours(e)){lent=0;try{FC.drag=JSON.parse(e.dataTransfer.getData(T))}catch(x){FC.drag=null}}},true);
 document.addEventListener('dragend',()=>{if(lent){lent=0;if(FC.drag&&!FC.drag.length)FC.drag=null}});
 /* a file dropped on this app's window: in a window of its own, as on Standard (the shell asks a page to do it: op=drop) */
 const _dfw=window.__dropFileOnWindow;if(_dfw)window.__dropFileOnWindow=function(W,path){
  if(W&&W===OA.main&&oaNative()&&W.app&&W.app!='explorer'&&W.app!='trash'){if(node(path)==null)return false;oaPost({op:'drop',file:path.join('\\'),in:String(W.app)});return true}
  return _dfw.apply(this,arguments)}}
/* (1.6.0 Part 12, item 1) the files are one: on Standard every window is one page with one file system; here each window is a
   page of its own, so a page that saved tells the others, which read the files again (the system's and the account's home) and
   show them as they are now (EXPL) - or a window would show, and save back, files another window changed */
if(ONEAPP&&typeof BroadcastChannel=='function'){const FSB=new BroadcastChannel('rudolf-fs'),ME=Math.random().toString(36).slice(2);
 const tell=()=>{try{homeChain.then(()=>FSB.postMessage({from:ME,id:typeof uid=='function'&&USER?uid():''}))}catch(e){}
  /* (1.5.9, 12.3c) and the other engine's windows: Oracle's page is in Chromium, the apps' in WebKit, each with storage of its own;
     once rudolfd's mirror has the files, the native desktop tells the others, which take them from there (__oaMirrorIn) */
  try{if(oaOut())oaDrained().then(()=>oaPost({op:'mirror',id:typeof uid=='function'&&USER?uid():''}))}catch(e){}};
 const _ff=fsFlush;fsFlush=function(){const d=fsDirty;const r=_ff.apply(this,arguments);if(d)setTimeout(tell,60);return r};
 let busy=null;FSB.onmessage=e=>{const m=e.data||{};if(m.from==ME||!USER||!IDB||!IDB.db||!FSLOADED||(m.id&&m.id!=uid()))return;busy=(busy||Promise.resolve()).then(oaFsReload)};
 async function oaFsReload(){try{
  const sys=await kvGet('fs:system');
  if(sys&&isDir(sys['C:'])){const C=fs['C:'],S=sys['C:'];for(const k of Object.keys(C))if(k!='USERS'&&!(k in S))delete C[k];for(const k in S)if(k!='USERS')C[k]=S[k];
   for(const k of Object.keys(fs))if(k!='C:'&&!(k in sys))delete fs[k];for(const k in sys)if(k!='C:')fs[k]=sys[k]}
  if(USER.r!='guest'&&HOME&&!HOMELOCKED){const rec=await kvGet('home:'+uid());let o=null;
   if(rec&&rec.ct&&KEY)o=JSON.parse(new TextDecoder().decode(await crypto.subtle.decrypt({name:'AES-GCM',iv:rec.iv},rec.k=='f'?KEY:(PKEY||KEY),rec.ct)));else if(rec&&rec.plain)o=JSON.parse(rec.plain);
   if(o&&isDir(o)){for(const k of Object.keys(HOME))delete HOME[k];Object.assign(HOME,o)}}
  OA_FSIN=1;try{EXPL.forEach(f=>{try{f()}catch(x){}})}finally{OA_FSIN=0}}catch(e){console.warn('one-app: the files again',e)}}
 /* (1.5.9, 12.3c) the files another engine's window saved: the system's and this account's records from rudolfd's mirror into this
    engine's storage (not sent back to the mirror), then read again as above */
 window.__oaMirrorIn=id=>{if(!USER||!IDB||!IDB.db||!FSLOADED||(id&&id!=uid())||typeof RD=='undefined'||!RD)return;busy=(busy||Promise.resolve()).then(async()=>{
  for(const k of ['fs:system','home:'+uid()])try{const r=await fetch('/api/idbm/get?f='+idbmName('rudolfos','kv',k),{headers:{'X-Rudolf-Token':RD.token},cache:'no-store'});if(!r.ok)continue;
   const j=await r.json();if(j.db!='rudolfos'||j.store!='kv'||j.key!==k)continue;const v=await idbmDec(j.v);await idbmRaw('rudolfos','kv','readwrite',st=>st.put(v,k))}catch(e){console.warn('one-app: the mirror',k,e)}
  await oaFsReload()})}}
/* (1.5.9, 12.3c) the files written and rudolfd's mirror up to date (before another engine's window is told, or opens a file) */
async function oaDrained(){try{fsFlush()}catch(e){}try{await homeChain}catch(e){}
 for(let i=0;i<100&&typeof IDBM=='object'&&(IDBM.busy||IDBM.queue.size);i++)await new Promise(r=>setTimeout(r,100))}
/* (1.6.0 Part 12, item 3) the settings are one: on Standard every window is one page with one S; here a page that saved its settings
   (Settings, or the native shell's quick settings through a page of their own) changes the others too, as on Standard (WebKit tells
   the other pages of a change to the storage they share) */
/* (1.6.0, the owner's report 3: Settings' display choices did not reach the native desktop and the other windows at once, and a window
   could put older settings back) Each window keeps what it last took (OA_SB, each saved setting as JSON): what differs from it is this
   window's own change; when it saves, the others' newer choices are taken in first, so it never writes back what another window
   changed. A save is told to the native shell at once (op "set", after the page's storage is in rudolfd's copy, which the shell
   draws from), and the shell tells every other window (__oaSetIn) and Oracle's pages, as WebKit's storage events may not cross
   between the windows' processes. */
let OA_SB=null;
const oaSnap=()=>{try{const o=SC||S,m={};for(const k in o)m[k]=JSON.stringify(o[k]);OA_SB=m}catch(e){}};
const oaOwn=()=>{const o=SC||S,r=new Set();if(OA_SB)for(const k in o)if(JSON.stringify(o[k])!==OA_SB[k])r.add(k);if(SC)for(const k in S)if(JSON.stringify(S[k])!==JSON.stringify(SC[k]))r.add(k);return r};
/* the saved settings us (another window's) taken in, this window's own unsaved changes kept; true when anything changed */
function oaSetTake(us){if(!us||typeof us!='object')return false;const own=oaOwn();let ch=false;
 for(const k in us){if(own.has(k))continue;const v=JSON.stringify(us[k]);if(JSON.stringify((SC||S)[k])===v&&JSON.stringify(S[k])===v)continue;S[k]=cln(us[k]);if(SC)SC[k]=cln(us[k]);ch=true}
 oaSnap();for(const k of own)if(OA_SB)OA_SB[k]=JSON.stringify(us[k]);return ch}
/* (applying what another window saved: applySet saves too, which here would only send the same settings back to rudolfd and tell the
   shell again, and the shell every window again: in QEMU dark mode took 95 s to settle with the windows echoing it) */
let OA_IN=0;
function oaSetApply(){OA_IN++;try{try{applySet()}catch(x){}try{a11yApply()}catch(x){}try{themeApply()}catch(x){}try{ncBadge()}catch(x){}}finally{OA_IN--}}
const oaSetMine=()=>{try{return USER?JSON.parse(localStorage.getItem('bd95set:'+uid())||'null'):null}catch(e){return null}};
if(ONEAPP){addEventListener('storage',e=>{try{if(!USER||e.key!='bd95set:'+uid()||!e.newValue)return;if(oaSetTake(JSON.parse(e.newValue)))oaSetApply()}catch(x){console.warn('one-app: the settings again',x)}});
 /* (told by the native shell that another window saved: from the storage this window shares, or, in Oracle's Chromium, from rudolfd's
    copy of the WebKit windows' storage) */
 window.__oaSetIn=async id=>{try{if(!USER||(id&&id!=uid()))return;let us=null;
   /* (rudolfd's copy first, which the saving window sent before the shell was told: WebKit's storage, shared by the windows, may
      reach this window's process seconds later (the release in QEMU: 3 to 6 s); taken in quietly, so a save here never writes
      the older copy back) */
   try{if(typeof RD!='undefined'&&RD){const r=await fetch('/api/lsync?key='+encodeURIComponent('bd95set:'+uid()),{headers:{'X-Rudolf-Token':RD.token},cache:'no-store'});const j=r.ok?await r.json():null;
    if(j&&typeof j.v=='string'){us=JSON.parse(j.v);try{window.__rudolfLsQuiet?__rudolfLsQuiet('bd95set:'+uid(),j.v):localStorage.setItem('bd95set:'+uid(),j.v)}catch(e){}}}}catch(e){}
   if(!us&&oaNative())us=oaSetMine();
   if(us&&oaSetTake(us))oaSetApply()}catch(e){console.warn('one-app: the settings from the shell',e)}};
 {let t=0;const tell=()=>{clearTimeout(t);t=setTimeout(async()=>{try{if(window.__rudolfLsFlush)await window.__rudolfLsFlush()}catch(e){}if(USER&&oaOut())oaPost({op:'set',id:uid()})},120)};
  const _ps=persistS;persistS=function(){if(OA_IN){oaSnap();return}try{if(USER&&OA_SB){const cur=oaSetMine();if(cur&&oaSetTake(cur))setTimeout(oaSetApply,0)}}catch(e){}
   const r=_ps.apply(this,arguments);oaSnap();tell();return r}}
 {const _ul=userLogin;userLogin=async function(){const r=await _ul.apply(this,arguments);oaSnap();return r}}}
/* (1.6.0 Part 12, item 3) a notification's pop-up is the native shell's: shown in the screen's corner as on Standard, not in this window
   (its click: the notification's own action, run here, __oaToastAct) */
if(ONEAPP&&window.__notifCentre){const NA=window.__notifCentre.add,ACTS={};window.__oaToastAct=id=>{const f=ACTS[id];delete ACTS[id];if(f)try{f()}catch(e){console.warn(e)}};
 window.__notifCentre.add=function(title,body,opt){const H=document.getElementById('ntoasts'),k=H?H.children.length:0,n=NA.apply(this,arguments);
  try{if(oaOut()&&!(opt&&opt.quiet))oaPost({op:'note',t:n.t,b:n.b,k:n.k,at:String(n.at)})}catch(e){}   /* (in the centre at once, and counted on its button) */
  try{const h=document.getElementById('ntoasts');if(oaOut()&&h&&h.children.length>k){const d=h.lastElementChild;d.remove();const id=String(n.id);if(n.act)ACTS[id]=n.act;
   oaPost({op:'toast',t:n.t,b:n.b,k:n.k,id,ms:String((opt&&opt.toast)||5000),why:d.querySelector('.bzwhy')?'1':'0',act:n.act?'1':'0'})}}catch(e){console.warn(e)}return n}}
/* (an app's own window is not the desktop: its icons' grid is the screen's, not the window's, or every window opened, or a page never
   shown, as do-desk's, would squeeze the desktop's places into its own size and keep them) */
if(ONEAPP&&typeof gridInfo=='function'){const _gi=gridInfo;gridInfo=function(){const g=_gi();try{const q=ONEAPP.q,zf=parseFloat(document.documentElement.style.zoom)||1,top=document.body.classList.contains('top'),
 sw=+q.get('sw')/(devicePixelRatio||1)||screen.width,sh=+q.get('sh')/(devicePixelRatio||1)||screen.height,W=sw/zf-16,H=sh/zf-(top?48:42)-16;
 if(W>0&&H>0){g.nc=Math.max(1,Math.floor((W-8)/g.w));g.nr=Math.max(1,Math.floor((H-8)/g.h))}}catch(e){}return g}}
const oaNative=()=>!!(window.webkit&&window.webkit.messageHandlers&&window.webkit.messageHandlers.rudolfNative);
/* (w: which of the shell's windows this is, from #app=NAME&w=N: WebKit gives every window the native shell opens the same
   message handler, so the shell knows the sender by it) */
function oaPost(m){try{if(oaNative())window.webkit.messageHandlers.rudolfNative.postMessage(Object.assign({w:ONEAPP?ONEAPP.w:0},m));else oaShell(m)}catch(e){}}
/* (1.6.0, the owner's computer check on the Vivobook) a page that wants every key for a moment (the check's keyboard step, Settings ›
   Keyboard's Press keys…): on Standard the page's own handler sees them all first; on Native the shell's global keys are grabbed, so it
   is asked to let them through while this window is in front (op kbcap). Nothing elsewhere. */
/* (1.6.0) a folder opened in File Explorer from an app's window: Explorer's own window, at that folder (#app=explorer&file=) */
function oaOpenFolder(dir){if(!(ONEAPP&&oaOut()&&OA.main))return false;oaDrained().then(()=>oaPost({op:'open',app:'explorer',file:dir.join('\\')}));return true}
/* (1.6.0) the screen's size in the page's pixels (the system report's Desktop size; on Standard the page is the whole screen) */
function oaDeskSize(){if(!ONEAPP)return null;const q=ONEAPP.q,d=devicePixelRatio||1,w=+q.get('sw')/d||screen.width,h=+q.get('sh')/d||screen.height;return[Math.round(w),Math.round(h)]}
function oaKeyCap(on){if(ONEAPP&&oaNative()){window.__oaKbcap=!!on;oaPost({op:'kbcap',on:on?'1':'0'})}}
/* (1.6.0) WebKitGTK names the Super keys as an old draft did (code "OSLeft"/"OSRight", key "Super", keyCode 0, metaKey false); Chromium,
   as the standard has it, "MetaLeft"/"MetaRight", "Meta", 91/92 and metaKey while it is held: the check's ❖ never lit up, and a page's
   Meta+key never matched. Given as Chromium gives them in Native's windows. */
if(ONEAPP&&oaNative()&&window.KeyboardEvent){const P=KeyboardEvent.prototype,d=k=>Object.getOwnPropertyDescriptor(P,k),dc=d('code'),dk=d('key'),dkc=d('keyCode'),dw=d('which'),dm=d('metaKey'),dgm=P.getModifierState;let held=0;
 const os=e=>{const c=dc.get.call(e);return c=='OSLeft'?1:c=='OSRight'?2:0};
 addEventListener('keydown',e=>{const o=os(e);if(o)held|=o},true);addEventListener('keyup',e=>{const o=os(e);if(o)held&=~o},true);addEventListener('blur',()=>{held=0});
 if(dc&&dc.get)Object.defineProperty(P,'code',{configurable:true,enumerable:dc.enumerable,get(){const c=dc.get.call(this);return c=='OSLeft'?'MetaLeft':c=='OSRight'?'MetaRight':c}});
 if(dk&&dk.get)Object.defineProperty(P,'key',{configurable:true,enumerable:dk.enumerable,get(){const k=dk.get.call(this);return k=='Super'||k=='OS'?'Meta':k}});
 for(const [n,dd] of [['keyCode',dkc],['which',dw]])if(dd&&dd.get)Object.defineProperty(P,n,{configurable:true,enumerable:dd.enumerable,get(){const v=dd.get.call(this),o=os(this);return o&&!v?(o==1?91:92):v}});
 if(dm&&dm.get)Object.defineProperty(P,'metaKey',{configurable:true,enumerable:dm.enumerable,get(){const o=os(this);return dm.get.call(this)||(o?this.type=='keydown':!!held)}});
 if(dgm)P.getModifierState=function(k){if(k=='Meta'||k=='OS'){const o=os(this);if(o?this.type=='keydown':held)return true}return dgm.call(this,k)}}
/* (1.5.9, docs/plan-1.5.9.md 12.3c) Oracle on the native desktop is this page in the shell, in Rudolf's Chromium (its real tabs):
   the window's title and its closing go to the shell, which is the window (a title kept until the shell has said hello) */
let OA_SHT='';function oaShell(m){if(!ONEAPP)return;if(m.op=='title'){OA_SHT=m.t;if(typeof SHELL!='undefined'&&SHELL)shellSend({rudolfShell:'oa',op:'title',t:m.t});oaTitleNative()}
 /* (closing: rudolf-launch decides, as Chromium asks "Download is in progress: exit anyway?" when its last window closes during a
    download, which Standard never shows: then the window goes from the screen and its page closes once the downloads are in) */
 else if(m.op=='close'&&typeof SHELL!='undefined'&&SHELL){if(oaChromium()){try{window.rudolfNativeOracle(JSON.stringify({w:ONEAPP.w||0,op:'closing'}))}catch(e){}
  clearTimeout(OA_CLT);OA_CLT=setTimeout(()=>window.__oaCloseNow(),15000)}else shellSend({rudolfShell:'oa',op:'close'})}
 /* (12.3c step 5) the rest goes to the native desktop itself: rudolf-launch's binding on its private pipe, in Oracle's windows only */
 else if(oaChromium())try{window.rudolfNativeOracle(JSON.stringify(Object.assign({w:ONEAPP.w||0},m)))}catch(e){}}
function oaChromium(){return typeof window.rudolfNativeOracle=='function'}
/* (1.6.0) the title to the native desktop too (rudolf-launch's binding): Rudolf's Chromium 158 kept an Oracle window's own title at
   the shell's name, so its frame and taskbar button said "Oracle" whatever the page; asked for again once the binding is there */
function oaTitleNative(){if(OA_SHT&&oaChromium())try{window.rudolfNativeOracle(JSON.stringify({w:ONEAPP.w||0,op:'title',t:String(OA_SHT).slice(0,250)}))}catch(e){}}
window.__oaTitleAgain=()=>{oaTitleNative();return 1};
let OA_CLT=0;
window.__oaCloseNow=()=>{clearTimeout(OA_CLT);try{shellSend({rudolfShell:'oa',op:'close'})}catch(e){}};
window.__oaCloseLater=()=>clearTimeout(OA_CLT);
/* (told by rudolf-launch once Chromium's downloads have finished: closed when this page has filed them, as the desktop does) */
window.__oaCloseWhenDone=()=>{const t0=Date.now(),go=()=>{const busy=typeof DLN=='object'&&(DLN.busy||DLN.ing>0||DLN.pend.some(x=>/^(wait|check|approve)$/.test(x.st)));
 if(busy&&Date.now()-t0<120e3)return setTimeout(go,1000);window.__oaCloseNow()};setTimeout(go,2500)};
/* (12.3c step 5) the page's own full screen (shell26.js's stand-in fills the page): Oracle's window on the native desktop is not the
   screen, as the shell's window is on Standard, so the window goes full screen too (rudolf-launch, through the binding) */
/* (1.6.0, the owner's report 5) the notice after an update: only in the native shell's start-up page (do-start, below), never in an
   app's window; Standard's desktop page shows it (a page of one app on Standard, a Tor window, leaves it to that page) */
let OA_UPD=null;if(ONEAPP&&typeof updNotice=='function'){OA_UPD=updNotice;updNotice=async()=>{}}
/* (1.6.0 Part 12, item 6) an app's window never starts the screen saver, the lock or sleep itself (core25.js idleCheck, every
   15 s in every page): the native shell does, once for the whole screen */
if(ONEAPP)idleWoke=()=>true;
if(ONEAPP)window.__oaFs=on=>{if(oaChromium())oaPost({op:'fs',on:on?1:0})};
/* (1.6.0 Part 12, test finding N1) the same in the native desktop's WebKit windows: the shell's stand-in (an element made full screen fills
   the page, Esc leaves) and the window covering the screen, as Standard's page is the screen (WebKit's own full screen showed a video
   black there, with only WebKit's control bar) */
if(ONEAPP&&oaNative()&&typeof fsStandIn=='function')fsStandIn(on=>{oaPost({op:'fs',on:on?'1':'0'});return true});
/* (12.3c step 5) a new private window (Ctrl+Shift+N, ☰ › New private tab) from Oracle's window on the native desktop: a window of its own, as
   on Standard, not one inside this page (the native desktop opens Oracle's jump "New private tab", as from its taskbar button) */
function oaPrivWin(){if(!(ONEAPP&&ONEAPP.k=='browser'&&OA.main&&oaChromium()))return false;oaPost({op:'open',app:'browser',jump:'New private tab'});return true}
/* (the native desktop is there: WebKit's message handler, or Oracle's binding in Rudolf's Chromium) */
const oaOut=()=>oaNative()||oaChromium();
/* (2026-11-16, the owner's Vivobook: no sound when a window opened or closed, or a button was pressed, on Native) the desktop's sounds
   (base_v24.html sfx: Volume Controls' volumes and styles) are played by the native shell, which draws the taskbar, the menus and the
   frames and plays theirs: this page sends its own (a window opened and closed, a button in it, a notification, an error) with the
   volume and tones it would play, once; its own Web Audio is not used (a window's page has no click yet, so WebKit kept it silent).
   A page doing something for the shell unseen (#app=do-NAME) clicks the page's buttons for the shell, which played that click already */
if(ONEAPP&&typeof sfx=='function'&&typeof PAT=='object'){let syn=false;addEventListener('click',e=>{syn=!e.isTrusted},true);const _sfx=sfx;
 sfx=function(n,force){if(!oaOut())return _sfx.apply(this,arguments);
  try{window.__a11y&&a11ySound(n)}catch(e){}const m=S.snd.master,s=S.snd[n];if(!s||!PAT[n])return;if(!force&&(!m.on||!s.on))return;const v=(m.vol/100)*(s.vol/100);if(v<=0)return;
  if(/^do-/.test(ONEAPP.k)&&syn&&(n=='click'||n=='game'))return;
  window.__sfx.push([n,+v.toFixed(3)]);let t=PAT[n];try{if(window.__a11y)t=sndPat(n)}catch(e){}
  oaPost({op:'sfx',k:n,v:String(+v.toFixed(4)),p:JSON.stringify({t:(t||[]).map(x=>x.slice(0,5))})})}}
/* (1.6.0 Part 12, item 12) the memory guard's "this is the desktop's page process" (mem47.js memIam: the processor kept busy for 0.6 s,
   15 s after loading and again until rudolfd knows it) is the Standard desktop's: on the native desktop no page is the desktop, and an
   app's window or Oracle's page claiming it would keep the guard from ending it. Not done there (every app window spent 0.6 s of
   processor time on it) */
if(ONEAPP&&oaOut())memIam=async()=>{};
if(ONEAPP)addEventListener('rudolf-shell',()=>{if(OA_SHT)shellSend({rudolfShell:'oa',op:'title',t:OA_SHT})});
const oaHex=h=>h&&/^([0-9a-f]{2})+$/i.test(h)?Uint8Array.from(h.match(/../g),x=>parseInt(x,16)):null;
if(ONEAPP){
 document.documentElement.classList.add('oneapp');
 {const st=document.createElement('style');st.textContent=`html.oneapp #bar,html.oneapp #menu,html.oneapp #desk>.ic,html.oneapp .wdg,html.oneapp #widgets{display:none!important}
html.oneapp,html.oneapp body{background:#e5e7eb!important}
html.oneapp .win.oamain{position:fixed!important;left:0!important;top:0!important;right:0!important;bottom:0!important;width:auto!important;height:auto!important;max-width:none!important;max-height:none!important;border-radius:0!important;border:0!important;box-shadow:none!important;display:flex!important;animation:none!important;transform:none!important;transition:none!important}
html.oneapp .win.oamain>.tb{display:none!important}html.oneapp .win.oamain>.body{border-radius:0!important}
#oaclosed{position:fixed;inset:0;z-index:2147483000;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:14px;background:#e5e7eb;color:#111827;font:16px system-ui,sans-serif}
#oaclosed button{font:inherit;padding:6px 16px;border-radius:6px;border:1px solid #9ca3af;background:#fff;cursor:pointer}`;document.head.appendChild(st)}
 /* (1.6.0 Part 12, item 8) WebKitGTK's form controls as Chromium draws them (the windows of the native desktop): a drop-down list
    is GTK's taller combo box there; here Chromium's (its border, padding and colours the page's, the chevron 20 px from its
    text, 4 px more room before its text, 2 px taller); check boxes and radio buttons with Chromium's size and margins. The page's own
    select rule's padding gets the same room; a list given other
    padding by a rule of its own keeps it, with the chevron. A line height of 1.5 at 13 px is 19 px in WebKit (it drops the half
    pixel) and 20 px in Chromium: Blitzen's greeting is given Chromium's.
    (Test findings N4, N10) a ticked box in Chromium's blue (#0075ff; WebKitGTK's own is GTK's accent); a number field's arrows only
    while the pointer is over it, as Chromium shows them (the Print dialog's Pages and Copies).
    (N6) a list's chevron also when the page gives it a background of its own (an inline style: WordLite's font and size, File
    Explorer's sort and view, the Media Player's speed...). In dark mode the chevron is the list's light writing (#e2e8f0), as Chromium draws it (found on the beta in QEMU, 2026-11-16: dark on dark, so not seen). (N5, N13, N9) Chromium's own controls, measured from its drawing, as the
    browser's defaults (a cascade layer under the page's own rules, as Chromium's built-in styles are): a range is a 129×16 box with an
    8 px track (#0075ff up to the knob's centre, #efefef after, a 1 px edge) and a 16 px knob, grey when disabled (GTK draws a thin
    line and a large white knob); a colour box 50×27 in #efefef with a grey edge and the swatch inset (GTK draws a pill); a time
    field has Chromium's clock and a date field its calendar (the 12-hour clock itself: rudolf-native gives WebKit Chromium's
    locale, en-US, when the system has none); a radio button Chromium's ring and dot; a progress bar Chromium's 10em by 1em with
    the range's 8 px track.
    (Second run, N22) Chromium's classic scrollbars (15 px: arrows, a #8b8b8b thumb inset 3 px, on #fcfcfc), which take their room
    as Chromium's do; WebKitGTK's own are GTK's overlay ones, which take none, so lines wrapped differently. (N25) A text box the
    page does not style has Chromium's 1 px grey edge, square corners and grey placeholder (WebKitGTK draws GTK's entry). */
 if(oaNative()){document.documentElement.classList.add('oawk');const st=document.createElement('style');st.textContent=`html.oawk select:not([multiple]):not([size]){-webkit-appearance:none;appearance:none;background-image:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='10' height='7'%3E%3Cpath d='M1.5 1.5L5 5l3.5-3.5' fill='none' stroke='%23111' stroke-width='2'/%3E%3C/svg%3E");background-repeat:no-repeat;background-position:right 2px center}
:where(html.oawk) select:where(:not([multiple]):not([size])){padding:4px 22px 4px 10px}
:where(html.oawk) input:where([type=checkbox]){margin:3px 3px 3px 4px;width:13px;height:13px}:where(html.oawk) input:where([type=radio]){margin:3px 3px 0 5px;width:13px;height:13px}
html.oawk .blzhello{line-height:20px}html.oawk .blzhello *{line-height:1.5}
:where(html.oawk) input:where([type=checkbox],[type=radio]){accent-color:#0075ff}
html.oawk input[type=number]::-webkit-inner-spin-button{opacity:0}html.oawk input[type=number]:hover::-webkit-inner-spin-button{opacity:1}
html.oawk input[type=time]::-webkit-datetime-edit{margin-right:30px}html.oawk input:is([type=date],[type=datetime-local],[type=month],[type=week])::-webkit-datetime-edit{margin-right:34px}
html.oawk select:not([multiple]):not([size]){background-image:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='10' height='7'%3E%3Cpath d='M1.5 1.5L5 5l3.5-3.5' fill='none' stroke='%23111' stroke-width='2'/%3E%3C/svg%3E")!important;background-repeat:no-repeat!important;background-position:right 2px center!important}
html.oawk.dark select:not([multiple]):not([size]){background-image:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='10' height='7'%3E%3Cpath d='M1.5 1.5L5 5l3.5-3.5' fill='none' stroke='%23e2e8f0' stroke-width='2'/%3E%3C/svg%3E")!important}
@layer oaua{
:where(html.oawk) input:where([type=range]){-webkit-appearance:none;appearance:none;background:transparent;width:129px;height:16px;margin:2px;padding:0;border:0;--oaf:.5}
:where(html.oawk) input:where([type=range])::-webkit-slider-runnable-track{height:8px;box-sizing:border-box;border:1px solid transparent;border-radius:4px;
 background:linear-gradient(to right,#0075ff calc(8px + (100% - 16px) * var(--oaf)),#efefef 0) padding-box,linear-gradient(to right,#3b75ba calc(8px + (100% - 16px) * var(--oaf)),#b2b2b2 0) border-box}
:where(html.oawk) input:where([type=range])::-webkit-slider-thumb{-webkit-appearance:none;appearance:none;width:16px;height:16px;margin-top:-5px;border:0;border-radius:50%;background:#0075ff}
:where(html.oawk) input:where([type=range]:disabled)::-webkit-slider-runnable-track{background:linear-gradient(to right,#cbcbcb calc(8px + (100% - 16px) * var(--oaf)),#fafafa 0) padding-box,linear-gradient(to right,#b1b1b1 calc(8px + (100% - 16px) * var(--oaf)),#d2d2d2 0) border-box}
:where(html.oawk) input:where([type=range]:disabled)::-webkit-slider-thumb{background:#cbcbcb}
:where(html.oawk) input:where([type=color]){-webkit-appearance:none;appearance:none;width:50px;height:27px;box-sizing:border-box;padding:5px 4px;border:1px solid #767676;border-radius:3px;background:#efefef}
:where(html.oawk) input:where([type=color])::-webkit-color-swatch-wrapper{padding:0}
:where(html.oawk) input:where([type=color])::-webkit-color-swatch{border:1px solid #777;border-radius:0}
:where(html.oawk) input:where([type=time],[type=date],[type=datetime-local],[type=month],[type=week]){border:1px solid #767676;border-radius:0;padding:1px 2px;background-color:#fff;background-repeat:no-repeat;background-origin:content-box;background-position:right 3px center}
:where(html.oawk) input:where([type=time]){background-image:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='14' height='14'%3E%3Ccircle cx='7' cy='7' r='5.4' fill='none' stroke='%23000' stroke-width='1.4'/%3E%3Cpath d='M6.6 3.8v3.6l2.4 2.4' fill='none' stroke='%23000' stroke-width='1.3'/%3E%3C/svg%3E")}
:where(html.oawk) input:where([type=date],[type=datetime-local],[type=month],[type=week]){background-image:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='13'%3E%3Cpath d='M3 0.5v2M9 0.5v2' stroke='%23000' stroke-width='1.1'/%3E%3Crect x='1' y='1.8' width='10' height='10.2' rx='1.4' fill='none' stroke='%23000' stroke-width='1.3'/%3E%3Cpath d='M1 4.4h10' stroke='%23000' stroke-width='2'/%3E%3C/svg%3E")}
:where(html.oawk) input:where([type=radio]){-webkit-appearance:none;appearance:none;box-sizing:border-box;border:1px solid #767676;border-radius:50%;background:#fff}
:where(html.oawk) input:where([type=radio]:checked){border-color:#0075ff;background:radial-gradient(circle,#0075ff 0 3.7px,#fff 4.3px)}
:where(html.oawk) input:where([type=radio]:disabled){border-color:#c5c5c5}:where(html.oawk) input:where([type=radio]:checked:disabled){background:radial-gradient(circle,#c5c5c5 0 3.7px,#fff 4.3px)}
::-webkit-scrollbar{width:15px;height:15px;background:#fcfcfc}
::-webkit-scrollbar-thumb{background:#8b8b8b padding-box;border:3px solid transparent;border-radius:6px}
::-webkit-scrollbar-thumb:hover{background:#636363 padding-box}::-webkit-scrollbar-thumb:active{background:#4a4a4a padding-box}
::-webkit-scrollbar-corner{background:#fcfcfc}
::-webkit-scrollbar-button:single-button{display:block;width:15px;height:15px;background:#fcfcfc no-repeat center}
::-webkit-scrollbar-button:single-button:vertical:decrement{background-image:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='15' height='15'%3E%3Cpath d='M3 11h9L7.5 6z' fill='%238b8b8b'/%3E%3C/svg%3E")}
::-webkit-scrollbar-button:single-button:vertical:increment{background-image:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='15' height='15'%3E%3Cpath d='M3 5h9L7.5 10z' fill='%238b8b8b'/%3E%3C/svg%3E")}
::-webkit-scrollbar-button:single-button:horizontal:decrement{background-image:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='15' height='15'%3E%3Cpath d='M11 3v9L6 7.5z' fill='%238b8b8b'/%3E%3C/svg%3E")}
::-webkit-scrollbar-button:single-button:horizontal:increment{background-image:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='15' height='15'%3E%3Cpath d='M5 3v9l5-4.5z' fill='%238b8b8b'/%3E%3C/svg%3E")}
:where(html.oawk) input:where(:not([type]),[type=text],[type=search],[type=password],[type=email],[type=url],[type=tel]){-webkit-appearance:none;appearance:none;border:1px solid #767676;border-radius:0;padding:2px 3px;background-color:#fff}
:where(html.oawk) input::placeholder,:where(html.oawk) textarea::placeholder{color:#757575}
:where(html.oawk) progress:where([value]){-webkit-appearance:none;appearance:none;box-sizing:border-box;width:10em;height:1em;padding:calc(.5em - 4px) 0;border:0;background:transparent}
:where(html.oawk) progress:where([value])::-webkit-progress-bar{border-radius:4px;background:#efefef;box-shadow:inset 0 0 0 1px #b2b2b2}
:where(html.oawk) progress:where([value])::-webkit-progress-value{border-radius:4px;background:#0075ff;box-shadow:inset 0 0 0 1px #3b75ba}
}`;document.head.appendChild(st);
  /* (N17) an iframe's sandbox flags that WebKit does not know (allow-presentation, which the page's browser gives its tabs) are left out
     rather than logged as errors on every tab ("…is an invalid sandbox flag"); Chromium knows them all, so they are not missed there */
  {const sa=Element.prototype.setAttribute,ok=(()=>{try{const f=document.createElement('iframe');return t=>{try{return f.sandbox.supports(t)}catch(e){return true}}}catch(e){return()=>true}})();
   Element.prototype.setAttribute=function(n,v){if(this.tagName=='IFRAME'&&String(n).toLowerCase()=='sandbox')v=String(v).split(/\s+/).filter(t=>t&&ok(t)).join(' ');return sa.call(this,n,v)}}
  /* (second run, N29) a video's frame callbacks (requestVideoFrameCallback: the hardware check counts a camera's frames with it): WebKitGTK
     never calls them back; here at the first animation frame after a new frame was shown (its count of frames, else its time moving) */
  if(window.HTMLVideoElement){const V=HTMLVideoElement.prototype,cbs=new Map();let id=0;const nf=v=>{try{return v.getVideoPlaybackQuality().totalVideoFrames}catch(e){return 0}};
   V.requestVideoFrameCallback=function(cb){const v=this,i=++id,t0=v.currentTime,n0=nf(v);
    const tick=now=>{if(!cbs.has(i))return;const t=v.currentTime,n=nf(v);
     if(v.readyState>=2&&(n>n0||(!n&&t!==t0))){cbs.delete(i);try{cb(now,{presentationTime:now,expectedDisplayTime:now,width:v.videoWidth,height:v.videoHeight,mediaTime:t,presentedFrames:n,processingDuration:0})}catch(e){console.warn(e)}return}
     cbs.set(i,requestAnimationFrame(tick))};
    cbs.set(i,requestAnimationFrame(tick));return i};
   V.cancelVideoFrameCallback=function(i){const r=cbs.get(i);if(r)cancelAnimationFrame(r);cbs.delete(i)}}
  /* (N8) the graphics chip's name, as Chromium gives it (WebKit gives every page "Apple GPU"): the native shell's (window.__oaGpu) */
  if(window.__oaGpu)for(const C of [window.WebGLRenderingContext,window.WebGL2RenderingContext])if(C){const g=C.prototype.getParameter;
   C.prototype.getParameter=function(p){if(p===0x9246)return String(__oaGpu.r);if(p===0x9245)return String(__oaGpu.v);return g.call(this,p)}}
  /* (N5) a range's filled part up to its knob, as Chromium draws it: --oaf, its value's share of its range, kept as it changes (by the
     person, by the page's script, by its attributes) */
  const rg=e=>{if(!e||e.type!='range')return;const mn=e.min===''?0:+e.min,mx=e.max===''?100:+e.max,v=+e.value;e.style.setProperty('--oaf',mx>mn&&isFinite(v)?String(Math.min(1,Math.max(0,(v-mn)/(mx-mn)))):'0')};
  const all=n=>{if(n.nodeType!=1)return;if(n.tagName=='INPUT')rg(n);else if(n.querySelectorAll)n.querySelectorAll('input[type=range]').forEach(rg)};
  addEventListener('input',e=>rg(e.target),true);addEventListener('change',e=>rg(e.target),true);
  for(const k of ['value','valueAsNumber']){const d=Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,k);if(d&&d.set)Object.defineProperty(HTMLInputElement.prototype,k,{configurable:true,enumerable:d.enumerable,get(){return d.get.call(this)},set(v){d.set.call(this,v);if(this.type=='range')rg(this)}})}
  for(const k of ['stepUp','stepDown']){const f=HTMLInputElement.prototype[k];HTMLInputElement.prototype[k]=function(){const r=f.apply(this,arguments);rg(this);return r}}
  const mo=()=>{all(document.body);new MutationObserver(L=>{for(const m of L){if(m.type=='attributes')rg(m.target);else m.addedNodes.forEach(all)}}).observe(document.body,{childList:true,subtree:true,attributes:true,attributeFilter:['value','min','max','type']})};
  if(document.body)mo();else addEventListener('DOMContentLoaded',mo)}
 /* (1.6.0 Part 12, test findings N2, N4, N10) the page's dialogs (.rmodal: Open and Save, Print, Allow access?, Close anyway?,
    Report a problem; .natdlg: a game's download) are over the whole desktop on Standard, dimmed, centred on the screen. Here the
    page's window is the app's own, so a dialog would be drawn inside it and cut off: the native shell makes the window cover the
    screen for as long as one is open, with a picture of the screen as it was behind it (the page's own dimming over it), and puts
    it back after. The dialog stays unseen (and can still take the focus) until the picture is there (oamwait), and the app's window keeps its size meanwhile. */
 if(oaNative()){const sel='body>.rmodal,body>.natdlg';let on=0,bg=null,st=null;
  const end=()=>{document.documentElement.classList.remove('oamodal','oamwait');if(bg){bg.remove();bg=null}if(st){st.remove();st=null}};
  const chk=()=>{const v=!!document.querySelector(sel);if(v==!!on)return;
   if(v){on=1;const r=OA.main&&OA.main.el?OA.main.el.getBoundingClientRect():null;st=document.createElement('style');
    if(r&&r.width)st.textContent=`html.oamodal .win.oamain{right:auto!important;bottom:auto!important;width:${Math.round(r.width)}px!important;height:${Math.round(r.height)}px!important}`;
    document.head.appendChild(st);document.documentElement.classList.add('oamodal','oamwait');oaPost({op:'modal',on:'1'})}
   else{on=0;end();oaPost({op:'modal',on:'0'})}};
  const obs=()=>new MutationObserver(chk).observe(document.body,{childList:true});if(document.body)obs();else addEventListener('DOMContentLoaded',obs);
  /* (the shell's picture of the screen, once the window covers it; none: shown over the page's own background) */
  window.__oaModalBg=src=>{if(!on)return;if(bg)bg.remove();if(!src){document.documentElement.classList.remove('oamwait');return}
   bg=document.createElement('img');bg.id='oamodalbg';bg.alt='';const show=()=>{if(on)document.documentElement.classList.remove('oamwait')};bg.onload=show;bg.onerror=show;bg.src=src;document.body.appendChild(bg)};
  const ms=document.createElement('style');ms.textContent=`#oamodalbg{position:fixed;left:0;top:0;width:100vw;height:100vh;z-index:1000002;pointer-events:none;user-select:none}
html.oamwait .rmodal,html.oamwait .natdlg,html.oamwait #oamodalbg{opacity:0!important}`;document.head.appendChild(ms)}
 /* the windows of the last session stay closed; the notifications' list is still read (the count) */
 window.__sessionRestore=()=>{try{NC=ld(NCK(),[]);ncUnread=0;ncBadge()}catch(e){}};
 /* only Settings writes the accounts */
 if(ONEAPP.k!='settings'){const _su=saveUser;saveUser=function(){if(OA.opened)return;return _su.apply(this,arguments)}}
 /* the first window made for the app is its main window: it fills the page and gives the page its title */
 {const _w=win;win=function(...a){const W=_w.apply(this,a);if(OA.want&&!OA.main&&W&&W.el){W.__oaReq=[a[2],a[3]];oaMain(W)}return W}}
 /* another app: a window of its own when the native shell is there */
 {const _l=launch;launch=function(k){if((OA.main||OA_SVC)&&k!=ONEAPP.k&&oaOut()&&APPS[k]){oaDrained().then(()=>oaPost({op:'open',app:k}));return}return _l.apply(this,arguments)}}
 /* (Settings at a page, asked by another app's window or a notification's action: Settings' own window, at that page) */
 {const _st=settings;settings=function(sec){if((OA.main||OA_SVC)&&ONEAPP.k!='settings'&&oaOut()){oaPost({op:'open',app:'settings',page:typeof sec=='string'?sec:''});return}return _st.apply(this,arguments)}}
 {const _lw=launchWith;launchWith=function(k,p){if((OA.main||OA_SVC)&&k!=ONEAPP.k&&oaOut()&&APPS[k]){const f=Array.isArray(p)?p.join('\\'):String(p||'');oaDrained().then(()=>oaPost({op:'open',app:k,file:f}));return}return _lw.apply(this,arguments)}}
 /* (1.5.9, 12.3c) Oracle's new window (Ctrl+N, a link or a website's new window from inside Oracle, a web page's file from another app:
    browser()) is a window of its own, as on Standard, not a second window inside this page: Oracle's, in Rudolf's Chromium. (Not a
    website's pop-up, whose tab this window's shell already holds with its opener (a sign-in's): adopt, kept here) */
 if(typeof browser=='function'){const _br=browser;browser=function(u,bo){if(!(bo&&bo.adopt)&&(ONEAPP.k!='browser'||(OA.main&&OA.main.el&&OA.main.el.isConnected))&&oaOut()){
  oaPost({op:'open',app:'browser',url:typeof u=='string'&&/^(https?:|about:|file:\/\/\/)/i.test(u)?u:'',priv:bo&&bo.priv?'1':''});return null}return _br.apply(this,arguments)}}
 /* (1.5.9, 12.3c step 5) Open with: the chosen app in a window of its own, as on Standard (not one inside this page): made by the
    native desktop's do-fs page, as its own Open with dialog does (oaFs op=openwith) */
 if(typeof OPENERS!='undefined')OPENERS.forEach(x=>{const f=x[4];if(!f||x[0]==ONEAPP.k)return;x[4]=p=>{if((OA.main||OA_SVC)&&oaOut()&&Array.isArray(p)){const fp=p.join('\\');oaDrained().then(()=>oaPost({op:'openwith',key:x[0],file:fp}));return}return f(p)}});
 /* the first sign-in screen: the native session's account instead, when there is one. (The owner's report 1) Until then the sign-in
    screen stays hidden from the window's first paint (base_v24.html's head: html.oalg), and shows only when this window really needs
    a sign-in (no native session, its keys refused: a page opened by hand) */
 {const _sl=showLogin,sl=function(){oaLoginUnhide();return _sl.apply(this,arguments)};let first=1;showLogin=function(){if(first&&!OA.opened){first=0;oaStart(sl);return}if(OA.opened&&OA.done)return;return sl.apply(this,arguments)}}
 /* after any sign-in: the app */
 {const _ul=userLogin;userLogin=async function(){const r=await _ul.apply(this,arguments);if(!OA.opened)setTimeout(oaOpen,0);return r}}
 /* (the ✕ button, not W.close: apps that ask about unsaved work watch the button) */
 window.__oneAppClose=()=>{const m=OA.main;if(m&&m.el.isConnected){const c=m.el.querySelector('.tb .cl');if(c)c.click();else m.close()}else oaDone()};
}
/* (a switch changed by a page doing it for the shell: in rudolfd's copy, and the shell told, before the page closes) */
function oaQsTold(){OA_QST=(async()=>{try{if(window.__rudolfLsFlush)await window.__rudolfLsFlush()}catch(e){}try{if(USER)oaPost({op:'set',id:uid()})}catch(e){}})()}
let OA_QST=null;
async function oaStart(showLogin0){
 let s=null;
 if(typeof RD!='undefined'&&RD){try{s=await rd('desktop/session')}catch(e){s=null}}
 /* (1.5.9, plan 16.2) a Tor window on Standard: no native session there; the desktop page gave rudolfd its account and file key
    when it asked for the window (torwin/session), so the window is signed in as the desktop is, not at a sign-in screen */
 if(!(s&&s.id)&&typeof RD!='undefined'&&RD&&ONEAPP&&ONEAPP.q.get('tor')=='1'){try{s=await rd('torwin/session')}catch(e){s=null}}
 let a=null;
 if(s&&s.id){a=s.r=='guest'?{n:'Guest',r:'guest',id:'guest',h:''}:ACCTS.find(x=>x.id==s.id)||null}
 if(!a){OA.login=1;oaPost({op:'login'});return showLogin0()}   /* (the native shell shows this window now: it does need its sign-in screen) */
 try{LG.style.display='none'}catch(e){}
 USER=a;
 try{const ks={pk:oaHex(s.pk),fk:oaHex(s.fk)};if(a.r!='guest'&&!ks.pk&&!ks.fk)throw new Error('no keys');await userLogin(a,null,a.r=='guest'?null:ks)}
 catch(e){console.warn('one-app sign-in',e);OA.login=1;oaPost({op:'login'});USER=ACCTS[0]||null;return showLogin0()}
 try{userUI()}catch(e){}}
function oaOpen(){if(OA.opened||!ONEAPP)return;OA.opened=1;
 try{LG.style.display='none'}catch(e){}oaLoginUnhide();
 const da0=/^do-/.test(ONEAPP.k)?ONEAPP.k.slice(3):'';if(da0=='desk'||da0=='fs'){OA.want=1;(da0=='desk'?oaDesk:oaFs)();return}
 /* (1.6.0, the owner's report 5) what Standard's desktop page does once at each sign-in, the native shell has a page do unseen
    (#app=do-start): the notice after an update (once per account and release, as on Standard). No app window shows it (each one is a
    page signing in, and every window opened showed it again); its "click to see what's new" opens Help's What's new in a window of
    its own; the page closes once its pop-ups are gone */
 if(da0=='start'){try{whatsNew=()=>oaPost({op:'open',app:'help',page:'new'});
   /* (the release seen is kept: devices26.js sets S.seenRel, but persistS writes the saved settings, SC, which it left out, so every
      page signing in announced the release again; Standard's one page at a time is left as it is) */
   if(OA_UPD)OA_UPD().then(()=>{if(SC&&S.seenRel&&SC.seenRel!==S.seenRel){SC.seenRel=S.seenRel;persistS()}}).catch(e=>console.warn(e))}catch(e){console.warn(e)}setTimeout(()=>oaFin({}),14000);return}
 /* (1.6.0 Part 12, item 12) the desktop's background work, once for the whole native desktop (#app=do-svc, opened by rudolf-native at
    sign-in and kept open, never shown): the timers every page's modules start as it loads, held back in the app windows (svc60.js),
    run here, signed in, as Standard's one page runs them (the Clock's alarms and timers, the Calendar's reminders, the updates...). Its
    notifications and sounds are the shell's, its dialogs over the screen; an app it opens (a reminder's click) is a window of its own,
    and a window it would make in itself (it has none of its own) is that app's own window instead */
 if(da0=='svc'){document.title='Rudolf OS';
   {const _w=win;win=function(...a){const W=_w.apply(this,a);try{if(W&&W.el&&oaOut()){const k=W.app&&APPS[W.app]?W.app:'';W.el.remove();if(W.tb)W.tb.remove();wins=wins.filter(x=>x!==W);if(k)oaPost({op:'open',app:k})}}catch(e){console.warn(e)}return W}}
   /* (the Clock's timers made before this sign-in are gone, as they go with Standard's page; the service page loaded again keeps them) */
   try{const since=+ONEAPP.q.get('since')||0,o=oaTmrRead();if(o&&since){const l=o.l.filter(t=>(+t.c||0)>=since);if(l.length!=o.l.length)oaTmrWrite(l)}}catch(e){}
   /* (the session's save, every 20 s as Standard's: the apps of the windows open on the whole desktop, asked of the shell as Task Manager
      asks, written only when they changed (each write goes to rudolfd's copy); kept as it was when the shell did not answer) */
   window.__sessionSave=async()=>{if(!USER||typeof oaWinsAsk!='function')return;const t=Date.now();try{await oaWinsAsk()}catch(e){}if(!(OA.winsAt>=t))return;
    const list=[...new Set(oaWins().filter(o=>o.app&&!/^do-/.test(o.app)&&!['settings','trash'].includes(o.app)).map(o=>o.app))].slice(0,8);
    if(JSON.stringify(ld(sessK(),[]))!==JSON.stringify(list))svj(sessK(),list)};
   try{__svcRun(1)}catch(e){console.warn('svc',e)}
   /* (the owner's Vivobook, 2026-11-16) the desktop's New folder and New text document, done here at once: this page is signed in already,
      where a page opened for it (#app=do-desk) had to load and sign in first, seconds on the Vivobook. As Standard's deskNew: the file, the
      icon, then its Rename, which the shell draws (op "done" with ask "rename", as oaDesk reports it) */
   window.__oaDeskNew=dir=>{let asked=null;const _rd=renameDlg;renameDlg=p=>{asked=p};
    try{deskNew(dir?1:0)}catch(e){console.warn('svc: new',e)}
    setTimeout(async()=>{renameDlg=_rd;try{fsFlush()}catch(e){}try{await oaCatalog()}catch(e){}
     oaPost(Object.assign({op:'done'},asked&&asked.length?{ask:'rename',file:asked.join('\\'),name:String(asked[asked.length-1])}:{}))},80);return 1};
   /* (and the shell's Rename dialog's answer, as oaFs's rename: renameDlg's checks and messages) */
   window.__oaRename=(f,to)=>{let r={ok:1};try{const p=resolve(String(f||''),['C:']),nv=String(to||'').trim().toUpperCase(),d=dirOf(p);
     if(!d||!(p.at(-1) in d))r={err:'The file is not there any more.'};else if(!validName(nv))r={err:'That name is not allowed.'};else if(nv!=p.at(-1)&&nv in d)r={err:'That name is already used.'};
     else{const v=d[p.at(-1)];delete d[p.at(-1)];d[nv]=v;fsSave();EXPL.forEach(f=>f())}}catch(e){r={err:String(e&&e.message||e)}}
    (async()=>{try{fsFlush()}catch(e){}try{await oaCatalog()}catch(e){}oaPost(Object.assign({op:'done'},r))})();return 1};
   oaPost({op:'svc',ready:'1'});return}
 /* (1.6.0, the owner's Vivobook, 2026-11-16: Sober's sign-in spun for ever on Native) the portal's requests (a Linux or Flatpak app's keyring
    prompts, file dialogs, links, notifications: linux26.js lxPortal, keyring60.js krPrompt), which the native shell collects and hands
    here one by one (#app=do-portal, a page of its own kept unseen while an account is signed in): each done as Standard's desktop does
    it, its dialog above every window (the page's dialogs cover the screen here), its notification the shell's. One that opens an app's
    window (a file, a folder, a mail link) is done by a page of its own that becomes that window (#app=do-portalapp&req=ID) */
 if(da0=='portal'){document.title='Rudolf OS';const opens=r=>r.kind=='openfile'||r.kind=='folder'||(r.kind=='url'&&/^(mailto|file):/i.test(String(r.uri||'')));
   const one=async id=>{let j;try{j=await lxApi('portal/poll')}catch(e){return}const it=(j.requests||[]).find(x=>x.id==id);if(!it||LX.seen.has(id))return;LX.seen.add(id);const r=it.req||{};
    if(opens(r)){oaPost({op:'portal',req:id});return}
    /* (a file dialog its app withdrew, ended or force-quit meanwhile: closed, as the keyring's prompt closes itself) */
    let wd=0;if(r.kind=='open'||r.kind=='save'){const had=new Set(document.querySelectorAll('body>.rmodal'));setTimeout(()=>{const o=[...document.querySelectorAll('body>.rmodal')].find(x=>!had.has(x));if(!o)return;
     wd=setInterval(async()=>{if(!o.isConnected)return clearInterval(wd);let k;try{k=await lxApi('portal/poll')}catch(e){return}if(!(k.requests||[]).some(q=>q.id==id)){clearInterval(wd);o.remove()}},1500)},300)}
    try{await lxPortal(id,r)}catch(e){console.warn('portal',e)}finally{clearInterval(wd)}};
   window.__oaPortal=ids=>{String(ids||'').split(',').filter(x=>/^[A-Za-z0-9_-]{1,40}$/.test(x)).forEach(id=>{one(id)})};
   oaAppsReady().then(()=>oaPost({op:'portal',ready:'1'}));return}
 if(da0=='portalapp'){OA.want=1;const id=ONEAPP.q.get('req')||'';(async()=>{await oaAppsReady();let j=null;try{j=await lxApi('portal/poll')}catch(e){}
   const it=j&&(j.requests||[]).find(x=>x.id==id);if(it){LX.seen.add(id);try{await lxPortal(id,it.req||{})}catch(e){console.warn('portal',e)}}
   setTimeout(()=>{if(!OA.main&&!OA.done){oaClosed('');oaPost({op:'close'})}},it?15000:0)})();return}
 /* (Help at a topic, as What's new asks from the page above: #app=help&page=TOPIC) */
 if(ONEAPP.k=='help'&&ONEAPP.q.get('page')&&typeof helpApp=='function'){OA.want=1;try{helpApp(ONEAPP.q.get('page'))}catch(e){console.warn(e)}setTimeout(()=>{if(!OA.main&&!OA.done){oaClosed('');oaPost({op:'close'})}},15000);return}
 /* (1.6.0 Part 12, item 6) the native desktop's screen saver: the page's own (ssRun) in this window, which the shell shows full
    screen above everything and closes when the person is back (#app=do-saver&style=NAME&time=0|1) */
 if(da0=='saver'&&typeof ssRun=='function'){OA.want=1;const q=ONEAPP.q,st=q.get('style');try{if(q.get('time'))SYSC.sstime=q.get('time')=='1';
   document.documentElement.style.background='#000';document.title='Screen saver';ssRun({style:SS_STYLES.some(x=>x[0]==st)?st:SYSC.ssstyle})}catch(e){console.warn(e)}return}
 /* (1.6.0 Part 12, item 3) the taskbar's menus' rows the shell leaves to the page: an app's jump (#app=KEY&jump=WORDS: Oracle's
    New private tab, its window this page's main one), Settings at a page (#app=settings&page=disp), a pinned app pinned or
    unpinned (#app=do-pin&k=KEY&on=0|1), the sound muted or not (#app=do-mute) */
 {const q=ONEAPP.q,j=q.get('jump'),sp=q.get('page');
  /* (1.5.9, plan Part 16) a Tor window with these tabs (#app=browser&tor=1&url=...&url=...), or a normal window given back a Tor
     window's tabs (untor=1): its first window this page's main one, the other tabs in it */
  if(ONEAPP.k=='browser'&&(q.get('tor')=='1'||q.get('untor')=='1')&&typeof browser=='function'){OA.want=1;const L=q.getAll('url').filter(u=>/^(https?:\/\/|about:|file:\/\/\/)/i.test(u)).slice(0,12),hm=u=>/^about:home/.test(u)?null:u;
   try{browser(L.length?hm(L[0]):null,q.get('tor')=='1'?{priv:1,tor:1}:{});L.slice(1).forEach(u=>{if(OA.main&&OA.main.oNewTab)OA.main.oNewTab(hm(u))})}catch(e){console.warn(e)}
   setTimeout(()=>{if(!OA.main&&!OA.done){oaClosed('');oaPost({op:'close'})}},15000);return}
  if(j&&typeof APP_JUMPS=='object'&&APP_JUMPS[ONEAPP.k]){const it=APP_JUMPS[ONEAPP.k]().find(x=>x[0]==j);if(it){OA.want=1;try{it[1]()}catch(e){console.warn(e)}setTimeout(()=>{if(!OA.main&&!OA.done){oaClosed('');oaPost({op:'close'})}},15000);return}}
  /* (1.5.9, 12.3c) Oracle's new window at an address, or a private one at an address (#app=browser&url=ADDRESS[&priv=1]) */
  if(ONEAPP.k=='browser'&&q.get('url')&&typeof browser=='function'){OA.want=1;const u=q.get('url');try{if(q.get('priv')=='1'&&typeof oraclePrivate=='function')oraclePrivate(u);else browser(u)}catch(e){console.warn(e)}
   setTimeout(()=>{if(!OA.main&&!OA.done){oaClosed('');oaPost({op:'close'})}},15000);return}
  if(ONEAPP.k=='settings'&&sp&&typeof settings=='function'){OA.want=1;try{settings(sp)}catch(e){console.warn(e)}setTimeout(()=>{if(!OA.main&&!OA.done){oaClosed('');oaPost({op:'close'})}},15000);return}
  /* (2026-11-16, the owner's Vivobook) the start menu's search as Standard's: the page's own startSearch for the native shell, which draws
     what it finds (#app=do-srch, unseen, kept while the menu is used): each row's group, words, picture and place, and what it opens (an
     app, a page of Settings, a file, a sum's value to copy), found by running its action with those four standing in */
  if(da0=='srch'&&typeof startSearch=='function'){window.__oaSrch=qq=>{let L=[];try{L=startSearch(String(qq||''))}catch(e){}
    return JSON.stringify({r:L.map(it=>{const o={g:String(it.g||''),t:String(it.t||''),i:String(it.i||''),sub:String(it.sub||'')};
     if(it.g=='Calculator'){o.a='copy';o.v=String(it.t).split(' = ').pop();return o}
     const sv=[launch,settings,openPath];try{launch=k=>{o.a='app';o.k=String(k)};settings=k=>{o.a='set';o.k=typeof k=='string'?k:''};openPath=p=>{o.a='file';o.p=Array.isArray(p)?p.join('\\'):String(p)};it.f()}
     catch(e){}finally{[launch,settings,openPath]=sv}return o})})};
   oaAppsReady().then(()=>oaPost({op:'srch'}));return}
  if(da0=='pin'){try{pinSet(q.get('k')||'',q.get('on')=='1')}catch(e){}return oaFin({})}
  /* (1.6.0 Part 12, item 3) quick settings' switches and the notifications' centre as the native shell draws them: the page's own
     panels made unseen and the same button pressed (#app=do-qs&item=WORDS; #app=do-nc&op=clear|dnd&on=0|1|dismiss&i=N|why&i=N) */
  /* (1.6.0) on=0|1: the switch as the shell shows it now, so a page that had not heard of a change yet does not undo it; the screen
     reader's "Screen reader on" said before the page closes (srSpeak: the voices', or rudolfd's say) */
  if(da0=='qs'){let sr=0;try{qsToggle();const it=q.get('item'),on=q.get('on'),b=[...document.querySelectorAll('#qsp .qst')].find(x=>(x.querySelector('span')||{}).textContent==it);
    if(b&&(on==null||(b.classList.contains('on')?'1':'0')!=on)){b.click();sr=it=='Screen reader'&&!!S.sr}
    /* (2026-11-16) the shell saves the switch itself now, so this page may have it already: the screen reader still says so, as the tile does */
    else if(b&&it=='Screen reader'){try{window.__srApply&&__srApply()}catch(e){}sr=!!S.sr}qsClose()
    /* (High contrast and Touch mode are kept as S.cb and S.touch, which the tile's own copy to SC (its keys hc, tm) leaves out: on Standard
       the one page shows them at once; here the other windows and the shell read what is saved) */
    if(SC){SC.cb=S.cb;SC.touch=S.touch;persistS()}}catch(e){console.warn(e)}
   /* (2026-11-16, k160_t.py in QEMU: the shell showed a switch off that every window had on) the change in rudolfd's copy and the shell told
      before this page closes: the save's own word to the shell (persistS's "set", 120 ms later) went with the page when it closed first */
   oaQsTold();
   if(!sr)return oaFin({});const t0=Date.now(),said=()=>{const a=window.__srAudio,ss=window.speechSynthesis;if(Date.now()-t0>8000||(Date.now()-t0>1500&&!(ss&&ss.speaking)&&!(a&&!a.ended&&!a.paused)))return oaFin({});setTimeout(said,250)};setTimeout(said,400);return}
  /* (1.6.0) the native desktop's on-screen keyboard: the page's own (#vkb, its keys for the language chosen) filling a window of its own
     that never takes the keyboard; a key goes to the shell, which gives it to the page of the window in front (__oaVkIn) */
  if(da0=='vk'&&typeof vkDraw=='function'){OA.want=1;OA_VK=1;const H=document.documentElement;H.classList.add('oavk');
   {const st=document.createElement('style');st.textContent=`html.oavk,html.oavk body{background:#e5e7eb!important;overflow:hidden!important;zoom:1!important}html.oavk body>*:not(#vkb){visibility:hidden!important}
html.oavk #vkb{display:block!important;visibility:visible!important;position:fixed!important;left:0!important;top:0!important;right:0!important;bottom:auto!important;transform:none!important;width:100%!important;max-width:none!important;border:0!important;border-radius:0!important;box-shadow:none!important;zoom:var(--oavkz,1)}`;document.head.appendChild(st)}
   vkInsert=c=>oaPost({op:'vk',c:String(c)});vkSpecial=k=>oaPost({op:'vk',sp:String(k)});
   /* (2026-11-16, k160_t.py in QEMU: Dark mode on also put the on-screen keyboard on) this page shows the keyboard without turning the
      setting on in its own settings (S.vk), which it saved when it took another window's change, so the shell read vk on; it saves nothing */
   vkShown=true;{const _vs=vkSync;vkSync=function(){const o=S.vk;S.vk=true;try{return _vs.apply(this,arguments)}finally{S.vk=o}}}persistS=function(){try{oaSnap()}catch(e){}};
   try{vkDraw();vkEl.style.display='block'}catch(e){console.warn(e)}document.title='On-screen keyboard';
   const z=Math.max(75,Math.min(250,+S.scale||100))/100;H.style.setProperty('--oavkz',z);H.style.zoom='';
   /* (its height for the window; its letters' places for the tests' state) */
   const tell=()=>oaPost({op:'vk',h:String(Math.ceil(vkEl.getBoundingClientRect().height/z)),keys:[...vkEl.querySelectorAll('.vk-k')].slice(0,60).map(b=>{const r=b.getBoundingClientRect();return encodeURIComponent(b.dataset.c||'')+' '+Math.round(r.left+r.width/2)+' '+Math.round(r.top+r.height/2)}).join('|')});
   setTimeout(tell,50);setTimeout(tell,800);return}
  if(da0=='short'){const s=typeof SHORT=='object'?SHORT.find(x=>x.id==q.get('id')):null;try{s&&s.fn()}catch(e){console.warn(e)}setTimeout(()=>oaFin({}),6000);return}
  /* (1.6.0) the power dialog's "Restart into Windows" (boot30.js): the administrator's password, the firmware asked to start Windows once,
     then the shell restarts the computer */
  if(da0=='pwr'&&q.get('op')=='windows'){OA.want=1;(async()=>{const n=q.get('n')||'',t=q.get('t')||'Windows';
    try{if(await elevate('Restart into Windows: the computer starts '+t+' once.')){await rdPost('bootmenu/windows?n='+encodeURIComponent(n));try{fsFlush()}catch(e){}try{await oaDrained()}catch(e){}oaPost({op:'pwr',a:'reboot'})}}
    catch(e){try{notify('Restart into Windows',e.message,{kind:'error'})}catch(x){}}oaFin({})})();return}
  if(da0=='nc'){const op=q.get('op')||'';try{NC=ld(NCK(),[]);ncToggle(true);const p=document.getElementById('ncp');
    if(op=='clear')p.querySelector('.clr').click();
    else if(op=='dnd'){const c=p.querySelector('.dnd');if(c.checked!=(q.get('on')=='1')){c.checked=q.get('on')=='1';c.dispatchEvent(new Event('change'))}}
    else if(op=='dismiss'||op=='why'){const it=p.querySelectorAll('.nitem')[+q.get('i')||0],b=it&&it.querySelector(op=='why'?'.bzwhy':'.nx');if(op=='why')OA.want=1;if(b)b.click()}
    p.remove()}catch(e){console.warn(e)}
   if(op=='why'){setTimeout(()=>{if(!OA.main&&!OA.done){oaClosed('');oaPost({op:'close'})}},15000);return}if(op=='dnd')oaQsTold();return oaFin({})}
  if(da0=='mute'){try{const m=S.snd&&S.snd.master;if(m){m.on=!m.on;trayVol()}}catch(e){}setTimeout(()=>oaFin({}),400);return}}
 /* (1.6.0 Part 12, item 3) a day double-clicked in the calendar under the native shell's clock: the Calendar at that day */
 {const cd=ONEAPP.q.get('day');if(ONEAPP.k=='calendar'&&cd&&/^\d{4}-\d\d-\d\d$/.test(cd))setTimeout(()=>{try{window.__calGo&&__calGo(cd)}catch(e){}},800)}
 const da=da0,act=da?(typeof A=='object'&&typeof A[da]=='function'?A[da]:da=='blitzen'&&typeof blitzenApp=='function'?blitzenApp:null):null;
 const app=APPS[ONEAPP.k]||(act?{name:da,open:act}:null);
 /* (2026-11-16, the owner's Vivobook) a Library package's, a Linux app's or a Windows program's key: this page has its app once it has asked
    rudolfd for them (pkgRegister, lxRegister: on Standard long done when anything is opened) */
 if(!app&&!act&&/^(pkg|lx|native|rapp):/.test(ONEAPP.k)&&!OA.waited){OA.waited=1;OA.opened=0;oaAppsReady().then(oaOpen);return}
 if(!app){oaClosed('There is no app called “'+ONEAPP.k+'”.');oaPost({op:'close'});return}
 OA.want=1;
 /* (1.5.9, 12.3c) Oracle without Rudolf's Chromium on the native desktop (none, or it would not start): said once */
 if(ONEAPP.k=='browser'&&ONEAPP.q.get('nochrome')=='1')setTimeout(()=>{try{notify('Oracle','Websites will work better on Standard: Oracle is running without Rudolf\u2019s Chromium here, so some sites cannot open in its tabs.')}catch(e){}},1500);
 try{if(act)act();else if(ONEAPP.file&&ONEAPP.k=='explorer'&&typeof explorer=='function'){const p=resolve(ONEAPP.file,['C:']);(async()=>{for(let i=0;i<60&&!isDir(node(p));i++)await new Promise(r=>setTimeout(r,100));explorer(isDir(node(p))?p:p.slice(0,-1))})()}   /* (1.6.0: a folder: tm26.js srOpenFolder; the account's home read first) */
  else if(ONEAPP.file&&app.openFile)launchWith(ONEAPP.k,resolve(ONEAPP.file,['C:']));
  else if(ONEAPP.k=='taskmgr'&&ONEAPP.q.get('urgent')=='1'&&typeof tmUrgent=='function')tmUrgent();   /* (1.6.0: Ctrl+Alt+Del's, tmtop57.js) */
  else{const r=launch(ONEAPP.k);
   /* (2026-11-16) a program rudolfd runs (a Linux app, a Windows program, a Library app in full screen) opens no window of this page's: as on
      Standard, once rudolfd has it (its "Starting…" said), this page closes unseen; if it made a window after all, that window stays */
   if(oaExt(ONEAPP.k)){Promise.resolve(r).catch(e=>console.warn(e)).then(()=>setTimeout(()=>{if(!OA.main&&!OA.done)oaDone()},2500));return}}}catch(e){console.warn(e)}
 /* an app that opens no window (or not yet: a large app loads its files first) */
 setTimeout(()=>{if(!OA.main&&!OA.done){oaClosed(app.name+' did not open a window.');oaPost({op:'close'})}},15000)}
/* (1.6.0 Part 12, item 1's second part) the native desktop's menus do what the page's do: #app=do-desk&on=ICON&item=WORDS
   gives the page's own menu for that desktop icon (its name, or a file's name; none: the desktop's) and chooses the item
   with those words, as a click would. What opens a window is that window; what has none is done unseen and the page
   closes; the page's own dialogs that the shell draws itself (Rename, Open with) are reported to it instead. */
function oaFin(m){(async()=>{try{if(OA_QST)await OA_QST}catch(e){}try{fsFlush()}catch(e){}try{await oaCatalog()}catch(e){}oaPost(Object.assign({op:'done'},m||{}));oaDone()})()}
function oaDesk(){const q=ONEAPP.q,on=q.get('on')||'',item=(q.get('item')||'').trim(),D=document.getElementById('desk');
 const el=on?[...D.querySelectorAll(':scope > .ic')].find(e=>(e.dataset.fn||((e.querySelector('span')||{}).textContent||'').trim())==on):null;
 if(on&&!el)return oaFin({err:'There is no “'+on+'” on the desktop.'});
 if(el&&el.dataset.fn&&/^open with/i.test(item)){const p=homePath('DESKTOP').concat(el.dataset.fn),L=openersFor(p);
  return oaFin({ask:'openwith',file:p.join('\\'),ext:ext(p.at(-1))||'',apps:JSON.stringify((L.length?L:OPENERS.slice(0,2)).map(x=>[x[0],x[1],x[2]]))})}
 (el||D).dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,cancelable:true,clientX:20,clientY:20}));
 const it=[...document.querySelectorAll('.ctx .cmi')].find(e=>((e.querySelector('span')||e).textContent||'').trim()==item);
 if(!it){document.querySelectorAll('.ctx').forEach(c=>c.remove());return oaFin({err:'There is no “'+item+'” in that menu.'})}
 it.click();
 setTimeout(()=>{if(OA.main)return;const m=document.querySelector('.rmodal');let r={};
  if(m){const rn=m.querySelector('input.rn');const fn=(deskFiles()&&Object.keys(deskFiles()).find(n=>rn&&n==rn.value))||(rn&&rn.value)||'';
   if(rn)r={ask:'rename',file:homePath('DESKTOP').concat(fn).join('\\'),name:fn};m.remove()}
  oaFin(r)},500)}
/* #app=do-fs&op=OP: the native desktop's own dialogs' answers, done by the page as its dialogs do them (rename: renameDlg's
   checks and messages; openwith: Open with's choice, Always included) */
function oaFs(){const q=ONEAPP.q,op=q.get('op')||'',p=resolve(ONEAPP.file||'',['C:']);
 if(op=='rename'){const nv=String(q.get('to')||'').trim().toUpperCase(),d=dirOf(p);
  if(!d||!(p.at(-1) in d))return oaFin({err:'The file is not there any more.'});
  if(!validName(nv))return oaFin({err:'That name is not allowed.'});if(nv!=p.at(-1)&&nv in d)return oaFin({err:'That name is already used.'});
  const v=d[p.at(-1)];delete d[p.at(-1)];d[nv]=v;fsSave();EXPL.forEach(f=>f());return oaFin({ok:1})}
 /* (2026-11-16) a file found by the start menu's search: opened as the page's search opens it (openPath: its app, its window this page's) */
 if(op=='open'){try{openPath(p)}catch(e){console.warn(e)}setTimeout(()=>{if(!OA.main)oaFin({})},1500);return}
 if(op=='openwith'){const x=OPENERS.find(o=>o[0]==q.get('key'));if(!x)return oaFin({err:'That app cannot open it.'});
  if(q.get('always')=='1'){const d=ld(defK(),{});d[ext(p.at(-1))]=x[0];svj(defK(),d)}
  try{x[4]?x[4](p):openPathDefault(p)}catch(e){console.warn(e)}setTimeout(()=>{if(!OA.main)oaFin({})},1500);return}
 /* (1.6.0 Part 12, item 1) a desktop file dropped on an app's window: what a11y25.js's __dropFileOnWindow does for that app (this
    page's window, if it makes one, is the file's window) */
 if(op=='drop'){const k=q.get('in')||'';let r=false;try{r=!!__dropFileOnWindow({app:k},p)}catch(e){console.warn(e)}
  setTimeout(()=>{if(!OA.main)oaFin(r?{}:{err:'nothing'})},1500);return}
 /* (1.6.1 Part 13) files dropped from a Linux app onto the native desktop (the shell took the X drop): what shell25.js's DESK drop does with
    Chromium's files on Standard, each file read through rudolfd (desktop/dropped: the account's own files and the drives, read by its
    Linux user) instead of by the browser */
 if(op=='xdrop'){oaXdrop(q).then(r=>oaFin(Object.assign({xd:'1'},r)),e=>{console.warn(e);oaFin({xd:'1',err:String(e&&e.message||e)})});return}
 oaFin({err:'Unknown: '+op})}
/* the type Chromium gives a dropped file (its own list of names: net/base/mime_util.cc), which decides, as on Standard, whether the page
   keeps it as text or as a data: URL */
const OA_XTYPE={html:'text/html',htm:'text/html',shtml:'text/html',shtm:'text/html',ehtml:'text/html',css:'text/css',xml:'text/xml',xsl:'text/xml',xbl:'text/xml',xslt:'text/xml',
 js:'text/javascript',mjs:'text/javascript',json:'application/json',txt:'text/plain',text:'text/plain',csv:'text/csv',ics:'text/calendar',vtt:'text/vtt',md:'text/markdown',
 svg:'image/svg+xml',svgz:'image/svg+xml',xhtml:'application/xhtml+xml',xht:'application/xhtml+xml',rss:'application/rss+xml',png:'image/png',jpg:'image/jpeg',jpeg:'image/jpeg',
 gif:'image/gif',webp:'image/webp',pdf:'application/pdf',zip:'application/zip',mp3:'audio/mpeg',mp4:'video/mp4',webm:'video/webm',wasm:'application/wasm'};
const OA_XWHY={'too-large':'','unreadable':'your account may not open it.','link':'it is a link to somewhere else.','outside':'it is not in your files or on a drive.',
 'gone':'it is not there any more.','folder':'folders and devices cannot be dropped.','path':'its name is too long.','nobody':'nobody is signed in.'};
async function oaXdrop(q){let fl=[];try{fl=JSON.parse(q.get('files')||'[]')}catch(e){}if(!Array.isArray(fl))fl=[];
 const d=deskFiles();if(!d)return{err:'no desktop'};let n=0;
 for(const p of fl.slice(0,256)){const fn=String(p).split('/').pop()||String(p);let f=null;
  try{if(!RD)throw new Error('nobody');const r=await fetch('/api/desktop/dropped?path='+encodeURIComponent(String(p)),{headers:{'X-Rudolf-Token':RD.token}});
   if(!r.ok){let w='';try{w=(await r.json()).error}catch(x){}throw new Error(w||'HTTP '+r.status)}const b=await r.blob();f=new File([b],fn,{type:OA_XTYPE[(fn.match(/\.([^.]+)$/)||[])[1]?.toLowerCase()]||''})}
  catch(e){const w=String(e&&e.message||e);if(w=='too-large')notify('Too large',fn+' is over 64 MB.',{kind:'error'});else notify('Blocked',fn+' was not copied: '+(OA_XWHY[w]||w),{kind:'error'});continue}
  /* (from here as shell25.js's drop, line by line) */
  if(f.size>64*1048576){notify('Too large',f.name+' is over 64 MB.',{kind:'error'});continue}const nm=f.name.toUpperCase().replace(/[\\\/:*?"<>|]/g,'_').slice(0,60);const bin=!/^text\/|json|xml|javascript/.test(f.type)&&!/\.(txt|lua|md|csv|json|html?|css|js|bat|cfg)$/i.test(f.name);
  const val=await new Promise(r=>{const fr=new FileReader();fr.onload=()=>r(fr.result);bin?fr.readAsDataURL(f):fr.readAsText(f)});const g=window.__sys&&secIngress(nm,val,'upload');if(g&&!g.ok){notify('Blocked',g.msg,{kind:'error'});continue}d[uniq(d,nm)]=val;n++}
 fsSave();EXPL.forEach(f=>f());return{ok:1,n:String(n)}}
/* (2026-11-16, found on the beta in QEMU at 1280×800) the size Standard's win() gives the window: what the app asked for, kept inside the
   screen's desktop (the screen less the taskbar), not inside this page's own window, which may be smaller when it is made (Settings
   came out 87 px shorter than on Standard on a 1280×800 screen) */
function oaScreenSize(W){const st=[Math.round(parseFloat(W.el.style.width)||0),Math.round(parseFloat(W.el.style.height)||0)];
 try{const r=W.__oaReq||[],w=parseFloat(r[0]),h=parseFloat(r[1]),q=ONEAPP.q,zf=parseFloat(document.documentElement.style.zoom)||1,top=document.body.classList.contains('top'),
  side=document.body.classList.contains('left')||document.body.classList.contains('right'),
  sw=+q.get('sw')/(devicePixelRatio||1)||screen.width,sh=+q.get('sh')/(devicePixelRatio||1)||screen.height,dw=sw/zf,avail=sh/zf-(side?0:top?48:42);
  if(w>0&&h>0&&dw>0&&avail>0)return{ww:String(Math.round(Math.min(w,dw-8))),wh:String(Math.round(Math.min(h,avail-8)))}}catch(e){}
 return{ww:String(st[0]),wh:String(st[1])}}
function oaMain(W){OA.main=W;OA.want=0;
 /* (1.6.0 Part 12) the size the page gives the app's window (its 1 px edge and title bar included), for the native frame */
 oaPost({op:'size',...oaScreenSize(W),app:String(W.app||'')});W.el.classList.add('oamain');
 const t=W.el.querySelector('.tb .t'),sync=()=>{const s=(t&&t.textContent||'').trim()||(APPS[ONEAPP.k]&&APPS[ONEAPP.k].name)||'Rudolf OS';if(document.title!=s){document.title=s;oaPost({op:'title',t:s,icon:W.icon||''})}};
 sync();if(t)new MutationObserver(sync).observe(t,{childList:true,characterData:true,subtree:true});
 /* (1.6.0 Part 12, test finding N14) a window whose own icon is not its app's (Flash Player's "not installed" window): its picture as the
    page draws it in the title bar (icSvg), for the native frame and the taskbar button, which otherwise show the app's */
 try{const ai=(APPS[ONEAPP.k]||{}).icon||'',wi=String(W.icon||''),k=typeof EMO2IC=='object'?EMO2IC[wi]||EMO2IC[wi.replace(/\uFE0F/g,'')]:'';
  if(wi&&wi!=ai&&k&&S.icons!=='emoji'&&typeof icSvg=='function')oaPng(icSvg(k),48).then(png=>oaPost({op:'title',t:document.title,icon:wi,png})).catch(()=>{})}catch(e){}
 /* (the window's own maximise and minimise mean nothing here: it always fills the page) */
 /* (closed however it closes: its ✕, its own menu's Exit, a closing animation: the window leaves the page) */
 const mo=new MutationObserver(()=>{if(!W.el.isConnected){mo.disconnect();oaDone()}});mo.observe(document.body,{childList:true,subtree:true});
 setTimeout(()=>{try{W.focus()}catch(e){}},0)}
/* the app has closed: its files and settings are written, then the page closes */
async function oaDone(){if(OA.done)return;OA.done=1;
 try{fsFlush()}catch(e){}
 try{if(typeof HD!='undefined'&&HD.chain)await Promise.race([HD.chain,new Promise(r=>setTimeout(r,8000))])}catch(e){}
 try{if(window.__rudolfLsFlush)await window.__rudolfLsFlush()}catch(e){}
 oaPost({op:'close'});
 try{window.close()}catch(e){}
 setTimeout(()=>oaClosed(''),300)}
function oaClosed(why){if(document.getElementById('oaclosed'))return;const d=document.createElement('div');d.id='oaclosed';d.setAttribute('role','status');
 const n=APPS[ONEAPP.k]?APPS[ONEAPP.k].name:ONEAPP.k;
 d.innerHTML=`<div style='font-size:20px;font-weight:600'></div><div class='oaw'></div><button>Open again</button>`;
 d.firstChild.textContent=why?n:n+' is closed';d.querySelector('.oaw').textContent=why;
 d.querySelector('button').onclick=()=>location.reload();document.body.appendChild(d);document.title=n}
/* ===== 1.5.9 (plan Part 12, step 2): what the page gives the native shell through rudolfd =====
   - desktop/apps, after each sign-in (any engine): the start menu's list (each app's key, name, icon and search words,
     and the account's hidden apps) and the drawn backgrounds' pictures (DYN: each one's light and dark file);
   - desktop/wallpaper: a picture of your own, which lives in this page's storage (IndexedDB), as the JPEG the page
     already made of it, whenever it is loaded or changed (once per picture). ===== */
async function oaCatalog(){if(typeof RD=='undefined'||!RD||!USER)return;
 const sig=oaCatSig();
 try{const apps=Object.values(APPS).filter(a=>a&&a.k&&a.name&&typeof a.open=='function').map(a=>Object.assign({k:a.k,name:String(a.name),icon:String(a.icon||''),kw:String(a.kw||'')},oaExt(a.k)?{x:1}:{}));
  const bg={};if(typeof DYN=='object')for(const k in DYN){const d=DYN[k];bg[k]={n:d.n||k,d:d.d||'',l:d.l||'',am:d.am||''}}
  /* (1.6.0 Part 12, test finding N12) Spirit Falls' statue as this page draws it (statue58.js: its animal, light or dark, its glow), a
     picture at the screen's size for the native shell, which draws the background's moving parts itself (N11) */
  const fx={};try{if(S.dyn=='oasis'&&typeof stSvg=='function'&&typeof ST=='object'&&ST.box){let mode='light';try{mode=dynResolve().mode}catch(e){}const G=stGlow(),key=stAnimal()+'|'+mode+'|'+G;
   const q=ONEAPP?ONEAPP.q:new URLSearchParams(''),sw=+q.get('sw')||screen.width,sh=+q.get('sh')||screen.height,sc=Math.max(sw/1600,sh/1067),b=ST.box;
   const im=await new Promise((res,rej)=>{const i=new Image();i.onload=()=>res(i);i.onerror=rej;i.src=stSvg(stAnimal(),mode,G)});
   const c=document.createElement('canvas');c.width=Math.max(1,Math.round(b[2]*sc));c.height=Math.max(1,Math.round(b[3]*sc));c.getContext('2d').drawImage(im,0,0,c.width,c.height);
   fx.statue={key,box:b,png:c.toDataURL('image/png')};OA_ST=key}}catch(e){console.warn('statue',e)}
  let hide=[];try{hide=ld(HIDEK(),[])}catch(e){}
  /* (1.5.9 step 7) the apps' pictures as the page draws them (icSvg: the icon style, not the emoji, which the system
     may draw pale or not at all), as small PNGs, one per picture */
  const icons={};if(typeof EMO2IC=='object'&&typeof icSvg=='function'&&S.icons!=='emoji')for(const a of apps){const e=a.icon,k=EMO2IC[e]||EMO2IC[e.replace(/\uFE0F/g,'')];
   if(!k)continue;a.ic=k;if(!icons[k])try{icons[k]=await oaPng(icSvg(k),48)}catch(e){delete a.ic}}
  /* (2026-11-16) an app's own picture (a Linux app's, a Windows program's: its package's icon, as Standard shows it) instead */
  const own=async k=>{if(!APPS[k]||typeof APPS[k].img!='function')return '';const ik=oaImgKey(k);if(icons[ik])return ik;const png=await oaImgPng(k);if(!png)return '';icons[ik]=png;return ik};
  await Promise.all(apps.filter(a=>APPS[a.k]&&typeof APPS[a.k].img=='function').map(async a=>{const ik=await own(a.k);if(ik)a.ic=ik}));
  /* (1.6.0 Part 12, item 1) the desktop's icons as this account sees them (the shortcuts, not the files yet), their
     places (bd95cells:ID) and the icon size, for the native shell's desktop */
  const desk=[];try{const D=document.getElementById('desk');if(D&&typeof icTarget=='function')for(const d of D.querySelectorAll(':scope > .ic')){
   if(d.style.display=='none')continue;
   /* (1.6.0 Part 12, item 1's second part) the files and folders in the Desktop folder: their names, pictures, places */
   if(d.classList.contains('fic')){const im=d.querySelector('img.ico'),e=im?im.dataset.emo||'':((d.querySelector('b')||{}).textContent||'').trim(),fn=d.dataset.fn,v=(deskFiles()||{})[fn];
    const k=typeof EMO2IC=='object'?EMO2IC[e]||EMO2IC[e.replace(/\uFE0F/g,'')]:'';if(k&&!icons[k]&&S.icons!=='emoji')try{icons[k]=await oaPng(icSvg(k),48)}catch(x){}
    desk.push({n:fn,k:'',kind:isDir(v)?'dir':'file',ic:k&&icons[k]?k:'',icon:e,cell:'file:'+fn,fp:homePath('DESKTOP').concat(fn).join('\\')});continue}
   const T=icTarget(d),im=d.querySelector('img.ico'),e=im?im.dataset.emo||'':((d.querySelector('b')||{}).textContent||'').trim();
   const k=typeof EMO2IC=='object'?EMO2IC[e]||EMO2IC[e.replace(/\uFE0F/g,'')]:'';if(k&&!icons[k]&&S.icons!=='emoji')try{icons[k]=await oaPng(icSvg(k),48)}catch(x){}
   const ik=T.k&&APPS[T.k]?await own(T.k):'';
   desk.push({n:T.name,k:T.k&&APPS[T.k]?T.k:'',kind:d.classList.contains('tr')?'trash':T.kind,full:d.classList.contains('full')?1:0,own:d.classList.contains('dapp')?1:0,ic:ik||(k&&icons[k]?k:''),icon:e,cell:d.dataset.n||T.name})}}catch(e){console.warn('desk',e)}
  /* (in an app's own window the desktop is laid out at the screen's size, not the window's: the places the page gives
     are the desktop's) */
  if(ONEAPP)try{const D=document.getElementById('desk'),zf=parseFloat(document.documentElement.style.zoom)||1,top=document.body.classList.contains('top'),
    /* (the screen's size from the shell: a page never shown, as do-desk's, does not know its screen) */
    q=ONEAPP.q||new URLSearchParams(''),sw=+q.get('sw')/(devicePixelRatio||1)||screen.width,sh=+q.get('sh')/(devicePixelRatio||1)||screen.height;
   Object.assign(D.style,{position:'fixed',left:'0px',right:'auto',bottom:'auto',top:(top?48:0)+'px',width:(sw/zf-16)+'px',height:(sh/zf-(top?48:42)-16)+'px'});layIcons()}catch(e){}
  let cells={};try{cells=JSON.parse(localStorage.getItem(typeof cellKey=='function'?cellKey():'bd95cells')||'{}')}catch(e){}
  /* (1.6.0 Part 12, "The rule": Native looks as Standard) the start menu's list as the page shows it, row by row: its
     separators, headings, pictures, greyed and indented rows, with each row's writing as the page's styles give it */
  const menu=[];try{const U=document.querySelector('#menu ul');if(U)for(const li of U.children){if(li.tagName=='HR'){menu.push({t:'hr'});continue}
   if(li.tagName!='LI'||getComputedStyle(li).display=='none')continue;const im=li.querySelector('img.ico'),e=im?im.dataset.emo||'':'',k=e&&typeof EMO2IC=='object'?EMO2IC[e]||EMO2IC[e.replace(/\uFE0F/g,'')]:'';
   if(k&&!icons[k]&&S.icons!=='emoji')try{icons[k]=await oaPng(icSvg(k),48)}catch(x){}
   const ik=!im&&li.dataset.v25?await own(li.dataset.v25):'';
   const c=getComputedStyle(li);menu.push({t:'li',a:li.dataset.a||'',v:li.dataset.v25||'',cls:String(li.className||''),txt:li.textContent.trim(),ic:ik||(k&&icons[k]?k:''),emo:e,op:+c.opacity,
    fs:parseFloat(c.fontSize),fw:+c.fontWeight||400,col:c.color,pl:parseFloat(c.paddingLeft),pt:parseFloat(c.paddingTop),pb:parseFloat(c.paddingBottom),ls:parseFloat(c.letterSpacing)||0,up:c.textTransform=='uppercase'})}}catch(e){console.warn('menu',e)}
  /* (1.6.0 Part 12, item 3) each app's own rows in its taskbar menus (private59.js APP_JUMPS: words, picture, key) */
  const jumps={};try{if(typeof APP_JUMPS=='object')for(const k in APP_JUMPS)jumps[k]=APP_JUMPS[k]().map(j=>[j[0],(j[3]&&j[3].icon)||'',(j[3]&&j[3].key)||''])}catch(e){}
  /* (1.6.0, the owner's report 2) the desktop's own sounds' styles (Settings › Volume Controls: each sound's tones, base_v24.html SPRE and PAT),
     which the native shell plays itself at its sign-in and shut-down screens */
  const sndp={};try{if(typeof SNDS=='object'&&typeof PAT=='object')for(const [k] of SNDS)sndp[k]=(typeof SPRE=='object'&&SPRE[k]?SPRE[k].map(x=>x[1]||PAT[k]):[PAT[k]]).map(p=>(p||[]).map(t=>t.slice(0,5)))}catch(e){}
  await rdPost('desktop/apps',JSON.stringify({apps,bg,fx,hide,icons,desk,cells,menu,jumps,sndp,is:+S.is||100,ver:typeof RVER!='undefined'?String(RVER):'',acct:uid()}));OA_CSIG=sig}catch(e){console.warn('desktop/apps',e)}}
/* (N12) the statue changes with the accent colour, its glow's rule and light or dark: given again, and the native shell told */
let OA_ST='';if(typeof stUpdate=='function'){const _su=stUpdate;let t=0;stUpdate=function(){const r=_su.apply(this,arguments);
 try{if(ONEAPP&&oaNative()&&USER&&S.dyn=='oasis'){let mode='light';try{mode=dynResolve().mode}catch(e){}const k=stAnimal()+'|'+mode+'|'+stGlow();
  if(k!==OA_ST){OA_ST=k;clearTimeout(t);t=setTimeout(()=>oaCatalog().then(()=>oaPost({op:'cat'})),600)}}}catch(e){}return r}}
/* (2026-11-16, the owner's Vivobook: Library, Linux and Windows apps did not open on Native, nor show their icons) the apps rudolfd runs
   itself, with no window of the page's (Standard's launch() starts them: lxRun, debian/launch, pkg/run, nativeOpen; a cloud game is Oracle's
   window): the native shell opens their page unseen ("x" in the catalogue) */
function oaExt(k){const a=APPS[k];if(!a)return false;if(a.lxApp||/^native:/.test(k))return true;
 if(typeof CLOUD_GAMES=='object'&&CLOUD_GAMES.some(g=>g.k==k))return true;
 if(a.pkg){try{const p=typeof PKGS=='object'&&PKGS&&(PKGS.installed||[]).find(x=>x.name==a.pkg);return !!(p&&p.kind=='app')}catch(e){}}return false}
/* the apps rudolfd knows (the Library's packages, the Linux and Windows programs), asked for once: on Standard the desktop has them long
   before anything is opened; each native window is a page of its own that signed in just now */
let OA_RDY=null;
function oaAppsReady(){if(!OA_RDY)OA_RDY=Promise.race([(async()=>{if(typeof RD=='undefined'||!RD)return;
  try{if(typeof pkgStatus=='function')await pkgStatus(true)}catch(e){}try{if(typeof lxInstalled=='function')await lxInstalled(true)}catch(e){}
  try{if(typeof debApps=='function'){await debApps();if(typeof lxRegister=='function')lxRegister()}}catch(e){}try{if(typeof nativeRegister=='function')nativeRegister()}catch(e){}})(),
 new Promise(r=>setTimeout(r,12000))]);return OA_RDY}
/* an app's own picture that is not one of the page's (a Linux app's or a Windows program's icon, from its package: a.img, lxImg), as a PNG for
   the native shell, which shows the page's pictures (icons): Standard draws them from the same place */
const OA_IMG={};
async function oaImgPng(k){const a=APPS[k];if(!a||typeof a.img!='function')return null;if(OA_IMG[k])return OA_IMG[k];
 const b=document.createElement('b');b.style.cssText='position:fixed;left:-9999px;top:0;width:48px;height:48px';document.body.appendChild(b);
 try{a.img(b);let im=null;for(let i=0;i<60;i++){im=b.querySelector('img');if(im&&im.complete&&im.naturalWidth)break;await new Promise(r=>setTimeout(r,100))}
  if(!im||!im.naturalWidth)return null;return OA_IMG[k]=await oaPng(im.src,48)}catch(e){return null}finally{b.remove()}}
const oaImgKey=k=>{let h=5381;for(const c of k)h=(h*33+c.codePointAt(0))>>>0;return 'im'+h.toString(36)};
/* (the catalogue given again, at once, when what it lists changed in this page: the apps rudolfd knows came, a desktop icon was added or
   removed (Desktop icons…, Add to desktop): Standard's one page shows such a change at once; the shell read the catalogue only when a page
   signed in, so an icon came when the next window opened) */
let OA_CSIG=null,OA_CT=0;
function oaCatSig(){try{const a=Object.keys(APPS).filter(k=>APPS[k]&&APPS[k].name&&typeof APPS[k].open=='function').sort();
 let df=null;try{const d=USER?deskFiles():null;df=d?Object.keys(d).sort().map(k=>(isDir(d[k])?'d:':'f:')+k):null}catch(e){}
 return JSON.stringify([a,USER?ld(DESKADD(),[]):0,USER?ld(DESKHIDE(),[]):0,USER?ld(HIDEK(),[]):0,Object.keys(OA_IMG).length,df])}catch(e){return ''}}
function oaCatPush(){if(!ONEAPP||!oaNative()||!USER||OA.done)return;clearTimeout(OA_CT);OA_CT=setTimeout(()=>{if((OA_CSIG===null&&!OA.main)||oaCatSig()===OA_CSIG)return;   /* (before the first one, at sign-in, that one has it; unless this page's window changed it) */oaCatalog().then(()=>oaPost({op:'cat'}))},150)}
if(ONEAPP){for(const f of ['pkgRegister','lxRegister','nativeRegister','deskApply'])if(typeof window[f]=='function'){const o=window[f];window[f]=function(){const r=o.apply(this,arguments);try{oaCatPush()}catch(e){}return r}}
 /* (Desktop icons…: the apps rudolfd knows are in its list once this page has them, as on Standard: Ember Reach was missing until Restore) */
 if(typeof deskIconsDlg=='function'){const _di=deskIconsDlg;deskIconsDlg=function(){const W=_di.apply(this,arguments);try{oaAppsReady().then(()=>{const q=W&&W.body&&W.body.querySelector('.dsq');if(q&&W.el&&W.el.isConnected)q.dispatchEvent(new Event('input'))})}catch(e){}return W}}}
const OA_PNG=new Map();
function oaPng(src,n){const ck=n+'|'+src;if(src.length<200000&&OA_PNG.has(ck))return Promise.resolve(OA_PNG.get(ck));return new Promise((res,rej)=>{const im=new Image();im.onload=()=>{try{const c=document.createElement('canvas');c.width=c.height=n;c.getContext('2d').drawImage(im,0,0,n,n);const u=c.toDataURL('image/png');if(src.length<200000)OA_PNG.set(ck,u);res(u)}catch(e){rej(e)}};im.onerror=()=>rej(new Error('icon'));im.src=src})}
/* (the owner's Vivobook report of 2026-11-16, "desktop icons take extremely long to load, if they load at all") the native desktop drew
   its icons from the catalogue a page gave 2.5 s after signing in, without telling the shell, which read it again only when the
   settings' "gen" changed: the shell's own pages that sign in at once (#app=do-start, opened first at sign-in, and the service page)
   give it as soon as they are signed in and tell the shell (op "cat"), as Standard's page shows its icons once signed in */
{const _ul=userLogin;userLogin=async function(){const r=await _ul.apply(this,arguments);
 if(ONEAPP&&oaNative()&&/^do-(start|svc)$/.test(ONEAPP.k))setTimeout(()=>oaCatalog().then(()=>{if(!OA.done)oaPost({op:'cat'})}),0);else setTimeout(oaCatalog,2500);if(ONEAPP&&oaNative())oaAppsReady().then(()=>setTimeout(oaCatPush,3000));return r}}
/* (1.6.0 Part 12, item 3) a window's taskbar button's menu is the page's own (winMenu: its rows, the app's jumps, Print, Force
   quit): the shell reads it from the window's page (__oaWinMenu: the rows as JSON) and has a row the shell does not do itself
   done by the page (__oaWinMenuDo: the row with those words clicked) */
function oaMenuRead(){const m=document.querySelector('.ctx');if(!m)return null;return{title:(m.querySelector('.cmh')||{}).textContent||'',items:[...m.children].map(e=>e.tagName=='HR'?{sep:1}:e.classList.contains('cmi')?{label:e.querySelector('span').textContent,icon:e.querySelector('i').textContent,key:e.querySelector('kbd').textContent,dis:e.classList.contains('dis')?1:0}:null).filter(x=>x)}}
function oaMenuShut(){document.querySelectorAll('.ctx').forEach(c=>c.remove());try{cmenuClose()}catch(e){}}
if(ONEAPP){window.__oaWinMenu=()=>{OA.noCtx=1;try{if(!OA.main)return'';winMenu(OA.main,0,0);const r=oaMenuRead();oaMenuShut();return r?JSON.stringify(r):''}catch(e){oaMenuShut();return''}finally{OA.noCtx=0}};
 window.__oaWinMenuDo=l=>{OA.noCtx=1;try{if(!OA.main)return;winMenu(OA.main,0,0);const it=[...document.querySelectorAll('.ctx .cmi')].find(e=>e.querySelector('span').textContent==l);if(it&&!it.classList.contains('dis'))it.click();else oaMenuShut()}catch(e){oaMenuShut()}finally{OA.noCtx=0}}}
/* (1.6.0 Part 12, test finding N7) Chromium's folder picker (showDirectoryPicker), which File Explorer's "Folders on this device" uses
   (Import a folder…, Export selection to a folder…), on the native desktop's WebKit windows: the shell shows the folder dialog and reads
   and writes inside the folder chosen only (rudolf-native hfs_msg); the handles here have what the page uses of Chromium's (name,
   kind, entries/values/keys, getDirectoryHandle and getFileHandle with create, getFile, createWritable's write and close) */
if(ONEAPP&&oaNative()&&!window.showDirectoryPicker){let n=0;const W={};
 window.__oaHfsIn=(id,j)=>{const w=W[id];delete W[id];if(!w)return;let o=null;try{o=JSON.parse(j)}catch(e){}if(!o||o.err)w[1](new DOMException(o?o.msg:'Failed',o?o.err:'UnknownError'));else w[0](o)};
 const ask=(d,m)=>new Promise((res,rej)=>{const id='h'+(++n);W[id]=[res,rej];oaPost(Object.assign({op:'hfs',id,do:d},m))});
 const sub=(p,x)=>p?p+'/'+x:x;
 const fileH=(t,p,name)=>({kind:'file',name,
  async getFile(){const o=await ask('read',{t,p});const b=atob(o.b),u=new Uint8Array(b.length);for(let i=0;i<b.length;i++)u[i]=b.charCodeAt(i);return new File([u],name,{lastModified:o.m||Date.now()})},
  async createWritable(){const parts=[];const dirp=p.split('/').slice(0,-1).join('/');return{async write(d){parts.push(d&&d.data!==undefined?d.data:d)},
   async close(){const u=new Uint8Array(await new Blob(parts).arrayBuffer());let s='';for(let i=0;i<u.length;i+=32768)s+=String.fromCharCode.apply(null,u.subarray(i,i+32768));await ask('write',{t,p:dirp,n:name,b:btoa(s)})},async abort(){parts.length=0}}}});
 const dirH=(t,p,name)=>{const h={kind:'directory',name,
  async *entries(){const o=await ask('ls',{t,p});for(const [x,d] of o.l||[])yield[x,d?dirH(t,sub(p,x),x):fileH(t,sub(p,x),x)]},
  async *values(){for await(const [,e] of h.entries())yield e},async *keys(){for await(const [k] of h.entries())yield k},
  async getDirectoryHandle(x,o){const st=await ask('stat',{t,p,n:x});if(!st.there){if(!(o&&o.create))throw new DOMException('A requested file or directory could not be found.','NotFoundError');await ask('mkdir',{t,p,n:x})}
   else if(!st.dir)throw new DOMException('The path supplied exists, but was not an entry of requested type.','TypeMismatchError');return dirH(t,sub(p,x),x)},
  async getFileHandle(x,o){const st=await ask('stat',{t,p,n:x});if(!st.there){if(!(o&&o.create))throw new DOMException('A requested file or directory could not be found.','NotFoundError');await ask('write',{t,p,n:x,b:''})}
   else if(st.dir)throw new DOMException('The path supplied exists, but was not an entry of requested type.','TypeMismatchError');return fileH(t,sub(p,x),x)}};
  h[Symbol.asyncIterator]=h.entries;return h};
 window.showDirectoryPicker=async o=>{const r=await ask('pick',{mode:o&&o.mode=='readwrite'?'readwrite':'read'});return dirH(r.t,'',r.name)}}
/* (1.6.0 Part 12, test finding N21) the apps' right-click menus (the page's shared cmenu: File Explorer's, a text box's...) over the screen,
   as Standard's page shows them, not cut off at the window's edge: the native shell shows the rows (op "ctx", rudolf-native ctx_msg)
   and the row chosen is done here (__oaCtxDo: its index, and which menu it was) */
if(ONEAPP&&oaNative()&&typeof cmenu=='function'){const _cm=cmenu;let gen=0,acts=[];
 window.__oaCtxDo=(i,g)=>{if(g!==gen)return;const f=acts[i];acts=[];if(f)try{f()}catch(e){console.warn(e)}};
 cmenu=function(x,y,items,opt){if(OA.noCtx||!OA.main||!OA.main.el||!OA.main.el.isConnected)return _cm.apply(this,arguments);opt=opt||{};
  try{document.querySelectorAll('.ctx').forEach(c=>c.remove());cmenuClose()}catch(e){}
  const rows=[];acts=[];let sep=true;
  for(const it of items||[]){if(!it)continue;if(it=='-'){if(!sep){rows.push({sep:1});sep=true}continue}
   const[lab,fn,en,o]=it,off=en===false;rows.push({label:String(lab),icon:o&&o.check!==undefined?(o.check?'✓':''):String(o&&o.icon||''),key:String(o&&o.key||''),dis:off?1:0});acts.push(off?null:fn);sep=false}
  if(rows.length&&rows[rows.length-1].sep)rows.pop();
  const zf=parseFloat(document.documentElement.style.zoom)||1,ae=document.activeElement;gen++;
  /* (Chromium rings the menu it focuses only when nothing had the focus: not after a click in a list or a text box) */
  oaPost({op:'ctx',x:String(x*zf),y:String(y*zf),gen:String(gen),title:String(opt.title||''),items:JSON.stringify(rows),ring:!ae||ae===document.body||ae===document.documentElement?'1':'0'});
  return document.createElement('div')}}
/* (1.6.0 Part 12, test finding N3) Task Manager's Apps on the native desktop: every window, as Standard's one page has them in wins. The
   shell gives its list (rudolf-native wins_ask: each window the taskbar would show, in any workspace, oldest first, with its page's
   own __oaMe: the weight Task Manager estimates from the window); this page's own window is its OA.main, the others stand-ins whose
   Switch to and End task the shell does (op "winop"; End task closes the window as Standard's does, without asking: __oaEnd) */
if(ONEAPP&&oaNative()){let OW=null,waitL=[];
 window.__oaMe=()=>{const m=OA.main;if(!m||!m.el||!m.el.isConnected)return null;let mb=null;try{mb=domWeight(m.el).mb}catch(e){}return{mb}};
 window.__oaWinsIn=j=>{try{const l=JSON.parse(j);OW=Array.isArray(l)?l:null;OA.winsAt=Date.now()}catch(e){OW=null}const f=waitL;waitL=[];f.forEach(r=>r())};
 window.__oaEnd=()=>{const m=OA.main;if(m&&m.el&&m.el.isConnected){try{m.onclose=null}catch(e){}try{m.close()}catch(e){oaDone()}}else oaDone()};
 window.oaWinsAsk=()=>new Promise(r=>{waitL.push(r);oaPost({op:'wins'});setTimeout(r,1000)});
 window.oaWins=()=>!OW?wins:OW.map(o=>{if(o.w&&o.w==ONEAPP.w&&OA.main)return OA.main;
  const el=document.createElement('div');el.className='win'+(o.act?' act':'');const t=document.createElement('span');t.className='t';t.textContent=String(o.title||'');el.appendChild(t);
  return{oaFake:1,app:String(o.app||''),icon:String(o.icon||''),min:!!o.min,ws:+o.ws||1,el,tb:{remove(){}},oaMb:o.me&&typeof o.me.mb=='number'?o.me.mb:null,
   focus(){oaPost({op:'winop',id:String(o.id),do:'focus'})},close(){oaPost({op:'winop',id:String(o.id),do:'end'})}}})}
/* (1.6.0 Part 12, item 2) the desktop's widgets are the native shell's to draw: an app's own page draws none (no clock, monitor or
   weather timers in every window), and takes the shell's list into the page's storage (bd95widgets:ID), where Standard and the
   other engines read it; rudolfd gives the shell's list while the page's is the one the shell replaced ("wgown") */
if(ONEAPP){wgDraw=function(){wgTimers.forEach(clearInterval);wgTimers=[];DESK.querySelectorAll('.wgt').forEach(x=>x.remove())};
 const _u2=userLogin;userLogin=async function(){const r=await _u2.apply(this,arguments);try{if(typeof RD!='undefined'&&RD&&USER&&oaNative()){const s=await rd('desktop/settings');
  if(s&&s.wgown&&Array.isArray(s.widgets)&&s.acct==uid()&&JSON.stringify(s.widgets)!==localStorage.getItem(WGK())){WG=s.widgets;svj(WGK(),s.widgets)}}}catch(e){}return r}}
/* (1.6.0 Part 12, item 1's second part) given again when the Desktop folder's files change */
/* (the owner's Vivobook, 2026-11-16: a folder made from the desktop's menu was in File Explorer's Desktop folder long before it was on
   the desktop) on the native desktop at once (a change seen within half a second), the shell told (op "cat"), by the page that changed
   the files and by the service page (the other windows read them again too, and say nothing) */
{let t=0,last=null;EXPL.add(()=>{try{const k=Object.keys(deskFiles()||{}).sort().map(n=>(isDir(deskFiles()[n])?'d:':'f:')+n).join('|');
 if(last!==null&&k!=last&&typeof USER!='undefined'&&USER&&!(ONEAPP&&OA.done)){clearTimeout(t);
  if(ONEAPP&&oaNative()){if(!OA_FSIN||OA_SVC)t=setTimeout(()=>oaCatalog().then(()=>{if(!OA.done)oaPost({op:'cat'})}),20)}else t=setTimeout(oaCatalog,1200)}last=k}catch(e){}})}
let OA_WP='';
async function oaWallpaper(){if(typeof RD=='undefined'||!RD||!USER||USER.r=='guest')return;const id=S.wpimg||'';
 if(!id||id!=WPID||!/^data:image\/jpeg;base64,/.test(WPURL||''))return;const key=uid()+':'+id;if(OA_WP==key)return;OA_WP=key;
 try{const b=atob(WPURL.slice(WPURL.indexOf(',')+1)),u=new Uint8Array(b.length);for(let i=0;i<b.length;i++)u[i]=b.charCodeAt(i);
  await rdPost('desktop/wallpaper?acct='+encodeURIComponent(uid())+'&img='+encodeURIComponent(id),u)}catch(e){OA_WP='';console.warn('desktop/wallpaper',e)}}
{const _wl=wpLoad;wpLoad=function(){const p=_wl.apply(this,arguments);Promise.resolve(p).then(()=>setTimeout(oaWallpaper,0)).catch(()=>{});return p}}

/* (1.6.0) what the native desktop's shell does for the whole screen, so the page in its window does not do it again (rudolf-native's
   window.__oaShell): the screen's colour (Night light's orange layer and High contrast's contrast: the X screen's gamma; the page keeps
   the saturation), the magnifier (the whole screen zoomed round the pointer) and the on-screen keyboard (a window of its own, its keys
   typed into the page of the window in front: __oaVkIn); and what it hands the page: rudolfd's events that are a page's (__oaEv: the
   shell has the power key, the keys' indicator and Task Manager), the global keys the page in front does (__oaShort), Task Manager
   brought up by Ctrl+Alt+Del again (__oaTmUrgent) */
let OA_VK=0;
/* (Oracle's page in Rudolf's Chromium on the native desktop has no user script of the shell's: the shell does these there too) */
if(ONEAPP&&(window.__oaShell&&oaNative()||typeof RD!='undefined'&&RD&&RD.engine=='native'&&!oaNative())){const SH=window.__oaShell||{gamma:1,mag:1,vk:1},H=document.documentElement;
 if(SH.gamma){H.classList.add('oagamma');const st=document.createElement('style');st.textContent='html.oagamma #nightl{display:none!important}';document.head.appendChild(st);
  if(typeof applyCB=='function'){const _cb=applyCB;applyCB=function(){const r=_cb.apply(this,arguments);if(S.cb=='contrast')H.style.filter='saturate(1.2)';return r}}}
 if(SH.mag&&typeof magApply=='function'){magApply=function(){const B=document.body;B.classList.toggle('magon',!!S.mag);B.style.transform=S.rot?`rotate(${S.rot}deg)`:'';B.style.transformOrigin=''};window.__magApply=magApply;try{magApply()}catch(e){}}
 if(SH.vk&&typeof vkSync=='function'){const _vs=vkSync;vkSync=function(){if(OA_VK){if(!vkShown){vkShown=true;oaPost({op:'vk',x:'1'})}vkEl.style.display='block';return}
  _vs.apply(this,arguments);try{vkEl.style.display='none';vkBtn.style.display='none'}catch(e){}}}
 const ED='input:not([type=checkbox]):not([type=radio]):not([type=range]):not([type=color]):not([type=file]):not([type=button]),textarea,[contenteditable=true],[contenteditable=""]';
 window.__oaVkIn=(c,sp)=>{try{const a=document.activeElement;if(a&&a.matches&&a.matches(ED))vkTarget=a;if(!vkTarget||!vkTarget.isConnected)return;if(sp)vkSpecial(sp);else if(c)vkInsert(c)}catch(e){console.warn(e)}}}
if(ONEAPP){
 window.__oaEv=t=>{try{evDispatch(String(t||'').split('\n').filter(l=>!/^power \d/.test(l)&&!/^osd /.test(l)&&l!='taskmgr').join('\n'))}catch(e){console.warn(e)}};
 window.__oaShort=id=>{const s=typeof SHORT=='object'?SHORT.find(x=>x.id==id):null;if(s)try{s.fn()}catch(e){console.warn(e)}};
 window.__oaTmUrgent=()=>{try{if(typeof tmUrgent=='function')tmUrgent()}catch(e){console.warn(e)}}}
/* (1.6.0) an app's window, and a page the native shell opens to do something (#app=do-qs for each quick settings' switch), never offers
   Standard's move to the browser shell ("Real browser tabs are ready"): its shell/switch restarted the whole native desktop, at every
   switch pressed in the QEMU check on the real release (the same guard as shell26.js's in the Vivobook reports' fix; either is enough) */
if(ONEAPP&&typeof shellOffer=='function'&&typeof RD!='undefined'&&RD&&RD.engine=='native')shellOffer=async()=>{};
/* (1.6.0 Part 12, item 12) remote access's question ("Let … see this screen?") is asked once, by the desktop's service page, above every
   window; each window still shows the red bar while someone is connected (svc60.js) */
if(ONEAPP&&!OA_SVC&&typeof RD!='undefined'&&RD&&RD.engine=='native'&&typeof rmAsk=='function')rmAsk=()=>{};
/* (1.6.0 Part 12, item 12) the Clock's timers (apps25b.js TIMERS) are the desktop's: on Standard they live in its one page; here the
   Clock's window makes them and the service page (do-svc) rings them, also once the Clock's window is closed. They are kept in the
   page's storage (bd95timers:ID, with when each was made) and told at once to the other windows (WebKit's storage may reach them
   seconds later): __tmrOut after a change, __tmrIn before each look (the newest wins). Standard's page has neither. */
const OA_TMR={v:0,bc:null};
const oaTmrKey=()=>'bd95timers:'+uid();
function oaTmrRead(){try{const o=JSON.parse(localStorage.getItem(oaTmrKey())||'null');return o&&Array.isArray(o.l)?o:null}catch(e){return null}}
function oaTmrWrite(l){const o={v:Date.now()*1000+Math.floor(Math.random()*1000),l};OA_TMR.v=o.v;try{localStorage.setItem(oaTmrKey(),JSON.stringify(o))}catch(e){}try{OA_TMR.bc&&OA_TMR.bc.postMessage({id:uid(),o})}catch(e){}}
function oaTmrTake(o){if(!o||!(o.v>OA_TMR.v)||!Array.isArray(o.l))return;OA_TMR.v=o.v;TIMERS.splice(0,TIMERS.length,...o.l.filter(t=>t&&+t.end>0).map(t=>({end:+t.end,label:String(t.label||'Timer'),total:+t.total||0,c:+t.c||0,done:t.done?1:0})))}
if(ONEAPP&&oaOut()&&typeof TIMERS=='object'){
 try{if(typeof BroadcastChannel=='function'){OA_TMR.bc=new BroadcastChannel('rudolf-timers');OA_TMR.bc.onmessage=e=>{const m=e.data||{};if(USER&&m.id==uid())oaTmrTake(m.o)}}}catch(e){}
 window.__tmrIn=()=>{if(USER)oaTmrTake(oaTmrRead())};
 window.__tmrOut=()=>{if(USER)oaTmrWrite(TIMERS.map(t=>({end:t.end,label:t.label,total:t.total,c:t.c||0,done:t.done?1:0})))}}
/* (the owner's Vivobook, 2026-11-16: "selecting a new background still doesn't apply immediately as a full-size preview on Native in the same
   way that the Chromium desktop does") Display Properties' choices are previewed on Standard on the whole desktop at once, before Apply
   (the page's live S, applySet), and go back on Revert, Discard or closing without applying. Here the desktop is the native shell's:
   Settings' window gives it each choice at once (op "prev": its S, and a picture of your own that only this page has until applied),
   and that it is over (an empty "prev": applied, reverted or discarded); the shell goes back by itself when this window closes. The
   screen saver's Preview is full screen on Standard: here the shell's (op "saver"), as its own screen saver is */
if(ONEAPP&&ONEAPP.k=='settings'&&oaNative()){let t=0,was=null,base=null,how='';
 /* (previewed: what differs from the settings as they were when the window opened or were last applied; the shell draws the saved ones) */
 const cur=()=>{try{return JSON.stringify(S)}catch(e){return null}};
 const send=(applied)=>{if(applied||!OA.main){base=cur();how=applied||'';if(!OA.main)return}if(base===null)return;clearTimeout(t);t=setTimeout(async()=>{if(!USER||OA.done)return;const s=cur();if(s===null)return;
  if(s===base){if(was===null)return;was=null;oaPost({op:'prev',s:'',keep:how=='secCommit'?'1':''});return}   /* (applied: the shell keeps drawing these until the saved settings reach it, op "set") */
  const own=S.wpimg&&typeof SC=='object'&&SC&&S.wpimg!==SC.wpimg,wp=own&&typeof WPID!='undefined'&&WPID==S.wpimg&&/^data:image\//.test(WPURL||'')?WPURL:'';
  if(own&&!wp)return;   /* (its picture still loading: given once it is there, wpLoad's applySet) */
  const k=s+'|'+(wp?S.wpimg:'');if(k===was)return;was=k;oaPost({op:'prev',s,wp:wp&&wp.length<24e6?wp:''})},0)};
 {const _as=applySet;applySet=function(){const r=_as.apply(this,arguments);send(0);return r}}
 {const _ul=userLogin;userLogin=async function(){const r=await _ul.apply(this,arguments);if(!OA.main)base=cur();return r}}
 /* (applied, or back to the saved settings: Apply, Revert, Discard, Reset to defaults) */
 for(const f of ['secCommit','secRevert'])if(typeof window[f]=='function'){const o=window[f];window[f]=function(){const r=o.apply(this,arguments);send(f);return r}}
 if(typeof ssRun=='function'){const _sr=ssRun;ssRun=function(o){if(o&&o.preview){oaPost({op:'saver',style:String(o.style||'')});return null}return _sr.apply(this,arguments)}}}
