/* ===== 1.6.0 (docs/plan-1.6.0.md "Before stable" 15): Settings › Update and recovery › Start-up of this drive =====
   Rudolf OS never rewrites the drive's start-up files by itself: a start only says in System health when they are not
   all the official ones. Here, with an administrator's name and password:
   - Check this drive's start-up…: its partition table, its EFI system partition and the files in it, and the firmware's
     entry for the drive, which is put right when it names start-up files that are no longer where it says (the rescue
     for a computer that opened its firmware's setup screen instead of starting: start it from a Rudolf OS USB stick,
     then use this). Nothing else is written. (rudolfd sys/startup-check, bootfiles.sh rescue)
   - Put back the official start-up files…: this release's official ones, downloaded and checked, put into the drive's
     existing EFI system partition one file at a time (each beside the old one, checked, then renamed); the partition
     table and the area in front of your files are never written; the firmware's entry is checked after. Only on mains
     power or with the battery above 50 %. (rudolfd sys/startup-files: espcheck repair, bootfiles.sh files) */
(function(){if(typeof updPanel!='function')return;const U0=updPanel;updPanel=function(c){U0(c);if(typeof RD=='undefined'||!RD)return;const x=c.querySelector('.updx');if(!x)return;
 const box=document.createElement('div');box.className='sup60';x.appendChild(box);const adm=isAdmin();
 box.innerHTML=`<h4 class='sh4'>Start-up of this drive</h4><p class='msg'>The files the computer starts Rudolf OS with (in the drive's EFI system partition) and the firmware's entry for the drive. Rudolf OS does not change them by itself.</p>
<div class='rmrow' style='justify-content:flex-start'><button class='supck'${adm?'':' disabled'}>Check this drive's start-up…</button><button class='supfx'${adm?'':' disabled'}>Put back the official start-up files…</button></div>${adm?'':"<div class='msg'>Only an administrator can do this.</div>"}<pre class='jl suplog' hidden></pre>`;
 const log=box.querySelector('.suplog'),T='Start-up of this drive';
 box.querySelector('.supck').onclick=async()=>{const cr=await elevate('Check this drive\'s start-up and put the firmware\'s entry for it right',{cred:true});if(!cr)return;
  log.hidden=false;log.textContent='Checking…';
  try{const r=await rd('sys/startup-check',{method:'POST',body:JSON.stringify({n:cr.n,p:cr.p})});const t=(r.report||'').replace(/\nexit=\d+\s*$/,'');log.textContent=t;
   const good=/exit=0\s*$/.test(r.report||'');notify(T,good?'All is right: the computer can start from this drive.':'Not all is right: see Settings › Update and recovery.',{kind:good?'info':'warning'})}
  catch(e){log.textContent=e.message;notify(T,e.message,{kind:'error'})}};
 box.querySelector('.supfx').onclick=async()=>{
  if(!await sysuDlg('Put back the official start-up files',`<p>Rudolf OS downloads this release's official start-up files, checks their signatures, and puts them into this drive's EFI system partition one file at a time: each new file is written beside the old one and checked before it replaces it. The old ones are kept until the computer has started with the new ones. Your files, settings and apps are not touched, nor is the drive's partition table.</p><p><b>Keep the computer connected to power</b> (or charged above 50 %) and do not switch it off until it says it is finished. If anything does not check, the old files are put back.</p>`,[['Cancel'],['Put them back',1,1]]))return;
  const cr=await elevate('Put back the official start-up files on this drive',{cred:true});if(!cr)return;
  log.hidden=false;log.textContent='';let st=null;
  try{st=await sysuJob('startup-files',l=>{log.textContent+=l+'\n';log.scrollTop=log.scrollHeight},JSON.stringify({n:cr.n,p:cr.p}))}catch(e){log.textContent+=e.message+'\n'}
  notify(T,st===0?'The official start-up files are back, and the firmware\'s entry for the drive is right.':'Nothing was changed, or the old files were put back: see Settings › Update and recovery.',{kind:st===0?'info':'error'})};
}})();
