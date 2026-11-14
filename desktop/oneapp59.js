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
/* (an app's own window is not the desktop: its icons' grid is the screen's, not the window's, or every window opened, or a page never
   shown, as do-desk's, would squeeze the desktop's places into its own size and keep them) */
if(ONEAPP&&typeof gridInfo=='function'){const _gi=gridInfo;gridInfo=function(){const g=_gi();try{const q=ONEAPP.q,zf=parseFloat(document.documentElement.style.zoom)||1,top=document.body.classList.contains('top'),
 sw=+q.get('sw')/(devicePixelRatio||1)||screen.width,sh=+q.get('sh')/(devicePixelRatio||1)||screen.height,W=sw/zf-16,H=sh/zf-(top?48:42)-16;
 if(W>0&&H>0){g.nc=Math.max(1,Math.floor((W-8)/g.w));g.nr=Math.max(1,Math.floor((H-8)/g.h))}}catch(e){}return g}}
const oaNative=()=>!!(window.webkit&&window.webkit.messageHandlers&&window.webkit.messageHandlers.rudolfNative);
/* (w: which of the shell's windows this is, from #app=NAME&w=N: WebKit gives every window the native shell opens the same
   message handler, so the shell knows the sender by it) */
function oaPost(m){try{if(oaNative())window.webkit.messageHandlers.rudolfNative.postMessage(Object.assign({w:ONEAPP?ONEAPP.w:0},m))}catch(e){}}
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
 {const _l=launch;launch=function(k){if(OA.main&&k!=ONEAPP.k&&oaNative()&&APPS[k]){oaPost({op:'open',app:k});return}return _l.apply(this,arguments)}}
 {const _lw=launchWith;launchWith=function(k,p){if(OA.main&&k!=ONEAPP.k&&oaNative()&&APPS[k]){oaPost({op:'open',app:k,file:Array.isArray(p)?p.join('\\'):String(p||'')});return}return _lw.apply(this,arguments)}}
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
 const da=da0,act=da?(typeof A=='object'&&typeof A[da]=='function'?A[da]:da=='blitzen'&&typeof blitzenApp=='function'?blitzenApp:null):null;
 const app=APPS[ONEAPP.k]||(act?{name:da,open:act}:null);
 if(!app){oaClosed('There is no app called “'+ONEAPP.k+'”.');oaPost({op:'close'});return}
 OA.want=1;
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
 oaFin({err:'Unknown: '+op})}
function oaMain(W){OA.main=W;OA.want=0;
 /* (1.6.0 Part 12) the size the page gives the app's window (its 1 px edge and title bar included), for the native frame */
 oaPost({op:'size',ww:String(Math.round(parseFloat(W.el.style.width)||0)),wh:String(Math.round(parseFloat(W.el.style.height)||0))});W.el.classList.add('oamain');
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
   desk.push({n:T.name,k:T.k&&APPS[T.k]?T.k:'',kind:d.classList.contains('tr')?'trash':T.kind,full:d.classList.contains('full')?1:0,ic:k&&icons[k]?k:'',icon:e,cell:d.dataset.n||T.name})}}catch(e){console.warn('desk',e)}
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
  await rdPost('desktop/apps',JSON.stringify({apps,bg,hide,icons,desk,cells,menu,is:+S.is||100,ver:typeof RVER!='undefined'?String(RVER):'',acct:uid()}))}catch(e){console.warn('desktop/apps',e)}}
function oaPng(src,n){return new Promise((res,rej)=>{const im=new Image();im.onload=()=>{try{const c=document.createElement('canvas');c.width=c.height=n;c.getContext('2d').drawImage(im,0,0,n,n);res(c.toDataURL('image/png'))}catch(e){rej(e)}};im.onerror=()=>rej(new Error('icon'));im.src=src})}
{const _ul=userLogin;userLogin=async function(){const r=await _ul.apply(this,arguments);setTimeout(oaCatalog,2500);return r}}
/* (1.6.0 Part 12, item 1's second part) given again when the Desktop folder's files change */
{let t=0,last=null;EXPL.add(()=>{try{const k=Object.keys(deskFiles()||{}).sort().join('|');
 if(last!==null&&k!=last&&typeof USER!='undefined'&&USER&&!(ONEAPP&&OA.done)){clearTimeout(t);t=setTimeout(oaCatalog,1200)}last=k}catch(e){}})}
let OA_WP='';
async function oaWallpaper(){if(typeof RD=='undefined'||!RD||!USER||USER.r=='guest')return;const id=S.wpimg||'';
 if(!id||id!=WPID||!/^data:image\/jpeg;base64,/.test(WPURL||''))return;const key=uid()+':'+id;if(OA_WP==key)return;OA_WP=key;
 try{const b=atob(WPURL.slice(WPURL.indexOf(',')+1)),u=new Uint8Array(b.length);for(let i=0;i<b.length;i++)u[i]=b.charCodeAt(i);
  await rdPost('desktop/wallpaper?acct='+encodeURIComponent(uid())+'&img='+encodeURIComponent(id),u)}catch(e){OA_WP='';console.warn('desktop/wallpaper',e)}}
{const _wl=wpLoad;wpLoad=function(){const p=_wl.apply(this,arguments);Promise.resolve(p).then(()=>setTimeout(oaWallpaper,0)).catch(()=>{});return p}}
