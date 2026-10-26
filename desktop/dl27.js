/* ===== 1.2.1: downloads of any size =====
   Chromium saves every website download (real tabs, the frames fallback, the sign-in window) in
   /home/rudolf/Downloads/.incoming. rudolfd lists what is there (dl/incoming); here each finished file is checked:
   the size limit (Settings ▸ Network ▸ Downloads), the file-type rules, an administrator's approval for program files
   (the file waits in the quarantine folder meanwhile) and the antivirus. Files of 1.5 MB or less then go into
   C:\DOWNLOADS as before; larger ones stay on the disk in /home/rudolf/Downloads and C:\DOWNLOADS holds a reference
   to them ("rudolfdisk:" + {p: path, s: size, av: 'full'|'part'}), opened through a /dlf/ link that the viewers read
   in pieces. A reference cannot be copied elsewhere in C: (it is put back), and when the last reference to a file is
   gone (Trash emptied, quarantined copy deleted) the file is deleted from the disk too. */
window.__dl27=1;
const DLREF='rudolfdisk:',DL_SMALL=1.5*1048576,DL_AVPART=4*1048576,DLRK='bd95dlrefs';
const isDlRef=v=>typeof v=='string'&&v.startsWith(DLREF);
function dlRefInfo(v){try{const o=JSON.parse(v.slice(DLREF.length));return o&&typeof o.p=='string'?o:null}catch(e){return null}}
const dlMkRef=(p,s,av)=>DLREF+JSON.stringify({p,s,av});
const dlMax=()=>Math.max(1,+(NETC.dl&&NETC.dl.max)||4096)*1048576;
const dlBase=p=>String(p).split('/').pop();
const dlCName=n=>String(n).toUpperCase().replace(/[\\\/:*?"<>|]/g,'_').slice(0,60)||'DOWNLOAD';
const DLN={kill:new Set(),seen:new Set(),pend:[],busy:false,chain:Promise.resolve(),ask:Promise.resolve(),warned:0,t:0};
const dlOn=()=>!!(RD&&window.__sys);
function dlHost(u){try{return new URL(u).hostname}catch(e){return''}}
function dlDone(rec,st,why,extra){Object.assign(rec,{st,why:why||''},extra||{});const i=DLN.pend.indexOf(rec);if(i>=0)DLN.pend.splice(i,1);saveDl();try{window.__dlHook&&window.__dlHook()}catch(e){}
 const ok=st=='saved';notify(ok?'Downloaded':'Download '+(st=='quarantined'?'quarantined':'stopped'),rec.n+(ok?(rec.disk?' is in C:\\DOWNLOADS (kept on the disk).':' is in C:\\DOWNLOADS.'):' – '+(why||'it could not be saved.')),{kind:ok?'':'error',action:()=>browser('about:downloads')});
 try{EXPL.forEach(f=>f())}catch(e){}}
/* a real tab (or the desktop) started a download: a waiting line in about:downloads until the file arrives */
function dlNative(u,o){o=o||{};let nm='';try{nm=decodeURIComponent(o.name||new URL(u).pathname.split('/').pop()||'')}catch(e){}
 const rec={n:nm?dlCName(nm):'DOWNLOAD',u:String(u||''),t:Date.now(),sz:0,st:'wait',why:'',nat:1};DLS.push(rec);if(DLS.length>100)DLS.shift();saveDl();DLN.pend.push(rec);
 try{window.__dlHook&&window.__dlHook()}catch(e){}clearTimeout(DLN.t);DLN.t=setTimeout(dlPoll,700);return rec}
/* hand a download to Chromium itself (the frames fallback, or a file too large to fetch here) */
function dlHandOff(u,rec){const f=document.createElement('iframe');f.setAttribute('sandbox','allow-downloads');f.style.cssText='position:fixed;width:1px;height:1px;left:-9px;top:-9px;border:0;opacity:0';f.src=u;document.body.appendChild(f);setTimeout(()=>f.remove(),120000);
 if(rec){rec.nat=1;rec.st='wait';rec.why='';if(!DLN.pend.includes(rec))DLN.pend.push(rec);saveDl()}clearTimeout(DLN.t);DLN.t=setTimeout(dlPoll,700)}
function dlMatch(it){const N=dlCName(it.name);let r=DLN.pend.find(x=>x.st=='wait'&&x.part==it.name+'.crdownload')||DLN.pend.find(x=>x.st=='wait'&&x.n==N);if(!r)r=DLN.pend.find(x=>x.st=='wait'&&Date.now()-x.t<3600e3);
 if(!r){r={n:N,u:'',t:Date.now(),sz:0,st:'wait',why:'',nat:1};DLS.push(r);if(DLS.length>100)DLS.shift();DLN.pend.push(r)}return r}
async function dlPoll(){clearTimeout(DLN.t);if(!dlOn())return;if(DLN.busy){DLN.t=setTimeout(dlPoll,1500);return}DLN.busy=true;let active=false;
 try{const j=await rd('dl/incoming');const items=j.items||[];
  for(const it of items){const key=it.name+'\0'+it.size+'\0'+it.mtime;
   if(it.partial){active=true;const r=DLN.pend.find(x=>x.st=='wait'&&(!x.part||x.part==it.name))||null;if(r){r.sz=it.size;r.part=it.name;if(r.gs!==it.size){r.gs=it.size;r.gt=Date.now()}}
    if(r&&Date.now()-r.gt>120e3){/* not grown for two minutes: Chromium is holding it (for example a file over http from a secure page) */
     await rdPost('dl/file?name='+encodeURIComponent(it.name)+'&to=del').catch(()=>{});dlDone(r,'failed','The download stopped. The site may offer it over an insecure connection (http) from a secure page.');continue}
    if(it.size>dlMax()){DLN.kill.add(it.name.replace(/\.crdownload$/,''));await rdPost('dl/file?name='+encodeURIComponent(it.name)+'&to=del').catch(()=>{});if(r)dlDone(r,'blocked','The file is larger than '+fmtB(dlMax())+', the limit set in Settings ▸ Network ▸ Downloads.')}
    continue}
   if(DLN.kill.has(it.name)){/* stopped while it arrived: Chromium finished writing it anyway */DLN.kill.delete(it.name);await rdPost('dl/file?name='+encodeURIComponent(it.name)+'&to=del').catch(()=>{});continue}
   if(DLN.seen.has(key))continue;DLN.seen.add(key);active=true;const rec=dlMatch(it);rec.n=dlCName(it.name);rec.sz=it.size;rec.st='check';
   DLN.chain=DLN.chain.then(()=>dlIngest(it,rec)).catch(e=>{console.warn('download',e);dlDone(rec,'failed',e&&e.message||'The file could not be checked.')})}
  DLN.pend.filter(x=>x.st=='wait'&&!x.part&&Date.now()-x.t>180e3).forEach(x=>dlDone(x,'failed','The download did not start (the site did not send the file).'));
  if(DLN.seen.size>500)DLN.seen=new Set([...DLN.seen].slice(-200));
  try{window.__dlHook&&window.__dlHook()}catch(e){}}
 catch(e){}finally{DLN.busy=false}
 try{dlReconcile()}catch(e){console.warn('downloads',e)}
 DLN.t=setTimeout(dlPoll,active||DLN.pend.length?1000:4000)}
/* bytes of a file in the download folders */
async function dlRead(name,from,off,len){const r=await rd('dl/read?name='+encodeURIComponent(name)+'&from='+from+'&off='+off+'&len='+len);return new Uint8Array(await r.arrayBuffer())}
const dlLatin=u8=>new TextDecoder('latin1').decode(u8);
/* the antivirus on a large file: its first and last 4 MB (the whole file up to 8 MB) */
async function dlScanBig(name,from,size,NM,prog){const hits=[];const h=await dlRead(name,from,0,DL_AVPART*2>=size?size:DL_AVPART);const parts=[dlLatin(h)];
 if(size>DL_AVPART*2)parts.push(dlLatin(await dlRead(name,from,-DL_AVPART,DL_AVPART)));
 try{avScan(NM,'',true).forEach(x=>hits.push(x))}catch(e){}
 if(!prog&&/^(MZ|\x7fELF)/.test(parts[0]))hits.push('Win.Executable.Header');
 for(const s of parts){if(/EICAR-STANDARD-ANTIVIRUS-TEST-FILE/.test(s))hits.push('EICAR-Test-File');try{SIGS.forEach(([n,r])=>{if(r.test(s))hits.push(n)})}catch(e){}}
 return[...new Set(hits)]}
function dlUniq(d,nm){let n2=nm;if(n2 in d){const m=/^(.*?)(\.[^.]*)?$/.exec(nm);for(let i=2;;i++){n2=m[1]+' ('+i+')'+(m[2]||'');if(!(n2 in d))break}}return n2}
/* a Rudolf OS release's own disc files (1.2.7): rudolf-os-X.Y.Z.iso (1.2.8: or its boot image, rudolf-os-X.Y.Z-boot.iso), its .partNN parts and its .sha256, from this
   project's GitHub releases (repository id 1414456577, in the release-asset address GitHub redirects to). They pass the
   file-type rules (which block .partNN and .sha256 as unknown and .iso as a program type); the antivirus still scans
   them, and Make a stick / Install from accept only images whose release list Rudolf OS's key signed. */
const DL_REPO_ID='1414456577',DL_REPO='/merciless146/RudolfOS/releases/download/';
function dlRelFile(name,u){if(!/^rudolf-os-\d+\.\d+\.\d+(-boot)?\.iso(\.part\d{2}|\.sha256)?$/i.test(String(name||'')))return false;
 let x;try{x=new URL(String(u||''))}catch(e){return false}if(x.protocol!='https:')return false;
 const h=x.hostname.toLowerCase(),p=decodeURIComponent(x.pathname);
 return ((h=='release-assets.githubusercontent.com'||h=='objects.githubusercontent.com')&&p.split('/').includes(DL_REPO_ID))||(h=='github.com'&&p.startsWith(DL_REPO))}
/* the antivirus part of the small-file check, for a release file (its type is allowed above) */
function dlAvOnly(nm,val){if(!SECC.av.on)return{ok:true};const th=avScan(nm,val,true);if(!th.length)return{ok:true};
 logEvt('av',`Blocked ${nm} (download): ${th.join(', ')}`);return{ok:false,msg:`Antivirus blocked ${nm}: ${th.join(', ')}.`}}
function dlCDir(){const C=fs['C:'];if(!C.DOWNLOADS||typeof C.DOWNLOADS!='object')C.DOWNLOADS={};return C.DOWNLOADS}
async function dlIngest(it,rec){let name=it.name,from='incoming';const NM=dlCName(name),e=ext(NM),rel=dlRelFile(name,rec.u),prog=!rel&&HIGHRISK.has(e),size=it.size;
 if(rel)logEvt('av',`Allowed ${NM}: a Rudolf OS disc file from the Rudolf OS releases`);
 const del=()=>rdPost('dl/file?name='+encodeURIComponent(name)+'&from='+from+'&to=del').catch(()=>{});
 if(size>dlMax()){await del();return dlDone(rec,'blocked','The file is larger than '+fmtB(dlMax())+', the limit set in Settings ▸ Network ▸ Downloads.')}
 if(prog){if(!(window.__net&&NETC.dl.exe=='ask')){await del();logEvt('av','Blocked download '+NM+': program file');return dlDone(rec,'blocked',typeGate(NM)||'Program files are blocked in Settings ▸ Network ▸ Downloads.')}
  const q=await rdPost('dl/file?name='+encodeURIComponent(name)+'&to=quar');name=q.name;from='quarantine';rec.st='approve';saveDl();try{window.__dlHook&&window.__dlHook()}catch(x){}
  const ok=await new Promise(res=>{DLN.ask=DLN.ask.then(()=>elevate(`${NM} is a program file${rec.u?' from '+dlHost(rec.u):''}. Allow it to be kept in Downloads? Rudolf OS can store it but cannot run it.`)).then(res,()=>res(false))});
  if(!ok){await del();logEvt('av','Program download not approved: '+NM);return dlDone(rec,'blocked','An administrator did not approve this program file.')}
  rec.exe=true;rec.by=USER&&isAdmin()?USER.n:'an administrator';rec.st='check'}
 else if(!rel){const tg=typeGate(NM);if(tg){await del();logEvt('av',`Blocked download ${NM}: ${tg}`);return dlDone(rec,'blocked',tg)}}
 if(size<=DL_SMALL){/* small: into C:\DOWNLOADS, checked as before */
  const u8=await dlRead(name,from,0,size||1),b=new Blob([u8],{type:(typeof MIMEX!='undefined'&&MIMEX[e])||'application/octet-stream'});
  const val=await new Promise(res=>{const fr=new FileReader();fr.onload=()=>res(fr.result);TEXTX.test(e)?fr.readAsText(b):fr.readAsDataURL(b)});
  const g=prog?netExeScan(NM,val):rel?dlAvOnly(NM,val):secIngress(NM,val,'download');await del();if(!g.ok)return dlDone(rec,g.q?'quarantined':'blocked',g.msg);
  const d=dlCDir(),n2=dlUniq(d,NM);d[n2]=val;if(!fsSave()){delete d[n2];return dlDone(rec,'failed','The disk is full.')}return dlDone(rec,'saved','',{path:n2})}
 /* large: stays on the disk */
 let av='off';if(SECC.av.on){const th=await dlScanBig(name,from,size,NM,prog);av=size>DL_AVPART*2?'part':'full';
  if(th.length){const qm=SECC.av.act=='quarantine';logEvt('av',`${qm?'Quarantined':'Blocked'} ${NM} (download): ${th.join(', ')}`);
   if(qm){let qp;if(from=='quarantine')qp='/home/rudolf/Downloads/.quarantine/'+name;else{const q=await rdPost('dl/file?name='+encodeURIComponent(name)+'&to=quar');qp=q.path}
    QUAR.push({id:Date.now()+Math.random(),n:NM,from:['C:','DOWNLOADS'],th,t:Date.now(),data:dlMkRef(qp,size,av),src:'download'});saveQ()}else await del();
   return dlDone(rec,qm?'quarantined':'blocked',`Antivirus ${qm?'quarantined':'blocked'} ${NM}: ${th.join(', ')}.`)}}
 const k=await rdPost('dl/file?name='+encodeURIComponent(name)+'&to='+(from=='quarantine'?'release':'keep'));
 const d=dlCDir(),n2=dlUniq(d,NM);d[n2]=dlMkRef(k.path,k.size,av);fsSave();dlRefsAdd(k.path);
 dlDone(rec,'saved','',{path:n2,disk:k.path,av});dlSpaceCheck()}
async function dlSpaceCheck(){try{const s=await rd('dl/space');if(s.total>0&&s.free/s.total<.1&&Date.now()-DLN.warned>600e3){DLN.warned=Date.now();
  notify('The disk is almost full',`${fmtB(s.free)} free of ${fmtB(s.total)}.${s.live?' Running live, downloads are kept in memory until you shut down.':''} Delete downloads you no longer need.`,{kind:'error',action:()=>launch&&launch('settings')})}}catch(e){}}
/* ---------- references: where they are, and the disk files that go with them ---------- */
function dlRefsAdd(p){const k=ld(DLRK,{});k[p]={w:'fs',u:'',miss:0};svj(DLRK,k)}
function dlWalk(n,path,out){for(const k in n){const v=n[k];if(isDir(v))dlWalk(v,path.concat(k),out);else if(isDlRef(v))out.push({path:path.concat(k),v})}return out}
/* 1.2.7: Files' "Downloads" is C:\DOWNLOADS now; say once where an account's own DOWNLOADS folder went */
function dlHomeNote(){try{if(!USER||USER.r=='guest')return;const k='bd95dlnote:'+uid();if(ld(k,0))return;svj(k,1);const h=node(homePath('DOWNLOADS'));
 if(isDir(h)&&Object.keys(h).length)notify('Downloads',`Downloads now opens C:\\DOWNLOADS, where Oracle saves what you download. What was in your own Downloads folder is still there: ${USER.n} › DOWNLOADS.`,{action:()=>explorer(homePath('DOWNLOADS'))})}catch(e){}}
let dlRecBusy=false;
async function dlReconcile(){if(dlRecBusy||!dlOn()||typeof FSLOADED!='undefined'&&!FSLOADED||!isDir(fs['C:']))return;dlRecBusy=true;dlHomeNote();
 try{const all=dlWalk(fs['C:'],['C:'],[]),D=dlCDir();let changed=false,moved=0;
  /* references outside C:\DOWNLOADS go back; copies of the same file are dropped */
  const inD=new Map();all.filter(x=>x.path.length==3&&x.path[1]=='DOWNLOADS').forEach(x=>{const i=dlRefInfo(x.v);if(!i)return;if(inD.has(i.p)){delete D[x.path[2]];changed=true;moved++}else inD.set(i.p,x)});
  all.filter(x=>!(x.path.length==3&&x.path[1]=='DOWNLOADS')).forEach(x=>{const i=dlRefInfo(x.v),dir=node(x.path.slice(0,-1));if(dir)delete dir[x.path.at(-1)];changed=true;moved++;
   if(i&&!inD.has(i.p)){const n2=dlUniq(D,x.path.at(-1));D[n2]=x.v;inD.set(i.p,{path:['C:','DOWNLOADS',n2],v:x.v})}});
  if(moved)notify('Kept in Downloads','Files larger than 1.5 MB are kept on the disk and stay in C:\\DOWNLOADS. Open them from there.');
  /* restored from quarantine: back into Downloads on the disk */
  for(const[p,x]of [...inD]){if(!p.includes('/.quarantine/'))continue;const i=dlRefInfo(x.v);
   try{const r=await rdPost('dl/file?name='+encodeURIComponent(dlBase(p))+'&to=release');const nv=dlMkRef(r.path,i.s,i.av);D[x.path[2]]=nv;inD.delete(p);inD.set(r.path,{path:x.path,v:nv});changed=true}catch(e){}}
  /* renamed in C:\DOWNLOADS: the same name on the disk */
  for(const[p,x]of [...inD]){const cn=x.path[2],dn=dlBase(p);if(cn.toUpperCase()==dn.toUpperCase()||p.includes('/.quarantine/'))continue;const i=dlRefInfo(x.v);
   try{const r=await rdPost('dl/file?name='+encodeURIComponent(dn)+'&from=home&to=rename&as='+encodeURIComponent(cn));const nv=dlMkRef(r.path,i.s,i.av);D[cn]=nv;inD.delete(p);inD.set(r.path,{path:x.path,v:nv});changed=true}catch(e){}}
  if(changed)fsSave();
  /* every place a reference can still be: C:, this account's Trash, the quarantine */
  const alive=new Map();inD.forEach((x,p)=>alive.set(p,{w:'fs',u:''}));
  const me=USER?uid():'';const tw=v=>{if(isDir(v))dlWalk(v,[],[]).forEach(y=>{const i=dlRefInfo(y.v);i&&alive.set(i.p,{w:'tr',u:me})});else if(isDlRef(v)){const i=dlRefInfo(v);i&&alive.set(i.p,{w:'tr',u:me})}};
  try{TR.forEach(e=>tw(e.v))}catch(e){}
  try{QUAR.forEach(e=>{if(isDlRef(e.data)){const i=dlRefInfo(e.data);i&&alive.set(i.p,{w:'q',u:''})}})}catch(e){}
  const k=ld(DLRK,{}),nk={};
  for(const p in k){if(alive.has(p))continue;const o=k[p];
   if(o.w=='tr'&&o.u&&o.u!=me){nk[p]=o;continue}/* in another account's Trash */
   if((o.miss||0)<1){nk[p]=Object.assign({},o,{miss:(o.miss||0)+1});continue}/* gone twice in a row: delete it */
   const from=p.includes('/.quarantine/')?'quarantine':'home';await rdPost('dl/file?name='+encodeURIComponent(dlBase(p))+'&from='+from+'&to=del').catch(()=>{})}
  alive.forEach((o,p)=>nk[p]=Object.assign({miss:0},o));svj(DLRK,nk);
  if(changed)try{EXPL.forEach(f=>f())}catch(e){}}
 finally{dlRecBusy=false}}
/* ---------- opening files kept on the disk ---------- */
async function dlLink(v){const i=dlRefInfo(v);if(!i)throw new Error('This reference is damaged.');const j=await rd('dl/link?path='+encodeURIComponent(i.p));return j.url}
const DL_VIDEO=/^(MP4|M4V|WEBM|MKV|MOV|OGV)$/,DL_AUDIO=/^(MP3|WAV|OGG|OPUS|M4A|AAC|FLAC)$/,DL_IMG=/^(PNG|JPG|JPEG|GIF|WEBP|BMP|AVIF|SVG)$/;
async function dlOpen(path){const v=node(path),nm=path.at(-1),e=ext(nm),i=dlRefInfo(v);if(!i)return;try{recentAdd(path)}catch(x){}
 if(e=='PDF')return launchWith('pdf',path);
 let url;try{url=await dlLink(v)}catch(x){return dlMissing(path,x.message)}
 if(DL_VIDEO.test(e)||DL_AUDIO.test(e)){const vid=DL_VIDEO.test(e),W=win(nm,vid?'🎬':'🎵',vid?760:420,vid?480:150);W.body.style.cssText='background:#000;display:flex;align-items:center;justify-content:center';
  const m=document.createElement(vid?'video':'audio');m.controls=true;m.autoplay=true;m.src=url;m.style.cssText=vid?'max-width:100%;max-height:100%;width:100%':'width:94%';m.onerror=()=>{W.body.style.background='var(--wbg,#fff)';W.body.textContent='This file cannot be played here (its format is not supported).'};W.body.appendChild(m);return W}
 if(DL_IMG.test(e)){const W=win(nm,'🖼️',720,520);W.body.style.cssText='background:#222;overflow:auto;display:flex;align-items:center;justify-content:center';const im=new Image();im.src=url;im.alt=nm;im.style.cssText='max-width:100%;max-height:100%';W.body.appendChild(im);return W}
 if(TEXTX.test(e)||/^(TXT|LOG|CSV|JSON|MD|XML|SRT|VTT)$/.test(e)){if(i.s>4*1048576)return dlInfo(path,'This text file is too large to open in Notepad. Open it in a Linux app from the Downloads folder instead.');
  const W=win(nm+' (read only)','🗒️',720,520);W.body.style.cssText='overflow:auto;background:var(--wbg,#fff)';const pre=document.createElement('pre');pre.style.cssText='margin:0;padding:10px;white-space:pre-wrap;font:12px monospace';pre.textContent='Opening…';W.body.appendChild(pre);
  try{pre.textContent=await(await fetch(url)).text()}catch(x){pre.textContent='The file could not be read.'}return W}
 return dlInfo(path,e=='ZIP'?'Archive Manager opens archives up to 1.5 MB. Open this one in a Linux app from the Downloads folder.':'Rudolf OS has no app that opens this kind of file from the disk.')}
function dlInfo(path,msg){const v=node(path),i=dlRefInfo(v)||{},W=win('Kept on the disk','💽',420,210);W.body.style.cssText='padding:14px;font-size:13px';
 W.body.innerHTML=`<b class='n'></b><p class='m'></p><p style='color:#6b7280'>${fmtB(i.s||0)} · in the Downloads folder on the disk${i.av=='part'?' · the antivirus checked its first and last 4 MB':''}</p><div style='text-align:right'><button>Close</button></div>`;
 W.body.querySelector('.n').textContent=path.at(-1);W.body.querySelector('.m').textContent=msg;W.body.querySelector('button').onclick=()=>W.close();return W}
function dlMissing(path,why){const W=dlInfo(path,'This file is no longer on the disk ('+(why||'not found')+'). The reference can be removed.');const b=document.createElement('button');b.textContent='Remove it';b.style.marginRight='6px';
 b.onclick=()=>{const d=node(path.slice(0,-1));if(d){delete d[path.at(-1)];fsSave();try{EXPL.forEach(f=>f())}catch(e){}}W.close()};W.body.querySelector('div').prepend(b)}
{const _op=openPath;openPath=function(path){if(isDlRef(node(path)))return dlOpen(path);return _op.apply(this,arguments)}}
{const _od=openPathDefault;openPathDefault=function(path){if(isDlRef(node(path)))return dlOpen(path);return _od.apply(this,arguments)}}
{const _of=openersFor;openersFor=function(path){if(!isDlRef(node(path)))return _of.apply(this,arguments);const e=ext(path.at(-1)),l=[['disk','Built-in viewer','💽',()=>true,p=>dlOpen(p)]];if(e=='PDF')l.unshift(OPENERS.find(x=>x[0]=='pdf'));return l.filter(Boolean)}}
/* ---------- Storage settings: what is kept on the disk ---------- */
if(typeof storPanel=='function'){const _sp=storPanel;storPanel=function(c){const r=_sp.apply(this,arguments);if(!dlOn())return r;
 const refs=dlWalk(dlCDir(),['C:','DOWNLOADS'],[]),h=document.createElement('div');h.className='dl27st';
 h.innerHTML=`<h4 class='sh4'>Downloads kept on the disk</h4><p class='msg dlsp'></p><table class='tmt'><tbody></tbody></table>`;
 const tb=h.querySelector('tbody');let tot=0;refs.forEach(x=>{const i=dlRefInfo(x.v)||{};tot+=i.s||0;const row=tb.insertRow();row.insertCell().textContent=x.path[2];row.insertCell().textContent=fmtB(i.s||0);
  const a=row.insertCell(),o=document.createElement('button'),d=document.createElement('button');o.textContent='Open';d.textContent='Delete';o.onclick=()=>dlOpen(x.path);
  d.onclick=()=>{if(trashPut([x.path])>0){storPanel(c)}};a.append(o,' ',d)});
 if(!refs.length)tb.insertRow().insertCell().textContent='None. Downloads larger than 1.5 MB are kept here.';
 const sp=h.querySelector('.dlsp');sp.textContent=`${refs.length} file(s), ${fmtB(tot)}.`;rd('dl/space').then(s=>{if(s.total>0)sp.textContent+=` The disk has ${fmtB(s.free)} free of ${fmtB(s.total)}.${s.live?' Running live: they are kept in memory and gone after you shut down.':''}`}).catch(()=>{});
 const bk=c.querySelectorAll('h4.sh4')[1];bk?c.insertBefore(h,bk):c.appendChild(h);return r}}
/* start */
if(dlOn()){const go=()=>{setTimeout(dlPoll,1500);setTimeout(dlSpaceCheck,8000)};typeof FSREADY!='undefined'?FSREADY.then(go,go):go()}
