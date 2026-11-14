/* ===== 1.4.4 (wallpaper31): a picture of your own as the desktop background, chosen in Display Properties from
   your files (Pictures, Downloads, anywhere on C:) or from this computer. The picture is kept, made no larger than
   the screen needs (at most 2560 pixels wide), in the per-user store (IndexedDB, mirrored to RUDOLFDATA), so it is
   there after a restart without the file it came from. Choosing a dynamic background, a preset or colours turns it
   off. ===== */
DEFS.disp.wpimg='';DEFS.disp.wpfit='fill';DEFS.disp.wpname='';
['wpimg','wpfit','wpname'].forEach(k=>{if(!DKEYS.includes(k))DKEYS.push(k);if(S[k]===undefined)S[k]=DEFS.disp[k];if(typeof SC=='object'&&SC&&SC[k]===undefined)SC[k]=S[k]});
const WP_FITS=[['fill','Fill (cropped to the screen)'],['fit','Fit (the whole picture)'],['stretch','Stretch'],['center','Centre'],['tile','Tile']];
const WP_RE=/\.(png|jpe?g|gif|webp|bmp|svg)$/i;
var WPURL='',WPID='',WPBUSY='',WPREADY=false;   /* var: bgCSS runs before this module when the page starts */
function wpKey(id){return 'wp:'+id}
/* the background for bgCSS (base_v24.html): '' when there is no picture, or it is still being read */
function wpCss(){const id=S.wpimg;if(!id||!WPREADY)return'';if(id!=WPID){wpLoad(id);return''}
 const u=`url("${WPURL}")`,bg=S.b1||'#000';
 return({fit:`${u} center/contain no-repeat ${bg}`,stretch:`${u} center/100% 100% no-repeat ${bg}`,center:`${u} center/auto no-repeat ${bg}`,tile:`${u} left top/auto repeat ${bg}`})[S.wpfit]||`${u} center/cover no-repeat ${bg}`}
var WPPROM=null;
function wpLoad(id){if(WPBUSY==id&&WPPROM)return WPPROM;WPBUSY=id;
 return WPPROM=(async()=>{try{const v=await kvGet(wpKey(id));if(S.wpimg!=id)return;
  if(typeof v=='string'&&v.startsWith('data:image')){WPURL=v;WPID=id}else{WPID='';S.wpimg=''}applySet();window.__dynRefresh&&__dynRefresh()}finally{WPBUSY=''}})()}
/* any picture source (a data: URL, a link to a download kept on the disk, a File) -> a JPEG data: URL no larger
   than the screen needs */
function wpShrink(src){return new Promise((res,rej)=>{const im=new Image();im.onload=()=>{const max=Math.min(2560,Math.max(1280,Math.round(screen.width*(devicePixelRatio||1)))),k=Math.min(1,max/im.naturalWidth);
  const w=Math.max(1,Math.round(im.naturalWidth*k)),h=Math.max(1,Math.round(im.naturalHeight*k)),c=document.createElement('canvas');c.width=w;c.height=h;const g=c.getContext('2d');g.fillStyle='#000';g.fillRect(0,0,w,h);g.drawImage(im,0,0,w,h);
  try{res(c.toDataURL('image/jpeg',0.9))}catch(e){rej(new Error('This picture cannot be used (it comes from another site).'))}};im.onerror=()=>rej(new Error('This file is not a picture Rudolf OS can show.'));im.src=src})}
const WPIDS=()=>'bd95wpids:'+uid();
async function wpUse(src,name){const d=await wpShrink(src),id=Date.now().toString(36);await kvSet(wpKey(id),d);
 /* keep only this picture and the one that is applied; the others go */
 const keep=new Set([id,S.wpimg,typeof SC=='object'&&SC?SC.wpimg:''].filter(Boolean)),ids=ld(WPIDS(),[]);ids.filter(x=>!keep.has(x)).forEach(x=>kvDel(wpKey(x)));svj(WPIDS(),[...keep]);
 WPURL=d;WPID=id;S.wpimg=id;S.wpname=String(name||'Picture').slice(0,80);if(S.dyn)S.dyn='';applySet();window.__dynRefresh&&__dynRefresh();return id}
async function wpFromFiles(){const p=await pickVfsFile('Choose a picture for the desktop',n=>WP_RE.test(n));if(!p)return false;const v=node(p);
 let src=typeof v=='string'&&v.startsWith('data:image')?v:null;if(!src&&typeof dlRefInfo=='function'&&typeof v=='string'&&dlRefInfo(v))src=await dlLink(v);
 if(!src)throw new Error('This file is not a picture Rudolf OS can show.');await wpUse(src,p.at(-1));return true}
function wpFromComputer(){return new Promise((res,rej)=>{const i=document.createElement('input');i.type='file';i.accept='image/*';i.onchange=()=>{const f=i.files[0];if(!f)return res(false);const r=new FileReader();r.onload=()=>wpUse(r.result,f.name).then(()=>res(true),rej);r.onerror=()=>rej(new Error('The file could not be read.'));r.readAsDataURL(f)};i.click()})}
/* Display Properties: the Picture row, under the background style */
(function(){const o=dispPanelS;dispPanelS=function(c,W){o(c,W);const bs=c.querySelector('#bs');if(!bs)return;
 const lab=document.createElement('label');lab.textContent='Picture';const d=document.createElement('div');d.className='wp31';
 bs.after(lab,d);
 const draw=()=>{const on=!!S.wpimg&&S.wpimg==WPID;d.innerHTML=`<div style='display:flex;gap:8px;align-items:center;flex-wrap:wrap'>${on?`<span class='wpth' style='width:64px;height:36px;border-radius:4px;border:1px solid var(--bd,#999);background:url("${WPURL}") center/cover'></span><span class='wpn'></span>`:"<span class='msg' style='margin:0'>None</span>"}
   <button class='wpf'>Choose from my files…</button><button class='wpc'>From this computer…</button>${on?"<button class='wpr'>Remove</button>":''}</div>
   ${on?`<div style='margin-top:6px'>Position <select class='wpfit'>${WP_FITS.map(([k,n])=>optH(k,n,S.wpfit||'fill')).join('')}</select></div>`:''}<div class='msg wpm' style='margin:4px 0 0'></div>`;
  const n=d.querySelector('.wpn');if(n)n.textContent=S.wpname||'Picture';
  const say=t=>d.querySelector('.wpm').textContent=t;const done=()=>{draw();c.dispatchEvent(new Event('input',{bubbles:true}))};
  d.querySelector('.wpf').onclick=e=>{e.preventDefault();wpFromFiles().then(ok=>ok&&done(),x=>say(x.message))};
  d.querySelector('.wpc').onclick=e=>{e.preventDefault();wpFromComputer().then(ok=>ok&&done(),x=>say(x.message))};
  const r=d.querySelector('.wpr');if(r)r.onclick=e=>{e.preventDefault();S.wpimg='';S.wpname='';applySet();window.__dynRefresh&&__dynRefresh();done()};
  const f=d.querySelector('.wpfit');if(f)f.onchange=e=>{e.stopPropagation();S.wpfit=f.value;applySet();c.dispatchEvent(new Event('input',{bubbles:true}))}};
 draw();if(S.wpimg&&S.wpimg!=WPID)wpLoad(S.wpimg).then(()=>d.isConnected&&draw());
 /* a drawn background or other colours: the picture is no longer the background */
 const off=()=>{if(!S.wpimg)return;S.wpimg='';S.wpname='';setTimeout(()=>{applySet();draw()},0)};
 c.addEventListener('click',e=>{if(e.target.closest&&e.target.closest('.dynb'))off()},true);
 c.addEventListener('input',e=>{if(['bs','b1','b2'].includes(e.target.id))off()},true)}})();
WPREADY=true;if(S.wpimg)wpLoad(S.wpimg);
