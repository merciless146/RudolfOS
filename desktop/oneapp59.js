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
    Explorer's sort and view, the Media Player's speed...). (N5, N13, N9) Chromium's own controls, measured from its drawing, as the
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
 /* (1.5.9, plan 16.2) a Tor window on Standard: no native session there; the desktop page gave rudolfd its account and file key
    when it asked for the window (torwin/session), so the window is signed in as the desktop is, not at a sign-in screen */
 if(!(s&&s.id)&&typeof RD!='undefined'&&RD&&ONEAPP&&ONEAPP.q.get('tor')=='1'){try{s=await rd('torwin/session')}catch(e){s=null}}
 let a=null;
 if(s&&s.id){a=s.r=='guest'?{n:'Guest',r:'guest',id:'guest',h:''}:ACCTS.find(x=>x.id==s.id)||null}
 if(!a){OA.login=1;return showLogin0()}
 try{LG.style.display='none'}catch(e){}
 USER=a;
 try{const ks={pk:oaHex(s.pk),fk:oaHex(s.fk)};if(a.r!='guest'&&!ks.pk&&!ks.fk)throw new Error('no keys');await userLogin(a,null,a.r=='guest'?null:ks)}
 catch(e){console.warn('one-app sign-in',e);OA.login=1;USER=ACCTS[0]||null;return showLogin0()}
 try{userUI()}catch(e){}}
function oaOpen(){if(OA.opened||!ONEAPP)return;OA.opened=1;
 try{LG.style.display='none'}catch(e){}
 const da0=/^do-/.test(ONEAPP.k)?ONEAPP.k.slice(3):'';if(da0=='desk'||da0=='fs'){OA.want=1;(da0=='desk'?oaDesk:oaFs)();return}
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
  if(da0=='pin'){try{pinSet(q.get('k')||'',q.get('on')=='1')}catch(e){}return oaFin({})}
  /* (1.6.0 Part 12, item 3) quick settings' switches and the notifications' centre as the native shell draws them: the page's own
     panels made unseen and the same button pressed (#app=do-qs&item=WORDS; #app=do-nc&op=clear|dnd&on=0|1|dismiss&i=N|why&i=N) */
  /* (1.6.0) on=0|1: the switch as the shell shows it now, so a page that had not heard of a change yet does not undo it; the screen
     reader's "Screen reader on" said before the page closes (srSpeak: the voices', or rudolfd's say) */
  if(da0=='qs'){let sr=0;try{qsToggle();const it=q.get('item'),on=q.get('on'),b=[...document.querySelectorAll('#qsp .qst')].find(x=>(x.querySelector('span')||{}).textContent==it);
    if(b&&(on==null||(b.classList.contains('on')?'1':'0')!=on)){b.click();sr=it=='Screen reader'&&!!S.sr}qsClose()
    /* (High contrast and Touch mode are kept as S.cb and S.touch, which the tile's own copy to SC (its keys hc, tm) leaves out: on Standard
       the one page shows them at once; here the other windows and the shell read what is saved) */
    if(SC){SC.cb=S.cb;SC.touch=S.touch;persistS()}}catch(e){console.warn(e)}
   if(!sr)return oaFin({});const t0=Date.now(),said=()=>{const a=window.__srAudio,ss=window.speechSynthesis;if(Date.now()-t0>8000||(Date.now()-t0>1500&&!(ss&&ss.speaking)&&!(a&&!a.ended&&!a.paused)))return oaFin({});setTimeout(said,250)};setTimeout(said,400);return}
  /* (1.6.0) the native desktop's on-screen keyboard: the page's own (#vkb, its keys for the language chosen) filling a window of its own
     that never takes the keyboard; a key goes to the shell, which gives it to the page of the window in front (__oaVkIn) */
  if(da0=='vk'&&typeof vkDraw=='function'){OA.want=1;OA_VK=1;const H=document.documentElement;H.classList.add('oavk');
   {const st=document.createElement('style');st.textContent=`html.oavk,html.oavk body{background:#e5e7eb!important;overflow:hidden!important;zoom:1!important}html.oavk body>*:not(#vkb){visibility:hidden!important}
html.oavk #vkb{display:block!important;visibility:visible!important;position:fixed!important;left:0!important;top:0!important;right:0!important;bottom:auto!important;transform:none!important;width:100%!important;max-width:none!important;border:0!important;border-radius:0!important;box-shadow:none!important;zoom:var(--oavkz,1)}`;document.head.appendChild(st)}
   vkInsert=c=>oaPost({op:'vk',c:String(c)});vkSpecial=k=>oaPost({op:'vk',sp:String(k)});
   vkShown=true;S.vk=true;try{vkDraw();vkEl.style.display='block'}catch(e){console.warn(e)}document.title='On-screen keyboard';
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
 try{if(act)act();else if(ONEAPP.file&&app.openFile)launchWith(ONEAPP.k,resolve(ONEAPP.file,['C:']));
  else if(ONEAPP.k=='taskmgr'&&ONEAPP.q.get('urgent')=='1'&&typeof tmUrgent=='function')tmUrgent();   /* (1.6.0: Ctrl+Alt+Del's, tmtop57.js) */
  else launch(ONEAPP.k)}catch(e){console.warn(e)}
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
 try{const apps=Object.values(APPS).filter(a=>a&&a.k&&a.name&&typeof a.open=='function').map(a=>({k:a.k,name:String(a.name),icon:String(a.icon||''),kw:String(a.kw||'')}));
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
  await rdPost('desktop/apps',JSON.stringify({apps,bg,fx,hide,icons,desk,cells,menu,jumps,is:+S.is||100,ver:typeof RVER!='undefined'?String(RVER):'',acct:uid()}))}catch(e){console.warn('desktop/apps',e)}}
/* (N12) the statue changes with the accent colour, its glow's rule and light or dark: given again, and the native shell told */
let OA_ST='';if(typeof stUpdate=='function'){const _su=stUpdate;let t=0;stUpdate=function(){const r=_su.apply(this,arguments);
 try{if(ONEAPP&&oaNative()&&USER&&S.dyn=='oasis'){let mode='light';try{mode=dynResolve().mode}catch(e){}const k=stAnimal()+'|'+mode+'|'+stGlow();
  if(k!==OA_ST){OA_ST=k;clearTimeout(t);t=setTimeout(()=>oaCatalog().then(()=>oaPost({op:'cat'})),600)}}}catch(e){}return r}}
function oaPng(src,n){return new Promise((res,rej)=>{const im=new Image();im.onload=()=>{try{const c=document.createElement('canvas');c.width=c.height=n;c.getContext('2d').drawImage(im,0,0,n,n);res(c.toDataURL('image/png'))}catch(e){rej(e)}};im.onerror=()=>rej(new Error('icon'));im.src=src})}
{const _ul=userLogin;userLogin=async function(){const r=await _ul.apply(this,arguments);setTimeout(oaCatalog,2500);return r}}
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
 window.__oaWinsIn=j=>{try{const l=JSON.parse(j);OW=Array.isArray(l)?l:null}catch(e){OW=null}const f=waitL;waitL=[];f.forEach(r=>r())};
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
{let t=0,last=null;EXPL.add(()=>{try{const k=Object.keys(deskFiles()||{}).sort().join('|');
 if(last!==null&&k!=last&&typeof USER!='undefined'&&USER&&!(ONEAPP&&OA.done)){clearTimeout(t);t=setTimeout(oaCatalog,1200)}last=k}catch(e){}})}
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
