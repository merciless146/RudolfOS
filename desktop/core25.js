/* ===== V25 core: rudolfd link, storage, accounts, encryption, lock screen, elevation, guest, parental controls, backup ===== */
window.__v25=1;
const RVER='1.2.2',RVERDATE='2026-10-18';
/* ---------- rudolfd (only present on the bootable build) ---------- */
const RD=window.RUDOLFD||null;
async function rd(path,opt){opt=opt||{};if(!RD)throw new Error('This needs the installed Rudolf OS (bootable disc or USB).');
 const r=await fetch('/api/'+path,{method:opt.method||(opt.body!=null?'POST':'GET'),headers:{'X-Rudolf-Token':RD.token},body:opt.body});
 const ct=r.headers.get('content-type')||'';if(ct.includes('json')){const j=await r.json();if(!j.ok)throw new Error(j.error||'Request failed');return j}
 if(!r.ok)throw new Error('HTTP '+r.status);return r}
const rdPost=(p,body)=>rd(p,{method:'POST',body});
if(RD){const hb=()=>rdPost('heartbeat').catch(()=>{});hb();setInterval(hb,8000);
 setInterval(async()=>{try{const e=await rd('events');if(/^power /m.test(e.events||''))A.off&&A.off();if(typeof devEvents=='function')devEvents(e.events)}catch(x){}},2500)}
/* ---------- notifications used by every V25 module (the shell adds a centre on top) ---------- */
function notify(title,body,opt){opt=opt||{};try{if(window.__notifCentre)return __notifCentre.add(title,body,opt)}catch(e){}
 const d=document.createElement('div');d.className='rtoast';d.textContent=title+(body?' — '+body:'');document.body.appendChild(d);setTimeout(()=>d.remove(),4000)}
/* ---------- IndexedDB key/value store ---------- */
var IDB={db:null};
IDB.open=new Promise(res=>{try{const q=indexedDB.open('rudolfos',1);q.onupgradeneeded=()=>q.result.createObjectStore('kv');q.onsuccess=()=>{IDB.db=q.result;res(true)};q.onerror=()=>res(false);q.onblocked=()=>res(false)}catch(e){res(false)}});
/* ---------- the databases, mirrored in rudolfd (Rudolf OS 1.2) ----------
   Files (drive C:), game ROMs and saves live in IndexedDB. The desktop runs in the shell (Oracle's real tabs) or as
   a plain page, and the two have separate storage, so every write also goes to rudolfd's mirror (/api/idbm/), one
   record per key. At start a desktop whose copy is older than the mirror loads the mirror first; the first desktop
   to use the mirror copies everything into it and then tells rudolfd the shell may be used. The generation this
   copy has seen is kept in the database itself (IDBM_GEN), not in localStorage, which rudolfd shares between them. */
const IDBM_STORES=[['rudolfos','kv'],['bd95vault','rom'],['bd95vault','sav']],IDBM_GEN='__idbm_gen';
const IDBM={on:false,queue:new Map(),busy:false,gen:0};
function idbmName(db,st,key){/* FNV-1a, 64 bits, of the record's place: the file name in the mirror */
 let h=0xcbf29ce484222325n;const b=new TextEncoder().encode(db+'\0'+st+'\0'+JSON.stringify(key));
 for(const c of b){h^=BigInt(c);h=(h*0x100000001b3n)&0xffffffffffffffffn}return h.toString(16).padStart(16,'0')}
/* values -> JSON: binary data becomes base64 (copied at once, so later changes do not leak in) */
function idbmEnc(v,jobs){if(v===null||typeof v!='object')return v;
 const bin=(tag,u8,extra)=>{const o=Object.assign({$:tag,b:''},extra);jobs.push([o,new Blob([u8])]);return o};
 if(v instanceof ArrayBuffer)return bin('ab',new Uint8Array(v));
 if(ArrayBuffer.isView(v))return bin('ta',new Uint8Array(v.buffer,v.byteOffset,v.byteLength),{t:v.constructor.name});
 if(v instanceof Blob){const o={$:'blob',t:v.type,n:v.name||null,b:''};jobs.push([o,v]);return o}
 if(v instanceof Date)return{$:'d',v:+v};
 if(v instanceof Map)return{$:'map',v:[...v].map(([a,b])=>[idbmEnc(a,jobs),idbmEnc(b,jobs)])};
 if(v instanceof Set)return{$:'set',v:[...v].map(x=>idbmEnc(x,jobs))};
 if(Array.isArray(v))return v.map(x=>idbmEnc(x,jobs));
 const o={};for(const k of Object.keys(v))o[k]=idbmEnc(v[k],jobs);return'$'in v?{$:'o',v:o}:o}
const idbmB64=blob=>new Promise((res,rej)=>{const r=new FileReader();r.onload=()=>res(String(r.result).replace(/^data:[^,]*,/,''));r.onerror=()=>rej(r.error);r.readAsDataURL(blob)});
async function idbmJSON(db,st,key,v){const jobs=[],enc=idbmEnc(v,jobs);for(const[o,b]of jobs)o.b=await idbmB64(b);return JSON.stringify({db,store:st,key,v:enc})}
async function idbmDec(v){if(v===null||typeof v!='object')return v;if(Array.isArray(v)){const a=[];for(const x of v)a.push(await idbmDec(x));return a}
 const bytes=async b=>new Uint8Array(await (await fetch('data:application/octet-stream;base64,'+b)).arrayBuffer());
 switch(v.$){case'ab':return(await bytes(v.b)).buffer;case'ta':{const u=await bytes(v.b),C=self[v.t]||Uint8Array;return new C(u.buffer,0,u.byteLength/(C.BYTES_PER_ELEMENT||1))}
  case'blob':{const u=await bytes(v.b);return v.n?new File([u],v.n,{type:v.t}):new Blob([u],{type:v.t})}case'd':return new Date(v.v);
  case'map':{const m=new Map();for(const[a,b]of v.v)m.set(await idbmDec(a),await idbmDec(b));return m}case'set':{const t=new Set();for(const x of v.v)t.add(await idbmDec(x));return t}
  case'o':v=v.v}const o={};for(const k of Object.keys(v))o[k]=await idbmDec(v[k]);return o}
/* writes go out one at a time; several writes to the same record send only the newest */
function idbmQueue(db,st,key,v,del){if(!IDBM.on||(db=='rudolfos'&&key===IDBM_GEN))return;IDBM.queue.set(idbmName(db,st,key),{db,st,key,v,del});idbmPump()}
async function idbmPump(){if(IDBM.busy)return;IDBM.busy=true;try{while(IDBM.queue.size){const[f,r]=IDBM.queue.entries().next().value;IDBM.queue.delete(f);
 try{const body=r.del?'':await idbmJSON(r.db,r.st,r.key,r.v);const j=await rdPost('idbm/'+(r.del?'del':'put')+'?f='+f,body);if(j&&j.gen)idbmSetGen(j.gen)}catch(e){console.warn('mirror',e)}}}finally{IDBM.busy=false}}
function idbmRaw(db,st,mode,fn){return new Promise((res,rej)=>{const q=indexedDB.open(db,1);q.onupgradeneeded=()=>{for(const[d,s]of IDBM_STORES)if(d==db&&!q.result.objectStoreNames.contains(s))q.result.createObjectStore(s)};
 q.onsuccess=()=>{const t=q.result.transaction(st,mode);let out;t.oncomplete=()=>{q.result.close();res(out)};t.onerror=t.onabort=()=>{q.result.close();rej(t.error)};out=fn(t.objectStore(st))};q.onerror=()=>rej(q.error)})}
function idbmSetGen(g){IDBM.gen=g;if(IDB.db)try{IDB.db.transaction('kv','readwrite').objectStore('kv').put(g,IDBM_GEN)}catch(e){}}
async function idbmSync(){
 const meta=await rd('idbm/meta');
 const local=await new Promise(r=>{try{const t=IDB.db.transaction('kv').objectStore('kv').get(IDBM_GEN);t.onsuccess=()=>r(+t.result||0);t.onerror=()=>r(0)}catch(e){r(0)}});
 if(meta.gen>local){/* the mirror is newer (the other storage was used last): load it */
  const list=await rd('idbm/list');document.documentElement.classList.add('idbm-loading');
  for(const[db,st]of IDBM_STORES)await idbmRaw(db,st,'readwrite',s=>s.clear());
  for(const f of list.items){try{const r=await (await fetch('/api/idbm/get?f='+f,{headers:{'X-Rudolf-Token':RD.token}})).json();const v=await idbmDec(r.v);await idbmRaw(r.db,r.store,'readwrite',s=>s.put(v,r.key))}catch(e){console.warn('mirror load',f,e)}}
  IDBM.gen=list.gen;await new Promise(r=>{const t=IDB.db.transaction('kv','readwrite');t.objectStore('kv').put(list.gen,IDBM_GEN);t.oncomplete=r;t.onerror=r});
  document.documentElement.classList.remove('idbm-loading');
 }else if(!meta.ready){/* first use of the mirror: copy everything into it */
  for(const[db,st]of IDBM_STORES){const keys=await idbmRaw(db,st,'readonly',s=>s.getAllKeys()).then(r=>r.result||[]).catch(()=>[]);
   for(const k of keys){if(db=='rudolfos'&&k===IDBM_GEN)continue;const v=await idbmRaw(db,st,'readonly',s=>s.get(k)).then(r=>r.result);
    const j=await rdPost('idbm/put?f='+idbmName(db,st,k),await idbmJSON(db,st,k,v));if(j&&j.gen)IDBM.gen=j.gen}}
  if(IDBM.gen)idbmSetGen(IDBM.gen);await rdPost('idbm/ready');IDBM.justCopied=true;
 }else IDBM.gen=local;
 IDBM.on=true}
IDB.ready=IDB.open.then(ok=>ok&&RD?idbmSync().catch(e=>{console.warn('mirror',e);IDBM.on=true}).then(()=>ok):ok);
function kvGet(k){return IDB.ready.then(()=>IDB.db?new Promise(r=>{try{const t=IDB.db.transaction('kv').objectStore('kv').get(k);t.onsuccess=()=>r(t.result);t.onerror=()=>r(undefined)}catch(e){r(undefined)}}):undefined)}
function kvPutNow(k,v){/* synchronous structured clone at call time; completes in the background */
 if(!IDB.db)return Promise.resolve(false);return new Promise(r=>{try{const tx=IDB.db.transaction('kv','readwrite');tx.objectStore('kv').put(v,k);idbmQueue('rudolfos','kv',k,v);tx.oncomplete=()=>r(true);tx.onerror=tx.onabort=()=>{storageFull();r(false)}}catch(e){storageFull();r(false)}})}
const kvSet=(k,v)=>IDB.ready.then(()=>kvPutNow(k,v));
function kvDel(k){return IDB.ready.then(()=>IDB.db&&new Promise(r=>{try{const tx=IDB.db.transaction('kv','readwrite');tx.objectStore('kv').delete(k);idbmQueue('rudolfos','kv',k,null,true);tx.oncomplete=()=>r(true);tx.onerror=()=>r(false)}catch(e){r(false)}}))}
let lastFullWarn=0;function storageFull(){if(Date.now()-lastFullWarn<20000)return;lastFullWarn=Date.now();notify('Storage is full','Some changes could not be saved. Delete large files or empty the Trash.',{kind:'error'})}
try{navigator.storage&&navigator.storage.persist&&navigator.storage.persist().catch(()=>{})}catch(e){}
async function storageInfo(){let e={};try{e=await navigator.storage.estimate()}catch(x){}return{used:e.usage||0,quota:e.quota||0}}
/* ---------- file system persistence ---------- */
var FSLOADED=false,HOME=null,KEY=null,fsTimer=0,fsDirty=false,homeChain=Promise.resolve();
const HOMESUB=['DESKTOP','DOCUMENTS','PICTURES','MUSIC','VIDEOS','DOWNLOADS'];
const uname=()=>USER?String(USER.n).toUpperCase().replace(/[\\\/:*?"<>|]/g,'_').slice(0,40):'';
const uid=()=>USER?(USER.id||'u-'+String(USER.n).toLowerCase()):'nobody';
const homePath=sub=>USER?['C:','USERS',uname(),...(sub?[sub]:[])]:['C:'];
function ensureSysFolders(){const C=fs['C:']||(fs['C:']={});if(!isDir(C.USERS))C.USERS={};if(!isDir(C.WINDOWS))C.WINDOWS={};if(!isDir(C.PROGRAMS))C.PROGRAMS={}}
const FSREADY=(async()=>{await IDB.ready;if(!IDB.db){FSLOADED=true;return}
 const sys=await kvGet('fs:system');
 if(sys&&sys['C:']){fs=sys}
 else{ensureSysFolders();kvPutNow('fs:system',fs)} /* first start of V25: migrate the old localStorage drive */
 ensureSysFolders();FSLOADED=true;
 try{localStorage.removeItem('blockdos95')}catch(e){}
 const m=await kvGet('meta');if(m)META=m;metaScan(true);EXPL.forEach(f=>f())})();
function fsSave(){fsDirty=true;clearTimeout(fsTimer);fsTimer=setTimeout(fsFlush,300);return true}
function fsFlush(){if(!fsDirty)return;fsDirty=false;
 if(!IDB||!IDB.db||!FSLOADED){try{localStorage.setItem('blockdos95',JSON.stringify(fs))}catch(e){storageFull()}return}
 const C=fs['C:'],U=C.USERS,stub={};if(isDir(U))for(const k in U)stub[k]={};
 C.USERS=stub;try{kvPutNow('fs:system',fs)}catch(e){storageFull()}finally{C.USERS=U}
 homeSave();metaScan()}
addEventListener('pagehide',()=>{fsFlush()});
/* homes: one encrypted record per account (AES-GCM, key from the password via PBKDF2) */
function homeSave(){if(!USER||!HOME||USER.r=='guest'||!IDB.db)return;
 const U=fs['C:'].USERS;if(U[uname()]!==HOME){for(const k in U)if(U[k]===HOME)delete U[k];U[uname()]=HOME}
 const json=JSON.stringify(HOME),id=uid(),key=KEY;
 homeChain=homeChain.then(async()=>{try{if(key){const iv=crypto.getRandomValues(new Uint8Array(12));const ct=await crypto.subtle.encrypt({name:'AES-GCM',iv},key,new TextEncoder().encode(json));await kvPutNow('home:'+id,{v:1,iv,ct})}else await kvPutNow('home:'+id,{v:1,plain:json})}catch(e){console.warn('home save',e)}})}
function defaultHome(){const h={};HOMESUB.forEach(s=>h[s]={});h.DOCUMENTS['WELCOME.TXT']='Welcome to your Rudolf OS home folder.\n\nFiles you keep here are private to your account and are stored encrypted with a key made from your password.\n';return h}
async function homeLoad(pw){const id=uid();let obj=null;
 if(USER.r=='guest'){obj=defaultHome()}
 else{const rec=await kvGet('home:'+id);
  if(rec&&rec.ct&&KEY){try{obj=JSON.parse(new TextDecoder().decode(await crypto.subtle.decrypt({name:'AES-GCM',iv:rec.iv},KEY,rec.ct)))}catch(e){notify('Your home folder could not be decrypted','It was kept unchanged; a new empty home is used for this session.',{kind:'error'});obj=defaultHome();HOMELOCKED=true}}
  else if(rec&&rec.plain){try{obj=JSON.parse(rec.plain)}catch(e){}}
  if(!obj){obj=defaultHome();const old=fs['C:'].USERS[uname()];if(isDir(old)&&Object.keys(old).length)obj=old}}
 HOMESUB.forEach(s=>{if(!isDir(obj[s]))obj[s]={}});
 HOME=obj;fs['C:'].USERS[uname()]=HOME;if(!HOMELOCKED)homeSave()}
let HOMELOCKED=false;
function homeUnload(){if(!HOME)return;if(!HOMELOCKED)homeSave();const U=fs['C:'].USERS;for(const k in U)if(U[k]===HOME)U[k]={};HOME=null;KEY=null;HOMELOCKED=false;EXPL.forEach(f=>f())}
/* ---------- passwords: per-user salt + PBKDF2 (legacy SHA-256 hashes are upgraded at the next sign-in) ---------- */
const b64e=u=>btoa(String.fromCharCode(...new Uint8Array(u))),b64d=s=>Uint8Array.from(atob(s),c=>c.charCodeAt(0));
const PBK_IT=210000;
async function pbkBits(pw,salt,bits){const k=await crypto.subtle.importKey('raw',new TextEncoder().encode(pw),'PBKDF2',false,['deriveBits']);return crypto.subtle.deriveBits({name:'PBKDF2',salt:b64d(salt),iterations:PBK_IT,hash:'SHA-256'},k,bits)}
async function pwMake(pw){const s=b64e(crypto.getRandomValues(new Uint8Array(16)));return'p2$'+s+'$'+b64e(await pbkBits(pw,s,256))}
async function pwCheck(a,pw){if(!a||pw==null)return false;if(a.r=='guest')return true;
 if(String(a.h).startsWith('p2$')){const[,s,h]=a.h.split('$');try{return b64e(await pbkBits(pw,s,256))===h}catch(e){return false}}
 if(await hsh(pw)===a.h){a.h=await pwMake(pw);saveUser();return true}return false}
async function pwFind(n,pw){const a=ACCTS.find(x=>x.n.toLowerCase()==String(n).toLowerCase());return a&&await pwCheck(a,pw)?a:null}
async function deriveKey(pw,salt){const k=await crypto.subtle.importKey('raw',new TextEncoder().encode(pw),'PBKDF2',false,['deriveKey']);return crypto.subtle.deriveKey({name:'PBKDF2',salt:b64d(salt),iterations:PBK_IT,hash:'SHA-256'},k,{name:'AES-GCM',length:256},false,['encrypt','decrypt'])}
async function homeRekey(pw){if(!USER||USER.r=='guest')return;USER.ks=b64e(crypto.getRandomValues(new Uint8Array(16)));KEY=await deriveKey(pw,USER.ks);saveUser();homeSave()}
/* ---------- per-user settings, trash and icon positions ---------- */
const cellKey=()=>USER?'bd95cells:'+uid():'bd95cells';
function persistS(){try{const v=JSON.stringify(SC||S);localStorage.setItem('bd95set',v);if(USER)localStorage.setItem('bd95set:'+uid(),v)}catch(e){}}
function trSave(){if(USER&&IDB&&IDB.db&&USER.r!='guest')kvPutNow('trash:'+uid(),TR);else if(!USER){try{localStorage.setItem('bd95trash',JSON.stringify(TR))}catch(e){}}trBadge();TRW.forEach(f=>f());return true}
/* ---------- sign-in / sign-out ---------- */
let SESSION=null;
async function userLogin(a,pw){await FSREADY;
 if(!a.id){a.id='u'+b64e(crypto.getRandomValues(new Uint8Array(6))).replace(/[^a-z0-9]/gi,'');saveUser()}
 if(a.r!='guest'){if(!a.ks){a.ks=b64e(crypto.getRandomValues(new Uint8Array(16)));saveUser()}KEY=await deriveKey(pw,a.ks)}
 USER=a;
 /* settings */
 const us=ld('bd95set:'+uid(),null);if(us){for(const k in us)S[k]=us[k];SC=cln(S)}else persistS();
 try{applySet()}catch(e){}try{a11yApply()}catch(e){}
 /* trash */
 let t=a.r=='guest'?[]:await kvGet('trash:'+uid());if(!Array.isArray(t)){t=a.r=='admin'&&!ld('bd95trash:migrated',0)?TR:[];try{localStorage.setItem('bd95trash:migrated','1');localStorage.removeItem('bd95trash')}catch(e){}}
 TR=t;trBadge();
 /* icons */
 const c=ld(cellKey(),null);if(c){cells=c;try{layIcons()}catch(e){}}
 await homeLoad(pw);
 try{metaScan(true)}catch(e){}
 a.last=Date.now();if(a.r!='guest')saveUser();
 SESSION={start:Date.now(),user:a};
 logEvt('sys',`${a.n} signed in`);
 parentalStart();
 setTimeout(()=>{try{window.__sessionRestore&&__sessionRestore()}catch(e){}},400);
 setTimeout(()=>{try{window.__afterLogin&&__afterLogin()}catch(e){}},600)}
function userLogout(){if(!USER)return;try{window.__sessionSave&&__sessionSave()}catch(e){}
 fsFlush();homeUnload();parentalStop();
 if(USER.r=='guest'){try{localStorage.removeItem('bd95set:guest');localStorage.removeItem('bd95cells:guest')}catch(e){}TR=[];
  for(const k in fs['C:'].USERS)if(k=='GUEST')delete fs['C:'].USERS[k];fsSave()}
 logEvt('sys',`${USER.n} signed out`);SESSION=null}
(function(){const lo=A.logoff,of=A.off;A.logoff=()=>{userLogout();lo()};A.off=()=>{userLogout();RD?shutdownDlg():of()}})();
/* Guest sign-in button on the login screen */
const SYSK='bd95sys';let SYSC=Object.assign({guest:true,ssmin:10,lockss:true,lockmin:0,suspmin:0},ld(SYSK,{}));const sysSave=()=>svj(SYSK,SYSC);
(function(){const box=$('#lg')&&$('#lg').parentElement;if(!box)return;const b=document.createElement('button');b.id='lgguest';b.textContent='Guest';b.title='Sign in as a guest. Nothing is kept after you sign out.';b.style.marginLeft='6px';$('#lg').after(b);
 const upd=()=>{b.style.display=SYSC.guest?'':'none'};upd();window.__guestUpd=upd;
 b.onclick=async()=>{const g={n:'Guest',r:'guest',id:'guest',h:''};USER=g;await userLogin(g,null);LG.style.display='none';le.textContent='';LP.clear();sfx('login');userUI()}})();
/* ---------- shutdown dialog (power button, Start → Shut down) ---------- */
function shutdownDlg(){const o=document.createElement('div');o.className='rmodal';o.innerHTML=`<div class='rmbox' role='dialog' aria-label='Power'><h3>Power</h3><p>What do you want the computer to do?</p><div class='rmrow'><button data-a='poweroff'>⏻ Shut down</button><button data-a='reboot'>↻ Restart</button><button data-a='suspend'>☾ Sleep</button><button data-a='x'>Cancel</button></div><div class='rmmsg'></div></div>`;document.body.appendChild(o);
 o.querySelector('button').focus();o.onclick=async e=>{const a=e.target.dataset&&e.target.dataset.a;if(!a)return;if(a=='x'){o.remove();showLogin();return}
  o.querySelector('.rmmsg').textContent=a=='suspend'?'Going to sleep…':'Please wait…';try{await fsFlush()}catch(x){}await homeChain;try{window.__sessionSave&&__sessionSave()}catch(x){}try{await (window.__rudolfLsFlush&&__rudolfLsFlush())}catch(x){}sfx('shutdown');try{await rdPost('power?action='+a)}catch(x){o.querySelector('.rmmsg').textContent=x.message}if(a=='suspend'){o.remove();showLogin()}}}
/* ---------- administrator permission (UAC / polkit style) ---------- */
function elevate(reason){return new Promise(res=>{if(window.__elevTrust&&isAdmin()&&Date.now()-__elevTrust<120000)return res(true);
 const admins=ACCTS.filter(a=>a.r=='admin');const o=document.createElement('div');o.className='rmodal';
 o.innerHTML=`<div class='rmbox' role='alertdialog' aria-label='Administrator permission'><h3>🛡️ Administrator permission</h3><p class='rmr'></p><div class='r2'><label>Account</label><select class='ea'></select><label>Password</label><input class='ep' autocomplete='off'></div><div class='rmmsg'></div><div class='rmrow'><button class='en'>Cancel</button><button class='ey pri'>Allow</button></div></div>`;
 o.querySelector('.rmr').textContent=reason;const sel=o.querySelector('.ea');admins.forEach(a=>{const op=document.createElement('option');op.textContent=a.n;op.value=a.n;sel.appendChild(op)});if(isAdmin())sel.value=USER.n;
 const P=maskInput(o.querySelector('.ep'));document.body.appendChild(o);setTimeout(()=>o.querySelector('.ep').focus(),30);
 const done=v=>{o.remove();res(v)};o.querySelector('.en').onclick=()=>done(false);
 const ok=async()=>{const a=admins.find(x=>x.n==sel.value);if(a&&await pwCheck(a,P.get())){logEvt('sys',`${a.n} approved: ${reason}`);if(isAdmin()&&a===USER)window.__elevTrust=Date.now();done(true)}else{o.querySelector('.rmmsg').textContent='Wrong password.';P.clear();sfx('error')}};
 o.querySelector('.ey').onclick=ok;o.querySelector('.ep').addEventListener('keydown',e=>{if(e.key=='Enter')ok();if(e.key=='Escape')done(false)})})}
/* ---------- lock screen, screensaver, idle, sleep ---------- */
let LOCKED=false,lastInput=Date.now(),SAVER=null;
['pointermove','pointerdown','keydown','wheel','touchstart'].forEach(ev=>addEventListener(ev,()=>{lastInput=Date.now();if(SAVER)saverStop()},{capture:true,passive:true}));
function lockScreen(reason,adminOnly){if(!USER||LOCKED)return;LOCKED=true;$('#menu').style.display='none';
 const o=document.createElement('div');o.id='lockov';o.setAttribute('role','dialog');o.setAttribute('aria-label','Locked');
 o.innerHTML=`<div class='lkclock'></div><div class='lkdate'></div><div class='lkbox'><div class='lkav'>👤</div><div class='lkn'></div><div class='lkr'></div><input class='lkp' aria-label='Password' placeholder='Password'><div class='lkm'></div><div class='rmrow'><button class='lks'>Switch user</button><button class='lku pri'>Unlock</button></div></div>`;
 o.querySelector('.lkn').textContent=USER.n;o.querySelector('.lkr').textContent=reason||'';const P=maskInput(o.querySelector('.lkp'));if(USER.r=='guest'&&!adminOnly)o.querySelector('.lkp').style.display='none';
 const tick=()=>{const d=sysNow?sysNow():new Date();o.querySelector('.lkclock').textContent=d.toLocaleTimeString([], {hour:'2-digit',minute:'2-digit'});o.querySelector('.lkdate').textContent=d.toLocaleDateString([], {weekday:'long',month:'long',day:'numeric'})};tick();const iv=setInterval(tick,1000);
 document.body.appendChild(o);setTimeout(()=>o.querySelector('.lkp').focus(),50);
 const un=async()=>{let ok=false;if(adminOnly)ok=await elevate(reason||'Unlock this account');else ok=USER.r=='guest'||await pwCheck(USER,P.get());
  if(ok){clearInterval(iv);o.remove();LOCKED=false;lastInput=Date.now();sfx('login');if(adminOnly&&window.__parentalExtend)__parentalExtend()}else{o.querySelector('.lkm').textContent='Wrong password.';P.clear();sfx('error')}};
 o.querySelector('.lku').onclick=un;o.querySelector('.lkp').addEventListener('keydown',e=>{e.stopPropagation();if(e.key=='Enter')un()});
 o.querySelector('.lks').onclick=()=>{clearInterval(iv);o.remove();LOCKED=false;A.logoff()};
 o.addEventListener('keydown',e=>{if(e.key!='Tab')e.stopPropagation()})}
function saverStart(){if(SAVER||!USER||LOCKED)return;const c=document.createElement('canvas');c.id='saver';c.width=innerWidth;c.height=innerHeight;document.body.appendChild(c);const g=c.getContext('2d');
 const st=[...Array(220)].map(()=>({x:Math.random()*2-1,y:Math.random()*2-1,z:Math.random()}));let raf;
 const fr=()=>{g.fillStyle='rgba(2,6,23,.35)';g.fillRect(0,0,c.width,c.height);for(const s of st){s.z-=0.006;if(s.z<=0.01){s.x=Math.random()*2-1;s.y=Math.random()*2-1;s.z=1}const k=0.5/s.z,x=c.width/2+s.x*k*c.width/2,y=c.height/2+s.y*k*c.height/2;g.fillStyle=`rgba(226,232,240,${1-s.z})`;g.fillRect(x,y,2/s.z,2/s.z)}
  g.fillStyle='#e2e8f0';g.font='bold 64px sans-serif';g.textAlign='center';const d=sysNow?sysNow():new Date();g.fillText(d.toLocaleTimeString([], {hour:'2-digit',minute:'2-digit'}),c.width/2,c.height/2);raf=requestAnimationFrame(fr)};fr();
 SAVER={c,stop:()=>{cancelAnimationFrame(raf);c.remove()},t:Date.now()}}
function saverStop(){if(!SAVER)return;const s=SAVER;SAVER=null;s.stop();if(SYSC.lockss&&Date.now()-s.t>3000)lockScreen()}
setInterval(()=>{if(!USER)return;const idle=(Date.now()-lastInput)/60000;
 if(SYSC.ssmin>0&&idle>=SYSC.ssmin&&!LOCKED&&!SAVER&&!document.fullscreenElement&&!document.pointerLockElement&&!(window.__mediaPlaying&&__mediaPlaying()))saverStart();
 if(SYSC.lockmin>0&&idle>=SYSC.lockmin&&!LOCKED){saverStop();lockScreen()}
 if(RD&&SYSC.suspmin>0&&idle>=SYSC.suspmin&&!(window.__mediaPlaying&&__mediaPlaying())){lastInput=Date.now();lockScreen();rdPost('power?action=suspend').catch(()=>{})}},15000);
/* ---------- parental controls: allowed apps and a daily time limit ---------- */
const APPCAT=[['browser','Oracle Browser',/^Oracle/],['explorer','File Explorer',/^File Explorer|^Exploring/],['notepad','Notepad',/Notepad/],['prompt','Rudolf Prompt',/^Rudolf Prompt/],['blocklua','BlockLua Builder',/^BlockLua/],['calc','Calculator',/^Calculator/],['wordlite','WordLite',/^WordLite/],['wordpro','WordPro',/^WordPro/],['paint','Paint',/^Paint/],['boards','Board Games',/^Board Games|^Checkers|^Chess/],['poker','Card Games',/^Card Games/],['galaxy','WOS Galaxy Map',/^WOS Galaxy Map/],['media','Media Player',/^Media Player/],['gameboy','Emu',/^Emu|Emu$|Game Boy|GBA|\.GBA?C?\b/i],['flash','Flash Player',/Flash/],['quake','Quake Engine',/^Quake/],['gpulab','GPU Lab',/^GPU Lab/],['settings','Settings',/^Settings|^Display Properties/]];
let parentT=0;
function parentalStart(){parentalStop();const pc=USER&&USER.pc;if(!pc||USER.r=='admin')return;const day=new Date().toDateString();const k='bd95ptime:'+uid();let u=ld(k,{d:day,m:0});if(u.d!=day)u={d:day,m:0};
 parentT=setInterval(()=>{if(LOCKED||!USER)return;u.m+=0.25;svj(k,u);if(pc.mins>0&&u.m>=pc.mins&&!LOCKED){closeAll();lockScreen('Your time for today is used up. An administrator can allow more time.',true)}},15000);
 window.__parentalExtend=()=>{u.m=Math.max(0,(pc.mins||0)-30);svj(k,u);notify('30 more minutes allowed','')}}
function parentalStop(){clearInterval(parentT);parentT=0}
function appAllowed(title){const pc=USER&&USER.pc;if(!pc||!pc.block||USER.r=='admin')return true;const e=APPCAT.find(x=>x[2].test(title));return!(e&&pc.block.includes(e[0]))}
(function(){const _w=win;win=function(title,...r){const W=_w(title,...r);if(!appAllowed(String(title))){setTimeout(()=>{W.onclose=null;W.close()},0);notify('Not allowed',`An administrator has turned off “${title}” for this account.`,{kind:'error'})}return W}})();
/* ---------- file metadata (created / modified / owner), recent files, favourites ---------- */
var META={},PREV=new Map(),metaT=0;
function metaWalk(n,p,out){for(const k in n){const v=n[k],pk=p+'/'+k;if(isDir(v))metaWalk(v,pk,out);else out.set(pk,v)}}
function metaScan(first){const cur=new Map();metaWalk(fs,'',cur);const now=Date.now(),own=USER?USER.n:'system';let ch=false;
 cur.forEach((v,k)=>{const m=META[k];if(!m){META[k]={c:first?now:now,m:now,o:first?'system':own};ch=true}else if(!first&&PREV.has(k)&&PREV.get(k)!==v){m.m=now;ch=true}});
 if(!first&&PREV.size)for(const k in META)if(!cur.has(k)&&k.startsWith('/C:/USERS/'+uname()+'/')){}
 PREV=cur;if(ch){clearTimeout(metaT);metaT=setTimeout(()=>kvSet('meta',META),1500)}}
const fileMeta=path=>META['/'+path.join('/')]||null;
const RECK=()=>'bd95recent:'+uid(),FAVK=()=>'bd95fav:'+uid();
function recentAdd(path){if(!USER)return;const k=path.join('\\');let r=ld(RECK(),[]).filter(x=>x!=k);r.unshift(k);svj(RECK(),r.slice(0,30))}
const recentList=()=>ld(RECK(),[]).map(k=>k.split('\\')).filter(p=>node(p)!=null);
const favList=()=>ld(FAVK(),[]).map(k=>k.split('\\')).filter(p=>isDir(node(p)));
function favToggle(path){const k=path.join('\\');let f=ld(FAVK(),[]);f=f.includes(k)?f.filter(x=>x!=k):[...f,k];svj(FAVK(),f);EXPL.forEach(x=>x())}
(function(){const o=openPath;openPath=function(path){try{if(!isDir(node(path)))recentAdd(path)}catch(e){}return o.apply(this,arguments)}})();
/* write permission: system folders need an administrator, other homes are private */
function fsCanWrite(path){if(path.length>=3&&path[1]=='USERS'&&path[2]!=uname())return false;return true}
/* ---------- saving a file out of Rudolf OS (downloads) ---------- */
const DLOK=/\.(gif|png|jpe?g|webp|mp4|webm|txt|json|md|docx|pptx|epub|csv|ttf|html|svg|pdf|xlsx|zip)$/i;
let DLCAP=null;(async()=>{try{if(window.claude&&claude.use)DLCAP=await claude.use('downloads')}catch(e){}})();
function dataToBlob(v,type){if(v instanceof Blob)return v;if(typeof v=='string'&&v.startsWith('data:')){const i=v.indexOf(','),meta=v.slice(5,i),b=meta.includes('base64')?Uint8Array.from(atob(v.slice(i+1)),c=>c.charCodeAt(0)):new TextEncoder().encode(decodeURIComponent(v.slice(i+1)));return new Blob([b],{type:meta.split(';')[0]})}return new Blob([v],{type:type||'text/plain'})}
async function saveOut(name,data,type){let blob=dataToBlob(data,type),fn=name.toLowerCase();
 if(DLCAP&&!DLOK.test(fn)){const z=new JSZip();z.file(fn,blob);blob=await z.generateAsync({type:'blob'});fn=fn+'.zip'}
 if(DLCAP){try{await DLCAP.save({filename:fn,data:blob});notify('Saved',fn);return true}catch(e){if(e&&e.code=='declined')return false;if(!/unavailable|not_granted|capability/.test(e&&e.code||''))notify('Download failed',e&&e.message||'',{kind:'error'});}}
 try{const a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download=fn;document.body.appendChild(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(a.href),60000);
  if(RD)notify('Downloaded',fn+' is in the Downloads folder of this computer.');return true}catch(e){notify('Download is not available here','',{kind:'error'});return false}}
/* ---------- backup and restore of the whole system ---------- */
async function backupAll(){fsFlush();await homeChain;const keys={};for(let i=0;i<localStorage.length;i++){const k=localStorage.key(i);if(/^bd95|^blockdos/.test(k))keys[k]=localStorage.getItem(k)}
 const recs={};await IDB.ready;if(IDB.db)await new Promise(r=>{const tx=IDB.db.transaction('kv');const st=tx.objectStore('kv');const q=st.openCursor();q.onsuccess=()=>{const c=q.result;if(!c)return r();recs[c.key]=c.value;c.continue()};q.onerror=()=>r()});
 const z=new JSZip();z.file('rudolf-backup.json',JSON.stringify({rudolf:RVER,date:new Date().toISOString(),ls:keys},null,0));
 const enc=v=>JSON.stringify(v,(k,x)=>typeof x=='string'&&x.startsWith('rudolfdisk:')&&k!='v'&&k!='data'?undefined:x instanceof ArrayBuffer?{__b:b64e(x)}:ArrayBuffer.isView(x)?{__u:b64e(x.buffer.slice(x.byteOffset,x.byteOffset+x.byteLength))}:x);
 for(const k in recs)z.file('db/'+encodeURIComponent(k)+'.json',enc(recs[k]));
 const blob=await z.generateAsync({type:'blob',compression:'DEFLATE'});const d=new Date();
 return saveOut(`rudolf-backup-${d.getFullYear()}${String(d.getMonth()+1).padStart(2,'0')}${String(d.getDate()).padStart(2,'0')}.zip`,blob)}
async function restoreAll(file){if(!await elevate('Restore a backup. Everything on this Rudolf OS is replaced by the backup.'))return false;
 const z=await JSZip.loadAsync(file);const m=z.file('rudolf-backup.json');if(!m)throw new Error('This is not a Rudolf OS backup.');const meta=JSON.parse(await m.async('string'));
 const dec=s=>JSON.parse(s,(k,x)=>x&&x.__b?b64d(x.__b).buffer:x&&x.__u?b64d(x.__u):x);
 await IDB.ready;for(const f of Object.keys(z.files).filter(n=>n.startsWith('db/')&&n.endsWith('.json'))){await kvSet(decodeURIComponent(f.slice(3,-5)),dec(await z.file(f).async('string')))}
 Object.keys(localStorage).filter(k=>/^bd95|^blockdos/.test(k)).forEach(k=>localStorage.removeItem(k));for(const k in meta.ls)localStorage.setItem(k,meta.ls[k]);
 notify('Backup restored','Rudolf OS restarts now.');setTimeout(()=>location.reload(),1500);return true}
