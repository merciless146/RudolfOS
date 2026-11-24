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
/* ===== 1.6.2 (docs/plan-1.6.2.md Part 38, step 3c): the shell's hidden page draws no pictures it never shows =====
   The hidden page (#app=do-bg) is never seen: the native shell draws the desktop, the taskbar and the start menu itself, from the
   catalogue this page gives. It drew the page's own start menu's, desktop's and taskbar's pictures all the same and kept them, and drew
   every picture again for the catalogue, at every sign-in (each an SVG picture of its own in WebKit; host, the hidden page alone:
   66-68 MB RssAnon, of which about 12 went this way). Here:
   - the title bars of the page's own windows (its dialogs: the keyring's password, Open and Save) have their pictures, as in an app's
     window (oaLeanIcons);
   - its start menu's, desktop's and taskbar's rows are marked as iconsApply would picture them, with the same elements less the
     picture itself (icImg without its src): the catalogue reads the rows as the page shows them (which picture each row has, its
     words), so it reads the same; no picture is loaded for them;
   - the catalogue's pictures (oaPng: each app's picture as a small PNG) are taken from this account's last catalogue when they were made
     from the same picture by the same engine, and the others drawn one after another through one image. The catalogue given is the
     same. The Emoji icon style, Standard, Lite and the app windows are not concerned. ===== */
const OA_BGLEAN=!!(OA_BG&&oaNative());
const OA_BGL={cat:0,given:0};
window.__oaBgLean=()=>({on:OA_BGLEAN,given:OA_BGL.given,png:OA_BGIM.n,seeded:OA_BGIM.seeded,marked:document.querySelectorAll('img.ico[data-bgic]').length,
 kept:[...document.querySelectorAll('#desk img.ico,#menu img.ico,#tasks img.ico,#pins img.ico')].filter(i=>i.getAttribute('src')).length,
 drawn:[...document.querySelectorAll('img.ico')].filter(i=>i.getAttribute('src')).length});
/* (a row marked: icImg's element less its picture) */
function oaBgMark(emo,cls){const k=EMO2IC[emo]||EMO2IC[emo.replace(/\uFE0F/g,'')];if(!k)return null;const i=document.createElement('img');
 i.className='ico '+(cls||'');i.alt='';i.setAttribute('aria-hidden','true');i.dataset.emo=emo;i.dataset.bgic=k;return i}
/* (the catalogue's drawn pictures (SVG) made into PNGs through one image, one after another, as the catalogue asks for them: each SVG
   picture is let go as the next one is drawn, and the last once the catalogue is given, instead of an image each, kept until the
   script engine collects them (in a page never seen, a long time); the same PNGs, kept as oaPng keeps them) */
const OA_BGIM={im:null,q:Promise.resolve(),n:0,seeded:0};
function oaBgPng(src,n){const ck=n+'|'+src;if(src.length<200000&&OA_PNG.has(ck))return Promise.resolve(OA_PNG.get(ck));
 const one=()=>new Promise((res,rej)=>{const im=OA_BGIM.im||(OA_BGIM.im=new Image());
  im.onload=()=>{try{const c=document.createElement('canvas');c.width=c.height=n;c.getContext('2d').drawImage(im,0,0,n,n);const u=c.toDataURL('image/png');c.width=c.height=0;
   if(src.length<200000)OA_PNG.set(ck,u);OA_BGIM.n++;res(u)}catch(e){rej(e)}};im.onerror=()=>rej(new Error('icon'));im.src=src});
 const p=OA_BGIM.q.then(one,one);OA_BGIM.q=p.catch(()=>{});return p}
/* (the pictures this account's last catalogue gave, which rudolfd keeps (desktop/apps?acct=ID): each drawn picture's PNG is taken
   again, not drawn, when the picture is the same as the one it was made from (its SVG's mark, "ich", which every page that gives the
   catalogue writes, oaIch); at the hidden page's first catalogue. A picture that changed (an update, the icon style) is drawn again) */
/* (the mark: the SVG's length and FNV-1a hash, and the engine that drew it (WebKit or Chromium: each draws a picture its own way)) */
function oaIchOf(s){let h=0x811c9dc5;for(let i=0;i<s.length;i++){h^=s.charCodeAt(i);h=Math.imul(h,0x01000193)>>>0}return s.length.toString(36)+'.'+h.toString(36)+'.'+(/Chrome\//.test(navigator.userAgent)?'c':'w')}
function oaIch(icons){const o={};try{if(typeof EMO2IC!='object')return o;const D=new Set(Object.values(EMO2IC));for(const k in icons)if(D.has(k))try{o[k]=oaIchOf(icSvg(k))}catch(e){}}catch(e){}return o}   /* (the drawn pictures' keys: EMO2IC's; an app's own picture's is oaImgKey's) */
let OA_BGSEED=null;
function oaBgSeed(){if(!OA_BGSEED)OA_BGSEED=(async()=>{try{if(typeof RD=='undefined'||!RD||!USER)return;
  const r=await fetch('/api/desktop/apps?acct='+encodeURIComponent(uid()),{headers:{'X-Rudolf-Token':RD.token},cache:'no-store'});if(!r.ok)return;
  const j=await r.json();if(!j||j.acct!==uid()||!j.ich||typeof j.ich!='object'||!j.icons||typeof j.icons!='object')return;
  for(const k in j.ich){const png=j.icons[k];if(typeof png!='string'||png.length>65536||!/^data:image\/png;base64,[A-Za-z0-9+\/=]+$/.test(png)||typeof EMO2IC!='object')continue;
   let src='';try{src=icSvg(k)}catch(e){continue}if(!src||src.length>=200000||oaIchOf(src)!==j.ich[k])continue;OA_PNG.set('48|'+src,png);OA_BGIM.seeded++}}catch(e){}})();return OA_BGSEED}
function oaBgPngEnd(){OA_BGIM.q.then(()=>{if(OA_BGL.cat||!OA_BGIM.im)return;OA_BGIM.im.onload=OA_BGIM.im.onerror=null;OA_BGIM.im.removeAttribute('src');OA_BGIM.im=null})}
if(OA_BGLEAN){
 /* (the rows marked wherever iconsApply would picture them; the Emoji style as before) */
 {const _ia=iconsApply;iconsApply=function(){if(S.icons==='emoji')return _ia.apply(this,arguments);try{oaLeanIcons.call(this,_ia)}catch(e){}   /* (the title bars' pictures first, drawn) */
  const ii=icImg;icImg=oaBgMark;try{return _ia.apply(this,arguments)}finally{icImg=ii}}}
 /* (the start menu's rows made again (the apps rudolfd knows coming in): marked at once, not by the next iconsApply only, so a
    catalogue made meanwhile reads them as the page shows them) */
 {const _s=startMenuApps;startMenuApps=function(){const r=_s.apply(this,arguments);try{iconsApply()}catch(e){}return r}}
 /* (the desktop's pictures in the accent colour (acIcons): none here, its rows are marked, not drawn) */
 acIcons=function(){};
 /* (the pictures drawn again for another icon style (icRedraw): the marked rows left as they are) */
 {const _r=icRedraw;icRedraw=function(){const m=[...document.querySelectorAll('img.ico[data-bgic]')];m.forEach(i=>i.classList.remove('ico'));try{return _r.apply(this,arguments)}finally{m.forEach(i=>i.classList.add('ico'))}}}
 {const _p=oaPng;oaPng=function(src,n){return /^data:image\/svg/.test(src)?oaBgPng(src,n):_p.apply(this,arguments)}}
 {const _c=oaCatalog;oaCatalog=async function(){OA_BGL.cat++;try{if(S.icons!=='emoji')await oaBgSeed();return await _c.apply(this,arguments)}
  finally{OA_BGL.cat--;OA_BGL.given++;oaBgPngEnd()}}}}
