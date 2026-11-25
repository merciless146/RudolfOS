/* ===== Rudolf OS 1.6.3: fingerprint readers (docs/plan-1.6.3.md Part 2) =====
   rudolfd and its fprint helper decide (rudolfd-fprint.inc, rudolf-fprint.c): this page only asks them, through fidoReq
   (fido61.js), which keeps each answer's status. An account's fingerprints are kept encrypted with its files' key, so
   they exist only for an account whose files are on this disk, and a finger is taken only once the account has signed
   in with its password since the computer started.
   Step 2: User Settings › Sign-in options › Fingerprints: the account's own (added with its password: a finger chosen,
   then the reader's scans shown as they come; removed), and for an administrator every other account's (Fingerprints…:
   the administrator's prompt; adding needs that account's files open and its owner's finger on the reader). A reader
   libfprint does not support is named plainly; with none the row says so. Guest has none. ===== */
const FP_FINGERS=[['right-index','Right index finger'],['left-index','Left index finger'],['right-thumb','Right thumb'],['left-thumb','Left thumb'],
 ['right-middle','Right middle finger'],['left-middle','Left middle finger'],['right-ring','Right ring finger'],['left-ring','Left ring finger'],
 ['right-little','Right little finger'],['left-little','Left little finger']];
const fpName=f=>(FP_FINGERS.find(x=>x[0]==f)||[f,f])[1];
async function fpStatus(id){try{const r=await fidoReq('fp/status?acct='+encodeURIComponent(id||''));return r&&r.status==200?r:{ok:false,status:r&&r.status}}catch(e){return{ok:false}}}
/* what the row says about the reader and the account: one plain sentence */
function fpSays(st,self){
 if(!st.ok)return 'Needs the installed Rudolf OS.';
 if(!st.helper)return 'Fingerprint readers are not in this Rudolf OS.';
 const rd=(st.readers||[])[0],un=(st.unsupported||[])[0];
 if(!rd&&un)return `The fingerprint reader “${un.name||'Fingerprint reader'}” (USB ${un.usb}) cannot be used: libfprint, which Rudolf OS reads fingerprints with, does not support it.`;
 if(!rd)return 'No fingerprint reader is connected. A touchpad or a touchscreen cannot read a fingerprint: a fingerprint reader is needed (in the power button, beside the keyboard, or one plugged into USB).';
 if(!st.disk)return (self?'Your files are':'This account’s files are')+' not on this computer’s disk, so fingerprints cannot be kept with them.';
 const n=(st.fingers||[]).length,base=`${rd.name}. `;
 if(!n)return base+'No fingerprints yet.';
 return base+(n==1?'1 fingerprint':n+' fingerprints')+(st.why=='tries'?'. Five fingers were not recognised: the password is needed once.':st.why=='restart'?'. After a restart or signing out, the password is asked first.':'. A finger unlocks the lock screen and answers administrator prompts; the password always works too.')}
/* the account's own password, or (another account's) an administrator's: {n,p} or null */
async function fpCred(o,what){if(o===USER){const v=typeof fidoAsk=='function'?await fidoAsk(what,'Type your password to change your fingerprints.'):null;return v?{n:USER.n,p:v.p}:null}
 const c=await elevate(what+' ('+o.n+')',{cred:1});return c&&c.n?c:null}
/* the finger and the scans: a dialog that follows fp/progress while fp/enroll waits; true when added */
async function fpEnroll(o,done){const st=await fpStatus(o.id);const have=new Set(st.fingers||[]);
 const left=FP_FINGERS.filter(x=>!have.has(x[0]));if(!left.length){notify('Fingerprints','Every finger is added already.',{app:'Settings'});return false}
 const c=await fpCred(o,'Add a fingerprint');if(!c)return false;
 const d=document.createElement('div');d.className='rmodal fpdlg';
 d.innerHTML="<div class='rmbox' role='dialog' aria-modal='true' aria-labelledby='fpt' style='max-width:420px;text-align:center'><h3 id='fpt'>Add a fingerprint</h3><div class='r2' style='text-align:left'><label>Finger</label><select class='fpf'></select></div><div class='fpic' aria-hidden='true' style='font-size:42px;margin:6px 0'>☝️</div><p class='msg fpm' aria-live='polite'></p><div class='fpbar' style='height:6px;background:#e5e7eb;border-radius:3px;overflow:hidden'><div class='fpbv' style='height:100%;width:0;background:#16a34a'></div></div><div class='rmrow'><button class='n'>Cancel</button><button class='y pri'>Start</button></div></div>";
 const q=s=>d.querySelector(s),sel=q('.fpf');left.forEach(([v,t])=>{const op=document.createElement('option');op.value=v;op.textContent=t;sel.appendChild(op)});
 q('.fpm').textContent=o===USER?'Choose a finger, then press Start and put it on the reader.':`${o.n} chooses a finger, presses Start and puts it on the reader.`;
 document.body.appendChild(d);setTimeout(()=>q('.y').focus(),30);
 return await new Promise(res=>{let busy=false,poll=null;const close=v=>{clearInterval(poll);d.remove();res(v)};
  q('.n').onclick=async()=>{if(busy){try{await fidoReq('fp/cancel',{})}catch(e){}}else close(false)};
  d.addEventListener('keydown',e=>{e.stopPropagation();if(e.key=='Escape')q('.n').click()});
  q('.y').onclick=async()=>{if(busy)return;busy=true;q('.y').style.display='none';sel.disabled=true;q('.fpm').textContent='Put your finger on the reader.';
   poll=setInterval(async()=>{try{const p=await fidoReq('fp/progress');if(p&&p.of){q('.fpbv').style.width=Math.round(100*Math.max(0,p.stage)/p.of)+'%';if(p.msg)q('.fpm').textContent=p.msg+(p.stage>0&&p.stage<p.of?` (${p.stage} of ${p.of})`:'')}}catch(e){}},500);
   let r;try{r=await fidoReq('fp/enroll',{acct:o.id,finger:sel.value,n:c.n,p:c.p})}catch(e){r={ok:false,error:e.message}}
   clearInterval(poll);busy=false;
   if(r&&r.ok){q('.fpbv').style.width='100%';q('.fpm').textContent=fpName(sel.value)+' added.';try{logEvt('sys',`${USER.n} added a fingerprint to ${o.n}`)}catch(e){}
    setTimeout(()=>close(true),700);return}
   if(r&&r.code=='cancelled'){close(false);return}
   q('.fpm').textContent=(r&&r.error)||'The fingerprint was not added.';q('.fpm').style.color='#dc2626';q('.n').textContent='Close';q('.n').onclick=()=>close(false)}})}
async function fpRemove(o,finger){const c=await fpCred(o,finger?'Remove a fingerprint':'Remove every fingerprint');if(!c)return false;
 const r=await fidoReq('fp/remove',finger?{acct:o.id,finger,n:c.n,p:c.p}:{acct:o.id,all:true,n:c.n,p:c.p});
 if(!r.ok){notify('Fingerprints',r.error||'Not removed.',{kind:'error'});return false}
 try{logEvt('sys',`${USER.n} removed ${finger?'a fingerprint':'every fingerprint'} of ${o.n}`)}catch(e){}return true}
/* one account's fingers, drawn into L, with Remove buttons, and the Add button's state */
async function fpDraw(o,box){const q=s=>box.querySelector(s);const st=await fpStatus(o.id);q('.fps').textContent=fpSays(st,o===USER);
 const L=q('.fpl');L.textContent='';(st.fingers||[]).forEach(f=>{const r=document.createElement('div');r.className='fpr';const s=document.createElement('span');s.textContent='☝️ '+fpName(f);
  const b=document.createElement('button');b.textContent='Remove';b.onclick=async()=>{if(await fpRemove(o,f))fpDraw(o,box)};r.appendChild(s);r.appendChild(document.createTextNode(' '));r.appendChild(b);L.appendChild(r)});
 const can=st.ok&&st.helper&&(st.readers||[]).length&&st.disk;q('.fpa').disabled=!can||!st.open;
 q('.fpw').textContent=can&&!st.open?(o===USER?'':`${o.n}’s files are not open: ${o.n} adds fingerprints when signed in.`):'';
 const ra=q('.fpra');if(ra)ra.style.display=(st.fingers||[]).length>1?'':'none';return st}
function fpBox(o){const box=document.createElement('div');box.className='fprow';
 box.innerHTML="<span class='fps'></span><div class='fpl'></div><button class='fpa'>Add a fingerprint…</button> <button class='fpra' style='display:none'>Remove all</button><div class='msg fpw'></div>";
 box.querySelector('.fpa').onclick=async()=>{if(await fpEnroll(o))fpDraw(o,box)};
 box.querySelector('.fpra').onclick=async()=>{if(await fpRemove(o,null))fpDraw(o,box)};return box}
/* an administrator's Fingerprints… for another account */
function fpOther(o){const d=document.createElement('div');d.className='rmodal fpother';
 d.innerHTML="<div class='rmbox' role='dialog' aria-modal='true' aria-labelledby='fpot' style='max-width:480px'><h3 id='fpot'></h3><div class='fpbox'></div><div class='rmrow'><button class='n pri'>Close</button></div></div>";
 d.querySelector('#fpot').textContent='☝️ Fingerprints of '+o.n;const box=fpBox(o);d.querySelector('.fpbox').appendChild(box);
 d.querySelector('.n').onclick=()=>d.remove();d.addEventListener('keydown',e=>{e.stopPropagation();if(e.key=='Escape')d.remove()});
 document.body.appendChild(d);fpDraw(o,box)}
function fpRow(c){const row=document.createElement('div');row.className='r2 fpsec';
 row.innerHTML="<label>Fingerprints</label><div class='fpme'></div>";const box=fpBox(USER);row.querySelector('.fpme').appendChild(box);
 if(isAdmin()){const others=ACCTS.filter(o=>o!==USER&&o.r!='guest');if(others.length){const l=document.createElement('label');l.textContent='Other accounts’ fingerprints';
  const w=document.createElement('div');w.className='fpo';others.forEach(o=>{const b=document.createElement('button');b.textContent=o.n+'…';b.title='Fingerprints of '+o.n;b.onclick=()=>fpOther(o);w.appendChild(b);w.appendChild(document.createTextNode(' '))});
  row.appendChild(l);row.appendChild(w)}}
 c.appendChild(row);fpDraw(USER,box)}
if(typeof usrPanelS=='function'){const _ufp=usrPanelS;usrPanelS=function(c,W){const r=_ufp.apply(this,arguments);try{if(USER&&USER.r!='guest'&&RD){
 const fr=c.querySelector('.fidorow'),pk=c.querySelector('.pka'),host=fr?fr.parentElement:pk&&pk.closest('.r2')?pk.closest('.r2').parentElement:c;fpRow(host)}}catch(e){console.warn('fprint63',e)}return r}}
