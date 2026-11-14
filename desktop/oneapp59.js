/* ===== Rudolf OS 1.5.9 (plan Part 12, step 1): the page's one-app mode =====
   index.html#app=NAME[&file=C:\PATH] is the desktop page with one app: the native desktop engine (rudolf-native) shows
   each app in a WebKit window of its own with this page (docs/plan-1.5.9.md 12.5). The page:
   - starts as always (storage, the mirror, the accounts), with html.oneapp: no taskbar, desktop icons, widgets or
     wallpaper; no start-up chime, session restore, screen saver, idle lock or heartbeat (the native shell does those,
     and the session must not take an app page for a living desktop: core25.js leaves the heartbeat out);
   - signs in by itself when rudolfd has a native session (GET desktop/session: the account and the key its password
     makes, pk, which deriveKey would make: userLogin takes it in place of the password); without one, its own sign-in
     screen comes first, then the app alone;
   - opens the app (with &file=, the app's openFile), its first window filling the page; with rudolf-native the page's
     title bar is hidden (the native frame draws one), elsewhere it stays without minimise and maximise. The window's
     title is the page's (document.title, and `title` to rudolf-native);
   - when that window closes (its ✕, Ctrl+W, the app's own close, or the native frame's ✕ calling
     window.__oneAppClose(), which closes it as its ✕ does, so an app's "save changes?" still comes), writes its files
     and closes the page (`close` to rudolf-native, and window.close()); a page a script cannot close (a browser tab)
     says "This app is closed", with Open again;
   - another app opened by name (launch) goes to rudolf-native, as a window of its own; elsewhere it opens in the page
     as on the desktop. A window the app itself opens (a dialog, Run Lua's Terminal) stays in the page;
   - tells rudolf-native of each notification it shows (`note`: the tray's count);
   - shares the storage with the other app pages: C:'s system part (fs:system, one record every page writes whole) is
     saved by a three-way merge (what this page last read, what it has, what is stored now: both sides' changes are
     kept, the same file changed on both keeps this page's) under a Web Lock, and a BroadcastChannel tells the other
     pages, which merge it in. Only Settings writes the accounts (bd95user). The home folder is per file on the disk
     already (home56.js). ===== */
const ONEAPP=(()=>{const m=/^#app=([^&]+)(?:&file=(.*))?$/.exec(location.hash||'');if(!m)return null;
 let k=m[1],f=m[2]||null;try{k=decodeURIComponent(k);f=f&&decodeURIComponent(f)}catch(e){}return{k,file:f,main:null,want:false,done:false}})();
/* the native shell's side, when this page is in one of its windows */
const oaNative=()=>!!(window.webkit&&webkit.messageHandlers&&webkit.messageHandlers.rudolfNative);
function oaPost(m){try{if(oaNative())webkit.messageHandlers.rudolfNative.postMessage(m)}catch(e){}}
/* deep equality of C:'s values (folders are objects, files strings) */
function oaEq(a,b){if(a===b)return true;if(!a||!b||typeof a!='object'||typeof b!='object')return false;
 const ka=Object.keys(a),kb=Object.keys(b);if(ka.length!=kb.length)return false;for(const k of ka)if(!(k in b)||!oaEq(a[k],b[k]))return false;return true}
/* the three-way merge: base (what was read), mine, theirs (stored now) */
function oaMerge3(b,m,t){if(oaEq(m,t))return m;if(oaEq(m,b))return t;if(oaEq(t,b))return m;
 const isO=x=>x&&typeof x=='object'&&!Array.isArray(x);
 if(!isO(m)||!isO(t))return m;   /* the same thing changed on both sides: this page's */
 const B=isO(b)?b:{},r={};
 for(const k of new Set([...Object.keys(m),...Object.keys(t)])){const inM=k in m,inT=k in t,inB=k in B;
  if(inM&&inT)r[k]=oaMerge3(B[k],m[k],t[k]);
  else if(inM){if(!(inB&&oaEq(B[k],m[k])))r[k]=m[k]}   /* only here: new here, or kept (gone there but changed here) */
  else{if(!(inB&&oaEq(B[k],t[k])))r[k]=t[k]}}           /* only there: new there, unless this page deleted it unchanged */
 return r}
/* a value made to equal another in place (the open apps keep their references to C:'s folders) */
function oaInto(dst,src){for(const k of Object.keys(dst))if(!(k in src))delete dst[k];
 for(const k of Object.keys(src)){const v=src[k];if(v&&typeof v=='object'&&dst[k]&&typeof dst[k]=='object'&&!Array.isArray(v))oaInto(dst[k],v);else dst[k]=v&&typeof v=='object'?structuredClone(v):v}}
if(ONEAPP){
 document.documentElement.classList.add('oneapp');if(oaNative())document.documentElement.classList.add('oanative');
 {const st=document.createElement('style');st.textContent=`html.oneapp #bar,html.oneapp #menu,html.oneapp #desk>:not(.win){display:none!important}
html.oneapp body,html.oneapp #desk{background:var(--oabg,#e5e7eb)!important;background-image:none!important}html.oneapp.dark body,html.oneapp.dark #desk{--oabg:#111827}
html.oneapp #desk{inset:0!important;bottom:0!important;top:0!important}
html.oneapp .win.oamain{position:fixed!important;left:0!important;top:0!important;width:100vw!important;height:100vh!important;border-radius:0!important;box-shadow:none!important;border:0!important;resize:none}
html.oneapp .win.oamain>.tb .mn,html.oneapp .win.oamain>.tb .mx,html.oneapp .win.oamain>div[style*="resize"]{display:none!important}
html.oanative .win.oamain>.tb{display:none!important}html.oanative .win.oamain>.body{border-radius:0!important}
#oamsg{position:fixed;inset:0;z-index:2147483000;display:flex;align-items:center;justify-content:center;flex-direction:column;gap:12px;background:var(--oabg,#e5e7eb);font:15px system-ui,sans-serif;color:#111827;text-align:center;padding:20px}
html.dark #oamsg{color:#e5e7eb}#oamsg button{font:inherit;padding:6px 16px}`;document.head.appendChild(st)}
 /* what the native shell and the session do instead */
 sGest=1;   /* no start-up chime at this page's sign-in screen */
 idleCheck=async function(){};saverStart=function(){};
 window.__sessionSave=function(){};
 {const _sr=window.__sessionRestore;setTimeout(()=>{window.__sessionRestore=function(){try{pinDraw()}catch(e){}}},0);void _sr}
 /* only Settings writes the accounts: the other app pages leave them as they are */
 {const _su=saveUser;saveUser=function(){if(ONEAPP.k!='settings')return;return _su.apply(this,arguments)}}
 /* notifications: the tray's count */
 {const _n=notify;notify=function(t,b,o){const r=_n.apply(this,arguments);if(!(o&&o.quiet))oaPost({op:'note',t:String(t||''),b:String(b||'')});return r}}
}
function oaMsg(html,btn){let o=document.getElementById('oamsg');if(!o){o=document.createElement('div');o.id='oamsg';document.body.appendChild(o)}
 o.innerHTML=html;if(btn){const b=document.createElement('button');b.textContent=btn[0];b.onclick=btn[1];o.appendChild(b)}return o}
/* the page's key from the native session in place of the password (deriveKey with it gives exactly that key) */
const OA_PW={oneApp:1};let OA_PK=null;
if(ONEAPP){const _dk=deriveKey;deriveKey=async function(pw,salt){if(pw===OA_PW&&OA_PK)return crypto.subtle.importKey('raw',OA_PK,{name:'AES-GCM'},false,['encrypt','decrypt']);return _dk.apply(this,arguments)};
 /* the Debian data folder is the native sign-in's (rudolfd): never with the stand-in password */
 if(typeof vaultSignin=='function'){const _vs=vaultSignin;vaultSignin=async function(a,pw){if(pw===OA_PW)return;return _vs.apply(this,arguments)}}
 if(typeof pwCheck=='function'){const _pc=pwCheck;pwCheck=async function(a,pw){if(pw===OA_PW)return false;return _pc.apply(this,arguments)}}}
const oaHex=h=>{const s=String(h||'').replace(/[^0-9a-f]/gi,'');const u=new Uint8Array(s.length>>1);for(let i=0;i<u.length;i++)u[i]=parseInt(s.substr(i*2,2),16);return u};
async function oaSession(){if(!RD)return false;let s;try{s=await rd('desktop/session')}catch(e){return false}
 const u=s&&s.user;if(!u||!s.pk)return false;await FSREADY;
 const a=ACCTS.find(x=>u.id&&x.id==u.id)||ACCTS.find(x=>String(x.n).toLowerCase()==String(u.n||'').toLowerCase());if(!a)return false;
 OA_PK=oaHex(s.pk);if(OA_PK.length!=32){OA_PK=null;return false}
 USER=a;try{await userLogin(a,OA_PW)}catch(e){console.warn('one-app sign-in',e)}finally{OA_PK.fill(0);OA_PK=null}
 LG.style.display='none';try{userUI()}catch(e){}return true}
/* the start: the native session's sign-in, else the page's own sign-in screen; later calls (Log Off) close the page */
if(ONEAPP){let first=true;const _sl=showLogin;
 showLogin=function(){if(!first){oaDone();return}first=false;
  if(!APPS[ONEAPP.k]){oaMsg(`<b>There is no app called “${String(ONEAPP.k).replace(/[<>&"]/g,'')}”.</b><span>This window closes in a few seconds.</span>`);setTimeout(oaClose,5000);return}
  const o=oaMsg('<span>Opening '+String(APPS[ONEAPP.k].name).replace(/[<>&]/g,'')+'…</span>');
  oaSession().then(ok=>{o.remove();if(!ok)_sl()}).catch(()=>{o.remove();_sl()})}}
/* after the sign-in (either way): the app */
if(ONEAPP){const o=window.__afterLogin;window.__afterLogin=function(){try{o&&o.apply(this,arguments)}catch(e){console.warn(e)}oaOpen()}}
function oaOpen(){if(!ONEAPP||ONEAPP.main||ONEAPP.want)return;const a=APPS[ONEAPP.k];if(!a)return;ONEAPP.want=true;
 try{if(ONEAPP.file&&a.openFile){const p=resolve(ONEAPP.file,['C:']);LAUNCHING=ONEAPP.k;try{a.openFile(p)}finally{LAUNCHING=null}}else launch(ONEAPP.k)}catch(e){console.warn(e)}
 /* an app that opens no window of its own (a Linux app, which rudolfd starts): the page has done its part */
 setTimeout(()=>{if(!ONEAPP.main){ONEAPP.want=false;if(/^lx:/.test(ONEAPP.k))return oaDone();oaMsg('<b>'+String(a.name).replace(/[<>&]/g,'')+' did not open a window.</b>',['Close',oaClose])}},15000)}
/* the app's first window fills the page; its title is the page's; closing it closes the page */
if(ONEAPP){const _w=win;win=function(){const W=_w.apply(this,arguments);if(ONEAPP.want&&!ONEAPP.main){ONEAPP.main=W;ONEAPP.want=false;oaMain(W)}return W}}
function oaMain(W){W.el.classList.add('oamain');const t0=W.el.querySelector('.t');const title=()=>(t0&&t0.textContent)||(APPS[ONEAPP.k]&&APPS[ONEAPP.k].name)||'Rudolf OS';
 const tell=()=>{document.title=title();oaPost({op:'title',t:document.title,icon:W.icon||(APPS[ONEAPP.k]&&APPS[ONEAPP.k].icon)||''})};
 const st=W.setTitle;W.setTitle=function(){const r=st.apply(this,arguments);tell();return r};
 if(t0)new MutationObserver(tell).observe(t0,{childList:true,characterData:true,subtree:true});
 const cl=W.close;W.close=function(){const r=cl.apply(this,arguments);if(!wins.includes(W))oaDone();return r};
 /* (its ✕ calls the first close it was given, not the wrapped one: it goes through this one; and a window taken
    away any other way still closes the page) */
 const cb=W.el.querySelector('.tb .cl');if(cb)cb.onclick=()=>W.close();
 new MutationObserver((l,o)=>{if(!W.el.isConnected){o.disconnect();oaDone()}}).observe(document.getElementById('desk'),{childList:true});
 /* the main window cannot be minimised (there would be nothing to show) */
 const mn=W.el.querySelector('.tb .mn');if(mn)mn.onclick=()=>{};
 tell();oaPost({op:'ready',app:ONEAPP.k})}
window.__oneAppClose=function(){if(!ONEAPP)return false;const W=ONEAPP.main;if(!W){oaDone();return true}const b=W.el.querySelector('.tb .cl');if(b)b.click();else W.close();return true};
/* the native frame's messages */
window.__rnIn=function(m){if(m&&m.op=='close')window.__oneAppClose()};
/* everything written, then the page closes */
async function oaDone(){if(!ONEAPP||ONEAPP.done)return;ONEAPP.done=true;
 try{fsFlush()}catch(e){}
 const wait=p=>Promise.race([Promise.resolve(p).catch(()=>{}),new Promise(r=>setTimeout(r,4000))]);
 await wait(OAFS.chain);try{if(typeof HD!='undefined')await wait(HD.chain)}catch(e){}await wait(homeChain);
 try{if(window.__rudolfLsFlush)await wait(__rudolfLsFlush())}catch(e){}
 oaClose()}
function oaClose(){oaPost({op:'close'});try{window.close()}catch(e){}
 setTimeout(()=>{oaMsg('<b>This app is closed.</b>',['Open again',()=>location.reload()])},400)}
/* other apps: a window of their own in the native shell */
if(ONEAPP){const _l=launch;launch=function(k){if(ONEAPP.main&&oaNative()&&APPS[k]&&!ONEAPP.want){oaPost({op:'open',app:k});return}return _l.apply(this,arguments)}}
/* C:'s system part, shared by the app pages: merged, under a lock, and passed on */
const OAFS={base:null,chain:Promise.resolve(),bc:null};
function oaStub(v){const c=structuredClone(v);try{const C=c['C:'];if(C&&C.USERS&&typeof C.USERS=='object')for(const k in C.USERS)C.USERS[k]={}}catch(e){}return c}
const oaLock=f=>navigator.locks&&navigator.locks.request?navigator.locks.request('rudolf-fs-system',f):f();
if(ONEAPP){FSREADY.then(()=>{OAFS.base=oaStub(fs)});
 try{OAFS.bc=new BroadcastChannel('rudolf-fs-system');OAFS.bc.onmessage=()=>{OAFS.chain=OAFS.chain.then(oaFsPull).catch(e=>console.warn('one-app C:',e))}}catch(e){}
 const _kp=kvPutNow;kvPutNow=function(k,v){if(k!=='fs:system'||!OAFS.base)return _kp.apply(this,arguments);
  const mine=oaStub(v);   /* now: fsFlush puts the account folders back right after this call */
  const p=OAFS.chain.then(()=>oaLock(async()=>{const theirs=await kvGet('fs:system');const merged=theirs?oaMerge3(OAFS.base,mine,oaStub(theirs)):mine;
   const ok=await _kp('fs:system',merged);OAFS.base=structuredClone(merged);oaFsApply(merged);try{OAFS.bc&&OAFS.bc.postMessage('saved')}catch(e){}return ok}));
  OAFS.chain=p.catch(e=>console.warn('one-app C:',e));return p}}
/* another page saved: its changes come in (this page's own unsaved ones stay) */
async function oaFsPull(){if(!OAFS.base)return;const theirs=await kvGet('fs:system');if(!theirs)return;const t=oaStub(theirs);
 const merged=oaMerge3(OAFS.base,oaStub(fs),t);OAFS.base=t;oaFsApply(merged);try{EXPL.forEach(f=>f())}catch(e){}}
function oaFsApply(merged){const C=fs['C:'],M=merged['C:'];if(!C||!M)return;const U=C.USERS;
 for(const k of Object.keys(C))if(k!='USERS'&&!(k in M))delete C[k];
 for(const k of Object.keys(M)){if(k=='USERS')continue;const v=M[k];if(v&&typeof v=='object'&&C[k]&&typeof C[k]=='object')oaInto(C[k],v);else C[k]=v&&typeof v=='object'?structuredClone(v):v}
 C.USERS=U;for(const k of Object.keys(fs))if(k!='C:'&&k in merged)fs[k]=merged[k]}
