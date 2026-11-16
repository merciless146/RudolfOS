/* ===== Rudolf OS 1.5.9 (docs/plan-1.5.9.md Part 1): cellular modems in Settings › Network =====
   Each modem ModemManager sees: maker and model, its SIM (missing, locked), the operator, the signal and the network
   type. A hotspot or a phone's tethering connects already.
   1.6.0 (docs/plan-1.6.0.md Part 1): the FCC unlock of laptop modules (their radio starts locked until the computer
   unlocks it: "Turn on this laptop's cellular module", saying what it does, for an administrator, whose name and
   password rudolfd checks), and the modules Linux supports only partly, named with what is known about them. */
const MDM_STATE={failed:'Not working',unknown:'Starting',initializing:'Starting',locked:'SIM locked',disabled:'Off',disabling:'Turning off',enabling:'Turning on',enabled:'On, not registered',searching:'Searching for a network',registered:'On the network',disconnecting:'Disconnecting',connecting:'Connecting',connected:'Connected'};
function mdmLine(m){const st=MDM_STATE[m.state]||m.state||'—';const parts=[st];const v=x=>x&&x!='--';
 if(!m.sim||m.failed=='sim-missing')parts.push('no SIM card');else if(v(m.locked)&&m.locked!='none')parts.push('the SIM needs its '+(m.locked=='sim-puk'?'PUK':'PIN')+' ('+m.locked+')');
 if(v(m.operator))parts.push(m.operator);if(v(m.tech))parts.push(m.tech.toUpperCase().replace(/,/g,', '));if(v(m.signal)&&m.signal!='0'&&m.state!='locked'&&m.sim)parts.push('signal '+m.signal+'%');return parts.join(' · ')}
/* the FCC unlock of one module, on or off (rudolfd checks the administrator) */
async function mdmFcc(m,on,redraw){const what=on?"Turn on this laptop's cellular module ("+m.fccId+"): Rudolf OS will run ModemManager's unlock for it each time it starts":"Turn off the FCC unlock of the cellular module ("+m.fccId+")";
 const cr=await elevate(what,{cred:true});if(!cr)return;
 try{const r=await rd('mobile/fcc?id='+encodeURIComponent(m.fccId)+'&on='+(on?1:0),{method:'POST',body:JSON.stringify({n:cr.n,p:cr.p})});notify('📶 Cellular module',(r&&r.message)||'Done.',{app:'Network'})}
 catch(e){notify('📶 Cellular module',e.message,{kind:'error',app:'Network'})}redraw&&redraw()}
(function(){if(typeof netPanelS!='function')return;const o=netPanelS;netPanelS=function(c,W){o.apply(this,arguments);if(!RD)return;
 const box=document.createElement('div');box.className='mdm59';c.appendChild(box);
 const draw=async()=>{let s=null;try{s=await rd('mobile/status')}catch(e){}if(!box.isConnected)return;
  const ms=(s&&s.modems)||[],ns=(s&&s.notes)||[];if(!ms.length&&!ns.length){box.innerHTML='';return}
  box.innerHTML=(ms.length?`<h4 class='sh4'>Cellular modems</h4><div class='mdml'></div>`:'')+(ns.length?`<h4 class='sh4'>Cellular modules Linux supports only partly</h4><div class='mdmn'></div>`:'');
  const L=box.querySelector('.mdml');ms.forEach(m=>{const r=document.createElement('div');r.className='acr wfrow';r.innerHTML=`<span style='font-size:22px'>📶</span><span style='flex:1'><b></b><div class='msg'></div><div class='mdmfcc'></div></span>`;
   r.querySelector('b').textContent=[m.maker,m.model].filter(x=>x&&x!='--').join(' ')||'Cellular modem';r.querySelector('.msg').textContent=mdmLine(m)+(m.firmware&&m.firmware!='--'?' · firmware '+m.firmware:'')+(m.driver?' · '+m.driver:'');
   const f=r.querySelector('.mdmfcc');
   if(m.fcc=='off'){f.innerHTML=`<div class='msg'><b>If its radio stays off:</b> laptop makers lock some cellular modules until the computer unlocks them (the “FCC lock”). Rudolf OS can run ModemManager's unlock for this module (<span class='mid'></span>) each time it starts, telling it this computer may use its radio. Turn it on only for a module that came with this laptop. An administrator's password is needed.</div><button class='mdmfccon'>Turn on this laptop's cellular module</button>`;
    f.querySelector('.mid').textContent=m.fccId;f.querySelector('button').onclick=()=>mdmFcc(m,1,draw)}
   else if(m.fcc=='on'){f.innerHTML=`<div class='msg'>The radio is unlocked each time the module starts (FCC unlock for <span class='mid'></span>).</div><button class='mdmfccoff'>Turn off the FCC unlock</button>`;
    f.querySelector('.mid').textContent=m.fccId;f.querySelector('button').onclick=()=>mdmFcc(m,0,draw)}
   L.appendChild(r)});
  const N=box.querySelector('.mdmn');ns.forEach(n=>{const r=document.createElement('div');r.className='acr wfrow';r.innerHTML=`<span style='font-size:22px'>⚠️</span><span style='flex:1'><b></b><div class='msg'></div></span>`;
   r.querySelector('b').textContent=n.name+' ('+n.id+')';r.querySelector('.msg').textContent=n.text;N.appendChild(r)})};
 draw();const iv=setInterval(()=>{if(!box.isConnected)return clearInterval(iv);draw()},10000)}})();
