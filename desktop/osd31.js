/* ===== 1.4.4 (osd31): the volume and brightness keys' indicator; the system volume on the taskbar and in Settings;
   brightness on the taskbar and in Display Properties. Installed Rudolf OS only (rudolfd): the keys run
   /usr/lib/rudolf/hotkey, which writes "osd volume 45|muted|none", "osd mic muted|on", "osd brightness 60|none" to the
   events. In the browser version the taskbar's volume is the desktop's own mix, as before. ===== */
const SV={v:null,m:false,bl:null,hasBl:false};   // system volume 0-100 (null: no sound server yet), muted, brightness 0-100 (null: unknown)
function osdSvg(k,n){
 if(k=='brightness'){const on=Math.max(1,Math.ceil((n==null?100:n)/12.5));let r='';for(let i=0;i<8;i++){const a=i*Math.PI/4,x1=12+Math.cos(a)*7.5,y1=12+Math.sin(a)*7.5,x2=12+Math.cos(a)*10.5,y2=12+Math.sin(a)*10.5;r+=`<path d='M${x1.toFixed(1)} ${y1.toFixed(1)}L${x2.toFixed(1)} ${y2.toFixed(1)}' stroke='${i<on?'#fbbf24':'#6b7280'}' stroke-width='2' stroke-linecap='round'/>`}
  return `<svg width='22' height='22' viewBox='0 0 24 24' fill='none'>${r}<circle cx='12' cy='12' r='4.5' fill='#fbbf24'/></svg>`}
 if(k=='mic')return `<svg width='22' height='22' viewBox='0 0 24 24' fill='none'><rect x='9' y='3' width='6' height='11' rx='3' fill='#e5e7eb'/><path d='M6 11a6 6 0 0 0 12 0M12 17v4' stroke='#e5e7eb' stroke-width='2' stroke-linecap='round'/>${n?`<path d='M4 4l16 16' stroke='#ef4444' stroke-width='2.2' stroke-linecap='round'/>`:''}</svg>`;
 const w=['M15.5 9.5a3.6 3.6 0 0 1 0 5','M18 7a7 7 0 0 1 0 10','M20.5 4.5a10.5 10.5 0 0 1 0 15'];
 return `<svg width='22' height='22' viewBox='0 0 24 24' fill='none'><path d='M3 9h4l5-4v14l-5-4H3z' fill='#e5e7eb'/>`+(n?w.slice(0,n).map(p=>`<path d='${p}' stroke='#3b82f6' stroke-width='2' stroke-linecap='round'/>`).join(''):`<path d='M16 9l5 6M21 9l-5 6' stroke='#9ca3af' stroke-width='2' stroke-linecap='round'/>`)+`</svg>`}
const volN=()=>SV.m||!SV.v?0:SV.v<=33?1:SV.v<=66?2:3;
/* the indicator: icon, bar and percentage near the top of the screen for 1.5 s (not while a popup of its own is open) */
let osdT=0;
function osdShow(k,txt){if(document.querySelector('.svp,.bvp'))return;let o=$('#osd');
 if(!o){o=document.createElement('div');o.id='osd';o.setAttribute('role','status');o.setAttribute('aria-live','polite');o.style.cssText='position:fixed;left:50%;top:28px;transform:translateX(-50%);z-index:2000000;display:flex;align-items:center;gap:10px;padding:10px 16px;border-radius:12px;background:rgba(17,24,39,.92);color:#f9fafb;font:600 13px system-ui,sans-serif;box-shadow:0 10px 30px rgba(0,0,0,.35);pointer-events:none;min-width:230px';document.body.appendChild(o)}
 let lv=null,icon='',label='';
 if(k=='volume'){if(SV.v==null){icon=osdSvg('volume',0);label='No sound output'}else{lv=SV.m?0:SV.v;icon=osdSvg('volume',volN());label=SV.m?'Muted':SV.v+'%'}}
 else if(k=='brightness'){if(SV.bl==null){icon=osdSvg('brightness',100);label='This screen’s brightness cannot be changed here'}else{lv=SV.bl;icon=osdSvg('brightness',SV.bl);label=SV.bl+'%'}}
 else{icon=osdSvg('mic',txt);label=txt?'Microphone off':'Microphone on'}
 o.innerHTML=icon+(lv!=null?`<div style='flex:1;height:6px;border-radius:3px;background:#374151;overflow:hidden'><div style='height:100%;width:${lv}%;background:${k=='brightness'?'#fbbf24':'#3b82f6'}'></div></div>`:'')+`<span style='min-width:${lv!=null?'52px':'0'};text-align:right'></span>`;
 o.lastElementChild.textContent=label;o.style.display='flex';clearTimeout(osdT);osdT=setTimeout(()=>{o.style.display='none'},1500)}
function osdEvents(t){for(const l of String(t||'').split('\n')){const m=l.match(/^osd (volume|mic|brightness) (\S+)/);if(!m)continue;const k=m[1],v=m[2];
 if(k=='volume'){if(v=='none')SV.v=null;else if(v=='muted')SV.m=true;else{SV.v=+v;SV.m=false}volUpd();osdShow('volume')}
 else if(k=='brightness'){SV.bl=v=='none'?null:+v;briUpd();osdShow('brightness')}
 else osdShow('mic',v=='muted')}}
/* ---------- the system volume ---------- */
function sysVolApply(j){if(j&&j.volume!=null){SV.v=j.volume;SV.m=!!j.muted}volUpd()}
async function sysVolGet(){try{sysVolApply(await rd('volume'))}catch(e){SV.v=null}return SV.v}
let svBusy=0,svNext=null;
async function sysVolSet(q){svNext=q;if(svBusy)return;svBusy=1;try{while(svNext){const p=svNext;svNext=null;const s=Object.entries(p).map(([a,b])=>a+'='+b).join('&');try{sysVolApply(await rdPost('volume?'+s))}catch(e){}}}finally{svBusy=0}}
/* the taskbar's icon, its popup and the Settings rows; returns false (the desktop's own mix is drawn) without a sound server */
function sysVolUpd(){if(SV.v==null)return false;const B=$('#vol');if(!B)return true;B.innerHTML=osdSvg('volume',volN());B.dataset.lv=volN();B.title='Volume: '+(SV.m?'muted':SV.v+'%');
 document.querySelectorAll('.svp,.sysv').forEach(p=>{const r=p.querySelector('input[type=range]'),c=p.querySelector('.svc'),k=p.querySelector('input[type=checkbox]');if(r&&document.activeElement!==r)r.value=SV.v;if(c)c.textContent=SV.m?'Muted':SV.v+'%';if(k)k.checked=SV.m});return true}
function flyPlace(p,B){const r=B.getBoundingClientRect(),b=document.body.className;let x=r.left+r.width/2-p.offsetWidth/2,y=r.top-p.offsetHeight-8;
 if(/top/.test(b))y=r.bottom+8;if(/left/.test(b)){x=r.right+8;y=r.top}if(/right/.test(b)){x=r.left-p.offsetWidth-8;y=r.top}
 p.style.left=Math.max(4,Math.min(innerWidth-p.offsetWidth-4,x))+'px';p.style.top=Math.max(4,Math.min(innerHeight-p.offsetHeight-4,y))+'px';
 const kill=e=>{if(!p.contains(e.target)&&!B.contains(e.target)){p.remove();document.removeEventListener('pointerdown',kill,true)}};setTimeout(()=>document.addEventListener('pointerdown',kill,true),0)}
function sysVolPop(){const old=document.querySelector('.svp');if(old){old.remove();return}if(SV.v==null)return volPop();const B=$('#vol'),p=document.createElement('div');p.className='vp svp';
 p.innerHTML=`<div style='font-weight:700;margin-bottom:6px'>Volume</div><div style='display:flex;align-items:center;gap:8px'><input type='range' min='0' max='100' aria-label='Volume' style='flex:1;min-width:0'><span class='svc' style='width:44px;text-align:right'></span></div><label style='display:block;margin-top:8px'><input type='checkbox'> Mute</label><a href='#' style='display:block;margin-top:8px;font-size:11px'>Desktop sounds…</a>`;
 document.body.appendChild(p);sysVolUpd();flyPlace(p,B);
 const rg=p.querySelector('input[type=range]'),mu=p.querySelector('input[type=checkbox]');
 rg.oninput=()=>{SV.v=+rg.value;SV.m=false;sysVolUpd();sysVolSet({set:rg.value})};rg.onchange=()=>sfx('notify');
 mu.onchange=()=>sysVolSet({muted:mu.checked?1:0});p.querySelector('a').onclick=e=>{e.preventDefault();p.remove();settings('vol')};
 sysVolGet()}
function sysVolStep(d){sysVolSet({step:d>0?'up':'down'})}
/* Settings › Volume Controls: System volume above the desktop's own mix */
function sysVolPanel(c){if(!RD)return;const h=c.querySelector('h3');if(!h)return;const d=document.createElement('div');d.className='sysv';d.style.display='none';
 d.innerHTML=`<h4 class='sh4'>System volume</h4><div class='vr2'><label><input type='checkbox'> Mute</label><span></span><input type='range' min='0' max='100' aria-label='System volume'><span class='svc'></span><span></span></div><div class='msg' style='color:#6b7280'>The computer's own volume, the same as the volume keys: every app, game and Linux app. The desktop's sounds below are mixed into it.</div><h4 class='sh4'>Desktop sounds</h4>`;
 h.after(d);const rg=d.querySelector('input[type=range]'),mu=d.querySelector('input[type=checkbox]');
 rg.oninput=e=>{e.stopPropagation();SV.v=+rg.value;SV.m=false;sysVolUpd();sysVolSet({set:rg.value})};rg.onchange=e=>e.stopPropagation();
 mu.onchange=e=>{e.stopPropagation();sysVolSet({muted:mu.checked?1:0})};
 sysVolGet().then(v=>{if(v!=null){d.style.display='';sysVolUpd()}})}
/* ---------- brightness ---------- */
let briBtn=null;
async function briGet(){try{const j=await rd('brightness');SV.bl=j.brightness}catch(e){SV.bl=null}briUpd();return SV.bl}
function briUpd(){if(!briBtn)return;briBtn.style.display=SV.hasBl?'':'none';briBtn.innerHTML=osdSvg('brightness',SV.bl);briBtn.title='Brightness: '+(SV.bl==null?'—':SV.bl+'%');
 document.querySelectorAll('.bvp,.briv').forEach(p=>{const r=p.querySelector('input[type=range]'),c=p.querySelector('.bvc');if(SV.bl!=null){if(r&&document.activeElement!==r)r.value=SV.bl;if(c)c.textContent=SV.bl+'%'}})}
let blBusy=0,blNext=null;
async function briSet(v){blNext=v;if(blBusy)return;blBusy=1;try{while(blNext!=null){const x=blNext;blNext=null;try{await rdPost('brightness?value='+x)}catch(e){}}}finally{blBusy=0}}
function briRow(el){const rg=el.querySelector('input[type=range]');rg.oninput=e=>{e.stopPropagation();SV.bl=+rg.value;briUpd();briSet(rg.value)};rg.onchange=e=>e.stopPropagation()}
function briPop(){const old=document.querySelector('.bvp');if(old){old.remove();return}const p=document.createElement('div');p.className='vp bvp';
 p.innerHTML=`<div style='font-weight:700;margin-bottom:6px'>Brightness</div><div style='display:flex;align-items:center;gap:8px'><input type='range' min='5' max='100' aria-label='Brightness' style='flex:1;min-width:0'><span class='bvc' style='width:44px;text-align:right'></span></div>`;
 document.body.appendChild(p);briUpd();flyPlace(p,briBtn);briRow(p);briGet()}
/* Settings › Display Properties: the screen's brightness (it follows the keys) */
function briPanel(c){if(!RD||!SV.hasBl)return;const h=c.querySelector('h3');if(!h)return;const d=document.createElement('div');d.className='r2 briv';
 d.innerHTML=`<label>Brightness</label><div style='display:flex;align-items:center;gap:8px'><input type='range' min='5' max='100' aria-label='Brightness' style='flex:1;min-width:0'><span class='bvc' style='width:44px'></span></div>`;
 h.after(d);briRow(d);briGet()}
if(RD){
 evOn(osdEvents);
 (async()=>{let i=0;while(await sysVolGet()==null&&i++<24)await new Promise(r=>setTimeout(r,5000))})();
 setInterval(()=>{if(!document.hidden)sysVolGet()},60000);
 rd('info').then(i=>{SV.hasBl=!!i.backlight;const V=$('#vol');if(!SV.hasBl||!V)return;briBtn=document.createElement('button');briBtn.id='bri';briBtn.setAttribute('aria-label','Brightness');
  briBtn.style.cssText='background:#1f2937;border:0;border-radius:5px;padding:3px 7px;height:26px;display:flex;align-items:center;justify-content:center;cursor:pointer;color:#fff';
  V.insertAdjacentElement('afterend',briBtn);briBtn.onclick=briPop;
  briBtn.addEventListener('wheel',e=>{e.preventDefault();rdPost('brightness/step?d='+(e.deltaY<0?'up':'down')).then(j=>{SV.bl=j.brightness;briUpd()}).catch(()=>{})},{passive:false});
  briGet()}).catch(()=>{})}
