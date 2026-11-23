/* ===== Automatic security updates (rudolf-update) =====
   Once a day Rudolf OS checks the signed release list. A newer release (Chromium and kernel security fixes)
   is downloaded as changes only into the spare system slot, then starts at the next restart; if it does
   not start twice, the computer goes back to the previous system by itself. This module schedules the
   checks, shows the state in Settings > Update and recovery, and offers Restart and Roll back. */
let SYSU=null;
async function sysuStatus(){if(!RD)return null;try{SYSU=await rd('sys/status')}catch(e){SYSU=null}return SYSU}
async function sysuJob(what,onLine,body){await(body?rd('sys/'+what,{method:'POST',body}):rdPost('sys/'+what));let seen=0;
 for(;;){await new Promise(r=>setTimeout(r,700));const j=await rd('job');const ls=(j.log||'').split('\n');
  if(onLine)for(;seen<ls.length-1;seen++)onLine(ls[seen]);if(!j.running){if(onLine&&ls[seen])onLine(ls[seen]);await sysuStatus();return j.status}}}
/* (2026-11-19, the Vivobook: "checked today at 7:33 PM" at 1:33 PM in Chicago) in the desktop's time zone (Display Properties,
   S.tz), as the taskbar's clock: the page itself runs in the system's, UTC */
const sysuWhen=t=>{if(!t)return'never';const d=new Date(t*1000),z=typeof S=='object'&&S.tz?{timeZone:S.tz}:{};let same,day,tm;
 try{const f=x=>x.toLocaleDateString('en-CA',z);same=f(d)==f(new Date());day=d.toLocaleDateString([],z);tm=d.toLocaleTimeString([],Object.assign({hour:'numeric',minute:'2-digit'},z))}
 catch(e){same=d.toDateString()==new Date().toDateString();day=d.toLocaleDateString();tm=d.toLocaleTimeString([],{hour:'numeric',minute:'2-digit'})}
 return(same?'today':day)+' at '+tm};
function sysuRestart(){if(!(SW&&wins.includes(SW))){settings('upd');return setTimeout(sysuRestart,400)}sdlg(SW,'Restart to finish the update','Rudolf OS restarts now and starts the updated system. Save your work first.',[['Later'],['Restart now',()=>rdPost('power?action=reboot').catch(e=>notify('Restart',e.message,{kind:'error'})),1]])}
/* ---------- Settings > Update and recovery ---------- */
{updPanel=__after(updPanel,function(c){if(!RD)return;const x=c.querySelector('.updx');if(!x)return;
 const box=document.createElement('div');box.className='sysu';x.insertBefore(box,x.firstChild);
 let busy=false;const adm=isAdmin();
 async function draw(){const s=await sysuStatus();if(!box.isConnected)return;
  if(!s){box.innerHTML=`<h4 class='sh4'>Security updates</h4><p class='msg'>This Rudolf OS has no automatic updates. Use a newer disc image.</p>`;return}
  const cur=s.current||{},st=s.state||{},av=s.available,nx=st.next&&s.slots[st.next]&&s.slots[st.next].complete?s.slots[st.next]:null;
  let line,cls='';
  if(!s.persistent){line='Security updates need Rudolf OS installed on this computer, or a USB drive with a RUDOLFDATA partition (see Install below).';cls='warn'}
  /* 1.5.7: a recovery start, or a system running off a USB stick or disc on an installed computer, never changes the installed system */
  else if(s.canUpdate===false){line=s.recovery?'Recovery mode: updates are paused. Restart normally to update Rudolf OS.':'Rudolf OS is running from a USB drive or disc: start it from this computer\'s own drive to update it.';cls='warn'}
  /* 1.5.8: the stick's rescue system (slot R) started because no updated system on the drive could */
  else if(s.booted=='r'&&!nx){line=`<b>Rescue system:</b> no updated Rudolf OS on this drive could start, so its rescue system (${pkgEsc(cur.version||RVER)}, kept by Make a stick) started. Install the latest update to put a working system back.`;cls='warn'}
  else if(s.trying&&st.good==s.booted){line=`<b>Kept:</b> Rudolf OS ${pkgEsc(cur.version||RVER)} started successfully and is now the system this computer starts.`;cls='ok'}
  else if(s.trying)line='Trying the new version: it is kept as soon as the desktop has started.';
  else if(nx){line=`<b>Ready:</b> Rudolf OS ${pkgEsc(nx.version)} starts when the computer restarts.`;cls='ok'}
  else if(av){line=`<b>Update available:</b> Rudolf OS ${pkgEsc(av.version)}${av.notes?' · '+pkgEsc(av.notes):''}${av.urgent?' · <b>urgent security fix</b>':''}${sysuWholeNote(av)}`;cls=av.urgent?'warn':''}
  else if(!s.url)line='No update address is set yet.';
  else line='Rudolf OS is up to date.';
  box.innerHTML=`<h4 class='sh4'>Security updates</h4><div class='sysucard ${cls}'><div>${line}</div>
<div class='msg'>Rudolf OS ${pkgEsc(cur.version||RVER)}${cur.chromium?' · Chromium '+pkgEsc(cur.chromium):''}${cur.kernel?' · Linux '+pkgEsc(cur.kernel):''} · checked ${sysuWhen(s.checked)}${s.booted=='r'?' · the rescue system':s.booted!='disc'?' · system slot '+s.booted.toUpperCase():''}</div>
${st.failed?`<div class='msg warn'>The update in slot ${pkgEsc(st.failed.toUpperCase())} did not start, so Rudolf OS went back to the previous version. It will try the next release.</div>`:''}</div>
<div class='rmrow' style='justify-content:flex-start'><button class='ucheck'>Check now</button>${av&&!nx&&s.persistent&&s.canUpdate!==false?`<button class='udl'>${sysuDlLabel(av)}</button>`:''}${nx?`<button class='urs'>Restart now</button>`:''}${st.good&&st.good!='disc'||st.prev?`<button class='urb'>Roll back system…</button>`:''}${sysuStableShown(s)?`<button class='ustable'${adm?'':' disabled'} title='${adm?'':'Only an administrator can do this.'}'>Roll back to last stable version…</button>`:''}</div>
<label class='ck'><input type='checkbox' class='uauto'${s.auto?' checked':''}${adm?'':' disabled'}> Download security updates automatically</label>
<label class='ck'><input type='checkbox' class='ubeta'${s.beta?' checked':''}${adm&&!s.developer?'':' disabled'}> Get beta versions</label>
<p class='msg ubetamsg'>${s.beta?'This computer gets beta versions: early releases for testing, which can have problems. Turn this off to go back to the latest stable release at the next update.':s.toStable?'Beta versions are off: the next update brings the latest stable release.':'Beta versions are early releases for testing. Turning them on needs an administrator\'s name and password.'}</p>
${s.recoveryVersion?`<p class='msg'>Recovery mode (in the start-up menu) starts the official Rudolf OS ${pkgEsc(s.recoveryVersion)}, the newest official stable release when it was last renewed${s.recoveryAt?' ('+new Date(s.recoveryAt*1000).toLocaleDateString()+')':''}. It is kept up to date from the official address, whatever the update address.</p>`:''}<details class='uadv'><summary>Update address</summary><div class='rmrow' style='justify-content:flex-start'><input class='uurl' value='${pkgEsc(s.url)}' placeholder='https://…/stable' style='flex:1'${adm?'':' disabled'}><button class='uchk'>Check this address</button><button class='usave'${adm?'':' disabled'}>Save</button></div><div class='uck'></div><p class='msg'>Releases must be signed by a key this computer trusts; others are refused.</p></details>
<pre class='jl ulog' hidden></pre>`;
  const q=s=>box.querySelector(s),log=q('.ulog');
  box.querySelectorAll('button').forEach(b=>b.disabled=busy||b.disabled);
  const job=async(w,body)=>{busy=true;log.hidden=false;log.textContent='';draw();let r=null;try{r=await sysuJob(w,l=>{log.textContent+=l+'\n';log.scrollTop=log.scrollHeight},body)}catch(e){log.textContent+=e.message+'\n'}busy=false;const keep=log.textContent;await draw();const L=box.querySelector('.ulog');L.hidden=false;L.textContent=keep;return{status:r,log:keep}};
  q('.ucheck').onclick=()=>job('check');
  q('.udl')&&(q('.udl').onclick=()=>job('download'));
  q('.urs')&&(q('.urs').onclick=sysuRestart);
  q('.ustable')&&(q('.ustable').onclick=()=>sysuToStable(s,job,draw));
  q('.urb')&&(q('.urb').onclick=async()=>{if(!await elevate('Go back to the previous version of Rudolf OS'))return;try{const r=await rdPost('sys/rollback');notify('Roll back',(r.message||'').trim()||'Done.')}catch(e){notify('Roll back',e.message,{kind:'error'})}draw()});
  q('.uauto').onchange=async e=>{const v=e.target.checked;if(!await elevate(v?'Turn on automatic security updates':'Turn off automatic security updates')){e.target.checked=!v;return}try{await rdPost('sys/set?k=auto&v='+(v?1:0))}catch(er){notify('Updates',er.message,{kind:'error'})}draw()};
  /* 1.6.0 (owner, 2026-11-15): beta versions are opt-in: on needs an administrator's name and password (checked by
     rudolfd); off brings the latest stable release at the next update */
  q('.ubeta').onchange=async e=>{const v=e.target.checked;
   if(v){const cr=await elevate('Get beta versions of Rudolf OS',{cred:true});if(!cr){e.target.checked=false;return}
    try{await rd('sys/beta?on=1',{method:'POST',body:JSON.stringify({n:cr.n,p:cr.p})});notify('Updates','Beta versions are on. Checking for updates…');job('check')}catch(er){e.target.checked=false;notify('Updates',er.message,{kind:'error'});draw()}return}
   if(!await elevate('Stop getting beta versions of Rudolf OS')){e.target.checked=true;return}
   try{await rdPost('sys/beta?on=0');notify('Updates','Beta versions are off: the next update brings the latest stable release. Checking for updates…');job('check')}catch(er){e.target.checked=true;notify('Updates',er.message,{kind:'error'});draw()}};
  /* 1.6.0 (plan-1.6.1 Part 16, moved to 1.6.0): Check this address (rudolf-update check-url): whether it answers, its
     release list, whose key signed it (this computer's: updates install; another build's: only a USB stick), its files,
     its other channel. Save checks it first, and asks before an address whose updates would not install here; the
     address is an administrator's (rudolfd checks the name and password) */
  const uAsk=t=>new Promise(res=>{const o=document.createElement('div');o.className='rmodal';o.innerHTML="<div class='rmbox' role='dialog' aria-modal='true' style='max-width:520px'><p class='msg'></p><div class='rmrow'><button class='n'>Cancel</button><button class='pri y'>Save it</button></div></div>";
   o.querySelector('p').textContent=t;const done=v=>{o.remove();res(v)};o.querySelector('.n').onclick=()=>done(false);o.querySelector('.y').onclick=()=>done(true);document.body.appendChild(o);o.querySelector('.n').focus()});
  const uck=async v=>{const box=q('.uck');box.textContent='Checking '+v+' …';try{const r=await rdPost('sys/urlcheck?url='+encodeURIComponent(v));
    box.textContent='';for(const [lv,t] of r.lines||[]){const d=document.createElement('div');d.className='msg';d.textContent=(lv=='ok'?'✅ ':lv=='warn'?'⚠️ ':'❌ ')+t;box.appendChild(d)}
    if(r.fpr){const d=document.createElement('div');d.className='msg';d.textContent='Key: '+r.fpr+(r.key=='official'?' (the official Rudolf OS key)':r.key=='developer'?' (Developer mode)':' (another build)');box.appendChild(d)}return r}
   catch(e){box.textContent='❌ '+e.message;return null}};
  q('.uchk').onclick=()=>{const v=q('.uurl').value.trim();if(!/^https:\/\//.test(v))return notify('Updates','The address starts with https://',{kind:'error'});uck(v)};
  q('.usave').onclick=async()=>{const v=q('.uurl').value.trim();if(v&&!/^https:\/\//.test(v))return notify('Updates','The address starts with https://',{kind:'error'});
   if(v){const r=await uck(v);if(!r)return;
    if(r.key=='other'&&!await uAsk('This address is another build of Rudolf OS (key '+r.fpr+'). Its updates do not install on this computer, so updates will stop here until the address is changed back. You can still make a USB stick from it. Save it anyway?'))return;
    if(r.key=='none'&&!await uAsk('This address is not set up for this computer (see the lines above). Save it anyway?'))return}
   const cr=await elevate('Change where Rudolf OS gets updates',{cred:true});if(!cr)return;
   try{await rd('sys/set?k=url&v='+encodeURIComponent(v),{method:'POST',body:JSON.stringify({n:cr.n,p:cr.p})});notify('Updates','Saved. Checking for updates…');job('check')}catch(e){notify('Updates',e.message,{kind:'error'})}}};
 draw()},"sysupd25.js");}
/* ---------- 1.6.0 (the owner, 2026-11-16): Roll back to last stable version ----------
   Shown only when Get beta versions is on, or when this system is not a release the update server lists as published
   stable (a beta, a test build, a developer's build: rudolf-update checks the running release against the stable
   channel's signed list and its signed history). It checks for updates (so the last stable release is the server's
   now), says which release it goes to, refuses with why when that release cannot read this computer's data (the data
   record each release carries), asks an administrator's name and password (checked by rudolfd), then: that release
   from the other system slot when it is still there, else downloaded as an update; Get beta versions off; a restart.
   On a developer's build it is Return to official Rudolf OS (Settings › Security). */
function sysuStableShown(s){return !!(s&&s.persistent&&s.canUpdate!==false&&('runningStable' in s)&&(s.beta||s.runningStable===false))}
async function sysuToStable(s,job,draw){const T='Roll back to last stable version';
 if(!isAdmin())return notify(T,'Only an administrator can do this.',{kind:'error'});
 if(s.developer||s.signer=='custom'){if(typeof devmReturn=='function')return devmReturn(draw);return notify(T,'This computer runs a custom build: use Return to official Rudolf OS in Settings › Security.',{kind:'error'})}
 /* what the update server says now: the check refreshes the last stable release and its history */
 await job('check');
 let i=null;try{i=await rd('sys/stable')}catch(e){return notify(T,e.message,{kind:'error'})}
 if(!i||!i.stable)return sysuDlg(T,'<p>Rudolf OS could not find out which release is the last stable one. Check the connection, then try again.</p>',[['OK']]);
 if(i.how=='official'){if(typeof devmReturn=='function')return devmReturn(draw);return}
 const v=`Rudolf OS ${pkgEsc(i.stable.version)} (stable, released ${pkgEsc(i.stable.released||'?')})`;
 if(!i.compatible)return sysuDlg('Cannot roll back to '+i.stable.version,`<p><b>${v}</b></p><p class='ustwhy'>${pkgEsc(i.why)}</p><p>Your system stays as it is. Updates keep coming as before.</p>`,[['OK']]);
 const notes=(i.notes||[]).length?`<p>While ${pkgEsc(i.stable.version)} runs:</p><ul class='ustnotes'>${i.notes.map(n=>`<li>${pkgEsc(n.charAt(0).toUpperCase()+n.slice(1))}.</li>`).join('')}</ul><p class='msg'>They are kept for when a newer version comes back.</p>`:'';
 const how=i.how=='running'?`This computer already runs ${v}. Get beta versions is turned off, so it stays on the stable releases.`
  :`${v} starts the next time the computer restarts${i.how=='slot'?': it is still on this computer, so nothing is downloaded':': it is downloaded first, as updates are (only the changes)'}. Get beta versions is turned off, so the next update does not bring the beta back. Your accounts, files and apps are kept.`;
 if(!await sysuDlg(T,`<p>${how}</p>${notes}`,[['Cancel'],[i.how=='running'?'Turn off beta versions':'Roll back',1,1]]))return;
 const cr=await elevate(`Roll back to ${i.stable.version}, the last stable version of Rudolf OS`,{cred:true});if(!cr)return;
 const r=await job('tostable',JSON.stringify({n:cr.n,p:cr.p}));
 const last=((r&&r.log)||'').trim().split('\n').pop()||'';
 if(!r||r.status!==0)return notify(T,last||'It did not work.',{kind:'error'});
 typeof logEvt=='function'&&logEvt('sys',`${USER?USER.n:'?'} rolled back to ${i.stable.version}, the last stable version`);
 if(i.how=='running')return notify(T,last);
 sysuDlg('Restart to roll back',`<p>${v} starts when the computer restarts. Save your work first.</p>`,[['Later'],['Restart now',1,1]]).then(go=>go&&rdPost('power?action=reboot').catch(e=>notify('Restart',e.message,{kind:'error'})))}
/* a dialog over Settings (the page's rmodal): HTML made by the caller from escaped text; resolves with the button's value */
function sysuDlg(title,html,btns){return new Promise(res=>{const o=document.createElement('div');o.className='rmodal';
 o.innerHTML=`<div class='rmbox ustdlg' role='alertdialog' style='max-width:520px'><h3></h3>${html}<div class='rmrow'></div></div>`;o.querySelector('h3').textContent=title;o.firstChild.setAttribute('aria-label',title);
 const row=o.querySelector('.rmrow');btns.forEach(([t,val,pri])=>{const b=document.createElement('button');b.textContent=t;if(pri)b.className='pri';b.onclick=()=>{o.remove();res(val||0)};row.appendChild(b)});
 document.body.appendChild(o);const pb=row.querySelector('.pri')||row.lastChild;pb&&pb.focus()})}
/* ---------- daily check and download ---------- */
async function sysuTick(){if(!RD||!USER||window.LOCKED||window.__bsaver||(typeof gameMode=='function'&&gameMode()))return;   /* (1.4.4) not while battery saver is on */
 const s=await sysuStatus();if(!s||!s.persistent||!s.auto||!s.url||s.canUpdate===false)return;   /* (1.5.7: not from a stick or in recovery) */
 const st=s.state||{},ready=st.next&&s.slots[st.next]&&s.slots[st.next].complete;
 if(ready){if(!sysuTick.told){sysuTick.told=1;notify('Security update ready','Restart to finish installing Rudolf OS '+s.slots[st.next].version+'.',{action:sysuRestart,kind:'info',snd:'update'})}return}
 if(Date.now()/1000-(s.checked||0)<20*3600&&!(s.available&&!ready))return;
 /* 1.5.7: offline (at start-up, before the Wi-Fi has an address): check once online, not in an hour */
 if(typeof netLive=='function'&&!netLive()){if(!sysuTick.wait){sysuTick.wait=1;netWait().then(()=>setTimeout(()=>{sysuTick.wait=0;sysuTick()},20000))}return}
 try{const j=await rd('job');if(j.running)return}catch(e){return}
 try{if(Date.now()/1000-(s.checked||0)>=20*3600)await sysuJob('check');const s2=SYSU;
 /* 1.5.8: on a metered connection (cellular data) the update is found but not downloaded by itself; 1.6.0 (Part 4): offered
    with its size ("Download over mobile data: up to 42 MB": the layers' size, as only the changes come) */
 if(s2&&s2.available&&(typeof netMetered=='function'&&netMetered())){if(!sysuTick.metered){sysuTick.metered=1;notify('Update available','Rudolf OS '+(s2.available.version||'')+' was not downloaded: this connection is metered'+(MTR&&MTR.auto=='modem'?' (mobile data)':'')+'. '+sysuDlLabel(s2.available)+' in Settings › Updates when you want, or wait for another network.',{app:'Updates',action:()=>settings('upd')})}return}
 if(s2&&s2.available){await sysuJob('download');const s3=SYSU,n=s3&&s3.state&&s3.state.next;
   if(n&&s3.slots[n].complete){sysuTick.told=1;notify(s2.available.urgent?'Urgent security update ready':'Security update ready','Restart to finish installing Rudolf OS '+s3.slots[n].version+'.',{action:sysuRestart,snd:'update',kind:s2.available.urgent?'warning':'info'})}}sysuTick.fails=0}
 catch(e){/* 1.5.7: a check that failed is tried again in 5 minutes (3 times), not in an hour */if((sysuTick.fails=(sysuTick.fails||0)+1)<=3)setTimeout(sysuTick,300000)}}
if(RD){setTimeout(sysuTick,10*60*1000);setInterval(sysuTick,60*60*1000)}
/* 1.6.0 (plan Part 4): the download button's words: on mobile data, with the size it can come to */
/* 1.6.2 (Part 31, item 4): an update whose system images are packed another way than this computer's (rudolf-update's
   "whole": xz to zstd) downloads them whole, once: its size is said before downloading, on every connection */
function sysuDlLabel(av){const mb=av&&av.size?Math.max(1,Math.round(av.size/1e6)):0,cell=typeof netMetered=='function'&&netMetered()&&MTR&&MTR.auto=='modem',
  w=av&&av.whole>0?Math.max(1,Math.round(av.whole/1e6)):0,sz=w?': about '+w+' MB':mb?': up to '+mb+' MB':'';
 return cell?'Download over mobile data'+sz:typeof netMetered=='function'&&netMetered()?'Download on this metered connection'+sz:w?'Download (about '+w+' MB)':'Download (only the changes)'}
function sysuWholeNote(av){if(!(av&&av.whole>0))return'';const w=Math.max(1,Math.round(av.whole/1e6)),n=String(av.wholeLayers||'').split(',').filter(Boolean).length;
 return `<div class='msg'>This update packs the system's images in a new way${av.packed?' ('+pkgEsc(av.packed)+')':''}, so it downloads ${n>1?'them':'it'} whole this once: about ${w} MB. Later updates download only the changes again.</div>`}
