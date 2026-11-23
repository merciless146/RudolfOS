/* ===== Rudolf OS 1.6.2 (docs/plan-1.6.2.md Part 33): a VPN for the whole computer =====
   Settings › Network › VPN: a WireGuard file (.conf) imported, named, connected and disconnected; the taskbar's network
   pop-up lists the VPNs under the adapters, each with its state, and turns one on or off with a click. While one is on,
   every app's traffic and its name lookups go through it (rudolfd's vpn requests: rudolfd-vpn.inc and the VPN helper,
   rudolf-vpn.c, which sets WireGuard up through the kernel's netlink itself). The page never sees a key: the file goes
   to rudolfd with an administrator's password and is kept where only root reads it; GET vpn never gives it back.
   Adding, renaming and removing need an administrator's password (checked by rudolfd); any account connects and
   disconnects (the native desktop's pop-up, drawn by rudolf-native.c, does the same).
   Native's Settings is this page (its app windows), so Settings looks and works the same on Native, Chromium and Lite. */
const VPN62={s:null,busy:null,t:0};
async function vpnGet(){try{VPN62.s=await rd('vpn')}catch(e){VPN62.s=null}dispatchEvent(new Event('rudolf-vpn'));return VPN62.s}
function vpnState(v){if(VPN62.busy==v.id)return v.on?'Turning off…':'Connecting…';return !v.on?'Off':v.state=='connected'?'Connected':v.state=='noanswer'?'No answer':'Connecting…'}
function vpnAsk(title,text,yes){return new Promise(res=>{const o=document.createElement('div');o.className='rmodal';
 o.innerHTML=`<div class='rmbox' role='alertdialog' aria-modal='true' aria-labelledby='vpnat'><h3 id='vpnat'></h3><p class='vpnatx'></p><div class='rmrow'><button class='n'>Cancel</button><button class='y pri'></button></div></div>`;
 o.querySelector('h3').textContent=title;o.querySelector('.vpnatx').textContent=text;o.querySelector('.y').textContent=yes;document.body.appendChild(o);
 const done=v=>{o.remove();res(v)};o.querySelector('.n').onclick=()=>done(false);o.querySelector('.y').onclick=()=>done(true);
 o.addEventListener('keydown',e=>{if(e.key=='Escape')done(false)});setTimeout(()=>o.querySelector('.y').focus(),30)})}
/* on or off (Settings' buttons and the pop-up's rows) */
async function vpnToggle(v){if(VPN62.busy)return;const T='VPN',path=v.on?'vpn/off':'vpn/on?id='+encodeURIComponent(v.id);
 VPN62.busy=v.id;dispatchEvent(new Event('rudolf-vpn'));
 try{const r=await rdPost(path);notify(T,v.on?v.name+' is off.':r.message||v.name+' is on.')}catch(e){notify(T,e.message,{kind:'error'})}
 VPN62.busy=null;await vpnGet()}
/* the add (import) dialog: the file, or its text pasted, and a name; an administrator's password, checked by rudolfd */
function vpnAdd(after){const o=document.createElement('div');o.className='rmodal';
 o.innerHTML=`<div class='rmbox' role='dialog' aria-modal='true' aria-labelledby='vpnadt' style='max-width:520px'><h3 id='vpnadt'>Add a VPN</h3><p class='msg'>A WireGuard file (.conf) from your VPN provider or your home router. Its private key is kept where only the system reads it; Rudolf OS never shows it again.</p>
<div class='r2'><label>File</label><div><input type='file' class='vpnf' accept='.conf,text/plain' aria-label='WireGuard file'></div><label>Name</label><input class='vpnn' maxlength='64' placeholder='Home, Work, ...' aria-label='Name'></div>
<details class='vpnpd'><summary>Or paste the file's text</summary><textarea class='vpnt' rows='8' spellcheck='false' autocomplete='off' style='width:100%;font:12px monospace' aria-label='The file'></textarea></details>
<p class='msg vpnm' role='alert'></p><div class='rmrow'><button class='n'>Cancel</button><button class='y pri'>Add</button></div></div>`;
 document.body.appendChild(o);const q=x=>o.querySelector(x),say=t=>{q('.vpnm').textContent=t};let text='';
 q('.vpnf').onchange=async e=>{const f=e.target.files[0];if(!f)return;if(f.size>65536){say('The file is over 64 KB: it is not a WireGuard file.');return}
  text=await f.text();if(!q('.vpnn').value.trim())q('.vpnn').value=f.name.replace(/\.conf$/i,'').slice(0,64);say('')};
 q('.n').onclick=()=>{text='';o.remove()};o.addEventListener('keydown',e=>{if(e.key=='Escape'){text='';o.remove()}});
 q('.y').onclick=async()=>{const name=q('.vpnn').value.trim(),conf=q('.vpnt').value.trim()?q('.vpnt').value:text;
  if(!conf)return say('Choose the WireGuard file, or paste its text.');if(!name)return say('Give it a name.');
  if(!/^\s*\[Interface\]/im.test(conf))return say('This is not a WireGuard file: it has no [Interface].');
  const cr=await elevate('Add the VPN '+name,{cred:true});if(!cr)return;q('.y').disabled=true;say('Adding…');
  try{const r=await rdPost('vpn/add',JSON.stringify({n:cr.n,p:cr.p,name,conf,type:'wireguard'}));text='';q('.vpnt').value='';o.remove();
   notify('VPN',name+' was added.'+((r.notes||[]).length?' '+r.notes.join(' '):''));if(after)after()}
  catch(e){q('.y').disabled=false;say(e.message)}}}
/* an administrator's change (the name) or the VPN removed */
async function vpnAdmin(v,what,val){const T='VPN';const say={name:'Rename the VPN '+v.name,remove:'Remove the VPN '+v.name}[what];
 if(what=='remove'&&!await vpnAsk('Remove '+v.name+'?','Its file and its key are removed from this computer'+(v.on?', and it is turned off':'')+'. You can add it again from its file.','Remove'))return false;
 const cr=await elevate(say,{cred:true});if(!cr)return false;const b={n:cr.n,p:cr.p,id:v.id};if(what!='remove')b[what]=val;
 try{await rdPost(what=='remove'?'vpn/remove':'vpn/set',JSON.stringify(b));notify(T,what=='remove'?v.name+' was removed.':'Saved.');return true}catch(e){notify(T,e.message,{kind:'error'});return false}}
/* Settings › Network › VPN */
(function(){if(typeof netPanelS!='function')return;netPanelS=__after(netPanelS,function(c,W){if(!RD||c.dataset.oracle)return;
 const box=document.createElement('div');box.className='vpn62';c.appendChild(box);let last='';
 const draw=async(force,cached)=>{const s=cached?VPN62.s:await vpnGet();if(!box.isConnected)return;if(!s||!s.vpns){box.innerHTML='';return}
  const k=JSON.stringify([s,VPN62.busy]);if(k==last&&!force)return;last=k;const adm=!!(s.me&&s.me.admin)||isAdmin();
  box.innerHTML=`<h4 class='sh4'>VPN</h4><div class='msg vpnwhy'>A VPN for the whole computer: while one is on, every app's traffic and its name lookups go through it.</div><div class='vpnl'></div>
<div class='rmrow' style='justify-content:flex-start'><button class='vpna'>Add a VPN…</button></div>${adm?'':"<div class='msg'>Only an administrator can add, rename or remove a VPN.</div>"}${s.wireguard===false?"<div class='msg'>This system's kernel has no WireGuard: update Rudolf OS.</div>":''}`;
  const L=box.querySelector('.vpnl');
  if(!s.vpns.length)L.innerHTML="<div class='msg'>No VPN yet. Add one from your VPN provider's or your home router's WireGuard file.</div>";
  s.vpns.forEach(v=>{const d=document.createElement('div');d.className='sysucard vpnc'+(v.on&&v.state=='connected'?' ok':'');d.dataset.id=v.id;
   d.innerHTML=`<div style='display:flex;gap:8px;align-items:center;flex-wrap:wrap'><b class='vpnnm' style='flex:1;min-width:120px'></b><span class='vpnst'></span><button class='vpnb pri'></button></div><div class='msg vpnin'></div><div class='msg vpnby'></div>
<div class='vpnad' style='display:flex;gap:6px;align-items:center;flex-wrap:wrap;margin-top:6px'><button class='vpnr'>Rename…</button><button class='vpnx'>Remove…</button></div>`;
   const q=x=>d.querySelector(x);q('.vpnnm').textContent=v.name;q('.vpnst').textContent=vpnState(v);
   q('.vpnin').textContent=v.broken?'Its file could not be read: remove it and add it again.':[v.server?'Server '+v.server:'',v.addresses&&v.addresses.length?'address '+v.addresses.join(', '):'',v.dns&&v.dns.length?'name lookups through '+v.dns.join(', '):"name lookups: the network's own (the file names no DNS server)",v.whole===false?'only its own networks go through it':''].filter(Boolean).join(' · ');
   q('.vpnby').textContent=v.on?'On for everyone on this computer'+(v.by?' (turned on by '+v.by+')':'')+'.':'';
   const b=q('.vpnb');b.textContent=v.on?'Disconnect':'Connect';b.disabled=!!VPN62.busy||v.broken;
   b.onclick=async()=>{await vpnToggle(v);draw(true)};
   q('.vpnr').onclick=async()=>{const n=await askText('A new name for '+v.name);if(n==null||!n.trim())return;if(n.trim().length>64)return notify('VPN','The name has 1 to 64 characters.',{kind:'error'});await vpnAdmin(v,'name',n.trim());draw(true)};
   q('.vpnx').onclick=async()=>{await vpnAdmin(v,'remove');draw(true)};
   q('.vpnad').hidden=!adm;L.appendChild(d)});
  const a=box.querySelector('.vpna');a.disabled=!adm||s.wireguard===false;a.onclick=()=>vpnAdd(()=>draw(true))};
 draw(true);const on=()=>{if(box.isConnected)draw(0,1);else removeEventListener('rudolf-vpn',on)};
 const iv=setInterval(()=>{if(!box.isConnected)return clearInterval(iv);vpnGet()},5000);setTimeout(()=>addEventListener('rudolf-vpn',on),0)},"vpn62.js");})();
/* the taskbar's network pop-up: the VPNs after the adapters, as rows of the same list (rudolf-native.c fl_net draws the same) */
function vpnRows(p){const L=p.querySelector('.npl'),s=VPN62.s;if(!L||!s||!s.vpns)return;
 const act=s.vpns.find(v=>v.on);const st=p.querySelector('.nps');if(act&&st&&!/VPN/.test(st.textContent))st.textContent+=' · VPN: '+act.name;
 s.vpns.forEach(v=>{const r=document.createElement('div');r.className='npr npvpn'+(v.on?' on':'');r.dataset.vpn=v.id;r.innerHTML='<span></span><b></b><i></i>';
  r.children[0].textContent='🔒';r.children[1].textContent='VPN · '+v.name;r.children[2].textContent=vpnState(v);
  r.title=v.on?'Click to turn it off':'Click to connect';r.setAttribute('role','button');r.tabIndex=0;
  const go=()=>vpnToggle(v);
  r.onclick=go;r.onkeydown=e=>{if(e.key=='Enter'||e.key==' '){e.preventDefault();go()}};L.appendChild(r)})}
if(typeof net31Fill=='function'){net31Fill=__after(net31Fill,function(p){vpnRows(p)},'vpn62.js');
 addEventListener('rudolf-vpn',()=>{const p=document.querySelector('.np31');if(p)net31Fill(p)})}
if(RD){if(typeof net31Poll=='function'){const _p=net31Poll;net31Poll=async function(){const r=await _p.apply(this,arguments);await vpnGet();return r}}
 evOn(t=>{if(/^vpn changed/m.test(t))vpnGet()})}
