/* ===== 1.4.0: the development link =====
   Settings › Security › Development link: a developer the owner chooses (a Claude session, a collaborator) runs tests
   on this computer through a private GitHub repository the owner owns (rudolf-devlink; rudolfd: devlink, devlink/on,
   off, forget, answer, log). While it is on: a red banner over everything with the time left and End now, and with
   Ask first every command waits here for Allow or Refuse. See docs/plan-1.4.0.md.
   1.5.9 (plan 15.4): off unless it is being used. Only when "Allow the development link on this computer" is ticked
   (its own switch, an administrator's password; 2026-11-14, in place of Developer mode, which needs a developer's key
   the official builds have no use for), only an administrator, whose password
   rudolfd checks itself; Ask first on again after every restart; off at every restart unless "Keep on after
   restarting"; off by itself after 4 hours without a command (said in a notification); System health shows it. */
window.__devlink40=1;
let DEVLST=null,DEVLASKED='';
const devlLeft=s=>{const m=Math.max(0,Math.round((s||0)/60));return m>=60?Math.floor(m/60)+' h '+(m%60)+' min':m+' min'};
const devlUntil=s=>s&&s.until?new Date(s.until*1000).toLocaleTimeString([], {hour:'2-digit',minute:'2-digit'}):'';
/* 1.5.9 (plan 15.7): read-only unless "Allow changes to this computer" (an administrator's password; off at every start) */
const devlMode=s=>s&&s.change?'can change this computer':'read-only';
function devlSummary(s){if(!s||!s.available)return'not available';if(!s.on)return'off';return'on until '+devlUntil(s)+' ('+s.repo+', '+devlMode(s)+(s.ask?', asks first':', runs without asking')+')'}
async function devlOff(){try{await rdPost('devlink/off')}catch(e){}devlPoll()}
/* 1.5.9 (plan 12.3b step 7): an app window of the Native desktop (one-app mode) draws no banner: it covered the top of
   every app (Settings' search box); the native shell shows the banner once, and asks about commands itself */
const devlOneApp=()=>typeof ONEAPP!='undefined'&&!!ONEAPP;
function devlBanner(s){let b=document.getElementById('devlinkBanner');
 if(devlOneApp())s=null;
 if(!s||!s.on){if(b)b.remove();document.documentElement.style.removeProperty('--dl-top');return}
 if(!b){b=document.createElement('div');b.id='devlinkBanner';b.setAttribute('role','alert');
  b.style.cssText='position:fixed;left:0;right:0;top:0;z-index:2147483647;display:flex;align-items:center;gap:10px;padding:3px 10px;background:#b00020;color:#fff;font:600 13px system-ui,sans-serif;box-shadow:0 1px 4px #0006;pointer-events:auto';
  b.innerHTML="<span class='dlt' style='flex:1;overflow:hidden;white-space:nowrap;text-overflow:ellipsis'></span><button class='dle' style='background:#fff;color:#b00020;border:0;border-radius:4px;padding:2px 10px;font-weight:700;cursor:pointer'>End now</button>";
  b.querySelector('.dle').onclick=devlOff;document.body.appendChild(b)}
 const st=s.status||{};let t='⚠ Development link on: commands from GitHub ('+s.repo+') '+(s.change?'can change this computer':'can look at this computer (read-only)')+' · '+devlLeft(s.secondsLeft)+' left';
 if(st.state&&st.state!='on')t+=' · '+st.state;if(st.error)t+=' · '+st.error;
 b.querySelector('.dlt').textContent=t;b.title=t}
function devlAsk(s){const p=s&&s.on&&!(devlOneApp()&&typeof oaNative=='function'&&oaNative())&&s.pending;let o=document.getElementById('devlinkAsk');
 if(!p){if(o)o.remove();DEVLASKED='';return}
 if(DEVLASKED==p.id&&o)return;if(o)o.remove();DEVLASKED=p.id;
 o=document.createElement('div');o.id='devlinkAsk';o.className='rmodal';o.style.zIndex='2147483646';
 o.innerHTML=`<div class='rmbox' role='alertdialog' aria-label='Development link: allow this command?' style='max-width:640px'><h3>🔧 Allow this command?</h3>
<p class='msg dlw'></p><pre class='dlc' style='max-height:45vh;overflow:auto;white-space:pre-wrap;background:#0003;padding:8px;border-radius:4px;font:12px monospace'></pre>
<p class='msg'>${s.change?'It runs with full control of this computer.':'It runs read-only: it can read files and take screenshots, but cannot change anything.'} Refuse anything you did not expect. Not answered in 10 minutes, it is refused.</p>
<div class='rmrow'><button class='dln'>Refuse</button><button class='pri dly'>Allow</button></div></div>`;
 o.querySelector('.dlw').textContent='Command '+p.id+' from '+s.repo+' ('+p.kind+'):';o.querySelector('.dlc').textContent=p.what||'';
 const ans=async a=>{o.remove();try{await rdPost('devlink/answer?id='+encodeURIComponent(p.id)+'&a='+a)}catch(e){}devlPoll()};
 o.querySelector('.dln').onclick=()=>ans('refuse');o.querySelector('.dly').onclick=()=>ans('allow');
 document.body.appendChild(o);o.querySelector('.dln').focus();typeof promptFront=='function'&&promptFront(o)}
/* 15.4: the link ended itself after 4 hours without a command: said once (status.json's time names that ending) */
function devlIdleNote(s){const st=s&&s.status;if(!s||s.on||!st||!/without a command/.test(st.state||'')||devlOneApp())return;
 if(!(st.at>Date.now()/1000-86400))return;let seen='';try{seen=localStorage.getItem('devl59idle')||''}catch(e){}
 if(seen==String(st.at))return;try{localStorage.setItem('devl59idle',String(st.at))}catch(e){}
 typeof notify=='function'&&notify('Development link','The development link turned itself off: 4 hours went by without a command. An administrator can switch it on again in Settings › Security.',{kind:'warning'})}
async function devlPoll(){if(!RD)return;try{DEVLST=await rd('devlink')}catch(e){return}devlBanner(DEVLST);devlAsk(DEVLST);devlIdleNote(DEVLST)}
/* 15.4: System health's line: on or off, since when and by whom, its last command */
function devlHealthText(s){if(!s||!s.available)return'';if(!s.on)return'Off';
 const lc=s.lastCommand,when=t=>t?new Date(t*1000).toLocaleString([], {hour:'2-digit',minute:'2-digit',day:'numeric',month:'short'}):'';
 return'On since '+when(s.since)+(s.by?' (switched on by '+s.by+')':'')+', '+s.repo+', '+devlMode(s)+(s.ask?', asks first':', runs without asking')+' · last command: '+(lc?lc.id+' ('+lc.kind+') at '+String(lc.t||'').replace('T',' ').replace(/:\d\dZ$/,' UTC')+', '+(lc.approved||''):'none yet')}
/* (called by healthPanel, sec50.js) */
function devlHealthRow(c){if(!RD)return;
 const d=document.createElement('div');d.className='dl59h r2';d.style.marginTop='6px';d.innerHTML="<label>Development link</label><div class='dl59t'>…</div>";
 const at=c.querySelector(':scope > .hlev');if(at)at.before(d);else c.appendChild(d);
 rd('devlink').then(s=>{const t=devlHealthText(s);if(!t)d.remove();else d.querySelector('.dl59t').textContent=t}).catch(()=>d.remove())}
if(typeof RD!='undefined'&&RD){devlPoll();setInterval(devlPoll,4000)}
async function devlLog(){let l='';try{l=(await rd('devlink/log')).log||''}catch(e){l=e.message}
 const rows=l.trim()?l.trim().split('\n').map(x=>{try{const j=JSON.parse(x);return j.t.replace('T',' ').replace('Z',' UTC')+'  '+j.id+'  '+j.kind+'  '+j.result+'  ('+j.approved+')\n    '+(j.what||'').replace(/\n/g,'\n    ')}catch(e){return x}}):['Nothing has been run yet.'];
 const o=document.createElement('div');o.className='rmodal';o.innerHTML=`<div class='rmbox' role='dialog' aria-label='Development link log' style='max-width:760px'><h3>Development link: log (30 days)</h3><pre style='max-height:60vh;overflow:auto;white-space:pre-wrap;font:12px monospace'></pre><div class='rmrow'><button class='pri'>Close</button></div></div>`;
 o.querySelector('pre').textContent=rows.reverse().join('\n');o.querySelector('button').onclick=()=>o.remove();document.body.appendChild(o)}
/* 2026-11-14: "Allow the development link on this computer": ticking it asks an administrator's password (rudolfd
   checks it); unticking needs none and ends the link at once */
function devlAllowRow(f,s,adm,redraw){const d=document.createElement('div');d.className='dlalw';
 d.innerHTML="<label style='display:block;margin:6px 0;font-weight:600'><input type='checkbox' class='dlal'> Allow the development link on this computer</label><p class='msg'>Unticked, it cannot be switched on at all; unticking it ends it at once. Ticking it needs an administrator's password.</p><p class='msg dlalerr' style='color:#e66'></p>";
 f.prepend(d);const cb=d.querySelector('.dlal');cb.checked=!!s.allowed;cb.disabled=!adm&&!s.allowed;
 cb.onchange=async()=>{const e=d.querySelector('.dlalerr');e.textContent='';
  if(!cb.checked){try{await rdPost('devlink/allow?on=0');typeof logEvt=='function'&&logEvt('sys',`${USER?USER.n:'?'} disallowed the development link`)}catch(x){e.textContent=x.message}devlPoll();redraw();return}
  window.__elevTrust=0;const cr=await elevate('Allow the development link on this computer: an administrator can then let commands from a GitHub repository look at (or, if allowed, change) this computer.',{cred:1});
  if(!cr){cb.checked=false;return}
  try{await rd('devlink/allow?on=1',{method:'POST',body:JSON.stringify({n:cr.n,p:cr.p})})}catch(x){e.textContent=x.message||'It could not be allowed.';cb.checked=false;return}
  redraw()}}
async function devlSection(c,W){
 const box=document.createElement('div');box.className='dl40';c.appendChild(box);
 const draw=async()=>{let s=null;try{s=await rd('devlink')}catch(e){}
  box.innerHTML="<h4 class='sh4'>Development link</h4><p class='msg dls'></p><div class='dlf'></div><div class='rmrow' style='justify-content:flex-start'></div>";
  const t=box.querySelector('.dls'),f=box.querySelector('.dlf'),row=box.querySelector('.rmrow'),adm=isAdmin();
  const btn=(l,fn,pri,need)=>{const b=document.createElement('button');b.textContent=l;if(pri)b.className='pri';if(need&&!adm)b.disabled=true;b.onclick=fn;row.appendChild(b);return b};
  if(!RD){t.textContent='The development link is part of the installed Rudolf OS (bootable disc or USB drive).';return}
  if(!s||!s.available){t.textContent='This Rudolf OS has no development link.';return}
  if(s.on){const st=s.status||{};t.textContent='On until '+devlUntil(s)+' ('+devlLeft(s.secondsLeft)+' left): commands from the GitHub repository '+s.repo+' '+(s.change?'can change this computer':'can look at this computer (read-only)')+(s.ask?'; each one waits for you to allow it':', without asking')+(s.restart?', also after a restart (with Ask first on again)':', until the computer restarts')+'. It turns itself off after 4 hours without a command.'+(st.state&&st.state!='on'?' Now: '+st.state+'.':'')+(st.error?' '+st.error+'.':'');
   f.innerHTML="<label style='display:block;margin:6px 0'><input type='checkbox' class='dlc'> Allow changes to this computer</label><p class='msg'>Unticked, its commands can only look: read files and logs, take screenshots, measure. Ticked, they can also install, change settings and restart. Ticking it needs an administrator's password; it is unticked again at every start.</p><p class='msg dlcerr' style='color:#b91c1c'></p>";
   const cb=f.querySelector('.dlc');cb.checked=!!s.change;cb.disabled=!s.change&&(!adm||s.allowed===false);
   cb.onchange=async()=>{const e=f.querySelector('.dlcerr');e.textContent='';
    if(!cb.checked){try{await rdPost('devlink/change?on=0')}catch(x){e.textContent=x.message}devlPoll();draw();return}
    window.__elevTrust=0;const cr=await elevate('Allow the development link to change this computer: commands from '+s.repo+' can then install, change settings and restart it.',{cred:1});
    if(!cr){cb.checked=false;return}
    try{await rd('devlink/change?on=1',{method:'POST',body:JSON.stringify({n:cr.n,p:cr.p})})}catch(x){e.textContent=x.message||'It could not be allowed.';cb.checked=false;return}
    devlPoll();draw()};
   devlAllowRow(f,s,adm,draw);
   btn('End now',async()=>{await devlOff();draw()},1);btn('Log…',devlLog);return}
  t.textContent='Off. When it is on, a developer you choose (for example a Claude session) can run tests on this computer through a private GitHub repository you own: run commands (read-only unless you allow changes), take screenshots and send files; with changes allowed, also bring files and restart it. Use it on a test computer only, never on one with personal files. A red banner shows while it is on.';
  f.innerHTML=`<div class='r2' style='grid-template-columns:auto 1fr;gap:6px 10px;margin:6px 0'><label>Repository</label><input class='dlr' placeholder='owner/name (private)' spellcheck='false' autocomplete='off'>
<label>Token</label><input class='dlk' type='password' spellcheck='false' autocomplete='off'>
<label>For</label><select class='dlh'><option value='1'>1 hour</option><option value='2' selected>2 hours</option><option value='4'>4 hours</option><option value='8'>8 hours</option></select></div>
<label style='display:block'><input type='checkbox' class='dla' checked> Ask first: each command waits for me to allow it (on again after every restart)</label>
<label style='display:block'><input type='checkbox' class='dlch'> Allow changes to this computer (unticked: its commands can only look; unticked again at every start)</label>
<label style='display:block'><input type='checkbox' class='dlb'> Keep on after restarting (until its time is up)</label>
<label style='display:block'><input type='checkbox' class='dlhome'> Let it read files in home folders</label>
<p class='msg'>It turns itself off after 4 hours without a command, and when the computer restarts unless you keep it on. On github.com, make a private repository, then a fine-grained personal access token for that repository only, with the permission <b>Contents: Read and write</b>. Paste it here; it is checked with GitHub, kept where only the system can read it, and never shown again. Revoking it on GitHub ends all access.</p><p class='msg dlerr' style='color:#e66'></p>`;
  const r=f.querySelector('.dlr'),k=f.querySelector('.dlk'),b=f.querySelector('.dlb');r.value=s.repo||'';if(s.hasToken)k.placeholder='kept from last time (paste a new one to replace it)';
  if(!s.persistent){b.disabled=true;b.parentNode.title='Needs the files kept on this drive (RUDOLFDATA)'}else b.checked=false;f.querySelector('.dla').checked=true;f.querySelector('.dlhome').checked=!!s.home;
  const dev=s.allowed!==false;devlAllowRow(f,s,adm,draw);
  btn('Switch on…',async ev=>{const e=f.querySelector('.dlerr');e.textContent='';
   if(!/^[\w.-]+\/[\w.-]+$/.test(r.value.trim())){e.textContent='Write the repository as owner/name.';return}
   if(!k.value.trim()&&!s.hasToken){e.textContent='Paste the token from GitHub.';return}
   if(!dev){e.textContent='Tick "Allow the development link on this computer" first.';return}
   window.__elevTrust=0;const cr=await elevate('Switch the development link on: commands from the GitHub repository '+r.value.trim()+' can '+(f.querySelector('.dlch').checked?'change':'look at')+' this computer for '+f.querySelector('.dlh').value+' hour(s).',{cred:1});if(!cr)return;
   ev.target.disabled=true;e.textContent='Checking the token with GitHub…';
   try{await rd('devlink/on?repo='+encodeURIComponent(r.value.trim())+'&hours='+f.querySelector('.dlh').value+'&ask='+(f.querySelector('.dla').checked?1:0)+'&restart='+(b.checked?1:0)+'&home='+(f.querySelector('.dlhome').checked?1:0)+'&change='+(f.querySelector('.dlch').checked?1:0),{method:'POST',body:JSON.stringify({n:cr.n,p:cr.p,token:k.value.trim()})});k.value='';devlPoll();draw()}
   catch(x){e.textContent=x.message||'It could not be switched on.';ev.target.disabled=false}},1,1).disabled=!adm||!dev;
  if(s.hasToken)btn('Forget the token',async()=>{if(!await elevate('Delete the development link\'s token from this computer.'))return;try{await rdPost('devlink/forget')}catch(e){}draw()},0,1);
  btn('Log…',devlLog);
  if(!adm||!dev){const n=document.createElement('span');n.className='msg';n.textContent=!dev?' Allow the development link on this computer first (above).':' An administrator can switch it on.';row.appendChild(n)}};
 draw()}
if(typeof secPanelS=='function'){const _sp2=secPanelS;secPanelS=function(c,W){const r=_sp2.apply(this,arguments);try{devlSection(c,W)}catch(e){}return r}}
