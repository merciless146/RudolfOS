/* ===== Rudolf OS 1.5.4: Thunderbolt and USB4 devices =====
   A device on a Thunderbolt or USB4 cable (a dock, a display, an external drive or graphics card) can read the
   computer's memory unless the IOMMU stops it, so the kernel waits for it to be approved. Where the firmware turned
   on DMA protection, Rudolf OS approves devices by itself (/usr/lib/rudolf/thunderbolt); otherwise this asks, once
   per device, and only an administrator can say yes. Approved devices are kept and approved again when plugged in.
   Settings › Devices lists them, with Forget. */
let TBQ=null,TB_ON=false;
function tbAskBox(a){let b=document.querySelector('.tb54q');
 if(!a){if(b)b.remove();TBQ=null;return}
 if(TBQ===a.t&&b)return;TBQ=a.t;if(!b){b=document.createElement('div');b.className='tb54q';document.body.appendChild(b)}
 b.style.cssText='position:fixed;right:16px;bottom:64px;z-index:99990;max-width:360px;background:#fff;color:#111827;border:1px solid #d1d5db;border-radius:10px;box-shadow:0 8px 30px rgba(0,0,0,.25);padding:14px 16px;font-size:14px';
 b.innerHTML=`<div style='font-weight:600;margin-bottom:6px'>⚡ Thunderbolt device</div><div class='tb54qt'></div><div class='msg' style='margin:6px 0 10px;font-size:12px'>Only allow docks and devices you trust: a device on this cable can read the computer's memory. An administrator's password is needed.</div><div class='tb54qb' style='display:flex;gap:8px;justify-content:flex-end'></div>`;
 b.querySelector('.tb54qt').textContent=`Allow ${a.name||'this device'}?`;
 const bb=b.querySelector('.tb54qb'),btn=(l,f,pri)=>{const x=document.createElement('button');x.textContent=l;if(pri)x.className='pri';x.onclick=f;bb.appendChild(x)};
 const done=()=>{bb.querySelectorAll('button').forEach(y=>y.disabled=true)};
 btn("Don't allow",async()=>{done();try{await rdPost('tb/deny?id='+encodeURIComponent(a.id))}catch(e){}tbAskBox(null)});
 btn('Allow',async()=>{if(!await elevate('Allow '+(a.name||'a Thunderbolt device')))return;done();
  try{await rdPost('tb/allow?id='+encodeURIComponent(a.id));notify('⚡ '+(a.name||'Thunderbolt device')+' is allowed','It is approved by itself from now on. Settings › Devices can forget it.')}catch(e){notify('Thunderbolt',e.message,{kind:'error'})}
  tbAskBox(null);dispatchEvent(new CustomEvent('rudolf-usb'))},1)}
async function tbAsk(){try{const r=await rd('tb/ask');tbAskBox(r.ask)}catch(e){}}
/* the section in Settings › Devices (devices26.js); null when this computer has no Thunderbolt or USB4 */
function tbSection(t){if(!t||!t.present)return '';
 const kept=new Set(t.devices.filter(d=>d.kept).map(d=>d.uid));
 const rows=t.devices.map(d=>`<tr><td style='width:28px'>⚡</td><td><b>${devEsc(d.name)}</b><br><small>${d.approved?'Connected':'Waiting to be allowed'}${d.kept?' · allowed by itself when plugged in':''}</small></td><td style='text-align:right;white-space:nowrap'>${d.approved?'':`<button data-tballow='${devEsc(d.id)}' data-tbname='${devEsc(d.name)}'>Allow</button> `}${d.kept?`<button data-tbforget='${devEsc(d.uid)}'>Forget</button>`:''}</td></tr>`)
  .concat(t.kept.filter(k=>!t.devices.some(d=>d.uid==k.uid)).map(k=>`<tr><td>⚡</td><td><b>${devEsc(k.name)}</b><br><small>Not plugged in · allowed by itself when plugged in</small></td><td style='text-align:right'><button data-tbforget='${devEsc(k.uid)}'>Forget</button></td></tr>`));
 const how=t.dma?'This computer protects its memory from devices on the cable (DMA protection), so devices are allowed by themselves.':'This computer does not report DMA protection, so each new device is allowed once by an administrator.';
 return `<h4 style='margin:12px 0 4px'>Thunderbolt and USB4</h4><p class='msg' style='margin:0 0 4px'>${how}</p>`+(rows.length?`<table class='dvt dvtb' style='width:100%;border-collapse:collapse'>${rows.join('')}</table>`:`<div class='msg dvtb'>None plugged in.</div>`)}
function tbWire(box,redraw){
 box.querySelectorAll('[data-tballow]').forEach(b=>b.onclick=async()=>{if(!await elevate('Allow '+b.dataset.tbname))return;b.disabled=true;try{await rdPost('tb/allow?id='+encodeURIComponent(b.dataset.tballow))}catch(e){notify('Thunderbolt',e.message,{kind:'error'})}tbAsk();redraw()});
 box.querySelectorAll('[data-tbforget]').forEach(b=>b.onclick=async()=>{if(!await elevate('Forget a Thunderbolt device'))return;b.disabled=true;try{await rdPost('tb/forget?uid='+encodeURIComponent(b.dataset.tbforget))}catch(e){notify('Thunderbolt',e.message,{kind:'error'})}redraw()})}
if(RD){evOn(t=>{if(/^tb /m.test(t)){TB_ON=true;tbAsk();dispatchEvent(new CustomEvent('rudolf-usb'))}});
 setTimeout(async()=>{try{TB_ON=!!(await rd('tb')).present}catch(e){}if(TB_ON)tbAsk()},5000);
 setInterval(()=>{if(TB_ON)tbAsk()},4000)}
