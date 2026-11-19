/* 1.6.0 (2026-11-19: a grandfather's old HP, its hard drive's documents, pictures and videos onto a USB stick, from a
   Rudolf OS stick): File Explorer › a drive › Copy to another drive… The files and folders chosen (or the folder open)
   copied straight onto another drive by rudolfd (fs/xcopy, drivecopy.sh): folders kept as they are, no limit on size
   or number, nothing on the source changed. "Only personal files" leaves out Windows, programs, AppData and system
   files. A drive that fills up stops the copy; the next one carries on with the rest ("Continue from the last drive"). */
const DC_LAST={key:'',full:false};
const dcEsc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const dcB=b=>b>=1e9?(b/1e9).toFixed(1)+' GB':b>=1e6?Math.round(b/1e6)+' MB':Math.round(b/1e3)+' KB';
/* the drives it can copy onto: partitions with a file system, not the one copied from, not a Rudolf OS stick's own parts */
async function dcTargets(srcDev){let r;try{r=await rd('disks')}catch(e){return[]}const out=[];
 (r.disks||[]).forEach(dk=>{if(/^dm-/.test(dk.name)||dk.partitions.some(x=>x.mount&&!/^\/media\//.test(x.mount)))return;
  dk.partitions.forEach(pt=>{if(!pt.fs||/^(swap|iso9660|crypto_LUKS|LVM2_member|linux_raid_member)$/.test(pt.fs)||pt.label=='RUDOLFDATA'||pt.label=='RUDOLF EFI'||/^RUDOLF_BOOT/.test(pt.label||'')||pt.name==srcDev)return;
   out.push({dev:pt.name,mount:pt.mount,fs:pt.fs,bytes:pt.bytes,removable:dk.removable,label:(pt.label||pt.name)+' ('+dcB(pt.bytes||0)+(dk.model?', '+dk.model:'')+')'})})});
 return out.sort((a,b)=>(b.removable?1:0)-(a.removable?1:0))}
async function driveCopyDlg(remote,items){
 if(!remote||remote.ro)return;
 const src=items&&items.length?items.map(i=>remote.path+'/'+i.name):[remote.path];
 const what=items&&items.length==1?items[0].name:items&&items.length?items.length+' items':(remote.path.split('/').pop()||remote.label);
 const key=src.join('\n'),day=new Date().toISOString().slice(0,10);
 const o=document.createElement('div');o.className='rmodal dcdlg';
 o.innerHTML=`<div class='rmbox' role='dialog' aria-modal='true' aria-labelledby='dct' style='max-width:600px'><h3 id='dct'>Copy to another drive</h3>
 <p class='msg'>Copies <b class='dcw'></b> from ${dcEsc(remote.label)} onto another drive, folders and all. Nothing on ${dcEsc(remote.label)} is changed.</p>
 <div class='r2'><label for='dcto'>Copy onto</label><select id='dcto' class='dcto'><option value=''>Looking for drives…</option></select></div>
 <div class='r2'><label for='dcn'>Into a folder named</label><input id='dcn' class='dcn' style='flex:1'></div>
 <label class='ck'><input type='checkbox' class='dcp' checked> Only personal files: leave out Windows, programs, AppData and system files (.exe, .dll, …)</label>
 <label class='ck dccl' hidden><input type='checkbox' class='dcc' checked> Continue from the last drive (it filled up): copy only what is not on it</label>
 <p class='msg dcm'></p><div class='dcpg' hidden><progress class='dcbar' max='1000' value='0' style='width:100%'></progress><div class='msg dcpt'></div></div>
 <pre class='jl dcl' hidden style='max-height:180px;overflow:auto;font-size:11px'></pre>
 <div class='rmrow dcr'><button class='dcx'>Cancel</button><button class='pri dcgo' disabled>Copy</button></div></div>`;
 document.body.appendChild(o);const q=s=>o.querySelector(s);q('.dcw').textContent=what;q('.dcn').value=`${what} (copied ${day})`.replace(/[\/\\:*?"<>|]/g,'_');
 let busy=false;const close=()=>{if(!busy)o.remove()};q('.dcx').onclick=close;o.addEventListener('keydown',e=>{if(e.key=='Escape')close()});
 if(DC_LAST.key==key&&DC_LAST.full)q('.dccl').hidden=false;
 const tg=await dcTargets(remote.dev),sel=q('.dcto');
 if(!tg.length){sel.innerHTML="<option value=''>No other drive: plug in a USB stick or drive</option>";q('.dcm').textContent='Plug in the drive to copy onto, then choose Copy to another drive again.';return}
 sel.innerHTML=tg.map((t,i)=>`<option value='${i}'>${t.removable?'🔌':'💽'} ${dcEsc(t.label)}</option>`).join('');q('.dcgo').disabled=false;
 const note=()=>{const t=tg[+sel.value];q('.dcm').textContent=t&&t.fs=='vfat'?'This drive is FAT32: files over 4 GB (long videos, say) cannot go on it and are listed at the end. An exFAT or NTFS drive takes them.':''};sel.onchange=note;note();
 q('.dcgo').onclick=async()=>{const t=tg[+sel.value],nm=q('.dcn').value.trim().replace(/[\/\\:*?"<>|]/g,'_').replace(/^\.+/,'');if(!t||!nm)return;
  busy=true;q('.dcgo').disabled=true;q('.dcx').disabled=true;sel.disabled=true;q('.dcn').disabled=true;q('.dcp').disabled=true;q('.dcc').disabled=true;
  try{if(!t.mount){await rdPost('mount?dev='+t.dev);t.mount='/media/'+t.dev}
   const cont=!q('.dccl').hidden&&q('.dcc').checked;
   await rd('fs/xcopy',{method:'POST',body:`personal=${q('.dcp').checked?1:0} continue=${cont?1:0}\n${t.mount}/${nm}\n${src.join('\n')}\n`})}
  catch(e){busy=false;q('.dcm').textContent=e.message;q('.dcgo').disabled=false;q('.dcx').disabled=false;sel.disabled=false;q('.dcn').disabled=false;q('.dcp').disabled=false;q('.dcc').disabled=false;return}
  q('.dcpg').hidden=false;q('.dcl').hidden=false;q('.dcm').textContent='Copying… You can keep using the computer; leave both drives plugged in.';
  for(;;){await new Promise(r=>setTimeout(r,1000));let j;try{j=await rd('job')}catch(e){continue}if(j.name&&j.name!='drive-copy')continue;
   const ls=(j.log||'').split('\n'),pg=ls.filter(l=>/^PROGRESS /.test(l)).pop();
   if(pg){const [,dn,tn,db,tb]=pg.split(' ').map(Number);q('.dcbar').value=tb?Math.round(db/tb*1000):dn/Math.max(1,tn)*1000;q('.dcpt').textContent=`${dn.toLocaleString()} of ${tn.toLocaleString()} files · ${dcB(db)} of ${dcB(tb)}`}
   const pre=q('.dcl');pre.textContent=ls.filter(l=>!/^PROGRESS /.test(l)).join('\n');pre.scrollTop=pre.scrollHeight;
   if(!j.running&&j.status>=0){busy=false;q('.dcx').disabled=false;q('.dcx').textContent='Close';
    if(j.status==0){DC_LAST.key='';q('.dcm').textContent='Done. Safely remove the drive before unplugging it.';try{sfx('notify')}catch(e){}}
    else if(j.status==28){DC_LAST.key=key;DC_LAST.full=true;q('.dcm').textContent='The drive is full. Safely remove it, plug in the next one, and choose Copy to another drive again with the same selection: it continues with the rest.';try{sfx('warning')}catch(e){}}
    else q('.dcm').textContent='The copy stopped: see the lines above.';
    if(t.removable){const b=document.createElement('button');b.textContent='⏏ Safely remove '+(t.label.split(' (')[0]);b.onclick=async()=>{try{await rdPost('umount?dev='+t.dev);b.disabled=true;b.textContent='It can be unplugged now'}catch(e){q('.dcm').textContent=e.message}};q('.dcr').prepend(b)}
    break}}}}
