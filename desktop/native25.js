/* ===== V25 native Linux apps: Roblox through Sober (Flatpak, from Flathub) =====
   rudolfd does the work: native/status, native/install|update|uninstall (a job), native/run|show|stop|log.
   rudolf-display shows the app full screen; Super+Esc returns to the desktop. */
const NATIVE=[{id:'org.vinegarhq.Sober',k:'roblox',name:'Roblox (Sober)',short:'Roblox',icon:'🎮',
 desc:'Play Roblox with Sober, the unofficial Roblox player for Linux made by the Vinegar team. Downloaded from Flathub: about 1 GB with the runtime it needs.'}];
const NATK='bd95native';
const nativeCache=()=>ld(NATK,{});
function nativeSetCache(id,inst){const c=nativeCache();if(!!c[id]===inst)return;c[id]=inst;svj(NATK,c);nativeRegister()}
function nativeRegister(){const c=nativeCache();NATIVE.forEach(a=>{const k='native:'+a.k;if(c[a.id]&&!APPS[k])regApp(k,{name:a.short,icon:a.icon,kw:'roblox game games sober linux play',open:()=>nativeOpen(a),re:new RegExp('^'+a.short+'$')});else if(!c[a.id]&&APPS[k])delete APPS[k]});try{startMenuApps()}catch(e){}}
async function nativeOpen(a){if(!RD){notify(a.name,'Roblox runs only when Rudolf OS is started from its disc, USB drive or hard disk.',{kind:'error'});return}
 try{const s=await rd('native/status?id='+a.id);if(!s.installed){nativeSetCache(a.id,false);notify(a.name,'It is not installed. Get it from the App Store.',{action:()=>launch('store')});return}
  if(s.running){await rdPost('native/show?id='+a.id);return}
  await rdPost('native/run?id='+a.id);notify('Starting '+a.short+'…','It opens full screen in a moment. Press Super+Esc to come back to the desktop.')}catch(e){notify(a.name,e.message,{kind:'error'})}}
function nativeConsent(a){return new Promise(res=>{const o=document.createElement('div');o.className='natdlg';
 o.innerHTML=`<div class='natbox' role='dialog' aria-modal='true' aria-labelledby='natt'><h3 id='natt'></h3><ul>
 <li><b>Unofficial.</b> Sober is made by the Vinegar team, not by Roblox Corporation or by Rudolf OS. Roblox does not support it and may change or block it at any time.</li>
 <li><b>Your Roblox account.</b> Playing through an unofficial player is at your own risk. Never type your Roblox password anywhere except the Roblox sign-in screen inside the game.</li>
 <li><b>Download.</b> About 1 GB from Flathub, over Wi-Fi or a network cable. If the connection drops, the download continues when you press Get again.</li>
 <li><b>Speed.</b> Roblox runs on the graphics card when Rudolf OS can use it (Settings → Display → Graphics card). On small computers such as an Intel N100, lower the graphics quality in Roblox’s settings.</li>
 <li><b>Playing.</b> Roblox opens full screen. Press <kbd>Super</kbd>+<kbd>Esc</kbd> to come back to the desktop.</li></ul>
 <label class='natok'><input type='checkbox'> I understand</label><div class='stbtn' style='justify-content:flex-end'><button class='c'>Cancel</button><button class='pri g' disabled>Get</button></div></div>`;
 o.querySelector('h3').textContent='Get '+a.name+'?';const cb=o.querySelector('input'),g=o.querySelector('.g');cb.onchange=()=>g.disabled=!cb.checked;
 const done=v=>{o.remove();res(v)};o.querySelector('.c').onclick=()=>done(false);g.onclick=()=>done(true);o.onkeydown=e=>{if(e.key=='Escape')done(false)};document.body.appendChild(o);cb.focus()})}
function nativeCard(B,a,instOnly){const d=document.createElement('div');d.className='stc natc';
 d.innerHTML=`<div class='sti'></div><div style='flex:1;min-width:0'><b></b> <span class='natbadge'>Linux app</span><div class='std'></div><div class='natst'></div><pre class='natlog' hidden></pre></div><div class='stbtn'></div>`;
 d.querySelector('.sti').textContent=a.icon;d.querySelector('b').textContent=a.name;d.querySelector('.std').textContent=a.desc;B.appendChild(d);
 const st=d.querySelector('.natst'),bx=d.querySelector('.stbtn'),lg=d.querySelector('.natlog');
 const btn=(l,f,pri)=>{const b=document.createElement('button');b.textContent=l;if(pri)b.className='pri';b.onclick=f;bx.appendChild(b)};
 let last='',busy=0;
 async function refresh(force){if(!d.isConnected)return;
  if(!RD){bx.innerHTML='';if(instOnly)return d.remove();st.textContent='Available when Rudolf OS runs from its disc, USB drive or hard disk.';btn('About',()=>helpApp('roblox'));return}
  let s;try{s=await rd('native/status?id='+a.id)}catch(e){st.textContent=e.message;return}
  const key=JSON.stringify(s);if(key==last&&!force)return;last=key;bx.innerHTML='';
  if(!s.flatpak){if(instOnly)return d.remove();st.textContent='This Rudolf OS build cannot run Linux apps.';return}
  nativeSetCache(a.id,!!s.installed);
  if(instOnly&&!s.installed)return d.remove();
  if(s.installed){const ver=(s.info||'').split('\n')[0];st.textContent='Installed'+(ver?', version '+ver:'')+(s.running?'. Running now.':'.');
   if(s.running){btn('Back to '+a.short,()=>nativeOpen(a),1);btn('Close',async()=>{try{await rdPost('native/stop?id='+a.id)}catch(e){notify(a.name,e.message)}setTimeout(()=>refresh(1),1000)})}
   else btn('Play',()=>nativeOpen(a),1);
   btn('Update',()=>job('update'));btn('Remove',async()=>{if(await elevate('Remove '+a.name+' from this computer'))job('uninstall')});
   btn('Details',async()=>{try{const r=await rdPost('native/log?id='+a.id);lg.hidden=!lg.hidden;lg.textContent=r.log||'Nothing logged yet.'}catch(e){notify(a.name,e.message)}})}
  else{let why='';if(!s.persistent)why='Install Rudolf OS on this computer first (Settings → Update and recovery): apps need a place to be kept.';
   else if(s.free&&s.free<2.5e9)why='Needs about 2.5 GB of free space; '+(s.free/1e9).toFixed(1)+' GB is free.';
   st.textContent=why||'Not installed. Needs an internet connection (about 1 GB to download).';
   if(!why)btn('Get',async()=>{if(await nativeConsent(a)&&await elevate('Install '+a.name+' from Flathub'))job('install')},1);btn('About',()=>helpApp('roblox'))}}
 async function job(what){try{await rdPost('native/'+what+'?id='+a.id)}catch(e){notify(a.name,e.message,{kind:'error'});return}
  busy=1;bx.innerHTML='';lg.hidden=false;lg.textContent='';st.textContent={install:'Downloading and installing. This can take a while…',update:'Updating…',uninstall:'Removing…'}[what];
  const iv=setInterval(async()=>{let j;try{j=await rd('job')}catch(e){return}lg.textContent=(j.log||'').slice(-6000);lg.scrollTop=lg.scrollHeight;
   if(!j.running&&j.status>=0){clearInterval(iv);const ok=j.status==0;
    notify(a.name,ok?{install:'Installed. It is in the Start menu as '+a.short+'.',update:'Up to date.',uninstall:'Removed.'}[what]:'That did not finish. The App Store shows what went wrong; press Get to continue the download.',{kind:ok?'info':'error',action:ok&&what=='install'?()=>nativeOpen(a):undefined});
    if(ok)lg.hidden=true;busy=0;refresh(1)}},1200)}
 refresh();const tick=setInterval(()=>{if(!d.isConnected)return clearInterval(tick);if(!busy&&!document.hidden)refresh()},4000);return d}
function nativeStoreCards(B,instOnly){NATIVE.forEach(a=>nativeCard(B,a,instOnly))}
/* Start menu entries for installed Linux apps */
{const _sma=startMenuApps;startMenuApps=function(){_sma();const ul=$('#menu ul');if(!ul)return;ul.querySelectorAll('li.natv').forEach(x=>x.remove());
 const before=ul.querySelector('li[data-a=settings]')||ul.querySelector('li[data-a=run]');
 NATIVE.filter(a=>APPS['native:'+a.k]).forEach(a=>{const k='native:'+a.k,li=document.createElement('li');li.className='natv';li.dataset.v25=k;li.textContent=a.icon+' '+a.short;li.tabIndex=0;li.onclick=e=>{e.stopPropagation();$('#menu').style.display='none';launch(k)};ul.insertBefore(li,before)})}}
nativeRegister();
if(RD)setTimeout(()=>NATIVE.forEach(a=>rd('native/status?id='+a.id).then(s=>{if(s.flatpak)nativeSetCache(a.id,!!s.installed)}).catch(()=>{})),5000);
/* Help */
HELP.splice(HELP.findIndex(h=>h[0]=='apps')+1,0,['roblox','Roblox (Sober)',()=>`<p>Rudolf OS can run Roblox through <b>Sober</b>, an unofficial Roblox player for Linux made by the Vinegar team. Sober is not made or supported by Roblox Corporation; Roblox can change or block it at any time, and you play at your own risk.</p>
<h4>What you need</h4><ul><li>Rudolf OS installed on the computer (<button data-set='upd'>Settings → Update and recovery</button>), or a USB drive with a RUDOLFDATA partition. Apps are not kept when you run from the disc alone.</li><li>A 64-bit Intel or AMD processor from about 2008 or later (SSE4.1), and 4 GB of memory or more.</li><li>An internet connection (Wi-Fi or a network cable) and about 2.5 GB of free space. Over Wi-Fi the App Store says how much it will download before it starts.</li></ul>
<h4>Get and play</h4><ol><li>Open the <button data-app='store'>🛍️ App Store</button>, find <b>Roblox (Sober)</b> and click <b>Get</b>. The download is about 1 GB.</li><li>When it is installed, open <b>Roblox</b> from the Start menu or click <b>Play</b> in the App Store.</li><li>Sign in on the Roblox screen inside the game.</li></ol>
<h4>While playing</h4><ul><li><kbd>Super</kbd>+<kbd>Esc</kbd> goes back to the Rudolf OS desktop. The game keeps running; open Roblox from the Start menu again to return to it.</li><li>To quit, leave the game from its own menu, or click <b>Close</b> in the App Store.</li></ul>
<h4>Graphics and sound</h4><ul><li>Roblox runs on the graphics card through Xorg when <button data-set='disp'>Settings → Display → Graphics card</button> allows it. If the graphics card cannot be used, Rudolf OS uses the software display, where games are slow.</li><li>On a small computer such as the Intel N100 in the Rudolf Cyberdeck, set Roblox’s graphics quality low (Esc → Settings in the game). Shift+F5 in a game shows its frame rate.</li><li>Game sound plays through the Rudolf OS sound server; choose the output in <button data-set='vol'>Settings → Sound</button>.</li><li>Wi-Fi is not supported yet.</li></ul>
<h4>Updates and removal</h4><p>Roblox updates often; if it stops starting, click <b>Update</b> in the App Store. <b>Remove</b> deletes Sober and its runtime. <b>Details</b> shows the app's messages, which help when something goes wrong.</p>`]);
/* Settings > Display: graphics path (Xorg on the graphics card or the software display); Cyberdeck test mode is in About › For developers (1.4.4) */
(function(){const o=dispPanelS;dispPanelS=function(c,W){o(c,W);if(!RD)return;
 const d=sp(c,`<h4 class='sh4'>Graphics</h4><div class='r2'><label for='dgp'>Graphics card</label><div><select id='dgp'>
  <option value='games'>Use it for games and Linux apps (recommended)</option><option value='full'>Use it for games and the desktop</option><option value='soft'>Do not use it (software display)</option></select>
  <div class='msg dgs'>Checking…</div><div class='msg'>Changes take effect when Rudolf OS restarts. If the graphics card cannot be used, Rudolf OS falls back to the software display by itself.</div></div></div>`);
 const sel=d.querySelector('#dgp'),st=d.querySelector('.dgs');
 const names={gpu:'graphics card (Xorg)',soft:'software display'};
 rd('display').then(r=>{sel.value=r.setting||'games';let t='Now using: '+(names[r.active]||r.active)+(r.gpu?' · driver '+r.gpu:'');if(r.reason)t+=' · '+r.reason;if(!r.xorg)t='This Rudolf OS build has no Xorg; the software display is used.';st.textContent=t;if(r.active=='gpu'){const row=[...c.querySelectorAll('.r2')].find(x=>x.firstElementChild&&x.firstElementChild.textContent.trim()=='Resolution');if(row&&row.lastElementChild)row.lastElementChild.textContent=screen.width*devicePixelRatio+'×'+screen.height*devicePixelRatio+' · the screen’s own resolution, set by the graphics card driver'}}).catch(e=>st.textContent=e.message);
 sel.onchange=()=>rdPost('display?mode='+sel.value).then(r=>{st.textContent=r.message;notify('Graphics',r.message,{action:()=>{try{fsFlush()}catch(e){}rdPost('power?action=reboot').catch(()=>{})}})}).catch(e=>notify('Graphics',e.message,{kind:'error'}));
 }})();
function devCyberdeck(host){const d=document.createElement('div');d.innerHTML=`<div class='r2'><label>Cyberdeck test</label><div>${ckH('ddt','Make this PC behave roughly like the Rudolf Cyberdeck (Intel N100)',false)}
  <div class='msg ddm'>Limits the processor to 4 cores without hyper-threading at about 2.4 GHz. It cannot slow down the graphics card, so game frame rates must still be measured on the cyberdeck itself.</div></div></div>`;host.appendChild(d);
 const dt=d.querySelector('#ddt'),dm=d.querySelector('.ddm');
 rd('decktest').then(r=>{if(!r.deck)return;dt.checked=!!r.deck.on;dm.textContent+=' This PC: '+r.deck.cpus+' logical processors, '+Math.round(r.deck.memkb/1048576)+' GB of memory'+(r.deck.on?'; now limited to processors '+r.deck.online+' at '+(r.deck.maxkhz/1e6).toFixed(1)+' GHz.':'.')}).catch(()=>{});
 dt.onchange=()=>rdPost('decktest?on='+(dt.checked?1:0)).then(r=>notify('Cyberdeck test',r.message)).catch(e=>{dt.checked=!dt.checked;notify('Cyberdeck test',e.message,{kind:'error'})})}
/* Keep open windows inside the free desktop area when the display scale or the screen size changes
   (a live scaling preview would otherwise push the Settings window and its Apply button under the taskbar) */
window.fitWins=function(){const D=$('#desk'),B=$('#bar');if(!D)return;const avail=B&&B.offsetTop>D.offsetTop+100?B.offsetTop-D.offsetTop:D.clientHeight,aw=D.clientWidth;
 wins.forEach(W=>{const e=W.el;if(!e||e.classList.contains('max')||!e.offsetWidth)return;let w=e.offsetWidth,h=e.offsetHeight;
  if(e.dataset.fitW){const pw=+e.dataset.fitW,ph=+e.dataset.fitH;w=Math.min(pw,aw-8);h=Math.min(ph,avail-8);e.style.width=w+'px';e.style.height=h+'px';if(w==pw&&h==ph){delete e.dataset.fitW;delete e.dataset.fitH}}
  if(w>aw-8||h>avail-8){if(!e.dataset.fitW){e.dataset.fitW=w;e.dataset.fitH=h}w=Math.min(w,aw-8);h=Math.min(h,avail-8);e.style.width=w+'px';e.style.height=h+'px'}
  e.style.left=Math.max(0,Math.min(e.offsetLeft,aw-w))+'px';e.style.top=Math.max(0,Math.min(e.offsetTop,avail-h-4))+'px'})};
addEventListener('resize',()=>setTimeout(fitWins,60));
{const _as=applySet;applySet=function(...a){const r=_as.apply(this,a);setTimeout(()=>{try{fitWins()}catch(e){}},80);return r}}
/* Settings > Display controls whose keys Apply did not track: display scaling ('scale', also kept by
   Accessibility), icon style and wallpaper-matched icons. Without this, Apply stayed disabled for them and
   the change was lost at the next start. */
Object.assign(DEFS.disp,{icstyle:DEFS.disp.icstyle??'rudolf',icdyn:DEFS.disp.icdyn??false});
['scale','icstyle','icdyn'].forEach(k=>{if(!DKEYS.includes(k))DKEYS.push(k);if(S[k]===undefined)S[k]=k=='scale'?(DEFS.acc.scale??100):DEFS.disp[k];if(SC&&SC[k]===undefined)SC[k]=cln(S[k])});
