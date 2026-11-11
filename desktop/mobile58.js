/* ===== Rudolf OS 1.5.8 (plan Part 6): mobile data on the desktop.
   - A network adapter plugged in later (a hotspot's or a phone's USB tethering, a cellular modem, a USB Ethernet
     dongle) gets an address by itself (netplug, from mdev); the desktop says so ("📱 Phone connected").
   - Metered connections (cellular data with a cap): rudolfd's net/metered says whether the connection in use is
     metered and why. While it is, nothing large is downloaded by itself: system updates are found but not downloaded
     (sysupd25), Linux apps and Debian are not updated in the background (linux26, term26). The browser and apps work as
     usual. Settings › Network › Metered connection: automatic, metered or not, per network, and this month's data.
   ===== */
let MTR={metered:false,connected:false},MTR_T=0;
function netMetered(){return !!(MTR&&MTR.connected&&MTR.metered)}
async function mtrPoll(){if(!RD)return MTR;try{const j=await rd('net/metered');if(j&&j.ok!==false){const was=netMetered();MTR=j;MTR_T=Date.now();if(was!=netMetered())dispatchEvent(new CustomEvent('rudolf-metered',{detail:j}))}}catch(e){}return MTR}
const MTR_KIND={usbmodem:['📶','Cellular modem'],phone:['📱','Phone (USB tethering)'],iphone:['📱','iPhone (USB tethering)'],usb:['🔌','USB network adapter'],ethernet:['🔌','Network cable'],wifi:['📶','Wi-Fi']};
const mtrFmt=b=>b>=1e9?(b/1e9).toFixed(2)+' GB':b>=1e6?(b/1e6).toFixed(1)+' MB':Math.round((b||0)/1e3)+' KB';
const mtrWhy=j=>j.manual?'set by you for this network':j.auto=='modem'?'a cellular modem':j.auto=='phone'?'a phone\'s tethering':j.auto=='dhcp'?'the network says it is metered':'';
/* devplug's events: a network adapter came or went */
(function(){if(typeof devEvent!='function')return;const o=devEvent;devEvent=function(v){if(v&&v.t=='net'){mtrNet(v);return}return o(v)}})();
function mtrNet(v){const k=MTR_KIND[v.kind]||['🌐','Network adapter'];
 if(v.a=='add'){notify(k[0]+' '+k[1]+' connected',(v.name?v.name+' · ':'')+'getting an address'+(v.metered?' · metered: updates and other large downloads wait while you use it':''),{app:'Network',action:()=>settings('net')});
  [3000,9000].forEach(t=>setTimeout(mtrPoll,t))}
 else{notify(k[0]+' Network adapter disconnected',v.if||'',{app:'Network',quiet:true,toast:3000});setTimeout(mtrPoll,2000)}
 dispatchEvent(new CustomEvent('rudolf-net',{detail:v}))}
if(RD){setTimeout(mtrPoll,5000);setInterval(mtrPoll,120000)}
/* Settings › Network › Metered connection */
(function(){if(typeof netPanelS!='function')return;const o=netPanelS;netPanelS=function(c,W){o(c,W);if(!RD)return;
 const d=sp(c,`<h4 class='sh4'>Metered connection</h4><div class='mtr58'><div class='msg mtrnow'>Looking…</div><div class='r2' style='margin-top:6px'><label for='mtrsel'>This network</label><select id='mtrsel'><option value='auto'>Automatic</option><option value='on'>Metered</option><option value='off'>Not metered</option></select></div>
  <div class='msg' style='margin-top:6px'>On a metered connection (cellular data with a monthly cap), Rudolf OS downloads nothing large by itself: system updates are found but wait for you, and Linux apps and Debian are not updated in the background. Websites and apps work as usual. Cellular modems and phones' USB tethering are metered automatically, and so are networks that say so (Android hotspots).</div></div>`);
 const q=s=>d.querySelector(s);
 const draw=async()=>{const j=await mtrPoll();if(!d.isConnected)return;const sel=q('#mtrsel');
  if(!j.connected){q('.mtrnow').textContent='Not connected.';sel.disabled=true;return}
  const k=MTR_KIND[j.kind]||['🌐','Network'],nm=j.kind=='wifi'?String(j.key||'').replace(/^wifi:/,''):(j.name||j.if);
  q('.mtrnow').textContent=`${k[0]} ${k[1]}: ${nm} — ${j.metered?'metered ('+mtrWhy(j)+')':'not metered'+(j.manual?' (set by you for this network)':'')}${j.metered?' · '+mtrFmt(j.used)+' used this month':''}`;
  sel.disabled=false;sel.value=j.manual?(j.metered?'on':'off'):'auto';sel.dataset.if=j.if};
 q('#mtrsel').onchange=async e=>{const v=e.target.value,i=e.target.dataset.if;try{await rdPost('net/metered?if='+encodeURIComponent(i)+'&v='+v)}catch(er){notify('Network',er.message,{kind:'error'})}draw()};
 draw();const t=setInterval(()=>{if(!d.isConnected){clearInterval(t);return}draw()},30000)}})();
/* 1.5.8 (plan 7.5): the browser build (no rudolfd) as a web app on a phone or tablet: "Add to home screen" opens it
   full screen, and it opens offline (rudolf-sw.js keeps the page and its files). Never on an installed Rudolf OS. */
if(!RD&&'serviceWorker' in navigator&&(location.protocol=='https:'||/^(localhost|127\.0\.0\.1)$/.test(location.hostname)))
 addEventListener('load',()=>{navigator.serviceWorker.register('rudolf-sw.js').catch(e=>console.warn('web app:',e.message))});
