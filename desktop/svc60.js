/* ===== Rudolf OS 1.6.0 (docs/plan-1.6.0.md Part 12, item 12): the native desktop's background work, once =====
   On Standard one page is the whole desktop: the timers its modules start while it loads (the Clock's alarms and timers, the
   Calendar's reminders, the microphone's and the cellular modem's notices, game mode, the Debian apps' updates, the home folder's
   refresh, the session's save, the lag meter, Linux apps' windows, the battery, the update check...) run once, whatever is open. On the
   native desktop every app window is a page of its own (oneapp59.js), and each ran all of them: with no window open none ran (an alarm
   did not ring), with three open each ran its own (an alarm rang three times).
   This module comes first in the page (build19.py puts it before the page's own code and core25.js). In a page of the native desktop
   (#app=NAME in rudolf-native's WebKit or Oracle's Chromium) the timers the modules start while the page loads are held back:
   - an app's window starts only those it needs: the short ones that finish the page's own start (under a second), the theme's and the
     icons' minute ticks (a11y25.js, the drawn backgrounds' light or dark), and its own app's (APPMOD: Oracle's tabs);
   - the shell's one hidden page for the whole desktop (#app=do-bg since 1.6.1, before it #app=do-svc: started by rudolf-native at
     sign-in, never shown) starts every one,
     once it is signed in, as Standard's page has them (oneapp59.js "svc");
   On Standard and Lite (no #app=), in Standard's Tor windows and in a page opened by hand (no native desktop) nothing here does anything:
   setTimeout and setInterval are the browser's own.
   build19.py marks where each module begins (__svcMod) and where the page's code ends (__svcEnd). ===== */
const SVC=(()=>{let k='';try{const h=String(location.hash||'').replace(/^#/,'');if(h)k=new URLSearchParams(h).get('app')||''}catch(e){}
 /* (only on the native desktop: rudolfd says so, or the shell's WebKit handler or Oracle's binding is there; a Tor window on Standard, a
    page of one app in Chromium, and a page opened by hand are left as they were) */
 let nat=false;try{nat=!!(window.RUDOLFD&&window.RUDOLFD.engine=='native')||!!(window.webkit&&window.webkit.messageHandlers&&window.webkit.messageHandlers.rudolfNative)||typeof window.rudolfNativeOracle=='function'}catch(e){}
 return{on:nat&&!!k&&/^[a-z0-9_:.+-]{1,127}$/i.test(k),k,svc:k=='do-svc'||k=='do-bg',mod:'base',held:[],ended:false,ran:false,fake:new Map(),n:0}})();
function __svcMod(m){SVC.mod=m}
/* (a held timer one of the list's: a module's, or a function's of the page's own code by its words) */
function svcIs(h,L){for(const x of L){if(typeof x=='string'){if(x==h.mod)return true}else{let s='';try{s=String(h.fn)}catch(e){}if(x.test(s))return true}}return false}
/* (the service page is never shown: it is the desktop's, as Standard's page that is on the screen, so what checks document.hidden (the
   microphone's, the network's and the volume's polls) runs there as on Standard) */
if(SVC.svc)try{Object.defineProperty(Document.prototype,'hidden',{configurable:true,get(){return false}});Object.defineProperty(Document.prototype,'visibilityState',{configurable:true,get(){return 'visible'}})}catch(e){}
/* (the theme's and the icons' minute ticks, and the drawn backgrounds' light or dark: what every window's own look needs) */
const SVC_EVERY=['a11y25.js','oneapp59.js',
 /* (and what is the window's own: the battery saver's look (power31.js pwrSaverApply), its own games' frames paused when unseen and told
    the theme (games58.js), its own game's full screen starting game mode (gamesleep59.js: the service page has a Linux game's), its own
    copy of the home folder read again from the disk (home56.js: what File Explorer and the Open and Save dialogs in it show)) */
 'power31.js','games58.js','gamesleep59.js','home56.js',
 /* (remote access's red bar "… sees this screen", over each window as before (the shell draws none); its question is the service
    page's only: oneapp59.js) */
 'remote50.js'];
const SVC_EVERYF=[/dynResolve\(\)\.mode/];
/* (what the native shell does itself for the whole desktop, so its service page does not do it again: the heartbeat (tick_heart), the
   microphone's 🎙️ (tick_mic), the cellular notices (tick_cell: they were shown once by the shell and again by every window), the
   development link's banner and question (tick_devlink)) */
const SVC_SHELL=['call59.js','mobile60.js','devlink40.js'];
const SVC_SHELLF=[/rdPost\('heartbeat'\)/];
/* (an app's own module-level timers, in its own window: Oracle's real tabs, its dark pages, the bookmarks bar's room, Tor's keep-alive) */
const SVC_APPMOD={browser:['tabs26.js','web25.js','web46.js','tor59.js','oracle58.js','private59.js','oracleart60.js','signin26.js','shell26.js','net31.js','net57.js',/netProbe\(\)/],
 settings:['net31.js','net57.js','osd31.js',/netProbe\(\)/]};
if(SVC.on){const ST=window.setTimeout,SI=window.setInterval,CT=window.clearTimeout,CI=window.clearInterval;
 const every=(h)=>{if(h.t=='t'&&!(+h.ms>=1000))return true;if(SVC_EVERY.includes(h.mod))return true;
  if(h.mod=='base'){let s='';try{s=String(h.fn)}catch(e){}if(SVC_EVERYF.some(r=>r.test(s)))return true}
  return svcIs(h,SVC_APPMOD[SVC.k]||[])};
 const start=h=>{if(h.gone||h.real!=null)return;const f=h.t=='i'?SI:ST;h.real=f.call(window,h.fn,h.ms,...h.a);h.at=Date.now()};
 const hold=t=>function(fn,ms,...a){if(SVC.ended)return(t=='i'?SI:ST).call(window,fn,ms,...a);
  const h={t,fn,ms,a,mod:SVC.mod,real:null,gone:false,id:2e9+(++SVC.n)};if(every(h)){start(h);h.every=1}SVC.held.push(h);SVC.fake.set(h.id,h);return h.id};
 const clr=c=>function(id){const h=SVC.fake.get(id);if(h){h.gone=true;if(h.real!=null)c.call(window,h.real);return}return c.call(window,id)};
 window.setTimeout=hold('t');window.setInterval=hold('i');window.clearTimeout=clr(CT);window.clearInterval=clr(CI)}
/* the page's code has run: in a native app's window the rest stay held (never started); were the native desktop not there after all,
   all of them now */
function __svcEnd(){if(!SVC.on||SVC.ended)return;SVC.ended=true;
 let nat=false;try{nat=!!(window.webkit&&window.webkit.messageHandlers&&window.webkit.messageHandlers.rudolfNative)||typeof window.rudolfNativeOracle=='function'||!!(window.RUDOLFD&&window.RUDOLFD.engine=='native')}catch(e){}
 SVC.native=nat;if(!nat)__svcRun()}
/* every timer held back, started now (the desktop's service page once signed in, or a page with no native desktop): each with its own
   delay from now, as Standard's page starts them as it loads */
function __svcRun(svc){if(SVC.ran)return;SVC.ran=true;for(const h of SVC.held)if(!h.every&&!h.gone&&h.real==null){if(svc&&SVC.native&&(SVC_SHELL.includes(h.mod)||svcIs(h,SVC_SHELLF)))continue;const f=h.t=='i'?window.setInterval:window.setTimeout;
 /* (through the page's own wrappers, which by now are the browser's: SVC.ended) */const r=f.call(window,h.fn,h.ms,...h.a);h.real=r;h.at=Date.now()}}
/* (for the tests: what was held back and what runs, by module) */
window.__svcList=()=>{return SVC.held.map(h=>({mod:h.mod,t:h.t,ms:h.ms,run:h.real!=null&&!h.gone,every:!!h.every,f:String(h.fn).slice(0,80)}))};
