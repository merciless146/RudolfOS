/* ===== 1.6.1 (the owner, 2026-11-20): ZIP files kept on the disk =====
   "Archive Manager would list what's inside a large ZIP without loading it into memory, and Extract here would unpack it next to
   the ZIP, with a progress window, using the same unpacking Add games… already uses."
   A download larger than 1.5 MB stays on the disk, in the account's folder (/home/rudolf/Downloads/.accounts/<id>/), and Downloads
   holds a reference to it (dl27.js). Archive Manager and Extract here read an archive into the page, so for such a ZIP they said
   "Archive Manager opens archives up to 1.5 MB". Now:
   - opening it (dl27.js dlOpen, File Explorer's Open with › Archive Manager, Archive Manager given its path): a window lists what is
     inside (rudolfd dl/zip: names, sizes, folders; at most 20,000 entries), with Extract here;
   - Extract here (that window's, or File Explorer's): rudolfd dl/unzip unpacks it as the desktop user (dlzip.sh) and moves each file
     flat into the account's folder; a progress window shows the file, n of N and the bytes, with Cancel. Then a folder named after
     the ZIP, beside it (dlUniq), holds its sub-folders as in the ZIP, each file a reference to its file on the disk (dlRefsAdd),
     checked by the antivirus as dlAdopt checks a file found in the folder (blocked or quarantined ones are said); then the files are
     saved, File Explorer drawn again and a notification says where they are.
   While it runs (and until its references are saved) rudolfd's dl/home says "unzip", so dlAdopt does not take the files in as
   downloads of their own, in any page (the native desktop's Explorer window is a page of its own, Oracle's another). The same on
   Standard, Native and Lite: it is the page's own code. ===== */
window.__dlzip61=1;
const DLZ={run:null};
/* the account's folder (acct) and the file's name on the disk for a reference to a ZIP kept on the disk, or null */
function dlzOf(path){const v=node(path),i=isDlRef(v)&&dlRefInfo(v);if(!i||ext(path.at(-1))!='ZIP')return null;
 const m=/^\/home\/rudolf\/Downloads\/\.accounts\/([a-z0-9-]{1,64})\/([^/]+)$/.exec(String(i.p));return m?{i,acct:m[1],name:m[2]}:null}
const dlzQ=z=>'name='+encodeURIComponent(z.name)+'&acct='+encodeURIComponent(z.acct);
const dlzC=s=>String(s).toUpperCase().replace(/[\\\/:*?"<>|]/g,'_').slice(0,60)||'_';
const dlzWait=ms=>new Promise(r=>setTimeout(r,ms));
/* the window: what is inside (the first 3,000 rows drawn; all are extracted) */
async function dlZipWin(path){const nm=path.at(-1),z=dlzOf(path);
 if(!z)return dlInfo(path,'Archive Manager opens archives up to 1.5 MB, and larger ones kept in your Downloads folder. Open this one in a Linux app from the Downloads folder.');
 const W=appWin('archive','Archive Manager - '+nm,'📦',640,440);W.body.style.cssText='display:flex;flex-direction:column';
 W.body.innerHTML=`<div class='xb'><button class='dlzx' disabled>Extract here</button></div><div class='al dlzl' style='flex:1;overflow:auto;background:var(--wbg,#fff)'></div><div class='stat dlzs'>Reading the archive…</div>`;
 const q=s=>W.body.querySelector(s),list=q('.dlzl'),stat=q('.dlzs'),bx=q('.dlzx');
 bx.onclick=()=>dlZipExtract(path);
 let j;try{j=await rd('dl/zip?'+dlzQ(z))}catch(e){stat.textContent=/not a ZIP/.test(e.message)?e.message:'This archive could not be read ('+e.message+').';return W}
 const items=j.items||[],files=items.filter(x=>!x.dir),t=document.createElement('table');
 t.innerHTML='<thead><tr><th>Name</th><th style="text-align:right">Size</th></tr></thead><tbody></tbody>';const tb=t.querySelector('tbody');
 items.slice(0,3000).forEach(x=>{const r=tb.insertRow(),a=r.insertCell(),b=r.insertCell();a.textContent=(x.dir?'📁 ':'📄 ')+x.name;b.style.textAlign='right';b.textContent=x.dir?'':fmtB(x.size)});
 list.textContent='';list.appendChild(t);
 const nf=j.more?j.total-items.filter(x=>x.dir).length:files.length;
 stat.textContent=`${nf.toLocaleString()} file(s), ${fmtB(j.bytes||0)} unpacked, in ${nm}`+(j.more||items.length>3000?` (the first ${Math.min(3000,items.length).toLocaleString()} of ${j.total.toLocaleString()} entries are shown; Extract here unpacks them all)`:'');
 bx.disabled=false;return W}
/* Extract here */
async function dlZipExtract(path){const nm=path.at(-1),z=dlzOf(path),parent=path.slice(0,-1);
 const no=t=>notify('Extract here',t,{kind:'error'});
 if(!z)return no(nm+' is not in a Downloads folder on the disk, so it cannot be extracted here.');
 if(z.acct!=dlAcct())return no(nm+' is kept with the downloads from before 1.5.5, which every account shares: copy it into your own Downloads first.');
 if(!isDir(node(parent))||!fsCanWrite(parent))return no('Files cannot be extracted into '+parent.join('\\')+'.');
 if(DLZ.run)return no('Another archive is being extracted. Try again when it has finished.');
 try{await rdPost('dl/unzip?'+dlzQ(z))}catch(e){return no(/busy|another task/i.test(e.message)?'Rudolf OS is busy with another task. Try again in a moment.':nm+' could not be extracted: '+e.message)}
 DLZ.run={path};DLN.ing=(DLN.ing||0)+1;
 const W=win('Extracting '+nm,'📦',480,210);
 W.body.innerHTML=`<p class='msg dlzn'>Starting…</p><div class='dsbar'><i style='width:3%'></i></div><p class='msg dlzb' style='color:#6b7280'></p><div style='text-align:right'><button class='dlzc'>Cancel</button></div>`;
 const note=W.body.querySelector('.dlzn'),bar=W.body.querySelector('.dsbar i'),sub=W.body.querySelector('.dlzb'),cb=W.body.querySelector('.dlzc');
 const show=(t,s,pct)=>{note.textContent=t;sub.textContent=s||'';if(pct!=null){bar.style.width=Math.max(3,Math.min(100,pct))+'%';if(typeof winProgress=='function')winProgress(W,pct>=100?null:pct,'zip')}};   /* (1.6.1 Part 25: and on its minimized window's taskbar button) */
 let stop=false,tot=0,bytes=0;
 cb.onclick=async()=>{if(stop)return;stop=true;cb.disabled=true;show('Stopping…','');try{await rdPost('dl/unzip?stop=1')}catch(e){}};
 try{
  /* the job: its log's last lines (the log is only its end; the whole result is read afterwards) */
  for(let k=0;;k++){await dlzWait(k?700:300);let j;try{j=await rd('job')}catch(e){continue}
   const ls=(j.log||'').split('\n');let pr=null,fl=null;
   for(let i=ls.length-1;i>=0&&(!pr||!fl||!tot);i--){const l=ls[i];let m;
    if(!pr&&(m=/^progress (\d+) (\d+) (.*)$/.exec(l)))pr=m;else if(!fl&&/^file /.test(l))fl=l.slice(5).split('\t');else if(!tot&&(m=/^total (\d+) (\d+)$/.exec(l))){tot=+m[1];bytes=+m[2]}}
   if(!stop){if(fl)show('Putting the files in place: '+fl[0].split('/').pop(),fmtB(bytes)+' unpacked',85);
    else if(pr)show('Unpacking '+pr[3].split('/').pop(),`${(+pr[1]).toLocaleString()} of ${(+pr[2]).toLocaleString()} files · about ${fmtB(bytes*pr[1]/Math.max(1,+pr[2]))} of ${fmtB(bytes)}`,3+pr[1]/Math.max(1,+pr[2])*77)}
   if(j.name!='dl-unzip'||!j.running&&j.status>=0)break}
  cb.disabled=true;
  let r;try{r=await rd('dl/unzipped')}catch(e){r={files:[],dirs:[],end:'error '+e.message}}
  const end=String(r.end||''),files=r.files||[],dirs=r.dirs||[];
  if(/^error /.test(end)||!end&&!files.length){show('Not extracted',end.slice(6)||'The extraction stopped unexpectedly.',100);no(nm+' was not extracted: '+(end.slice(6)||'the extraction stopped unexpectedly.'));return}
  if(!files.length&&!dirs.length){show(/^stopped/.test(end)?'Stopped':'Nothing to extract',/^stopped/.test(end)?'Nothing was extracted.':nm+' holds no files.',100);if(!/^stopped/.test(end))notify('Extract here',nm+' holds no files.');setTimeout(()=>{try{W.close()}catch(e){}},1500);return}
  /* the folder beside the ZIP, its sub-folders as in the ZIP */
  const P=node(parent);if(!isDir(P)){no('The folder of '+nm+' is not there any more.');return}
  const fn=dlUniq(P,dlzC(nm.replace(/\.ZIP$/i,''))),root={},dm=new Map([['',root]]);
  const dirFor=parts=>{let d=root,key='';for(const p of parts){key=key?key+'/'+p:p;if(dm.has(key)){d=dm.get(key);continue}let c=dlzC(p);if(!isDir(d[c])){if(c in d)c=dlUniq(d,c);d[c]={}}d=d[c];dm.set(key,d)}return d};
  dirs.forEach(x=>dirFor(String(x).split('/').filter(Boolean)));
  const base=DLADIR(),blocked=[],quar=[],unchecked=[];let n=0,done=0;
  for(const f of files){const parts=String(f.path).split('/').filter(Boolean);if(!parts.length)continue;const d=dirFor(parts.slice(0,-1)),NM=dlzC(parts.at(-1));let av='off';
   done++;if(done%20==1||done==files.length)show(SECC.av.on?'Checking the files with the antivirus':'Making the folder',`${done.toLocaleString()} of ${files.length.toLocaleString()} files`,85+done/files.length*15);
   if(SECC.av.on){let th;try{th=await dlScanBig(f.disk,'home',f.size,NM,false)}catch(e){unchecked.push(NM);continue}
    av=f.size>DL_AVPART*2?'part':'full';
    if(th.length){const qm=SECC.av.act=='quarantine';logEvt('av',`${qm?'Quarantined':'Blocked'} ${NM} (extracted from ${nm}): ${th.join(', ')}`);
     try{if(qm){const q=await rdPost('dl/file?name='+encodeURIComponent(f.disk)+'&from=home&to=quar'+DLAQ());QUAR.push({id:Date.now()+Math.random(),n:NM,from:[...parent,fn,...parts.slice(0,-1).map(dlzC)],th,t:Date.now(),data:dlMkRef(q.path,f.size,av),src:'download'});saveQ();quar.push(NM)}
      else{await rdPost('dl/file?name='+encodeURIComponent(f.disk)+'&from=home&to=del'+DLAQ());blocked.push(NM)}}catch(e){unchecked.push(NM)}
     continue}}
   const n2=dlUniq(d,NM);d[n2]=dlMkRef(base+f.disk,f.size,av);dlRefsAdd(base+f.disk);n++}
  P[fn]=root;fsSave();try{EXPL.forEach(x=>x())}catch(e){}
  const where=[...parent,fn].join('\\'),stopped=/^stopped/.test(end);
  const bad=(quar.length?` The antivirus quarantined ${quar.length}: ${quar.slice(0,3).join(', ')}${quar.length>3?'…':''}.`:'')+(blocked.length?` The antivirus blocked and deleted ${blocked.length}: ${blocked.slice(0,3).join(', ')}${blocked.length>3?'…':''}.`:'')+
   (unchecked.length?` ${unchecked.length} could not be checked and ${unchecked.length==1?'is':'are'} left in Downloads, to be checked again.`:'');
  show(stopped?'Stopped':'Extracted',`${n.toLocaleString()} file(s) in ${where}.${bad}`,100);
  notify(stopped?'Extraction stopped':'Extracted',`${stopped?'Stopped: ':''}${n.toLocaleString()} file(s) from ${nm} are in ${where}.${bad}`,{kind:quar.length||blocked.length?'error':'',action:()=>explorer([...parent,fn])});
  if(!bad)setTimeout(()=>{try{W.close()}catch(e){}},1500)}
 catch(e){console.warn('extract',e);show('Not extracted',e&&e.message||'',100);no(nm+' was not extracted: '+(e&&e.message||'it stopped unexpectedly.'))}
 finally{DLZ.run=null;if(typeof winProgress=='function')winProgress(W,null,'zip');
  /* the references saved (on the native desktop: in rudolfd's copy, which Oracle's page reads), then the result taken, so dl/home no
     longer says "unzip" and dlAdopt looks at the folder again (a file left unreferenced is then taken in as before) */
  try{if(typeof oaDrained=='function')await oaDrained()}catch(e){}
  try{await rdPost('dl/unzipped')}catch(e){}
  DLN.ing=Math.max(0,(DLN.ing||0)-1)}}
const dlzIs=path=>Array.isArray(path)&&isDlRef(node(path))&&ext(path.at(-1))=='ZIP';
/* File Explorer's Open with: Archive Manager for such a ZIP too (dl27.js lists only the built-in viewer and Linux apps); as Archive
   Manager, so on the native desktop it is a window of its own (launchWith, oneapp59.js) */
{const _of=openersFor;openersFor=function(path){const l=_of.apply(this,arguments);if(!dlzIs(path))return l;const k=l.findIndex(o=>o&&o[0]=='disk');
 l.splice(k+1,0,['archive','Archive Manager','📦',()=>true,p=>launchWith('archive',p)]);return l}}
/* Archive Manager given such a ZIP (dl27.js dlOpen, Open with, a default app, another window's request) */
{const _aa=archiveApp;archiveApp=function(path){if(dlzIs(path))return dlZipWin(path);return _aa.apply(this,arguments)}}
