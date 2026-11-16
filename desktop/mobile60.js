/* ===== Rudolf OS 1.6.0 (docs/plan-1.6.0.md Part 2): Settings › Mobile network, and airplane mode =====
   For each cellular modem (rudolfd's mobile requests, ModemManager underneath):
   - the SIM: missing, locked with its PIN (an administrator's name and password, checked by rudolfd; three tries), its
     PUK and a new PIN after that (with the warning that ten wrong PUKs block the SIM for good), blocked for good;
   - the operator's settings (APN): found by themselves from the operators' database by the SIM's operator code
     (Rudolf's corrections first), or typed in exactly, letters' case kept (docs/plan-1.6.1.md Part 7);
   - roaming: off by default, with what it may cost; the network type (5G, 4G, 3G) where the modem lets it be chosen.
   Quick settings: ✈️ Airplane mode: Wi-Fi, Bluetooth and the modems off together, and back as they were.
   (Part 5) The signal's details (RSRP, RSRQ, SINR) in Settings; the taskbar's network icon shows mobile data's bars,
   type and operator (net31.js; the native desktop draws the same); System health's modem events (sec50.js).
   (Part 4) Data this month (from the interface's counters, per SIM, its month from the day chosen), a monthly limit
   with a notice at 80 % and, if chosen, mobile data off at 100 %; updates on mobile data offered with their size
   (sysupd25.js), never downloaded by themselves.
   (Part 3) Connect and Disconnect (Settings › Mobile network, and quick settings' 📶 Mobile data with a modem whose
   SIM is unlocked), Connect automatically; the notices when mobile data takes over from Wi-Fi or a cable and when it
   goes back (rudolfd's mobile/notes, which the native desktop shows the same way). */
const MOB_MODE={auto:'Automatic (the best there is)','5g':'5G preferred (with 4G)','4g':'4G (LTE) only','3g':'3G only'};
let AIR60={on:false,t:0};
async function airPoll(){if(!RD)return AIR60;try{const j=await rd('mobile/airplane');AIR60={on:!!j.on,t:Date.now()}}catch(e){}return AIR60}
async function airSet(on){try{await rd('mobile/airplane?on='+(on?1:0),{method:'POST'});AIR60={on:!!on,t:Date.now()}}catch(e){notify('✈️ Airplane mode',e.message,{kind:'error'})}
 dispatchEvent(new CustomEvent('rudolf-airplane',{detail:AIR60}));return AIR60}
/* quick settings: the ninth switch (the page's own; the native desktop draws the same tile, rudolf-native.c) */
(function(){if(typeof qsToggle!='function')return;const o=qsToggle;qsToggle=function(){o.apply(this,arguments);const p=document.getElementById('qsp');if(!p||!RD)return;const g=p.querySelector('.qsg');if(!g||g.querySelector('.qair'))return;
 const b=document.createElement('button');b.className='qst qair'+(AIR60.on?' on':'');b.setAttribute('aria-pressed',AIR60.on);b.innerHTML=`<i>✈️</i><span>Airplane mode</span>`;
 const show=v=>{b.classList.toggle('on',!!v);b.setAttribute('aria-pressed',!!v)};
 b.onclick=async()=>{const cur=(await airPoll()).on;show(!cur);const r=await airSet(!cur);show(r.on)};g.appendChild(b);airPoll().then(r=>show(r.on));
 /* (Part 3) 📶 Mobile data: the first modem whose SIM is unlocked, connected or not */
 rd('mobile/status').then(s=>{const m=mobUsable(s);if(!m||!p.isConnected||g.querySelector('.qmob'))return;const t=document.createElement('button');const on=m.state=='connected';t.className='qst qmob'+(on?' on':'');t.setAttribute('aria-pressed',on);t.innerHTML=`<i>📶</i><span>Mobile data</span>`;
  t.onclick=async()=>{const now=!t.classList.contains('on');t.classList.toggle('on',now);t.setAttribute('aria-pressed',now);try{await mobConnect(m.id,now)}catch(e){t.classList.toggle('on',!now);t.setAttribute('aria-pressed',!now)}};g.appendChild(t)}).catch(()=>{})}})();
function mobUsable(s){return ((s&&s.modems)||[]).find(m=>m.sim&&(!m.locked||m.locked=='none')&&m.state!='failed')||null}
async function mobConnect(id,on){try{const r=await rd('mobile/connect?id='+id+'&on='+(on?1:0),{method:'POST'});notify('📶 Mobile data',on?'Connected.':'Disconnected: it stays off until you connect again.',{app:'Network',quiet:true,toast:3000});return r}
 catch(e){notify('📶 Mobile data',e.message,{kind:'error',app:'Network'});throw e}}
/* (Part 3) the cellular connection's notices, every 5 s (the first answer only says where they are up to) */
let MOBN=-1;
async function mobNotes(){if(!RD)return;try{const j=await rd('mobile/notes?since='+(MOBN<0?0:MOBN));if(MOBN>=0&&typeof USER!='undefined'&&USER&&!LOCKED)(j.notes||[]).forEach(n=>notify(n.t,n.b,{app:'Network',action:()=>settings('mob')}));MOBN=j.last||0}catch(e){}}
if(RD){setTimeout(mobNotes,3000);setInterval(mobNotes,5000)}
/* Settings › Mobile network */
function mobPanelS(c,W){
 if(!RD){sp(c,`<h4 class='sh4'>Mobile network</h4><div class='msg'>The bootable Rudolf OS uses a cellular modem's SIM card here: its PIN, the operator's settings, roaming and the network type.</div>`);return}
 const box=sp(c,`<div class='mob60'><div class='msg'>Looking for cellular modems…</div></div>`).querySelector('.mob60');
 const draw=async()=>{let s=null,air=await airPoll();try{s=await rd('mobile/status')}catch(e){}if(!box.isConnected)return;const ms=(s&&s.modems)||[];MOBUSE={route:(s&&s.route)||'',cellular:!!(s&&s.cellular)};
  box.innerHTML=(air.on?`<div class='msg mobair'>✈️ Airplane mode is on: the cellular modem, Wi-Fi and Bluetooth are off. Turn it off in quick settings.</div>`:'');
  if(!ms.length){box.insertAdjacentHTML('beforeend',`<h4 class='sh4'>Mobile network</h4><div class='msg mobnone'>No cellular modem is connected. A USB cellular stick, or a laptop's built-in cellular module, shows here with its SIM card. A hotspot or a phone sharing its connection works through Wi-Fi or USB (Network Settings).</div>`);return}
  for(const m of ms)box.appendChild(await mobModem(m,draw))};
 draw()}
async function mobModem(m,redraw){
 const d=document.createElement('div');d.className='mobm';d.dataset.id=m.id;const name=[m.maker,m.model].filter(x=>x&&x!='--').join(' ')||'Cellular modem';
 d.innerHTML=`<h4 class='sh4'></h4><div class='msg mobst'></div><div class='mobsim'></div><div class='mobconnb'></div><div class='mobset'></div>`;d.querySelector('h4').textContent='📶 '+name;
 d.querySelector('.mobst').textContent=typeof mdmLine=='function'?mdmLine(m):m.state;
 const sim=d.querySelector('.mobsim'),say=(t,bad)=>{const e=sim.querySelector('.mobmsg');if(e){e.textContent=t;e.style.color=bad?'#dc2626':''}};
 const codes=async(what,body,label)=>{const cr=await elevate(label,{cred:true});if(!cr)return;
  try{const r=await rd('mobile/'+what+'?id='+m.id,{method:'POST',body:JSON.stringify({n:cr.n,p:cr.p,...body})});notify('📶 SIM card',r.message||'Unlocked.',{app:'Network'});redraw()}
  catch(e){say(e.message,1);if(/blocked|tries left/.test(e.message))setTimeout(redraw,1500)}};
 if(!m.sim||m.failed=='sim-missing')sim.innerHTML=`<div class='msg'>No SIM card. Insert one (turn the computer off first if its slot is inside, under a cover), then restart.</div>`;
 else if(m.failed=='sim-error')sim.innerHTML=`<div class='msg mobdead' style='color:#dc2626'><b>The SIM card is blocked for good</b> (its PUK was given wrongly too many times). Only your operator can replace it: ask them for a new SIM.</div>`;
 else if(m.locked=='sim-pin'){sim.innerHTML=`<div class='msg'>The SIM card is locked with its PIN. <b class='mobleft'></b></div><div class='r2'><label>PIN</label><input class='mobpin' inputmode='numeric' maxlength='8' autocomplete='off'></div><button class='mobpinb'>Unlock the SIM</button><div class='msg mobmsg'></div><div class='msg'>An administrator's password is needed. After three wrong PINs the SIM asks for its PUK.</div>`;
  sim.querySelector('.mobleft').textContent=(m.retries&&m.retries.pin!=null?m.retries.pin:3)+' tries left.';const P=maskInput(sim.querySelector('.mobpin'));
  sim.querySelector('.mobpinb').onclick=()=>{const pin=P.get();if(!/^\d{4,8}$/.test(pin))return say('A PIN is 4 to 8 digits.',1);codes('pin',{pin},'Unlock the SIM card with its PIN')}}
 else if(m.locked=='sim-puk'){sim.innerHTML=`<div class='msg mobpukw' style='color:#b45309'>⚠️ <b>The SIM card is blocked</b> after three wrong PINs. Type its PUK (8 digits: on the card the SIM came on, or from your operator) and a new PIN. <b>Careful: ten wrong PUKs block the SIM for good</b>, and only your operator can replace it. <b class='mobleft'></b></div>
  <div class='r2'><label>PUK</label><input class='mobpuk' inputmode='numeric' maxlength='8' autocomplete='off'><label>New PIN</label><input class='mobnp1' inputmode='numeric' maxlength='8' autocomplete='off'><label>New PIN again</label><input class='mobnp2' inputmode='numeric' maxlength='8' autocomplete='off'></div><button class='mobpukb'>Unblock the SIM</button><div class='msg mobmsg'></div>`;
  sim.querySelector('.mobleft').textContent=(m.retries&&m.retries.puk!=null?m.retries.puk:10)+' tries left.';const K=maskInput(sim.querySelector('.mobpuk')),N1=maskInput(sim.querySelector('.mobnp1')),N2=maskInput(sim.querySelector('.mobnp2'));
  sim.querySelector('.mobpukb').onclick=()=>{const puk=K.get(),pin=N1.get();if(!/^\d{8}$/.test(puk))return say('A PUK is 8 digits.',1);if(!/^\d{4,8}$/.test(pin))return say('The new PIN must be 4 to 8 digits.',1);if(pin!=N2.get())return say('The two new PINs are not the same.',1);
   codes('puk',{puk,pin},'Unblock the SIM card with its PUK and a new PIN')}}
 else{sim.innerHTML=`<div class='msg mobsimok'></div>`;sim.querySelector('.mobsimok').textContent='SIM card: '+(m.simName||'unnamed operator')+(m.simOp?' ('+m.simOp+')':'')+(m.reg=='roaming'?' · roaming now'+(m.operator?' on '+m.operator:''):'')}
 if(m.sim&&!m.locked&&m.failed!='sim-error'&&m.failed!='sim-missing'){mobConn(d.querySelector('.mobconnb'),m,redraw);mobSignal(d.querySelector('.mobconnb'),m);await mobSettings(d.querySelector('.mobset'),m)}
 return d}
async function mobSettings(el,m){let s=null;try{s=await rd('mobile/settings?id='+m.id)}catch(e){el.innerHTML=`<div class='msg'></div>`;el.firstChild.textContent='The SIM\'s settings could not be read: '+e.message;return}
 const set=async(k,v)=>{try{await rd('mobile/settings?id='+m.id+'&k='+k+'&v='+encodeURIComponent(v),{method:'POST'});return true}catch(e){notify('📶 Mobile network',e.message,{kind:'error',app:'Network'});return false}};
 const auto=s.auto,custom=!!s.apn||!auto;
 el.innerHTML=`<h4 class='sh4'>The operator's settings (APN)</h4><div><label><input type='radio' name='mobapn${+m.id|0}' class='mobauto'> <span class='mobautot'></span></label></div><div><label><input type='radio' name='mobapn${+m.id|0}' class='mobcust'> Typed in</label></div>
  <div class='mobcf'><div class='r2'><label>APN</label><input class='mobapn' autocomplete='off' spellcheck='false' autocapitalize='off'><label>User name</label><input class='mobuser' autocomplete='off' spellcheck='false' autocapitalize='off'><label>Password</label><input class='mobpass' type='password' autocomplete='off'></div>
  <div class='msg'>Type the APN exactly as your operator writes it: some networks refuse it in other capitals. The user name and password are usually empty.</div><button class='mobsave'>Save</button></div>
  <h4 class='sh4'>Connect automatically</h4><label><input type='checkbox' class='mobautoc'> Connect to mobile data by itself</label><div class='msg'>On the SIM's home network; abroad only when roaming is allowed below. Wi-Fi and cables come first: mobile data stays in reserve and takes over within seconds when they drop.</div>
  <h4 class='sh4'>Data this month</h4><div class='msg mobuse'></div><div class='mobbar' style='height:8px;border-radius:4px;background:#e5e7eb;overflow:hidden;margin:4px 0'><div class='mobbarv' style='height:100%;width:0;background:#16a34a'></div></div>
  <div class='r2'><label>Monthly limit (MB)</label><input class='moblim' type='number' min='0' step='100' style='width:110px'><label>The month starts on day</label><select class='mobsd'></select></div>
  <label><input type='checkbox' class='mobdoff'> Turn mobile data off at the limit</label><div class='msg'>A notice at 80 %. Counted on this computer from what the modem sends and receives: your operator's count can differ a little.</div>
  <h4 class='sh4'>Roaming</h4><label><input type='checkbox' class='mobroam'> Use mobile data when roaming</label><div class='msg'>Off by default: abroad, or on another operator's network, data can cost much more than at home.</div>
  <h4 class='sh4'>Network type</h4><div class='r2'><label>Use</label><select class='mobmode'></select></div><div class='msg mobmodem'></div>`;
 const q=x=>el.querySelector(x);
 q('.mobautot').textContent=auto?'Automatic: '+auto.apn+' ('+auto.name+', '+(auto.source=='rudolf'?'corrected by Rudolf OS':'from the operators\' database')+')':'Automatic: none is known for this SIM\'s operator'+(s.simOp?' ('+s.simOp+')':'')+': type it in';
 q('.mobauto').disabled=!auto;q('.mobauto').checked=!custom;q('.mobcust').checked=custom;q('.mobapn').value=s.apn||'';q('.mobuser').value=s.apnuser||'';q('.mobpass').placeholder=s.apnpass?'(kept: type to change)':'';
 const cf=()=>{q('.mobcf').style.display=q('.mobcust').checked?'':'none'};cf();
 q('.mobauto').onchange=async()=>{cf();if(await set('apn',''))notify('📶 Mobile network','The operator\'s settings are found by themselves again'+(auto?': '+auto.apn:'')+'.',{app:'Network',quiet:true})};q('.mobcust').onchange=cf;
 q('.mobsave').onclick=async()=>{const a=q('.mobapn').value.trim();if(!/^[A-Za-z0-9][A-Za-z0-9._-]{0,99}$/.test(a))return notify('📶 Mobile network','An APN has letters, digits, dots and hyphens only.',{kind:'error',app:'Network'});
  if(!(await set('apn',a)))return;await set('apnuser',q('.mobuser').value);const pw=q('.mobpass').value;if(pw&&await set('apnpass',pw))s.apnpass=true;q('.mobpass').value='';q('.mobpass').placeholder=s.apnpass?'(kept: type to change)':'';
  notify('📶 Mobile network','Saved: the APN is '+a+'.',{app:'Network'})};
 q('.mobautoc').checked=s.autoconnect!==false;q('.mobautoc').onchange=async e=>{if(!(await set('autoconnect',e.target.checked?1:0)))e.target.checked=!e.target.checked};
 const u=s.usage||{},fmt=b=>b>=1e9?(b/1e9).toFixed(2)+' GB':Math.round((b||0)/1e6)+' MB';
 q('.mobuse').textContent=fmt(u.used)+(u.limit?' of '+u.limit+' MB ('+u.pct+' %)':'')+' since '+(u.start||'the 1st')+(u.next?', until '+u.next:'')+(u.off?' · mobile data is off: the limit is reached (Connect to go on: it may cost)':'');
 q('.mobbarv').style.width=Math.min(100,u.pct||0)+'%';q('.mobbarv').style.background=(u.pct||0)>=100?'#dc2626':(u.pct||0)>=80?'#d97706':'#16a34a';q('.mobbar').style.display=u.limit?'':'none';
 q('.moblim').value=u.limit||0;q('.moblim').onchange=async e=>{const v=Math.max(0,Math.round(+e.target.value||0));if(!(await set('limit',v)))e.target.value=u.limit||0};
 for(let i=1;i<=28;i++){const o=document.createElement('option');o.value=i;o.textContent=i;q('.mobsd').appendChild(o)}q('.mobsd').value=u.startday||1;q('.mobsd').onchange=async e=>{if(!(await set('startday',e.target.value)))e.target.value=u.startday||1};
 q('.mobdoff').checked=!!u.dataoff;q('.mobdoff').onchange=async e=>{if(!(await set('dataoff',e.target.checked?1:0)))e.target.checked=!e.target.checked};
 q('.mobroam').checked=!!s.roaming;q('.mobroam').onchange=async e=>{if(!(await set('roaming',e.target.checked?1:0)))e.target.checked=!e.target.checked};
 const sel=q('.mobmode');(s.modes||['auto']).forEach(k=>{const o=document.createElement('option');o.value=k;o.textContent=MOB_MODE[k]||k;sel.appendChild(o)});sel.value=s.mode||'auto';
 if((s.modes||[]).length<2)q('.mobmodem').textContent='This modem does not let the network type be chosen.';
 sel.onchange=async()=>{const v=sel.value;if(!(await set('mode',v)))sel.value=s.mode||'auto';else s.mode=v}}
/* (Part 3) the connection: connected or not, in use or kept in reserve behind Wi-Fi or a cable; Connect, Disconnect */
function mobConn(el,m,redraw){const on=m.state=='connected',d=document.createElement('div');d.className='mobconn';
 d.innerHTML=`<h4 class='sh4'>Connection</h4><div class='msg mobcs'></div><button class='mobcb'></button>`;
 d.querySelector('.mobcs').textContent=on?(m.iface?'Connected'+(MOBUSE.cellular?' and in use':': kept in reserve while '+(/^wl/.test(MOBUSE.route)?'Wi-Fi':MOBUSE.route?'a cable':'another connection')+' carries the traffic'):'Connected'):m.state=='registered'?'Not connected':typeof mdmLine=='function'?mdmLine(m):m.state;
 const b=d.querySelector('.mobcb');b.textContent=on?'Disconnect':'Connect';b.onclick=async()=>{b.disabled=true;try{await mobConnect(m.id,!on)}catch(e){}b.disabled=false;redraw()};el.appendChild(d)}
let MOBUSE={route:'',cellular:false};
/* (Part 5) the signal's details, for placing the computer or an antenna: LTE's RSRP (strength), RSRQ (quality), SINR
   (against the noise), and 5G's or 3G's; every 5 s while the page is open */
const MOBQ=(v,good,fair,poor)=>v==null?'':v>=good?'excellent':v>=fair?'good':v>=poor?'fair':'poor';
function mobSignal(el,m){if(!(m.state=='registered'||m.state=='connected'))return;const d=document.createElement('div');d.className='mobsig';
 d.innerHTML=`<h4 class='sh4'>Signal</h4><div class='msg mobsigl'>Reading the signal…</div><div class='msg'>Move the computer (or an outside antenna) while watching these: they change every few seconds.</div>`;el.appendChild(d);
 const L=d.querySelector('.mobsigl');let iv=null,seen=0;const one=async()=>{if(!d.isConnected){if(seen)clearInterval(iv);return}seen=1;let g=null;try{g=await rd('mobile/signal?id='+m.id)}catch(e){}if(!d.isConnected)return;
  if(!g){L.textContent='The modem gives no signal details.';return}const p=[],u=(v,x)=>v==null?null:(Math.round(v*10)/10)+' '+x;
  const lte=g.lte||{},nr=g.nr5g||{},um=g.umts||{};
  if(lte.rsrp!=null)p.push('LTE: RSRP '+u(lte.rsrp,'dBm')+' ('+MOBQ(lte.rsrp,-80,-90,-100)+')'+(lte.rsrq!=null?' · RSRQ '+u(lte.rsrq,'dB')+' ('+MOBQ(lte.rsrq,-10,-15,-20)+')':'')+(lte.sinr!=null?' · SINR '+u(lte.sinr,'dB')+' ('+MOBQ(lte.sinr,20,13,0)+')':'')+(lte.rssi!=null?' · RSSI '+u(lte.rssi,'dBm'):''));
  if(nr.rsrp!=null)p.push('5G: RSRP '+u(nr.rsrp,'dBm')+' ('+MOBQ(nr.rsrp,-80,-90,-100)+')'+(nr.rsrq!=null?' · RSRQ '+u(nr.rsrq,'dB'):'')+(nr.sinr!=null?' · SINR '+u(nr.sinr,'dB'):''));
  if(um.rscp!=null)p.push('3G: RSCP '+u(um.rscp,'dBm')+(um.ecio!=null?' · Ec/Io '+u(um.ecio,'dB'):'')+(um.rssi!=null?' · RSSI '+u(um.rssi,'dBm'):''));
  L.textContent=p.length?p.join('\n'):'The modem gives no signal details yet.';L.style.whiteSpace='pre-line'};
 setTimeout(one,50);iv=setInterval(one,5000)}
