/* ===== Rudolf OS 1.5.1: Bluetooth (rudolfd: GET bt, POST bt/power, bt/scan, bt/pair, bt/connect, bt/disconnect,
   bt/remove; /usr/lib/rudolf/bluetooth: BlueZ) =====
   Settings › Bluetooth: on/off, the paired devices (connected or not, battery), Search for devices, Pair, Connect,
   Disconnect, Forget. Pairing needs no code: controllers, mice, headphones and most keyboards pair after their own
   pairing button is pressed. Headphones become a sound output (Volume Controls) when they connect.
   Notifications: a paired device connects or goes away, its battery runs low. */
window.__bt51=1;
let BT51=null;
async function btStatus(){if(!RD)return null;try{return BT51=await rd('bt')}catch(e){return null}}
const BT_KIND=d=>{const i=d.icon||'';return /input-gaming/.test(i)?['🎮','controller']:/input-keyboard/.test(i)?['⌨️','keyboard']:/input-mouse|input-tablet/.test(i)?['🖱️','mouse']:/audio-head|audio-card|audio-speaker/.test(i)||d.audio?['🎧','headphones or speaker']:/phone/.test(i)?['📱','phone']:/computer/.test(i)?['💻','computer']:['🔵','device']};
const btName=d=>d.name&&!/^([0-9A-F]{2}[-:]){5}[0-9A-F]{2}$/.test(d.name)?d.name:'Unnamed device';
function btPanel(c,W){
 c.innerHTML=`<h3>Bluetooth</h3><p class='msg' style='margin:0 0 10px'>Controllers, keyboards, mice and headphones. To pair one, put it in pairing mode (usually by holding its pairing or Share/Sync button until its light flashes), then choose <b>Search for devices</b>.</p>
<div class='r2'><label>Bluetooth</label><div><label class='ck'><input type='checkbox' class='bto'> On</label> <span class='bts' style='color:#6b7280;font-size:12px'></span></div></div>
<h4 class='sh4'>My devices</h4><div class='btm'></div>
<h4 class='sh4'>Other devices</h4><div class='rmrow' style='justify-content:flex-start'><button class='btf'>Search for devices</button> <span class='btw' style='color:#6b7280;font-size:12px'></span></div><div class='rmrow' style='justify-content:flex-start'><button class='btv'>Let this computer be found</button> <span class='btvs' style='color:#6b7280;font-size:12px'></span></div><p class='msg' style='margin:4px 0 8px;font-size:12px;color:#6b7280'>To pair from a phone, choose this, then look for this computer in the phone's Bluetooth settings.</p><div class='bt51n'></div><div class='msg btmsg'></div>`;
 const q=s=>c.querySelector(s),cb=q('.bto'),st=q('.bts'),mine=q('.btm'),near=q('.bt51n'),say=(t,ok)=>{const m=q('.btmsg');m.textContent=t;m.className='msg btmsg '+(ok?'ok':'bad')};
 let busy=false;
 const row=(d,host,btns)=>{const k=BT_KIND(d),r=document.createElement('div');r.className='acr';r.style.cssText='display:flex;gap:8px;align-items:center;padding:4px 0';
  r.innerHTML=`<span style='font-size:20px'>${k[0]}</span><span style='flex:1'><b></b><br><small style='color:#6b7280'></small></span>`;
  r.querySelector('b').textContent=btName(d);r.querySelector('small').textContent=[k[1],d.connected?'connected':d.paired?'not connected':'',d.battery!=null?'battery '+d.battery+'%':''].filter(Boolean).join(' · ');
  btns.forEach(([l,f,pri])=>{const b=document.createElement('button');b.textContent=l;if(pri)b.className='pri';b.onclick=async()=>{if(busy)return;busy=true;c.querySelectorAll('button').forEach(x=>x.disabled=true);try{await f()}finally{busy=false;draw()}};r.appendChild(b)});host.appendChild(r)};
 const act=async(what,d,okText)=>{say(what=='pair'?`Pairing with ${btName(d)}… keep it in pairing mode. A phone shows a code: check it matches the one shown here.`:'Working…',true);if(what=='pair')setTimeout(btAsk,1500);
  try{const r=await rdPost(`bt/${what}?mac=${d.mac}`);say(okText,true);if(r.status)BT51=r.status}catch(e){say(`${btName(d)}: ${e.message||'it did not work'}. Make sure it is on, near the computer, and (to pair) in pairing mode.`)}};
 const draw=async(s)=>{s=s||await btStatus();mine.innerHTML='';near.innerHTML='';
  if(!s){st.textContent='Bluetooth needs the installed Rudolf OS.';cb.disabled=true;q('.btf').disabled=true;q('.btv').disabled=true;return}
  if(!s.adapter){q('.btv').disabled=true;st.textContent='This computer has no Bluetooth adapter Rudolf OS can use. A USB Bluetooth adapter works too.';cb.disabled=true;q('.btf').disabled=true;mine.innerHTML="<i style='color:#6b7280'>None.</i>";return}
  cb.disabled=false;cb.checked=!!s.powered;st.textContent=s.powered?(s.discovering?'On · searching…':'On'):(s.running?'Off':'Starting…');q('.btf').disabled=!s.powered;q('.btv').disabled=!s.powered;q('.btvs').textContent=s.powered&&s.visible?`Visible as “${s.name||'Rudolf OS'}” for a few minutes.`:'';
  const paired=s.devices.filter(d=>d.paired),others=s.devices.filter(d=>!d.paired).sort((a,b)=>(b.rssi||-999)-(a.rssi||-999));
  if(!paired.length)mine.innerHTML="<i style='color:#6b7280'>No devices paired yet.</i>";
  paired.forEach(d=>row(d,mine,[d.connected?['Disconnect',()=>act('disconnect',d,`${btName(d)} is disconnected.`)]:['Connect',()=>act('connect',d,`${btName(d)} is connected.`),1],['Forget',()=>act('remove',d,`${btName(d)} is forgotten. Pair it again to use it.`)]]));
  if(!others.length)near.innerHTML=`<i style='color:#6b7280'>${s.discovering?'Searching…':'Choose Search for devices to find one.'}</i>`;
  others.forEach(d=>row(d,near,[['Pair',()=>act('pair',d,`${btName(d)} is paired and connected.`),1]]))};
 cb.onchange=async()=>{const on=cb.checked;cb.disabled=true;try{draw(await rdPost('bt/power?on='+(on?1:0)))}catch(e){say(e.message||'Bluetooth could not be changed.');draw()}};
 q('.btv').onclick=async()=>{if(busy)return;q('.btv').disabled=true;try{draw(await rdPost('bt/visible'))}catch(e){say(e.message||'It did not work.')}};
 q('.btf').onclick=async()=>{if(busy)return;busy=true;q('.btf').disabled=true;q('.btw').textContent='Searching for 15 seconds…';
  const t=setInterval(()=>btStatus().then(s=>s&&c.isConnected&&!busy?draw(s):0),3000);
  try{draw(await rdPost('bt/scan?s=15'))}catch(e){say(e.message||'The search did not work.')}finally{clearInterval(t);busy=false;q('.btw').textContent=''}};
 draw();const iv=setInterval(()=>{if(!c.isConnected){clearInterval(iv);return}if(!busy)draw()},8000)}
/* notifications: a paired device connects or goes away, its battery is low (checked every 20 s while Bluetooth is on) */
let BT_SEEN=null;
async function btWatch(){const s=await btStatus();setTimeout(btWatch,s&&s.powered?20000:60000);if(!s||!s.devices)return;
 const now=new Map(s.devices.filter(d=>d.paired).map(d=>[d.mac,d]));
 if(BT_SEEN){for(const[m,d]of now){const was=BT_SEEN.get(m);const k=BT_KIND(d);
   if(d.connected&&(!was||!was.connected))notify(k[0]+' '+btName(d)+' connected',d.audio?'Sound now plays through it.':d.battery!=null?`Battery ${d.battery}%.`:'Ready to use.');
   else if(!d.connected&&was&&was.connected)notify(k[0]+' '+btName(d)+' disconnected',d.audio?'Sound plays through the computer again.':'It reconnects by itself when it is turned on nearby.');
   if(d.connected&&d.battery!=null&&d.battery<=15&&!(was&&was.connected&&was.battery!=null&&was.battery<=15))notify(k[0]+' '+btName(d)+': battery low',`${d.battery}% left. Charge it soon.`,{kind:'error'})}}
 BT_SEEN=now}
if(RD)setTimeout(btWatch,9000);
/* the pairing question, wherever you are (1.5.1): a phone's code to compare, a device asking to pair, a code to type on
   a keyboard. rudolfd's bt/ask is a small file the pairing agent writes; it is asked every 2 s while Bluetooth is on,
   and an unanswered question is answered no after 30 s. */
let BTQ=null;
function btAskBox(a){let b=document.querySelector('.bt51q');
 if(!a){if(b)b.remove();BTQ=null;return}
 if(BTQ===a.t&&b)return;BTQ=a.t;if(!b){b=document.createElement('div');b.className='bt51q';document.body.appendChild(b)}
 b.style.cssText='position:fixed;right:16px;bottom:64px;z-index:99990;max-width:340px;background:#fff;color:#111827;border:1px solid #d1d5db;border-radius:10px;box-shadow:0 8px 30px rgba(0,0,0,.25);padding:14px 16px;font-size:14px';
 const nm=a.name||'',who=nm||'A Bluetooth device';
 b.innerHTML=`<div style='font-weight:600;margin-bottom:6px'>🔵 Bluetooth pairing</div><div class='bt51qt'></div><div class='bt51qc' style='font-size:28px;letter-spacing:4px;font-weight:700;margin:8px 0'></div><div class='bt51qb' style='display:flex;gap:8px;justify-content:flex-end'></div>`;
 const t=b.querySelector('.bt51qt'),c=b.querySelector('.bt51qc'),bb=b.querySelector('.bt51qb');
 const btn=(l,ok,pri)=>{const x=document.createElement('button');x.textContent=l;if(pri)x.className='pri';x.onclick=async()=>{bb.querySelectorAll('button').forEach(y=>y.disabled=true);try{await rdPost('bt/answer?ok='+(ok?1:0))}catch(e){}btAskBox(null)};bb.appendChild(x)};
 if(a.kind=='confirm'){t.textContent=`Pair with ${nm||'this device'}? Choose Yes only if it shows the same code:`;c.textContent=a.code;btn('No',0);btn('Yes, pair',1,1)}
 else if(a.kind=='accept'){t.textContent=`${who} wants to pair with this computer.`;c.remove();btn("Don't pair",0);btn('Pair',1,1)}
 else if(a.kind=='keyboard'){t.textContent=`Type this code on ${nm||'the keyboard'}, then press Enter:`;c.textContent=a.code}
 else{b.remove();BTQ=null}}
async function btAsk(){try{const r=await rd('bt/ask');btAskBox(r.ask)}catch(e){}}
if(RD)setInterval(()=>{if(BT51&&BT51.powered)btAsk()},2000);
