/* ===== Rudolf OS 1.5.5: which app shortcuts are on the desktop =====
   Each account chooses the app shortcuts on its desktop: Remove from desktop (the icon's menu, File Explorer's
   Desktop folder, or dropping the icon on the Trash), and Add to desktop (the Start menu, or Desktop icons… in the
   desktop's menu). The built-in icons that are removed are only hidden; apps that have no built-in icon get one of
   their own. Kept per account: DESKHIDE (built-in icons by name) and DESKADD (app keys). Files and folders on the
   desktop are files in the Desktop folder, as before: they go to the Trash. */
const DESKHIDE=()=>'bd95deskhide:'+uid(),DESKADD=()=>'bd95deskadd:'+uid();
/* a desktop icon that is an app shortcut (not a file, not the Trash): its app key, or null */
function deskAppOf(d){if(!d||d.classList.contains('fic')||d.classList.contains('tr'))return null;if(d.dataset.app)return d.dataset.app;
 try{const T=icTarget(d);return T&&T.k&&(T.kind=='app'||T.kind=='computer')?T.k:null}catch(e){return null}}
const deskBuiltins=()=>[...DESK.querySelectorAll(':scope > .ic:not(.fic):not(.tr):not([data-app])')];
const deskVisible=d=>d.style.display!='none';
/* any app shortcut can be removed, built in or not (My Computer and Programs too); files go to the Trash instead */
const deskRemovable=d=>!!d&&!d.classList.contains('fic')&&!d.classList.contains('tr');
/* the visible icon that opens app k, if any */
function deskIconFor(k){return [...DESK.querySelectorAll(':scope > .ic:not(.fic):not(.tr)')].find(d=>deskVisible(d)&&deskAppOf(d)==k)||null}
function deskHasApp(k){return !!deskIconFor(k)}
/* a shortcut of our own for an app without a built-in icon */
function deskMakeIcon(k){const a=APPS[k];if(!a)return null;let el=DESK.querySelector(`:scope > .ic[data-app="${CSS.escape(k)}"]`);if(el)return el;
 el=document.createElement('div');el.className='ic dapp';el.dataset.app=k;el.dataset.n='app:'+k;el.innerHTML='<b></b><span></span>';el.querySelector('b').textContent=a.icon||'🔗';el.querySelector('span').textContent=a.name;el.title=a.name;el.tabIndex=0;
 el.style.cssText+=';position:absolute;margin:0;touch-action:none;transition:left .1s,top .1s';
 let t=0;el.onclick=e=>{if(e&&e.ctrlKey)return;DESK.querySelectorAll('.ic').forEach(x=>x.classList.remove('sel'));el.classList.add('sel');if(Date.now()-t<dblMs()||window.__mouse&&S.msingle)launch(k);t=Date.now()};
 el.onkeydown=e=>{if(e.key=='Enter')launch(k);if(e.key=='Delete')deskRemove(el)};
 deskDrag(el);DESK.appendChild(el);IC.push(el);return el}
/* dragging one of our own shortcuts: to another place, or onto the Trash (removed from the desktop) */
function deskDrag(d){let sx,sy,ox,oy,mv=0,dn=0;d.addEventListener('pointerdown',e=>{if(e.button!==0)return;dn=1;mv=0;sx=e.clientX;sy=e.clientY;ox=d.offsetLeft;oy=d.offsetTop;d.setPointerCapture(e.pointerId)});
 d.addEventListener('pointermove',e=>{if(!dn)return;const dx=e.clientX-sx,dy=e.clientY-sy;if(!mv&&Math.hypot(dx,dy)<5)return;mv=1;d.style.transition='none';d.style.zIndex=5;d.style.left=ox+dx+'px';d.style.top=oy+dy+'px'});
 d.addEventListener('pointerup',e=>{if(!dn)return;dn=0;if(!mv)return;d.style.transition='left .1s,top .1s';d.style.zIndex='';
  if(deskOnTrash(e)){deskRemove(d);return}
  const g=gridInfo(),used=new Set();IC.forEach(o=>{if(o!==d&&cells[o.dataset.n])used.add(cells[o.dataset.n]+'')});const c=Math.max(0,Math.min(g.nc-1,Math.round((d.offsetLeft-8)/g.w))),r=Math.max(0,Math.min(g.nr-1,Math.round((d.offsetTop-8)/g.h)));cells[d.dataset.n]=freeCell(c,r,used,g.nc,g.nr);layIcons()});
 d.addEventListener('click',e=>{if(mv){e.stopImmediatePropagation();mv=0}},true)}
function deskOnTrash(e){return [...DESK.querySelectorAll(':scope > .ic.tr')].some(t=>{const r=t.getBoundingClientRect();return e.clientX>r.left&&e.clientX<r.right&&e.clientY>r.top&&e.clientY<r.bottom})}
/* show what this account chose: built-in icons hidden or shown, our own shortcuts made or removed */
function deskApply(){const hid=USER?ld(DESKHIDE(),[]):[],add=USER?ld(DESKADD(),[]).filter(k=>APPS[k]):[];
 deskBuiltins().forEach(d=>{const off=hid.includes(d.dataset.n||d.querySelector('span').textContent);d.style.display=off?'none':'';if(off){IC=IC.filter(x=>x!==d);d.classList.remove('sel')}else if(!IC.includes(d))IC.push(d)});
 [...DESK.querySelectorAll(':scope > .ic[data-app]')].forEach(el=>{if(!add.includes(el.dataset.app)){el.remove();IC=IC.filter(x=>x!==el);delete cells[el.dataset.n]}});
 add.forEach(k=>{if(!deskBuiltins().some(d=>deskVisible(d)&&deskAppOf(d)==k))deskMakeIcon(k)});
 try{layIcons()}catch(e){}try{EXPL.forEach(f=>f())}catch(e){}}
function deskRemove(d){if(!USER||!d)return false;const k=deskAppOf(d);const nm=(d.querySelector('span')||{}).textContent||'';
 if(d.dataset.app){svj(DESKADD(),ld(DESKADD(),[]).filter(x=>x!=d.dataset.app))}
 else{const h=ld(DESKHIDE(),[]);const n=d.dataset.n||nm;if(!h.includes(n))svj(DESKHIDE(),[...h,n])}
 deskApply();notify(nm+' removed from the desktop','The app stays in the Start menu; right-click it there to add it back.',{quiet:true,toast:3000});return true}
function deskAdd(k){if(!USER||!APPS[k])return false;if(deskHasApp(k))return true;
 const b=deskBuiltins().find(d=>deskAppOf(d)==k);
 if(b){svj(DESKHIDE(),ld(DESKHIDE(),[]).filter(x=>x!=(b.dataset.n||b.querySelector('span').textContent)))}
 else{const a=ld(DESKADD(),[]);if(!a.includes(k))svj(DESKADD(),[...a,k])}
 deskApply();notify(APPS[k].name+' is on the desktop','',{quiet:true,toast:2500});return true}
function deskToggle(k){const d=deskIconFor(k);return d?deskRemove(d):deskAdd(k)}
/* the menu item for app k: Add to desktop / Remove from desktop */
function deskMenuItem(k){if(!USER||!APPS[k])return null;const on=deskHasApp(k);return [on?'Remove from desktop':'Add to desktop',()=>deskToggle(k),true,{icon:on?'➖':'🖥️'}]}
/* Desktop icons…: every app, on the desktop or not */
function deskIconsDlg(){if(!USER)return;const W=win('Desktop icons','🖥️',380,460);W.body.style.cssText='display:flex;flex-direction:column;padding:10px;gap:8px';
 W.body.innerHTML=`<div class='msg'>The app shortcuts on your desktop. Files and folders on the desktop are in your Desktop folder.</div><input type='search' class='dsq' placeholder='Search apps' aria-label='Search apps'><div class='dsl' style='flex:1;overflow:auto;border:1px solid #e5e7eb;border-radius:6px'></div><div style='display:flex;justify-content:space-between'><button class='rst'>Restore the standard icons</button><button class='ok'>Done</button></div>`;
 const L=W.body.querySelector('.dsl'),Q=W.body.querySelector('.dsq');
 const keys=()=>Object.keys(APPS).filter(k=>APPS[k]&&APPS[k].name&&!['trash'].includes(k)).sort((a,b)=>APPS[a].name.localeCompare(APPS[b].name));
 function draw(){const q=Q.value.trim().toLowerCase();L.innerHTML='';keys().filter(k=>!q||APPS[k].name.toLowerCase().includes(q)).forEach(k=>{const r=document.createElement('label');r.className='dsr';r.style.cssText='display:flex;gap:8px;align-items:center;padding:5px 8px;border-bottom:1px solid #f3f4f6;cursor:pointer';
  r.innerHTML=`<input type='checkbox'><span style='width:22px;text-align:center'></span><span style='flex:1'></span>`;const c=r.querySelector('input');c.checked=deskHasApp(k);c.dataset.k=k;r.children[1].textContent=APPS[k].icon||'';r.children[2].textContent=APPS[k].name;
  c.onchange=()=>{if(c.checked)deskAdd(k);else{const d=deskIconFor(k);d&&deskRemove(d)}c.checked=deskHasApp(k)};L.appendChild(r)})}
 W.body.querySelector('.rst').onclick=()=>{svj(DESKHIDE(),[]);svj(DESKADD(),[]);deskApply();draw()};
 Q.oninput=draw;W.body.querySelector('.ok').onclick=()=>W.close();draw();return W}
/* the desktop icon's menu: Remove from desktop for app shortcuts */
{const _im=iconMenu;iconMenu=function(d,x,y){const sel=deskIcons().filter(i=>i.classList.contains('sel'));
 if(!deskRemovable(d)||!USER||(sel.length>1&&sel.includes(d)))return _im.call(this,d,x,y);
 const name=d.querySelector('span').textContent,extra=[['Open',()=>openIcon(d),true,{icon:'↗',key:'Enter'}],['Remove from desktop',()=>deskRemove(d),true,{icon:'➖',key:'Del'}],'-',['Arrange icons by name',arrangeIcons],['Desktop icons…',deskIconsDlg],'-',['Properties',()=>iconProps(d)]];
 if(!d.classList.contains('sel')){deskIcons().forEach(i=>i.classList.remove('sel'));d.classList.add('sel')}cmenu(x,y,extra,{title:name})}}
/* Delete on a selected built-in app icon removes it from the desktop (files go to the Trash, as before) */
DESK.addEventListener('keydown',e=>{if(e.key!='Delete'||!USER)return;const d=e.target.closest&&e.target.closest('#desk > .ic');if(!deskRemovable(d)||d.dataset.app)return;e.preventDefault();deskRemove(d)});
/* dropping a built-in app icon on the Trash removes it from the desktop (the pointer is over the Trash only when
   the icon was dragged there: icons never overlap) */
DESK.addEventListener('pointerup',e=>{if(!USER)return;const d=e.target.closest&&e.target.closest('#desk > .ic');if(!deskRemovable(d)||d.dataset.app)return;
 if(deskOnTrash(e))setTimeout(()=>deskRemove(d),0)},true);
/* after signing in and out */
{const _sr=window.__sessionRestore;window.__sessionRestore=function(){try{deskApply()}catch(e){}return _sr&&_sr.apply(this,arguments)}}
{const _lo=A.logoff;A.logoff=function(){const r=_lo.apply(this,arguments);try{deskApply()}catch(e){}return r}}
