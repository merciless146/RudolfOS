/* ===== Rudolf OS 1.6.3 (docs/plan-1.6.3.md Part 8, item 4): the native desktop's icons at once =====
   "Desktop icon removal/changes still take several minutes on Native." The native shell drew the desktop's icons only from the whole
   catalogue a page gives (oneapp59.js oaCatalog: every app's picture drawn as a PNG, the start menu's rows with their styles, the
   statue, the sounds), which a busy computer took minutes to make and send (the owner's Vivobook, 2026-11-22: 192 s while Steam
   started). Each time a page of the native desktop gives the catalogue, it first gives the shell just the desktop's icons (op "desk":
   their names, kinds, pictures' keys, places and the icon size, and "dt", when they were read), drawn at once; the catalogue that
   follows carries the same "dt", and the shell draws its icons only when they are as new (rudolf-native.c desk_quick). The pictures'
   keys are the ones the catalogue gives (EMO2IC's, a program's own oaImgKey), whose pictures the shell has from before; one it does
   not have yet is drawn as its emoji until the catalogue brings it. As the catalogue, after the apps rudolfd knows are in
   (oaAppsReady), so a window that has not got them yet never takes their icons away. ===== */
const OA_DQ={last:'',n:0,hold:0,held:null};
async function oaDeskQuick(){if(!ONEAPP||typeof oaNative!='function'||!oaNative()||!USER||typeof RD=='undefined'||!RD||OA.done)return;
 try{await oaAppsReady()}catch(e){}if(!USER||OA.done)return;
 /* (an app's window gives nothing that is what was given last, as its catalogue: lean61.js) */
 try{if(typeof OA_LEAN!='undefined'&&OA_LEAN&&oaLeanSig()===oaCatSigSaved())return}catch(e){}
 const dt=Date.now(),key=e=>typeof EMO2IC=='object'&&S.icons!=='emoji'?EMO2IC[e]||EMO2IC[e.replace(/️/g,'')]||'':'';
 const desk=[];try{const D=document.getElementById('desk');if(D&&typeof icTarget=='function')for(const d of D.querySelectorAll(':scope > .ic')){
  if(d.style.display=='none')continue;const im=d.querySelector('img.ico'),e=im?im.dataset.emo||'':((d.querySelector('b')||{}).textContent||'').trim();
  if(d.classList.contains('fic')){const fn=d.dataset.fn,v=(deskFiles()||{})[fn];desk.push({n:fn,k:'',kind:isDir(v)?'dir':'file',ic:key(e),icon:e,cell:'file:'+fn,fp:homePath('DESKTOP').concat(fn).join('\\')});continue}
  const T=icTarget(d),k=T.k&&APPS[T.k]?T.k:'',own=k&&typeof APPS[k].img=='function'&&typeof oaImgKey=='function'?oaImgKey(k):'';
  desk.push({n:T.name,k,kind:d.classList.contains('tr')?'trash':T.kind,full:d.classList.contains('full')?1:0,own:d.classList.contains('dapp')?1:0,ic:own||key(e),icon:e,cell:d.dataset.n||T.name})}}catch(e){console.warn('desk at once',e);return}
 let cells={};try{cells=JSON.parse(localStorage.getItem(typeof cellKey=='function'?cellKey():'bd95cells')||'{}')}catch(e){}
 const place=typeof dkPlaceFresh=='function'?dkPlaceFresh():null,body={acct:uid(),is:+S.is||100,desk,cells,place};
 const j=JSON.stringify(body);if(j===OA_DQ.last)return;OA_DQ.last=j;OA_DQ.n++;
 const m={op:'desk',j:JSON.stringify(Object.assign(body,{dt}))};if(OA_DQ.hold){OA_DQ.held=m;return}oaPost(m)}
let OA_DQP=null;
if(ONEAPP&&typeof oaCatalog=='function'){const _c=oaCatalog;oaCatalog=function(){try{OA_DQP=oaDeskQuick()}catch(e){OA_DQP=null}return _c.apply(this,arguments)}}
/* (the hidden page's actions, oneapp59.js oaCatSoon: done once the desktop's icons are given, not after the whole catalogue, which a busy
   computer gave minutes later while the next action waited for this one; the catalogue goes on, and the shell is told when it is in) */
/* (the icons given just after "done", not before: the shell's dialog (a Rename) is closed by "done", and with the icons first a click
   meant for the desktop came while it was still open and was taken by it: native59_t.sh 62, 65-68 now and then) */
if(ONEAPP&&typeof oaCatSoon=='function'){oaCatSoon=function(){OA_DQ.hold=1;OA_DQ.held=null;const p=oaCatalog(),q=OA_DQP||Promise.resolve();p.then(()=>{if(!OA.done)oaPost({op:'cat'})}).catch(()=>{});
 const rel=()=>{if(!OA_DQ.hold)return;OA_DQ.hold=0;const m=OA_DQ.held;OA_DQ.held=null;if(m)setTimeout(()=>oaPost(m),0)};
 return Promise.race([p,q]).then(rel,e=>{rel();throw e})}}
window.__oaDeskQuick=()=>({n:OA_DQ.n});
/* (and the files of an account on the disk, home56.js, in every window as they are on the disk: a page that saved tells the others
   (oneapp59.js's "rudolf-fs" channel), which read the disk again; a page still signing in when told, or told before its files were
   loaded, reads them once it is signed in. Before, a window showed the files it loaded, or the browser's copy from before the move,
   until its next look 15 s on, and gave the desktop's icons from that: the others' new icons went and came back) */
if(ONEAPP&&typeof BroadcastChannel=='function'&&typeof hdRefresh=='function'){const ch=new BroadcastChannel('rudolf-fs');let want=0,t=0;
 const go=()=>{clearTimeout(t);t=setTimeout(async()=>{if(!USER||typeof hdOn!='function'||!hdOn()||typeof FSLOADED=='undefined'||!FSLOADED){want=1;return}want=0;try{await HD.chain;await hdRefresh()}catch(e){}},250)};
 ch.onmessage=e=>{const m=e.data||{};if(USER&&m.id&&m.id!=uid())return;go()};
 {const _ul=userLogin;userLogin=async function(){const r=await _ul.apply(this,arguments);setTimeout(()=>{if(want||(typeof hdOn=='function'&&hdOn()))go()},1500);return r}}}
