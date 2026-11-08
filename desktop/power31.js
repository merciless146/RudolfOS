/* ===== 1.4.4 (power31): the battery on the taskbar, and Power and battery, in System health since 1.5.6 (installed Rudolf OS:
   /usr/lib/rudolf/power through rudolfd's power/status and power/set; it applies the settings itself, at every start
   and when the charger is plugged in or out, signed in or not). ===== */
/* the computer's own batteries: type Battery, not a device's (a mouse, a pen: scope Device), a level of 0-100 (the
   Vivobook also lists a battery at -1 %) */
const batOwn=l=>(l||[]).filter(b=>/^battery$/i.test(b.type||'')&&b.scope!='Device'&&b.capacity>=0&&b.capacity<=100);
let PWR=null;   /* the last power/status (installed), or {battery,ac} from the browser's battery */
window.__bsaver=false;
/* time left or to full, from the battery's energy (µWh) and power (µW), or its charge and current */
function batTime(b){if(!b)return'';const e=b.energy_now!=null?[b.energy_now,b.energy_full,b.power_now]:b.charge_now!=null?[b.charge_now,b.charge_full,b.current_now]:null;if(!e||!e[2]||e[2]<=0)return'';
 const h=b.status=='Charging'?(e[1]-e[0])/e[2]:b.status=='Discharging'?e[0]/e[2]:0;if(!(h>0)||h>48)return'';const m=Math.round(h*60);return(m>=60?Math.floor(m/60)+' h '+(m%60)+' min':m+' min')+(b.status=='Charging'?' to full':' left')}
const batState=b=>({Charging:'charging',Discharging:'on battery',Full:'full','Not charging':'plugged in, not charging'})[b.status]||(b.status||'').toLowerCase();
let batBtn=null;
function batDraw(){if(!batBtn){const W=$('#wsw');if(!W)return;batBtn=document.createElement('button');batBtn.id='bat';batBtn.style.cssText='background:transparent;border:0;display:flex;align-items:center;gap:3px;color:#e5e7eb;font:600 12px system-ui,sans-serif;cursor:pointer;padding:0 6px;height:26px';W.before(batBtn);batBtn.onclick=()=>pwrShow()}
 const b=PWR&&PWR.battery;if(!b){batBtn.style.display='none';return}batBtn.style.display='flex';
 const lv=Math.max(0,Math.min(100,b.capacity)),ch=b.status=='Charging'||(PWR.ac&&b.status!='Discharging'),col=lv<=10?'#ef4444':lv<=20?'#f59e0b':'#22c55e',w=Math.round(lv/100*15);
 batBtn.innerHTML=`<svg width='26' height='14' viewBox='0 0 26 14'><rect x='1' y='1' width='20' height='12' rx='2.5' fill='none' stroke='#e5e7eb' stroke-width='1.6'/><rect x='22' y='4.5' width='2.5' height='5' rx='1' fill='#e5e7eb'/><rect x='3.2' y='3.2' width='${w}' height='7.6' rx='1' fill='${col}'/>${ch?"<path d='M12.5 2.2 8.6 7.6h3l-1 4.4 4.1-5.6h-3z' fill='#fde047' stroke='#111' stroke-width='.6'/>":''}${window.__bsaver?"<path d='M17 11c-4 0-5-3-4-6 3 0 5 1 4 6zM13 5l3.6 5.4' fill='#86efac' stroke='#14532d' stroke-width='.6'/>":''}</svg><span></span>`;
 batBtn.lastChild.textContent=lv+'%';const t=batTime(b);batBtn.title=`Battery ${lv}%, ${batState(b)}${t?' · '+t:''}${window.__bsaver?' · battery saver on':''}`;batBtn.setAttribute('aria-label',batBtn.title)}
function pwrSaverApply(on){const was=window.__bsaver;window.__bsaver=!!on;const H=document.documentElement;H.classList.toggle('bsaver',!!on);if(on){H.classList.add('rmotion','rtrans')}else if(was){try{a11yApply()}catch(e){}}}
(function(){const o=a11yApply;a11yApply=function(){o.apply(this,arguments);if(window.__bsaver)document.documentElement.classList.add('rmotion','rtrans')}})();
async function pwrPoll(){try{if(RD){PWR=await rd('power/status');pwrSaverApply(PWR.now&&PWR.now.saver)}else if(navigator.getBattery){const b=await navigator.getBattery();PWR={ac:b.charging,battery:b.level<1||!b.charging||b.dischargingTime!=Infinity?{capacity:Math.round(b.level*100),status:b.charging?'Charging':'Discharging'}:null}}}catch(e){}batDraw();return PWR}
/* the power button: Ask is the Shut down dialog (core25); Shut down flushes the files first */
function pwrEvents(t){if(/^power (ac|battery|saver)/m.test(t))pwrPoll();if(/^power shutdown$/m.test(t)){try{fsFlush()}catch(e){}try{window.__rudolfLsFlush&&__rudolfLsFlush()}catch(e){}}}
/* Power and battery: in Settings › System health since 1.5.6 (System Settings before), between the summary and the
   events, so that a long event list does not push it out of sight */
const PWR_SCR=[0,1,2,5,10,15,30],PWR_SLP=[0,5,10,15,30,60];
function pwrPanel(c,W){const d=document.createElement('div');d.className='pwr31';d.id='pwr31';const at=c.querySelector(':scope > .hlev')||c.querySelector(':scope > .sfoot');if(at)at.before(d);else c.appendChild(d);
 const head=`<h4 class='sh4 sgrp' style='font-size:15px;margin-top:18px'>Power and battery</h4>`;
 if(!RD){d.innerHTML=head+`<div class='msg pwb'></div><div class='msg'>Power settings are part of Rudolf OS on its own computer; in the browser, your device manages its power.</div>`;pwrPoll().then(p=>{const b=p&&p.battery;d.querySelector('.pwb').textContent=b?`Battery ${b.capacity}%, ${batState(b)}.`:'No battery reported.'});return}
 d.innerHTML=head+`<div class='msg'>Loading…</div>`;
 const draw=p=>{if(!d.isConnected)return;const adm=isAdmin(),D=adm?'':' disabled',s=p.set,cn=p.can||{},b=p.battery;
  const sel=(k,opts,dis,why)=>`<select data-k='${k}'${D}${dis?' disabled title="'+lxEsc(why)+'"':''}>${opts.map(([v,t])=>optH(v,t,s[k])).join('')}</select>`;
  const pair=(k,opts,dis,why)=>`<div style='display:flex;gap:8px;flex-wrap:wrap'><label style='display:flex;flex-direction:column;font-size:11px'>On battery${sel(k+'_bat',opts,dis,why)}</label><label style='display:flex;flex-direction:column;font-size:11px'>Plugged in${sel(k+'_ac',opts,dis,why)}</label></div>`;
  const mins=(l,never)=>l.map(m=>[m,m?m+(m==1?' minute':' minutes'):never]);
  const noProf=!cn.profile&&!cn.epp,why=noProf?'This computer offers no power modes to Rudolf OS (no platform profile, no processor energy preference).':'';
  let health='';if(b){const f=b.energy_full??b.charge_full,dz=b.energy_full_design??b.charge_full_design;if(f&&dz)health=` · health ${Math.round(f/dz*100)} %`;if(b.cycle_count>0)health+=`, ${b.cycle_count} charge cycles`}
  d.innerHTML=head+`<h4 class='sh4'>Battery</h4><div class='msg'>${b?`${b.capacity} %, ${lxEsc(batState(b))}${batTime(b)?' · '+batTime(b):''}${health}${p.now&&p.now.saver?' · <b>battery saver is on</b>':''}`:'This computer has no battery.'}</div>
  <div class='r2'><label>Power mode</label>${pair('mode',[['saver','Power saver'],['balanced','Balanced'],['performance','Performance']],noProf,why)}${noProf?`<div></div><div class='msg'>${why}</div>`:''}
  <label>Battery saver</label><div>${b?`<label class='ck'><input type='checkbox' class='pwsn'${s.saver_now?' checked':''}${D}> On now (until the charger is plugged in)</label><div style='margin-top:4px'>On by itself at ${sel('saver_at',[[0,'never'],[10,'10 %'],[20,'20 %'],[30,'30 %']])}</div><div class='msg'>Power saver mode, the screen dimmer, fewer animations, the screen saver slower, no update checks${cn.wifiSave?', Wi-Fi power saving':''}.</div>`:'<div class="msg">Needs a battery.</div>'}</div>
  <label>Turn the screen off after</label>${pair('screen',mins(PWR_SCR,'Never'))}
  <label>Sleep after</label>${pair('sleep',mins(PWR_SLP,'Never'))}
  <label>When the lid is closed</label>${pair('lid',[['sleep','Sleep'],['screen','Turn the screen off'],['nothing','Do nothing']],!cn.lid,'This computer has no lid switch.')}
  <label>When the power button is pressed</label>${sel('button',[['ask','Ask what to do'],['sleep','Sleep'],['shutdown','Shut down']])}
  <label>Battery care</label><div>${sel('care',[[70,'Stop charging at 70 %'],[80,'Stop charging at 80 % (for a laptop that is mostly plugged in)'],[90,'Stop charging at 90 %'],[100,'Charge to 100 %']],!cn.care,'This battery does not let Rudolf OS limit its charge.')}${cn.care?(p.now&&p.now.care!=null&&p.now.care!=s.care?`<div class='msg pwcn'>The battery reports a limit of ${+p.now.care} %: it may take only some limits, and stop where its own firmware chooses.</div>`:''):`<div class='msg'>This battery does not let Rudolf OS limit its charge.</div>`}</div></div>
  <div class='msg pwm'>${adm?'Changes apply at once and hold for everyone, also before anyone signs in.':'Only administrators can change these.'}${p.now&&(p.now.profile||p.now.epp)?` In force: ${[p.now.profile&&'platform profile '+lxEsc(p.now.profile),p.now.epp&&'processor preference '+lxEsc(p.now.epp)].filter(Boolean).join(', ')}.`:''}</div>`;
  const send=async q=>{try{const r=await rdPost('power/set?'+q);PWR=r;pwrSaverApply(r.now&&r.now.saver);batDraw();draw(r)}catch(e){notify('Power and battery',e.message,{kind:'error'})}};
  d.querySelectorAll('select[data-k]').forEach(x=>x.addEventListener('change',e=>{e.stopPropagation();send(x.dataset.k+'='+x.value)}));
  const sn=d.querySelector('.pwsn');if(sn)sn.addEventListener('change',e=>{e.stopPropagation();send('saver_now='+(sn.checked?1:0))})};
 pwrPoll().then(p=>p&&p.set?draw(p):d.innerHTML=head+`<div class='msg'>The power settings could not be read.</div>`)}
/* (healthPanel in sec50.js calls pwrPanel itself: sec50 comes after this module) */
/* where it is, opened from elsewhere (the battery on the taskbar, User Settings' Sleep) */
function pwrShow(W){const go=()=>setTimeout(()=>{const g=document.getElementById('pwr31');g&&g.scrollIntoView({block:'start'})},300);if(W&&W.show){W.show('health');go()}else{settings('health');go()}}
/* User Settings: Sleep is in Power and battery now (a time set there before 1.4.4 is taken over once) */
(function(){const o=usrPanelS;usrPanelS=function(c,W){o(c,W);const s=c.querySelector('#usp');if(!s)return;const l=s.previousElementSibling;const n=document.createElement('div');n.className='msg';n.innerHTML=`In <a href='#'>System health › Power and battery</a>.`;s.replaceWith(n);n.querySelector('a').onclick=e=>{e.preventDefault();pwrShow(W)}}})();
async function pwrMigrate(){if(!RD||!SYSC.suspmin||SYSC.suspmig)return;const m=[5,10,15,30,60].reduce((a,v)=>Math.abs(v-SYSC.suspmin)<Math.abs(a-SYSC.suspmin)?v:a,60);
 try{await rdPost(`power/set?sleep_ac=${m}&sleep_bat=${m}`);SYSC.suspmig=1;sysSave()}catch(e){}}
if(typeof evOn=='function')evOn(pwrEvents);
setTimeout(()=>{pwrPoll();pwrMigrate()},1500);setInterval(()=>{if(!document.hidden)pwrPoll()},30000);
