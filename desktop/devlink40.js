/* ===== 1.4.0: the development link =====
   Settings › Security › Development link: a developer the owner chooses (a Claude session, a collaborator) runs tests
   on this computer through a private GitHub repository the owner owns (rudolf-devlink; rudolfd: devlink, devlink/on,
   off, forget, answer, log). While it is on: a red banner over everything with the time left and End now, and with
   Ask first every command waits here for Allow or Refuse. See docs/plan-1.4.0.md. */
window.__devlink40=1;
let DEVLST=null,DEVLASKED='';
const devlLeft=s=>{const m=Math.max(0,Math.round((s||0)/60));return m>=60?Math.floor(m/60)+' h '+(m%60)+' min':m+' min'};
const devlUntil=s=>s&&s.until?new Date(s.until*1000).toLocaleTimeString([], {hour:'2-digit',minute:'2-digit'}):'';
function devlSummary(s){if(!s||!s.available)return'not available';if(!s.on)return'off';return'on until '+devlUntil(s)+' ('+s.repo+(s.ask?', asks first':', runs without asking')+')'}
async function devlOff(){try{await rdPost('devlink/off')}catch(e){}devlPoll()}
function devlBanner(s){let b=document.getElementById('devlinkBanner');
 if(!s||!s.on){if(b)b.remove();document.documentElement.style.removeProperty('--dl-top');return}
 if(!b){b=document.createElement('div');b.id='devlinkBanner';b.setAttribute('role','alert');
  b.style.cssText='position:fixed;left:0;right:0;top:0;z-index:2147483647;display:flex;align-items:center;gap:10px;padding:3px 10px;background:#b00020;color:#fff;font:600 13px system-ui,sans-serif;box-shadow:0 1px 4px #0006;pointer-events:auto';
  b.innerHTML="<span class='dlt' style='flex:1;overflow:hidden;white-space:nowrap;text-overflow:ellipsis'></span><button class='dle' style='background:#fff;color:#b00020;border:0;border-radius:4px;padding:2px 10px;font-weight:700;cursor:pointer'>End now</button>";
  b.querySelector('.dle').onclick=devlOff;document.body.appendChild(b)}
 const st=s.status||{};let t='⚠ Development link on: commands from GitHub ('+s.repo+') can control this computer · '+devlLeft(s.secondsLeft)+' left';
 if(st.state&&st.state!='on')t+=' · '+st.state;if(st.error)t+=' · '+st.error;
 b.querySelector('.dlt').textContent=t;b.title=t}
function devlAsk(s){const p=s&&s.on&&s.pending;let o=document.getElementById('devlinkAsk');
 if(!p){if(o)o.remove();DEVLASKED='';return}
 if(DEVLASKED==p.id&&o)return;if(o)o.remove();DEVLASKED=p.id;
 o=document.createElement('div');o.id='devlinkAsk';o.className='rmodal';o.style.zIndex='2147483646';
 o.innerHTML=`<div class='rmbox' role='alertdialog' aria-label='Development link: allow this command?' style='max-width:640px'><h3>🔧 Allow this command?</h3>
<p class='msg dlw'></p><pre class='dlc' style='max-height:45vh;overflow:auto;white-space:pre-wrap;background:#0003;padding:8px;border-radius:4px;font:12px monospace'></pre>
<p class='msg'>It runs with full control of this computer. Refuse anything you did not expect. Not answered in 10 minutes, it is refused.</p>
<div class='rmrow'><button class='dln'>Refuse</button><button class='pri dly'>Allow</button></div></div>`;
 o.querySelector('.dlw').textContent='Command '+p.id+' from '+s.repo+' ('+p.kind+'):';o.querySelector('.dlc').textContent=p.what||'';
 const ans=async a=>{o.remove();try{await rdPost('devlink/answer?id='+encodeURIComponent(p.id)+'&a='+a)}catch(e){}devlPoll()};
 o.querySelector('.dln').onclick=()=>ans('refuse');o.querySelector('.dly').onclick=()=>ans('allow');
 document.body.appendChild(o);o.querySelector('.dln').focus()}
async function devlPoll(){if(!RD)return;try{DEVLST=await rd('devlink')}catch(e){return}devlBanner(DEVLST);devlAsk(DEVLST)}
if(typeof RD!='undefined'&&RD){devlPoll();setInterval(devlPoll,4000)}
async function devlLog(){let l='';try{l=(await rd('devlink/log')).log||''}catch(e){l=e.message}
 const rows=l.trim()?l.trim().split('\n').map(x=>{try{const j=JSON.parse(x);return j.t.replace('T',' ').replace('Z',' UTC')+'  '+j.id+'  '+j.kind+'  '+j.result+'  ('+j.approved+')\n    '+(j.what||'').replace(/\n/g,'\n    ')}catch(e){return x}}):['Nothing has been run yet.'];
 const o=document.createElement('div');o.className='rmodal';o.innerHTML=`<div class='rmbox' role='dialog' aria-label='Development link log' style='max-width:760px'><h3>Development link: log (30 days)</h3><pre style='max-height:60vh;overflow:auto;white-space:pre-wrap;font:12px monospace'></pre><div class='rmrow'><button class='pri'>Close</button></div></div>`;
 o.querySelector('pre').textContent=rows.reverse().join('\n');o.querySelector('button').onclick=()=>o.remove();document.body.appendChild(o)}
async function devlSection(c,W){
 const box=document.createElement('div');box.className='dl40';c.appendChild(box);
 const draw=async()=>{let s=null;try{s=await rd('devlink')}catch(e){}
  box.innerHTML="<h4 class='sh4'>Development link</h4><p class='msg dls'></p><div class='dlf'></div><div class='rmrow' style='justify-content:flex-start'></div>";
  const t=box.querySelector('.dls'),f=box.querySelector('.dlf'),row=box.querySelector('.rmrow'),adm=isAdmin();
  const btn=(l,fn,pri,need)=>{const b=document.createElement('button');b.textContent=l;if(pri)b.className='pri';if(need&&!adm)b.disabled=true;b.onclick=fn;row.appendChild(b);return b};
  if(!RD){t.textContent='The development link is part of the installed Rudolf OS (bootable disc or USB drive).';return}
  if(!s||!s.available){t.textContent='This Rudolf OS has no development link.';return}
  if(s.on){const st=s.status||{};t.textContent='On until '+devlUntil(s)+' ('+devlLeft(s.secondsLeft)+' left): commands from the GitHub repository '+s.repo+' can control this computer'+(s.ask?'; each one waits for you to allow it':', without asking')+(s.restart?', also after a restart':'')+'.'+(st.state&&st.state!='on'?' Now: '+st.state+'.':'')+(st.error?' '+st.error+'.':'');
   btn('End now',async()=>{await devlOff();draw()},1);btn('Log…',devlLog);return}
  t.textContent='Off. When it is on, a developer you choose (for example a Claude session) can run tests on this computer through a private GitHub repository you own: run commands with full control, take screenshots, send and bring files, restart it. Use it on a test computer only, never on one with personal files. A red banner shows while it is on.';
  f.innerHTML=`<div class='r2' style='grid-template-columns:auto 1fr;gap:6px 10px;margin:6px 0'><label>Repository</label><input class='dlr' placeholder='owner/name (private)' spellcheck='false' autocomplete='off'>
<label>Token</label><input class='dlk' type='password' spellcheck='false' autocomplete='off'>
<label>For</label><select class='dlh'><option value='1'>1 hour</option><option value='2' selected>2 hours</option><option value='4'>4 hours</option><option value='8'>8 hours</option></select></div>
<label style='display:block'><input type='checkbox' class='dla' checked> Ask first: each command waits for me to allow it</label>
<label style='display:block'><input type='checkbox' class='dlb'> Stay on across a restart (until its time is up)</label>
<label style='display:block'><input type='checkbox' class='dlhome'> Let it read files in home folders</label>
<p class='msg'>On github.com, make a private repository, then a fine-grained personal access token for that repository only, with the permission <b>Contents: Read and write</b>. Paste it here; it is checked with GitHub, kept where only the system can read it, and never shown again. Revoking it on GitHub ends all access.</p><p class='msg dlerr' style='color:#e66'></p>`;
  const r=f.querySelector('.dlr'),k=f.querySelector('.dlk'),b=f.querySelector('.dlb');r.value=s.repo||'';if(s.hasToken)k.placeholder='kept from last time (paste a new one to replace it)';
  if(!s.persistent){b.disabled=true;b.parentNode.title='Needs the files kept on this drive (RUDOLFDATA)'}else b.checked=!!s.restart;f.querySelector('.dla').checked=s.ask!==false;f.querySelector('.dlhome').checked=!!s.home;
  btn('Switch on…',async ev=>{const e=f.querySelector('.dlerr');e.textContent='';
   if(!/^[\w.-]+\/[\w.-]+$/.test(r.value.trim())){e.textContent='Write the repository as owner/name.';return}
   if(!k.value.trim()&&!s.hasToken){e.textContent='Paste the token from GitHub.';return}
   window.__elevTrust=0;if(!await elevate('Switch the development link on: commands from the GitHub repository '+r.value.trim()+' can control this computer for '+f.querySelector('.dlh').value+' hour(s).'))return;
   ev.target.disabled=true;e.textContent='Checking the token with GitHub…';
   try{await rd('devlink/on?repo='+encodeURIComponent(r.value.trim())+'&hours='+f.querySelector('.dlh').value+'&ask='+(f.querySelector('.dla').checked?1:0)+'&restart='+(b.checked?1:0)+'&home='+(f.querySelector('.dlhome').checked?1:0),{method:'POST',body:k.value.trim()});k.value='';devlPoll();draw()}
   catch(x){e.textContent=x.message||'It could not be switched on.';ev.target.disabled=false}},1,1);
  if(s.hasToken)btn('Forget the token',async()=>{if(!await elevate('Delete the development link\'s token from this computer.'))return;try{await rdPost('devlink/forget')}catch(e){}draw()},0,1);
  btn('Log…',devlLog);
  if(!adm){const n=document.createElement('span');n.className='msg';n.textContent=' An administrator can switch it on.';row.appendChild(n)}};
 draw()}
if(typeof secPanelS=='function'){const _sp2=secPanelS;secPanelS=function(c,W){const r=_sp2.apply(this,arguments);try{devlSection(c,W)}catch(e){}return r}}
