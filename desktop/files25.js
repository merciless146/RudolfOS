/* ===== V25 File Explorer: places sidebar, tabs, split view, thumbnails, preview pane, search in contents,
   undo, properties, Open with / default apps, ZIP, disks and USB drives, folders on this computer ===== */
const OPENERS=[ /* key, label, icon, test(ext,value), open(path) */
 ['notepad','Notepad','🗒️',(e,v)=>typeof v=='string'&&!v.startsWith('data:'),p=>notepad(p)],
 ['code','Code Editor','🧑‍💻',(e,v)=>typeof v=='string'&&!v.startsWith('data:'),p=>launchWith('code',p)],
 ['wordpro','WordPro','📝',e=>e=='DOC'||e=='TXT'||e=='RTF',p=>wordpro(p)],
 ['paint','Paint','🎨',(e,v)=>/^data:image\/(png|jpe?g|gif|webp|bmp)/.test(v),p=>paint(p)],
 ['viewer','Image Viewer','🖼️',(e,v)=>/^data:image\//.test(v),p=>launchWith('viewer',p)],
 ['pdf','PDF Viewer','📕',(e,v)=>e=='PDF'||/^data:application\/pdf/.test(v),p=>launchWith('pdf',p)],
 ['media','Media Player','🎵',(e,v)=>/^data:(audio|video)\//.test(v),p=>media(p)],
 ['sheet','Spreadsheet','📊',e=>e=='CSV'||e=='RSHEET',p=>launchWith('sheet',p)],
 ['deck','Presentations','📽️',e=>e=='RDECK',p=>launchWith('deck',p)],
 ['archive','Archive Manager','📦',e=>e=='ZIP'||e=='TAR',p=>launchWith('archive',p)],
 ['browser','Oracle Browser','🌐',e=>e=='HTM'||e=='HTML'||e=='SVG',p=>browser('file:///'+p.join('/'))],
 ['prompt','Rudolf Prompt (run)','⬛',e=>e=='LUA'||e=='BAT'||e=='WASM',null],
 ['fonts','Font Viewer','🔤',e=>/^(TTF|OTF|WOFF2?)$/.test(e),p=>launchWith('fonts',p)],
 ['emu','Emu','🦤',e=>/^(SFC|SMC|NDS)$/.test(e),p=>window.emuOpenDrive&&emuOpenDrive(p)]];
function launchWith(k,p){LAUNCHING=k;try{return APPS[k]&&APPS[k].openFile?APPS[k].openFile(p):launch(k)}finally{LAUNCHING=null}}
const defK=()=>'bd95defapps:'+uid();
function openersFor(path){const v=node(path),e=ext(path.at(-1));return OPENERS.filter(o=>{try{return o[3](e,v)}catch(x){return false}})}
function openWith(path,always){const list=openersFor(path);const o=document.createElement('div');o.className='rmodal';
 o.innerHTML=`<div class='rmbox' role='dialog' aria-label='Open with'><h3>Open “<span></span>” with</h3><div class='owl'></div><label class='ck'><input type='checkbox' class='alw'> Always use this app for .${ext(path.at(-1))||'?'} files</label><div class='rmrow'><button class='c'>Cancel</button></div></div>`;
 o.querySelector('h3 span').textContent=path.at(-1);const l=o.querySelector('.owl');
 (list.length?list:OPENERS.slice(0,2)).forEach(x=>{const b=document.createElement('button');b.className='owb';b.textContent=x[2]+'  '+x[1];b.onclick=()=>{if(o.querySelector('.alw').checked){const d=ld(defK(),{});d[ext(path.at(-1))]=x[0];svj(defK(),d)}o.remove();x[4]?x[4](path):openPathDefault(path)};l.appendChild(b)});
 if(always)o.querySelector('.alw').checked=true;o.querySelector('.c').onclick=()=>o.remove();document.body.appendChild(o);l.querySelector('button').focus()}
let openPathDefault=openPath;
(function(){const o=openPath;openPathDefault=o;openPath=function(path){const v=node(path);if(v!=null&&!isDir(v)){const e=ext(path.at(-1));const d=ld(defK(),{})[e];const op=d&&OPENERS.find(x=>x[0]==d);if(op&&op[4]){try{recentAdd(path)}catch(x){}return op[4](path)}
  if(e=='PDF'||/^data:application\/pdf/.test(v))return launchWith('pdf',path);if(/^data:image\/(gif|jpe?g|webp|svg|bmp)/.test(v)&&APPS.viewer)return launchWith('viewer',path);if(e=='ZIP')return launchWith('archive',path);if(e=='CSV'||e=='RSHEET')return launchWith('sheet',path);if(e=='RDECK')return launchWith('deck',path);if(e=='WASM')return window.wasiRun&&wasiRun(path);if(e=='RAPP')return window.rappInstall&&rappInstall(path)}
 return o.apply(this,arguments)}})();
/* ---------- properties ---------- */
function folderStats(v){let files=0,dirs=0,bytes=0;const w=n=>{for(const k in n){if(isDir(n[k])){dirs++;w(n[k])}else{files++;bytes+=Math.max(0,sizeOf(n[k]))}}};w(v);return{files,dirs,bytes}}
function fileProps(path){const v=node(path);if(v==null)return;const W=win('Properties','ℹ️',380,360);const m=fileMeta(path)||{};const isD=isDir(v);const st=isD?folderStats(v):null;const prot=isProt(path)||!fsCanWrite(path);
 W.body.style.cssText='padding:14px;font-size:13px;overflow:auto';
 W.body.innerHTML=`<div style='display:flex;gap:12px;align-items:center'><div style='font-size:40px'></div><div><b class='pn'></b><div style='color:#6b7280' class='pt'></div></div></div><table class='ptab'></table><div class='pact' style='margin-top:10px;display:flex;gap:6px;flex-wrap:wrap'></div>`;
 W.body.querySelector('div div').textContent=fIcon(path.at(-1),v);W.body.querySelector('.pn').textContent=path.at(-1);W.body.querySelector('.pt').textContent=fType(path.at(-1),v);
 const rows=[['Location',path.slice(0,-1).join('\\')],['Size',isD?`${fmtB(st.bytes)} in ${st.files} files and ${st.dirs} folders`:fmtB(sizeOf(v))],['Created',m.c?new Date(m.c).toLocaleString():'—'],['Modified',m.m?new Date(m.m).toLocaleString():'—'],['Owner',m.o||(path[1]=='USERS'?path[2]:'system')],['Access',prot?'Read only for you (system or another user)':'You can read and change it']];
 if(!isD){const d=ld(defK(),{})[ext(path.at(-1))];rows.push(['Opens with',d?(OPENERS.find(x=>x[0]==d)||[,d])[1]:'Rudolf OS default'])}
 const t=W.body.querySelector('.ptab');rows.forEach(([a,b])=>{const r=t.insertRow();const c1=r.insertCell(),c2=r.insertCell();c1.textContent=a;c2.textContent=b});
 const act=W.body.querySelector('.pact');const btn=(l,f)=>{const b=document.createElement('button');b.textContent=l;b.onclick=f;act.appendChild(b)};
 if(!isD)btn('Change default app…',()=>openWith(path,true));if(isD)btn(favList().some(p=>pkey(p)==pkey(path))?'Remove from favourites':'Add to favourites',()=>{favToggle(path);W.close()});btn('Close',()=>W.close())}
/* ---------- ZIP helpers ---------- */
const MIMEX={PNG:'image/png',JPG:'image/jpeg',JPEG:'image/jpeg',GIF:'image/gif',WEBP:'image/webp',SVG:'image/svg+xml',BMP:'image/bmp',MP3:'audio/mpeg',OGG:'audio/ogg',WAV:'audio/wav',MP4:'video/mp4',WEBM:'video/webm',PDF:'application/pdf',ZIP:'application/zip',WASM:'application/wasm',TTF:'font/ttf',OTF:'font/otf',WOFF:'font/woff',WOFF2:'font/woff2'};
const TEXTE=/^(TXT|LUA|MD|CSV|JSON|HTML?|CSS|JS|BAT|CFG|INI|XML|LOG|RSHEET|RDECK|SVG|DOC)$/;
async function vfsToZip(paths){const z=new JSZip();const add=(p,v,pre)=>{if(isDir(v)){const f=z.folder(pre+p.at(-1));for(const k in v)add([...p,k],v[k],pre+p.at(-1)+'/')}else if(typeof v=='string'&&v.startsWith('rudolfdisk:')){}else{if(typeof v=='string'&&v.startsWith('data:'))z.file(pre+p.at(-1),dataToBlob(v));else z.file(pre+p.at(-1),String(v))}};paths.forEach(p=>add(p,node(p),''));return z.generateAsync({type:'blob',compression:'DEFLATE'})}
async function blobToDataURL(b){return new Promise(r=>{const fr=new FileReader();fr.onload=()=>r(fr.result);fr.readAsDataURL(b)})}
async function zipToVfs(zipData,dest){const z=await JSZip.loadAsync(typeof zipData=='string'?dataToBlob(zipData):zipData);let n=0;
 for(const name of Object.keys(z.files)){const f=z.files[name];const parts=name.split('/').filter(Boolean).map(x=>x.toUpperCase().replace(/[\\:*?"<>|]/g,'_').slice(0,60));if(!parts.length)continue;let d=dest;for(const p of parts.slice(0,-1)){if(!isDir(d[p]))d[p]={};d=d[p]}
  const last=parts.at(-1);if(f.dir){if(!isDir(d[last]))d[last]={};continue}const e=ext(last);
  if(TEXTE.test(e)&&e!='SVG')d[last]=await f.async('string');else{const b=await f.async('blob');d[last]=await blobToDataURL(new Blob([b],{type:MIMEX[e]||'application/octet-stream'}))}n++}
 fsSave();return n}
/* ---------- the File Explorer window ---------- */
function explorer(start){const W=win('File Explorer','📁',900,560);W.app='explorer';
 const mkPane=p=>({path:Array.isArray(p)&&isDir(node(p))?p.slice():(USER?homePath():['C:']),back:[],fwd:[],sel:new Set(),anchor:null,view:S.xview||'list',filt:'',deep:false,sk:'name',sd:1,remote:p&&p.remote?p.remote:null,special:null});
 let tabs=[{panes:[mkPane(start)],act:0}],ti=0,undo=[],preview=!!S.xprev;
 const T=()=>tabs[ti],P=()=>T().panes[T().act];
 W.body.style.background='var(--wbg,#fff)';W.body.classList.add('xp25');
 W.body.innerHTML=`<div class='xtabs' role='tablist'></div><div class='xb'><button class='bk' title='Back (Alt+Left)' aria-label='Back'>◀</button><button class='fw' title='Forward (Alt+Right)' aria-label='Forward'>▶</button><button class='up' title='Up one level (Backspace)' aria-label='Up'>⬆</button><input class='ad' aria-label='Address' spellcheck='false'><input class='sr' type='search' placeholder='Search this folder' aria-label='Search'><label class='ck' title='Search in all subfolders and inside files'><input type='checkbox' class='deep'> Everywhere</label></div>
<div class='xb xcmd' role='toolbar' aria-label='Commands'><button class='nf'>📁 New folder</button><button class='nt'>📄 New file</button><button class='cu' title='Ctrl+X'>✂ Cut</button><button class='cp' title='Ctrl+C'>⧉ Copy</button><button class='ps' title='Ctrl+V'>📋 Paste</button><button class='rn' title='F2'>✏ Rename</button><button class='dl' title='Delete'>🗑 Delete</button><button class='un' title='Ctrl+Z'>↶ Undo</button><button class='zp'>📦 Compress</button><button class='ul'>⬆ Upload</button><button class='dn'>⬇ Download</button><span style='flex:1'></span><select class='vw' aria-label='View'><option value='list'>☰ Details</option><option value='icons'>▦ Icons</option><option value='thumbs'>🖼 Thumbnails</option></select><button class='pv' title='Preview pane (Alt+P)' aria-pressed='false'>👁 Preview</button><button class='spl' title='Split view (F3)'>◫ Split</button><input type='file' class='fi' multiple style='display:none'></div>
<div class='xmain'><nav class='xside' aria-label='Places'></nav><div class='xpanes'></div><aside class='xprev' aria-label='Preview'></aside></div><div class='stat' role='status'></div>`;
 const q=s=>W.body.querySelector(s),side=q('.xside'),panesEl=q('.xpanes'),prevEl=q('.xprev'),stat=q('.stat'),ad=q('.ad');
 const say=(t,bad)=>{stat.textContent=t;stat.style.color=bad?'#b91c1c':'';if(bad)sfx('error')};
 const dirOfP=pn=>pn.remote?null:node(pn.path);
 const canWrite=pth=>!isProt(pth)&&fsCanWrite(pth);
 async function needWrite(pth){if(fsCanWrite(pth)&&!isProt(pth))return true;if(!fsCanWrite(pth)){say('That folder belongs to another account.',true);return false}return elevate('Change files in the system folder '+pth.join('\\'))}
 function go(pn,p,rec){const n=node(p);if(!isDir(n)){say('That folder does not exist.',true);return false}if(rec!==false&&(pn.remote||pn.special||pkey(p)!=pkey(pn.path))){pn.back.push(pn.remote?{remote:pn.remote}:pn.special?{special:pn.special}:pn.path);pn.fwd=[]}pn.path=p.slice();pn.remote=null;pn.special=null;pn.sel=new Set();pn.anchor=null;pn.filt='';draw();return true}
 /* ---- sidebar ---- */
 function drawSide(){side.innerHTML='';const sec=t=>{const h=document.createElement('div');h.className='xsh';h.textContent=t;side.appendChild(h)};
  const item=(icon,label,on,act,drop)=>{const d=document.createElement('div');d.className='xsi'+(on?' on':'');d.tabIndex=0;d.setAttribute('role','link');d.innerHTML='<i></i><span></span>';d.querySelector('i').textContent=icon;d.querySelector('span').textContent=label;d.onclick=act;d.onkeydown=e=>{if(e.key=='Enter')act()};
   if(drop){d.ondragover=e=>{e.preventDefault();d.classList.add('dt')};d.ondragleave=()=>d.classList.remove('dt');d.ondrop=e=>{e.preventDefault();d.classList.remove('dt');drop(e)}}side.appendChild(d);return d};
  const pn=P();const at=p=>!pn.remote&&!pn.special&&pkey(pn.path)==pkey(p);
  if(USER){sec('Home');item('🏠',USER.n,at(homePath()),()=>go(pn,homePath()));[['🖥️','Desktop','DESKTOP'],['📄','Documents','DOCUMENTS'],['🖼️','Pictures','PICTURES'],['🎵','Music','MUSIC'],['🎬','Videos','VIDEOS'],['⬇️','Downloads','DOWNLOADS']].forEach(([i,l,s])=>{const p=homePath(s);if(isDir(node(p)))item(i,l,at(p),()=>go(pn,p),e=>dropTo(p,e.ctrlKey))});
   item('🕘','Recent',pn.special=='recent',()=>{pn.back.push(pn.path);pn.special='recent';pn.remote=null;draw()});const fv=favList();if(fv.length){sec('Favourites');fv.forEach(p=>item('⭐',p.at(-1),at(p),()=>go(pn,p),e=>dropTo(p,e.ctrlKey)))}}
  sec('This computer');item('💽','Rudolf OS (C:)',at(['C:']),()=>go(pn,['C:']),e=>dropTo(['C:'],e.ctrlKey));item('📦','Programs',at(['C:','PROGRAMS']),()=>go(pn,['C:','PROGRAMS']));item('🗑️','Trash',false,()=>trash(),e=>{const it=FC.drag||[];FC.drag=null;if(it.length){trashPut(it);EXPL.forEach(f=>f())}});
  if(RD){sec('Drives');const box=document.createElement('div');side.appendChild(box);item('🎮','Games (Emu library)',pn.remote&&pn.remote.dev=='games',()=>openRemote(pn,{dev:'games',root:'/home/rudolf/Games',path:'/home/rudolf/Games',label:'Games'}));rd('disks').then(r=>{r.disks.forEach(dk=>{if(dk.partitions.some(x=>x.label=='RUDOLFDATA'||x.fs=='iso9660'))return;dk.partitions.filter(x=>x.fs&&x.fs!='swap').forEach(pt=>{const lab=(pt.label||pt.name)+' ('+fmtB(pt.bytes)+')';const d=item(dk.removable?'🔌':'💽',lab,pn.remote&&pn.remote.dev==pt.name,async()=>{try{if(!pt.mount){await rdPost('mount?dev='+pt.name);pt.mount='/media/'+pt.name}openRemote(pn,{dev:pt.name,root:pt.mount,path:pt.mount,label:lab})}catch(e){say(e.message,true)}});if(dk.model)d.title=dk.model;
     if(dk.removable&&pt.mount){const ej=document.createElement('button');ej.className='xej';ej.textContent='⏏';ej.title='Eject '+(pt.label||pt.name)+' (safe to remove)';ej.setAttribute('aria-label',ej.title);ej.style.cssText='margin-left:auto;border:0;background:none;cursor:pointer;padding:0 4px';ej.onclick=async e=>{e.stopPropagation();try{await rdPost('eject?dev='+dk.name);say((pt.label||pt.name)+' can be removed now.')}catch(er){say(er.message,true)}};d.style.display='flex';d.appendChild(ej)}
     box.appendChild(d)})})}).catch(()=>{})}
  if(window.showDirectoryPicker){sec('Folders on this device');item('📂','Import a folder…',false,()=>hostImport());item('💾','Export selection to a folder…',false,()=>hostExport())}}
 /* ---- panes ---- */
 function names(pn){const d=dirOfP(pn)||{};let k=Object.keys(d);if(pn.filt&&!pn.deep)k=k.filter(n=>n.toLowerCase().includes(pn.filt.toLowerCase()));const key=n=>pn.sk=='name'?n.toLowerCase():pn.sk=='type'?fType(n,d[n]):pn.sk=='date'?((fileMeta([...pn.path,n])||{}).m||0):sizeOf(d[n]);
  k.sort((a,b)=>{const da=isDir(d[a]),db=isDir(d[b]);if(da!=db)return da?-1:1;const x=key(a),y=key(b);return(x<y?-1:x>y?1:0)*pn.sd});return k}
 function deepSearch(pn){const out=[],q2=pn.filt.toLowerCase();const walk=(n,p,dp)=>{if(out.length>=300||dp>12)return;for(const k in n){const v=n[k],pp=[...p,k];if(k.toLowerCase().includes(q2)||(typeof v=='string'&&v.length<400000&&!v.startsWith('data:')&&v.toLowerCase().includes(q2)))out.push(pp);if(isDir(v))walk(v,pp,dp+1)}};walk(node(pn.path),pn.path,0);return out}
 function rowsFor(pn){if(pn.special=='recent')return recentList().map(p=>({p,n:p.at(-1),v:node(p),sub:p.slice(0,-1).join('\\')}));
  if(pn.filt&&pn.deep)return deepSearch(pn).map(p=>({p,n:p.at(-1),v:node(p),sub:p.slice(0,-1).join('\\')}));const d=dirOfP(pn);return names(pn).map(n=>({p:[...pn.path,n],n,v:d[n]}))}
 function draw(){const tb=q('.xtabs');tb.innerHTML='';tabs.forEach((t,i)=>{const pn=t.panes[t.act];const b=document.createElement('div');b.className='xtab'+(i==ti?' on':'');b.setAttribute('role','tab');b.setAttribute('aria-selected',i==ti);b.tabIndex=0;b.innerHTML='<span></span><button aria-label="Close tab">✕</button>';
   b.querySelector('span').textContent=pn.remote?pn.remote.label:pn.special=='recent'?'Recent':(pn.path.at(-1)||'C:');b.onclick=()=>{ti=i;draw()};b.onauxclick=e=>{if(e.button==1)closeTab(i)};b.querySelector('button').onclick=e=>{e.stopPropagation();closeTab(i)};tb.appendChild(b)});
  const nb=document.createElement('button');nb.className='xtnew';nb.textContent='+';nb.title='New tab (Ctrl+T)';nb.setAttribute('aria-label','New tab');nb.onclick=()=>newTab();tb.appendChild(nb);
  const pn=P();ad.value=pn.remote?pn.remote.path:pn.special=='recent'?'Recent files':pn.path.join('\\')+(pn.path.length==1?'\\':'');W.setTitle('File Explorer - '+ad.value);q('.sr').value=pn.filt;q('.deep').checked=pn.deep;q('.vw').value=pn.view;q('.pv').setAttribute('aria-pressed',preview);
  prevEl.style.display=preview?'':'none';drawSide();panesEl.innerHTML='';T().panes.forEach((pp,i)=>panesEl.appendChild(drawPane(pp,i)));panesEl.classList.toggle('split',T().panes.length>1);updPrev();updStat()}
 function drawPane(pn,idx){const box=document.createElement('div');box.className='xc'+(T().panes.length>1&&idx==T().act?' act':'');box.tabIndex=0;box.setAttribute('role','grid');box.setAttribute('aria-label','Files');
  box.onfocus=box.onpointerdown=()=>{if(T().act!=idx){T().act=idx;draw();panesEl.children[idx]&&panesEl.children[idx].focus()}};
  if(pn.remote){drawRemote(pn,box);return box}
  const rows=rowsFor(pn);const showSub=pn.special=='recent'||(pn.filt&&pn.deep);const lnks=deskLinks(pn);
  if(pn.view=='list'){const t=document.createElement('table');const ar=k=>pn.sk==k?(pn.sd>0?' ▴':' ▾'):'';t.innerHTML=`<thead><tr><th data-s='name'>Name${ar('name')}</th><th data-s='date'>Modified${ar('date')}</th><th data-s='type'>Type${ar('type')}</th><th data-s='size' style='text-align:right'>Size${ar('size')}</th></tr></thead><tbody></tbody>`;const tbd=t.querySelector('tbody');
   lnks.forEach(l=>{const tr=document.createElement('tr');tr.className='xlnk';tr.innerHTML=`<td></td><td></td><td>Shortcut</td><td></td>`;tr.firstChild.append(lnkIcon(l),' '+l.name);lnkWire(tr,l);tbd.appendChild(tr)});
   rows.forEach(r=>{const tr=document.createElement('tr');tr.dataset.n=r.n;tr.dataset.k=pkey(r.p);if(pn.sel.has(pkey(r.p)))tr.classList.add('s');if(FC.op=='cut'&&FC.items.some(i=>pkey(i)==pkey(r.p)))tr.classList.add('cut');tr.draggable=true;tr.setAttribute('role','row');
    const m=fileMeta(r.p);tr.innerHTML=`<td></td><td>${m&&m.m?new Date(m.m).toLocaleDateString():''}</td><td>${fType(r.n,r.v)}</td><td style='text-align:right'>${isDir(r.v)?Object.keys(r.v).length+' item(s)':fmtB(sizeOf(r.v))}</td>`;tr.firstChild.textContent=fIcon(r.n,r.v)+' '+r.n;if(showSub){const s=document.createElement('small');s.textContent='  '+r.sub;s.style.opacity=.6;tr.firstChild.appendChild(s)}wire(pn,tr,r);tbd.appendChild(tr)});
   t.querySelectorAll('th').forEach(h=>h.onclick=()=>{if(pn.sk==h.dataset.s)pn.sd=-pn.sd;else{pn.sk=h.dataset.s;pn.sd=1}draw()});box.appendChild(t)}
  else{const g=document.createElement('div');g.className='xg'+(pn.view=='thumbs'?' thumbs':'');lnks.forEach(l=>{const c=document.createElement('div');c.className='xlnk';c.appendChild(lnkIcon(l));c.append(l.name);c.title=l.name+' — shortcut on the desktop';lnkWire(c,l);g.appendChild(c)});rows.forEach(r=>{const c=document.createElement('div');c.dataset.n=r.n;c.dataset.k=pkey(r.p);if(pn.sel.has(pkey(r.p)))c.classList.add('s');c.draggable=true;c.setAttribute('role','gridcell');
    if(pn.view=='thumbs'&&typeof r.v=='string'&&/^data:image\//.test(r.v)){const im=document.createElement('img');im.src=r.v;im.alt='';im.loading='lazy';c.appendChild(im)}else{const b=document.createElement('b');b.textContent=fIcon(r.n,r.v);c.appendChild(b)}c.append(r.n);c.title=r.n+(r.sub?' — '+r.sub:'');wire(pn,c,r);g.appendChild(c)});box.appendChild(g)}
  if(!rows.length&&!lnks.length){const e=document.createElement('div');e.className='xempty';e.textContent=pn.filt?'No matches.':pn.special=='recent'?'Files you open show up here.':'This folder is empty.';box.appendChild(e)}
  box.onclick=e=>{if(e.target===box||e.target.classList.contains('xg')||e.target.classList.contains('xempty')){pn.sel=new Set();markSel()}};
  box.oncontextmenu=e=>{if(e.target.closest('[data-k]'))return;e.preventDefault();pn.sel=new Set();markSel();ctx(e.clientX,e.clientY,false)};
  box.ondragover=e=>e.preventDefault();box.ondrop=e=>{e.preventDefault();if(e.dataTransfer.files&&e.dataTransfer.files.length&&!FC.drag)return uploadFiles([...e.dataTransfer.files]);if(pn.special)return;const it=FC.drag;FC.drag=null;if(it&&it.length&&pkey(it[0].slice(0,-1))!=pkey(pn.path))xfer(it,pn.path,e.ctrlKey?'copy':'cut')};
  box.onkeydown=e=>keys(e,pn);return box}
 function markSel(){const pn=P();panesEl.querySelectorAll('.xc.act [data-k],.xpanes:not(.split) [data-k]').forEach(r=>r.classList.toggle('s',pn.sel.has(r.dataset.k)));updPrev();updStat()}
 function selPaths(pn){pn=pn||P();return[...pn.sel].map(k=>k.split('/')).filter(p=>node(p)!=null)}
 /* the Desktop folder also lists the desktop's app shortcuts (My Computer, Notepad...), which are not files:
   shown first, opened like on the desktop; they cannot be renamed, moved or deleted from here */
function deskLinks(pn){if(!USER||pn.special||pn.remote||(pn.filt&&pn.deep)||pkey(pn.path)!=pkey(homePath('DESKTOP')))return [];
 return [...DESK.querySelectorAll(':scope > .ic:not(.fic)')].filter(e=>e.style.display!='none').map(el=>({el,ib:el.querySelector('b'),icon:(el.querySelector('b')||{}).textContent||'🔗',name:((el.querySelector('span')||{}).textContent||'').trim()})).filter(l=>l.name&&(!pn.filt||l.name.toLowerCase().includes(pn.filt.toLowerCase())))}
function lnkIcon(l){const b=l.ib?l.ib.cloneNode(true):document.createElement('b');if(!l.ib)b.textContent=l.icon;b.querySelectorAll('[id]').forEach(x=>x.removeAttribute('id'));return b}
function lnkOpen(l){const el=l.el;if(!el.isConnected||!el.onclick)return;if(window.__mouse&&S.msingle)el.onclick({});else{el.onclick({});el.onclick({})}el.classList.remove('sel')}
function lnkWire(c,l){c.tabIndex=0;c.setAttribute('role','gridcell');
 c.onclick=e=>{e.stopPropagation();c.parentNode.querySelectorAll('.xlnk.s').forEach(x=>x.classList.remove('s'));c.classList.add('s');if(window.__mouse&&S.msingle)lnkOpen(l)};
 c.ondblclick=e=>{e.stopPropagation();lnkOpen(l)};c.onkeydown=e=>{if(e.key=='Enter'){e.stopPropagation();lnkOpen(l)}};
 c.oncontextmenu=e=>{e.preventDefault();e.stopPropagation();cmenu(e.clientX,e.clientY,[['Open',()=>lnkOpen(l),true,{icon:'↗'}]],{title:l.name})}}
function wire(pn,el,r){const k=pkey(r.p);
  el.onclick=e=>{e.stopPropagation();const all=rowsFor(pn).map(x=>pkey(x.p));if(e.shiftKey&&pn.anchor&&all.includes(pn.anchor)){const a=all.indexOf(pn.anchor),b=all.indexOf(k);pn.sel=new Set(all.slice(Math.min(a,b),Math.max(a,b)+1))}else if(e.ctrlKey||e.metaKey){pn.sel.has(k)?pn.sel.delete(k):pn.sel.add(k);pn.anchor=k}else{pn.sel=new Set([k]);pn.anchor=k}markSel()};
  el.ondblclick=e=>{e.stopPropagation();openIt(pn,r.p)};
  el.oncontextmenu=e=>{e.preventDefault();e.stopPropagation();if(!pn.sel.has(k)){pn.sel=new Set([k]);pn.anchor=k;markSel()}ctx(e.clientX,e.clientY,true)};
  el.ondragstart=e=>{if(!pn.sel.has(k)){pn.sel=new Set([k]);pn.anchor=k}e.dataTransfer.setData('text/plain','bd95');e.dataTransfer.effectAllowed='copyMove';FC.drag=selPaths(pn)};
  el.ondragend=()=>{setTimeout(()=>{FC.drag=null},100)};
  if(isDir(r.v)){el.ondragover=e=>{e.preventDefault();el.classList.add('dt')};el.ondragleave=()=>el.classList.remove('dt');el.ondrop=e=>{e.preventDefault();e.stopPropagation();el.classList.remove('dt');dropTo(r.p,e.ctrlKey)}}}
 function openIt(pn,p){if(isDir(node(p)))go(pn,p);else openPath(p)}
 function dropTo(dest,copy){const items=FC.drag||[];FC.drag=null;if(items.length)xfer(items,dest,copy?'copy':'cut')}
 async function xfer(items,dest,mode){const dd=node(dest);if(!isDir(dd))return say('Destination not found.',true);if(!await needWrite(dest))return;let ok=0;const op={t:mode=='cut'?'move':'copy',items:[]};
  for(const it of items){const src=node(it);if(src==null)continue;if(isDir(src)&&under(it,dest))return say('A folder cannot be moved or copied into itself.',true);const par=dirOf(it);
   if(mode=='cut'){if(isProt(it)||!fsCanWrite(it)){say('System items cannot be moved.',true);continue}if(pkey(it.slice(0,-1))==pkey(dest))continue;const nm=uniq(dd,it.at(-1));dd[nm]=src;delete par[it.at(-1)];op.items.push({from:it,to:[...dest,nm]});ok++}
   else{const nm=uniq(dd,it.at(-1));dd[nm]=clone(src);op.items.push({to:[...dest,nm]});ok++}}
  if(ok){undo.push(op);fsSave();sfx('notify');say(`${ok} item(s) ${mode=='cut'?'moved':'copied'}.`)}if(mode=='cut'){FC.op=null;FC.items=[]}P().sel=new Set();EXPL.forEach(f=>f())}
 const doCut=()=>{const s=selPaths();if(!s.length)return say('Select something first.',true);FC.op='cut';FC.items=s;EXPL.forEach(f=>f());say(s.length+' item(s) cut — open a folder and press Paste.')};
 const doCopy=()=>{const s=selPaths();if(!s.length)return say('Select something first.',true);FC.op='copy';FC.items=s;say(s.length+' item(s) copied — open a folder and press Paste.');
  try{const txt=s.map(p=>node(p)).filter(v=>typeof v=='string'&&!v.startsWith('data:'));if(s.length==1&&txt.length==1&&txt[0].length<100000)navigator.clipboard.writeText(txt[0]).catch(()=>{})}catch(e){}};
 const doPaste=()=>{const pn=P();if(pn.remote)return remoteUpload(pn);if(!FC.op||!FC.items.length)return say('The clipboard is empty.',true);xfer(FC.items.filter(i=>node(i)!=null),pn.path,FC.op)};
 async function doDelete(){const s=selPaths();if(!s.length)return say('Select something first.',true);if(s.some(p=>isProt(p)&&p.length<=2))return say('System folders cannot be deleted.',true);if(s.some(p=>!canWrite(p.slice(0,-1)))&&!await needWrite(s[0].slice(0,-1)))return;
  dlg(`Move ${s.length==1?'“'+esc2(s[0].at(-1))+'”':s.length+' items'} to the Trash?<br><small>You can restore it from the Trash, or press Undo.</small>`,[['Cancel'],['Move to Trash',()=>{const before=TR.length;const r=trashPut(s);if(r<0){say('The Trash is full. Empty it first.',true);return}undo.push({t:'trash',ents:TR.slice(before)});P().sel=new Set();sfx('notify');EXPL.forEach(f=>f())},1]])}
 function doUndo(){const op=undo.pop();if(!op)return say('Nothing to undo.',true);
  if(op.t=='move')op.items.slice().reverse().forEach(m=>{const v=node(m.to);if(v==null)return;const fromD=node(m.from.slice(0,-1));if(!isDir(fromD))return;fromD[uniq(fromD,m.from.at(-1))]=v;delete dirOf(m.to)[m.to.at(-1)]});
  else if(op.t=='copy'||op.t=='create')op.items.forEach(m=>{const d=dirOf(m.to);if(d)delete d[m.to.at(-1)]});
  else if(op.t=='rename'){const d=node(op.dir);if(d&&op.nv in d){const v=d[op.nv];delete d[op.nv];d[op.old]=v}}
  else if(op.t=='trash')op.ents.forEach(e=>{const i=TR.indexOf(e);if(i>=0)trRestore(i)});
  fsSave();EXPL.forEach(f=>f());say('Undone.')}
 function dlg(html,btns){const o=document.createElement('div');o.className='dlg';o.innerHTML='<div>'+html+'<div style="text-align:right;margin-top:12px"></div></div>';const r=o.lastChild.lastChild;btns.forEach(([t,f,pri])=>{const b=document.createElement('button');b.textContent=t;b.style.marginLeft='6px';if(pri){b.style.background='var(--ac)';b.style.color='#fff'}b.onclick=()=>{o.remove();f&&f()};r.appendChild(b)});W.body.appendChild(o);r.lastChild.focus();return o}
 async function create(dirFlag){const pn=P();if(pn.special||pn.remote){if(pn.remote&&dirFlag)return remoteMkdir(pn);return}if(!await needWrite(pn.path))return;const d=node(pn.path),n=uniq(d,dirFlag?'NEW FOLDER':'NEW FILE.TXT');d[n]=dirFlag?{}:'';fsSave();undo.push({t:'create',items:[{to:[...pn.path,n]}]});pn.sel=new Set([pkey([...pn.path,n])]);EXPL.forEach(f=>f());startRename([...pn.path,n])}
 function startRename(p){const row=panesEl.querySelector(`[data-k="${CSS.escape(pkey(p))}"]`);if(!row)return;const n=p.at(-1),pn=P();const cell=row.tagName=='TR'?row.firstChild:row;const inp=document.createElement('input');inp.value=n;inp.style.cssText='width:calc(100% - 8px);box-sizing:border-box';cell.textContent='';cell.appendChild(inp);
  inp.focus();inp.setSelectionRange(0,n.lastIndexOf('.')>0?n.lastIndexOf('.'):n.length);let done=0;
  const fin=async ok=>{if(done)return;done=1;let err='';if(ok){const nv=inp.value.trim().toUpperCase(),d=dirOf(p);if(nv!=n){if(isProt(p)||!fsCanWrite(p))err='System items cannot be renamed.';else if(!validName(nv))err='Names cannot be empty or contain \\ / : * ? " < > |';else if(nv in d)err='An item with that name already exists.';else if(window.__sys&&!isDir(d[n])&&secNameBlocked(nv))err=secNameBlocked(nv);else{const ent=Object.entries(d);Object.keys(d).forEach(k=>delete d[k]);ent.forEach(([k,v])=>d[k==n?nv:k]=v);fsSave();undo.push({t:'rename',dir:p.slice(0,-1),old:n,nv});pn.sel=new Set([pkey([...p.slice(0,-1),nv])]);EXPL.forEach(f=>f());return}}}draw();if(err)say(err,true)};
  inp.onkeydown=e=>{e.stopPropagation();if(e.key=='Enter')fin(1);else if(e.key=='Escape')fin(0)};inp.onblur=()=>fin(1);inp.onclick=e=>e.stopPropagation();inp.ondblclick=e=>e.stopPropagation()}
 const doRename=()=>{const s=selPaths();if(s.length!=1)return say('Select exactly one item to rename.',true);startRename(s[0])};
 async function doZip(){const s=selPaths();if(!s.length)return say('Select files or folders to compress.',true);const pn=P();if(!await needWrite(pn.path))return;say('Compressing…');try{const b=await vfsToZip(s);const d=node(pn.path);const nm=uniq(d,(s.length==1?s[0].at(-1).replace(/\.[^.]*$/,''):'ARCHIVE')+'.ZIP');d[nm]=await blobToDataURL(new Blob([b],{type:'application/zip'}));fsSave();undo.push({t:'create',items:[{to:[...pn.path,nm]}]});EXPL.forEach(f=>f());say('Created '+nm+' ('+fmtB(b.size)+').')}catch(e){say('Could not compress: '+e.message,true)}}
 async function doUnzip(p){const pn=P();if(!await needWrite(pn.path))return;say('Extracting…');try{const d=node(p.slice(0,-1));const nm=uniq(d,p.at(-1).replace(/\.ZIP$/i,''));d[nm]={};const n=await zipToVfs(node(p),d[nm]);undo.push({t:'create',items:[{to:[...p.slice(0,-1),nm]}]});EXPL.forEach(f=>f());say(`Extracted ${n} file(s) to ${nm}.`)}catch(e){say('This ZIP file could not be read.',true)}}
 async function doDown(){const s=selPaths();if(!s.length)return say('Select a file to download.',true);if(s.length==1&&!isDir(node(s[0])))return saveOut(s[0].at(-1),node(s[0]));const b=await vfsToZip(s);saveOut((s.length==1?s[0].at(-1):'files')+'.zip',b)}
 async function doShare(){const s=selPaths().filter(p=>!isDir(node(p)));if(!s.length)return say('Select a file to share.',true);const files=s.map(p=>new File([dataToBlob(node(p))],p.at(-1).toLowerCase()));
  try{if(navigator.canShare&&navigator.canShare({files})){await navigator.share({files,title:s[0].at(-1)});return}}catch(e){if(e.name=='AbortError')return}
  cmenu(innerWidth/2-100,innerHeight/3,[['Copy to my Desktop',()=>{const d=node(homePath('DESKTOP'));s.forEach(p=>d[uniq(d,p.at(-1))]=clone(node(p)));fsSave();EXPL.forEach(f=>f());say('Copied to the Desktop.')}],['Download / save to this device',()=>doDown()],['Send by e-mail…',()=>{const t=s.map(p=>mlText(node(p))).filter(Boolean).join('\n\n');emailCompose({subject:s.map(p=>p.at(-1)).join(', ').slice(0,120),body:t,files:s.map(p=>[p.at(-1),node(p)])})}],['Open with…',()=>openWith(s[0])]],{title:'Share'})}
 function ctx(x,y,onItem){const s=selPaths(),one=s.length==1?s[0]:null,v=one&&node(one),isFile=one&&!isDir(v),pn=P();
  const items=onItem?[['Open',()=>openIt(pn,one),!!one,{icon:'↗',key:'Enter'}],['Open with…',()=>openWith(one),!!isFile],['Open in new tab',()=>newTab(one),!!(one&&isDir(v))],['Edit in Notepad',()=>notepad(one),!!(isFile&&typeof v=='string'&&!v.startsWith('data:'))],'-',['Cut',doCut,true,{key:'Ctrl+X'}],['Copy',doCopy,true,{key:'Ctrl+C'}],['Paste',doPaste,!!FC.op,{key:'Ctrl+V'}],'-',['Rename',doRename,!!one,{key:'F2'}],['Move to Trash',doDelete,true,{key:'Del'}],'-',
   ['Compress to ZIP',doZip],['Extract here',()=>doUnzip(one),!!(isFile&&ext(one.at(-1))=='ZIP')],['Download',doDown],['Share…',doShare,s.some(p=>!isDir(node(p)))],[one&&isDir(v)&&favList().some(p=>pkey(p)==pkey(one))?'Remove from favourites':'Add to favourites',()=>favToggle(one),!!(one&&isDir(v))],'-',['Properties',()=>fileProps(one),!!one,{key:'Alt+Enter'}]]
   :[['New folder',()=>create(1),!pn.special],['New file',()=>create(0),!pn.special&&!pn.remote],'-',['Paste',doPaste,!!FC.op||!!pn.remote,{key:'Ctrl+V'}],['Upload…',()=>q('.fi').click(),!pn.special],['Undo',doUndo,undo.length>0,{key:'Ctrl+Z'}],'-',['View as details',()=>{pn.view='list';draw()},true,{check:pn.view=='list'}],['View as icons',()=>{pn.view='icons';draw()},true,{check:pn.view=='icons'}],['View as thumbnails',()=>{pn.view='thumbs';draw()},true,{check:pn.view=='thumbs'}],'-',['Refresh',()=>draw(),true,{key:'F5'}],['Folder properties',()=>fileProps(pn.path),!pn.special&&!pn.remote]];
  cmenu(x,y,items,{title:onItem?(s.length>1?s.length+' items':(one||[''])[one?one.length-1:0]):(pn.remote?pn.remote.label:pn.path.join('\\'))})}
 function keys(e,pn){if(e.target.tagName=='INPUT')return;const k=e.key,c=e.ctrlKey||e.metaKey;
  if(k=='Enter'&&e.altKey){const s=selPaths();s[0]&&fileProps(s[0])}else if(k=='Enter'){const s=selPaths();if(s.length==1)openIt(pn,s[0])}else if(k=='Delete')doDelete();else if(k=='F2')doRename();else if(k=='F5')draw();else if(k=='F3')toggleSplit();
  else if(k=='Backspace'||(e.altKey&&k=='ArrowUp')){e.preventDefault();q('.up').click()}else if(e.altKey&&k=='ArrowLeft')q('.bk').click();else if(e.altKey&&k=='ArrowRight')q('.fw').click();else if(e.altKey&&k.toLowerCase()=='p'){preview=!preview;S.xprev=preview;draw()}
  else if(c&&k.toLowerCase()=='c')doCopy();else if(c&&k.toLowerCase()=='x')doCut();else if(c&&k.toLowerCase()=='v')doPaste();else if(c&&k.toLowerCase()=='z')doUndo();else if(c&&k.toLowerCase()=='t')newTab();else if(c&&k.toLowerCase()=='w')closeTab(ti);else if(c&&k.toLowerCase()=='f')q('.sr').focus();
  else if(c&&k.toLowerCase()=='a'){e.preventDefault();pn.sel=new Set(rowsFor(pn).map(r=>pkey(r.p)));draw()}
  else if(k=='ArrowDown'||k=='ArrowUp'){e.preventDefault();const ns=rowsFor(pn).map(r=>pkey(r.p));if(!ns.length)return;let i=ns.indexOf([...pn.sel].pop());i=k=='ArrowDown'?Math.min(ns.length-1,i+1):Math.max(0,i<0?0:i-1);pn.sel=new Set([ns[i]]);pn.anchor=ns[i];markSel();const el=panesEl.querySelector(`[data-k="${CSS.escape(ns[i])}"]`);el&&el.scrollIntoView({block:'nearest'});try{S.sr&&speak(ns[i].split('/').at(-1))}catch(x){}}else return;e.preventDefault()}
 /* ---- tabs and split view ---- */
 function newTab(p){tabs.push({panes:[mkPane(p||P().path)],act:0});ti=tabs.length-1;draw()}
 function closeTab(i){if(tabs.length==1)return W.close();tabs.splice(i,1);ti=Math.min(ti,tabs.length-1);draw()}
 function toggleSplit(){const t=T();if(t.panes.length>1){t.panes.length=1;t.act=0}else{t.panes.push(mkPane(P().path));t.act=1}draw()}
 /* ---- preview pane ---- */
 function updPrev(){if(!preview){prevEl.innerHTML='';return}const s=selPaths();prevEl.innerHTML='';if(s.length!=1){prevEl.innerHTML=`<div class='xempty'>${s.length?s.length+' items selected':'Select a file to preview it.'}</div>`;return}
  const p=s[0],v=node(p);const h=document.createElement('div');h.className='xph';h.textContent=fIcon(p.at(-1),v)+' '+p.at(-1);prevEl.appendChild(h);
  if(isDir(v)){const st=folderStats(v);const d=document.createElement('div');d.textContent=`${st.files} files, ${st.dirs} folders, ${fmtB(st.bytes)}`;prevEl.appendChild(d);return}
  if(typeof v=='string'&&/^data:image\//.test(v)){const im=document.createElement('img');im.src=v;im.alt=p.at(-1);prevEl.appendChild(im)}
  else if(typeof v=='string'&&/^data:audio\//.test(v)){const a=document.createElement('audio');a.controls=true;a.src=v;prevEl.appendChild(a)}
  else if(typeof v=='string'&&/^data:video\//.test(v)){const a=document.createElement('video');a.controls=true;a.src=v;a.style.maxWidth='100%';prevEl.appendChild(a)}
  else if(typeof v=='string'&&!v.startsWith('data:')&&!v.startsWith('rudolfrom:')&&!v.startsWith('rudolfdisk:')){const pre=document.createElement('pre');pre.textContent=v.slice(0,4000)+(v.length>4000?'\n…':'');prevEl.appendChild(pre)}
  else{const d=document.createElement('div');d.textContent=fType(p.at(-1),v);prevEl.appendChild(d)}
  const m=fileMeta(p);const inf=document.createElement('div');inf.className='xpinf';inf.textContent=`${fmtB(sizeOf(v))}${m&&m.m?' · modified '+new Date(m.m).toLocaleString():''}`;prevEl.appendChild(inf)}
 function updStat(){const pn=P();if(pn.remote)return;const n=rowsFor(pn).length;storageInfo().then(si=>{say(`${n} item(s)${pn.sel.size?' · '+pn.sel.size+' selected':''}  ·  Storage: ${fmtB(si.used)}${si.quota?' of '+fmtB(si.quota):''} used`)})}
 /* ---- uploads ---- */
 async function uploadFiles(fl){const pn=P();if(pn.remote)return remoteUploadFiles(pn,fl);if(pn.special)return;if(!await needWrite(pn.path))return;let ok=0;const d=node(pn.path);
  for(const f of fl){const mmb=window.__mm&&mmBin(f.name);if(f.size>64*1048576&&!mmb){say(f.name+' is larger than 64 MB and was skipped.',true);continue}const nm=f.name.toUpperCase().replace(/[\\\/:*?"<>|]/g,'_').slice(0,60);
   if(mmb&&f.size>1048576&&window.__rv){const rr=await rvIngest(f,nm);if(rr.ok){d[uniq(d,nm)]=rr.stub;ok++}else say(rr.msg,true);continue}
   const bin=!/^text\/|json|xml|javascript/.test(f.type)&&!/\.(txt|lua|md|csv|json|html?|css|js|bat|cfg|ini|log|rsheet|rdeck|xml)$/i.test(f.name);
   const val=await new Promise(r=>{const fr=new FileReader();fr.onload=()=>r(fr.result);bin?fr.readAsDataURL(f):fr.readAsText(f)});
   {const g=window.__sys&&secIngress(nm,val,'upload');if(g&&!g.ok){say(g.msg,true);continue}}if(window.__mm){const mi=await mmIngress(nm,val);if(mi){say(mi,true);continue}}d[uniq(d,nm)]=val;ok++}
  if(ok){fsSave();sfx('notify');say(ok+' file(s) uploaded.')}EXPL.forEach(f=>f())}
 q('.fi').onchange=e=>{const fl=[...e.target.files];e.target.value='';uploadFiles(fl)};
 /* ---- drives (bootable build) ---- */
 function openRemote(pn,r){pn.back.push(pn.remote?{remote:pn.remote}:pn.path);pn.remote=r;pn.special=null;pn.sel=new Set();draw()}
 async function drawRemote(pn,box){box.innerHTML=`<div class='xempty'>Loading…</div>`;try{const r=await rd('fs/list?path='+encodeURIComponent(pn.remote.path));const items=r.items.sort((a,b)=>(b.dir-a.dir)||a.name.localeCompare(b.name));box.innerHTML='';
   const bar=document.createElement('div');bar.className='xrb';bar.innerHTML=`<button class='rup'>⬆ Up</button><button class='rim'>⬇ Copy selected into Rudolf OS</button><button class='rpu'>⬆ Copy Rudolf OS clipboard here</button><button class='rmk'>📁 New folder</button><button class='rdl'>🗑 Delete</button><button class='rej'>⏏ Safely remove</button>`;box.appendChild(bar);
   const t=document.createElement('table');t.innerHTML='<thead><tr><th>Name</th><th>Modified</th><th style="text-align:right">Size</th></tr></thead><tbody></tbody>';const tb=t.querySelector('tbody');
   items.forEach(it=>{const tr=document.createElement('tr');tr.dataset.k='R:'+it.name;tr.innerHTML=`<td></td><td>${new Date(it.mtime).toLocaleDateString()}</td><td style='text-align:right'>${it.dir?'':fmtB(it.size)}</td>`;tr.firstChild.textContent=(it.dir?'📁 ':fIcon(it.name,'')+' ')+it.name;
    tr.onclick=e=>{e.stopPropagation();if(!(e.ctrlKey||e.metaKey))pn.sel=new Set();pn.sel.has(tr.dataset.k)?pn.sel.delete(tr.dataset.k):pn.sel.add(tr.dataset.k);box.querySelectorAll('tr[data-k]').forEach(x=>x.classList.toggle('s',pn.sel.has(x.dataset.k)))};
    tr.ondblclick=async()=>{if(it.dir){pn.remote=Object.assign({},pn.remote,{path:pn.remote.path+'/'+it.name});pn.sel=new Set();draw()}else{if(window.emuOpenReal&&/\.(sfc|smc|nds)$/i.test(it.name)){emuOpenReal(pn.remote.path+'/'+it.name);return}const p=await remoteImport(pn,[it]);p[0]&&openPath(p[0])}};tb.appendChild(tr)});box.appendChild(t);
   const cur=()=>items.filter(i=>pn.sel.has('R:'+i.name));
   bar.querySelector('.rup').onclick=()=>{if(pn.remote.path==pn.remote.root)return;pn.remote.path=pn.remote.path.replace(/\/[^/]+$/,'');draw()};
   bar.querySelector('.rim').onclick=()=>remoteImport(pn,cur());bar.querySelector('.rpu').onclick=()=>remoteUpload(pn);bar.querySelector('.rmk').onclick=()=>remoteMkdir(pn);
   bar.querySelector('.rdl').onclick=async()=>{const c=cur();if(!c.length)return say('Select files on the drive first.',true);for(const i of c){try{await rdPost('fs/delete?path='+encodeURIComponent(pn.remote.path+'/'+i.name))}catch(e){say(e.message,true)}}draw()};
   bar.querySelector('.rej').onclick=async()=>{try{await rdPost('umount?dev='+pn.remote.dev);say('You can now remove the drive.');go(pn,homePath())}catch(e){say(e.message,true)}};
   if(pn.msg){say(pn.msg,pn.msgBad);pn.msg=null}else say(items.length+' item(s) on '+pn.remote.label)}catch(e){box.innerHTML='';const d=document.createElement('div');d.className='xempty';d.textContent='Could not read the drive: '+e.message;box.appendChild(d)}}
 /* copy from a drive into Downloads: files, and (1.2.7) folders with everything in them (rudolfd's fs/tree). The whole
    copy is counted first: more than 5,000 files, or more than fits in this browser's storage, is refused before
    anything is copied; files over 256 MB are left out and listed; Cancel keeps what was copied so far. */
 async function remoteImport(pn,list){const DP=node(homePath('DOWNLOADS'))?homePath('DOWNLOADS'):['C:'],dest=node(DP);const out=[],skip=[],FN=n=>n.toUpperCase().replace(/[\\:*?"<>|]/g,'_').slice(0,60);
  const jobs=[];   // {src, parts: names below dest, size}; folders: {dir:true, parts}
  try{for(const it of list){const base=pn.remote.path+'/'+it.name;
    if(!it.dir){jobs.push({src:base,parts:[it.name],size:it.size});continue}
    jobs.push({dir:true,parts:[it.name]});say('Counting what is in '+it.name+'…');
    const tr=await rd('fs/tree?path='+encodeURIComponent(base));
    for(const e of tr.items||[]){const parts=[it.name,...e.p.split('/')];if(e.dir)jobs.push({dir:true,parts});else jobs.push({src:base+'/'+e.p,parts,size:e.size})}}}
  catch(e){say('Could not read the drive: '+e.message,true);return out}
  const fl=jobs.filter(j=>!j.dir),big=fl.filter(j=>j.size>256*1048576),todo=fl.filter(j=>j.size<=256*1048576);
  big.forEach(j=>skip.push(j.parts.join('/')+' (over 256 MB)'));
  const total=todo.reduce((a,j)=>a+j.size,0);
  if(todo.length>5000){say(`That is ${todo.length} files: copy at most 5,000 at a time (a folder further down, say).`,true);return out}
  if(list.some(i=>i.dir)){const si=await storageInfo();if(si.quota&&total*1.4>(si.quota*0.9-si.used)){say(`That is ${fmtB(total)}: it does not fit in this computer's Rudolf OS storage (${fmtB(Math.max(0,si.quota*0.9-si.used))} free). Copy a smaller folder, or keep it on the drive.`,true);return out}}
  // folders first, each name made unique once (at the top) and then followed down
  const map=new Map();
  const ensure=parts=>{const k=parts.join('/');if(map.has(k))return map.get(k);const par=parts.length>1?ensure(parts.slice(0,-1)):{d:dest,p:DP};
   const nm=uniq(par.d,FN(parts.at(-1)));par.d[nm]={};const v={d:par.d[nm],p:[...par.p,nm]};map.set(k,v);if(parts.length==1)out.push(v.p);return v};
  jobs.filter(j=>j.dir).forEach(j=>ensure(j.parts));
  let cancel=false,done=0,bytes=0;const cx=document.createElement('button');cx.textContent='Cancel';cx.className='rcx';cx.onclick=()=>{cancel=true};
  if(todo.length>1)stat.after(cx);
  try{for(const j of todo){if(cancel)break;
    say(todo.length>1?`Copying ${done+1} of ${todo.length} files (${fmtB(bytes)})…`:'Copying '+j.parts.at(-1)+'…');
    let b;try{const r=await rd('fs/read?path='+encodeURIComponent(j.src));b=await r.blob()}catch(e){skip.push(j.parts.join('/')+' (could not be read)');continue}
    const par=j.parts.length>1?ensure(j.parts.slice(0,-1)):{d:dest,p:DP};const nm=uniq(par.d,FN(j.parts.at(-1)));const e=ext(nm);
    par.d[nm]=TEXTE.test(e)&&e!='SVG'&&b.size<4*1048576?await b.text():await blobToDataURL(new Blob([b],{type:MIMEX[e]||b.type||'application/octet-stream'}));
    if(j.parts.length==1)out.push([...par.p,nm]);done++;bytes+=b.size}}
  finally{cx.remove()}
  if(out.length||done){fsSave();EXPL.forEach(f=>f());
   const what=out.map(p=>p.at(-1)).join(', ');
   notify(cancel?'Copying stopped':'Copied into '+(DP.length>1?'Downloads':DP[0]),(cancel?`${done} of ${todo.length} files were copied: `:'')+what+(skip.length?` · left out: ${skip.slice(0,5).join(', ')}${skip.length>5?' and '+(skip.length-5)+' more':''}`:''))}
  /* the result stays in the status bar after the drive's list is drawn again */
  const msg=[cancel?`Stopped: ${done} of ${todo.length} files copied.`:todo.length>1?`Copied ${done} files (${fmtB(bytes)}) into ${DP.at(-1)}.`:'',
   skip.length?`Left out ${skip.length}: ${skip.slice(0,3).join(', ')}${skip.length>3?'…':''}`:''].filter(Boolean).join(' ');
  if(msg){pn.msg=msg;pn.msgBad=!!skip.length;say(msg,!!skip.length)}
  return out}
 async function remoteUpload(pn){if(!FC.items.length)return say('Copy files in Rudolf OS first, then paste them here.',true);for(const p of FC.items){const v=node(p);if(v==null||isDir(v))continue;say('Writing '+p.at(-1)+'…');try{await rd('fs/write?path='+encodeURIComponent(pn.remote.path+'/'+p.at(-1).toLowerCase()),{method:'POST',body:dataToBlob(v)})}catch(e){say(e.message,true);return}}draw()}
 async function remoteUploadFiles(pn,fl){for(const f of fl){try{await rd('fs/write?path='+encodeURIComponent(pn.remote.path+'/'+f.name),{method:'POST',body:f})}catch(e){say(e.message,true)}}draw()}
 async function remoteMkdir(pn){const n='New folder '+new Date().toLocaleTimeString().replace(/:/g,'.');try{await rdPost('fs/mkdir?path='+encodeURIComponent(pn.remote.path+'/'+n));draw()}catch(e){say(e.message,true)}}
 /* ---- folders on this device (File System Access API) ---- */
 async function hostImport(){const pn=P();if(pn.special||pn.remote||!await needWrite(pn.path))return;try{const h=await showDirectoryPicker();const dest=node(pn.path);const nm=uniq(dest,h.name.toUpperCase().slice(0,60));dest[nm]={};let n=0;
   const walk=async(dh,d)=>{for await(const [name,e] of dh.entries()){const N=name.toUpperCase().replace(/[\\:*?"<>|]/g,'_').slice(0,60);if(e.kind=='directory'){d[N]={};await walk(e,d[N])}else{const f=await e.getFile();if(f.size>64*1048576)continue;const E=ext(N);d[N]=TEXTE.test(E)&&E!='SVG'?await f.text():await blobToDataURL(f);n++}}};
   say('Importing…');await walk(h,dest[nm]);fsSave();EXPL.forEach(f=>f());say(`Imported ${n} file(s) into ${nm}.`)}catch(e){if(e.name!='AbortError')say('Folder access is not allowed here.',true)}}
 async function hostExport(){const s=selPaths();if(!s.length)return say('Select what to export first.',true);try{const h=await showDirectoryPicker({mode:'readwrite'});let n=0;
   const put=async(dh,p,v)=>{if(isDir(v)){const sub=await dh.getDirectoryHandle(p.at(-1),{create:true});for(const k in v)await put(sub,[...p,k],v[k])}else{const fh=await dh.getFileHandle(p.at(-1).toLowerCase(),{create:true});const w=await fh.createWritable();await w.write(dataToBlob(v));await w.close();n++}};
   for(const p of s)await put(h,p,node(p));say(`Exported ${n} file(s).`)}catch(e){if(e.name!='AbortError')say('Folder access is not allowed here.',true)}}
 /* ---- wiring ---- */
 q('.bk').onclick=()=>{const pn=P();if(!pn.back.length)return;const b=pn.back.pop();pn.fwd.push(pn.remote?{remote:pn.remote}:pn.special?{special:pn.special}:pn.path);if(b.remote){pn.remote=b.remote;pn.special=null}else if(b.special){pn.special=b.special;pn.remote=null}else{pn.path=b;pn.remote=null;pn.special=null}pn.sel=new Set();draw()};
 q('.fw').onclick=()=>{const pn=P();if(!pn.fwd.length)return;const f=pn.fwd.pop();pn.back.push(pn.remote?{remote:pn.remote}:pn.special?{special:pn.special}:pn.path);if(f.remote){pn.remote=f.remote}else if(f.special){pn.special=f.special}else{pn.path=f;pn.remote=null;pn.special=null}pn.sel=new Set();draw()};
 q('.up').onclick=()=>{const pn=P();if(pn.remote){if(pn.remote.path!=pn.remote.root){pn.remote.path=pn.remote.path.replace(/\/[^/]+$/,'');draw()}return}if(pn.special){go(pn,pn.path);return}if(pn.path.length>1)go(pn,pn.path.slice(0,-1))};
 q('.nf').onclick=()=>create(1);q('.nt').onclick=()=>create(0);q('.cu').onclick=doCut;q('.cp').onclick=doCopy;q('.ps').onclick=doPaste;q('.rn').onclick=doRename;q('.dl').onclick=doDelete;q('.un').onclick=doUndo;q('.zp').onclick=doZip;q('.ul').onclick=()=>q('.fi').click();q('.dn').onclick=doDown;
 q('.vw').onchange=e=>{P().view=e.target.value;S.xview=e.target.value;draw()};q('.pv').onclick=()=>{preview=!preview;S.xprev=preview;draw()};q('.spl').onclick=toggleSplit;
 ad.onkeydown=e=>{if(e.key=='Enter'){const p=resolve(ad.value.trim()||'C:\\',['C:']);if(!go(P(),p))ad.value=P().path.join('\\')}};
 let srT=0;q('.sr').oninput=e=>{clearTimeout(srT);srT=setTimeout(()=>{P().filt=e.target.value;draw();q('.sr').focus()},200)};q('.deep').onchange=e=>{P().deep=e.target.checked;draw()};
 W.el.addEventListener('keydown',e=>{if(e.target.closest('.xc'))return;if((e.ctrlKey||e.metaKey)&&['t','w'].includes(e.key.toLowerCase())){e.preventDefault();keys(e,P())}});
 EXPL.add(draw);W.refresh=draw;const onDrive=e=>{const v=e.detail||{};if(v.a!='add')tabs.forEach(t=>t.panes.forEach(pn=>{if(pn.remote&&pn.remote.dev==v.dev){pn.remote=null;pn.path=USER?homePath():['C:']}}));draw()};
 addEventListener('rudolf-drive',onDrive);
 W.onclose=()=>{EXPL.delete(draw);removeEventListener('rudolf-drive',onDrive)};
 W.openPath=p=>{go(P(),p)};
 draw();setTimeout(()=>{const c=panesEl.querySelector('.xc');c&&c.focus()},30);return W}
