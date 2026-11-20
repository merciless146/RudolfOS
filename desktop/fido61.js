/* ===== Rudolf OS 1.6.1: security keys that unlock the files (docs/plan-1.6.1.md Part 18; Part 17 items 1, 2, 4 and 6;
   docs/security-keys.md) =====
   rudolfd talks to the FIDO2 keys and decides (rudolfd-fido.inc): this page only asks it. User Settings › Sign-in options ›
   Security keys: add a key (the password, the key touched twice, its PIN when it has one: never kept), remove one, and
   "Require a security key" once there are two (a spare): rudolfd checks the whole way with a touch, then the password's
   own copies of the file key and of an administrator's private key are deleted from the account, so a copy of the disk
   opens nothing without one of the keys. Signing in (and the lock screen) with keys: the password, then "Touch your
   security key"; rudolfd hands back the password's key and the file key (as Native's session does), and the page opens
   nothing itself. A key is the second step: an account with keys is not asked for a code too.
   The old keys checked by this page only (settings25.js passkeyAdd, 1.4: WebAuthn) keep working on Standard until the
   person sets keys up again here, with their consent; the old one is then removed (Native and Lite then accept the account).
   An administrator who resets an account's password removes its keys too (rudolfd keys/reset): the person adds them again.
   The account keeps one more box, "fks" (the password's secret, sealed with the password's key as its other boxes):
   a new password seals it again (acRekey below), so changing the password needs no key. ===== */
const FIDO={pv:null};
const fidoHex=h=>Uint8Array.from(String(h||'').match(/../g)||[],x=>parseInt(x,16));
const fidoHas=a=>!!(a&&a.fido&&a.fido.n);
async function fidoReq(path,body){if(!RD)throw new Error('This needs the installed Rudolf OS.');
 const r=await fetch('/api/'+path,{method:body?'POST':'GET',headers:{'X-Rudolf-Token':RD.token},body:body?JSON.stringify(body):undefined});
 let j={};try{j=await r.json()}catch(e){}j.status=r.status;return j}
/* a dialog that says what to do with the key; .msg(text, error), .retry() waits for Try again / Cancel, .close() */
function fidoDlg(title){const o=document.createElement('div');o.className='rmodal fidodlg';
 o.innerHTML="<div class='rmbox' role='dialog' aria-modal='true' aria-labelledby='fidot' style='max-width:400px;text-align:center'><h3 id='fidot'></h3><div class='fidoic' aria-hidden='true' style='font-size:42px;margin:6px 0'>🔑</div><p class='msg fidom' role='status' aria-live='polite'></p><div class='rmrow'><button class='n'>Cancel</button><button class='pri y' style='display:none'>Try again</button></div></div>";
 o.querySelector('#fidot').textContent=title;const m=o.querySelector('.fidom'),y=o.querySelector('.y');let wait=null;const D={cancelled:false};
 o.querySelector('.n').onclick=()=>{D.cancelled=true;if(wait)wait(false);D.close()};y.onclick=()=>{y.style.display='none';if(wait)wait(true)};
 o.addEventListener('keydown',e=>{e.stopPropagation();if(e.key=='Escape')o.querySelector('.n').click()});
 D.msg=(t,err)=>{m.textContent=t;m.style.color=err?'#dc2626':''};D.retry=()=>new Promise(r=>{y.style.display='';y.focus();wait=v=>{wait=null;r(v)}});
 D.close=()=>o.remove();document.body.appendChild(o);setTimeout(()=>o.querySelector('.n').focus(),30);return D}
/* the second step with a key for account A whose password PW was right: null (it has no keys: as before), false (refused
   or cancelled), or the keys for userLogin ({pk, fk}) */
async function fidoSignin(a,pw,title){
 if(!a||a.r=='guest')return null;
 if(!RD)return fidoHas(a)&&a.fido.req?(notify('Security key','This account needs its security key, which only the installed Rudolf OS can read.',{kind:'error'}),false):null;
 let st={};try{st=await fidoReq('desktop/keys?acct='+encodeURIComponent(a.id||''))}catch(e){}
 if(st.ok&&!Array.isArray(st.keys))st.ok=false;
 if(!st.ok){if(fidoHas(a)&&a.fido.req){notify('Security key','Rudolf OS\'s system service did not answer, and this account needs its security key.',{kind:'error'});return false}return null}
 if(st.bad){notify('Security key','This account\'s security keys could not be read: see Settings › System health.',{kind:'error'});return false}
 if(!st.keys||!st.keys.length){if(fidoHas(a)){delete a.fido;saveUser()}return null}
 const D=fidoDlg(title||'Two-step sign-in');D.msg(`${a.n}: checking…`);
 let r=await fidoReq('desktop/keys/signin',{n:a.n,p:pw});
 if(!(r.status==401&&r.need=='key')){D.close();notify('Security key',r.error||'The sign-in did not work.',{kind:'error'});return false}
 for(;;){D.msg(`${a.n}: touch your security key (any of yours). If it is not plugged in, plug it in first.`);
  r=await fidoReq('desktop/keys/signin',{n:a.n,p:pw,k:true});if(D.cancelled)return false;
  if(r.ok){D.close();FIDO.pv=r.pv?b64d(r.pv):null;if(r.fks)a.fks=r.fks;a.fido={n:st.keys.length,req:!!r.req};saveUser();try{sfx('login')}catch(e){}
   return{pk:fidoHex(r.pk),fk:fidoHex(r.fk)}}
  try{sfx('error')}catch(e){}D.msg(r.error||'The security key did not answer.',true);
  if(r.status==429||!(await D.retry())){D.close();return false}}}
/* the lock screen's step: 'none' (no keys), 'ok' or 'no' */
async function fidoGate(a,pw,title){const g=await fidoSignin(a,pw,title||'Unlock');FIDO.pv=null;return g===null?'none':g?'ok':'no'}
/* ---- what happens around the sign-in ---- */
if(typeof acctKeys=='function'){const _ak=acctKeys;acctKeys=async function(a,pw){
 /* an administrator who requires a key: its private key came from rudolfd (the password's copy is gone) */
 if(a&&a.r=='admin'&&FIDO.pv&&a.pub&&!a.priv){try{ADMPRIVRAW=FIDO.pv;ADMPRIV=await crypto.subtle.importKey('pkcs8',FIDO.pv,ECP,false,['deriveBits'])}catch(e){}}
 FIDO.pv=null;
 if(a&&a.fido&&a.fido.req&&!FKRAW)throw new Error('This account needs its security key.');   /* (never a new file key in place of the keys') */
 const r=await _ak.apply(this,arguments);
 /* required (perhaps from another engine, or a copy made before): the password's copies go */
 if(a&&a.fido&&a.fido.req&&(a.fk||a.priv)){delete a.fk;delete a.priv;saveUser()}
 return r}}
/* a new password: the account's box for the keys sealed again with the new password's key (no key needed); a required
   account's password copies are not made again */
if(typeof acRekey=='function'){const _rk=acRekey;acRekey=async function(a,pw){let pws=null;if(a&&a.fks&&PKEY){try{pws=await acDec(PKEY,a.fks)}catch(e){}}
 const r=await _rk.apply(this,arguments);
 if(pws)a.fks=await acEnc(PKEY,pws);
 if(a&&a.fido&&a.fido.req){delete a.fk;delete a.priv}
 return r}}
/* an administrator resets another account's password: its keys are removed first (rudolfd), or nothing changes */
if(typeof acReset=='function'){const _rs=acReset;acReset=async function(o,pw){
 if(RD&&o&&o.id){let st={};try{st=await fidoReq('desktop/keys?acct='+encodeURIComponent(o.id))}catch(e){}
  if(st.ok&&st.keys&&st.keys.length){const c=await elevate(`Remove the security keys of ${o.n} (they add them again after the new password)`,{cred:true});if(!c||c===true)throw new Error('Not allowed.');
   let r=await fidoReq('desktop/keys/reset',{n:c.n,p:c.p,acct:o.id});
   if(r.status==401&&r.need=='key'){const D=fidoDlg('Administrator permission');D.msg(`${c.n}: touch your security key.`);r=await fidoReq('desktop/keys/reset',{n:c.n,p:c.p,acct:o.id,k:true});D.close()}
   if(!r.ok)throw new Error(r.error||'The keys could not be removed.')}}
 const kept=await _rs.apply(this,arguments);delete o.fks;delete o.fido;saveUser();return kept}}
/* ---- User Settings › Sign-in options › Security keys ---- */
/* a dialog asking for the password (and, for adding, a name and the key's PIN when it has one): the values, or null */
function fidoAsk(title,text,o){o=o||{};return new Promise(res=>{const d=document.createElement('div');d.className='rmodal fidoask';
 d.innerHTML="<div class='rmbox' role='dialog' aria-modal='true' aria-labelledby='fidoat' style='max-width:440px'><h3 id='fidoat'></h3><p class='msg fidoaw'></p><div class='r2'><label class='fnl'>Name</label><input class='fnm' maxlength='40' placeholder='Blue YubiKey'><label>Password</label><input class='fpw' type='password' aria-label='Your password'><label class='fpl'>The key's PIN</label><input class='fpin' type='password' inputmode='numeric' aria-label='The key’s PIN' placeholder='Only if it asks'></div><p class='msg fidoam' role='alert'></p><div class='rmrow'><button class='n'>Cancel</button><button class='pri y'>Continue</button></div></div>";
 const q=s=>d.querySelector(s);q('#fidoat').textContent=title;q('.fidoaw').textContent=text||'';
 if(!o.name){q('.fnl').style.display=q('.fnm').style.display='none'}else q('.fnm').value=o.name===true?'':o.name;
 if(!o.pin){q('.fpl').style.display=q('.fpin').style.display='none'}
 if(o.err)q('.fidoam').textContent=o.err;
 const done=v=>{d.remove();res(v)};q('.n').onclick=()=>done(null);
 q('.y').onclick=()=>{const p=q('.fpw').value;if(!p){q('.fidoam').textContent='Type your password.';return}done({p,name:q('.fnm').value.trim(),pin:q('.fpin').value})};
 d.addEventListener('keydown',e=>{e.stopPropagation();if(e.key=='Enter')q('.y').click();if(e.key=='Escape')done(null)});
 document.body.appendChild(d);setTimeout(()=>(o.name?q('.fnm'):q('.fpw')).focus(),30)})}
/* a request that may need the key touched: the first answer 401 need "key" shows the touch dialog and asks again with k */
async function fidoTouchReq(path,body,title,text){let r=await fidoReq(path,body);if(!(r.status==401&&r.need=='key'))return r;
 const D=fidoDlg(title);for(;;){D.msg(text);r=await fidoReq(path,{...body,k:true});if(D.cancelled)return{ok:false,cancelled:true};
  if(r.ok||r.need=='pin'){D.close();return r}D.msg(r.error||'The security key did not answer.',true);if(!(await D.retry())){D.close();return{ok:false,cancelled:true}}}}
function fidoRow(d){const box=document.createElement('div');box.className='r2 fidorow';
 box.innerHTML="<label>Security keys</label><div><span class='fks'></span><div class='fkl'></div><button class='fka'>Add a security key…</button> <label class='ck'><input type='checkbox' class='fkq'> Require a security key (protects your files)</label><div class='msg fkw'></div><div class='msg fko' style='display:none'></div></div>";
 d.appendChild(box);const q=s=>box.querySelector(s);let st={keys:[]};
 const draw=async()=>{try{st=await fidoReq('desktop/keys?acct='+encodeURIComponent(USER.id||''))}catch(e){st={ok:false}}
  if(!Array.isArray(st.keys)){st.ok=false;st.keys=[]}
  const n=st.ok?st.keys.length:0,L=q('.fkl');L.textContent='';
  q('.fks').textContent=!st.ok?'Needs the installed Rudolf OS.':st.bad?'The keys’ record is damaged: see System health.':!st.lib?'Not available: libfido2 is missing from this Rudolf OS.':n?(st.req?`${n} keys; your files need one of them.`:`${n===1?'1 key':n+' keys'}; asked for after your password.`):'None.';
  (st.keys||[]).forEach(k=>{const r=document.createElement('div');r.className='fkr';const s=document.createElement('span');s.textContent='🔑 '+k.name+(k.last?' · last used '+new Date(k.last*1000).toLocaleDateString():'');
   const b=document.createElement('button');b.textContent='Remove';b.onclick=()=>fidoRemove(k).then(draw);r.appendChild(s);r.appendChild(document.createTextNode(' '));r.appendChild(b);L.appendChild(r)});
  q('.fka').disabled=!st.ok||!st.lib||n>=5;const c=q('.fkq');c.checked=!!st.req;c.disabled=!st.ok||n<2&&!st.req;
  q('.fkw').textContent=n<2?'Add two keys (one as a spare, kept somewhere safe) to be able to require one: then a copy of your disk opens nothing without one of your keys and your password. A code from an authenticator app guards the sign-in only.':
   st.req?(acAllowed(USER)?'Administrators can still open your files with their own copy (if you lose all your keys, that is the way back: docs/security-keys.md); with that turned off for your account, nobody can open them without one of your keys.':'If you lose all your keys, nobody can open your files.'):'Your files are still opened by your password alone until you require a key.';
  const old=!!USER.pk;q('.fko').style.display=old?'':'none';if(old)q('.fko').textContent='Your old security key is checked by the sign-in screen only, so it does not protect your files and Native and Lite cannot use it. Add your keys here again: the old one is then removed.';
  if(USER.fido&&(USER.fido.n!=n||USER.fido.req!=!!st.req)&&st.ok&&!st.bad){if(n)USER.fido={n,req:!!st.req};else delete USER.fido;saveUser()}};
 q('.fka').onclick=()=>fidoAdd(st).then(draw);
 q('.fkq').onchange=async e=>{const on=e.target.checked;e.target.checked=!on;await fidoRequire(on);draw()};
 draw();return box}
async function fidoAdd(st){const n=(st.keys||[]).length,old=!!USER.pk;
 let v=await fidoAsk('Add a security key',(st.req?'First touch one of your keys; then plug in the new one and touch it twice. ':'Plug in the key; you will touch it twice (once to make its credential, once to read its secret). ')+
  (old?'Your old security key (checked by the sign-in screen only) is removed once this is added: OK?':''),{name:n?'Spare':'My security key',pin:false});
 if(!v)return;let body={n:USER.n,p:v.p,name:v.name||'Security key'};
 for(;;){const r=await fidoTouchReq('desktop/keys/add',body,'Add a security key',st.req?'Touch one of your keys; then plug in the new key and touch it twice.':'Touch the new key (twice).');
  if(r.cancelled)return;
  if(r.need=='pin'){const w=await fidoAsk('The key’s PIN','This key has a PIN. It is asked for only now, to add the key, and never kept.',{pin:true,err:/Wrong/.test(r.error||'')?r.error:''});if(!w)return;body={...body,p:w.p,pin:w.pin};continue}
  if(!r.ok){notify('Security key',r.error||'The key was not added.',{kind:'error'});return}
  if(r.fks)USER.fks=r.fks;USER.fido={n:r.n,req:!!r.req};if(old)delete USER.pk;saveUser();try{window.__rudolfLsFlush&&__rudolfLsFlush()}catch(e){}
  try{logEvt('sys',`${USER.n} added a security key`)}catch(e){}
  notify('Security key added',r.n<2?'Add a second one as a spare: then you can require a key to protect your files.':'You will be asked to touch one of your keys after your password.',{app:'Settings'});return}}
async function fidoRemove(k){const v=await fidoAsk('Remove '+k.name,'Signing in will no longer accept this key.');if(!v)return;
 const r=await fidoReq('desktop/keys/remove',{n:USER.n,p:v.p,i:k.i});if(!r.ok)return notify('Security key',r.error||'Not removed.',{kind:'error'});
 if(r.n)USER.fido={n:r.n,req:!!r.req};else delete USER.fido;saveUser();try{logEvt('sys',`${USER.n} removed a security key`)}catch(e){}}
async function fidoRequire(on){
 const v=await fidoAsk(on?'Require a security key':'Stop requiring a security key',on?'Your password’s own copy of your file key is deleted: from then on your files open only with your password and one of your keys. Keep the spare somewhere safe. You will touch one of your keys now.':'Your files will open with your password alone again (your keys still asked for at sign-in). You will touch one of your keys now.');
 if(!v)return;
 const r=await fidoTouchReq('desktop/keys/require',{n:USER.n,p:v.p,on},'Security key','Touch one of your security keys.');
 if(!r.ok){if(!r.cancelled)notify('Security key',r.error||'Not changed.',{kind:'error'});return}
 if(on){delete USER.fk;delete USER.priv}
 else{if(FKRAW&&PKEY)USER.fk=await acEnc(PKEY,FKRAW);if(ADMPRIVRAW&&PKEY)USER.priv=await acEnc(PKEY,ADMPRIVRAW)}
 USER.fido={...(USER.fido||{}),req:!!on};saveUser();try{window.__rudolfLsFlush&&__rudolfLsFlush()}catch(e){}
 try{logEvt('sys',`${USER.n} ${on?'now requires':'no longer requires'} a security key`)}catch(e){}
 notify('Security key',on?'Your files now need your password and one of your keys.':'Your files open with your password alone again.',{app:'Settings'})}
if(typeof usrPanelS=='function'){const _uf=usrPanelS;usrPanelS=function(c,W){const r=_uf.apply(this,arguments);try{if(USER&&USER.r!='guest'&&RD){
 const pk=c.querySelector('.pka'),row=pk&&pk.closest('.r2');
 if(row){/* (the old page-only key: no new ones; Remove stays for one that is there) */pk.style.display='none';const m=row.querySelector('.msg');if(m&&!USER.pk)m.textContent='Security keys are under Security keys below.'}
 fidoRow(row?row.parentElement:c)}}catch(e){console.warn('fido61',e)}return r}}
