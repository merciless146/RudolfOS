/* ===== Automatic security updates (rudolf-update) =====
   Once a day Rudolf OS checks the signed release list. A newer release (Chromium and kernel security fixes)
   is downloaded as changes only into the spare system slot, then starts at the next restart; if it does
   not start twice, the computer goes back to the previous system by itself. This module schedules the
   checks, shows the state in Settings > Update and recovery, and offers Restart and Roll back. */
let SYSU=null;
async function sysuStatus(){if(!RD)return null;try{SYSU=await rd('sys/status')}catch(e){SYSU=null}return SYSU}
async function sysuJob(what,onLine){await rdPost('sys/'+what);let seen=0;
 for(;;){await new Promise(r=>setTimeout(r,700));const j=await rd('job');const ls=(j.log||'').split('\n');
  if(onLine)for(;seen<ls.length-1;seen++)onLine(ls[seen]);if(!j.running){if(onLine&&ls[seen])onLine(ls[seen]);await sysuStatus();return j.status}}}
const sysuWhen=t=>{if(!t)return'never';const d=new Date(t*1000),n=new Date();return(d.toDateString()==n.toDateString()?'today':d.toLocaleDateString())+' at '+d.toLocaleTimeString([],{hour:'numeric',minute:'2-digit'})};
function sysuRestart(){if(!(SW&&wins.includes(SW))){settings('upd');return setTimeout(sysuRestart,400)}sdlg(SW,'Restart to finish the update','Rudolf OS restarts now and starts the updated system. Save your work first.',[['Later'],['Restart now',()=>rdPost('power?action=reboot').catch(e=>notify('Restart',e.message,{kind:'error'})),1]])}
/* ---------- Settings > Update and recovery ---------- */
{const _up=updPanel;updPanel=function(c){_up(c);if(!RD)return;const x=c.querySelector('.updx');if(!x)return;
 const box=document.createElement('div');box.className='sysu';x.insertBefore(box,x.firstChild);
 let busy=false;const adm=isAdmin();
 async function draw(){const s=await sysuStatus();if(!box.isConnected)return;
  if(!s){box.innerHTML=`<h4 class='sh4'>Security updates</h4><p class='msg'>This Rudolf OS has no automatic updates. Use a newer disc image.</p>`;return}
  const cur=s.current||{},st=s.state||{},av=s.available,nx=st.next&&s.slots[st.next]&&s.slots[st.next].complete?s.slots[st.next]:null;
  let line,cls='';
  if(!s.persistent){line='Security updates need Rudolf OS installed on this computer, or a USB drive with a RUDOLFDATA partition (see Install below).';cls='warn'}
  else if(s.trying&&st.good==s.booted){line=`<b>Kept:</b> Rudolf OS ${pkgEsc(cur.version||RVER)} started successfully and is now the system this computer starts.`;cls='ok'}
  else if(s.trying)line='Trying the new version: it is kept as soon as the desktop has started.';
  else if(nx){line=`<b>Ready:</b> Rudolf OS ${pkgEsc(nx.version)} starts when the computer restarts.`;cls='ok'}
  else if(av){line=`<b>Update available:</b> Rudolf OS ${pkgEsc(av.version)}${av.notes?' · '+pkgEsc(av.notes):''}${av.urgent?' · <b>urgent security fix</b>':''}`;cls=av.urgent?'warn':''}
  else if(!s.url)line='No update address is set yet.';
  else line='Rudolf OS is up to date.';
  box.innerHTML=`<h4 class='sh4'>Security updates</h4><div class='sysucard ${cls}'><div>${line}</div>
<div class='msg'>Rudolf OS ${pkgEsc(cur.version||RVER)}${cur.chromium?' · Chromium '+pkgEsc(cur.chromium):''}${cur.kernel?' · Linux '+pkgEsc(cur.kernel):''} · checked ${sysuWhen(s.checked)}${s.booted!='disc'?' · system slot '+s.booted.toUpperCase():''}</div>
${st.failed?`<div class='msg warn'>The update in slot ${pkgEsc(st.failed.toUpperCase())} did not start, so Rudolf OS went back to the previous version. It will try the next release.</div>`:''}</div>
<div class='rmrow' style='justify-content:flex-start'><button class='ucheck'>Check now</button>${av&&!nx&&s.persistent?`<button class='udl'>Download (only the changes)</button>`:''}${nx?`<button class='urs'>Restart now</button>`:''}${st.good&&st.good!='disc'||st.prev?`<button class='urb'>Roll back system…</button>`:''}</div>
<label class='ck'><input type='checkbox' class='uauto'${s.auto?' checked':''}${adm?'':' disabled'}> Download security updates automatically</label>
<details class='uadv'><summary>Update address</summary><div class='rmrow' style='justify-content:flex-start'><input class='uurl' value='${pkgEsc(s.url)}' placeholder='https://…/stable' style='flex:1'${adm?'':' disabled'}><button class='usave'${adm?'':' disabled'}>Save</button></div><p class='msg'>Releases must be signed by a key this computer trusts; others are refused.</p></details>
<pre class='jl ulog' hidden></pre>`;
  const q=s=>box.querySelector(s),log=q('.ulog');
  box.querySelectorAll('button').forEach(b=>b.disabled=busy||b.disabled);
  const job=async w=>{busy=true;log.hidden=false;log.textContent='';draw();try{await sysuJob(w,l=>{log.textContent+=l+'\n';log.scrollTop=log.scrollHeight})}catch(e){log.textContent+=e.message+'\n'}busy=false;const keep=log.textContent;await draw();const L=box.querySelector('.ulog');L.hidden=false;L.textContent=keep};
  q('.ucheck').onclick=()=>job('check');
  q('.udl')&&(q('.udl').onclick=()=>job('download'));
  q('.urs')&&(q('.urs').onclick=sysuRestart);
  q('.urb')&&(q('.urb').onclick=async()=>{if(!await elevate('Go back to the previous version of Rudolf OS'))return;try{const r=await rdPost('sys/rollback');notify('Roll back',(r.message||'').trim()||'Done.')}catch(e){notify('Roll back',e.message,{kind:'error'})}draw()});
  q('.uauto').onchange=async e=>{const v=e.target.checked;if(!await elevate(v?'Turn on automatic security updates':'Turn off automatic security updates')){e.target.checked=!v;return}try{await rdPost('sys/set?k=auto&v='+(v?1:0))}catch(er){notify('Updates',er.message,{kind:'error'})}draw()};
  q('.usave').onclick=async()=>{const v=q('.uurl').value.trim();if(v&&!/^https?:\/\//.test(v))return notify('Updates','The address starts with https://',{kind:'error'});if(!await elevate('Change where Rudolf OS gets updates'))return;try{await rdPost('sys/set?k=url&v='+encodeURIComponent(v));notify('Updates','Saved. Checking for updates…');job('check')}catch(e){notify('Updates',e.message,{kind:'error'})}}};
 draw()}}
/* ---------- daily check and download ---------- */
async function sysuTick(){if(!RD||!USER||window.LOCKED)return;const s=await sysuStatus();if(!s||!s.persistent||!s.auto||!s.url)return;
 const st=s.state||{},ready=st.next&&s.slots[st.next]&&s.slots[st.next].complete;
 if(ready){if(!sysuTick.told){sysuTick.told=1;notify('Security update ready','Restart to finish installing Rudolf OS '+s.slots[st.next].version+'.',{action:sysuRestart,kind:'info'})}return}
 if(Date.now()/1000-(s.checked||0)<20*3600&&!(s.available&&!ready))return;
 try{const j=await rd('job');if(j.running)return}catch(e){return}
 try{if(Date.now()/1000-(s.checked||0)>=20*3600)await sysuJob('check');const s2=SYSU;if(s2&&s2.available){await sysuJob('download');const s3=SYSU,n=s3&&s3.state&&s3.state.next;
   if(n&&s3.slots[n].complete){sysuTick.told=1;notify(s2.available.urgent?'Urgent security update ready':'Security update ready','Restart to finish installing Rudolf OS '+s3.slots[n].version+'.',{action:sysuRestart,kind:s2.available.urgent?'warning':'info'})}}}catch(e){}}
if(RD){setTimeout(sysuTick,10*60*1000);setInterval(sysuTick,60*60*1000)}
