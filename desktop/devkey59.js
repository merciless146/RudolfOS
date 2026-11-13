/* ===== Rudolf OS 1.5.9 (docs/plan-1.5.9.md 8.1): the publisher key =====
   User Settings › Developer: a person's own publisher key (Ed25519), which signs their games or builds (rpk-tool.py
   build --key, Developer mode, a developer certificate from the Library). Not tied to this computer: the private half is
   kept only in a backup file protected by a passphrase (PBKDF2-SHA-256, 600 000 rounds, AES-GCM), which the person
   copies somewhere safe (a USB stick); the desktop keeps the public half and its fingerprint for the account. A lost
   computer does not lose the key; a stolen backup is useless without the passphrase. The private half is shown only on
   request, after the passphrase, to paste into a GitHub secret (never written unprotected).
   Not here yet: hardware security keys (FIDO2/PIV) and the computer's own key (TPM). */
const DK_FILE='RUDOLF PUBLISHER KEY (BACKUP).JSON';
const dkKey=()=>'pubkey:'+(USER?uname():'');
const dkB64=a=>btoa(String.fromCharCode(...new Uint8Array(a))),dkUnB64=s=>Uint8Array.from(atob(s),c=>c.charCodeAt(0));
async function dkFp(pubB64){const h=new Uint8Array(await crypto.subtle.digest('SHA-256',dkUnB64(pubB64)));return [...h.slice(0,8)].map(x=>x.toString(16).padStart(2,'0')).join('').replace(/(.{4})(?=.)/g,'$1 ').toUpperCase()}
async function dkWrapKey(pass,salt){const b=await crypto.subtle.importKey('raw',new TextEncoder().encode(pass),'PBKDF2',false,['deriveKey']);
 return crypto.subtle.deriveKey({name:'PBKDF2',hash:'SHA-256',salt,iterations:600000},b,{name:'AES-GCM',length:256},false,['encrypt','decrypt'])}
/* a new key pair: the backup (the private half, raw 32 bytes as rpk-tool keeps it, encrypted) and the public half */
async function dkMake(pass){const k=await crypto.subtle.generateKey({name:'Ed25519'},true,['sign','verify']);
 const pub=dkB64(await crypto.subtle.exportKey('raw',k.publicKey)),p8=new Uint8Array(await crypto.subtle.exportKey('pkcs8',k.privateKey)),raw=p8.slice(p8.length-32);
 const salt=crypto.getRandomValues(new Uint8Array(16)),iv=crypto.getRandomValues(new Uint8Array(12)),w=await dkWrapKey(pass,salt);
 const data=await crypto.subtle.encrypt({name:'AES-GCM',iv},w,new TextEncoder().encode(dkB64(raw)));raw.fill(0);
 return {backup:{format:'rudolf-publisher-key 1',pub,kdf:'PBKDF2-SHA256',rounds:600000,salt:dkB64(salt),iv:dkB64(iv),data:dkB64(data),made:new Date().toISOString().slice(0,10)},pub}}
/* the private half from a backup and its passphrase (base64 of the raw 32 bytes, as rpk-tool's .key file holds), or an error */
async function dkOpen(b,pass){if(!b||b.format!='rudolf-publisher-key 1')throw new Error('That is not a Rudolf OS publisher key backup.');
 const w=await dkWrapKey(pass,dkUnB64(b.salt));let t;try{t=await crypto.subtle.decrypt({name:'AES-GCM',iv:dkUnB64(b.iv)},w,dkUnB64(b.data))}catch(e){throw new Error('Wrong passphrase.')}
 return new TextDecoder().decode(t)}
const dkGet=()=>{try{return JSON.parse(localStorage.getItem(dkKey())||'null')}catch(e){return null}};
const dkSet=v=>{try{v?localStorage.setItem(dkKey(),JSON.stringify(v)):localStorage.removeItem(dkKey())}catch(e){}};
function dkAsk(title,html,fields){return new Promise(res=>{const o=document.createElement('div');o.className='rmodal';
 o.innerHTML=`<div class='rmbox' role='dialog' style='max-width:500px'><h3></h3><div class='dkb'>${html}</div><div class='r2'>${fields.map((f,i)=>`<label>${f}</label><input class='dk${i}' autocomplete='off' spellcheck='false'>`).join('')}</div><div class='rmmsg'></div><div class='rmrow'><button class='n'>Cancel</button><button class='y pri'>OK</button></div></div>`;
 o.querySelector('h3').textContent=title;const P=fields.map((f,i)=>maskInput(o.querySelector('.dk'+i)));document.body.appendChild(o);setTimeout(()=>{const i=o.querySelector('.dk0');i&&i.focus()},30);
 o.querySelector('.n').onclick=()=>{o.remove();res(null)};o.querySelector('.y').onclick=()=>res({vals:P.map(p=>p.get()),say:m=>o.querySelector('.rmmsg').textContent=m,close:()=>o.remove()})})}
async function dkMakeUI(after){let a;for(;;){a=await dkAsk('Make a publisher key','<p class="msg">Your publisher key signs your games and builds. Its private half is kept only in a backup file protected by a passphrase: choose a long one you will remember (12 characters or more). Copy the backup file to a USB stick afterwards.</p>',['Passphrase','Again']);
  if(!a)return;if(a.vals[0].length<12){a.close();notify('Publisher key','The passphrase has 12 characters or more.',{kind:'error'});continue}if(a.vals[0]!=a.vals[1]){a.close();notify('Publisher key','The two passphrases are not the same.',{kind:'error'});continue}break}
 a.say('Making the key…');try{const m=await dkMake(a.vals[0]);const path=[...homePath('DOCUMENTS'),DK_FILE];
  if(!vfsWrite(path,JSON.stringify(m.backup,null,1)))throw new Error('The backup could not be saved in Documents.');
  dkSet({pub:m.pub,fp:await dkFp(m.pub),made:m.backup.made});a.close();
  notify('Publisher key made','Its backup is '+path.join('\\')+'. Copy it to a USB stick: without it (and the passphrase) the key cannot be used again.',{kind:'info'})}
 catch(e){a.close();notify('Publisher key',e.message,{kind:'error'})}after&&after()}
async function dkImportUI(after){const p=await pickVfsFile('Choose the publisher key backup (.JSON)',n=>/\.JSON$/i.test(n));if(!p)return;let b;try{b=JSON.parse(node(p))}catch(e){return notify('Publisher key','That is not a publisher key backup.',{kind:'error'})}
 const a=await dkAsk('Use this publisher key','<p class="msg">Its passphrase opens it once, to check it.</p>',['Passphrase']);if(!a)return;
 try{await dkOpen(b,a.vals[0]);dkSet({pub:b.pub,fp:await dkFp(b.pub),made:b.made});a.close();notify('Publisher key','This account uses the publisher key '+(await dkFp(b.pub))+'.')}catch(e){a.close();notify('Publisher key',e.message,{kind:'error'});return}after&&after()}
async function dkShowPrivate(){const p=await pickVfsFile('Choose the publisher key backup (.JSON)',n=>/\.JSON$/i.test(n));if(!p)return;let b;try{b=JSON.parse(node(p))}catch(e){return}
 const a=await dkAsk('Show the private key','<p class="msg">⚠ Only to paste into a GitHub secret (RPK_SIGNING_KEY) for your own builds. Anyone who sees it can sign as you.</p>',['Passphrase']);if(!a)return;
 let raw;try{raw=await dkOpen(b,a.vals[0])}catch(e){a.close();notify('Publisher key',e.message,{kind:'error'});return}a.close();
 const o=document.createElement('div');o.className='rmodal';o.innerHTML=`<div class='rmbox' style='max-width:520px'><h3>Private key</h3><input class='pk' readonly style='width:100%;font-family:monospace'><p class='msg'>It hides itself in 60 seconds.</p><div class='rmrow'><button class='c'>Copy</button><button class='y pri'>Done</button></div></div>`;
 o.querySelector('.pk').value=raw;raw='';document.body.appendChild(o);const close=()=>o.remove();o.querySelector('.y').onclick=close;setTimeout(close,60000);
 o.querySelector('.c').onclick=()=>navigator.clipboard.writeText(o.querySelector('.pk').value).then(()=>notify('Copied','Paste it into the GitHub secret, then copy something else.',{quiet:true})).catch(()=>{})}
/* User Settings › Developer */
if(typeof usrPanelS=='function'){const _u=usrPanelS;usrPanelS=function(c,W){const r=_u.apply(this,arguments);try{if(!USER||USER.r=='guest')return r;
 const d=document.createElement('div');d.className='dk59';c.appendChild(d);
 const draw=()=>{const k=dkGet();d.innerHTML=`<h4 class='sh4'>Developer: publisher key</h4>${k?`<div class='r2'><label>Public key</label><div><input class='dkp' readonly style='width:100%;font-family:monospace'><div class='msg'>Fingerprint ${pkgEsc(k.fp)} · made ${pkgEsc(k.made||'')}</div></div></div>`:"<p class='msg'>A publisher key signs the games and Rudolf OS builds you make (see docs/custom-builds.md). Its public half goes in Developer mode or in a Library developer certificate.</p>"}
<div class='rmrow' style='justify-content:flex-start'>${k?"<button class='dkc'>Copy the public key</button><button class='dks'>Show the private key…</button><button class='dkf'>Forget it here</button>":"<button class='dkm pri'>Make a publisher key…</button><button class='dki'>Use a backup…</button>"}</div>`;
  const q=x=>d.querySelector(x);if(k)q('.dkp').value=k.pub;
  q('.dkm')&&(q('.dkm').onclick=()=>dkMakeUI(draw));q('.dki')&&(q('.dki').onclick=()=>dkImportUI(draw));
  q('.dkc')&&(q('.dkc').onclick=()=>navigator.clipboard.writeText(k.pub).then(()=>notify('Copied','The public key',{quiet:true})).catch(()=>{}));
  q('.dks')&&(q('.dks').onclick=()=>dkShowPrivate());q('.dkf')&&(q('.dkf').onclick=()=>{dkSet(null);draw()})};
 draw()}catch(e){console.warn(e)}return r}}
