/* ===== Rudolf OS 1.5.9 (docs/plan-1.5.9.md 8.3): Developer mode, and Official or Custom =====
   Settings › Update and recovery › Developer mode (administrators): one more trusted key, a developer's publisher key,
   and their update address, so a build of their own installs like an update (rudolf-update developer on KEY URL; the
   key is kept on RUDOLFDATA, never in the system image, and the official key stays). A strong warning, the
   administrator's password every time and a typed confirmation first. Turning it off removes the key and the address.
   Settings › About and the update card say whether the running system is the Official Rudolf OS (every file listed in a
   release list signed by the official key) or a Custom build, with the key that signed it; the recovery system stays
   the official one either way (8.4). */
const DEVM_CONFIRM='I TRUST THIS DEVELOPER';
const devmFp=k=>k?String(k).slice(0,8)+'…'+String(k).slice(-6,-1):'';
/* the build in words: "Official Rudolf OS", "Custom build (signed by key ABCD…)", or '' (unknown, or no system updates) */
function devmBuild(s){if(!s||!s.signer)return '';return s.signer=='official'?'Official Rudolf OS':s.signer=='custom'?`Custom build (signed by key ${devmFp(s.signerKey)})`:''}
async function devmOn(after){if(!isAdmin())return notify('Developer mode','Only an administrator can turn on Developer mode.',{kind:'error'});
 const o=document.createElement('div');o.className='rmodal';
 o.innerHTML=`<div class='rmbox' role='alertdialog' aria-label='Developer mode' style='max-width:540px'><h3>⚠️ Turn on Developer mode</h3>
<p>Rudolf OS will also install system updates signed by <b>a developer's key</b>, from <b>their address</b>. A build from someone else can do anything on this computer: read every account's files, watch what you type, or stop working. Only do this for your own builds, or a developer you trust completely.</p>
<p class='msg'>The official Rudolf OS key stays, and Recovery mode always starts the official Rudolf OS, so you can go back.</p>
<div class='r2'><label>Publisher key</label><input class='dk' maxlength='44' placeholder='44 characters, from the developer' autocomplete='off' spellcheck='false'><label>Update address</label><input class='du' placeholder='https://…/beta' autocomplete='off' spellcheck='false'><label>Type ${DEVM_CONFIRM}</label><input class='dc' autocomplete='off' spellcheck='false'></div><div class='rmmsg'></div><div class='rmrow'><button class='n'>Cancel</button><button class='y pri' style='background:#b91c1c;border-color:#b91c1c'>Turn on</button></div></div>`;
 document.body.appendChild(o);o.querySelector('.dk').focus();const m=o.querySelector('.rmmsg');
 o.querySelector('.n').onclick=()=>o.remove();
 o.querySelector('.y').onclick=async()=>{const k=o.querySelector('.dk').value.trim(),u=o.querySelector('.du').value.trim(),c=o.querySelector('.dc').value.trim();
  if(!/^[A-Za-z0-9+/]{43}=$/.test(k))return m.textContent='The publisher key is 44 characters ending in "=".';
  if(!/^https:\/\/[^\s'"]+$/.test(u))return m.textContent='The update address starts with https://';
  if(c.toUpperCase()!=DEVM_CONFIRM)return m.textContent=`Type ${DEVM_CONFIRM} to confirm.`;
  o.remove();window.__elevTrust=0;if(!await elevate('Turn on Developer mode: install system updates signed by key '+devmFp(k)+' from '+u))return;
  try{const r=await rdPost('sys/developer?on=1&key='+encodeURIComponent(k)+'&url='+encodeURIComponent(u));notify('Developer mode',r.message);typeof logEvt=='function'&&logEvt('sys',`${USER?USER.n:'?'} turned on Developer mode (key ${devmFp(k)}, ${u})`)}catch(e){notify('Developer mode',e.message,{kind:'error'})}
  after&&after()}}
async function devmOff(after){if(!isAdmin())return notify('Developer mode','Only an administrator can turn off Developer mode.',{kind:'error'});
 window.__elevTrust=0;if(!await elevate('Turn off Developer mode: only official Rudolf OS releases are installed again'))return;
 try{const r=await rdPost('sys/developer?on=0');notify('Developer mode',r.message);typeof logEvt=='function'&&logEvt('sys',`${USER?USER.n:'?'} turned off Developer mode`)}catch(e){notify('Developer mode',e.message,{kind:'error'})}
 after&&after()}
/* 8.5: Return to official Rudolf OS: Developer mode off, the official system at the next restart (the recovery
   system, or the newest official release downloaded), keeping accounts, files and apps or erasing them */
function devmReturn(after){if(!isAdmin())return notify('Return to official Rudolf OS','Only an administrator can do this.',{kind:'error'});
 const o=document.createElement('div');o.className='rmodal';
 o.innerHTML=`<div class='rmbox' role='alertdialog' aria-label='Return to official Rudolf OS' style='max-width:520px'><h3>↩️ Return to official Rudolf OS</h3><p>Developer mode is turned off and the official Rudolf OS starts the next time the computer restarts. The custom build is removed once the official one has started.</p>
<label class='ck' style='display:block'><input type='radio' name='dre' value='0' checked> Keep my accounts, files and apps</label><label class='ck' style='display:block'><input type='radio' name='dre' value='1'> Erase everything (a factory reset: every account, file and setting)</label>
<div class='rmrow'><button class='n'>Cancel</button><button class='y pri'>Return to official</button></div></div>`;
 document.body.appendChild(o);o.querySelector('.n').onclick=()=>o.remove();
 o.querySelector('.y').onclick=async()=>{const er=o.querySelector('input[name=dre]:checked').value=='1';o.remove();window.__elevTrust=0;
  if(!await elevate('Return to the official Rudolf OS'+(er?', erasing every account, file and setting':', keeping accounts, files and apps')))return;
  try{const st=await sysuJob('return?erase='+(er?1:0));const j=await rd('job');const msg=((j.log||'').trim().split('\n').pop()||'');
   if(st===0)notify('Return to official Rudolf OS',msg+' Restart when you are ready.',{action:()=>typeof sysuRestart=='function'&&sysuRestart()});else notify('Return to official Rudolf OS',msg||'It did not work.',{kind:'error'})}catch(e){notify('Return to official Rudolf OS',e.message,{kind:'error'})}
  after&&after()}}
/* in Recovery mode, when this computer normally runs a custom build: the way back, at once */
if(RD&&window.RUDOLFD&&RUDOLFD.recovery)setTimeout(async()=>{try{const s=await rd('sys/status');if(s&&s.developer)notify('This computer runs a custom build','Recovery mode is the official Rudolf OS. To go back to it for good, use Return to official Rudolf OS.',{kind:'warning',action:()=>devmReturn()})}catch(e){}},8000);
/* Settings › Update and recovery: the section, below the update card */
{const _up=updPanel;updPanel=function(c){_up.apply(this,arguments);if(!RD)return;const x=c.querySelector('.updx');if(!x)return;
 const box=document.createElement('div');box.className='devm';x.appendChild(box);
 const draw=async()=>{let s=null;try{s=await rd('sys/status')}catch(e){}if(!box.isConnected||!s||!('developer' in s)){box.innerHTML='';return}
  const adm=isAdmin(),b=devmBuild(s);
  box.innerHTML=`<h4 class='sh4'>Developer mode</h4><div class='sysucard ${s.developer?'warn':''}'><div><b>${s.developer?'On':'Off'}</b>${b?' · this system: '+pkgEsc(b):''}</div>
<div class='msg'>${s.developer?`Updates signed by a developer's key are installed too, from ${pkgEsc(s.url)}. Recovery mode still starts the official Rudolf OS${s.recoveryOfficial?'':' (once an official release has started on this computer)'}.`:'Only releases signed by Rudolf OS are installed. Developers can install their own builds here.'}</div></div>
<div class='rmrow' style='justify-content:flex-start'><button class='dvb'${adm?'':' disabled'}>${s.developer?'Turn off Developer mode…':'Turn on Developer mode…'}</button>${s.developer||s.signer=='custom'?`<button class='dvr'${adm?'':' disabled'}>Return to official Rudolf OS…</button>`:''}</div>${adm?'':"<div class='msg'>Only an administrator can change this.</div>"}`;
  box.querySelector('.dvb').onclick=()=>s.developer?devmOff(draw):devmOn(draw);
  const rb=box.querySelector('.dvr');if(rb)rb.onclick=()=>devmReturn(draw)};
 draw()}}
/* Settings › About: Official or Custom, under the version */
{const _ab=aboutPanel;aboutPanel=function(c){_ab.apply(this,arguments);if(!RD)return;
 rd('sys/status').then(s=>{const b=devmBuild(s);if(!b||!c.isConnected)return;const v=c.querySelector('.msg');if(!v)return;
  const d=document.createElement('div');d.className='devmab';d.textContent=(s.signer=='official'?'✅ ':'⚠️ ')+b;d.style.cssText='font-size:12px;margin-top:2px;'+(s.signer=='official'?'':'color:#b45309;font-weight:600');v.after(d)}).catch(()=>{})}}
