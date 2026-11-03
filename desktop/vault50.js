/* ===== Rudolf OS 1.5.0: data encryption (docs/plan-1.5.0.md, part 4) =====
   The kernel encrypts /persist/vault (rudolfd: vault/*); Debian, Flatpak apps and Downloads move into it. The 64-byte
   data key (DK) is never stored as it is: /persist/etc/vault.json (kept by rudolfd as the desktop gives it) has it
   wrapped for every account that can unlock it, {salt,iv,ct} with AES-256-GCM and a key made from the account's
   password (PBKDF2-SHA256, 210 000 rounds), and for the recovery key shown once at setup. The first sign-in with a
   password after start-up unwraps it and hands it to rudolfd, which adds it to the kernel; Guest does not unlock it.
   The key stays in this page's memory while it is open, so an account that has no copy yet gets one when it signs
   in, and new accounts, new passwords and password resets wrap it again. */
window.__vault50=1;
let VAULTDK=null,VAULT50=null;
const VLT_PARTS={debian:'Linux (Debian) and Windows programs',flatpak:'Linux apps (Flatpak)',downloads:'Files kept in Downloads'};
const vHex=u=>[...u].map(b=>b.toString(16).padStart(2,'0')).join('');
async function vStatus(){if(!RD)return null;try{return VAULT50=await rd('vault')}catch(e){return null}}
async function vPwKey(pw,salt){const k=await crypto.subtle.importKey('raw',new TextEncoder().encode(pw),'PBKDF2',false,['deriveKey']);
 return crypto.subtle.deriveKey({name:'PBKDF2',salt,iterations:PBK_IT,hash:'SHA-256'},k,{name:'AES-GCM',length:256},false,['encrypt','decrypt'])}
async function vWrap(secret,dk){const salt=crypto.getRandomValues(new Uint8Array(16));return{salt:b64e(salt),...await acEnc(await vPwKey(secret,salt),dk)}}
async function vUnwrap(secret,box){return acDec(await vPwKey(secret,b64d(box.salt)),box)}
/* the recovery key: 30 letters and digits in groups of five (no 0/O, 1/I/L) */
function vRecoveryKey(){const A='ABCDEFGHJKMNPQRSTUVWXYZ23456789',r=crypto.getRandomValues(new Uint8Array(30));let s='';r.forEach((b,i)=>{s+=A[b%A.length];if(i%5==4&&i<29)s+='-'});return s}
const vRecNorm=s=>String(s||'').toUpperCase().replace(/[^A-Z0-9]/g,'').replace(/(.{5})(?=.)/g,'$1-');
async function vStore(data){await rdPost('vault/store',JSON.stringify(data))}
/* this account's copy of the key, made or renewed (the data key must be in memory) */
async function vAddEntry(a,pw,data){if(!VAULTDK||!a||!a.id||a.r=='guest')return false;data=data||(VAULT50&&VAULT50.data);if(!data)return false;
 data.accts=data.accts||{};data.accts[a.id]=await vWrap(pw,VAULTDK);
 const ids=new Set(ACCTS.map(x=>x.id).filter(Boolean));ids.add(a.id);for(const k in data.accts)if(!ids.has(k))delete data.accts[k];
 await vStore(data);return true}
async function vUnlock(dk){VAULTDK=dk;const s=await rdPost('vault/unlock',vHex(dk));VAULT50=s;return s}
/* at sign-in (after the home folder: core25 userLogin → acctKeys → here) */
async function vaultSignin(a,pw){if(!RD||!a||a.r=='guest'||!pw)return;const s=await vStatus();if(!s||!s.data||s.state=='off'||s.state=='unavailable')return;
 const box=s.data.accts&&s.data.accts[a.id];
 if(box&&!VAULTDK){try{VAULTDK=await vUnwrap(pw,box)}catch(e){hlNote('vault','an account\'s copy of the data key did not open');return}}
 if(VAULTDK&&(s.state=='locked'||s.state=='ready')){try{await vUnlock(VAULTDK);notify('Your data is unlocked',s.state=='ready'?'Rudolf OS is now moving Debian, Linux apps and Downloads into the encrypted folder.':'Linux programs and Downloads are ready.');if(s.state=='ready')vMoveDlg()}catch(e){notify('Encrypted data',e.message||'It could not be unlocked.',{kind:'error'})}}
 if(VAULTDK&&!box)try{await vAddEntry(a,pw,s.data)}catch(e){}}
/* the account's password changed (User Settings) or was reset by an administrator: its copy is made again */
async function vRekey(a,pw){if(!VAULTDK||!RD)return;const s=await vStatus();if(s&&s.data)try{await vAddEntry(a,pw,s.data)}catch(e){}}
if(typeof acctNew=='function'){const _an50=acctNew;acctNew=async function(n,pw,r){const a=await _an50(n,pw,r);try{await vRekey(a,pw)}catch(e){}return a}}
if(typeof acReset=='function'){const _ar50=acReset;acReset=async function(o,pw){const k=await _ar50(o,pw);try{await vRekey(o,pw)}catch(e){}return k}}
if(typeof acRekey=='function'){const _ak50=acRekey;acRekey=async function(a,pw){await _ak50(a,pw);try{await vRekey(a,pw)}catch(e){}}}
if(typeof acctKeys=='function'){const _ck50=acctKeys;acctKeys=async function(a,pw){await _ck50(a,pw);try{await vaultSignin(a,pw)}catch(e){console.warn('vault50',e)}}}
/* the move, as a job (rudolfd vault-move) */
function vMoveDlg(done){const o=document.createElement('div');o.className='rmodal';
 o.innerHTML=`<div class='rmbox' role='alertdialog' aria-label='Encrypting your data' style='max-width:560px'><h3>🔐 Encrypting your data</h3><p class='msg'>Debian, Linux apps and Downloads are copied into the encrypted folder, then the unencrypted copies are deleted. Linux programs stop until it is done.</p><pre class='vl' style='max-height:200px;overflow:auto;white-space:pre-wrap;background:rgba(0,0,0,.25);padding:8px;font-size:12px'>Starting…</pre><div class='rmrow'><button class='c' disabled>Close</button></div></div>`;
 document.body.appendChild(o);const L=o.querySelector('.vl'),c=o.querySelector('.c');c.onclick=()=>{o.remove();done&&done()};
 (async()=>{for(let i=0;i<20000;i++){await new Promise(r=>setTimeout(r,800));let j;try{j=await rd('job')}catch(e){continue}
  if(j.name!='vault'&&i>5&&!j.running)break;if(j.name!='vault')continue;L.textContent=(j.log||'').trim()||'Working…';L.scrollTop=L.scrollHeight;
  if(!j.running){c.disabled=false;c.focus();if(j.status==0)notify('Your data is encrypted','Debian, Linux apps and Downloads are in the encrypted folder.');else notify('Encryption did not finish',(j.log||'').trim().split('\n').pop()||'See Settings › Security Settings.',{kind:'error'});vStatus();return}}c.disabled=false})()}
/* setup: the recovery key first, then the drive */
async function vSetup(redraw){
 if(!USER||USER.r=='guest')return;
 if(!await elevate('Encrypt my data: Debian, Linux apps and Downloads move into a folder only your accounts\' passwords (or the recovery key) can open.'))return;
 const rk=vRecoveryKey(),o=document.createElement('div');o.className='rmodal';
 o.innerHTML=`<div class='rmbox' role='alertdialog' aria-label='Encrypt my data' style='max-width:580px'><h3>🔐 Encrypt my data</h3>
<p>Your <b>recovery key</b>. It opens your encrypted data if every password on this computer is forgotten. Write it down or save it somewhere other than this computer: it is shown only now.</p>
<div style='font:600 20px/1.4 monospace;letter-spacing:1px;text-align:center;padding:10px;background:rgba(128,128,128,.12);border-radius:8px;user-select:all' class='rk'></div>
<div class='rmrow' style='justify-content:center'><button class='sv'>Save as a file…</button><button class='cp'>Copy</button></div>
<div class='r2'><label>Your password</label><input class='pw' autocomplete='off'></div>
<label class='ck'><input type='checkbox' class='ok'> I have written down or saved the recovery key</label>
<p class='msg' style='color:#6b7280;font-size:12px'>Other accounts can unlock it once they sign in on this computer while it is unlocked (they get their own copy of the key then). Encryption cannot be turned off again.</p>
<div class='rmmsg'></div><div class='rmrow'><button class='n'>Cancel</button><button class='y pri' disabled>Encrypt</button></div></div>`;
 o.querySelector('.rk').textContent=rk;const P=maskInput(o.querySelector('.pw')),m=o.querySelector('.rmmsg'),y=o.querySelector('.y');document.body.appendChild(o);
 o.querySelector('.ok').onchange=e=>y.disabled=!e.target.checked;
 o.querySelector('.cp').onclick=()=>{try{navigator.clipboard.writeText(rk);notify('Recovery key','Copied. Paste it somewhere safe, not on this computer.')}catch(e){}};
 o.querySelector('.sv').onclick=()=>{const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([`Rudolf OS recovery key\r\n\r\n${rk}\r\n\r\nIt opens the encrypted data on the computer where it was made (${new Date().toISOString().slice(0,10)}). Keep it away from that computer.\r\n`],{type:'text/plain'}));a.download='Rudolf OS recovery key.txt';a.click()};
 o.querySelector('.n').onclick=()=>o.remove();
 y.onclick=async()=>{const pw=P.get();if(!(await pwCheck(USER,pw))){m.textContent='Wrong password.';P.clear();return}
  y.disabled=true;m.textContent='Preparing…';
  try{const dk=crypto.getRandomValues(new Uint8Array(64));const data={v:1,rec:await vWrap(vRecNorm(rk).replace(/-/g,''),dk),accts:{}};
   if(!USER.id){USER.id='u'+b64e(crypto.getRandomValues(new Uint8Array(6))).replace(/[^a-z0-9]/gi,'');saveUser()}
   data.accts[USER.id]=await vWrap(pw,dk);
   const s=await rdPost('vault/setup',JSON.stringify(data));VAULTDK=dk;VAULT50=s;o.remove();logEvt('sys','Data encryption set up');
   if(s.state=='restart'){notify('Restart to continue','The drive was prepared for encryption. Restart Rudolf OS; after you sign in, your data moves into the encrypted folder.',{action:()=>{try{fsFlush()}catch(e){}rdPost('power?action=reboot').catch(()=>{})}})}
   else{await vUnlock(dk);vMoveDlg(redraw)}
   redraw&&redraw()}catch(e){m.textContent=e.message||'Encryption could not be set up.';y.disabled=false}}}
/* unlock with the recovery key (Security Settings, while it is locked) */
function vRecoverDlg(redraw){const o=document.createElement('div');o.className='rmodal';
 o.innerHTML=`<div class='rmbox' role='alertdialog' aria-label='Unlock with the recovery key' style='max-width:520px'><h3>🔑 Unlock with the recovery key</h3><div class='r2'><label>Recovery key</label><input class='rk' placeholder='XXXXX-XXXXX-XXXXX-XXXXX-XXXXX-XXXXX' autocomplete='off' spellcheck='false'><label>Your password</label><input class='pw' autocomplete='off'></div><p class='msg' style='color:#6b7280;font-size:12px'>Your account then gets its own copy of the key, so your password unlocks it from now on.</p><div class='rmmsg'></div><div class='rmrow'><button class='n'>Cancel</button><button class='y pri'>Unlock</button></div></div>`;
 const P=maskInput(o.querySelector('.pw')),m=o.querySelector('.rmmsg');document.body.appendChild(o);o.querySelector('.n').onclick=()=>o.remove();
 o.querySelector('.y').onclick=async()=>{const s=VAULT50||await vStatus();if(!s||!s.data||!s.data.rec){m.textContent='There is no recovery key data.';return}
  const pw=P.get();if(!(await pwCheck(USER,pw))){m.textContent='Wrong password.';return}
  let dk;try{dk=await vUnwrap(vRecNorm(o.querySelector('.rk').value).replace(/-/g,''),s.data.rec)}catch(e){m.textContent='That is not the recovery key.';hlNote('vault','a wrong recovery key was entered');return}
  try{await vUnlock(dk);await vAddEntry(USER,pw,s.data);o.remove();notify('Your data is unlocked','Your password unlocks it from now on.');redraw&&redraw()}catch(e){m.textContent=e.message||'It could not be unlocked.'}}}
/* Security Settings › Data encryption */
function vSection(c,W){if(!RD)return;const box=document.createElement('div');box.className='v50';
 box.innerHTML=`<h4 class='sh4'>Data encryption</h4><p class='msg vt' style='margin:2px 0 6px'></p><div class='vp' style='font-size:13px;margin-bottom:6px'></div><div class='rmrow' style='justify-content:flex-start;gap:6px'></div>`;
 const fw=c.querySelector('.fw50');if(fw&&fw.nextSibling)c.insertBefore(box,fw.nextSibling);else c.appendChild(box);
 const t=box.querySelector('.vt'),pp=box.querySelector('.vp'),row=box.querySelector('.rmrow'),adm=isAdmin();
 const btn=(l,f,pri)=>{const b=document.createElement('button');b.textContent=l;if(pri)b.className='pri';b.onclick=f;row.appendChild(b);return b};
 const draw=async()=>{row.innerHTML='';pp.innerHTML='';const s=await vStatus();
  if(!s){t.textContent='Not available.';return}
  const who=s.data&&s.data.accts?ACCTS.filter(a=>a.id&&s.data.accts[a.id]).map(a=>a.n):[];
  const parts=()=>{for(const k in VLT_PARTS){const d=document.createElement('div');d.textContent=(s.moved[k]?'🔒 ':'○ ')+VLT_PARTS[k]+(s.moved[k]?': encrypted':': not moved yet');pp.appendChild(d)}
   const d=document.createElement('div');d.style.color='#6b7280';d.textContent='Accounts whose password unlocks it: '+(who.length?who.join(', '):'none yet')+'. Not encrypted: the system itself (it is signed and checked instead), settings, logs and the browser\'s own data (the desktop encrypts each account\'s home folder itself).';pp.appendChild(d)};
  switch(s.state){
   case'unavailable':t.textContent='Encryption needs Rudolf OS installed, or a USB drive that keeps your files.';break;
   case'off':t.textContent='Off. Debian, Linux apps and files kept in Downloads are stored as they are: anyone with this computer\'s drive can read them.';const b=btn('Encrypt my data…',()=>vSetup(draw),1);b.disabled=!adm;if(!adm){const n=document.createElement('span');n.className='msg';n.textContent=' An administrator can turn this on.';row.appendChild(n)}break;
   case'restart':t.textContent='The drive was prepared for encryption. Restart Rudolf OS to continue; after you sign in, your data moves into the encrypted folder.';btn('Restart now',()=>{try{fsFlush()}catch(e){}rdPost('power?action=reboot').catch(()=>{})},1);break;
   case'ready':t.textContent='Set up, waiting to start: it starts when an account that can unlock it signs in with its password.';parts();break;
   case'locked':t.textContent='On, locked: sign in to an account with its password to unlock it. Linux programs cannot start until then.';parts();btn('Unlock with the recovery key…',()=>vRecoverDlg(draw));break;
   case'moving':t.textContent='On, unlocked. Moving your data into the encrypted folder has not finished.';parts();btn('Show progress',()=>vMoveDlg(draw));btn('Move again',async()=>{try{await rdPost('vault/move');vMoveDlg(draw)}catch(e){notify('Encryption',e.message,{kind:'error'})}});break;
   default:t.textContent='On, unlocked. Turned off, this computer\'s drive shows only scrambled names and contents for these:';parts()}};
 draw()}
if(typeof secPanelS=='function'){const _sv50=secPanelS;secPanelS=function(c,W){const r=_sv50.apply(this,arguments);try{vSection(c,W)}catch(e){console.warn('vault50',e)}return r}}
if(RD)setTimeout(()=>vStatus(),7000);
