/* ===== Rudolf OS 1.5.9 (docs/plan-1.5.9.md Part 4): Settings › Network › Share this connection =====
   The computer's Wi-Fi as a hotspot for other devices: a name, a password (8 to 63 characters, kept by the system for
   next time) and the band; off by default, administrators only (their password each time), off again at a restart.
   When the Wi-Fi chip cannot share the Wi-Fi it is connected to, it says so: a cable, a phone's USB tethering or a modem
   is shared instead. rudolfd net/hotspot runs /usr/lib/rudolf/hotspot. */
function hsRandPw(){const a='abcdefghjkmnpqrstuvwxyz23456789';const r=crypto.getRandomValues(new Uint32Array(12));return [...r].map(x=>a[x%a.length]).join('')}
(function(){if(typeof netPanelS!='function')return;const o=netPanelS;netPanelS=function(c,W){o.apply(this,arguments);if(!RD)return;
 const box=document.createElement('div');box.className='hs59';c.appendChild(box);let last='';
 const draw=async(force)=>{let s=null;try{s=await rd('net/hotspot')}catch(e){}if(!box.isConnected)return;if(!s){box.innerHTML='';return}
  const k=JSON.stringify(s);if(k==last&&!force)return;last=k;const adm=isAdmin();
  box.innerHTML=`<h4 class='sh4'>Share this connection</h4><div class='sysucard ${s.on?'ok':''}'><div class='hsl'></div></div>
<div class='r2 hsf'><label>Network name</label><input class='hsn' maxlength='32'><label>Password</label><div style='display:flex;gap:6px'><input class='hsp' maxlength='63' autocomplete='off' spellcheck='false' style='flex:1'><button class='hsg' title='A new random password'>🎲</button></div><label>Band</label><select class='hsb'><option value='2.4'>2.4 GHz (reaches further, every device)</option><option value='5'>5 GHz (faster)</option></select></div>
<div class='rmrow' style='justify-content:flex-start'><button class='hst pri'></button></div>${adm?'':"<div class='msg'>Only an administrator can share this computer's connection.</div>"}`;
  const q=x=>box.querySelector(x),L=q('.hsl');
  if(s.on)L.innerHTML=`<b>On</b> · <span class='hsnm'></span> · ${s.devices} device${s.devices==1?'':'s'} connected · sharing ${pkgEsc(s.uplink)}${s.band?' · '+pkgEsc(s.band)+' GHz':''}`,q('.hsnm').textContent=s.name;
  else L.innerHTML=s.why?`<b>Off</b> · <span class='msg'></span>`:'<b>Off</b> · other devices can use this computer’s internet connection through its Wi-Fi.';
  if(!s.on&&s.why)L.querySelector('.msg').textContent=s.why;
  q('.hsn').value=s.name||('Rudolf-'+String(Math.random()).slice(2,6));q('.hsb').value=s.band=='5'?'5':'2.4';
  q('.hsp').placeholder=s.saved?'(the saved password)':'8 to 63 characters';if(!s.saved)q('.hsp').value=hsRandPw();
  q('.hsf').hidden=s.on;q('.hsg').onclick=()=>{q('.hsp').value=hsRandPw()};
  const t=q('.hst');t.textContent=s.on?'Turn off':'Turn on';t.disabled=!adm||(!s.on&&!!s.why);
  t.onclick=async()=>{window.__elevTrust=0;
   if(s.on){if(!await elevate('Stop sharing this computer’s connection'))return;try{const m=await rdPost('net/hotspot?on=0');notify('Share this connection',m.message)}catch(e){notify('Share this connection',e.message,{kind:'error'})}draw(true);return}
   const n=q('.hsn').value.trim(),p=q('.hsp').value,b=q('.hsb').value;
   if(!n||n.length>32)return notify('Share this connection','The network name has 1 to 32 characters.',{kind:'error'});
   if(p&&(p.length<8||p.length>63))return notify('Share this connection','The password has 8 to 63 characters.',{kind:'error'});
   if(!p&&!s.saved)return notify('Share this connection','Choose a password (8 to 63 characters).',{kind:'error'});
   if(!await elevate('Share this computer’s connection as the Wi-Fi network '+n))return;t.disabled=true;t.textContent='Starting…';
   try{const m=await rdPost('net/hotspot',n+'\n'+p+'\n'+b);notify('Share this connection',m.message)}catch(e){notify('Share this connection',e.message,{kind:'error'})}draw(true)}};
 draw(true);const iv=setInterval(()=>{if(!box.isConnected)return clearInterval(iv);draw()},5000)}})();
