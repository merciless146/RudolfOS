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
 return k&&/^[a-z0-9_-]{1,40}$/i.test(k)?{k,file:q.get('file')||'',w:+q.get('w')||0,q}:null}catch(e){return null}})();
const OA={main:null,want:0,opened:0,done:0,login:0};
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
  EXPL.forEach(f=>{try{f()}catch(x){}})}catch(e){console.warn('one-app: the files again',e)}}
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
if(ONEAPP)addEventListener('storage',e=>{try{if(!USER||e.key!='bd95set:'+uid()||!e.newValue)return;const us=JSON.parse(e.newValue);for(const k in us)S[k]=us[k];SC=cln(S);
 try{applySet()}catch(x){}try{a11yApply()}catch(x){}try{themeApply()}catch(x){}try{ncBadge()}catch(x){}}catch(x){console.warn('one-app: the settings again',x)}});
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
/* (1.5.9, docs/plan-1.5.9.md 12.3c) Oracle on the native desktop is this page in the shell, in Rudolf's Chromium (its real tabs):
   the window's title and its closing go to the shell, which is the window (a title kept until the shell has said hello) */
let OA_SHT='';function oaShell(m){if(!ONEAPP)return;if(m.op=='title'){OA_SHT=m.t;if(typeof SHELL!='undefined'&&SHELL)shellSend({rudolfShell:'oa',op:'title',t:m.t})}
 /* (closing: rudolf-launch decides, as Chromium asks "Download is in progress: exit anyway?" when its last window closes during a
    download, which Standard never shows: then the window goes from the screen and its page closes once the downloads are in) */
 else if(m.op=='close'&&typeof SHELL!='undefined'&&SHELL){if(oaChromium()){try{window.rudolfNativeOracle(JSON.stringify({w:ONEAPP.w||0,op:'closing'}))}catch(e){}
  clearTimeout(OA_CLT);OA_CLT=setTimeout(()=>window.__oaCloseNow(),15000)}else shellSend({rudolfShell:'oa',op:'close'})}
 /* (12.3c step 5) the rest goes to the native desktop itself: rudolf-launch's binding on its private pipe, in Oracle's windows only */
 else if(oaChromium())try{window.rudolfNativeOracle(JSON.stringify(Object.assign({w:ONEAPP.w||0},m)))}catch(e){}}
function oaChromium(){return typeof window.rudolfNativeOracle=='function'}
let OA_CLT=0;
window.__oaCloseNow=()=>{clearTimeout(OA_CLT);try{shellSend({rudolfShell:'oa',op:'close'})}catch(e){}};
window.__oaCloseLater=()=>clearTimeout(OA_CLT);
/* (told by rudolf-launch once Chromium's downloads have finished: closed when this page has filed them, as the desktop does) */
window.__oaCloseWhenDone=()=>{const t0=Date.now(),go=()=>{const busy=typeof DLN=='object'&&(DLN.busy||DLN.ing>0||DLN.pend.some(x=>/^(wait|check|approve)$/.test(x.st)));
 if(busy&&Date.now()-t0<120e3)return setTimeout(go,1000);window.__oaCloseNow()};setTimeout(go,2500)};
/* (12.3c step 5) the page's own full screen (shell26.js's stand-in fills the page): Oracle's window on the native desktop is not the
   screen, as the shell's window is on Standard, so the window goes full screen too (rudolf-launch, through the binding) */
if(ONEAPP)window.__oaFs=on=>{if(oaChromium())oaPost({op:'fs',on:on?1:0})};
/* (12.3c step 5) a new private window (Ctrl+Shift+N, ☰ › New private tab) from Oracle's window on the native desktop: a window of its own, as
   on Standard, not one inside this page (the native desktop opens Oracle's jump "New private tab", as from its taskbar button) */
function oaPrivWin(){if(!(ONEAPP&&ONEAPP.k=='browser'&&OA.main&&oaChromium()))return false;oaPost({op:'open',app:'browser',jump:'New private tab'});return true}
/* (the native desktop is there: WebKit's message handler, or Oracle's binding in Rudolf's Chromium) */
const oaOut=()=>oaNative()||oaChromium();
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
 /* the windows of the last session stay closed; the notifications' list is still read (the count) */
 window.__sessionRestore=()=>{try{NC=ld(NCK(),[]);ncUnread=0;ncBadge()}catch(e){}};
 /* only Settings writes the accounts */
 if(ONEAPP.k!='settings'){const _su=saveUser;saveUser=function(){if(OA.opened)return;return _su.apply(this,arguments)}}
 /* the first window made for the app is its main window: it fills the page and gives the page its title */
 {const _w=win;win=function(...a){const W=_w.apply(this,a);if(OA.want&&!OA.main&&W&&W.el)oaMain(W);return W}}
 /* another app: a window of its own when the native shell is there */
 {const _l=launch;launch=function(k){if(OA.main&&k!=ONEAPP.k&&oaOut()&&APPS[k]){oaDrained().then(()=>oaPost({op:'open',app:k}));return}return _l.apply(this,arguments)}}
 /* (Settings at a page, asked by another app's window or a notification's action: Settings' own window, at that page) */
 {const _st=settings;settings=function(sec){if(OA.main&&ONEAPP.k!='settings'&&oaOut()){oaPost({op:'open',app:'settings',page:typeof sec=='string'?sec:''});return}return _st.apply(this,arguments)}}
 {const _lw=launchWith;launchWith=function(k,p){if(OA.main&&k!=ONEAPP.k&&oaOut()&&APPS[k]){const f=Array.isArray(p)?p.join('\\'):String(p||'');oaDrained().then(()=>oaPost({op:'open',app:k,file:f}));return}return _lw.apply(this,arguments)}}
 /* (1.5.9, 12.3c) Oracle's new window (Ctrl+N, a link or a website's new window from inside Oracle, a web page's file from another app:
    browser()) is a window of its own, as on Standard, not a second window inside this page: Oracle's, in Rudolf's Chromium. (Not a
    website's pop-up, whose tab this window's shell already holds with its opener (a sign-in's): adopt, kept here) */
 if(typeof browser=='function'){const _br=browser;browser=function(u,bo){if(!(bo&&bo.adopt)&&(ONEAPP.k!='browser'||(OA.main&&OA.main.el&&OA.main.el.isConnected))&&oaOut()){
  oaPost({op:'open',app:'browser',url:typeof u=='string'&&/^(https?:|about:|file:\/\/\/)/i.test(u)?u:'',priv:bo&&bo.priv?'1':''});return null}return _br.apply(this,arguments)}}
 /* (1.5.9, 12.3c step 5) Open with: the chosen app in a window of its own, as on Standard (not one inside this page): made by the
    native desktop's do-fs page, as its own Open with dialog does (oaFs op=openwith) */
 if(typeof OPENERS!='undefined')OPENERS.forEach(x=>{const f=x[4];if(!f||x[0]==ONEAPP.k)return;x[4]=p=>{if(OA.main&&oaOut()&&Array.isArray(p)){const fp=p.join('\\');oaDrained().then(()=>oaPost({op:'openwith',key:x[0],file:fp}));return}return f(p)}});
 /* the first sign-in screen: the native session's account instead, when there is one */
 {const _sl=showLogin;let first=1;showLogin=function(){if(first&&!OA.opened){first=0;oaStart(_sl);return}if(OA.opened&&OA.done)return;return _sl.apply(this,arguments)}}
 /* after any sign-in: the app */
 {const _ul=userLogin;userLogin=async function(){const r=await _ul.apply(this,arguments);if(!OA.opened)setTimeout(oaOpen,0);return r}}
 /* (the ✕ button, not W.close: apps that ask about unsaved work watch the button) */
 window.__oneAppClose=()=>{const m=OA.main;if(m&&m.el.isConnected){const c=m.el.querySelector('.tb .cl');if(c)c.click();else m.close()}else oaDone()};
}
async function oaStart(showLogin0){
 let s=null;
 if(typeof RD!='undefined'&&RD){try{s=await rd('desktop/session')}catch(e){s=null}}
 let a=null;
 if(s&&s.id){a=s.r=='guest'?{n:'Guest',r:'guest',id:'guest',h:''}:ACCTS.find(x=>x.id==s.id)||null}
 if(!a){OA.login=1;return showLogin0()}
 try{LG.style.display='none'}catch(e){}
 USER=a;
 try{await userLogin(a,null,a.r=='guest'?null:{pk:oaHex(s.pk),fk:oaHex(s.fk)})}
 catch(e){console.warn('one-app sign-in',e);OA.login=1;USER=ACCTS[0]||null;return showLogin0()}
 try{userUI()}catch(e){}}
function oaOpen(){if(OA.opened||!ONEAPP)return;OA.opened=1;
 try{LG.style.display='none'}catch(e){}
 const da0=/^do-/.test(ONEAPP.k)?ONEAPP.k.slice(3):'';if(da0=='desk'||da0=='fs'){OA.want=1;(da0=='desk'?oaDesk:oaFs)();return}
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
  if(da0=='pin'){try{pinSet(q.get('k')||'',q.get('on')=='1')}catch(e){}return oaFin({})}
  /* (1.6.0 Part 12, item 3) quick settings' switches and the notifications' centre as the native shell draws them: the page's own
     panels made unseen and the same button pressed (#app=do-qs&item=WORDS; #app=do-nc&op=clear|dnd&on=0|1|dismiss&i=N|why&i=N) */
  if(da0=='qs'){try{qsToggle();const b=[...document.querySelectorAll('#qsp .qst')].find(x=>(x.querySelector('span')||{}).textContent==q.get('item'));if(b)b.click();qsClose()}catch(e){console.warn(e)}return oaFin({})}
  if(da0=='nc'){const op=q.get('op')||'';try{NC=ld(NCK(),[]);ncToggle(true);const p=document.getElementById('ncp');
    if(op=='clear')p.querySelector('.clr').click();
    else if(op=='dnd'){const c=p.querySelector('.dnd');if(c.checked!=(q.get('on')=='1')){c.checked=q.get('on')=='1';c.dispatchEvent(new Event('change'))}}
    else if(op=='dismiss'||op=='why'){const it=p.querySelectorAll('.nitem')[+q.get('i')||0],b=it&&it.querySelector(op=='why'?'.bzwhy':'.nx');if(op=='why')OA.want=1;if(b)b.click()}
    p.remove()}catch(e){console.warn(e)}
   if(op=='why'){setTimeout(()=>{if(!OA.main&&!OA.done){oaClosed('');oaPost({op:'close'})}},15000);return}return oaFin({})}
  if(da0=='mute'){try{const m=S.snd&&S.snd.master;if(m){m.on=!m.on;trayVol()}}catch(e){}setTimeout(()=>oaFin({}),400);return}}
 /* (1.6.0 Part 12, item 3) a day double-clicked in the calendar under the native shell's clock: the Calendar at that day */
 {const cd=ONEAPP.q.get('day');if(ONEAPP.k=='calendar'&&cd&&/^\d{4}-\d\d-\d\d$/.test(cd))setTimeout(()=>{try{window.__calGo&&__calGo(cd)}catch(e){}},800)}
 const da=da0,act=da?(typeof A=='object'&&typeof A[da]=='function'?A[da]:da=='blitzen'&&typeof blitzenApp=='function'?blitzenApp:null):null;
 const app=APPS[ONEAPP.k]||(act?{name:da,open:act}:null);
 if(!app){oaClosed('There is no app called “'+ONEAPP.k+'”.');oaPost({op:'close'});return}
 OA.want=1;
 /* (1.5.9, 12.3c) Oracle without Rudolf's Chromium on the native desktop (none, or it would not start): said once */
 if(ONEAPP.k=='browser'&&ONEAPP.q.get('nochrome')=='1')setTimeout(()=>{try{notify('Oracle','Websites will work better on Standard: Oracle is running without Rudolf\u2019s Chromium here, so some sites cannot open in its tabs.')}catch(e){}},1500);
 try{if(act)act();else if(ONEAPP.file&&app.openFile)launchWith(ONEAPP.k,resolve(ONEAPP.file,['C:']));else launch(ONEAPP.k)}catch(e){console.warn(e)}
 /* an app that opens no window (or not yet: a large app loads its files first) */
 setTimeout(()=>{if(!OA.main&&!OA.done){oaClosed(app.name+' did not open a window.');oaPost({op:'close'})}},15000)}
/* (1.6.0 Part 12, item 1's second part) the native desktop's menus do what the page's do: #app=do-desk&on=ICON&item=WORDS
   gives the page's own menu for that desktop icon (its name, or a file's name; none: the desktop's) and chooses the item
   with those words, as a click would. What opens a window is that window; what has none is done unseen and the page
   closes; the page's own dialogs that the shell draws itself (Rename, Open with) are reported to it instead. */
function oaFin(m){(async()=>{try{fsFlush()}catch(e){}try{await oaCatalog()}catch(e){}oaPost(Object.assign({op:'done'},m||{}));oaDone()})()}
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
 if(op=='openwith'){const x=OPENERS.find(o=>o[0]==q.get('key'));if(!x)return oaFin({err:'That app cannot open it.'});
  if(q.get('always')=='1'){const d=ld(defK(),{});d[ext(p.at(-1))]=x[0];svj(defK(),d)}
  try{x[4]?x[4](p):openPathDefault(p)}catch(e){console.warn(e)}setTimeout(()=>{if(!OA.main)oaFin({})},1500);return}
 /* (1.6.0 Part 12, item 1) a desktop file dropped on an app's window: what a11y25.js's __dropFileOnWindow does for that app (this
    page's window, if it makes one, is the file's window) */
 if(op=='drop'){const k=q.get('in')||'';let r=false;try{r=!!__dropFileOnWindow({app:k},p)}catch(e){console.warn(e)}
  setTimeout(()=>{if(!OA.main)oaFin(r?{}:{err:'nothing'})},1500);return}
 oaFin({err:'Unknown: '+op})}
function oaMain(W){OA.main=W;OA.want=0;
 /* (1.6.0 Part 12) the size the page gives the app's window (its 1 px edge and title bar included), for the native frame */
 oaPost({op:'size',ww:String(Math.round(parseFloat(W.el.style.width)||0)),wh:String(Math.round(parseFloat(W.el.style.height)||0)),app:String(W.app||'')});W.el.classList.add('oamain');
 const t=W.el.querySelector('.tb .t'),sync=()=>{const s=(t&&t.textContent||'').trim()||(APPS[ONEAPP.k]&&APPS[ONEAPP.k].name)||'Rudolf OS';if(document.title!=s){document.title=s;oaPost({op:'title',t:s,icon:W.icon||''})}};
 sync();if(t)new MutationObserver(sync).observe(t,{childList:true,characterData:true,subtree:true});
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
 try{const apps=Object.values(APPS).filter(a=>a&&a.k&&a.name&&typeof a.open=='function').map(a=>({k:a.k,name:String(a.name),icon:String(a.icon||''),kw:String(a.kw||'')}));
  const bg={};if(typeof DYN=='object')for(const k in DYN){const d=DYN[k];bg[k]={n:d.n||k,d:d.d||'',l:d.l||''}}
  let hide=[];try{hide=ld(HIDEK(),[])}catch(e){}
  /* (1.5.9 step 7) the apps' pictures as the page draws them (icSvg: the icon style, not the emoji, which the system
     may draw pale or not at all), as small PNGs, one per picture */
  const icons={};if(typeof EMO2IC=='object'&&typeof icSvg=='function'&&S.icons!=='emoji')for(const a of apps){const e=a.icon,k=EMO2IC[e]||EMO2IC[e.replace(/\uFE0F/g,'')];
   if(!k)continue;a.ic=k;if(!icons[k])try{icons[k]=await oaPng(icSvg(k),48)}catch(e){delete a.ic}}
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
   desk.push({n:T.name,k:T.k&&APPS[T.k]?T.k:'',kind:d.classList.contains('tr')?'trash':T.kind,full:d.classList.contains('full')?1:0,own:d.classList.contains('dapp')?1:0,ic:k&&icons[k]?k:'',icon:e,cell:d.dataset.n||T.name})}}catch(e){console.warn('desk',e)}
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
   const c=getComputedStyle(li);menu.push({t:'li',a:li.dataset.a||'',v:li.dataset.v25||'',cls:String(li.className||''),txt:li.textContent.trim(),ic:k&&icons[k]?k:'',emo:e,op:+c.opacity,
    fs:parseFloat(c.fontSize),fw:+c.fontWeight||400,col:c.color,pl:parseFloat(c.paddingLeft),pt:parseFloat(c.paddingTop),pb:parseFloat(c.paddingBottom),ls:parseFloat(c.letterSpacing)||0,up:c.textTransform=='uppercase'})}}catch(e){console.warn('menu',e)}
  /* (1.6.0 Part 12, item 3) each app's own rows in its taskbar menus (private59.js APP_JUMPS: words, picture, key) */
  const jumps={};try{if(typeof APP_JUMPS=='object')for(const k in APP_JUMPS)jumps[k]=APP_JUMPS[k]().map(j=>[j[0],(j[3]&&j[3].icon)||'',(j[3]&&j[3].key)||''])}catch(e){}
  await rdPost('desktop/apps',JSON.stringify({apps,bg,hide,icons,desk,cells,menu,jumps,is:+S.is||100,ver:typeof RVER!='undefined'?String(RVER):'',acct:uid()}))}catch(e){console.warn('desktop/apps',e)}}
function oaPng(src,n){return new Promise((res,rej)=>{const im=new Image();im.onload=()=>{try{const c=document.createElement('canvas');c.width=c.height=n;c.getContext('2d').drawImage(im,0,0,n,n);res(c.toDataURL('image/png'))}catch(e){rej(e)}};im.onerror=()=>rej(new Error('icon'));im.src=src})}
{const _ul=userLogin;userLogin=async function(){const r=await _ul.apply(this,arguments);setTimeout(oaCatalog,2500);return r}}
/* (1.6.0 Part 12, item 3) a window's taskbar button's menu is the page's own (winMenu: its rows, the app's jumps, Print, Force
   quit): the shell reads it from the window's page (__oaWinMenu: the rows as JSON) and has a row the shell does not do itself
   done by the page (__oaWinMenuDo: the row with those words clicked) */
function oaMenuRead(){const m=document.querySelector('.ctx');if(!m)return null;return{title:(m.querySelector('.cmh')||{}).textContent||'',items:[...m.children].map(e=>e.tagName=='HR'?{sep:1}:e.classList.contains('cmi')?{label:e.querySelector('span').textContent,icon:e.querySelector('i').textContent,key:e.querySelector('kbd').textContent,dis:e.classList.contains('dis')?1:0}:null).filter(x=>x)}}
function oaMenuShut(){document.querySelectorAll('.ctx').forEach(c=>c.remove());try{cmenuClose()}catch(e){}}
if(ONEAPP){window.__oaWinMenu=()=>{try{if(!OA.main)return'';winMenu(OA.main,0,0);const r=oaMenuRead();oaMenuShut();return r?JSON.stringify(r):''}catch(e){oaMenuShut();return''}};
 window.__oaWinMenuDo=l=>{try{if(!OA.main)return;winMenu(OA.main,0,0);const it=[...document.querySelectorAll('.ctx .cmi')].find(e=>e.querySelector('span').textContent==l);if(it&&!it.classList.contains('dis'))it.click();else oaMenuShut()}catch(e){oaMenuShut()}}}
/* (1.6.0 Part 12, item 2) the desktop's widgets are the native shell's to draw: an app's own page draws none (no clock, monitor or
   weather timers in every window), and takes the shell's list into the page's storage (bd95widgets:ID), where Standard and the
   other engines read it; rudolfd gives the shell's list while the page's is the one the shell replaced ("wgown") */
if(ONEAPP){wgDraw=function(){wgTimers.forEach(clearInterval);wgTimers=[];DESK.querySelectorAll('.wgt').forEach(x=>x.remove())};
 const _u2=userLogin;userLogin=async function(){const r=await _u2.apply(this,arguments);try{if(typeof RD!='undefined'&&RD&&USER&&oaNative()){const s=await rd('desktop/settings');
  if(s&&s.wgown&&Array.isArray(s.widgets)&&s.acct==uid()&&JSON.stringify(s.widgets)!==localStorage.getItem(WGK())){WG=s.widgets;svj(WGK(),s.widgets)}}}catch(e){}return r}}
/* (1.6.0 Part 12, item 1's second part) given again when the Desktop folder's files change */
{let t=0,last=null;EXPL.add(()=>{try{const k=Object.keys(deskFiles()||{}).sort().join('|');
 if(last!==null&&k!=last&&typeof USER!='undefined'&&USER&&!(ONEAPP&&OA.done)){clearTimeout(t);t=setTimeout(oaCatalog,1200)}last=k}catch(e){}})}
let OA_WP='';
async function oaWallpaper(){if(typeof RD=='undefined'||!RD||!USER||USER.r=='guest')return;const id=S.wpimg||'';
 if(!id||id!=WPID||!/^data:image\/jpeg;base64,/.test(WPURL||''))return;const key=uid()+':'+id;if(OA_WP==key)return;OA_WP=key;
 try{const b=atob(WPURL.slice(WPURL.indexOf(',')+1)),u=new Uint8Array(b.length);for(let i=0;i<b.length;i++)u[i]=b.charCodeAt(i);
  await rdPost('desktop/wallpaper?acct='+encodeURIComponent(uid())+'&img='+encodeURIComponent(id),u)}catch(e){OA_WP='';console.warn('desktop/wallpaper',e)}}
{const _wl=wpLoad;wpLoad=function(){const p=_wl.apply(this,arguments);Promise.resolve(p).then(()=>setTimeout(oaWallpaper,0)).catch(()=>{});return p}}
