/* ===== 1.4.4 (net31): the taskbar's network icon on installed Rudolf OS shows the real network (rudolfd's net): the
   interface with the default route decides, Wi-Fi with its network name and signal, wired with its speed. Before,
   it showed the browser version's simulated "Ethernet" connection, also on a laptop on Wi-Fi without an Ethernet port.
   The browser version keeps the simulation. ===== */
let N31={ifs:[],cur:null};   // the real adapters (no virtual ones), the one carrying the default route (or the first one with an address)
function net31Pick(n){const ifs=(n.interfaces||[]).filter(i=>!i.virtual);let cur=ifs.find(i=>i.default&&i.ip)||ifs.find(i=>i.ip&&i.state=='up')||null;return{ifs,cur}}
function net31Desc(i){if(!i)return'';if(i.wireless)return'Wi-Fi — connected'+(i.ssid?' ('+(typeof wfDecode=='function'?wfDecode(i.ssid):i.ssid)+')':'');
 return'Ethernet — connected ('+i.name+(i.speed>0?', '+(i.speed>=1000?i.speed/1000+' Gb/s':i.speed+' Mb/s'):'')+')'}
const net31Bars=r=>r==null?3:r>=-60?3:r>=-70?2:1;
async function net31Poll(){if(!RD)return;let n;try{n=await rd('net')}catch(e){return}const was=window.__net31&&window.__net31.up,o=net31Pick(n);N31=o;
 window.__net31={up:!!o.cur};netUpd();if(o.cur&&!was)netProbe();else if(!o.cur)NETST.inet=null}
/* the icon (netUpd hands over to this once the real network is known) */
function net31Upd(){const B=$('#net');if(!B)return;const i=N31.cur,type=i?(i.wireless?'wifi':'wired'):N31.ifs.some(x=>x.wireless)||!N31.ifs.length?'wifi':'wired';
 let st='off',txt='Not connected';
 if(i&&navigator.onLine!==false){if(NETST.inet===false){st='limited';txt=net31Desc(i).replace(' — connected',' — connected, no internet access detected')}else{st='on';txt=net31Desc(i)}}
 else if(i){txt='The device is offline'}
 const col=st=='on'?'#34d399':st=='limited'?'#fbbf24':'#9ca3af',dim='#4b5563';
 let svg;
 if(type=='wifi'){const b=st=='off'?3:net31Bars(i&&i.rssi),arc=['M8.5 16a5 5 0 0 1 7 0','M5 12.5a10 10 0 0 1 14 0','M2 9a15 15 0 0 1 20 0'];
  svg=arc.map((d,k)=>`<path d='${d}' stroke='${k<b?col:dim}' stroke-width='2' stroke-linecap='round' fill='none'/>`).join('')+`<circle cx='12' cy='19.5' r='1.7' fill='${col}'/>`}
 else svg=NETIC.wired.replace(/C/g,col);
 B.innerHTML=`<svg width='22' height='22' viewBox='0 0 24 24'>${svg}${st=='off'?"<path d='M3 3l18 18' stroke='#ef4444' stroke-width='2.4' stroke-linecap='round'/>":''}${st=='limited'?"<circle cx='19' cy='5' r='4.2' fill='#f59e0b'/><path d='M19 3v2.6' stroke='#111' stroke-width='1.6' stroke-linecap='round'/><circle cx='19' cy='7.2' r='.9' fill='#111'/>":''}</svg>`;
 B.title=txt+(i&&i.wireless&&i.rssi!=null?' · signal '+['weak','fair','good'][net31Bars(i.rssi)-1]:'');B.setAttribute('aria-label',B.title);B.dataset.st=st;B.dataset.type=type;
 const p=document.querySelector('.np31');if(p)net31Fill(p)}
/* the popup: the real adapters */
function net31Fill(p){const i=N31.cur;p.querySelector('.nps').textContent=i?(NETST.inet===false?'Connected — no internet access detected':'Connected'):'Not connected';
 const L=p.querySelector('.npl');L.innerHTML='';
 N31.ifs.forEach(a=>{const r=document.createElement('div');r.className='npr'+(a===i?' on':'');r.style.cursor='default';r.innerHTML="<span></span><b></b><i></i>";
  r.children[0].textContent=a.wireless?'📶':'🔌';r.children[1].textContent=a.wireless?'Wi-Fi'+(a.ssid&&a.ip?' · '+(typeof wfDecode=='function'?wfDecode(a.ssid):a.ssid):''):'Ethernet ('+a.name+')';
  r.children[2].textContent=a.ip?'Connected':a.state=='up'?'No address':a.wireless?'Not connected':'No cable';L.appendChild(r)});
 if(!N31.ifs.length)L.innerHTML="<div style='color:#6b7280'>No network adapter was found.</div>"}
function net31Pop(){const old=document.querySelector('.np');if(old){old.remove();return}const B=$('#net'),p=document.createElement('div');p.className='np np31';p.style.width='260px';
 p.innerHTML=`<div style='font-weight:700;margin-bottom:2px'>Network</div><div class='nps' style='color:#4b5563;margin-bottom:6px'></div><div class='npl'></div><div style='margin-top:8px;display:flex;gap:6px;justify-content:flex-end'><button class='npo'>Network Settings…</button></div>`;
 document.body.appendChild(p);net31Fill(p);flyPlace(p,B);p.querySelector('.npo').onclick=()=>{p.remove();settings('net')};net31Poll()}
if(RD){net31Poll();setInterval(()=>{if(!document.hidden)net31Poll()},10000);
 evOn(t=>{if(/^wifi (dis)?connected/m.test(t))net31Poll()});addEventListener('online',net31Poll);addEventListener('offline',net31Poll)}
