/* ===== 1.2.4: keep files on the USB stick Rudolf OS runs from =====
   A stick written from the disc image (Rufus, Etcher) runs live: nothing is kept. rudolfd (GET keepfiles,
   POST keepfiles: keepfiles.sh) adds the RUDOLFDATA partition in the free space after the image while Rudolf OS
   runs; the live session's files move onto it at the next restart. Offered once after signing in on such a
   stick, and always in Settings > Storage and backup. */
window.__keep29=1;
async function kfStatus(){if(!RD)return null;try{return await rd('keepfiles')}catch(e){return null}}
function kfRestart(){try{fsFlush()}catch(e){}rdPost('power?action=reboot').catch(()=>{})}
async function kfMake(W,done){
 if(!await elevate('Keep files on this drive: Rudolf OS adds a RUDOLFDATA partition in its free space.'))return;
 const o=document.createElement('div');o.className='rmodal';
 o.innerHTML=`<div class='rmbox' role='alertdialog' aria-label='Keep files on this drive' style='max-width:540px'><h3>💾 Keeping your files on this drive</h3><pre class='kfl' style='max-height:180px;overflow:auto;white-space:pre-wrap;background:rgba(0,0,0,.25);padding:8px;font-size:12px'>Starting…</pre><div class='rmrow'><button class='c' disabled>Close</button><button class='pri rs' disabled>Restart now</button></div></div>`;
 document.body.appendChild(o);const L=o.querySelector('.kfl');
 o.querySelector('.c').onclick=()=>o.remove();o.querySelector('.rs').onclick=()=>{o.remove();kfRestart()};
 try{await rdPost('keepfiles')}catch(e){L.textContent=e.message||'Could not start.';o.querySelector('.c').disabled=false;return}
 for(;;){await new Promise(r=>setTimeout(r,700));let j;try{j=await rd('job')}catch(e){continue}
  L.textContent=(j.log||'').trim()||'Working…';L.scrollTop=L.scrollHeight;
  if(!j.running){o.querySelector('.c').disabled=false;if(j.status==0){o.querySelector('.rs').disabled=false;o.querySelector('.rs').focus();
    notify('Your files will be kept','Restart Rudolf OS to finish: everything you made in this session moves onto the drive.',{action:kfRestart})}
   done&&done();return}}}
function kfOffer(s){
 const o=document.createElement('div');o.className='rmodal';
 o.innerHTML=`<div class='rmbox' role='alertdialog' aria-label='Keep your files' style='max-width:520px'><h3>💾 Keep your files on this USB stick?</h3>
<p>Rudolf OS is running <b>live</b>: your files, settings and apps are lost when you shut down.</p>
<p>This drive has <b></b> of free space after Rudolf OS. Use it to keep everything, and to install apps such as Roblox (Sober)? Rudolf OS itself is not changed; nothing else is on that free space.</p>
<p class='msg'>You can also do this later in Settings › Storage and backup.</p>
<div class='rmrow'><button class='c'>Not now</button><button class='pri ok'>Keep my files…</button></div></div>`;
o.querySelectorAll('p b')[1].textContent=fmtB(s.freeBytes);
 o.querySelector('.c').onclick=()=>{o.remove();try{localStorage.setItem('rudolf.kfAsked','1')}catch(e){}};
 o.querySelector('.ok').onclick=()=>{o.remove();kfMake(null)};
 document.body.appendChild(o);o.querySelector('.ok').focus()}
/* once per start, after signing in, when the drive can keep files and nobody said "Not now" */
(function kfTick(){if(!RD)return;if(typeof USER=='undefined'||!USER||window.LOCKED){setTimeout(kfTick,4000);return}
 let asked=false;try{asked=!!localStorage.getItem('rudolf.kfAsked')}catch(e){}
 if(asked||!isAdmin())return;
 setTimeout(async()=>{const s=await kfStatus();if(s&&s.possible)kfOffer(s)},6000)})();
async function kfSection(c){
 if(!RD)return;const box=document.createElement('div');box.className='kf29';c.appendChild(box);
 const draw=async()=>{const s=await kfStatus();if(!box.isConnected||!s)return;
  if(!s.possible&&!s.pending&&/already keeps/.test(s.reason||'')){box.innerHTML='';return}
  box.innerHTML="<h4 class='sh4'>Keep files on this drive</h4><p class='msg kft'></p><div class='rmrow' style='justify-content:flex-start'></div>";
  const t=box.querySelector('.kft'),row=box.querySelector('.rmrow');
  if(s.pending){t.textContent='Ready: restart Rudolf OS to finish. Everything you made in this session moves onto the drive.';const b=document.createElement('button');b.className='pri';b.textContent='Restart now';b.onclick=kfRestart;row.appendChild(b);return}
  if(s.possible){t.textContent=`Rudolf OS is running live. This drive has ${fmtB(s.freeBytes)} of free space after Rudolf OS: use it to keep your files, settings and apps. Rudolf OS itself is not changed.`;
   const b=document.createElement('button');b.className='pri';b.textContent='Keep my files on this drive…';b.disabled=!isAdmin();b.onclick=()=>kfMake(null,draw);row.appendChild(b);return}
  t.textContent='Your files cannot be kept on this drive: '+(s.reason||'unknown reason')+'.'};
 draw()}
if(typeof storPanel=='function'){const _kp=storPanel;storPanel=function(c){const r=_kp.apply(this,arguments);try{kfSection(c)}catch(e){}return r}}
