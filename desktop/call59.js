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
