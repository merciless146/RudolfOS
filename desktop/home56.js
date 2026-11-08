/* ===== Rudolf OS 1.5.6: your home folder on the disk (docs/plan-1.5.6.md, stage 2) =====
   For an account that keeps its files on the disk (USER.disk: in stage 2, accounts whose first sign-in is on 1.5.6 on
   a computer with RUDOLFDATA), C:\USERS\<name> is a view of /persist/accounts/<id>, kept by rudolfd (home/*,
   rudolfd-home.inc) and encrypted with the account's own key:
   - at sign-in the key (64 bytes made from the account's file key with HKDF) opens the folder; the tree is read, files
     up to 1.5 MB are loaded (text as text, the rest as data: URLs, as C: keeps them), larger ones become references
     ("rudolfdisk:" with a path in the account's folder) that the viewers open through a /dlf/ link, as large downloads;
   - saving (fsSave → homeSave) compares the home with what was last written and writes only what changed: new or
     changed files, new folders, files moved or copied in C: (references are copied on the disk), and deletions;
   - a write that finds the file changed by another program meanwhile keeps both (the desktop's copy beside it);
   - every 15 s the tree is read again, so files Linux apps save show up;
   - at sign-out, once everything is written, the folder is locked again.
   The apps keep using C: as before (node(), d[name]=value, fsSave()). Other accounts, the guest, the browser-only
   version and live sessions keep the storage they had. */
const HD={acct:null,snap:new Map(),chain:Promise.resolve(),poll:0,busy:0,gen:0};
const HD_SMALL=1.5*1048576,HD_ROOT='/persist/accounts/';
/* the six folders: C:'s names and the disk's (Linux apps see the disk's) */
const HD_TOP={DESKTOP:'Desktop',DOCUMENTS:'Documents',PICTURES:'Pictures',MUSIC:'Music',VIDEOS:'Videos',DOWNLOADS:'Downloads'};
const HD_TOPR=Object.fromEntries(Object.entries(HD_TOP).map(([a,b])=>[b,a]));
const hdOn=()=>!!(HD.acct&&USER&&USER.disk&&HOME&&!HOMELOCKED&&uid()==HD.acct);
const hdQ=()=>'acct='+encodeURIComponent(HD.acct);
/* C:'s path inside the home (["DOCUMENTS","A.TXT"]) ↔ the disk's ("Documents/A.TXT") */
const hdRel=parts=>parts.map((x,i)=>i==0&&HD_TOP[x]?HD_TOP[x]:x).join('/');
const hdParts=rel=>rel.split('/').map((x,i)=>i==0&&HD_TOPR[x]?HD_TOPR[x]:x);
async function hdKeyHex(raw,id){const k=await crypto.subtle.importKey('raw',raw||FKRAW,'HKDF',false,['deriveBits']);
 const b=await crypto.subtle.deriveBits({name:'HKDF',hash:'SHA-512',salt:new TextEncoder().encode('rudolf-os home folder'),info:new TextEncoder().encode(id||uid())},k,512);
 return [...new Uint8Array(b)].map(x=>x.toString(16).padStart(2,'0')).join('')}
async function hdSupported(){try{return !!(await rd('home')).supported}catch(e){return false}}
/* the bytes of a value from C: */
function hdBytes(v){const m=/^data:[^,]*;base64,/.exec(v);if(m){const b=atob(v.slice(m[0].length));const u=new Uint8Array(b.length);for(let i=0;i<b.length;i++)u[i]=b.charCodeAt(i);return u}return new TextEncoder().encode(v)}
/* a file from the disk as C: keeps it: text, or a data: URL */
function hdValue(name,u8){if(!u8.includes(0))try{return new TextDecoder('utf-8',{fatal:true}).decode(u8)}catch(e){}
 const mt=(typeof MIMEX!='undefined'&&MIMEX[ext(name)])||'application/octet-stream';let s='';for(let i=0;i<u8.length;i+=32768)s+=String.fromCharCode.apply(null,u8.subarray(i,i+32768));return 'data:'+mt+';base64,'+btoa(s)}
const hdRef=(rel,s,acct)=>dlMkRef(HD_ROOT+(acct||HD.acct)+'/'+rel,s,'none');
const hdRefRel=(v,acct)=>{const i=isDlRef(v)&&dlRefInfo(v);const P=HD_ROOT+(acct||HD.acct)+'/';return i&&i.p.startsWith(P)?i.p.slice(P.length):null};
async function hdRead(rel,acct){const r=await rd('home/read?acct='+encodeURIComponent(acct||HD.acct)+'&path='+encodeURIComponent(rel));return new Uint8Array(await r.arrayBuffer())}
/* the home from the disk's tree: folders, small files loaded, large ones as references; versions into the snapshot */
async function hdLoadTree(acct){acct=acct||HD.acct;const t=await rd('home/tree?acct='+encodeURIComponent(acct));const obj={},snap=new Map();
 const items=(t.items||[]).filter(i=>!i.p.split('/').some(x=>x.startsWith('.')));
 items.sort((a,b)=>a.p.length-b.p.length);
 const put=(parts,v)=>{let o=obj;for(const x of parts.slice(0,-1)){if(!isDir(o[x]))o[x]={};o=o[x]}o[parts.at(-1)]=v};
 for(const i of items)if(i.d){put(hdParts(i.p),{});snap.set(i.p,{d:1})}
 const files=items.filter(i=>!i.d);let k=0;
 const work=async()=>{while(k<files.length){const i=files[k++];const parts=hdParts(i.p);let v;
  if(i.s>HD_SMALL)v=hdRef(i.p,i.s,acct);else{try{v=hdValue(parts.at(-1),await hdRead(i.p,acct))}catch(e){v=hdRef(i.p,i.s,acct)}}
  put(parts,v);snap.set(i.p,{v,ver:i.v})}};
 await Promise.all([work(),work(),work(),work()]);
 HOMESUB.forEach(s=>{if(!isDir(obj[s]))obj[s]={}});
 return {obj,snap}}
/* everything in the home now: rel → value (folders: {d:1}) */
function hdWalk(o,parts,out){for(const k in o){const v=o[k],p=parts.concat(k),rel=hdRel(p);if(isDir(v)){out.set(rel,{d:1,o:v});hdWalk(v,p,out)}else if(typeof v=='string')out.set(rel,{v,o,k})}return out}
/* write what changed since the snapshot (the comparison is made now; the writes follow in order) */
function hdSync(){if(!hdOn())return HD.chain;return hdSyncFor(HD.acct,HOME,HD.snap)}
/* stage 4: any account's home (the signed-in one, or one an administrator has open) */
function hdSyncFor(acct,home,old){const now=hdWalk(home,[],new Map());
 const ops=[];
 /* folders first (by depth), then files, then what went away (deepest first; a folder's files go with it) */
 [...now.keys()].filter(r=>now.get(r).d&&!old.has(r)).sort((a,b)=>a.length-b.length).forEach(r=>ops.push({t:'mkdir',r}));
 for(const[r,x]of now){if(x.d)continue;const o=old.get(r);if(o&&!o.d&&o.v===x.v)continue;
  const src=hdRefRel(x.v,acct);
  /* (stage 4: back from the Trash: moved, not copied) */
  if(src!=null){if(src===r){old.set(r,{v:x.v,ver:o&&o.ver});continue}ops.push({t:src.startsWith('.Trash/')?'move':'copy',r,from:src,x})}
  else if(isDlRef(x.v))ops.push({t:'adopt',r,x});
  else ops.push({t:'write',r,x,base:o&&!o.d?o.ver:'new'})}
 const gone=[...old.keys()].filter(r=>!now.has(r)).sort((a,b)=>b.length-a.length).filter((r,i,a)=>!a.some(p=>p!==r&&r.startsWith(p+'/')));
 /* stage 4: what goes holding large files (references) goes to the account's .Trash on the disk, so the Trash can
    still give it back; the rest is deleted (small files are kept in the Trash's own record, as before) */
 gone.forEach(r=>{const refs=[...old].some(([k,x])=>(k===r||k.startsWith(r+'/'))&&!x.d&&hdRefRel(x.v,acct)!=null);
  ops.push(refs?{t:'trash',r,stamp:Date.now().toString(36)+Math.random().toString(36).slice(2,6)}:{t:'del',r,d:!!old.get(r).d})});
 if(!ops.length)return HD.chain;
 /* the snapshot takes the new state at once, so the next comparison does not repeat these; a failed write is put back */
 for(const op of ops){if(op.t=='mkdir')old.set(op.r,{d:1});else if(op.t=='del'||op.t=='trash'){[...old.keys()].forEach(k=>{if(k===op.r||k.startsWith(op.r+'/'))old.delete(k)})}else old.set(op.r,{v:op.x.v,ver:null})}
 const q='acct='+encodeURIComponent(acct);
 HD.chain=HD.chain.then(async()=>{let failed=0;
  for(const op of ops){   /* (written even after sign-out: the account and its changes were taken when compared) */
   try{
    if(op.t=='mkdir')await rdPost('home/mkdir?'+q+'&path='+encodeURIComponent(op.r));
    else if(op.t=='del')await rdPost('home/delete?'+q+'&path='+encodeURIComponent(op.r));
    else if(op.t=='trash'){const to='.Trash/'+op.stamp+'/'+op.r.split('/').pop();await rdPost('home/rename?'+q+'&from='+encodeURIComponent(op.r)+'&to='+encodeURIComponent(to));hdTrashRepoint(acct,op.r,to)}
    else if(op.t=='move'){await rdPost('home/rename?'+q+'&from='+encodeURIComponent(op.from)+'&to='+encodeURIComponent(op.r)+'&replace=1');
     const nv=dlMkRef(HD_ROOT+acct+'/'+op.r,(dlRefInfo(op.x.v)||{}).s||0,'none');if(op.x.o[op.x.k]===op.x.v)op.x.o[op.x.k]=nv;old.set(op.r,{v:nv,ver:null})}
    else if(op.t=='copy'){await rdPost('home/copy?'+q+'&from='+encodeURIComponent(op.from)+'&to='+encodeURIComponent(op.r)+'&replace=1');
     const nv=dlMkRef(HD_ROOT+acct+'/'+op.r,(dlRefInfo(op.x.v)||{}).s||0,'none');if(op.x.o[op.x.k]===op.x.v)op.x.o[op.x.k]=nv;old.set(op.r,{v:nv,ver:null})}
    else if(op.t=='adopt'){const i=dlRefInfo(op.x.v);const j=await rdPost('home/adopt?'+q+'&from='+encodeURIComponent(i.p)+'&to='+encodeURIComponent(op.r));
     const nv=dlMkRef(HD_ROOT+acct+'/'+op.r,i.s||0,i.av||'none');if(op.x.o[op.x.k]===op.x.v)op.x.o[op.x.k]=nv;old.set(op.r,{v:nv,ver:j.v||null})}
    else{const b=hdBytes(op.x.v);let j;
     try{j=await rd('home/write?'+q+'&path='+encodeURIComponent(op.r)+'&mkdirs=1'+(op.base?'&base='+encodeURIComponent(op.base):''),{method:'POST',body:b})}
     catch(e){if(!/changed meanwhile|already exists/.test(e.message))throw e;
      /* another program changed it: keep both */
      const nm=op.r.split('/').pop(),dot=nm.lastIndexOf('.'),alt=op.r.slice(0,op.r.length-nm.length)+(dot>0?nm.slice(0,dot)+' (desktop copy)'+nm.slice(dot):nm+' (desktop copy)');
      j=await rd('home/write?'+q+'&path='+encodeURIComponent(alt)+'&mkdirs=1',{method:'POST',body:b});
      notify('Two versions of '+nm,'Another program changed it while it was open here. Both are kept: the desktop\'s is "'+alt.split('/').pop()+'".',{kind:'warn'});
      old.delete(op.r);HD.poll=0}
     const s=old.get(op.r);if(s&&s.v===op.x.v)s.ver=j.v}
   }catch(e){failed++;console.warn('home',op.t,op.r,e);if(op.t!='del'&&op.t!='trash')old.delete(op.r)}}
  if(failed){HD.fails=(HD.fails||0)+1;if(HD.fails==1||HD.fails%20==0)notify('Some files were not saved',failed+' change(s) could not be written to the disk; they are tried again at the next save.',{kind:'error'});fsSave()}else HD.fails=0});
 return HD.chain}
/* files other programs changed, made or deleted (a Linux app on the same folders) */
async function hdRefresh(){if(!hdOn()||HD.busy)return;HD.busy=1;const gen=HD.gen;
 try{await HD.chain;const t=await rd('home/tree?'+hdQ());if(gen!==HD.gen||!hdOn())return;
  const disk=new Map((t.items||[]).filter(i=>!i.p.split('/').some(x=>x.startsWith('.'))).map(i=>[i.p,i]));const now=hdWalk(HOME,[],new Map());let ch=0;
  for(const[r,i]of disk){const s=HD.snap.get(r);if(i.d){if(!s){const parts=hdParts(r);let o=HOME;for(const x of parts){if(!isDir(o[x]))o[x]={};o=o[x]}HD.snap.set(r,{d:1});ch++}continue}
   if(s&&s.ver===i.v)continue;const n=now.get(r);if(s&&n&&n.v!==s.v)continue;   /* changed here too: the next save decides */
   const parts=hdParts(r);let v;if(i.s>HD_SMALL)v=hdRef(r,i.s);else{try{v=hdValue(parts.at(-1),await hdRead(r))}catch(e){continue}}
   let o=HOME;for(const x of parts.slice(0,-1)){if(!isDir(o[x]))o[x]={};o=o[x]}o[parts.at(-1)]=v;HD.snap.set(r,{v,ver:i.v});ch++}
  for(const[r,s]of[...HD.snap]){if(disk.has(r))continue;const n=now.get(r);if(!n)continue;if(!s.d&&n.v!==s.v)continue;
   const parts=hdParts(r);let o=HOME;for(const x of parts.slice(0,-1)){o=o&&o[x]}if(o&&isDir(o)){delete o[parts.at(-1)];ch++}[...HD.snap.keys()].forEach(k=>{if(k===r||k.startsWith(r+'/'))HD.snap.delete(k)})}
  if(ch){try{EXPL.forEach(f=>f())}catch(e){}}
 }catch(e){}finally{HD.busy=0}}
setInterval(()=>{if(hdOn()&&document.visibilityState!='hidden')hdRefresh()},15000);
/* sign-in: which accounts keep their files on the disk, and opening the folder */
/* stage 3: an account from before 1.5.6 moves its home to the disk, once, in the background after signing in:
   1. its folder is opened (without the Linux binds yet);
   2. every folder and file is written (small ones) or moved in (large downloads), replacing what an earlier attempt
      left;
   3. everything is read back and compared (SHA-256 of each file; the size of moved downloads);
   4. only then does the account switch (USER.disk), taking what was written as the snapshot, so changes made during
      the move are written next; the Linux binds follow.
   The old record in the browser's storage is kept: if 1.5.6 does not start, the update goes back to 1.5.5, which still
   has it. It is deleted once the account has been on the disk for 7 days. A move that fails changes nothing and is
   tried again at the next sign-in. */
const HD_KEEP=7*864e5;
const hdSha=async u8=>[...new Uint8Array(await crypto.subtle.digest('SHA-256',u8))].map(x=>x.toString(16).padStart(2,'0')).join('');
async function hdMove(a){const acct=uid(),q='acct='+encodeURIComponent(acct),home=HOME;if(!home||HOMELOCKED)return;
 const prog=(t)=>{try{notify('Moving your files to the disk',t,{quiet:true,toast:4000,tag:'hdmove'})}catch(e){}};
 try{
  if(!FKRAW){FKRAW=crypto.getRandomValues(new Uint8Array(32));a.fk=await acEnc(PKEY,FKRAW);KEY=await acFkKey(FKRAW);saveUser();homeSave()}
  const key=await hdKeyHex();await rd('home/unlock?'+q,{method:'POST',body:key});
  const all=hdWalk(home,[],new Map()),dirs=[...all].filter(([,x])=>x.d).map(([r])=>r).sort((x,y)=>x.length-y.length),files=[...all].filter(([,x])=>!x.d);
  for(const r of dirs)await rdPost('home/mkdir?'+q+'&path='+encodeURIComponent(r));
  const snap=new Map(dirs.map(r=>[r,{d:1}])),sums=new Map();let n=0,t0=Date.now(),bytes=0;
  for(const[r,x]of files){
   if(isDlRef(x.v)){const i=dlRefInfo(x.v);
    if(i&&i.p.startsWith(HD_ROOT+acct+'/')){snap.set(r,{v:x.v,ver:null});sums.set(r,{s:i.s})}
    else if(i){const j=await rdPost('home/adopt?'+q+'&from='+encodeURIComponent(i.p)+'&to='+encodeURIComponent(r));const nv=dlMkRef(HD_ROOT+acct+'/'+r,i.s||0,i.av||'none');if(x.o[x.k]===x.v)x.o[x.k]=nv;snap.set(r,{v:nv,ver:j.v||null});sums.set(r,{s:i.s})}}
   else{const b=hdBytes(x.v);const j=await rd('home/write?'+q+'&path='+encodeURIComponent(r)+'&mkdirs=1',{method:'POST',body:b});snap.set(r,{v:x.v,ver:j.v});sums.set(r,{h:await hdSha(b),s:b.length});bytes+=b.length}
   if(++n%200==0||Date.now()-t0>5000){t0=Date.now();prog(n.toLocaleString()+' of '+files.length.toLocaleString()+' files…')}}
  /* read back and compare */
  const t=await rd('home/tree?'+q),disk=new Map((t.items||[]).map(i=>[i.p,i]));const bad=[];
  for(const[r,w]of sums){const i=disk.get(r);if(!i||i.d||(w.s!=null&&i.s!==w.s)){bad.push(r);continue}
   if(w.h){const back=await rd('home/read?'+q+'&path='+encodeURIComponent(r)).then(x=>x.arrayBuffer()).then(x=>new Uint8Array(x)).catch(()=>null);if(!back||await hdSha(back)!==w.h)bad.push(r)}}
  for(const r of dirs){const i=disk.get(r);if(!i||!i.d)bad.push(r)}
  if(bad.length)throw new Error(bad.length+' item(s) did not arrive as they were ('+bad.slice(0,3).join(', ')+')');
  if(USER!==a||HOME!==home)return;   /* signed out meanwhile: done again next time */
  /* switch: what was written is the snapshot; the Linux binds */
  a.disk=1;a.oldHome=Date.now();saveUser();
  HD.gen++;HD.acct=acct;HD.snap=snap;
  for(const[r,i]of disk){const s=snap.get(r);if(s&&!s.d&&s.ver==null&&!i.d)s.ver=i.v}
  await rd('home/unlock?'+q+'&session=1',{method:'POST',body:key}).catch(e=>console.warn('home binds',e));
  hdSync();
  logEvt('sys',`${a.n}'s files moved to the disk (${files.length} files, ${dirs.length} folders)`);
  notify('Your files are on the disk','They are kept as files now, encrypted with your account\'s key; Linux apps see your Documents, Pictures, Music and Videos. A copy stays in the old place for 7 days.',{tag:'hdmove'});
 }catch(e){console.warn('home move',e);logEvt('sys',`${a.n}'s files could not be moved to the disk: ${e.message}`);
  notify('Your files stay where they were','Moving them to the disk did not work ('+e.message+'). Nothing was lost; it is tried again at your next sign-in.',{kind:'warn',tag:'hdmove'})}}
{const _hl=homeLoad;homeLoad=async function(pw){
 const a=USER;
 if(RD&&a&&a.r!='guest'&&!a.disk&&!a.fk&&!(await kvGet('home:'+uid()))&&await hdSupported()){a.disk=1;saveUser()}   /* stage 2: new accounts */
 if(RD&&a&&a.r!='guest'&&!a.disk){const r=await _hl.apply(this,arguments);   /* stage 3: accounts from before 1.5.6 */
  if(HOME&&!HOMELOCKED&&await hdSupported())setTimeout(()=>{if(USER===a&&!HD.moving){HD.moving=1;hdMove(a).finally(()=>{HD.moving=0})}},1500);
  return r}
 if(!(RD&&a&&a.r!='guest'&&a.disk))return _hl.apply(this,arguments);
 /* 7 days on the disk: the old record goes */
 if(a.oldHome&&Date.now()-a.oldHome>HD_KEEP){try{await kvDel('home:'+uid())}catch(e){}delete a.oldHome;saveUser()}
 if(!FKRAW){FKRAW=crypto.getRandomValues(new Uint8Array(32));a.fk=await acEnc(PKEY,FKRAW);KEY=await acFkKey(FKRAW);saveUser()}
 HD.gen++;HD.acct=uid();
 let loaded=null,made=false;
 try{made=(await rd('home/unlock?'+hdQ()+'&session=1',{method:'POST',body:await hdKeyHex()})).message=='made';loaded=await hdLoadTree()}
 catch(e){HD.acct=null;notify('Your files could not be opened',e.message+' Nothing was changed; a new empty home is used for this session.',{kind:'error'});HOMELOCKED=true;HOME=defaultHome();fs['C:'].USERS[uname()]=HOME;return}
 /* stage 4: a folder made just now for an account that has a record (a backup restored here, or this account on another
    computer): the record's files are moved in, as for an account from before 1.5.6 */
 if(made&&await kvGet('home:'+uid())){HD.acct=null;a.disk=0;saveUser();const r=await _hl.apply(this,arguments);
  if(HOME&&!HOMELOCKED)setTimeout(()=>{if(USER===a&&!HD.moving){HD.moving=1;hdMove(a).finally(()=>{HD.moving=0})}},1500);return r}
 HD.snap=loaded.snap;HOME=loaded.obj;fs['C:'].USERS[uname()]=HOME;
 setTimeout(hdTrashSweep,5000);
 /* a new account's folder: the welcome file, as a home in C: had */
 if(![...HD.snap.values()].some(x=>!x.d))HOME.DOCUMENTS['WELCOME.TXT']=defaultHome().DOCUMENTS['WELCOME.TXT'];
 hdSync()}}
{const _hs=homeSave;homeSave=function(){if(USER&&USER.disk&&RD){if(hdOn())hdSync();return}return _hs.apply(this,arguments)}}
{const _hu=homeUnload;homeUnload=function(){const was=hdOn()?HD.acct:null;if(was)hdSync();const r=_hu.apply(this,arguments);
 if(was){HD.gen++;const q='acct='+encodeURIComponent(was);const c=HD.chain;HD.acct=null;HD.snap=new Map();HD.chain=c.then(()=>rdPost('home/lock?'+q)).catch(e=>console.warn('home lock',e))}return r}}
/* large files in the home open through a /dlf/ link, as large downloads (dl27.js) */
{const _dl=dlLink;dlLink=async function(v){const i=dlRefInfo(v);if(i&&i.p.startsWith(HD_ROOT)){const rest=i.p.slice(HD_ROOT.length),sl=rest.indexOf('/');
  const j=await rd('home/link?acct='+encodeURIComponent(rest.slice(0,sl))+'&path='+encodeURIComponent(rest.slice(sl+1)));return j.url}return _dl.apply(this,arguments)}}

/* ---------- stage 4 ---------- */
/* the Trash's entries for what went to .Trash/<stamp>/ point there now (so Restore finds the files) */
function hdTrashRepoint(acct,from,to){const P=HD_ROOT+acct+'/',a=P+from;let ch=0;
 const fix=v=>{if(typeof v=='string'&&isDlRef(v)){const i=dlRefInfo(v);if(i&&(i.p===a||i.p.startsWith(a+'/'))){ch++;return dlMkRef(P+to+i.p.slice(a.length),i.s,i.av)}return v}
  if(isDir(v)){for(const k in v)v[k]=fix(v[k])}return v};
 if(acct===HD.acct&&Array.isArray(TR)){TR.forEach(e=>{e.v=fix(e.v)});if(ch)try{trSave()}catch(e){}}}
/* .Trash/<stamp> folders nothing points at any more (emptied from the Trash) are deleted */
async function hdTrashSweep(){if(!hdOn())return;const acct=HD.acct;
 try{await HD.chain;const l=await rd('home/list?acct='+encodeURIComponent(acct)+'&path=.Trash');const used=new Set();
  const look=v=>{if(typeof v=='string'&&isDlRef(v)){const m=/\/\.Trash\/([^/]+)\//.exec((dlRefInfo(v)||{}).p||'');if(m)used.add(m[1])}else if(isDir(v))for(const k in v)look(v[k])};
  (TR||[]).forEach(e=>look(e.v));look(HOME);
  for(const i of l.items||[]){const st=i.p.split('/').pop();if(i.d&&!used.has(st))await rdPost('home/delete?acct='+encodeURIComponent(acct)+'&path='+encodeURIComponent(i.p)).catch(()=>{})}}catch(e){}}
{const _ts=trSave;trSave=function(){const r=_ts.apply(this,arguments);if(hdOn()){clearTimeout(HD.sw);HD.sw=setTimeout(hdTrashSweep,3000)}return r}}
/* backup: the signed-in account's home goes into the backup as the record a backup always had (encrypted with the
   file key), so "Back up everything" and a restore on another computer keep working; large files are not in it, as
   large downloads never were */
{const _bb=backupBlob;backupBlob=async function(){if(hdOn()&&KEY){await HD.chain;const json=JSON.stringify(HOME);const iv=crypto.getRandomValues(new Uint8Array(12));
  const ct=await crypto.subtle.encrypt({name:'AES-GCM',iv},KEY,new TextEncoder().encode(json));await kvPutNow('home:'+uid(),{v:1,k:'f',iv,ct});
  /* (the record is the backup's copy; a newer one than the move's, which is fine: the disk is what is used) */}
  return _bb.apply(this,arguments)}}
/* administrators: another account's home on the disk, opened with its file key (unwrapped by acct50.js) */
{const _oh=acOpenHome;acOpenHome=async function(o,f){if(!(RD&&o&&o.disk&&o.id))return _oh.apply(this,arguments);
 try{await rd('home/unlock?acct='+encodeURIComponent(o.id),{method:'POST',body:await hdKeyHex(f,o.id)});const t=await hdLoadTree(o.id);
  const nm=acUname(o);fs['C:'].USERS[nm]=t.obj;AC_OTHERS.set(o.id,{a:o,key:await acFkKey(f),name:nm,disk:1,snap:t.snap})}catch(e){console.warn('home (other)',e)}}}
{const _so=acSaveOthers;acSaveOthers=function(){const keep=[];for(const[id,x]of AC_OTHERS)if(x.disk){keep.push([id,x]);AC_OTHERS.delete(id)}
 try{_so.apply(this,arguments)}finally{keep.forEach(([id,x])=>AC_OTHERS.set(id,x))}
 for(const[id,x]of keep){const obj=fs['C:'].USERS[x.name];if(isDir(obj))hdSyncFor(id,obj,x.snap)}}}
{const _ac=acClose;acClose=function(){const locks=[];for(const[id,x]of AC_OTHERS)if(x.disk){const obj=fs['C:'].USERS[x.name];if(isDir(obj))hdSyncFor(id,obj,x.snap);locks.push(id)}
 const r=_ac.apply(this,arguments);if(locks.length){const me=HD.acct;HD.chain=HD.chain.then(async()=>{for(const id of locks)if(id!==me)await rdPost('home/lock?acct='+encodeURIComponent(id)).catch(()=>{})})}return r}}
