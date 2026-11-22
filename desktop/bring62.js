/* ===== Rudolf OS 1.6.2 (docs/plan-1.6.2.md Part 27; the owner, 2026-11-21: "Would it be possible for someone downloading and
   installing Rudolf to be able to save/transfer their personal files (documents, pictures, videos, music, etc.) from their old
   operating system?", then "Go ahead"): Bring your files.
   - bringStep(disk): Settings › Update and recovery › Install here…, after the administrator's prompt and before the "Erase?"
     question. The disk is looked at read-only (rudolfd bring/scan: the drives helper, bringfiles.sh): Windows (Users\NAME,
     OneDrive's folders too), macOS (said: Rudolf OS cannot read APFS) and Linux (/home/NAME and the XDG folders), per person,
     each folder with its size. Keep them (the default when any are found: copied onto a USB drive, every file checked with
     SHA-256 read back from the drive, before anything is erased; a drive that fills up and the next one taking the rest), choose
     which, or erase without keeping (said with the size lost). A BitLocker disk asks for its recovery key (opened read-only,
     the key never kept); a Windows not shut down fully is read read-only and said. A copy that fails or is cancelled erases
     nothing, and install.sh itself refuses the disk until the copy (or the choice to erase) was made (bringfiles.sh guard).
   - After installing: at sign-in, or when a drive with a copy is plugged in, a notification offers to bring the files in;
     Settings › Update and recovery › Bring in files from the old computer does it: the old Documents into Documents, Pictures
     into Pictures and so on (the rest into Documents\From the old computer), into the account's own encrypted folder, as the
     account's own Linux user; names that clash kept side by side; each file checked against the copy's SHA-256 list; then
     the antivirus checks what came in, as it checks files extracted from a ZIP. The copy on the drive is left as it is.
   The same on Standard, Native (Settings is its own window there, the same page) and Lite: it is the page's own code. ===== */
const BR={offered:new Set(),run:false,lastSel:new Set()};
const brB=b=>typeof fmtB=='function'?fmtB(b):b+' B';
const brWait=ms=>new Promise(r=>setTimeout(r,ms));
const BR_KEYS={Desktop:'Desktop',Documents:'Documents',Pictures:'Pictures',Music:'Music',Videos:'Videos',Downloads:'Downloads',Other:'Other files and folders'};
const BR_TOP='Rudolf OS - Bring your files';
const brEl=(tag,cls,text)=>{const e=document.createElement(tag);if(cls)e.className=cls;if(text!=null)e.textContent=text;return e};
/* a job of rudolfd's, until it ends: its log as it grows */
async function brJob(name,onLog){for(;;){await brWait(700);let j;try{j=await rd('job')}catch(e){continue}if(j.name&&j.name!=name)continue;try{onLog&&onLog(j.log||'',j)}catch(e){}if(!j.running&&j.status>=0)return j}}
const brProg=log=>{const pg=String(log).split('\n').filter(l=>/^PROGRESS /.test(l)).pop();return pg?pg.split(' ').slice(1).map(Number):null};
const brText=log=>String(log).split('\n').filter(l=>!/^(PROGRESS|RESULT) /.test(l)).join('\n');
const BR_LOCKED={
 bitlocker:'Windows on this partition is locked with BitLocker (many Windows 11 laptops are, often without their owner knowing). Type its recovery key to read it: 48 digits, usually kept in the Microsoft account of whoever set the computer up (account.microsoft.com/devices/recoverykey, on a phone or another computer), on a printout, in a file saved on a USB drive, or with the company or school that gave the computer. It is used only to open the disk read-only, and never kept.',
 apfs:'This disk has macOS on it (APFS). Rudolf OS cannot read it. Copy your files on the Mac first: start the Mac, plug in a USB drive, open Finder and drag your Documents, Pictures, Music, Movies and Desktop folders onto the drive. Then install Rudolf OS.',
 hfs:'This disk has an older macOS on it (Mac OS Extended). Rudolf OS cannot read it. Copy your files on the Mac first (Finder: drag Documents, Pictures, Music, Movies and Desktop onto a USB drive), then install.',
 luks:'This partition is an encrypted Linux one (LUKS). Rudolf OS cannot open it here. Copy your files from that Linux first, then install.',
 'ntfs-unreadable':'Windows on this partition could not be read. Windows may not have been shut down fully, or the disk needs checking: start Windows, hold Shift while choosing Shut down (or run chkdsk), then look again.'};
const BR_STATE={dirty:'Windows was not shut down cleanly. Its files are read as they are on the disk (read-only); changes made just before may be missing. To be sure, start Windows, hold Shift while choosing Shut down, then look again.',
 hibernated:'Windows is hibernated, or uses Fast Startup. Its files are read as they are on the disk (read-only); files saved just before it went to sleep may be missing. To be sure, start Windows, hold Shift while choosing Shut down (or turn Fast Startup off in Power Options), then look again.'};
/* the drives a copy can go onto: not the disk being installed on (drivecopy60.js's list) */
async function brTargets(disk){const t=typeof dcTargets=='function'?await dcTargets(''):[];return t.filter(x=>!String(x.dev).startsWith(disk))}
/* ---------- the installer's step: true when the installation may go on to "Erase?" ---------- */
async function bringStep(k){if(!RD||!k||!k.name)return true;const disk=k.name;
 return new Promise(res=>{
  const o=brEl('div','rmodal brdlg');
  o.innerHTML=`<div class='rmbox' role='dialog' aria-modal='true' aria-labelledby='brt' style='max-width:660px;max-height:92vh;overflow:auto'><h3 id='brt'>Bring your files</h3>
  <p class='msg brm'></p><div class='brs'></div><div class='brch' hidden></div>
  <div class='brpg' hidden><progress class='brbar' max='1000' value='0' style='width:100%'></progress><div class='msg brpt'></div></div>
  <pre class='jl brl' hidden style='max-height:150px;overflow:auto;font-size:11px;white-space:pre-wrap;word-break:break-word'></pre>
  <div class='rmrow brr'><button class='brx'>Cancel</button><button class='pri brgo' disabled>Next</button></div></div>`;
  document.body.appendChild(o);const q=s=>o.querySelector(s),msg=t=>{q('.brm').textContent=t};
  let busy=false,R=null,mode='keep',name='',cont=false,ended=false;
  const end=v=>{if(ended)return;ended=true;o.remove();if(!v)rdPost('bring/close?disk='+encodeURIComponent(disk)).catch(()=>{});res(v)};
  q('.brx').onclick=()=>{if(busy){rdPost('bring/cancel').catch(()=>{});q('.brx').disabled=true;msg('Stopping… nothing is erased.');return}end(false)};
  o.addEventListener('keydown',e=>{if(e.key=='Escape'&&!busy)end(false)});
  const go=q('.brgo');
  const sel=()=>[...o.querySelectorAll('.brf:checked')].map(c=>({id:c.dataset.id,k:c.dataset.k,b:+c.dataset.b}));
  /* 1. look */
  async function look(){busy=true;go.disabled=true;q('.brs').textContent='';q('.brch').hidden=true;msg(`Looking at what is on ${disk} (read-only: nothing on it is changed)…`);
   try{await rdPost('bring/scan?disk='+encodeURIComponent(disk));const j=await brJob('bring-scan');if(j.status!=0)throw new Error(brText(j.log).trim().split('\n').pop()||'it could not be looked at');R=await rd('bring/result?disk='+encodeURIComponent(disk))}
   catch(e){busy=false;msg('The disk could not be looked at: '+e.message+'. Nothing was changed.');go.textContent='Look again';go.disabled=false;go.onclick=look;return}
   busy=false;
   if(!R.found){end(true);return}
   draw()}
  /* 2. what was found, and the choice */
  function draw(keep){const box=q('.brs');box.textContent='';let people=0;
   if(!keep)msg(`${disk} has another system on it. Its personal files can be kept: copied onto a USB drive and checked before anything is erased, then brought into your account after Rudolf OS starts.`);
   (R.systems||[]).forEach(s=>{const h=brEl('h4','sh4',`${s.name} (${s.part})`);box.appendChild(h);
    if(BR_STATE[s.state]){const w=brEl('p','msg brw',BR_STATE[s.state]);w.style.color='#b45309';box.appendChild(w)}
    (s.people||[]).forEach(p=>{if(!(p.folders||[]).length)return;people++;const fs=brEl('fieldset','brp');fs.style.cssText='border:1px solid var(--bd,#cbd5e1);border-radius:8px;margin:6px 0;padding:6px 10px';
     fs.appendChild(brEl('legend','',`${p.name} — ${brB(p.bytes)}`));
     p.folders.forEach(f=>{const l=brEl('label','ck');const c=document.createElement('input');c.type='checkbox';c.className='brf';c.checked=!cont||BR.lastSel.has(p.id+'|'+f.k);c.disabled=cont;c.dataset.id=p.id;c.dataset.k=f.k;c.dataset.b=f.bytes;c.onchange=sum;l.appendChild(c);l.append(` ${BR_KEYS[f.k]||f.k} — ${brB(f.bytes)} (${Number(f.files).toLocaleString()} file${f.files==1?'':'s'})`);fs.appendChild(l)});
     box.appendChild(fs)})});
   (R.locked||[]).forEach(L=>{const d=brEl('div','brlk');d.style.margin='8px 0';d.appendChild(brEl('h4','sh4',(L.kind=='bitlocker'?'Locked with BitLocker':L.kind=='apfs'||L.kind=='hfs'?'macOS':L.kind=='luks'?'Encrypted Linux':'Windows')+` (${L.part})`));
    d.appendChild(brEl('p','msg',BR_LOCKED[L.kind]||'Rudolf OS cannot read this partition.'));
    if(L.kind=='bitlocker'){const row=brEl('div','rmrow');row.style.justifyContent='flex-start';const i=document.createElement('input');i.className='brkey';i.setAttribute('aria-label','BitLocker recovery key');i.placeholder='123456-123456-123456-123456-123456-123456-123456-123456';i.autocomplete='off';i.spellcheck=false;i.style.flex='1';
     const b=brEl('button','bru','Open it (read-only)');const m=brEl('span','msg brum');row.append(i,b);d.append(row,m);
     b.onclick=async()=>{const key=i.value.trim();if(!key)return;b.disabled=true;m.textContent='Opening…';
      try{await rd('bring/unlock?part='+encodeURIComponent(L.part),{method:'POST',body:key});i.value='';m.textContent='Opened.';await look()}
      catch(e){m.textContent=/403|does not open/i.test(e.message)?'That key does not open it. Check the digits (the key ID shown by Windows helps find the right one).':e.message;b.disabled=false}}}
    box.appendChild(d)});
   const lost=R.total||0,ch=q('.brch');ch.hidden=false;ch.textContent='';
   const opt=(v,t)=>{const l=brEl('label','ck');const r=document.createElement('input');r.type='radio';r.name='brmode';r.value=v;r.className='brmd';r.checked=mode==v;r.onchange=()=>{mode=v;sum()};l.appendChild(r);l.append(' '+t);ch.appendChild(l);return r};
   if(people)opt('keep','Keep them: copy the folders ticked above onto a USB drive first (they are checked before anything is erased)');else mode='erase';
   opt('erase',people?`Erase them without keeping: ${brB(lost)} of personal files on ${disk} will be lost for good`:`Erase ${disk} without keeping anything from it: what is on it will be lost for good`);
   const tg=brEl('div','r2 brtg');tg.innerHTML=`<label for='brto'>Copy onto</label><select id='brto' class='brto'><option value=''>Looking for drives…</option></select>`;ch.appendChild(tg);
   const nt=brEl('p','msg brnt');ch.appendChild(nt);
   name=name||`Old computer ${new Date().toISOString().slice(0,16).replace('T',' ').replace(':','.')}`;
   brTargets(disk).then(t=>{const s=q('.brto');if(!s)return;s.textContent='';BR.tg=t;if(!t.length){const op=brEl('option','','No USB drive: plug one in, then choose Look for drives');op.value='';s.appendChild(op)}
    t.forEach((x,i)=>{const op=brEl('option','',(x.removable?'🔌 ':'💽 ')+x.label);op.value=i;s.appendChild(op)});sum()});
   const rl=brEl('button','brrl','Look for drives');rl.onclick=()=>draw();tg.appendChild(rl);
   const la=brEl('button','brla','Look again');la.onclick=look;ch.appendChild(la);
   sum()}
  function sum(){const s=sel(),tot=s.reduce((a,x)=>a+x.b,0),tg=q('.brtg'),nt=q('.brnt');if(!tg)return;tg.hidden=mode!='keep';
   const t=(BR.tg||[])[+((q('.brto')||{}).value)];
   if(mode=='keep'){nt.textContent=s.length?`${s.length} folder(s), ${brB(tot)}, are copied${t?' onto '+t.label:''} into “${BR_TOP}\\${name}” and checked.${t&&t.fs=='vfat'?' This drive is FAT32: a file over 4 GB cannot go on it (an exFAT or NTFS drive takes them).':''}`:'Tick the folders to keep.';
    go.textContent=cont?'Continue onto this drive':'Copy and check';go.disabled=!s.length||!t}
   else{nt.textContent='';go.textContent='Erase without keeping';go.disabled=false}
   go.onclick=mode=='keep'?copy:erase}
  async function erase(){go.disabled=true;try{await rdPost(`bring/decide?disk=${encodeURIComponent(disk)}&what=erase`);end(true)}catch(e){msg(e.message);go.disabled=false}}
  /* 3. the copy */
  async function copy(){const s=sel(),t=(BR.tg||[])[+q('.brto').value];if(!s.length||!t)return;
   BR.lastSel=new Set(s.map(x=>x.id+'|'+x.k));busy=true;go.disabled=true;o.querySelectorAll('.brs input,.brs button,.brch input,.brch select,.brch button').forEach(x=>x.disabled=true);
   const pg=q('.brpg'),pre=q('.brl');pg.hidden=false;pre.hidden=false;q('.brx').textContent='Stop';
   const tbw=typeof tbpOwner=='function'?tbpOwner():null,tbp=v=>{if(tbw&&typeof winProgress=='function')winProgress(tbw,v,'bring')};tbp('busy');
   let j;
   try{if(!t.mount){await rdPost('mount?dev='+encodeURIComponent(t.dev));t.mount='/media/'+t.dev}
    msg('Copying… leave the drive plugged in. Nothing is erased until every file is copied and checked.');
    await rd('bring/copy',{method:'POST',body:`${disk} continue=${cont?1:0}\n${t.mount}/${BR_TOP}/${name}\n`+s.map(x=>x.id+'\t'+x.k).join('\n')+'\n'});
    j=await brJob('bring-copy',log=>{const p=brProg(log);if(p){const [dn,tn,db,tb]=p;q('.brbar').value=tb?Math.round(db/tb*1000):dn/Math.max(1,tn)*1000;tbp(tb?db/tb*100:dn/Math.max(1,tn)*100);q('.brpt').textContent=`${dn.toLocaleString()} of ${tn.toLocaleString()} files · ${brB(db)} of ${brB(tb)}`}pre.textContent=brText(log);pre.scrollTop=pre.scrollHeight})}
   catch(e){j={status:-2,log:e.message}}
   busy=false;tbp(null);q('.brx').disabled=false;q('.brx').textContent='Cancel';
   const st=j.status,eject=()=>{if(!t.removable)return;const b=brEl('button','brej','⏏ Safely remove '+t.label.split(' (')[0]);b.onclick=async()=>{try{await rdPost('umount?dev='+encodeURIComponent(t.dev));b.disabled=true;b.textContent='It can be unplugged now'}catch(e){msg(e.message)}};q('.brr').prepend(b)};
   if(st==0){cont=false;msg('Copied and checked: every file was read back from the drive and matches. Keep the drive until your files are in Rudolf OS (it offers to bring them in after you sign in).');try{sfx('notify')}catch(e){}
    go.textContent='Continue to install';go.disabled=false;go.onclick=()=>end(true);return}
   if(st==3){msg('Copied and checked, except files that could not be read (listed above). They are not kept.');go.textContent='Erase anyway (they are not kept)';go.disabled=false;
    go.onclick=async()=>{go.disabled=true;try{await rdPost(`bring/decide?disk=${encodeURIComponent(disk)}&what=partial`);end(true)}catch(e){msg(e.message);go.disabled=false}};return}
   if(st==28){cont=true;eject();msg('The drive is full: what is on it is checked. Safely remove it, plug in another drive, choose it below and continue: it takes the rest. Nothing is erased until everything is copied.');try{sfx('warning')}catch(e){}draw(1);return}
   cont=false;msg((st==130?'Stopped.':st==27?'Not copied: files over 4 GB cannot go on a FAT32 drive (listed above). Use an exFAT or NTFS drive, or untick their folders.':st==9?'The copy read back from the drive does not match: the drive may be failing. Try another drive.':'The copy did not finish.')+' Nothing was erased: the disk is as it was.');
   eject();draw(1)}
  look()})}
/* ---------- after installing: bringing them in ---------- */
const brDoneK=()=>'bd95bring:'+(typeof uid=='function'?uid():'');
const brSetKey=s=>s.dir.split('/').slice(3).join('/')+'|'+s.made;
const brDone=()=>{try{return ld(brDoneK(),[])}catch(e){return[]}};
const brMark=k=>{const d=brDone();if(!d.includes(k)){d.push(k);svj(brDoneK(),d.slice(-50))}};
const brReady=()=>typeof RD!='undefined'&&RD&&typeof hdOn=='function'&&hdOn();
/* the antivirus on what came in (its first and last 4 MB, the whole file up to 8 MB), as on files extracted from a ZIP */
async function brReadPart(rel,off,len){const r=await fetch('/api/home/read?acct='+encodeURIComponent(HD.acct)+'&path='+encodeURIComponent(rel),{headers:Object.assign({Range:off<0?`bytes=${off}`:`bytes=${off}-${off+len-1}`},RD&&RD.token?{'X-Rudolf-Token':RD.token}:{})});if(!r.ok&&r.status!=206)throw new Error('HTTP '+r.status);return new Uint8Array(await r.arrayBuffer())}
async function brScan(rels,onN){const bad=[];if(typeof SECC=='undefined'||!SECC.av.on)return bad;let n=0;const P=typeof DL_AVPART=='number'?DL_AVPART:4*1048576;
 for(const rel of rels){n++;if(onN&&n%10==1)onN(n);const nm=rel.split('/').pop().toUpperCase();let hits=[];
  try{const h=await brReadPart(rel,0,P*2);const parts=[new TextDecoder('latin1').decode(h)];if(h.length>=P*2)parts.push(new TextDecoder('latin1').decode(await brReadPart(rel,-P,P)));
   try{avScan(nm,'',true).forEach(x=>hits.push(x))}catch(e){}if(/^(MZ|\x7fELF)/.test(parts[0]))hits.push('Win.Executable.Header');
   for(const s of parts){if(/EICAR-STANDARD-ANTIVIRUS-TEST-FILE/.test(s))hits.push('EICAR-Test-File');try{SIGS.forEach(([k,r])=>{if(r.test(s))hits.push(k)})}catch(e){}}}catch(e){continue}
  hits=[...new Set(hits)];if(!hits.length)continue;
  try{await rdPost('home/delete?acct='+encodeURIComponent(HD.acct)+'&path='+encodeURIComponent(rel));logEvt('av',`Blocked ${nm} (brought in from the old computer): ${hits.join(', ')}`);bad.push(rel)}catch(e){}}
 return bad}
async function brImport(set,person,ui){if(BR.run)return;BR.run=true;const say=ui.say,bar=ui.bar;
 try{await rdPost(`bring/import?acct=${encodeURIComponent(HD.acct)}&dir=${encodeURIComponent(set.dir)}&person=${encodeURIComponent(person.dir)}`);
  const j=await brJob('bring-import',log=>{const p=brProg(log);if(p){const [dn,tn,db,tb]=p;bar(tb?db/tb*900:dn/Math.max(1,tn)*900);say(`Bringing in ${dn.toLocaleString()} of ${tn.toLocaleString()} files · ${brB(db)} of ${brB(tb)}…`)}});
  const tail=brText(j.log).trim();
  if(j.status!=0&&j.status!=3){say('Not brought in: '+(tail.split('\n').pop()||'it stopped.'));return}
  let rels=[];try{const r=await fetch('/api/home/read?acct='+encodeURIComponent(HD.acct)+'&path='+encodeURIComponent('.rudolf/bring-imported.txt'),{headers:RD&&RD.token?{'X-Rudolf-Token':RD.token}:{}});if(r.ok)rels=(await r.text()).split('\n').filter(Boolean)}catch(e){}
  say(SECC&&SECC.av.on?'Checking what came in with the antivirus…':'Putting them in place…');
  const bad=await brScan(rels,n=>{bar(900+n/Math.max(1,rels.length)*100);say(`Checking with the antivirus: ${n.toLocaleString()} of ${rels.length.toLocaleString()} files…`)});
  bar(1000);brMark(brSetKey(set)+'|'+person.dir);try{if(typeof hdRefresh=='function')hdRefresh()}catch(e){}
  const lines=tail.split('\n').filter(l=>/^(Brought in|\d+ files were not)|^  /.test(l));
  const av=bad.length?` The antivirus blocked ${bad.length} (deleted here, still on the drive): ${bad.slice(0,3).map(x=>x.split('/').pop()).join(', ')}${bad.length>3?'…':''}.`:'';
  say((lines.join('\n')||'Done.')+av+' Your old Documents are in Documents, Pictures in Pictures and so on; the rest in Documents\\From the old computer. The copy on the drive is left as it is: delete it when you no longer need it.');
  notify('Your files from the old computer are here',`${rels.length.toLocaleString()} files brought into your folders.${av}`,{kind:bad.length||j.status==3?'warn':'',action:()=>{try{explorer([...homePath(),'DOCUMENTS'])}catch(e){}}})}
 catch(e){say('Not brought in: '+e.message)}
 finally{BR.run=false}}
/* Settings › Update and recovery: Bring in files from the old computer */
function brSection(c){if(typeof RD=='undefined'||!RD)return;const x=c.querySelector('.updx');if(!x)return;
 const d=brEl('div','brin');d.innerHTML=`<h4 class='sh4'>Bring in files from the old computer</h4><p class='msg'>The files the installer kept on a USB drive (${BR_TOP}) go into your folders: the old Documents into Documents, Pictures into Pictures and so on. Plug the drive in, then look.</p><div class='brsets'></div><div class='rmrow' style='justify-content:flex-start'><button class='brlook'>Look on the drives</button></div>`;
 x.appendChild(d);const q=s=>d.querySelector(s),box=q('.brsets');
 const look=async()=>{box.textContent='';if(!brReady()){box.appendChild(brEl('p','msg','Sign in to an account that keeps its files on this computer\'s disk (an installed Rudolf OS) to bring them in. Otherwise copy them from the drive in File Explorer.'));return}
  box.appendChild(brEl('p','msg','Looking…'));let r;try{r=await rd('bring/found')}catch(e){box.textContent=e.message;return}box.textContent='';
  const sets=r.sets||[];if(!sets.length){box.appendChild(brEl('p','msg','No copy of an old computer\'s files on the drives plugged in.'));return}
  const done=brDone();
  sets.forEach(s=>{const f=brEl('fieldset','brset');f.style.cssText='border:1px solid var(--bd,#cbd5e1);border-radius:8px;margin:6px 0;padding:6px 10px';
   f.appendChild(brEl('legend','',`${s.system=='windows'?'Windows':s.system=='linux'?'Linux':s.system} · copied ${String(s.made||'').slice(0,10)}${s.drive>1||!s.last?` · drive ${s.drive}${s.last?' (the last)':' (more on another drive)'}`:''}`));
   const me=(typeof USER!='undefined'&&USER&&USER.n||'').toLowerCase();let pick=(s.people||[]).findIndex(p=>p.name.toLowerCase()==me);if(pick<0)pick=0;
   (s.people||[]).forEach((p,i)=>{const l=brEl('label','ck');const rb=document.createElement('input');rb.type='radio';rb.name='brp'+s.made+s.drive;rb.className='brpp';rb.value=i;rb.checked=i==pick;l.appendChild(rb);
    l.append(` ${p.name}'s files — ${brB(p.bytes)} (${Number(p.files).toLocaleString()} files)${done.includes(brSetKey(s)+'|'+p.dir)?' · brought in already':''}`);f.appendChild(l)});
   const pg=document.createElement('progress');pg.max=1000;pg.value=0;pg.hidden=true;pg.style.width='100%';const m=brEl('p','msg brim');m.style.whiteSpace='pre-wrap';
   const b=brEl('button','pri brgoin','Bring them in');b.onclick=async()=>{const i=+((f.querySelector('.brpp:checked')||{}).value||0),p=s.people[i];if(!p)return;b.disabled=true;pg.hidden=false;
    await brImport(s,p,{say:t=>{m.textContent=t},bar:v=>{pg.value=v}});b.disabled=false};
   f.append(b,pg,m);box.appendChild(f)})};
 q('.brlook').onclick=look}
(function(){if(typeof updPanel!='function')return;updPanel=__after(updPanel,function(c){try{brSection(c)}catch(e){console.warn('bring',e)}},'bring62.js')})();
/* the offer: at sign-in, and when a drive with a copy is plugged in (Standard's page; Native's service page) */
async function brOffer(){if(!brReady())return;if(typeof ONEAPP!='undefined'&&ONEAPP&&!(typeof OA_SVC!='undefined'&&OA_SVC))return;
 let r;try{r=await rd('bring/found')}catch(e){return}
 const done=brDone(),acct=HD.acct;
 for(const s of r.sets||[]){const k=acct+'|'+brSetKey(s);if(BR.offered.has(k))continue;if((s.people||[]).every(p=>done.includes(brSetKey(s)+'|'+p.dir)))continue;BR.offered.add(k);
  notify('Bring in your files from the old computer?',`A copy of ${s.system=='windows'?'Windows':s.system=='linux'?'Linux':'the old computer'}'s files is on a drive (${(s.people||[]).map(p=>p.name).join(', ')}). Click to bring them into your folders.`,{app:'Settings',toast:12000,action:()=>{try{settings('upd')}catch(e){}}});break}}
setInterval(()=>{if(!brReady())return;if(BR.acct===HD.acct)return;BR.acct=HD.acct;setTimeout(brOffer,4000)},5000);
addEventListener('rudolf-drive',e=>{const v=e.detail||{};if(v.a=='add')setTimeout(brOffer,2500)});
