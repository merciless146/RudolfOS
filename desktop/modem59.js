/* ===== Rudolf OS 1.5.9 (docs/plan-1.5.9.md Part 1): cellular modems in Settings › Network (read only) =====
   Each modem ModemManager sees: maker and model, its SIM (missing, locked), the operator, the signal and the network
   type. Connecting comes in 1.6.0 (SIM, APN and metering together); a hotspot or a phone's tethering connects already. */
const MDM_STATE={failed:'Not working',unknown:'Starting',initializing:'Starting',locked:'SIM locked',disabled:'Off',disabling:'Turning off',enabling:'Turning on',enabled:'On, not registered',searching:'Searching for a network',registered:'On the network',disconnecting:'Disconnecting',connecting:'Connecting',connected:'Connected'};
function mdmLine(m){const st=MDM_STATE[m.state]||m.state||'—';const parts=[st];
 if(!m.sim||m.failed=='sim-missing')parts.push('no SIM card');else if(m.locked&&m.locked!='--'&&m.locked!='none')parts.push('the SIM needs its PIN ('+m.locked+')');
 if(m.operator&&m.operator!='--')parts.push(m.operator);if(m.tech&&m.tech!='--')parts.push(m.tech.toUpperCase().replace(/,/g,', '));if(m.signal&&m.signal!='--')parts.push('signal '+m.signal+'%');return parts.join(' · ')}
(function(){if(typeof netPanelS!='function')return;const o=netPanelS;netPanelS=function(c,W){o.apply(this,arguments);if(!RD)return;
 const box=document.createElement('div');box.className='mdm59';c.appendChild(box);
 const draw=async()=>{let s=null;try{s=await rd('mobile/status')}catch(e){}if(!box.isConnected)return;
  if(!s||!(s.modems||[]).length){box.innerHTML='';return}
  box.innerHTML=`<h4 class='sh4'>Cellular modems</h4><div class='mdml'></div><p class='msg'>Connecting with a SIM card comes in Rudolf OS 1.6.0. A hotspot, or a phone sharing its connection by USB, works now.</p>`;
  const L=box.querySelector('.mdml');s.modems.forEach(m=>{const r=document.createElement('div');r.className='acr wfrow';r.innerHTML=`<span style='font-size:22px'>📶</span><span style='flex:1'><b></b><div class='msg'></div></span>`;
   r.querySelector('b').textContent=[m.maker,m.model].filter(x=>x&&x!='--').join(' ')||'Cellular modem';r.querySelector('.msg').textContent=mdmLine(m)+(m.firmware&&m.firmware!='--'?' · firmware '+m.firmware:'')+(m.driver?' · '+m.driver:'');L.appendChild(r)})};
 draw();const iv=setInterval(()=>{if(!box.isConnected)return clearInterval(iv);draw()},10000)}})();
