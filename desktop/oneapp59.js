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
   - The windows of the last session are not reopened (each app window would reopen them), and only Settings writes the
     accounts (all app windows share the same storage). ===== */
const ONEAPP=(()=>{try{const h=String(location.hash||'').replace(/^#/,'');if(!h)return null;const q=new URLSearchParams(h),k=q.get('app');
 return k&&/^[a-z0-9_-]{1,40}$/i.test(k)?{k,file:q.get('file')||'',w:+q.get('w')||0}:null}catch(e){return null}})();
const OA={main:null,want:0,opened:0,done:0,login:0};
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
 /* 1.5.9 (step 7): the development link's banner and its Allow / Refuse question come once, from the native shell,
    not in every app window (where the banner also hid the top of the app: Settings' search box) */
 if(oaNative()){devlBanner=()=>{const b=document.getElementById('devlinkBanner');if(b)b.remove()};devlAsk=()=>{const o=document.getElementById('devlinkAsk');if(o)o.remove()}}
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
 const app=APPS[ONEAPP.k];
 if(!app){oaClosed('There is no app called “'+ONEAPP.k+'”.');oaPost({op:'close'});return}
 OA.want=1;
 try{if(ONEAPP.file&&app.openFile)launchWith(ONEAPP.k,resolve(ONEAPP.file,['C:']));else launch(ONEAPP.k)}catch(e){console.warn(e)}
 /* an app that opens no window (or not yet: a large app loads its files first) */
 setTimeout(()=>{if(!OA.main&&!OA.done){oaClosed(app.name+' did not open a window.');oaPost({op:'close'})}},15000)}
function oaMain(W){OA.main=W;OA.want=0;W.el.classList.add('oamain');
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
  await rdPost('desktop/apps',JSON.stringify({apps,bg,hide,acct:uid(),ver:typeof RVER=='string'?RVER:''}))}catch(e){console.warn('desktop/apps',e)}}
{const _ul=userLogin;userLogin=async function(){const r=await _ul.apply(this,arguments);setTimeout(oaCatalog,2500);return r}}
let OA_WP='';
async function oaWallpaper(){if(typeof RD=='undefined'||!RD||!USER||USER.r=='guest')return;const id=S.wpimg||'';
 if(!id||id!=WPID||!/^data:image\/jpeg;base64,/.test(WPURL||''))return;const key=uid()+':'+id;if(OA_WP==key)return;OA_WP=key;
 try{const b=atob(WPURL.slice(WPURL.indexOf(',')+1)),u=new Uint8Array(b.length);for(let i=0;i<b.length;i++)u[i]=b.charCodeAt(i);
  await rdPost('desktop/wallpaper?acct='+encodeURIComponent(uid())+'&id='+encodeURIComponent(id),u)}catch(e){OA_WP='';console.warn('desktop/wallpaper',e)}}
{const _wl=wpLoad;wpLoad=function(){const p=_wl.apply(this,arguments);Promise.resolve(p).then(()=>setTimeout(oaWallpaper,0)).catch(()=>{});return p}}
