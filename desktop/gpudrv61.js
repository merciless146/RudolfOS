/* ===== Rudolf OS 1.6.1 (docs/plan-1.6.1.md Part 19): Settings › Display › Graphics driver =====
   "Open (NVK)" (nouveau and Mesa's NVK, the default) or "NVIDIA's driver" (its open kernel modules and its own OpenGL
   and Vulkan libraries, the signed system extension nvidia-driver). Shown only on a computer with an NVIDIA card; NVIDIA's
   driver offered only for GeForce 16 and newer (rudolfd's gpu/driver says why not). Changing it needs an administrator's
   password (rudolfd checks it) and takes effect at the next start. When NVIDIA's driver could not be used, the computer
   switched back to the open driver by itself: a notification says so once, at the next sign-in. Standard and Native
   alike (the page draws Settings in both). Drawn as text. */
window.__gpudrv61=1;
async function gdStatus(){try{return await rd('gpu/driver')}catch(e){return null}}
function gdNow(s){return s.inUse=='nvidia'?`NVIDIA's driver${s.version?' '+s.version:''}`:'the open driver (NVK)'}
(function(){const o=dispPanelS;dispPanelS=function(c,W){o(c,W);if(!RD)return;
 gdStatus().then(s=>{if(!s||!s.card||!c.isConnected)return;   /* (no NVIDIA card: nothing to choose) */
  const d=sp(c,`<h4 class='sh4'>Graphics driver</h4><div class='r2'><label for='gdsel'>NVIDIA card</label><div><select id='gdsel'>
   <option value='open'>Open (NVK): nouveau and Mesa (recommended)</option><option value='nvidia'>NVIDIA's driver</option></select>
   <div class='msg gdn'></div><div class='msg gdw'></div><details class='gdl' hidden><summary>NVIDIA's licence</summary><div class='msg'>NVIDIA's driver comes with NVIDIA's own licence: Rudolf OS passes its files on unmodified, as its section 1.d allows, and it may be used only on GeForce and Titan cards (its 2.8).</div><pre class='jl' style='max-height:240px;overflow:auto;white-space:pre-wrap'></pre></details><div class='msg'>Changes take effect when Rudolf OS restarts. If NVIDIA's driver cannot be used, Rudolf OS goes back to the open driver by itself and says so. Recovery mode always uses the open driver.</div></div></div>`);
  const sel=d.querySelector('#gdsel'),n=d.querySelector('.gdn'),w=d.querySelector('.gdw'),nv=sel.querySelector('option[value=nvidia]');
  const show=s=>{sel.value=s.choice=='nvidia'?'nvidia':'open';
   n.textContent=`${s.name||s.card}: now using ${gdNow(s)}.`+(s.choice!=s.inUse?` At the next start: ${s.choice=='nvidia'?"NVIDIA's driver":'the open driver'}.`:'');
   nv.disabled=!s.offered&&s.choice!='nvidia';
   w.textContent=!s.offered?`NVIDIA's driver is not offered here: ${s.why}.`:!s.extension.installed?"NVIDIA's driver is a system extension: install “NVIDIA's graphics driver” in Settings › Update and recovery › System extensions first.":s.extension.state&&s.extension.state!='in-use'&&s.extension.state!='next'?`The NVIDIA driver extension is not in use (${s.extension.state}).`:'';
   if(s.fellBack)w.textContent=`Switched back to the open driver on ${new Date(s.fellBack.at).toLocaleString()}: ${s.fellBack.why}.`};
  show(s);
  if(s.extension&&s.extension.installed)rd('gpu/driver/licence').then(r=>r.text()).then(t=>{if(typeof t!='string'||!t)return;const l=d.querySelector('.gdl');l.hidden=false;l.querySelector('pre').textContent=t}).catch(()=>{});
  sel.onchange=async()=>{const to=sel.value;const cr=await elevate(to=='nvidia'?"Use NVIDIA's graphics driver from the next start":'Use the open graphics driver from the next start',{cred:1});
   if(!cr){show(s);return}
   try{s=await rd('gpu/driver/set?to='+to,{method:'POST',body:JSON.stringify({n:cr.n,p:cr.p})});show(s);
    notify('Graphics driver',`${to=='nvidia'?"NVIDIA's driver":'The open driver'} is used from the next start.`,{action:()=>{try{fsFlush()}catch(e){}rdPost('power?action=reboot').catch(()=>{})}})}
   catch(e){notify('Graphics driver',e.message,{kind:'error'});show(s)}}})}})();
/* after a start that switched back by itself: one notification once someone is signed in (rudolfd forgets the note once
   it was shown); by the desktop page on Standard, by the service page on Native (not by every app window) */
const gdMine=()=>!(typeof ONEAPP!='undefined'&&ONEAPP)||(typeof OA_SVC!='undefined'&&OA_SVC);
let GD_NOTED=false;const gdNote=setInterval(async()=>{if(GD_NOTED||!RD||typeof USER=='undefined'||!USER||window.LOCKED)return;if(!gdMine()){clearInterval(gdNote);return}GD_NOTED=true;clearInterval(gdNote);
 const s=await gdStatus();if(!s||!s.fellBack)return;
 notify('Graphics driver',`NVIDIA's driver could not be used (${s.fellBack.why}), so Rudolf OS went back to the open driver. Settings › Display › Graphics driver has the choice.`,{kind:'info',action:()=>{try{settings('disp')}catch(e){}}});
 try{await rdPost('gpu/driver/ack')}catch(e){}},5000);
