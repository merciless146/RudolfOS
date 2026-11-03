/* ===== Rudolf OS 1.5.0: administrator access to account files (docs/plan-1.5.0.md, part 3) =====
   Before, each home folder was encrypted with a key made from its own password: nobody else could open it, and a
   forgotten password meant an empty home folder. Now:
   - every account has a random file key (FK, AES-256-GCM); its home folder is encrypted with it (record k:'f');
   - a.fk: FK encrypted with the key made from the account's password (PBKDF2, as before: a.ks is its salt);
   - administrators have an ECDH P-256 key pair: a.pub (public, JWK), a.priv (private, PKCS#8, encrypted with the
     administrator's password key);
   - a.ak[adminId]: FK encrypted for that administrator (an ephemeral ECDH key with the administrator's public key,
     HKDF-SHA256 to AES-256-GCM), so it can be made while the administrator is not signed in;
   - a.aa: "Administrators can open this account's files", per account, on unless an administrator turns it off;
     turned off, the administrators' copies (a.ak) are deleted.
   The account's own sign-in wraps its FK for every administrator; an administrator's sign-in wraps the FKs it can
   open for administrators that have no copy yet (a new administrator), and opens the other accounts' home folders in
   C:\USERS. An administrator can reset another account's password without the files being lost.
   The Linux side (Debian, Wine) has one Linux home for every account, as before. */
window.__acct50=1;
let FKRAW=null,PKEY=null,ADMPRIV=null,ADMPRIVRAW=null;
const AC_OTHERS=new Map();   /* account id -> {a, key, name}: other accounts' homes an administrator has open */
const acB64=u=>b64e(u instanceof ArrayBuffer?u:u.buffer.slice(u.byteOffset,u.byteOffset+u.byteLength));
async function acEnc(key,bytes){const iv=crypto.getRandomValues(new Uint8Array(12));return{iv:b64e(iv),ct:acB64(new Uint8Array(await crypto.subtle.encrypt({name:'AES-GCM',iv},key,bytes)))}}
async function acDec(key,box){return new Uint8Array(await crypto.subtle.decrypt({name:'AES-GCM',iv:b64d(box.iv)},key,b64d(box.ct)))}
const acFkKey=raw=>crypto.subtle.importKey('raw',raw,{name:'AES-GCM'},false,['encrypt','decrypt']);
const ECP={name:'ECDH',namedCurve:'P-256'};
async function acEcAes(priv,pubJwk,salt){const pub=await crypto.subtle.importKey('jwk',pubJwk,ECP,false,[]);
 const bits=await crypto.subtle.deriveBits({name:'ECDH',public:pub},priv,256);
 const hk=await crypto.subtle.importKey('raw',bits,'HKDF',false,['deriveKey']);
 return crypto.subtle.deriveKey({name:'HKDF',hash:'SHA-256',salt,info:new TextEncoder().encode('rudolf-fk-v1')},hk,{name:'AES-GCM',length:256},false,['encrypt','decrypt'])}
/* FK for an administrator's public key */
async function acWrapFor(pubJwk,raw){const e=await crypto.subtle.generateKey(ECP,true,['deriveBits']);const salt=crypto.getRandomValues(new Uint8Array(16));
 const k=await acEcAes(e.privateKey,pubJwk,salt);return{epk:await crypto.subtle.exportKey('jwk',e.publicKey),salt:b64e(salt),...await acEnc(k,raw)}}
async function acUnwrap(priv,box){const k=await acEcAes(priv,box.epk,b64d(box.salt));return acDec(k,box)}
const acAdmins=()=>ACCTS.filter(a=>a.r=='admin'&&a.id&&a.pub);
const acAllowed=a=>a.aa!==false;
const acUname=a=>String(a.n).toUpperCase().replace(/[\\\/:*?"<>|]/g,'_').slice(0,40);
/* the administrators' copies of a's FK: one for every administrator with a key pair (not a itself), none when off */
async function acWrapAll(a,raw){
 if(!acAllowed(a)){if(a.ak){delete a.ak;return true}return false}
 a.ak=a.ak||{};let ch=false;const ids=new Set(acAdmins().map(b=>b.id));
 for(const k in a.ak)if(!ids.has(k)){delete a.ak[k];ch=true}
 for(const b of acAdmins()){if(b===a||a.ak[b.id])continue;try{a.ak[b.id]=await acWrapFor(b.pub,raw);ch=true}catch(e){console.warn('acWrapAll',e)}}
 return ch}
/* a new account (Settings › User Settings › Create new account): its file key and the administrators' copies now */
async function acctNew(n,pw,r){const a={n,h:await pwMake(pw),r,id:'u'+b64e(crypto.getRandomValues(new Uint8Array(6))).replace(/[^a-z0-9]/gi,''),ks:b64e(crypto.getRandomValues(new Uint8Array(16)))};
 const pk=await deriveKey(pw,a.ks),raw=crypto.getRandomValues(new Uint8Array(32));a.fk=await acEnc(pk,raw);
 if(r=='admin'){const kp=await crypto.subtle.generateKey(ECP,true,['deriveBits']);a.pub=await crypto.subtle.exportKey('jwk',kp.publicKey);a.priv=await acEnc(pk,new Uint8Array(await crypto.subtle.exportKey('pkcs8',kp.privateKey)))}
 await acWrapAll(a,raw);
 /* a new administrator gets copies of the files this administrator can open */
 if(r=='admin'&&ADMPRIV)for(const o of ACCTS){if(!acAllowed(o)||!o.ak||!o.ak[USER.id])continue;try{const f=await acUnwrap(ADMPRIV,o.ak[USER.id]);o.ak[a.id]=await acWrapFor(a.pub,f)}catch(e){}}
 return a}
/* after the home folder is loaded at sign-in (core25 userLogin): move it to a file key, make the administrator's key
   pair, hand out copies, open the other accounts' homes (administrators) */
async function acctKeys(a,pw){if(!a||a.r=='guest'||!PKEY)return;let ch=false;
 if(!FKRAW){FKRAW=crypto.getRandomValues(new Uint8Array(32));a.fk=await acEnc(PKEY,FKRAW);KEY=await acFkKey(FKRAW);ch=true;homeSave()}
 if(a.r=='admin'){
  if(a.pub&&a.priv){try{ADMPRIVRAW=await acDec(PKEY,a.priv);ADMPRIV=await crypto.subtle.importKey('pkcs8',ADMPRIVRAW,ECP,false,['deriveBits'])}catch(e){delete a.pub;delete a.priv}}
  if(!a.pub){const kp=await crypto.subtle.generateKey(ECP,true,['deriveBits']);a.pub=await crypto.subtle.exportKey('jwk',kp.publicKey);ADMPRIVRAW=new Uint8Array(await crypto.subtle.exportKey('pkcs8',kp.privateKey));
   a.priv=await acEnc(PKEY,ADMPRIVRAW);ADMPRIV=await crypto.subtle.importKey('pkcs8',ADMPRIVRAW,ECP,false,['deriveBits']);ch=true}}
 if(await acWrapAll(a,FKRAW))ch=true;
 if(a.r=='admin'&&ADMPRIV){
  for(const o of ACCTS){if(o===a||!acAllowed(o)||!o.ak||!o.ak[a.id])continue;
   let f;try{f=await acUnwrap(ADMPRIV,o.ak[a.id])}catch(e){continue}
   if(await acWrapAll(o,f))ch=true;
   await acOpenHome(o,f)}}
 if(ch)saveUser();
 try{acWelcome()}catch(e){}EXPL.forEach(f=>f())}
/* another account's home in C:\USERS\<NAME> (administrators) */
async function acOpenHome(o,f){if(!o.id)return;const key=await acFkKey(f);const rec=await kvGet('home:'+o.id);let obj=null;
 if(rec&&rec.ct&&rec.k=='f'){try{obj=JSON.parse(new TextDecoder().decode(await crypto.subtle.decrypt({name:'AES-GCM',iv:rec.iv},key,rec.ct)))}catch(e){obj=null}}
 if(!obj)return;   /* not moved to a file key yet (moves at that account's next sign-in), or nothing kept */
 const nm=acUname(o);fs['C:'].USERS[nm]=obj;AC_OTHERS.set(o.id,{a:o,key,name:nm})}
/* fsFlush: the other homes an administrator has open are saved too */
function acSaveOthers(){for(const[id,x]of AC_OTHERS){const obj=fs['C:'].USERS[x.name];if(!isDir(obj))continue;const json=JSON.stringify(obj);
 homeChain=homeChain.then(async()=>{try{const iv=crypto.getRandomValues(new Uint8Array(12));const ct=await crypto.subtle.encrypt({name:'AES-GCM',iv},x.key,new TextEncoder().encode(json));await kvPutNow('home:'+id,{v:1,k:'f',iv,ct})}catch(e){console.warn('home save (other)',e)}})}}
function acClose(){for(const[,x]of AC_OTHERS){if(fs['C:']&&fs['C:'].USERS)fs['C:'].USERS[x.name]={}}AC_OTHERS.clear();FKRAW=null;PKEY=null;ADMPRIV=null;ADMPRIVRAW=null}
/* the account's own new password (User Settings): the file key and the administrator's private key are wrapped again */
async function acRekey(a,pw){a.ks=b64e(crypto.getRandomValues(new Uint8Array(16)));PKEY=await deriveKey(pw,a.ks);
 if(FKRAW)a.fk=await acEnc(PKEY,FKRAW);if(ADMPRIVRAW)a.priv=await acEnc(PKEY,ADMPRIVRAW)}
/* an administrator sets another account's password: the files stay when this administrator can open them */
async function acReset(o,pw){let f=null;if(ADMPRIV&&o.ak&&o.ak[USER.id]){try{f=await acUnwrap(ADMPRIV,o.ak[USER.id])}catch(e){}}
 o.h=await pwMake(pw);o.ks=b64e(crypto.getRandomValues(new Uint8Array(16)));const pk=await deriveKey(pw,o.ks);
 if(f)o.fk=await acEnc(pk,f);else{delete o.fk;delete o.ak}
 /* an administrator's own key pair cannot be opened without the old password: a new one at its next sign-in, and the
    other administrators hand it copies again */
 if(o.r=='admin'){delete o.pub;delete o.priv;for(const x of ACCTS)if(x.ak&&o.id)delete x.ak[o.id]}
 delete o.pk;saveUser();return!!f}
/* the per-account switch (administrators) */
async function acSetAllowed(o,on){o.aa=!!on;if(!on){delete o.ak;const x=AC_OTHERS.get(o.id);if(x){fsFlush();fs['C:'].USERS[x.name]={};AC_OTHERS.delete(o.id)}}
 else if(o===USER&&FKRAW)await acWrapAll(o,FKRAW);   /* another account: the copies are made at its next sign-in */
 saveUser();EXPL.forEach(f=>f())}
/* what the account's own welcome file says */
const AC_WELCOME_OLD='Files you keep here are private to your account and are stored encrypted with a key made from your password.\n';
function acWelcomeText(a){return acAllowed(a)?'Files you keep here are stored encrypted. You and the administrators of this computer can open them (an administrator can turn that off in Settings › User Settings).\n':'Files you keep here are stored encrypted. Only you can open them.\n'}
function acWelcome(){if(!HOME||!USER||USER.r=='guest'||!HOME.DOCUMENTS)return;const w=HOME.DOCUMENTS['WELCOME.TXT'];if(typeof w!='string')return;
 const nw='Welcome to your Rudolf OS home folder.\n\n'+acWelcomeText(USER);
 if(w=='Welcome to your Rudolf OS home folder.\n\n'+AC_WELCOME_OLD||(w!==nw&&/^Welcome to your Rudolf OS home folder\.\n\nFiles you keep here are stored encrypted\./.test(w)&&w.split('\n').length<=4)){HOME.DOCUMENTS['WELCOME.TXT']=nw;fsSave()}}
/* Settings › User Settings: Files and administrators */
function acSection(c,W){const adm=isAdmin();if(!USER||USER.r=='guest')return;
 const box=document.createElement('div');box.className='ac50';
 box.innerHTML=`<h4 class='sh4'>Files and administrators</h4><p class='msg acme' style='margin:2px 0 8px'></p><div class='acl50'></div>`;
 const foot=c.querySelector(':scope > .sfoot');if(foot)c.insertBefore(box,foot);else c.appendChild(box);
 const me=box.querySelector('.acme'),L=box.querySelector('.acl50');
 const draw=()=>{me.textContent='Your files: '+(acAllowed(USER)?'you and the administrators of this computer can open them.':'only you can open them.')+(adm?'':' Only an administrator can change this.');
  L.innerHTML='';if(!adm)return;
  ACCTS.forEach(o=>{const r=document.createElement('div');r.className='acr';r.style.cssText='display:flex;gap:8px;align-items:center;flex-wrap:wrap;padding:3px 0';
   const self=o===USER,open=AC_OTHERS.has(o.id),has=!!(o.ak&&o.ak[USER.id]);
   r.innerHTML=`<span class='acn' style='min-width:140px'></span><label class='ck'><input type='checkbox' class='aa'> Administrators can open this account's files</label><span class='as' style='color:#6b7280;font-size:12px'></span>${self?'':"<button class='rp'>Reset password…</button>"}`;
   r.querySelector('.acn').textContent='👤 '+o.n+(self?' (you)':'');const cb=r.querySelector('.aa');cb.checked=acAllowed(o);
   r.querySelector('.as').textContent=self?'':!acAllowed(o)?'':open?`open in C:\\USERS\\${acUname(o)}`:has?'':'from their next sign-in';
   cb.onchange=async()=>{const on=cb.checked;cb.checked=!on;if(!await elevate((on?'Let administrators open the files of ':'Stop administrators from opening the files of ')+o.n))return;await acSetAllowed(o,on);logEvt('sys',`${USER.n} turned ${on?'on':'off'} administrator access to ${o.n}'s files`);hlNote&&hlNote('account',`administrator access to an account's files turned ${on?'on':'off'}`);draw()};
   const rp=r.querySelector('.rp');if(rp)rp.onclick=()=>acResetDlg(o,draw);L.appendChild(r)})};
 draw()}
function acResetDlg(o,done){const has=!!(ADMPRIV&&o.ak&&o.ak[USER.id]);const d=document.createElement('div');d.className='rmodal';
 d.innerHTML=`<div class='rmbox' role='alertdialog' aria-label='Reset password' style='max-width:480px'><h3>🔑 Reset the password of <span class='nm'></span></h3><p class='msg'>${has?'Their files stay: they open with the new password.':'⚠ You cannot open this account\'s files (they are not shared with administrators yet, or it is turned off), so with a new password the account starts with an <b>empty home folder</b>. The old files are kept in case the old password is remembered.'}</p>${o.r=='admin'?"<p class='msg'>As an administrator, they get new keys at their next sign-in; the other accounts' files reach them again after another administrator signs in.</p>":''}<div class='r2'><label>New password</label><input class='p1'><label>Confirm</label><input class='p2'></div><div class='rmmsg'></div><div class='rmrow'><button class='n'>Cancel</button><button class='y pri'>Reset password</button></div></div>`;
 d.querySelector('.nm').textContent=o.n;const P1=maskInput(d.querySelector('.p1')),P2=maskInput(d.querySelector('.p2')),m=d.querySelector('.rmmsg');document.body.appendChild(d);
 d.querySelector('.n').onclick=()=>d.remove();
 d.querySelector('.y').onclick=async()=>{const a=P1.get(),b=P2.get();if(!a){m.textContent='Enter a new password.';return}if(a!=b){m.textContent='The passwords do not match.';return}
  if(!await elevate('Reset the password of '+o.n))return;const kept=await acReset(o,a);d.remove();logEvt('sys',`${USER.n} reset the password of ${o.n}`);hlNote&&hlNote('account','an administrator reset an account\'s password'+(kept?'':' (its files could not be kept)'));
  notify('Password reset',kept?`${o.n} can sign in with the new password; their files are kept.`:`${o.n} can sign in with the new password.`);done&&done()}}
if(typeof usrPanelS=='function'){const _up50=usrPanelS;usrPanelS=function(c,W){const r=_up50.apply(this,arguments);try{acSection(c,W)}catch(e){console.warn('acct50',e)}return r}}
