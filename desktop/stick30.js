/* ===================== USB sticks: make one, back up to one, restore from one (1.2.5) =====================
   Settings › Storage and backup:
   - Make a Rudolf OS USB stick: rudolfd's mkstick (mkstick.sh) writes this Rudolf OS (or a downloaded image) to
     a USB stick or memory card, reads it back to check it, adds RUDOLFDATA, and copies the installed updates and,
     if asked, this session's files and settings. An administrator confirms by typing the stick's size.
   - Back up to a USB stick: the same backup as "Back up everything…", written into RUDOLF-BACKUP on a plugged-in
     drive with a .sha256 file beside it.
   First start: "Restore from a backup" finds those backups on plugged-in drives and refuses a damaged one. */
const STK={BDIR:'RUDOLF-BACKUP'};
const stkGB=b=>(b/1e9).toFixed(1)+' GB';
/* the whole drive Rudolf OS started from ('/dev/sdb1' -> 'sdb') */
const stkDisk=dev=>{const m=String(dev||'').replace(/^\/dev\//,'');return /^(nvme\d+n\d+|mmcblk\d+)p\d+$/.test(m)?m.replace(/p\d+$/,''):/^(sd|vd)[a-z]+\d+$/.test(m)?m.replace(/\d+$/,''):m};
/* drives a stick can be made on: USB sticks and memory cards of 2 GB or more, not the one Rudolf OS runs from */
function stkTargets(disks,info){const own=stkDisk(info&&info.mediaDev);return (disks||[]).filter(k=>k.removable&&k.bytes>=2e9&&k.name!=own)}
/* partitions a backup can be written to (or read from): on removable drives, a file system other than Rudolf OS's own
   (not a Rudolf OS stick's image, boot partition or RUDOLFDATA) */
function stkBackupParts(disks,info){const own=stkDisk(info&&info.mediaDev),out=[];(disks||[]).filter(k=>k.removable&&k.name!=own).forEach(k=>k.partitions.forEach(p=>{if(p.fs&&!['iso9660','swap'].includes(p.fs)&&!['RUDOLFDATA','RUDOLF EFI'].includes(p.label))out.push({disk:k,part:p})}));return out}
async function stkJob(pre,onLine){let seen=0;for(;;){await new Promise(r=>setTimeout(r,1000));let j;try{j=await rd('job')}catch(e){continue}const ls=(j.log||'').split('\n');pre.textContent=j.log||'';pre.scrollTop=pre.scrollHeight;if(onLine)for(;seen<ls.length-1;seen++)onLine(ls[seen]);if(!j.running&&j.status>=0)return j}}
/* the confirmation: the administrator types the size shown, so the wrong drive is not erased by a slip */
function stkConfirm(k){return new Promise(res=>{const o=document.createElement('div');o.className='natdlg';const sz=stkGB(k.bytes);
 o.innerHTML=`<div class='natbox' role='dialog' aria-modal='true' aria-labelledby='stkt'><h3 id='stkt'>Erase this drive?</h3><p class='stkd'></p><p class='stke'></p><p>To go on, type its size: <b>${sz}</b></p><input class='stki' autocomplete='off' style='width:120px'><div class='stbtn' style='justify-content:flex-end'><button class='c'>Cancel</button><button class='pri g' disabled>Erase and make the stick</button></div></div>`;
 o.querySelector('.stkd').textContent=`${k.name} — ${k.model||'USB drive'} — ${sz}`;const pl=(k.partitions||[]).filter(p=>p.fs).map(p=>p.label?`“${p.label}” (${p.fs})`:p.fs);o.querySelector('.stke').textContent=pl.length?`Everything on it is erased: ${pl.join(', ')}, with its files and name. If it is open in File Explorer, Rudolf OS closes it first.`:'Everything on it is erased.';const i=o.querySelector('.stki'),g=o.querySelector('.g');
 i.oninput=()=>g.disabled=i.value.trim().replace(/\s+/g,' ').toUpperCase()!=sz.toUpperCase();const done=v=>{o.remove();res(v)};o.querySelector('.c').onclick=()=>done(false);g.onclick=()=>done(true);
 o.onkeydown=e=>{e.stopPropagation();if(e.key=='Escape')done(false);if(e.key=='Enter'&&!g.disabled)done(true)};document.body.appendChild(o);i.focus()})}
async function stkSection(c){if(!RD)return;const box=document.createElement('div');box.className='stk30';c.appendChild(box);
 let info={},disks=[],imgs=[];try{info=await rd('info');disks=(await rd('disks')).disks||[]}catch(e){}try{imgs=(await rd('install/images')).images||[]}catch(e){}
 const adm=isAdmin(),tg=stkTargets(disks,info),bp=stkBackupParts(disks,info);
 box.innerHTML=`<h4 class='sh4'>Make a Rudolf OS USB stick</h4><p class='msg'>Puts Rudolf OS on a USB stick or memory card (2 GB or more) that starts on any computer and keeps its files. Everything on that drive is erased.</p>
 <div class='r2'><label>Rudolf OS to put on it</label><select class='stks'><option value=''>This Rudolf OS ${lxEsc(RVER)}${info.updateInstalled?' (with its updates)':''}</option>${imgs.map(x=>`<option value='${lxEsc(x.path)}'>${lxEsc(x.name)} (Downloads)</option>`).join('')}</select></div>
 <label class='ck'><input type='checkbox' class='stkf'> Also copy my files and settings (Linux apps are not copied)</label><div class='stkl'></div><pre class='jl stkj' hidden></pre>
 <h4 class='sh4'>Back up to a USB stick</h4><p class='msg'>The same backup as “Back up everything…”, written straight to a plugged-in drive (in its ${STK.BDIR} folder, with a checksum). A new Rudolf OS can restore it when it first starts.</p><div class='stkb'></div><div class='msg stkbm'></div>`;
 const q=s=>box.querySelector(s),L=q('.stkl'),pre=q('.stkj');
 if(!tg.length)L.innerHTML="<p class='msg'>Plug in a USB stick or memory card of 2 GB or more (not the one Rudolf OS runs from), then open this page again.</p>";
 tg.forEach(k=>{const r=document.createElement('div');r.className='acr';r.innerHTML=`<span style='flex:1'></span><button>Make the stick…</button>`;r.firstChild.textContent=`🔌 ${k.name} — ${k.model||'USB drive'} — ${stkGB(k.bytes)}${k.partitions.some(p=>p.fs=='iso9660')?' (an older Rudolf OS stick)':''}`;
  const b=r.lastChild;b.disabled=!adm;b.onclick=async()=>{if(!await elevate(`Make a Rudolf OS USB stick on ${k.name} (${k.model||''}, ${stkGB(k.bytes)}). Everything on it is erased.`))return;if(!await stkConfirm(k))return;
   L.querySelectorAll('button').forEach(x=>x.disabled=true);pre.hidden=false;pre.textContent='';
   try{await rdPost(`mkstick?disk=${k.name}&confirm=${k.name}&image=${encodeURIComponent(q('.stks').value)}&files=${q('.stkf').checked?1:0}`)}catch(e){pre.textContent=e.message;L.querySelectorAll('button').forEach(x=>x.disabled=!adm);return}
   const j=await stkJob(pre);const ok=j.status==0;
   notify(ok?'The Rudolf OS USB stick is ready':'The stick could not be made',ok?`${k.name} can be removed. Start a computer from it with its boot menu (often F12).`:((j.log||'').trim().split('\n').pop()||'See Settings › Storage and backup.'),{kind:ok?'info':'error'});
   if(ok)r.firstChild.textContent+=' — ready';L.querySelectorAll('button').forEach(x=>x.disabled=!adm)};L.appendChild(r)});
 if(!adm&&tg.length)L.insertAdjacentHTML('beforeend',"<p class='msg'>An administrator can make a stick.</p>");
 const B=q('.stkb');if(!bp.length)B.innerHTML="<p class='msg'>Plug in a USB stick or drive to back up to.</p>";
 bp.forEach(({disk,part})=>{const r=document.createElement('div');r.className='acr';r.innerHTML=`<span style='flex:1'></span><button>Back up here</button>`;
  r.firstChild.textContent=`🔌 ${part.label||part.name} — ${disk.model||'USB drive'} — ${stkGB(part.bytes)} (${part.fs})`;r.lastChild.onclick=async e=>{e.target.disabled=true;const m=q('.stkbm');m.textContent='Making the backup…';
   try{const n=await stkBackupTo(part,t=>m.textContent=t);m.textContent=`Backed up to ${part.label||part.name}: ${STK.BDIR}/${n}. You can remove the drive.`;notify('Backup saved',`${part.label||part.name}: ${STK.BDIR}/${n}`)}catch(x){m.textContent='The backup could not be saved: '+x.message}e.target.disabled=false};B.appendChild(r)})}
/* mount (unless already mounted), write the backup and its checksum, unmount again if we mounted it */
async function stkMount(part){if(part.mount)return{path:part.mount,mine:false};await rdPost('mount?dev='+part.name);return{path:'/media/'+part.name,mine:true}}
async function stkUnmount(part,m){if(m&&m.mine)try{await rdPost('umount?dev='+part.name)}catch(e){}}
async function stkSha(blob){const h=await crypto.subtle.digest('SHA-256',await blob.arrayBuffer());return [...new Uint8Array(h)].map(b=>b.toString(16).padStart(2,'0')).join('')}
async function stkBackupTo(part,say){const blob=await backupBlob();if(blob.size>500e6)throw new Error('the backup is larger than 500 MB; use “Back up everything…” and copy the file');
 const d=new Date(),p=n=>String(n).padStart(2,'0'),name=`rudolf-backup-${d.getFullYear()}${p(d.getMonth()+1)}${p(d.getDate())}-${p(d.getHours())}${p(d.getMinutes())}.zip`;
 say&&say('Checking it…');const sum=await stkSha(blob);const m=await stkMount(part);const dir=m.path+'/'+STK.BDIR;
 try{say&&say(`Writing ${fmtB(blob.size)}…`);await rdPost('fs/mkdir?path='+encodeURIComponent(dir)).catch(()=>{});
  await rd('fs/write?path='+encodeURIComponent(dir+'/'+name),{method:'POST',body:blob});await rd('fs/write?path='+encodeURIComponent(dir+'/'+name+'.sha256'),{method:'POST',body:`${sum}  ${name}\n`});
  /* read it back from the drive and compare */
  const back=await (await fetch('/api/fs/read?path='+encodeURIComponent(dir+'/'+name),{headers:{'X-Rudolf-Token':RD.token}})).blob();if(await stkSha(back)!=sum)throw new Error('the copy on the drive does not match: the drive may be failing')}
 finally{await stkUnmount(part,m)}return name}
/* backups on plugged-in drives: [{part, disk, name, path, sum}] (mounting what is needed; unmounted again after) */
async function stkFindBackups(){if(!RD)return[];let info={},disks=[];try{info=await rd('info');disks=(await rd('disks')).disks||[]}catch(e){return[]}const out=[];
 for(const {disk,part} of stkBackupParts(disks,info)){let m;try{m=await stkMount(part)}catch(e){continue}
  try{const l=await rd('fs/list?path='+encodeURIComponent(m.path+'/'+STK.BDIR));(l.items||[]).filter(x=>!x.dir&&/^rudolf-backup-.*\.zip$/i.test(x.name)).forEach(x=>out.push({disk,part,name:x.name,size:x.size,mtime:x.mtime,path:m.path+'/'+STK.BDIR+'/'+x.name,m}))}catch(e){}
  if(!out.some(o=>o.part===part))await stkUnmount(part,m)}
 return out.sort((a,b)=>b.mtime-a.mtime)}
async function stkReadBackup(b){const h={headers:{'X-Rudolf-Token':RD.token}};const blob=await (await fetch('/api/fs/read?path='+encodeURIComponent(b.path),h)).blob();
 let want='';try{const r=await fetch('/api/fs/read?path='+encodeURIComponent(b.path+'.sha256'),h);if(r.ok)want=(await r.text()).trim().split(/\s+/)[0]}catch(e){}
 if(!want)throw new Error('its checksum file is missing, so it cannot be checked');if(await stkSha(blob)!=want.toLowerCase())throw new Error('it is damaged (its checksum does not match)');return blob}
/* the first-start screen: Restore from a backup */
function stkRestoreDlg(){return new Promise(async res=>{const o=document.createElement('div');o.className='natdlg';o.style.zIndex=2000010;
 o.innerHTML=`<div class='natbox' role='dialog' aria-modal='true' aria-labelledby='strt'><h3 id='strt'>Restore from a backup</h3><div class='strl'><p class='msg'>Looking on plugged-in drives…</p></div><div class='msg strm'></div><div class='stbtn' style='justify-content:flex-end'><button class='c'>Cancel</button></div></div>`;
 document.body.appendChild(o);const L=o.querySelector('.strl'),M=o.querySelector('.strm');let found=[];const close=v=>{found.forEach(b=>stkUnmount(b.part,b.m));o.remove();res(v)};o.querySelector('.c').onclick=()=>close(false);
 const draw=async()=>{L.innerHTML="<p class='msg'>Looking on plugged-in drives…</p>";found=await stkFindBackups();
  if(!found.length){L.innerHTML=`<p class='msg'>No backup was found. Plug in the drive with the ${STK.BDIR} folder, then look again.</p><button class='again'>Look again</button>`;L.querySelector('.again').onclick=draw;return}
  L.innerHTML='';found.forEach(b=>{const r=document.createElement('div');r.className='acr';r.innerHTML=`<span style='flex:1'></span><button class='pri'>Restore</button>`;r.firstChild.textContent=`${b.name} — ${fmtB(b.size)} — on ${b.part.label||b.part.name}`;
   r.lastChild.onclick=async()=>{L.querySelectorAll('button').forEach(x=>x.disabled=true);M.textContent='Checking the backup…';
    try{const blob=await stkReadBackup(b);M.textContent='Restoring…';await restoreAll(blob,true);close(true)}catch(e){M.textContent='This backup cannot be restored: '+e.message;L.querySelectorAll('button').forEach(x=>x.disabled=false)}};L.appendChild(r)})};
 draw()})}
if(typeof storPanel=='function'){const _sp=storPanel;storPanel=function(c){const r=_sp.apply(this,arguments);try{stkSection(c)}catch(e){}return r}}
{const _fr=firstRun;firstRun=function(){const r=_fr.apply(this,arguments);const o=document.getElementById('oobe');if(o&&RD&&!o.querySelector('.oorest')){const b=document.createElement('button');b.className='oorest';b.textContent='Restore from a backup on a USB stick…';
 b.style.cssText='position:absolute;left:50%;bottom:18px;transform:translateX(-50%)';b.onclick=()=>stkRestoreDlg();o.appendChild(b)}return r}}
