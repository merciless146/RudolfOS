/* ===== Rudolf OS 1.6.1 (docs/plan-1.6.1.md Part 23 B, stage 3): an app's window on the native desktop builds only what it shows =====
   Every window of the native desktop is a page of the whole desktop (oneapp59.js ONEAPP) showing one app; the desktop, the taskbar and the
   start menu are the native shell's own, drawn from the catalogue (desktop/apps) the shell's hidden page gives. Each window's page still
   built the desktop page's own for itself, hidden by oneapp59.js's styles: the start menu's, the desktop's and the taskbar's pictures (each
   an SVG drawn by the page: about 40 of them, 8-11 MB of the window's memory on the host), the taskbar's pinned buttons, the dynamic
   background's moving parts (the aurora, the statue, the petals and lanterns, under the app's window), and the catalogue itself 2.5 s
   after signing in (every app's picture drawn again as a PNG), which the hidden page had given already.
   Here, in an app's window of the native desktop (not the shell's own pages, #app=do-NAME, nor Standard, Lite, a Tor window on Standard
   or a page opened by hand: they build all of it as before):
   - the pictures are made only where the window shows them (the title bars of the page's own windows: its dialogs);
   - the taskbar's pinned buttons are not made (the shell's taskbar has its own);
   - the dynamic background's moving parts are not drawn (oneapp59.js's styles put a plain colour behind the app's window);
   - the catalogue is given only when what it lists is not what was given last (bd95catsig:ID, written by every page that gives it: the
     hidden page at sign-in, a window whose apps or desktop files changed), after the apps rudolfd knows are in (oaAppsReady), with every
     picture made first, as before. The hidden page gives it again when it takes the settings another window saved (the icons' style, the
     accent colour, light or dark), which the windows that signed in afterwards gave before.
   Nothing else changes: the menu's rows and the desktop's icons are still made (the catalogue reads them), and so is everything an app
   uses. ===== */
const OA_LEAN=!!(ONEAPP&&!/^do-/.test(ONEAPP.k)&&oaNative());
const OA_LEANS={full:0,given:0,skipped:0};
window.__oaLean=()=>({on:OA_LEAN,full:OA_LEANS.full,given:OA_LEANS.given,skipped:OA_LEANS.skipped});
/* (the last catalogue given, by its signature: shared by the windows, as their storage is) */
const oaCatSigK=()=>'bd95catsig:'+(typeof uid=='function'&&USER?uid():'');
function oaCatSigSaved(){try{return localStorage.getItem(oaCatSigK())}catch(e){return null}}
function oaCatSigSave(sig){try{if(USER&&sig)localStorage.setItem(oaCatSigK(),sig)}catch(e){}}
/* (oaCatSig without its count of the pictures this page has drawn of apps' own icons, which only a page that gave the catalogue has) */
function oaLeanSig(){try{const a=JSON.parse(oaCatSig());a.splice(4,1);return JSON.stringify(a)}catch(e){return ''}}
/* the pictures in the title bars of the page's own windows only (iconsApply's first part, a11y25.js); all of them once the window gives
   the catalogue (it reads the menu's and the desktop's) */
function oaLeanIcons(full0){if(OA_LEANS.full||S.icons==='emoji')return full0.apply(this,arguments);
 document.documentElement.classList.add('svgicons');
 document.querySelectorAll('.win > .tb > span:first-child').forEach(el=>{if(el.querySelector('img.ico'))return;const t=el.textContent.trim();const im=icImg(t);if(im){el.textContent='';el.appendChild(im)}});
 try{if(typeof acIcons=='function')acIcons()}catch(e){}}
/* the catalogue: given when this window's differs from the last one given (after the apps rudolfd knows are in) */
async function oaLeanCat(cat0,args){try{await oaAppsReady()}catch(e){}
 if(!USER||OA.done)return;const sig=oaLeanSig();if(sig&&sig===oaCatSigSaved()){OA_CSIG=oaCatSig();OA_LEANS.skipped++;return}
 OA_LEANS.full=1;try{iconsApply()}catch(e){}OA_LEANS.given++;return cat0.apply(this,args)}
/* (and what the page drew of them as it loaded, for the last account's background, before this module: taken away) */
function oaLeanDynGone(){try{if(typeof STS=='object'&&STS.el){petStop();STS.el.remove();STS.el=STS.img=null;STS.key='';PET.cv=PET.lcv=null}}catch(e){}
 try{if(typeof AUR=='object'&&AUR.el){aurStop();AUR.el.remove();AUR.el=AUR.cv=AUR.sky=null;AUR.on=false}}catch(e){}
 try{if(typeof DSV=='object'&&DSV.el){dsStop();DSV.el.remove();DSV.el=DSV.cv=null;DSV.on=false}}catch(e){}}
if(OA_LEAN){
 {const _ia=iconsApply;iconsApply=function(){return oaLeanIcons.call(this,_ia,...arguments)}}
 pinDraw=function(){PINS.innerHTML=''};
 dynShown=()=>false;oaLeanDynGone();
 {const _c=oaCatalog;oaCatalog=function(){return oaLeanCat(_c,arguments)}}}
/* (every page of the native desktop that gives the catalogue says so: its signature, once rudolfd has it) */
if(ONEAPP&&oaNative()){const _c=oaCatalog;oaCatalog=async function(){const g=OA_LEANS.given,r=await _c.apply(this,arguments);try{if(OA_CSIG&&(!OA_LEAN||OA_LEANS.given>g))oaCatSigSave(oaLeanSig())}catch(e){}return r}}
/* (the hidden page: another window's saved settings taken in, the catalogue given again and the shell told) */
if(OA_BG){let t=0;const _sa=oaSetApply;oaSetApply=function(){const r=_sa.apply(this,arguments);
 clearTimeout(t);t=setTimeout(()=>{if(!USER||OA.done)return;oaCatalog().then(()=>{if(!OA.done)oaPost({op:'cat'})}).catch(()=>{})},800);return r}}
