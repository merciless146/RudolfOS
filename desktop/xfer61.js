/* ===== Rudolf OS 1.6.1 (the owner, 2026-11-20, with a photo of two File Explorer windows side by side: Downloads and the
   Games (Emu library) drive): "Users should be able to move files by click-dragging items from one file explorer window to a
   different window showing a different location within it. They should also be able to use the mouse to click-drag a
   selection box over items ... A pop-up window should appear that shows file transfer progress." (docs/plan-1.6.1.md Part 22)
   - xfRun: every transfer between C: (the account's files) and rudolfd's places (a drive, the Games folder, a folder of this
     computer), and between two of those. A move across stores is a copy that is checked (sizes; and SHA-256 where rudolfd
     has it: fs/hash, home/hash), then the originals are removed only once everything arrived; a move that is cancelled or
     fails leaves the originals untouched (what was copied of it is removed again) and says what was and was not moved.
     It keeps remoteImport's limits for what C: takes in (5,000 files, the storage left, files over 256 MB left out, the
     antivirus and the file-type rules) and the drives' (a read-only folder takes nothing, its files are copied, not moved).
   - xfJob: the progress window ("Moving 3 items", the file, n of N, bytes, a bar, the time left, Cancel): across stores at
     once, within C: only when it takes more than half a second; it closes itself a moment after a success and stays with
     the details after a failure. Each transfer has its own.
   - xfMarquee: the selection box over a file list, with the mouse (touch and keys unchanged).
   - On the native desktop each window is a page of its own (oneapp59.js): the clipboard (Cut, Copy) is kept where the other
     windows read it, and a page that changed a drive's folder tells the others, which draw it again. ===== */
const XF_PIECE=8*1048576,XF_C_MAX=256*1048576,XF_C_FILES=5000;
const xfB=b=>typeof fmtB=='function'?fmtB(b):b+' B';
const xfLeft=s=>s<60?'About '+Math.max(1,Math.round(s))+' s left':s<3600?'About '+Math.round(s/60)+' min left':'About '+(s/3600).toFixed(1)+' hours left';
/* ---------- the progress window ---------- */
function xfJob(kind,n,opt){opt=opt||{};const j={kind,n,cancel:false,end:false,i:0,N:0,bytes:0,total:0,name:'',t0:Date.now(),W:null,ac:new AbortController(),onCancel:null};
 const word=kind=='move'?'Moving':kind=='delete'?'Deleting':'Copying';
 j.title=`${word} ${n} item${n==1?'':'s'}`;
 j.show=()=>{if(j.W||typeof win!='function')return;const W=j.W=win(j.title,'📦',400,236);W.app='xfer';W.xfer=j;
  /* at the bottom right of the screen (the desktop, or this window's page on the native desktop), several stacked upwards */
  try{const zf=parseFloat(document.documentElement.style.zoom)||1,dk=document.getElementById('desk'),bw=(dk&&dk.clientWidth)||innerWidth/zf,bh=(dk&&dk.clientHeight)||innerHeight/zf,
   k=wins.filter(o=>o.xfer&&o!==W&&o.el.isConnected).length;W.el.style.left=Math.max(0,bw-412)+'px';W.el.style.top=Math.max(0,bh-248-k*244)+'px'}catch(e){}
  W.body.classList.add('xfw');W.body.style.cssText='padding:12px 14px;font-size:13px;display:flex;flex-direction:column;gap:5px;background:var(--wbg,#fff)';
  W.body.innerHTML=`<b class='xft' role='status' aria-live='polite'></b><div class='xfr' style='color:#6b7280;font-size:12px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis'></div><div class='xfn' style='white-space:nowrap;overflow:hidden;text-overflow:ellipsis'></div><progress class='xfbar' max='1000' value='0' style='width:100%'></progress><div style='display:flex;gap:8px'><span class='xfc'></span><span class='xfb' style='flex:1;text-align:center'></span><span class='xfe'></span></div><pre class='xfd' hidden style='margin:0;max-height:90px;overflow:auto;white-space:pre-wrap;font:11px monospace'></pre><div style='text-align:right;margin-top:auto'><button class='xfx'>Cancel</button></div>`;
  W.body.querySelector('.xft').textContent=j.title;W.body.querySelector('.xfr').textContent=opt.route||'';
  W.body.querySelector('.xfx').onclick=()=>{if(j.end)return W.close();j.cancel=true;try{j.ac.abort()}catch(e){}W.body.querySelector('.xfx').disabled=true;W.body.querySelector('.xfn').textContent='Stopping…';j.onCancel&&j.onCancel()};
  W.onclose=()=>{clearInterval(j.ti);if(!j.end){j.cancel=true;try{j.ac.abort()}catch(e){}}};j.ti=setInterval(()=>{if(j.end||!W.el.isConnected)clearInterval(j.ti);else j.draw()},500);j.draw()};
 let dt=0;j.draw=()=>{const W=j.W;if(!W||!W.el.isConnected)return;const q=s=>W.body.querySelector(s);
  if(!j.end){q('.xfn').textContent=j.cancel?'Stopping…':j.name;q('.xfc').textContent=j.N?`${Math.min(j.i+1,j.N).toLocaleString()} of ${j.N.toLocaleString()}`:'Counting…';
   q('.xfb').textContent=j.total?`${xfB(j.bytes)} of ${xfB(j.total)}`:'';q('.xfbar').value=j.total?Math.round(j.bytes/j.total*1000):j.N?Math.round(j.i/j.N*1000):0;
   const el=(Date.now()-j.t0)/1000;q('.xfe').textContent=el>1.5&&j.bytes>0&&j.total>j.bytes?xfLeft((j.total-j.bytes)/(j.bytes/el)):''}};
 j.set=(name,i,N)=>{if(name!=null)j.name=name;if(i!=null)j.i=i;if(N!=null)j.N=N;const t=Date.now();if(t-dt>80){dt=t;j.draw()}};
 j.finish=(ok,msg,details)=>{j.end=true;clearTimeout(j.tm);clearInterval(j.ti);if(!j.W){if(ok)return;j.show()}const W=j.W;if(!W||!W.el.isConnected)return;const q=s=>W.body.querySelector(s);
  q('.xft').textContent=msg||j.title;q('.xfn').textContent='';q('.xfe').textContent='';if(ok){q('.xfbar').value=1000;q('.xfc').textContent=j.N?`${j.N.toLocaleString()} of ${j.N.toLocaleString()}`:'';q('.xfb').textContent=j.total?xfB(j.total):''}
  if(details&&details.length){const d=q('.xfd');d.hidden=false;d.textContent=details.join('\n')}
  const x=q('.xfx');x.disabled=false;x.textContent='Close';W.setTitle(ok?j.title.replace(/^\w+/,w=>({Moving:'Moved',Copying:'Copied',Deleting:'Deleted'})[w]||w):'Not done: '+j.title.toLowerCase());
  if(ok)setTimeout(()=>{if(W.el.isConnected)W.close()},1500)};
 if(opt.now)j.show();else j.tm=setTimeout(()=>{if(!j.end)j.show()},500);
 return j}
/* ---------- rudolfd's places ---------- */
const xfRJoin=(a,b)=>a.replace(/\/+$/,'')+'/'+b;
const xfRParent=p=>p.replace(/\/[^/]+$/,'')||'/';
async function xfRList(path){try{return (await rd('fs/list?path='+encodeURIComponent(path))).items||[]}catch(e){return null}}
async function xfRHash(path){try{return (await rd('fs/hash?path='+encodeURIComponent(path))).sha256||null}catch(e){return null}}
async function xfSha(buf){try{const h=await crypto.subtle.digest('SHA-256',buf);return [...new Uint8Array(h)].map(x=>x.toString(16).padStart(2,'0')).join('')}catch(e){return null}}
/* a name not yet in the folder (any case, as FAT and NTFS see names): "name (2).ext" */
function xfRUniq(taken,name){const low=new Set([...taken].map(x=>String(x).toLowerCase()));if(!low.has(name.toLowerCase()))return name;
 const m=/^(.*?)(\.[^.]*)?$/.exec(name);for(let i=2;;i++){const c=m[1]+' ('+i+')'+(m[2]||'');if(!low.has(c.toLowerCase()))return c}}
/* a C: name as a file on a drive: what Linux and the drives read (lower case, as remoteUpload wrote them) */
const xfRName=n=>String(n).toLowerCase().replace(/[\\\/:*?"<>|\u0000-\u001f]/g,'_').replace(/^\.+/,'_')||'file';
const xfCName=n=>String(n).toUpperCase().replace(/[\\\/:*?"<>|]/g,'_').slice(0,60)||'FILE';
/* the bytes of a C: value (text, data: URL, a game kept in the browser's storage); null: a file kept on the disk */
async function xfCBytes(v){if(typeof isDlRef=='function'&&isDlRef(v))return null;if(typeof isStub=='function'&&isStub(v))return await mmBytes(v);
 return new Uint8Array(await dataToBlob(v).arrayBuffer())}
/* reading through rudolfd with a signal; the bytes counted as they come (into C:, so 256 MB at most) */
async function xfFetch(url,j,onBytes){const r=await fetch(url,{headers:RD&&RD.token?{'X-Rudolf-Token':RD.token}:{},signal:j.ac.signal});
 if(!r.ok){let m='HTTP '+r.status;try{const x=await r.json();m=x.error||m}catch(e){}throw new Error(m)}
 if(!r.body||!r.body.getReader){const b=await r.blob();onBytes(b.size);return b}
 const rd2=r.body.getReader(),parts=[];let type=r.headers.get('content-type')||'';for(;;){const {done,value}=await rd2.read();if(done)break;parts.push(value);onBytes(value.length)}return new Blob(parts,{type})}
/* a file written onto a drive: in one go up to 8 MB, else in pieces (fs/write part=, final=1) */
async function xfRWrite(path,src,size,mtime,j,onBytes){const q='fs/write?path='+encodeURIComponent(path)+(mtime>0?'&mtime='+Math.round(mtime):'');
 if(src instanceof Uint8Array&&src.length<=XF_PIECE){const r=await rd(q,{method:'POST',body:src});onBytes(src.length);return r.size!=null?r.size:src.length}
 let off=0,last=null;const put=async(b,fin)=>{if(j.cancel)throw new Error('stopped');last=await rd(q+'&part='+off+(fin?'&final=1':''),{method:'POST',body:b});off+=b.length;onBytes(b.length)};
 if(src instanceof Uint8Array){for(let i=0;i<src.length||i==0;i+=XF_PIECE){const b=src.subarray(i,i+XF_PIECE);await put(b,i+XF_PIECE>=src.length);if(!src.length)break}}
 else{/* a stream (a file kept on the disk, read through its /dlf/ link) */const r=await fetch(src,{signal:j.ac.signal});if(!r.ok)throw new Error('the file kept on the disk could not be read ('+r.status+')');
  const rr=r.body.getReader();let buf=[],bl=0;for(;;){const {done,value}=await rr.read();if(value){buf.push(value);bl+=value.length}
   if(bl>=XF_PIECE||done){const b=new Uint8Array(bl);let o=0;buf.forEach(x=>{b.set(x,o);o+=x.length});buf=[];bl=0;await put(b,done)}if(done)break}}
 return last&&last.size!=null?last.size:off}
async function xfRDeleteTree(path,isDir){if(isDir){let t=null;try{t=await rd('fs/tree?path='+encodeURIComponent(path))}catch(e){}
  const it=(t&&t.items)||[];for(const f of it.filter(x=>!x.dir))await rdPost('fs/delete?path='+encodeURIComponent(xfRJoin(path,f.p)));
  for(const d of it.filter(x=>x.dir).sort((a,b)=>b.p.length-a.p.length))await rdPost('fs/delete?path='+encodeURIComponent(xfRJoin(path,d.p)))}
 await rdPost('fs/delete?path='+encodeURIComponent(path))}
/* ---------- the native desktop: the other windows' pages ---------- */
const XFB=typeof BroadcastChannel=='function'?new BroadcastChannel('rudolf-xfer'):null;
if(XFB)XFB.onmessage=e=>{const m=e.data||{};if(m.t=='changed')try{EXPL.forEach(f=>f())}catch(x){}};
function xfChanged(){try{EXPL.forEach(f=>f())}catch(e){}if(typeof ONEAPP!='undefined'&&ONEAPP&&XFB)try{XFB.postMessage({t:'changed'})}catch(e){}}
/* Cut and Copy: one clipboard for every window (the pages of the native desktop share their storage) */
const xfFcK=()=>'bd95fc:'+(typeof uid=='function'&&USER?uid():'');
function fcPut(){if(!(typeof ONEAPP!='undefined'&&ONEAPP))return;try{FC.t=Date.now();localStorage.setItem(xfFcK(),JSON.stringify({op:FC.op,items:FC.items||[],r:FC.r||null,t:FC.t}))}catch(e){}
 /* (1.6.1 Part 28) the native shell's desktop shows Paste while there is something to paste */
 try{if(typeof oaNative=='function'&&oaNative())oaPost({op:'fc',n:String(FC.op?(FC.r&&FC.r.items?FC.r.items.length:(FC.items||[]).length):0)})}catch(e){}}
function fcTake(){if(!(typeof ONEAPP!='undefined'&&ONEAPP))return;try{const o=JSON.parse(localStorage.getItem(xfFcK())||'null');if(o&&(o.t||0)>(FC.t||0)){FC.op=o.op;FC.items=o.items||[];FC.r=o.r||null;FC.t=o.t}}catch(e){}}
/* ---------- the transfer ----------
   src: {s:'C', p:[C: path]} or {s:'R', base:'/folder', name, dir, size, mtime}, all from one place (srcR: that place, for R);
   dest: {s:'C', p:[C: path]} or {s:'R', path:'/folder', remote}; mode 'copy' or 'cut'.
   cx: the File Explorer's {say, needWrite, label}. Returns {out: the C: paths made at the top, done, failed}. */
async function xfRun(srcs,dest,mode,cx){cx=cx||{};const say=cx.say||(()=>{}),move=mode=='cut';
 const srcR=cx.srcR||null,roSrc=!!(srcR&&srcR.ro);const res={out:[],done:0,failed:[]};
 if(!srcs.length)return res;
 if(dest.s=='R'&&dest.remote&&dest.remote.ro){say((dest.remote.label||dest.path)+' is read-only: nothing can be put there.',true);return res}
 const dLabel=cx.label||(dest.s=='C'?(dest.p.at(-1)||'C:'):(dest.path.split('/').pop()||dest.path));
 /* the same place: nothing to do */
 srcs=srcs.filter(s=>s.s=='C'?!(dest.s=='C'&&pkey(s.p.slice(0,-1))==pkey(dest.p))&&!(dest.s=='C'&&pkey(s.p)==pkey(dest.p)):!(dest.s=='R'&&(s.base==dest.path||xfRJoin(s.base,s.name)==dest.path)));
 if(!srcs.length)return res;
 for(const s of srcs){if(s.dir&&s.s=='R'&&dest.s=='R'&&(dest.path+'/').startsWith(xfRJoin(s.base,s.name)+'/'))return say('A folder cannot be moved or copied into itself.',true),res;
  if(s.s=='C'&&dest.s=='C'&&isDir(node(s.p))&&under(s.p,dest.p))return say('A folder cannot be moved or copied into itself.',true),res}
 if(dest.s=='C'){const dd=node(dest.p);if(!isDir(dd))return say('Destination not found.',true),res;if(cx.needWrite&&!await cx.needWrite(dest.p))return res}
 if(move&&srcs.some(s=>s.s=='C'&&(isProt(s.p)||!fsCanWrite(s.p))))return say('System items cannot be moved.',true),res;
 const kindMove=move&&!roSrc;
 const j=xfJob(kindMove?'move':'copy',srcs.length,{now:true,route:`${cx.fromLabel||''}${cx.fromLabel?' → ':'To '}${dLabel}`});
 try{cx.onJob&&cx.onJob(j)}catch(e){}
 const details=[],skip=[];
 /* 1. what there is: folders and files below each item (names below the destination; the top names made unique) */
 const jobs=[];let taken;
 try{if(dest.s=='R'){const l=await xfRList(dest.path);if(!l)throw new Error('could not read '+dLabel);taken=new Set(l.map(x=>x.name))}else taken=new Set(Object.keys(node(dest.p)));
  for(const s of srcs){if(j.cancel)break;
   const top=dest.s=='R'?xfRUniq(taken,s.s=='C'?xfRName(s.p.at(-1)):s.name):uniq(Object.fromEntries([...taken].map(k=>[k,1])),s.s=='C'?s.p.at(-1):xfCName(s.name));taken.add(top);
   const item={s,top,files:[],dirs:[]};jobs.push(item);
   if(s.s=='C'){const v=node(s.p);if(v==null)continue;
    if(isDir(v)){item.dir=true;const walk=(o,parts,cp)=>{for(const k in o){const x=o[k],np=[...parts,dest.s=='R'?xfRName(k):k];if(isDir(x)){item.dirs.push(np);walk(x,np,[...cp,k])}else item.files.push({cp:[...cp,k],parts:np,v:x,size:Math.max(0,sizeOf(x)),mtime:(fileMeta([...cp,k])||{}).m||0})}};walk(v,[],s.p)}
    else item.files.push({cp:s.p,parts:[],v,size:Math.max(0,sizeOf(v)),mtime:(fileMeta(s.p)||{}).m||0})}
   else{const base=xfRJoin(s.base,s.name);if(s.dir){item.dir=true;j.set('Counting what is in '+s.name+'…');say('Counting what is in '+s.name+'…');
     const tr=await rd('fs/tree?path='+encodeURIComponent(base));
     for(const e of tr.items||[]){const ps=e.p.split('/'),np=dest.s=='C'?ps.map(xfCName):ps;if(e.dir)item.dirs.push(np);else item.files.push({src:xfRJoin(base,e.p),rel:s.name+'/'+e.p,parts:np,size:e.size,mtime:e.mtime})}}
    else item.files.push({src:base,rel:s.name,parts:[],size:s.size||0,mtime:s.mtime||0})}}}
 catch(e){j.finish(false,'Could not read the drive',[e.message]);say('Could not read the drive: '+e.message,true);return res}
 /* 2. the limits of where it goes */
 let all=jobs.flatMap(x=>x.files);
 if(dest.s=='C'){const big=all.filter(f=>f.size>XF_C_MAX);big.forEach(f=>{f.skip=1;skip.push((f.rel||f.parts.join('/'))+' (over 256 MB)')});all=all.filter(f=>!f.skip);
  if(all.length>XF_C_FILES){const m=`That is ${all.length} files: copy at most 5,000 at a time (a folder further down, say).`;j.finish(false,'Too many files',[m]);say(m,true);return res}
  const total=all.reduce((a,f)=>a+f.size,0);if(srcs.some(s=>s.dir)||total>16*1048576){const si=await storageInfo();if(si.quota&&total*1.4>(si.quota*0.9-si.used)){const m=`That is ${xfB(total)}: it does not fit in this computer's Rudolf OS storage (${xfB(Math.max(0,si.quota*0.9-si.used))} free). Copy a smaller folder, or keep it on the drive.`;j.finish(false,'It does not fit',[m]);say(m,true);return res}}}
 j.N=all.length;j.total=all.reduce((a,f)=>a+f.size,0);j.set(null,0);
 /* 3. on the same drive, a move is a rename (instant); onto another drive it is copied */
 const made=[];   /* for a move that stops: what to take away again ({s, p|path, dir}) */
 let stopped=false,fail=null,done=0;const t0=Date.now();
 const remoteMove=kindMove&&dest.s=='R';
 try{
  for(const it of jobs){if(j.cancel){stopped=true;break}const s=it.s;
   if(remoteMove&&s.s=='R'){try{await rdPost('fs/move?from='+encodeURIComponent(xfRJoin(s.base,s.name))+'&to='+encodeURIComponent(xfRJoin(dest.path,it.top)));it.renamed=true;
     for(const f of it.files){j.bytes+=f.size;j.set(f.rel||it.top,++done-1)}continue}catch(e){if(!/another drive|409/.test(e.message))throw e}}
   /* folders first */
   if(dest.s=='C'){const D=node(dest.p);let root=D;if(it.dir){D[it.top]={};root=D[it.top];made.push({s:'C',p:[...dest.p,it.top]});res.out.push([...dest.p,it.top])}
    it.cdir=root;for(const parts of it.dirs){let o=root;for(const x of parts){if(!isDir(o[x]))o[x]={};o=o[x]}}}
   else{if(it.dir){await rdPost('fs/mkdir?path='+encodeURIComponent(xfRJoin(dest.path,it.top)));made.push({s:'R',path:xfRJoin(dest.path,it.top),dir:true})}
    for(const parts of it.dirs.slice().sort((a,b)=>a.length-b.length))await rdPost('fs/mkdir?path='+encodeURIComponent(xfRJoin(dest.path,[it.top,...parts].join('/'))))}
   for(const f of it.files){if(f.skip)continue;if(j.cancel){stopped=true;break}
    const label=f.rel||[it.top,...f.parts].join('/');j.set(f.parts.at(-1)||it.top,done);say(j.N>1?`${kindMove?'Moving':'Copying'} ${done+1} of ${j.N} files (${xfB(j.bytes)})…`:(kindMove?'Moving ':'Copying ')+(f.parts.at(-1)||it.top)+'…');
    const b0=j.bytes;const onB=n=>{j.bytes+=n;j.set()};
    try{
     if(dest.s=='C'){/* into C: (from a drive): read, checked by the antivirus, kept as C: keeps files */
      const blob=await xfFetch('/api/fs/read?path='+encodeURIComponent(f.src),j,onB);
      if(blob.size!==f.size)throw new Error('it arrived incomplete ('+xfB(blob.size)+' of '+xfB(f.size)+')');
      if(kindMove){const h=await xfRHash(f.src);if(h){const mine=await xfSha(await blob.arrayBuffer());if(mine&&mine!==h)throw new Error('it arrived changed (its SHA-256 differs)')}}
      let par=it.cdir;for(const x of f.parts.slice(0,-1))par=par[x];const nm=f.parts.length?uniq(par,f.parts.at(-1)):it.top;const e=ext(nm);
      const val=TEXTE.test(e)&&e!='SVG'&&blob.size<4*1048576?await blob.text():await blobToDataURL(new Blob([blob],{type:MIMEX[e]||blob.type||'application/octet-stream'}));
      if(window.__sys&&typeof secIngress=='function'){const g=secIngress(nm,val,'upload');if(!g.ok)throw new Error(g.msg)}
      if(window.__mm&&typeof mmIngress=='function'){const mi=await mmIngress(nm,val);if(mi)throw new Error(mi)}
      par[nm]=val;if(!it.dir){made.push({s:'C',p:[...dest.p,nm]});res.out.push([...dest.p,nm])}
      if(f.mtime>0&&typeof META=='object'){const P=[...dest.p,...(it.dir?[it.top,...f.parts.slice(0,-1),nm]:[nm])];META['/'+P.join('/')]={c:Date.now(),m:f.mtime,o:typeof uname=='function'?uname():''}}}
     else{const to=xfRJoin(dest.path,[it.top,...f.parts].join('/'));
      if(s.s=='R'){/* drive to drive: rudolfd copies it */const r=await rdPost('fs/copy?from='+encodeURIComponent(f.src)+'&to='+encodeURIComponent(to));if(!it.dir)made.push({s:'R',path:to});else made.push({s:'R',path:to});
       if(r.size!=null&&r.size!==f.size)throw new Error('it arrived incomplete');
       if(kindMove){const [a,b]=await Promise.all([xfRHash(f.src),xfRHash(to)]);if(a&&b&&a!==b)throw new Error('it arrived changed (its SHA-256 differs)')}onB(f.size)}
      else{/* from C: */let bytes=await xfCBytes(f.v),sz;
       if(bytes==null){/* a file kept on the disk: read through its link, written in pieces */const u=await dlLink(f.v);made.push({s:'R',path:to});sz=await xfRWrite(to,u,f.size,f.mtime,j,onB)}
       else{made.push({s:'R',path:to});sz=await xfRWrite(to,bytes,bytes.length,f.mtime,j,onB)}
       const want=bytes?bytes.length:f.size;if(sz!==want)throw new Error('it arrived incomplete ('+xfB(sz)+' of '+xfB(want)+')');
       if(kindMove){let a=null;if(bytes)a=await xfSha(bytes);else if(typeof hdRefRel=='function'&&hdRefRel(f.v)!=null){try{a=(await rd('home/hash?acct='+encodeURIComponent(HD.acct)+'&path='+encodeURIComponent(hdRefRel(f.v)))).sha256}catch(e){}}
        if(a){const b=await xfRHash(to);if(b&&a!==b)throw new Error('it arrived changed (its SHA-256 differs)')}}}}
     done++;res.done++}
    catch(e){if(j.cancel||e.name=='AbortError'){stopped=true;break}
     j.bytes=b0+f.size;const why=/permission|denied|403/i.test(e.message)?'could not be read':String(e.message).replace(/\.+$/,'');
     if(kindMove){fail={label,why};break}
     if(dest.s=='C'&&!/antivirus|not allowed|blocked|quarantin/i.test(why))skip.push(label+' (could not be read)');else skip.push(label+' ('+why+')');details.push(label+': '+why)}}
   if(stopped||fail)break}}
 catch(e){if(j.cancel||e.name=='AbortError')stopped=true;else fail={label:'',why:e.message}}
 /* 4. a move: the originals go only now that everything arrived; one that stopped takes away what it made */
 const total=all.length;let msg='',ok=true;
 if(kindMove&&(stopped||fail)){ok=false;
  for(const m of made.slice().reverse()){try{if(m.s=='C'){const d=node(m.p.slice(0,-1));if(d)delete d[m.p.at(-1)]}else await xfRDeleteTree(m.path,m.dir)}catch(e){}}
  for(const it of jobs)if(it.renamed){/* (renamed on the same drive: put back) */try{await rdPost('fs/move?from='+encodeURIComponent(xfRJoin(dest.path,it.top))+'&to='+encodeURIComponent(xfRJoin(it.s.base,it.s.name)))}catch(e){details.push(it.s.name+' stayed in '+dLabel+': it could not be put back ('+e.message+')')}}
  res.out=[];
  msg=stopped?`Stopped: nothing was moved. The originals are where they were${done?` (the ${done} file${done==1?'':'s'} already copied ${done==1?'was':'were'} taken away again)`:''}.`
   :`Not moved: ${fail.label?fail.label+' ':''}${fail.why}. Nothing was moved: the originals are where they were.`;
  details.unshift(stopped?'Not moved: '+jobs.map(x=>x.s.s=='C'?x.s.p.at(-1):x.s.name).join(', '):'Not moved: '+jobs.map(x=>x.s.s=='C'?x.s.p.at(-1):x.s.name).join(', ')+' ('+fail.why+')')}
 else if(kindMove){const gone=[],kept=[];
  for(const it of jobs){if(it.renamed){gone.push(it);continue}const s=it.s;if(it.files.some(f=>f.skip)){kept.push(s);continue}
   try{if(s.s=='C'){const d=dirOf(s.p);if(d)delete d[s.p.at(-1)];try{favMoved(s.p,dest.s=='C'?[...dest.p,it.top]:s.p)}catch(e){}}else await xfRDeleteTree(xfRJoin(s.base,s.name),!!s.dir);gone.push(it)}
   catch(e){kept.push(s);details.push((s.s=='C'?s.p.at(-1):s.name)+': copied, but the original could not be removed ('+e.message+')')}}
  msg=`Moved ${gone.length} item${gone.length==1?'':'s'}${j.total?' ('+xfB(j.total)+')':''} to ${dLabel}.`;
  if(kept.length){msg+=` ${kept.length} stayed where they were${skip.length?' (files over 256 MB do not go into Rudolf OS: leave them on the drive)':''}.`;ok=false}}
 else{msg=stopped?`Stopped: ${done} of ${total} files copied.`:total>1?`Copied ${done} files (${xfB(j.bytes)}) into ${dLabel}.`:done?`Copied ${(jobs[0]&&jobs[0].top)||''} into ${dLabel}.`:'';
  if(roSrc&&move)msg=(msg?msg+' ':'')+`${srcR.label||'That folder'} is read-only, so the files were copied, not moved.`;if(stopped)ok=false}
 if(skip.length){msg=(msg?msg+' ':'')+`Left out ${skip.length}: ${skip.slice(0,3).join(', ')}${skip.length>3?'…':''}`;ok=false;details.push(...skip.map(x=>'Left out: '+x))}
 if(fail&&!kindMove)ok=false;
 if(dest.s=='C'||jobs.some(x=>x.s.s=='C'))fsSave();
 if(move&&FC.op=='cut'){FC.op=null;FC.items=[];FC.r=null;fcPut()}
 res.msg=msg;res.ok=ok&&!stopped;res.stopped=stopped;
 try{cx.result&&cx.result(res)}catch(e){}
 xfChanged();j.finish(res.ok,msg,details);if(msg)say(msg,!res.ok);
 if(res.ok&&typeof sfx=='function')try{sfx('notify')}catch(e){}
 if(Date.now()-t0>4000&&typeof notify=='function'&&!(j.W&&j.W.el.isConnected&&!res.ok))try{notify(stopped?'Stopped':kindMove?'Moved':'Copied',msg,{quiet:true})}catch(e){}
 return res}
/* ---------- a file kept on the disk (a large download), copied within C: (dl27.js): a file of its own on the disk ----------
   (the Vivobook, 2026-11-20: a 692 MB download copied with Copy and Paste became a 99-byte "- COPY" that was not on the disk:
   the reference was copied, not the file). The account's own large downloads are copied by rudolfd (dl/file to=copy); one
   in a home folder kept on the disk is copied by home56.js (home/copy) when the folder is saved; any other is refused. */
function xfRefKind(v){const i=typeof dlRefInfo=='function'&&dlRefInfo(v);if(!i)return'bad';
 if(typeof hdRefRel=='function'&&typeof hdOn=='function'&&hdOn()&&hdRefRel(v)!=null)return'home';
 if(typeof DLADIR=='function'&&String(i.p).startsWith(DLADIR())&&String(i.p).slice(DLADIR().length).indexOf('/')<0)return'acct';
 return'other'}
async function xfRefCopy(v,newName){const k=xfRefKind(v),i=dlRefInfo(v);if(k=='home')return clone(v);
 if(k!='acct')throw new Error('this file is kept on the disk where Rudolf OS cannot copy it');
 const r=await rdPost('dl/file?name='+encodeURIComponent(dlBase(i.p))+'&from=home&to=copy&as='+encodeURIComponent(newName)+DLAQ());
 dlRefsAdd(r.path);return dlMkRef(r.path,r.size!=null?r.size:i.s,i.av)}
/* ---------- the selection box ----------
   box: the scrolling list; e: the pointerdown on an empty spot; o: {items() the elements with data-k, get() the selection
   (a Set), set(Set), keep: Ctrl or Shift held (add to the selection)} */
function xfMarquee(box0,e,o){if(e.button!==0||e.pointerType&&e.pointerType!='mouse')return false;let box=box0;
 /* (the list drawn again meanwhile, a file saved by another window, say: the box goes on in the new one, at the same scroll) */
 const live=()=>{const nb=o.box&&o.box();if(nb&&nb!==box&&nb.isConnected&&!box.isConnected){nb.scrollTop=box.scrollTop;box=nb;if(getComputedStyle(box).position=='static')box.style.position='relative';if(m)box.appendChild(m);box.__mq=1}return box};
 const br0=box.getBoundingClientRect();if(e.clientX-br0.left>=box.clientLeft+box.clientWidth||e.clientY-br0.top>=box.clientTop+box.clientHeight)return false;   /* (its scroll bars) */
 const pos=ev=>{const r=box.getBoundingClientRect();return{x:ev.clientX-r.left-box.clientLeft+box.scrollLeft,y:ev.clientY-r.top-box.clientTop+box.scrollTop}};
 const a=pos(e),base=o.keep?new Set(o.get()):new Set();let m=null,last=e,tm=0,started=false;
 if(getComputedStyle(box).position=='static')box.style.position='relative';
 const hits=()=>{live();const b=pos(last),x1=Math.min(a.x,b.x),x2=Math.max(a.x,b.x),y1=Math.min(a.y,b.y),y2=Math.max(a.y,b.y);
  m.style.cssText=`position:absolute;left:${x1}px;top:${y1}px;width:${x2-x1}px;height:${y2-y1}px;border:1px solid var(--ac,#2563eb);background:rgba(37,99,235,.16);pointer-events:none;z-index:5;box-sizing:border-box`;
  const r=box.getBoundingClientRect(),S=new Set(base);
  for(const el of o.items()){const q=el.getBoundingClientRect(),ex=q.left-r.left-box.clientLeft+box.scrollLeft,ey=q.top-r.top-box.clientTop+box.scrollTop;
   if(ex<x2&&ex+q.width>x1&&ey<y2&&ey+q.height>y1)S.add(el.dataset.k)}
  const cur=o.get();if(S.size!=cur.size||[...S].some(k=>!cur.has(k)))o.set(S)};
 const scroll=()=>{live();const r=box.getBoundingClientRect(),y=last.clientY,edge=24;let d=0;if(y<r.top+edge)d=-Math.ceil((r.top+edge-y)/3);else if(y>r.bottom-edge)d=Math.ceil((y-r.bottom+edge)/3);
  if(d){const s0=box.scrollTop;box.scrollTop+=Math.max(-40,Math.min(40,d));if(box.scrollTop!=s0)hits()}};
 const mv=ev=>{if(!(ev.buttons&1))return;/* (a move from before the press, delivered late) */last=ev;if(!started){if(Math.abs(ev.clientX-e.clientX)<4&&Math.abs(ev.clientY-e.clientY)<4)return;started=true;m=document.createElement('div');m.className='xmarq';box.appendChild(m);box.__mq=1;document.body.style.userSelect='none';try{getSelection().removeAllRanges()}catch(x){}tm=setInterval(scroll,30)}
  ev.preventDefault();hits()};
 const up=ev=>{document.removeEventListener('pointermove',mv,true);document.removeEventListener('pointerup',up,true);document.removeEventListener('pointercancel',up,true);clearInterval(tm);document.body.style.userSelect='';
  /* the click that ends the box does not clear the selection: the list ignores clicks until the next press */
  if(m){live();m.remove();const b=box,clr=()=>{b.__mq=0;box0.__mq=0;document.removeEventListener('pointerdown',clr,true)};document.addEventListener('pointerdown',clr,true);setTimeout(clr,1500);o.end&&o.end()}};
 document.addEventListener('pointermove',mv,true);document.addEventListener('pointerup',up,true);document.addEventListener('pointercancel',up,true);
 return true}
/* ---------- dragging: what a list takes, and the pointer it shows ---------- */
/* an empty spot of a list: not an item, its header, a drive's buttons, the search line */
function xEmptySpot(t,box){if(!t||!box.contains(t))return false;if(t.closest('[data-k],.xlnk,thead,th,.xrb,.xsrch,button,input,select,textarea,a,label'))return false;return true}
/* something File Explorer can take: its own items (this window's or another's), a drive's, files from outside */
const xDragOk=e=>!!(FC.drag||FC.dragR||(e&&e.dataTransfer&&[...e.dataTransfer.types].includes('Files')));
/* the pointer: move, or copy with Ctrl (and always from a read-only folder; files from outside are copied) */
/* Ctrl held: WebKit (Lite and Native) gives a drop no modifier keys (GitHub's WebKit check, real input): the keys are
   followed here, and the drag's own action (copy, when the system's drag took Ctrl) is taken too */
const XFK={ctrl:false,fx:''};
addEventListener('keydown',e=>{if(e.key=='Control')XFK.ctrl=true},true);addEventListener('keyup',e=>{if(e.key=='Control')XFK.ctrl=false},true);addEventListener('blur',()=>{XFK.ctrl=false});
['dragover','drag'].forEach(t=>document.addEventListener(t,e=>{if(e.ctrlKey)XFK.ctrl=true},true));document.addEventListener('dragend',()=>{XFK.fx=''},true);
function xCopy(e){if(!e)return false;let os=false;try{os=e.type=='drop'&&XFK.fx=='move'&&e.dataTransfer&&e.dataTransfer.dropEffect=='copy'}catch(x){}return !!(e.ctrlKey||XFK.ctrl||os)}
function xMode(e,rr){return xCopy(e)||rr&&rr.r&&rr.r.ro?'copy':'cut'}
function xDropFx(e){try{const dt=e.dataTransfer,ext=!FC.drag&&!FC.dragR;dt.dropEffect=ext||e.ctrlKey||XFK.ctrl||(FC.dragR&&FC.dragR.r&&FC.dragR.r.ro)||[...dt.types].includes('text/x-rudolf-ro')?'copy':'move';XFK.fx=dt.dropEffect}catch(x){}}
/* the targets' highlight goes when the drag ends, wherever it ends */
['dragend','drop'].forEach(t=>document.addEventListener(t,()=>setTimeout(()=>document.querySelectorAll('.xp25 .xdt,.xp25 .dt').forEach(x=>x.classList.remove('xdt','dt')),0)));
