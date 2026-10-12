/* ===== Linux apps from Flathub (Flatpak) =====
   App Store › Linux apps lists Flathub's catalogue (rudolfd apps/catalog); apps install system-wide on
   RUDOLFDATA (apps/install) and appear in the Start menu with their own icons. Their windows get taskbar
   buttons (rudolf-display lists them; apps/show, apps/close); a click anywhere else on the taskbar
   returns to the desktop. Documents, Pictures and Downloads are kept in step with real folders in the
   home folder so apps can open and save there, and Files' "Open with" lists the apps that handle a
   file type. Apps' file dialogs, links and notifications arrive through the Rudolf portal (portal/poll)
   and are answered with Rudolf's own dialogs. */
const LX={cat:null,inst:null,instAt:0,wins:[],mode:'desktop',shown:'',icons:new Map(),seen:new Set(),syncing:null};
const lxEsc=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
async function lxApi(path,body){const r=await fetch('/api/'+path,{method:'POST',headers:{'X-Rudolf-Token':RD.token},body});const ct=r.headers.get('content-type')||'';
 if(ct.includes('json')){const j=await r.json();if(!j.ok)throw new Error(j.error||j.message||'Request failed');return j}if(!r.ok)throw new Error('HTTP '+r.status);return r}
function lxIcon(id,icon){const k=id+'|'+(icon||'');if(!LX.icons.has(k))LX.icons.set(k,lxApi('apps/icon?id='+encodeURIComponent(id)+(icon?'&icon='+encodeURIComponent(icon):'')).then(r=>r.blob()).then(b=>URL.createObjectURL(b)).catch(()=>null));return LX.icons.get(k)}
function lxImg(el,id,icon){lxIcon(id,icon).then(u=>{if(u&&el.isConnected){el.textContent='';const i=new Image();i.src=u;i.alt='';el.appendChild(i)}})}
const lxJobWait=onLine=>new Promise(res=>{let seen=0;const iv=setInterval(async()=>{let j;try{j=await rd('job')}catch(e){return}const ls=(j.log||'').split('\n');if(onLine)for(;seen<ls.length-1;seen++)onLine(ls[seen]);if(!j.running&&j.status>=0){clearInterval(iv);res(j)}},900)});
/* ---------- installed apps: Start menu, search, launching ---------- */
const LXK='bd95lx';
async function lxInstalled(force){if(!RD)return[];if(!force&&LX.inst&&Date.now()-LX.instAt<20000)return LX.inst;
 try{const j=await lxApi('apps/installed');LX.inst=j.apps||[];LX.instAt=Date.now();svj(LXK,LX.inst.map(a=>({id:a.id,name:a.name,launchers:a.launchers})));lxRegister()}catch(e){if(!LX.inst)LX.inst=[]}return LX.inst}
function lxLaunchers(){const l=LX.inst||ld(LXK,[]);const out=[];l.forEach(a=>(a.launchers&&a.launchers.length?a.launchers:[{desktop:a.id,name:a.name,icon:a.id,mime:'',wmclass:'',cats:''}]).forEach(x=>out.push({...x,app:a.id})));return out}
function lxRegister(){Object.keys(APPS).filter(k=>k.startsWith('lx:')).forEach(k=>delete APPS[k]);
 lxLaunchers().forEach(x=>regApp('lx:'+x.desktop,{name:x.name,icon:'🐧',kw:'linux app '+(x.comment||'')+' '+(x.cats||'').replace(/;/g,' '),open:()=>lxRun(x),re:/^\u0000/}));
 try{startMenuApps()}catch(e){}lxOpeners()}
async function lxRun(x,file){if(!RD)return;try{await lxApi('apps/run?id='+encodeURIComponent(x.app)+'&desktop='+encodeURIComponent(x.desktop)+(file?'&file='+encodeURIComponent(file):''));
 if(!lxRun.told){lxRun.told=1;notify('Starting '+x.name+'…','It opens above the desktop in a moment. Click the desktop part of the taskbar (or press Super+Esc) to come back; its taskbar button brings it back.')}}catch(e){notify(x.name,e.message,{kind:'error'})}}
{const _sma=startMenuApps;startMenuApps=function(){_sma();const ul=$('#menu ul');if(!ul)return;ul.querySelectorAll('li.lxv').forEach(x=>x.remove());
 const ls=lxLaunchers().sort((a,b)=>a.name.localeCompare(b.name));if(!ls.length)return;const before=ul.querySelector('li[data-a=settings]')||ul.querySelector('li[data-a=run]');
 ls.forEach(x=>{const li=document.createElement('li');li.className='lxv';li.tabIndex=0;li.title=x.comment||x.name;li.innerHTML=`<span class='lxmi'>🐧</span> ${lxEsc(x.name)}`;lxImg(li.querySelector('.lxmi'),x.app,x.icon);
  li.onclick=e=>{e.stopPropagation();$('#menu').style.display='none';lxRun(x)};li.onkeydown=e=>{if(e.key=='Enter')li.click()};ul.insertBefore(li,before)})}}
/* ---------- Open with: apps whose launcher declares the file type ---------- */
const LX_MIME={txt:'text/plain',md:'text/markdown',csv:'text/csv',htm:'text/html',html:'text/html',xml:'application/xml',json:'application/json',js:'text/javascript',py:'text/x-python',c:'text/x-csrc',h:'text/x-chdr',sh:'application/x-shellscript',
 doc:'application/msword',docx:'application/vnd.openxmlformats-officedocument.wordprocessingml.document',odt:'application/vnd.oasis.opendocument.text',rtf:'application/rtf',
 xls:'application/vnd.ms-excel',xlsx:'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet',ods:'application/vnd.oasis.opendocument.spreadsheet',
 ppt:'application/vnd.ms-powerpoint',pptx:'application/vnd.openxmlformats-officedocument.presentationml.presentation',odp:'application/vnd.oasis.opendocument.presentation',pdf:'application/pdf',
 png:'image/png',jpg:'image/jpeg',jpeg:'image/jpeg',gif:'image/gif',bmp:'image/bmp',webp:'image/webp',svg:'image/svg+xml',tif:'image/tiff',tiff:'image/tiff',xcf:'image/x-xcf',ico:'image/vnd.microsoft.icon',
 mp3:'audio/mpeg',wav:'audio/x-wav',ogg:'audio/ogg',flac:'audio/flac',m4a:'audio/mp4',mp4:'video/mp4',mkv:'video/x-matroska',webm:'video/webm',avi:'video/x-msvideo',mov:'video/quicktime',zip:'application/zip'};
function lxHandles(x,e){const m=LX_MIME[String(e||'').toLowerCase()];if(!m||!x.mime)return false;return x.mime.split(';').some(t=>t&&(t==m||(t.endsWith('/*')&&m.startsWith(t.slice(0,-1)))))}
function lxOpeners(){if(typeof OPENERS=='undefined')return;for(let i=OPENERS.length;i--;)if(String(OPENERS[i][0]).startsWith('lx:'))OPENERS.splice(i,1);
 lxLaunchers().filter(x=>x.mime).forEach(x=>OPENERS.push(['lx:'+x.desktop,x.name+' (Linux app)','🐧',e=>lxHandles(x,e),p=>lxOpenFile(x,p)]))}
async function lxOpenFile(x,path){const real=lxRealPath(path);
 if(real){await lxSync();lxRun(x,real);return}
 /* outside the shared folders: a copy in Downloads, which the app can reach */
 try{const name=path.at(-1),v=node(path);await lxApi('fs/write?path='+encodeURIComponent('/home/rudolf/Downloads/'+name),dataToBlob(v));
  notify('Opened a copy',name+' was copied to Downloads so '+x.name+' can open it.');lxRun(x,'/home/rudolf/Downloads/'+name)}catch(e){notify(x.name,e.message,{kind:'error'})}}
/* ---------- the folders shared with apps ---------- */
const LX_SHARED=[['DOCUMENTS','Documents'],['PICTURES','Pictures'],['DOWNLOADS','Downloads']];
function lxRealPath(path){if(!USER)return null;for(const[k,r]of LX_SHARED){const h=homePath(k);if(path.length>h.length&&h.every((x,i)=>String(path[i]).toUpperCase()==String(x).toUpperCase()))return'/home/rudolf/'+r+'/'+path.slice(h.length).join('/')}return null}
function lxDeskPath(real){if(!USER)return null;for(const[k,r]of LX_SHARED){const pre='/home/rudolf/'+r;if(real==pre)return homePath(k);if(real.startsWith(pre+'/'))return[...homePath(k),...real.slice(pre.length+1).split('/')]}return null}
const LX_TEXT=/\.(txt|md|csv|json|xml|html?|js|css|py|c|h|sh|ini|log|svg)$/i;
function lxHash(v){let h=0x811c9dc5;const s=typeof v=='string'?v:JSON.stringify(v);for(let i=0;i<s.length;i+=s.length>2e6?7:1){h^=s.charCodeAt(i);h=Math.imul(h,16777619)}return(h>>>0).toString(36)+':'+s.length}
function lxOwner(){const o=ld('bd95lxowner',null);if(!o&&USER){svj('bd95lxowner',uid());return uid()}return o}
/* two-way sync of one shared folder: changes on one side since the last sync are copied to the other;
   a file changed on both sides keeps the app's version and saves the desktop's as "(conflict)" */
async function lxSyncFolder(key,real){const base='/home/rudolf/'+real,dk=homePath(key);let root=node(dk);if(!isDir(root))return 0;
 const SK='bd95lxsync:'+uid(),st=ld(SK,{});let n=0;
 const tree=(await lxApi('fs/tree?path='+encodeURIComponent(base))).items;const R=new Map(tree.map(i=>[real+'/'+i.p,i]));
 const D=new Map();(function walk(o,rel){Object.keys(o).forEach(k=>{const p=rel?rel+'/'+k:k,v=o[k];D.set(real+'/'+p,{dir:isDir(v),v});if(isDir(v))walk(v,p)})})(root,'');
 const dset=(p,v)=>{const parts=p.split('/').slice(1),name=parts.pop();let o=root;for(const s of parts){if(!isDir(o[s]))o[s]={};o=o[s]}o[name]=v};
 const ddel=p=>{const parts=p.split('/').slice(1),name=parts.pop();let o=root;for(const s of parts){o=o[s];if(!isDir(o))return}delete o[name]};
 const rpath=p=>'/home/rudolf/'+p;
 const push=async(p,v)=>{const j=await lxApi('fs/write?path='+encodeURIComponent(rpath(p)),dataToBlob(v));st[p]={h:lxHash(v),m:j.mtime,s:j.size};n++};
 const pull=async(p,r)=>{if(r.size>20e6)return;const b=await(await lxApi('fs/read?path='+encodeURIComponent(rpath(p)))).blob();const v=LX_TEXT.test(p)&&r.size<4e6?await b.text():await blobToDataURL(b);dset(p,v);st[p]={h:lxHash(v),m:r.mtime,s:r.size};n++};
 const keys=[...new Set([...R.keys(),...D.keys()])].sort();
 for(const p of keys){const d=D.get(p),r=R.get(p),s=st[p];
  try{if(d&&d.dir||r&&r.dir){if(d&&d.dir&&!r){if(s){ddel(p);delete st[p];n++}else{await lxApi('fs/mkdir?path='+encodeURIComponent(rpath(p)));st[p]={dir:1}}}
    else if(r&&r.dir&&!d){if(s){/* folder removed on the desktop: removed once empty, below */}else{dset(p,{});st[p]={dir:1};n++}}else if(d&&r)st[p]={dir:1};continue}
   if(d&&r){const dc=!s||s.h!==lxHash(d.v),rc=!s||s.m!==r.mtime||s.s!==r.size;
    if(!s){const size=dataToBlob(d.v).size;if(size==r.size){st[p]={h:lxHash(d.v),m:r.mtime,s:r.size}}else{const c=p.replace(/(\.[^./]*)?$/,' (conflict)$1');dset(c,d.v);await pull(p,r)}}
    else if(dc&&!rc)await push(p,d.v);else if(rc&&!dc)await pull(p,r);
    else if(dc&&rc){const c=p.replace(/(\.[^./]*)?$/,' (conflict)$1');dset(c,d.v);await pull(p,r)}}
   else if(d){if(s){ddel(p);delete st[p];n++}else await push(p,d.v)}
   else if(r){if(s){await lxApi('fs/delete?path='+encodeURIComponent(rpath(p))).catch(()=>{});delete st[p];n++}else await pull(p,r)}
   else delete st[p]}catch(e){console.warn('Linux apps sync',p,e)}}
 /* folders deleted on the desktop: remove them on disk once their files are gone (deepest first) */
 for(const p of keys.filter(p=>R.get(p)&&R.get(p).dir&&!D.get(p)&&st[p]).sort((a,b)=>b.length-a.length)){try{await lxApi('fs/delete?path='+encodeURIComponent(rpath(p)));delete st[p]}catch(e){}}
 svj(SK,st);if(n){fsSave();try{EXPL.forEach(f=>f())}catch(e){}}return n}
function lxSync(){if(!RD||!USER||LX.syncing)return LX.syncing||Promise.resolve(0);if(!(LX.inst||ld(LXK,[])).length&&!(typeof DEB!='undefined'&&DEB.st&&DEB.st.installed))return Promise.resolve(0);
 if(lxOwner()!=uid())return Promise.resolve(0);
 LX.syncing=(async()=>{let n=0;for(const[k,r]of LX_SHARED){try{n+=await lxSyncFolder(k,r)}catch(e){}}return n})().finally(()=>LX.syncing=null);return LX.syncing}
{const _fs=fsSave;fsSave=function(){const r=_fs.apply(this,arguments);if(RD&&!lxSync.busy){clearTimeout(lxSync.t);lxSync.t=setTimeout(()=>{lxSync.busy=1;lxSync().finally(()=>lxSync.busy=0)},2500)}return r}}
/* ---------- taskbar buttons for app windows ---------- */
function lxAppFor(w){const c=(w.class||'').toLowerCase(),i=(w.instance||'').toLowerCase();const ls=lxLaunchers();
 return ls.find(x=>x.wmclass&&(x.wmclass.toLowerCase()==c||x.wmclass.toLowerCase()==i))||ls.find(x=>x.desktop.toLowerCase()==c||x.app.toLowerCase()==c)||
  ls.find(x=>{const last=x.app.split('.').pop().toLowerCase();return last.length>2&&(c.startsWith(last)||i.startsWith(last))})||null}
function lxTaskbar(){const T=$('#tasks');if(!T)return;T.querySelectorAll('.lxtb').forEach(b=>{if(!LX.wins.some(w=>w.win==b.dataset.win))b.remove()});
 LX.wins.filter(w=>!w.web).forEach(w=>{let b=T.querySelector(`.lxtb[data-win="${w.win}"]`);const a=lxAppFor(w),label=w.title||(a&&a.name)||w.class||'Linux app';
  if(!b){b=document.createElement('button');b.className='lxtb';b.dataset.win=w.win;b.innerHTML=`<span class='lxti'>🐧</span><span class='lxtt'></span>`;if(a)lxImg(b.querySelector('.lxti'),a.app,a.icon);
   b.onclick=e=>{e.stopPropagation();const cur=LX.wins.find(x=>x.win==b.dataset.win);lxApi(cur&&cur.shown?'apps/desktop':'apps/show?win='+b.dataset.win).then(lxWinPoll).catch(()=>{})};
   b.oncontextmenu=e=>{e.preventDefault();e.stopPropagation();cmenu(e.clientX,e.clientY,[['Show',()=>lxApi('apps/show?win='+b.dataset.win)],['Close window',()=>lxApi('apps/close?win='+b.dataset.win)]],{title:label})};T.appendChild(b)}
  b.querySelector('.lxtt').textContent=label;b.title=label;b.classList.toggle('on',!!w.shown);b.setAttribute('aria-pressed',w.shown?'true':'false')})}
async function lxWinPoll(){try{const j=await lxApi('apps/windows');const was=LX.mode;LX.wins=j.windows||[];LX.mode=j.mode||'desktop';const sh=LX.wins.find(w=>w.shown);if(sh)LX.shown=sh.win;lxTaskbar();
  if(was.startsWith('win')&&LX.mode=='desktop')lxSync()}catch(e){}}
/* a click on the visible part of the desktop (the taskbar) while an app is in front brings the desktop back */
addEventListener('pointerdown',e=>{if(RD&&LX.mode.startsWith('win')&&!e.target.closest('.lxtb')&&!(TRAYX.web&&TRAYX.web.contains(e.target))){LX.mode='desktop';lxApi('apps/desktop').then(lxWinPoll).catch(()=>{})}},true);
/* ---------- the portal: file dialogs, links, notifications from apps ---------- */
async function lxPortalPoll(){let j;try{j=await lxApi('portal/poll')}catch(e){return}
 for(const q of j.requests||[]){if(LX.seen.has(q.id))continue;LX.seen.add(q.id);lxPortal(q.id,q.req).catch(e=>console.warn('portal',e))}}
function lxAppName(id){const a=(LX.inst||[]).find(x=>x.id==id);return a?a.name:(id?id.split('.').pop():'A Linux app')}
async function lxAnswer(id,obj,back){await lxApi('portal/answer?id='+encodeURIComponent(id)+(back?'&back='+back:''),JSON.stringify(obj)).catch(()=>{})}
async function lxPortal(id,r){const name=lxAppName(r.app),back=LX.shown;
 if(r.kind=='notify'){notify(r.title||name,r.body||'');await lxAnswer(id,{});return}
 if(r.kind=='url'){await lxAnswer(id,{response:0});const u=String(r.uri||'');
  if(/^mailto:/i.test(u)){await lxApi('apps/desktop');emailCompose({to:decodeURIComponent(u.slice(7).split('?')[0])})}
  else if(/^https?:/i.test(u)){if(webWants(u))webOpen(u);else{await lxApi('apps/desktop');browser(u)}}
  else if(/^file:/i.test(u)){const p=lxDeskPath(decodeURIComponent(u.replace(/^file:\/\//,'')));if(p){await lxApi('apps/desktop');openPathDefault(p)}}return}
 if(r.kind=='openfile'||r.kind=='folder'){await lxAnswer(id,{response:0});const p=r.path&&lxDeskPath(r.path);if(p){await lxApi('apps/desktop');r.kind=='folder'?explorer(p):openPathDefault(p)}else notify(name,'It asked to open '+(r.path||'a file')+', which is outside your shared folders.');return}
 if(r.kind=='open'||r.kind=='save'){await lxApi('apps/desktop');await lxSync().catch(()=>{});const res=await lxFileDialog(r,name);await lxAnswer(id,res?{response:0,uris:res}:{response:1},back);setTimeout(lxWinPoll,300)}}
function lxFileDialog(r,name){return new Promise(done=>{const save=r.kind=='save';let cur='/home/rudolf/'+(r.folder&&/\/home\/rudolf\/(Documents|Pictures|Downloads)/.test(r.folder)?r.folder.replace(/^\/home\/rudolf\//,''):'Documents');let all=false;const sel=new Set();
 const pats=(r.patterns||[]).map(p=>new RegExp('^'+p.replace(/[.+^${}()|\\]/g,'\\$&').replace(/\*/g,'.*').replace(/\?/g,'.')+'$','i'));
 const o=document.createElement('div');o.className='rmodal';o.innerHTML=`<div class='rmbox lxfd' role='dialog' aria-label='${save?'Save':'Open'}'><h3></h3><div class='lxfdb'><div class='lxpl'>${LX_SHARED.map(([k,f])=>`<button data-f='${f}'>${{Documents:'📄',Pictures:'🖼️',Downloads:'⬇️'}[f]} ${f}</button>`).join('')}</div><div class='lxmain'><div class='lxcr'></div><div class='lxls' role='listbox'></div></div></div>
${save?`<label class='lxnm'>Name <input class='nm'></label>`:''}<div class='rmrow'>${pats.length?`<label class='ck' style='margin-right:auto'><input type='checkbox' class='al'> Show all files</label>`:''}<button class='c'>Cancel</button><button class='ok pri'>${lxEsc(r.accept||(save?'Save':r.directory?'Choose this folder':'Open'))}</button></div></div>`;
 o.querySelector('h3').textContent=(r.title||(save?'Save':'Open'))+' — '+name;document.body.appendChild(o);const q=s=>o.querySelector(s),nm=q('.nm');if(nm)nm.value=r.name||'';
 const end=v=>{o.remove();done(v)},uri=p=>'file://'+p.split('/').map(encodeURIComponent).join('/');
 async function draw(){q('.lxcr').textContent=cur.replace('/home/rudolf/','').split('/').join(' › ');const L=q('.lxls');L.textContent='Loading…';let items=[];try{items=(await lxApi('fs/list?path='+encodeURIComponent(cur))).items}catch(e){L.textContent=e.message;return}
  items=items.filter(i=>!i.name.startsWith('.')&&(i.dir||all||!pats.length||pats.some(re=>re.test(i.name)))).sort((a,b)=>(b.dir-a.dir)||a.name.localeCompare(b.name));sel.clear();L.innerHTML='';
  if(cur.split('/').length>4){const u=document.createElement('div');u.className='lxit';u.textContent='⬆ ..';u.ondblclick=u.onclick=()=>{cur=cur.split('/').slice(0,-1).join('/');draw()};L.appendChild(u)}
  if(!items.length)L.insertAdjacentHTML('beforeend',`<div class='msg' style='padding:8px'>This folder is empty.</div>`);
  items.forEach(i=>{const d=document.createElement('div');d.className='lxit';d.setAttribute('role','option');d.tabIndex=0;d.textContent=(i.dir?'📁 ':'📄 ')+i.name;
   d.onclick=e=>{if(i.dir&&!(e.detail==1&&r.directory)){cur+='/'+i.name;draw();return}if(i.dir)return;if(save){nm.value=i.name;return}if(!(r.multiple&&e.ctrlKey))sel.clear(),L.querySelectorAll('.on').forEach(x=>x.classList.remove('on'));sel.add(i.name);d.classList.add('on')};
   d.ondblclick=()=>{if(!i.dir&&!save&&!r.directory)end([uri(cur+'/'+i.name)])};L.appendChild(d)})}
 o.querySelectorAll('.lxpl button').forEach(b=>b.onclick=()=>{cur='/home/rudolf/'+b.dataset.f;draw()});q('.al')&&(q('.al').onchange=e=>{all=e.target.checked;draw()});
 q('.c').onclick=()=>end(null);o.addEventListener('keydown',e=>{e.stopPropagation();if(e.key=='Escape')end(null)});
 q('.ok').onclick=async()=>{if(r.directory)return end([uri(cur)]);if(!save){if(!sel.size)return;return end([...sel].map(n=>uri(cur+'/'+n)))}
  const n=(nm.value||'').trim();if(!n||/[\/\\]/.test(n))return nm.focus();let exists=false;try{exists=(await lxApi('fs/list?path='+encodeURIComponent(cur))).items.some(i=>i.name==n)}catch(e){}
  if(exists&&!confirm(n+' already exists. Replace it?'))return;end([uri(cur+'/'+n)])};
 draw();(nm||q('.ok')).focus()})}
/* ---------- App Store › Linux apps ---------- */
const LX_CATS=[['','All'],['Office','Office'],['Graphics','Graphics'],['AudioVideo','Audio & video'],['Game','Games'],['Development','Programming'],['Education','Education'],['Network','Internet'],['Utility','Tools'],['Science','Science'],['System','System']];
const LX_PICKS=['org.libreoffice.LibreOffice','org.gimp.GIMP','org.inkscape.Inkscape','org.videolan.VLC','org.mozilla.firefox','com.visualstudio.code','org.kde.krita','org.audacityteam.Audacity','com.obsproject.Studio','org.blender.Blender'];
function lxPerms(p){const out=[];if(!p)return out;const has=(k,v)=>(p[k]||'').split(';').includes(v);
 if(has('shared','network'))out.push(['Uses the internet',0]);if(has('sockets','pulseaudio'))out.push(['Plays and records sound',0]);if(has('sockets','cups'))out.push(['Prints',0]);
 if(has('devices','all'))out.push(['Uses all devices (USB, cameras, game controllers)',1]);else if(has('devices','dri'))out.push(['Uses the graphics card',0]);
 (p.filesystems||'').split(';').filter(Boolean).forEach(f=>{const ro=f.endsWith(':ro'),b=f.replace(/:(ro|rw|create)$/,'');const t={host:'All files on this computer','host-os':'System files (read)','host-etc':'System settings files (read)',home:'Everything in your home folder',
  'xdg-documents':'Your Documents','xdg-download':'Your Downloads','xdg-pictures':'Your Pictures','xdg-music':'Your Music','xdg-videos':'Your Videos','xdg-desktop':'Your Desktop folder'}[b]||('The folder '+b);
  if(!b.startsWith('xdg-run')&&!b.startsWith('xdg-config')&&!b.startsWith('xdg-cache')&&!b.startsWith('xdg-data'))out.push([(ro?'Reads ':'Reads and changes ')+t.charAt(0).toLowerCase()+t.slice(1),b=='host'||b=='home'?1:0])});
 return out}
async function flatStoreTab(B){B.innerHTML='';const msg=t=>{B.innerHTML=`<p class='msg' style='padding:6px'>${t}</p>`};
 if(!RD)return msg('Linux apps run when Rudolf OS is started from its disc, USB drive or hard disk.');
 let s;try{s=await lxApi('apps/status')}catch(e){return msg(lxEsc(e.message))}
 if(!s.flatpak)return msg('This Rudolf OS build cannot run Linux apps.');
 if(!s.persistent)return msg('Linux apps need Rudolf OS installed on this computer, or a USB drive with a RUDOLFDATA partition (Settings › Update and recovery › Install): they need somewhere to be kept.');
 let cat;try{cat=(await lxApi('apps/catalog')).apps||[]}catch(e){cat=null}
 if(!cat){B.innerHTML=`<div class='lxempty'><p>Thousands of Linux desktop apps, such as LibreOffice, GIMP and VLC, are available from Flathub. First Rudolf OS downloads Flathub's app list (about 10 MB).</p><button class='pri lxdl'>Download the app list</button><pre class='jl' hidden></pre></div>`;
  B.querySelector('.lxdl').onclick=async e=>{e.target.disabled=true;const pre=B.querySelector('pre');pre.hidden=false;try{await lxApi('apps/refresh')}catch(er){pre.textContent=er.message;e.target.disabled=false;return}
   const j=await lxJobWait(l=>{pre.textContent+=l+'\n'});if(j.status==0)flatStoreTab(B);else{pre.textContent+='\nThat did not work. Is the network cable connected?';e.target.disabled=false}};return}
 LX.cat=cat;await lxInstalled();const inst=new Set((LX.inst||[]).map(a=>a.id));
 B.innerHTML=`<div class='lxbar'><input type='search' class='lxq' placeholder='Search Linux apps' aria-label='Search Linux apps'><select class='lxc' aria-label='Category'>${LX_CATS.map(([k,n])=>`<option value='${k}'>${n}</option>`).join('')}</select><label class='ck'><input type='checkbox' class='lxv' checked> Verified only</label></div><div class='lxlist'></div><p class='msg lxfoot'></p>`;
 const L=B.querySelector('.lxlist'),Q=B.querySelector('.lxq'),C=B.querySelector('.lxc'),V=B.querySelector('.lxv');let limit=60;
 const io=new IntersectionObserver(es=>es.forEach(en=>{if(en.isIntersecting){const el=en.target;io.unobserve(el);lxImg(el,el.dataset.id)}}),{root:B});
 function draw(){const q=Q.value.trim().toLowerCase(),c=C.value;let l=cat.filter(a=>(!V.checked||a.verified||inst.has(a.id))&&(!c||(a.cats||[]).includes(c))&&(!q||(a.name+' '+a.summary+' '+a.id+' '+(a.dev||'')).toLowerCase().includes(q)));
  const pick=a=>{const i=LX_PICKS.indexOf(a.id);return i<0?99:i};l.sort((a,b)=>(q||c?0:pick(a)-pick(b))||(b.verified-a.verified)||a.name.localeCompare(b.name));
  L.innerHTML='';l.slice(0,limit).forEach(a=>{const d=document.createElement('div');d.className='stc lxrow';d.tabIndex=0;
   d.innerHTML=`<div class='sti lxi' data-id='${lxEsc(a.id)}'>🐧</div><div style='flex:1;min-width:0'><b>${lxEsc(a.name)}</b>${a.verified?` <span class='lxver' title='Verified by Flathub: published by the app's own developer'>✔ verified</span>`:''}${inst.has(a.id)?` <span class='natbadge'>Installed</span>`:''}<div class='std'>${lxEsc(a.summary)}</div></div>`;
   d.onclick=d.onkeydown=e=>{if(e.type=='keydown'&&e.key!='Enter')return;flatAppPage(B,a,()=>flatStoreTab(B))};L.appendChild(d);io.observe(d.querySelector('.lxi'))});
  B.querySelector('.lxfoot').textContent=l.length>limit?`Showing ${limit} of ${l.length}. Search to find more.`:`${l.length} app${l.length==1?'':'s'}. From Flathub; apps run in a sandbox.`;
  if(l.length>limit){const m=document.createElement('button');m.textContent='Show more';m.onclick=()=>{limit+=60;draw()};L.appendChild(m)}}
 let t;Q.oninput=()=>{clearTimeout(t);t=setTimeout(()=>{limit=60;draw()},200)};C.onchange=V.onchange=()=>{limit=60;draw()};draw();Q.focus()}
async function flatAppPage(B,a,back){B.innerHTML=`<button class='lxback'>‹ All Linux apps</button><div class='lxhead'><div class='sti lxbig'>🐧</div><div><h3>${lxEsc(a.name)}</h3><div class='msg'>${lxEsc(a.dev||'')}${a.verified?' · <span class=lxver>✔ verified</span>':' · not verified'}</div><div class='lxact'></div></div></div><div class='lxshots'></div><p class='lxsum'><b>${lxEsc(a.summary)}</b></p><div class='lxdesc msg'>Loading details…</div><div class='lxfacts msg'></div>`;
 B.querySelector('.lxback').onclick=back;lxImg(B.querySelector('.lxbig'),a.id);const act=B.querySelector('.lxact');
 const btn=(l,f,pri)=>{const b=document.createElement('button');b.textContent=l;if(pri)b.className='pri';b.onclick=f;act.appendChild(b);return b};
 let info=null;
 async function buttons(){act.innerHTML='';await lxInstalled(true);const ia=(LX.inst||[]).find(x=>x.id==a.id);
  if(ia){btn('Open',()=>{const x=lxLaunchers().find(l=>l.app==a.id);if(x)lxRun(x)},1);btn('Uninstall',async()=>{if(!await elevate('Uninstall '+a.name))return;job('uninstall')});act.insertAdjacentHTML('beforeend',`<span class='msg'> Installed${ia.version?', version '+lxEsc(ia.version):''}${ia.size?' · '+lxEsc(ia.size):''}</span>`)}
  else btn('Get',async()=>{if(await flatConsent(a,info)&&await elevate('Install '+a.name+' from Flathub'))job('install')},1)}
 async function job(what){act.innerHTML=`<pre class='jl'></pre>`;const pre=act.querySelector('pre');try{await lxApi('apps/'+what+'?id='+encodeURIComponent(a.id))}catch(e){pre.textContent=e.message;setTimeout(buttons,3000);return}
  const j=await lxJobWait(l=>{pre.textContent+=l+'\n';pre.scrollTop=pre.scrollHeight});await lxInstalled(true);
  notify(a.name,j.status==0?(what=='install'?'Installed. It is in the Start menu.':'Uninstalled.'):'That did not finish: '+((j.log||'').trim().split('\n').pop()||'error'),{kind:j.status==0?'info':'error',action:j.status==0&&what=='install'?()=>{const x=lxLaunchers().find(l=>l.app==a.id);x&&lxRun(x)}:undefined});buttons()}
 buttons();
 try{info=await lxApi('apps/info?id='+encodeURIComponent(a.id))}catch(e){B.querySelector('.lxdesc').textContent=e.message;return}
 if(!B.isConnected)return;B.querySelector('.lxdesc').textContent=info.description||'';
 const sh=B.querySelector('.lxshots');(info.screenshots||[]).slice(0,4).forEach(u=>{const i=new Image();i.loading='lazy';i.alt='Screenshot of '+a.name;i.src=u;i.onerror=()=>i.remove();sh.appendChild(i)});
 const facts=[info.version&&'Version '+lxEsc(info.version),info.license&&'Licence '+lxEsc(info.license),info.download&&'Download '+lxEsc(info.download)+(info.runtimeInstalled===false&&info.runtime?' plus the '+lxEsc(info.runtime.split('/')[0])+' runtime if not already here':''),info.installedSize&&'Uses '+lxEsc(info.installedSize)+' on disk',info.homepage&&`<a href='#' class='lxhp'>Website</a>`].filter(Boolean);
 B.querySelector('.lxfacts').innerHTML=facts.join(' · ')+(lxPerms(info.permissions).length?`<h4 class='sh4'>This app can</h4><ul class='lxperm'>${lxPerms(info.permissions).map(([t,w])=>`<li${w?' class=warn':''}>${lxEsc(t)}</li>`).join('')}</ul>`:'');
 const hp=B.querySelector('.lxhp');hp&&(hp.onclick=e=>{e.preventDefault();browser(info.homepage)})}
function flatConsent(a,info){return new Promise(res=>{const o=document.createElement('div');o.className='rmodal';const per=lxPerms(info&&info.permissions);
 o.innerHTML=`<div class='rmbox' role='dialog' aria-label='Get app'><h3>Get ${lxEsc(a.name)}?</h3><p class='msg'>From Flathub${a.verified?', published by its developer (verified)':', <b>not verified</b>: published by someone other than the app\'s developer'}. ${info&&info.download?'Download about '+lxEsc(info.download)+(info.runtimeInstalled===false?' plus shared parts it needs':'')+'.':''}</p>
 ${per.length?`<p><b>It will be able to:</b></p><ul class='lxperm'>${per.map(([t,w])=>`<li${w?' class=warn':''}>${lxEsc(t)}</li>`).join('')}</ul>`:''}<p class='msg'>Linux apps run in a sandbox; files you want to use with them go in Documents, Pictures or Downloads.</p><div class='rmrow'><button class='c'>Cancel</button><button class='ok pri'>Get</button></div></div>`;
 document.body.appendChild(o);const end=v=>{o.remove();res(v)};o.querySelector('.c').onclick=()=>end(false);o.querySelector('.ok').onclick=()=>end(true);o.querySelector('.ok').focus()})}
/* App Store › Installed: Linux apps, sizes, updates and settings */
async function flatInstalledCards(B){if(!RD)return;let s;try{s=await lxApi('apps/status')}catch(e){return}if(!s.flatpak||!s.persistent)return;const l=await lxInstalled(true);
 const box=document.createElement('div');box.className='lxinst';B.appendChild(box);
 box.innerHTML=`<h4 class='sh4'>Linux apps</h4>${l.length?'':`<p class='msg'>None yet. Find them under Linux apps.</p>`}<div class='lxil'></div><div class='rmrow' style='justify-content:flex-start'><button class='lxup'>Update Linux apps now</button></div>
 <label class='ck'><input type='checkbox' class='lxau'${s.auto?' checked':''}> Update Linux apps automatically (once a day)</label><br><label class='ck'><input type='checkbox' class='lxun'${s.unverified?' checked':''}> Allow apps not verified by Flathub</label><pre class='jl' hidden></pre>`;
 const L=box.querySelector('.lxil');l.forEach(a=>{const d=document.createElement('div');d.className='stc';d.innerHTML=`<div class='sti'>🐧</div><div style='flex:1;min-width:0'><b>${lxEsc(a.name)}</b> <span class='natbadge'>Linux app</span><div class='std'>${lxEsc([a.version&&'Version '+a.version,a.size].filter(Boolean).join(' · '))}</div></div><div class='stbtn'><button class='pri o'>Open</button><button class='u'>Uninstall</button></div>`;
  lxImg(d.querySelector('.sti'),a.id);d.querySelector('.o').onclick=()=>{const x=lxLaunchers().find(z=>z.app==a.id);x&&lxRun(x)};
  d.querySelector('.u').onclick=async()=>{if(!await elevate('Uninstall '+a.name))return;const pre=box.querySelector('pre');pre.hidden=false;pre.textContent='';try{await lxApi('apps/uninstall?id='+encodeURIComponent(a.id))}catch(e){pre.textContent=e.message;return}await lxJobWait(t=>pre.textContent+=t+'\n');await lxInstalled(true);box.remove();flatInstalledCards(B)};L.appendChild(d)});
 box.querySelector('.lxup').onclick=async e=>{const pre=box.querySelector('pre');pre.hidden=false;pre.textContent='';e.target.disabled=true;try{await lxApi('apps/update')}catch(er){pre.textContent=er.message;e.target.disabled=false;return}const j=await lxJobWait(t=>{pre.textContent+=t+'\n'});e.target.disabled=false;svj('bd95lxupd',Date.now());notify('Linux apps',j.status==0?'Up to date.':'Updating did not finish.')};
 const set=async(k,v,el)=>{if(!await elevate(k=='auto'?'Change automatic updates of Linux apps':'Change which Linux apps can be installed')){el.checked=!v;return}try{await lxApi('apps/settings?'+k+'='+(v?1:0))}catch(e){el.checked=!v;notify('Linux apps',e.message,{kind:'error'})}};
 box.querySelector('.lxau').onchange=e=>set('auto',e.target.checked,e.target);box.querySelector('.lxun').onchange=e=>set('unverified',e.target.checked,e.target)}
/* daily automatic updates of Linux apps (after the security check has had its turn) */
async function lxAutoUpdate(){if(!RD||!USER||window.LOCKED)return;if(Date.now()-ld('bd95lxupd',0)<20*3600e3)return;let s;try{s=await lxApi('apps/status')}catch(e){return}
 if(!s.flatpak||!s.persistent||!s.auto)return;if(!(await lxInstalled()).length)return;try{const j=await rd('job');if(j.running)return}catch(e){return}
 try{await lxApi('apps/update')}catch(e){return}svj('bd95lxupd',Date.now());const j=await lxJobWait();if(j.status==0&&/Updating|Installing/i.test(j.log||''))notify('Linux apps updated','Your Linux apps have the latest versions.')}
if(RD){lxRegister();setTimeout(()=>lxInstalled(true),4000);setInterval(lxWinPoll,1500);setInterval(lxPortalPoll,1000);setTimeout(lxSync,9000);setInterval(()=>{if(LX.wins.some(w=>!w.web))lxSync()},20000);setTimeout(lxAutoUpdate,15*60e3);setInterval(lxAutoUpdate,3*3600e3)}
