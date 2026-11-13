/* ===== Rudolf OS 1.5.9 (docs/plan-1.5.9.md 7.2): "In a call" =====
   While any app records from a microphone (a call in Oracle's tabs on either engine, Zoom's Linux app, a Windows app),
   the taskbar shows 🎙️ with the apps' names, and one click mutes the microphone for everything (the sound server's
   default source; the microphone key does the same), as phones do. A call holds off the screen saver, the lock and
   sleep, as a video playing does (rudolfd's media answer counts the apps using a microphone too). */
const CALL59={users:[],muted:false,iv:0};
mediaOther=async function(){try{const m=await rd('media');return m.playing>0||m.mic>0}catch(e){return false}};
function call59Draw(){if(!CALL59.users.length){trayRemove('call');return}
 const names=CALL59.users.join(', '),t=CALL59.muted?`Microphone muted (${names}): click to turn it back on`:`In a call: ${names} use${CALL59.users.length==1?'s':''} the microphone. Click to mute it for everything`;
 const b=TRAYX.call&&TRAYX.call.title==t?TRAYX.call:trayAdd('call',{icon:CALL59.muted?'🔇':'🎙️',title:t,onclick:call59Toggle});
 b.classList.toggle('call59',true);b.classList.toggle('muted',CALL59.muted)}
async function call59Poll(){if(!RD||!USER||document.hidden)return;try{const j=await rd('mic');CALL59.users=j.users||[];CALL59.muted=!!j.muted}catch(e){CALL59.users=[]}call59Draw()}
async function call59Toggle(){try{const j=await rdPost('mic?muted='+(CALL59.muted?0:1));CALL59.muted=!!j.muted;CALL59.users=j.users||CALL59.users}catch(e){notify('Microphone',e.message,{kind:'error'})}call59Draw()}
if(RD){CALL59.iv=setInterval(call59Poll,5000);setTimeout(call59Poll,3000)}
/* Settings › Sound › Calls (plan 7.2): the echo canceller for Linux apps without their own (Chromium cancels echo
   itself). Off by default: it costs processor time. A Bluetooth headset's microphone works in calls by itself (the
   headset cancels echo); its call sound is switched to while an app records from it (/usr/lib/rudolf/bluetooth). */
if(typeof volPanelS=='function'&&RD){const _v=volPanelS;volPanelS=function(c,W){const r=_v.apply(this,arguments);try{
 const d=document.createElement('div');d.className='echo59';c.appendChild(d);
 d.innerHTML=`<h4 class='sh4'>Calls</h4><label class='ck'><input type='checkbox' class='ec'> Cancel echo for Linux apps</label><div class='msg ecm'>For calls in Linux apps that have no echo cancelling of their own, with the speakers on. It uses some processor time. Oracle's calls cancel echo themselves; a Bluetooth headset does too.</div>`;
 const cb=d.querySelector('.ec'),m=d.querySelector('.ecm'),base=m.textContent;
 const show=s=>{cb.checked=!!s.on;m.textContent=base+(s.on&&!s.active?' (Not active now: there is no microphone or speaker it can use.)':'')};
 rd('sound/echo').then(show).catch(()=>{cb.disabled=true});
 cb.onchange=()=>{cb.disabled=true;rdPost('sound/echo?on='+(cb.checked?1:0)).then(s=>{show(s);notify('Echo cancelling',s.on?'On':'Off',{quiet:true})}).catch(e=>{cb.checked=!cb.checked;notify('Echo cancelling',e.message,{kind:'error'})}).finally(()=>cb.disabled=false)};
}catch(e){console.warn(e)}return r}}
