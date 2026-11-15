/* ===== 1.6.0 (docs/plan-1.6.0.md Part 13): Settings › Update and recovery › System extensions =====
   Signed system extensions: parts added to the system (a driver, firmware, printer support) without a new release, laid
   over the unchanged base read-only at each start. rudolfd's GET sysext/status (rpk sysext-status) says what is
   installed and what this start did with each one (in use, left out and why); Install…, Remove, Try again and Start once
   without extensions need an administrator's password, which rudolfd checks itself ({cred:1}). The App Store does not
   show them (they are not apps). Drawn as text. */
window.__sysext60=1;
function sxSize(b){return b>=1048576?(b/1048576).toFixed(1)+' MB':Math.max(1,Math.round(b/1024))+' KB'}
function sxState(e){if(e.off)return['Left out: '+e.off,'warn'];if(e.state=='in-use')return e.problem?['In use now; left out at the next start: '+e.problem,'warn']:['In use',''];
 if(e.state=='left-out')return['Left out: '+(e.why||'see System health'),'warn'];if(e.problem)return['Cannot be used: '+e.problem,'warn'];return['In use after a restart','']}
async function sxAdmin(what,name,reason){const cr=await elevate(reason,{cred:1});if(!cr)return null;
 return rd('sysext/'+what+(name?'?name='+encodeURIComponent(name):''),{method:'POST',body:JSON.stringify({n:cr.n,p:cr.p})})}
async function sxJobWait(onLine){let seen=0;for(;;){await new Promise(r=>setTimeout(r,600));const j=await rd('job');const L=(j.log||'').split('\n');
 for(;seen<L.length-1;seen++)onLine(L[seen]);if(!j.running){if(L[seen])onLine(L[seen]);return j.status===0}}}
async function sxSection(box,said){
 box.innerHTML=`<h4 class='sh4'>System extensions</h4><p class='msg'>Signed parts added to Rudolf OS without a new release (printer support, a driver, firmware). They are laid over the system at each start and never change it; one that would replace a part of Rudolf OS, or keeps Rudolf OS from starting, is left out.</p>
<p class='msg sxs'></p><table class='tmt pkt sxt'><thead><tr><th>Extension</th><th>Version</th><th>Size</th><th>State</th><th></th></tr></thead><tbody></tbody></table>
<div class='rmrow' style='justify-content:flex-start'><button class='sxi'>Install…</button><button class='sxc'>Check for extensions</button><button class='sxo'>Start once without extensions</button><button class='sxr' hidden>Restart now</button></div><p class='msg sxm'></p><pre class='jl sxl' hidden></pre>`;
 const q=s=>box.querySelector(s),tb=q('tbody'),m=q('.sxm'),log=q('.sxl');m.textContent=said||'';
 const row=t=>{tb.innerHTML='';const r=tb.insertRow(),d=r.insertCell();d.colSpan=5;d.style.opacity='.6';d.textContent=t};
 let s=null;try{s=await rd('sysext/status')}catch(e){row('The system extensions could not be read: '+e.message);box.querySelectorAll('button').forEach(b=>b.disabled=true);return}
 if(!box.isConnected)return;
 const inst=s.installed||[],adm=isAdmin(),busy=v=>box.querySelectorAll('button').forEach(b=>b.disabled=v||(b.dataset.adm&&!adm));
 const st=q('.sxs');st.textContent=s.started&&!['with extensions','none installed','not yet'].includes(s.started)?'This start: '+s.started+'.':s.skipOnce?'The next start is without system extensions.':'';
 if(!s.persistent){row('System extensions need Rudolf OS installed on this computer (or a RUDOLFDATA partition): they are kept there.');box.querySelectorAll('button').forEach(b=>b.disabled=true);return}
 let pend=false;
 if(!inst.length)row('No system extensions are installed.');else{tb.innerHTML='';inst.forEach(e=>{const r=tb.insertRow();r.dataset.name=e.name;
  const c0=r.insertCell(),b=document.createElement('b');b.textContent=e.title||e.name;c0.appendChild(b);if(e.summary){const d=document.createElement('div');d.className='msg';d.textContent=e.summary;c0.appendChild(d)}
  r.insertCell().textContent=e.version||'';r.insertCell().textContent=e.size?sxSize(e.size):'';
  const [t,cl]=sxState(e),c3=r.insertCell();c3.textContent=t;c3.className='sxst'+(cl?' '+cl:'');if(e.state=='next')pend=true;
  const c4=r.insertCell();
  if(e.off){const a=document.createElement('button');a.textContent='Try again';a.className='sxa';a.dataset.adm=1;a.onclick=async()=>{try{if(!await sxAdmin('retry',e.name,'Try the system extension '+(e.title||e.name)+' again at the next start'))return;sxSection(box,'It is tried again at the next start.')}catch(x){m.textContent=x.message}};c4.appendChild(a)}
  const rm=document.createElement('button');rm.textContent='Remove';rm.className='sxx';rm.dataset.adm=1;
  rm.onclick=async()=>{if(!await docAsk('Remove the system extension "'+(e.title||e.name)+'"? What it adds is gone after the next restart.','Remove','Cancel'))return;await job('remove',e.name,'Remove the system extension '+(e.title||e.name))};c4.appendChild(rm)})}
 q('.sxr').hidden=!pend&&!box.__changed;q('.sxr').onclick=async()=>{if(await docAsk('Restart Rudolf OS now? The system extensions change then. Save your work first.','Restart now','Later'))rdPost('power?action=reboot').catch(e=>notify('Restart',e.message,{kind:'error'}))};
 ['.sxi','.sxo','.sxx','.sxa'].forEach(k=>box.querySelectorAll(k).forEach(b=>b.dataset.adm=1));busy(false);
 async function job(what,name,reason){m.textContent='';let r=null;try{r=await sxAdmin(what,name,reason)}catch(e){m.textContent=e.message;return}if(!r)return;
  busy(true);log.hidden=false;log.textContent='';let ok=false;try{ok=await sxJobWait(l=>{log.textContent+=l+'\n';log.scrollTop=log.scrollHeight})}catch(e){log.textContent+=e.message+'\n'}
  const keep=log.textContent;box.__changed=box.__changed||ok;await sxSection(box,ok?(what=='install'?'Installed: it is in use after a restart.':'Removed: it is gone after a restart.'):'It did not work: see the lines below.');const L=box.querySelector('.sxl');L.hidden=false;L.textContent=keep}
 q('.sxo').onclick=async()=>{try{if(!await sxAdmin('skip-once','','Start Rudolf OS once without system extensions'))return;sxSection(box,'The next start is without system extensions; the one after uses them again.')}catch(e){m.textContent=e.message}};
 q('.sxc').onclick=async()=>{busy(true);m.textContent='Checking for extensions…';try{await rdPost('pkg/refresh');await sxJobWait(()=>{})}catch(e){m.textContent=e.message}sxSection(box,'Checked.')};
 q('.sxi').onclick=()=>{const av=(s.available||[]).filter(a=>!a.installed||a.installed!=a.version);
  const o=document.createElement('div');o.className='rmodal';o.innerHTML=`<div class='rmbox' role='dialog' aria-label='Install a system extension'><h3>Install a system extension</h3><p class='msg'>Signed by Rudolf OS for this version. It is in use after a restart.</p><div class='sxav'></div><div class='rmrow'><button class='sxn'>Cancel</button></div></div>`;
  const L=o.querySelector('.sxav');if(!av.length){const p=document.createElement('p');p.className='msg';p.textContent=(s.available||[]).length?'Every extension is installed.':'No extensions are listed for this version of Rudolf OS yet. Try Check for extensions.';L.appendChild(p)}
  av.forEach(a=>{const d=document.createElement('div');d.className='acr wfrow sxone';d.innerHTML=`<span style='flex:1'><b></b><div class='msg'></div></span><button class='pri'>Install</button>`;d.querySelector('b').textContent=(a.title||a.name)+' '+a.version;
   d.querySelector('.msg').textContent=[a.summary,a.size?sxSize(a.size):'',a.license].filter(Boolean).join(' · ');d.querySelector('button').onclick=()=>{o.remove();job('install',a.name,'Install the system extension '+(a.title||a.name))};L.appendChild(d)});
  o.querySelector('.sxn').onclick=()=>o.remove();document.body.appendChild(o)}}
(function(){if(typeof updPanel!='function')return;const U0=updPanel;updPanel=function(c){U0(c);if(typeof RD=='undefined'||!RD)return;const x=c.querySelector('.updx');if(!x)return;
 const box=document.createElement('div');box.className='sx60';const su=x.querySelector('.sysu');su?su.after(box):x.insertBefore(box,x.firstChild);sxSection(box)}})();
