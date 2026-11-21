/* ===== Rudolf OS 1.6.1 (the owner, 2026-11-21): File Explorer to the desktop, by dragging and by Paste (docs/plan-1.6.1.md Part 28)
   "Click-dragging items from the file explorer directly to the desktop should place the items onto the desktop. When people
   click-drag select items in the file explorer and use the right-click menu to cut/copy them, they should also be able to see the
   paste option when they right-click on the desktop, with the pasted items being added."
   - Dropped on the desktop's empty area: into the account's Desktop folder, as a drop on an Explorer window showing that folder
     (C: a move, Ctrl a copy; a drive, the Games folder, a folder of this computer: xfer61.js's checked transfer, read-only places
     copied), the icons where they were dropped (the first in the cell under the pointer, the rest next to it, freeCell as the
     desktop places icons), kept as any icon's place. On a folder's icon: into that folder; on the Trash: the Trash takes C:'s
     items, as Explorer's Trash does (base_v24.html's own handler).
   - Paste in the desktop's menu (greyed while File Explorer's clipboard FC holds nothing), Ctrl+V on the desktop; Paste into folder
     on a folder's icon; Cut and Copy on the desktop's files (the same FC, so Paste in File Explorer takes them).
   - Native: the shell draws the desktop. A drag from an Explorer window's page dropped on it is done by that page (__deskDrop, the
     shell names the cell, the folder or the Trash); the places go to the shell with the page's list (oaCatalog's "place"); the
     shell's menus' Paste, Cut, Copy and Paste into folder are this page's items (oaDesk). ===== */
const DKX={q:null,busy:null,last:null};
/* (the drag of an Explorer window's items, kept past its dragend: on the native desktop the shell asks this page for it at the drop) */
document.addEventListener('dragstart',()=>{try{const d=FC.drag&&FC.drag.length?FC.drag.slice():null,r=FC.dragR&&FC.dragR.items&&FC.dragR.items.length?FC.dragR:null;DKX.last=d||r?{drag:d,dragR:r,t:Date.now()}:null}catch(e){}});
/* the clipboard holds something to paste */
function dkClip(){try{fcTake()}catch(e){}return !!(FC.op&&((FC.r&&FC.r.items&&FC.r.items.length)||(FC.items&&FC.items.some(p=>node(p)!=null))))}
const dkSay=(m,bad)=>{if(m&&bad)DKX.said=m;if(m&&bad)try{notify('File Explorer',m,{kind:'error',quiet:true,toast:6000})}catch(e){}};
/* Explorer's own check before writing into a C: folder (files25.js needWrite) */
async function dkNeedWrite(p){if(fsCanWrite(p)&&!isProt(p))return true;if(!fsCanWrite(p)){dkSay('That folder belongs to another account.',true);return false}return elevate('Change files in the system folder '+p.join('\\'))}
/* the desktop's cell under a point of the screen */
function dkCellAt(x,y){const g=gridInfo(),r=DESK.getBoundingClientRect(),kx=DESK.clientWidth/(r.width||1),ky=DESK.clientHeight/(r.height||1);
 const c=Math.floor(((x-r.left)*kx-8)/g.w),w=Math.floor(((y-r.top)*ky-8)/g.h);return{cell:[Math.max(0,Math.min(g.nc-1,c)),Math.max(0,Math.min(g.nr-1,w))]}}
/* the new icons' places: the first at the cell, the others the nearest free ones; kept (bd95place) for the other pages, and for the native
   shell, which places them itself from the same cell in its own grid (where its icons are) */
const dkPlK=()=>'bd95place:'+(typeof uid=='function'&&USER?uid():'');
function dkPlace(names,at){if(!at||!at.cell||!names.length)return;const g=gridInfo(),nc=at.nc||g.nc,nr=at.nr||g.nr,keys=new Set(names.map(n=>'file:'+n)),used=new Set();
 IC.forEach(o=>{const k=o.dataset.n;if(!keys.has(k)&&cells[k])used.add(cells[k]+'')});const P={};
 for(const n of names){const f=freeCell(Math.min(at.cell[0],nc-1),Math.min(at.cell[1],nr-1),used,nc,nr);used.add(f+'');P['file:'+n]=f;cells['file:'+n]=f}
 const t=Date.now();DKPL_T=t;try{localStorage.setItem(dkPlK(),JSON.stringify({t,c:P,at:at.cell,n:names}))}catch(e){}try{layIcons()}catch(e){}}
let DKPL_T=0;
function dkPlaceTake(){try{const o=JSON.parse(localStorage.getItem(dkPlK())||'null');if(!o||!(o.t>DKPL_T))return;DKPL_T=o.t;if(Date.now()-o.t<120000)Object.assign(cells,o.c||{})}catch(e){}}
function dkPlaceFresh(){try{const o=JSON.parse(localStorage.getItem(dkPlK())||'null');return o&&Date.now()-o.t<120000?o:null}catch(e){return null}}
/* (another page placed them: taken before the desktop's icons are laid out) */
{const _ds=deskSync,arr=[...EXPL];deskSync=function(){dkPlaceTake();return _ds.apply(this,arguments)};EXPL.clear();arr.forEach(f=>EXPL.add(f===_ds?deskSync:f))}
/* within C: (files25.js xfer, without its window's Undo): a move, or a copy (a file kept on the disk copied as a file of its own) */
const dkHasRef=v=>isDlRef(v)||isDir(v)&&Object.values(v).some(dkHasRef);
async function dkCopyVal(v,nm){if(isDlRef(v)){try{return await xfRefCopy(v,nm)}catch(e){throw new Error(nm+' is kept on the disk and could not be copied ('+e.message+')')}}
 if(isDir(v)){const o={};for(const k in v)o[k]=await dkCopyVal(v[k],k);return o}return clone(v)}
async function dkXferC(items,dest,mode,at){const dd=node(dest);if(!isDir(dd))return dkSay('Destination not found.',true);if(!await dkNeedWrite(dest))return;
 const j=xfJob(mode=='cut'?'move':'copy',items.length,{route:'To '+(pkey(dest)==pkey(homePath('DESKTOP'))?'Desktop':dest.at(-1)||'C:')}),bad=[],made=[];let n=0,ok=0;
 for(const it of items){const src=node(it);j.set(it.at(-1),n++,items.length);if(j.cancel)break;if(src==null)continue;
  if(isDir(src)&&(pkey(it)==pkey(dest)||under(it,dest))){j.finish(true);return dkSay('A folder cannot be moved or copied into itself.',true)}const par=dirOf(it);
  if(mode=='cut'){if(isProt(it)||!fsCanWrite(it)){dkSay('System items cannot be moved.',true);continue}if(pkey(it.slice(0,-1))==pkey(dest))continue;const nm=uniq(dd,it.at(-1));dd[nm]=src;delete par[it.at(-1)];try{favMoved(it,[...dest,nm])}catch(e){}made.push(nm);ok++}
  else{const nm=uniq(dd,it.at(-1));let v;try{v=dkHasRef(src)?await dkCopyVal(src,nm):clone(src)}catch(e){bad.push(it.at(-1)+': '+e.message);continue}dd[nm]=v;made.push(nm);ok++}}
 const msg=`${ok} item(s) ${mode=='cut'?'moved':'copied'}.`+(bad.length?` Not copied: ${bad.join('; ')}`:'');
 if(made.length&&pkey(dest)==pkey(homePath('DESKTOP')))dkPlace(made,at);
 if(ok){fsSave();if(!bad.length)try{sfx('notify')}catch(e){}}if(bad.length)dkSay(msg,true);j.finish(!bad.length,msg,bad);if(mode=='cut'){FC.op=null;FC.items=[];FC.r=null;fcPut()}EXPL.forEach(f=>f());return ok}
/* a drive's items (rr: {r: the place, items}) into a C: folder: xfer61.js's transfer, the progress window, its checks */
function dkXferR(rr,dest,mode,at){const srcs=rr.items.map(i=>({s:'R',base:rr.r.path,name:i.name,dir:!!i.dir,size:i.size||0,mtime:i.mtime||0})),desk=pkey(dest)==pkey(homePath('DESKTOP'));
 return xfRun(srcs,{s:'C',p:dest},mode,{say:dkSay,needWrite:dkNeedWrite,srcR:rr.r,fromLabel:rr.r.label||rr.r.path,label:desk?'Desktop':undefined,
  result:res=>{if(desk&&res.out&&res.out.length)dkPlace(res.out.map(p=>p.at(-1)),at)}})}
/* what was dragged ({drag: C: paths} or {dragR}) or cut/copied, put into dest */
function dkPut(src,dest,copy,at){if(src.dragR&&src.dragR.items&&src.dragR.items.length){const rr=src.dragR;return dkXferR(rr,dest,copy||rr.r&&rr.r.ro?'copy':'cut',at)}
 const it=(src.drag||[]).filter(p=>node(p)!=null);if(it.length&&!it.some(i=>pkey(i)==pkey(dest)))return dkXferC(it,dest,copy?'copy':'cut',at)}
function dkPaste(dest,at){if(!dkClip())return dkSay('The clipboard is empty.',true);const p=(FC.r&&FC.r.items&&FC.r.items.length)?dkXferR(FC.r,dest,FC.op,at):dkXferC(FC.items.filter(i=>node(i)!=null),dest,FC.op,at);
 DKX.busy=Promise.resolve(p).catch(()=>{});return p}
/* Cut and Copy of the desktop's files: File Explorer's clipboard */
function dkClipPut(op,paths){if(!paths.length)return;FC.op=op;FC.items=paths;FC.r=null;fcPut();EXPL.forEach(f=>f())}
const dkSelFiles=el=>{const s=el&&!el.classList.contains('sel')?[el]:[...DESK.querySelectorAll(':scope > .ic.fic.sel')];return s.map(x=>homePath('DESKTOP').concat(x.dataset.fn))};
/* ---------- Standard and Lite: the drag from an Explorer window onto the desktop (one page) ---------- */
const dkDragging=()=>!!(FC.drag&&FC.drag.length||FC.dragR&&FC.dragR.items&&FC.dragR.items.length);
const dkTarget=e=>{const t=e.target;if(!t||!t.closest||t.closest('.win'))return null;const ic=t.closest('#desk > .ic');
 if(ic&&ic.classList.contains('tr'))return{trash:ic};if(ic&&ic.classList.contains('fic')&&isDir((deskFiles()||{})[ic.dataset.fn]))return{dir:ic};return t===DESK||ic||DESK.contains(t)?{desk:1}:null};
DESK.addEventListener('dragover',e=>{if(!USER||!dkDragging())return;const T=dkTarget(e);if(!T||T.trash)return;e.preventDefault();xDropFx(e);
 DESK.querySelectorAll(':scope > .ic.fic.dkt').forEach(x=>{if(x!==T.dir){x.classList.remove('dkt','sel')}});if(T.dir&&!T.dir.classList.contains('dkt')){T.dir.classList.add('dkt','sel')}});
DESK.addEventListener('dragleave',e=>{const ic=e.target.closest&&e.target.closest('#desk > .ic.fic.dkt');if(ic&&!ic.contains(e.relatedTarget))ic.classList.remove('dkt','sel')});
DESK.addEventListener('drop',e=>{if(!USER||!dkDragging())return;const T=dkTarget(e);DESK.querySelectorAll(':scope > .ic.dkt').forEach(x=>x.classList.remove('dkt','sel'));if(!T||T.trash)return;
 e.preventDefault();const src={drag:FC.drag,dragR:FC.dragR};FC.drag=null;FC.dragR=null;const copy=xCopy(e);
 if(T.dir)return dkPut(src,homePath('DESKTOP').concat(T.dir.dataset.fn),copy,null);dkPut(src,homePath('DESKTOP'),copy,dkCellAt(e.clientX,e.clientY))});
/* Ctrl+V, Ctrl+C, Ctrl+X on the desktop (nothing in front of it: no window, no field, no menu) */
document.addEventListener('keydown',e=>{if(typeof ONEAPP!='undefined'&&ONEAPP||!USER||!(e.ctrlKey||e.metaKey)||e.altKey||e.shiftKey||CMENU)return;const a=e.target,k=String(e.key).toLowerCase();   /* (the key's own element: a window's list drawn again by its own Paste is no longer the focus) */
 if(!(!a||a===document.body||a===document.documentElement||a===DESK||a.closest&&a.closest('#desk > .ic'))||isEditable(a)||!deskFiles())return;
 if(k=='v'&&dkClip()){e.preventDefault();dkPaste(homePath('DESKTOP'),null)}
 else if((k=='c'||k=='x')&&DESK.querySelector(':scope > .ic.fic.sel')){e.preventDefault();dkClipPut(k=='x'?'cut':'copy',dkSelFiles(null))}});
/* ---------- the native desktop: the shell's drop, done by the page the drag came from ----------
   o: {on: a desktop folder's name, trash: 1, c, r: the cell (the shell's grid nc × nr), copy: Ctrl held} */
window.__deskDrop=o=>{o=o||{};const L=DKX.last;let src={drag:FC.drag,dragR:FC.dragR};if(!(src.drag&&src.drag.length)&&!(src.dragR&&src.dragR.items&&src.dragR.items.length)&&L&&Date.now()-L.t<120000)src={drag:L.drag,dragR:L.dragR};
 FC.drag=null;FC.dragR=null;DKX.last=null;if(!USER||!deskFiles())return 0;
 if(o.trash){const ps=(src.drag||[]).filter(p=>!PROT.includes(p.join('/')));if(!ps.length)return 0;const r=trashPut(ps);if(r>0){try{sfx('notify')}catch(e){}EXPL.forEach(f=>f())}else try{sfx('error')}catch(e){}return 1}
 if(o.on){const v=(deskFiles()||{})[o.on];if(!isDir(v))return 0;dkPut(src,homePath('DESKTOP').concat(o.on),!!o.copy,null);return 1}
 dkPut(src,homePath('DESKTOP'),!!o.copy,o.c!=null?{cell:[+o.c||0,+o.r||0],nc:+o.nc||0,nr:+o.nr||0}:null);return 1};
