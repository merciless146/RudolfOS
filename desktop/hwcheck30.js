/* ===================== Check this computer (1.2.5) =====================
   Task Manager › Hardware › Check this computer…: a guided check of about a minute, one step per screen: sound,
   microphone, network (wired speed, Wi-Fi, a download from the update server), USB, graphics and, if wanted,
   sleep. 1.2.8: keyboard, touchpad or mouse, camera, battery and screen, before sleep. The result is a pass/fail list, kept so that "Report a problem" adds it to the system report (no names,
   files, addresses or network names: only what each step found). */
const HWC_K='bd95hwcheck';
const HWC_STEPS=[['sound','Sound'],['mic','Microphone'],['net','Network'],['usb','USB'],['gfx','Graphics'],['keys','Keyboard'],['pointer','Touchpad or mouse'],['cam','Camera'],['bat','Battery'],['screen','Screen'],['sleep','Sleep']];
function hwcLast(){const r=ld(HWC_K,null);return r&&r.results?r:null}
/* one line for the system report: "sound pass, mic fail (no level), …" */
function hwcSummary(r){r=r||hwcLast();if(!r)return 'not run';const d=new Date(r.at);return HWC_STEPS.filter(([k])=>r.results[k]).map(([k,n])=>`${n.toLowerCase()} ${r.results[k].s}${r.results[k].d?' ('+r.results[k].d+')':''}`).join(', ')+' · '+d.toISOString().slice(0,16).replace('T',' ')}
/* a tone on the current output: 440 Hz then 660 Hz, a second each */
function hwcTone(){try{const a=new AudioContext(),o=a.createOscillator(),g=a.createGain();o.connect(g);g.connect(a.destination);g.gain.value=0.2;o.frequency.value=440;o.frequency.setValueAtTime(660,a.currentTime+1);o.start();o.stop(a.currentTime+2);o.onended=()=>a.close();return true}catch(e){return false}}
/* the microphone's loudest moment over ms, in dBFS (null: no microphone or not allowed) */
async function hwcMicPeak(ms,onLevel){let st;try{st=await navigator.mediaDevices.getUserMedia({audio:true})}catch(e){return{err:e.name=='NotFoundError'?'no microphone':e.name=='NotAllowedError'?'not allowed':e.message}}
 const a=new AudioContext(),src=a.createMediaStreamSource(st),an=a.createAnalyser();an.fftSize=2048;src.connect(an);const buf=new Float32Array(an.fftSize);let peak=0;const t0=performance.now();
 await new Promise(res=>{const tick=()=>{an.getFloatTimeDomainData(buf);let m=0;for(const v of buf)m=Math.max(m,Math.abs(v));peak=Math.max(peak,m);onLevel&&onLevel(m);if(performance.now()-t0<ms)requestAnimationFrame(tick);else res()};tick()});
 st.getTracks().forEach(t=>t.stop());a.close();return{db:peak>0?20*Math.log10(peak):-120}}
/* a spinning WebGL scene for ms: frames per second (null: no WebGL) */
async function hwcFps(cv,ms){const g=cv.getContext('webgl');if(!g)return null;const vs=g.createShader(g.VERTEX_SHADER);g.shaderSource(vs,'attribute vec2 p;uniform float t;void main(){float c=cos(t),s=sin(t);gl_Position=vec4(p.x*c-p.y*s,p.x*s+p.y*c,0,1);}');g.compileShader(vs);
 const fs=g.createShader(g.FRAGMENT_SHADER);g.shaderSource(fs,'precision mediump float;uniform float t;void main(){gl_FragColor=vec4(.5+.5*sin(t),.4,.8,1);}');g.compileShader(fs);const pr=g.createProgram();g.attachShader(pr,vs);g.attachShader(pr,fs);g.linkProgram(pr);g.useProgram(pr);
 const b=g.createBuffer();g.bindBuffer(g.ARRAY_BUFFER,b);const tri=[];for(let i=0;i<300;i++){const a=i/300*6.283,r=.3+.6*((i*7919)%100)/100;tri.push(Math.cos(a)*r,Math.sin(a)*r,Math.cos(a+.05)*r*.5,Math.sin(a+.05)*r*.5,0,0)}
 g.bufferData(g.ARRAY_BUFFER,new Float32Array(tri),g.STATIC_DRAW);const loc=g.getAttribLocation(pr,'p');g.enableVertexAttribArray(loc);g.vertexAttribPointer(loc,2,g.FLOAT,false,0,0);const ut=g.getUniformLocation(pr,'t');
 let n=0;const t0=performance.now();await new Promise(res=>{const f=()=>{const t=(performance.now()-t0)/1000;g.clearColor(.1,.1,.15,1);g.clear(g.COLOR_BUFFER_BIT);g.uniform1f(ut,t);g.drawArrays(g.TRIANGLES,0,tri.length/2);n++;if(performance.now()-t0<ms)requestAnimationFrame(f);else res()};f()});
 return n/((performance.now()-t0)/1000)}
/* the keyboard drawn by key position (KeyboardEvent.code); its labels come from the layout in use when Chromium
   can tell (navigator.keyboard.getLayoutMap) */
const HWC_KROWS=[['Escape','F1','F2','F3','F4','F5','F6','F7','F8','F9','F10','F11','F12'],
 ['Backquote','Digit1','Digit2','Digit3','Digit4','Digit5','Digit6','Digit7','Digit8','Digit9','Digit0','Minus','Equal','Backspace'],
 ['Tab','KeyQ','KeyW','KeyE','KeyR','KeyT','KeyY','KeyU','KeyI','KeyO','KeyP','BracketLeft','BracketRight','Backslash'],
 ['CapsLock','KeyA','KeyS','KeyD','KeyF','KeyG','KeyH','KeyJ','KeyK','KeyL','Semicolon','Quote','Enter'],
 ['ShiftLeft','KeyZ','KeyX','KeyC','KeyV','KeyB','KeyN','KeyM','Comma','Period','Slash','ShiftRight'],
 ['ControlLeft','MetaLeft','AltLeft','Space','AltRight','ControlRight','ArrowLeft','ArrowUp','ArrowDown','ArrowRight']];
const HWC_KNAMES={Escape:'Esc',Backspace:'⌫',Tab:'Tab',CapsLock:'Caps',Enter:'Enter',ShiftLeft:'Shift',ShiftRight:'Shift',ControlLeft:'Ctrl',ControlRight:'Ctrl',MetaLeft:'❖',AltLeft:'Alt',AltRight:'AltGr',Space:'Space',ArrowLeft:'←',ArrowUp:'↑',ArrowDown:'↓',ArrowRight:'→'};
const HWC_KWIDE={Backspace:2,Tab:1.5,CapsLock:1.8,Enter:2.2,ShiftLeft:2.4,ShiftRight:2.6,Space:6};
async function hwcKeyLabels(){const L={};HWC_KROWS.flat().forEach(c=>L[c]=HWC_KNAMES[c]||(/^Key/.test(c)?c.slice(3):/^Digit/.test(c)?c.slice(5):/^F\d/.test(c)?c:{Backquote:'`',Minus:'-',Equal:'=',BracketLeft:'[',BracketRight:']',Backslash:'\\',Semicolon:';',Quote:"'",Comma:',',Period:'.',Slash:'/'}[c]||c));
 try{const m=await navigator.keyboard.getLayoutMap();m.forEach((v,k)=>{if(L[k]!==undefined&&!HWC_KNAMES[k]&&!/^F\d/.test(k))L[k]=v.toUpperCase()})}catch(e){}return L}
/* battery health from rudolfd's battery reading: energy in µWh, or charge in µAh (with the design voltage in µV) */
function hwcBatHealth(b){const wh=(e,c)=>e!=null?e/1e6:c!=null&&b.voltage_min_design>0?c*b.voltage_min_design/1e12:null;
 const full=wh(b.energy_full,b.charge_full),design=wh(b.energy_full_design,b.charge_full_design);
 return{full,design,pct:full!=null&&design>0?Math.round(full/design*100):null,cycles:b.cycle_count>0?b.cycle_count:null}}
function hwCheck(){const o=document.createElement('div');o.className='natdlg hwc';const R={};let i=0;
 o.innerHTML=`<div class='natbox' role='dialog' aria-modal='true' aria-labelledby='hwct' style='max-width:520px;width:90vw'><div class='msg hwcn'></div><h3 id='hwct'></h3><div class='hwcb'></div><div class='stbtn hwcbt' style='justify-content:flex-end'></div></div>`;
 document.body.appendChild(o);const T=o.querySelector('h3'),B=o.querySelector('.hwcb'),BT=o.querySelector('.hwcbt'),N=o.querySelector('.hwcn');
 const btn=(l,f,pri)=>{const b=document.createElement('button');b.textContent=l;if(pri)b.className='pri';b.onclick=f;BT.appendChild(b);return b};
 let cleanup=null,keysMode=false;const tidy=()=>{if(cleanup){try{cleanup()}catch(e){}cleanup=null}keysMode=false};
 const set=(k,s,d)=>{R[k]={s,d:d||''}};const next=()=>{tidy();i++;show()};const close=()=>{tidy();o.remove()};
 o.onkeydown=e=>{e.stopPropagation();if(e.key=='Escape'&&!keysMode)close()};
 const steps={
  async sound(){B.innerHTML=`<p>Turn the volume up. Rudolf OS plays two tones on the speakers or headphones in use now.</p><p class='msg hws'></p>`;let s=null;try{s=await rd('system')}catch(e){}
   const cards=s?tmLines(s.audio).filter(l=>/^\s*\d/.test(l)).length:0;B.querySelector('.hws').textContent=cards?`${cards} sound card${cards>1?'s':''}. Another output (HDMI, headphones) can be chosen in Settings › Sound, then checked again.`:'No sound card was found.';
   if(!cards&&RD){set('sound','fail','no sound card');btn('Next',next,1);return}
   hwcTone();btn('Play again',()=>hwcTone());btn('No, nothing',()=>{set('sound','fail','not heard');next()});btn('Yes, I heard both',()=>{set('sound','pass');next()},1)},
  async mic(){B.innerHTML=`<p>Say something for three seconds.</p><div class='hwmb' style='height:14px;background:#e5e7eb;border-radius:7px;overflow:hidden'><i style='display:block;height:100%;width:0;background:#16a34a'></i></div><p class='msg hwmm'>Listening…</p>`;
   const bar=B.querySelector('i');const r=await hwcMicPeak(3000,m=>bar.style.width=Math.min(100,m*300)+'%');const M=B.querySelector('.hwmm');
   if(r.err){set('mic','fail',r.err);M.textContent='The microphone could not be used: '+r.err+'.';btn('Next',next,1);return}
   const heard=r.db>-45;M.textContent=heard?`Heard you (loudest ${r.db.toFixed(0)} dB).`:`Hardly anything was heard (loudest ${r.db.toFixed(0)} dB). Is the microphone muted or turned down?`;
   if(heard){set('mic','pass',r.db.toFixed(0)+' dB');btn('Next',next,1)}else{btn('Try again',()=>{BT.innerHTML='';steps.mic()});btn('Next',()=>{set('mic','fail','no level ('+r.db.toFixed(0)+' dB)');next()},1)}},
  async net(){B.innerHTML=`<p class='hwnl'>Checking the network…</p><ul class='hwnu'></ul>`;const U=B.querySelector('.hwnu'),li=t=>{const l=document.createElement('li');l.textContent=t;U.appendChild(l)};let s=null,bad=[],notes=[];try{s=await rd('system')}catch(e){}
   const nets=s?tmLines(s.net).map(l=>l.split(' ')):[];const wired=nets.filter(n=>n[5]!='wifi'),wifi=nets.filter(n=>n[5]=='wifi');
   wired.forEach(([n,st,sp,,, kind,drv])=>{const up=st=='up';li(`Wired ${n}: ${up?'connected'+(+sp>0?', '+sp+' Mb/s':''):'no cable'}`);if(up&&tmSlow(kind,st,sp,drv)){li('…slower than this chip can go: try another cable or router port.');notes.push('wired '+sp+' Mb/s, slow')}else if(up)notes.push('wired '+sp+' Mb/s')});
   if(wifi.length){try{const r=await rdPost('wifi/scan');const k=(r.raw||'').split('\n').slice(1).filter(l=>l.split('\t').length>=5).length;li(`Wi-Fi: ${k} network${k==1?'':'s'} in range`);notes.push('wifi '+k+' networks');if(!k)bad.push('no Wi-Fi networks')}catch(e){li('Wi-Fi: '+e.message);bad.push('wifi scan failed')}}
   else if(!wired.length){li('No network adapter was found.');bad.push('no adapter')}
   B.querySelector('.hwnl').textContent='Downloading from the Rudolf OS update server…';
   try{const r=await rd('speedtest');const mbps=r.bytes*8/1e6/(Math.max(r.ms,1)/1000);li(`Download: ${mbps.toFixed(mbps<10?1:0)} Mb/s (${fmtB(r.bytes)} in ${(r.ms/1000).toFixed(1)} s)`);notes.push(mbps.toFixed(0)+' Mb/s down')}catch(e){li('Download: '+e.message);bad.push('no download')}
   B.querySelector('.hwnl').textContent=bad.length?'Something is not working:':'The network works.';set('net',bad.length?'fail':'pass',(bad.length?bad.join(', ')+'; ':'')+notes.join(', '));btn('Next',next,1)},
  async usb(){let before=[];try{before=((await rd('disks')).disks||[]).map(d=>d.name)}catch(e){}
   B.innerHTML=`<p>Plug a USB stick (or any USB drive) into a USB port now.</p><p class='msg hwum'>Waiting for it… (30 seconds)</p>`;let done=false,t0=Date.now();
   btn('Skip',()=>{done=true;set('usb','skip');next()});
   while(!done&&Date.now()-t0<30000){await new Promise(r=>setTimeout(r,1500));if(done)return;let d=[];try{d=(await rd('disks')).disks||[]}catch(e){}const n=d.find(x=>!before.includes(x.name)&&x.removable);
    if(n){done=true;BT.innerHTML='';B.querySelector('.hwum').textContent=`Found ${n.model||'a USB drive'} (${fmtB(n.bytes)}). The USB port works.`;set('usb','pass',fmtB(n.bytes));btn('Next',next,1);return}
    B.querySelector('.hwum').textContent=`Waiting for it… (${Math.max(0,30-Math.round((Date.now()-t0)/1000))} seconds)`}
   if(!done){BT.innerHTML='';B.querySelector('.hwum').textContent='Nothing appeared. Try another port or another stick.';set('usb','fail','nothing appeared');btn('Next',next,1)}},
  async gfx(){B.innerHTML=`<canvas width='320' height='180' style='width:100%;max-width:320px;border-radius:6px;background:#111'></canvas><ul class='hwgu'><li>Drawing a moving picture…</li></ul>`;let s=null;try{s=await rd('system')}catch(e){}
   const fps=await hwcFps(B.querySelector('canvas'),3000),U=B.querySelector('.hwgu');U.innerHTML='';const li=t=>{const l=document.createElement('li');l.textContent=t;U.appendChild(l)};
   li('The screen is drawn by '+(RD?tmDrawn(s):'the browser'));li('Pages are drawn with '+tmPages());li(fps==null?'WebGL: not available':`Frames per second: ${fps.toFixed(0)}`);
   const soft=RD&&s&&!s.renderer,ok=fps!=null&&fps>=24;set('gfx',ok&&!soft?'pass':'fail',(soft?'processor drawing, ':'')+(fps==null?'no WebGL':fps.toFixed(0)+' fps'));
   if(soft)li('Rudolf OS is not using a graphics card. Settings › Display › Graphics card has the choices.');btn('Next',next,1)},
  async keys(){keysMode=true;const L=await hwcKeyLabels(),seen=new Set();
   B.innerHTML=`<p>Press every key once. Each one lights up when Rudolf OS sees it. (Ctrl+W, Ctrl+T, Ctrl+N and Ctrl+Q are kept by Rudolf OS: press Ctrl and those letters on their own.)</p><div class='hwkb' style='display:flex;flex-direction:column;gap:3px;font:11px sans-serif;user-select:none'>${HWC_KROWS.map(r=>`<div style='display:flex;gap:3px'>${r.map(c=>`<span data-c='${c}' style='flex:${HWC_KWIDE[c]||1};min-width:0;padding:5px 0;text-align:center;border:1px solid #9ca3af;border-radius:4px;background:#f3f4f6;color:#111'>${lxEsc(L[c])}</span>`).join('')}</div>`).join('')}</div><p class='msg hwkm'>0 of ${HWC_KROWS.flat().length} keys</p>`;
   const total=HWC_KROWS.flat().length,M=B.querySelector('.hwkm');
   const kd=e=>{if(!document.body.contains(o))return;e.preventDefault();e.stopPropagation();const el=B.querySelector(`[data-c='${e.code}']`);if(el&&!seen.has(e.code)){seen.add(e.code);el.style.background='#16a34a';el.style.color='#fff';M.textContent=`${seen.size} of ${total} keys`}};
   addEventListener('keydown',kd,true);addEventListener('keyup',kd,true);cleanup=()=>{removeEventListener('keydown',kd,true);removeEventListener('keyup',kd,true)};
   btn('Skip',()=>{set('keys','skip');next()});btn('Done',()=>{const miss=HWC_KROWS.flat().filter(c=>!seen.has(c));tidy();BT.innerHTML='';
    if(!miss.length){M.textContent='Every key works.';set('keys','pass',total+' keys');btn('Next',next,1);return}
    const names=[...new Set(miss.map(c=>L[c]))];M.textContent=`These did not answer: ${names.join(' ')}. Some keyboards do not have all of them (a right Ctrl or AltGr, for example).`;
    btn('Those are not on this keyboard',()=>{set('keys','pass',(total-miss.length)+' keys, '+miss.length+' not on it');next()});btn('Mark as a problem',()=>{set('keys','fail',miss.length+' keys did not answer: '+miss.join(' '));next()},1)},1)},
  async pointer(){const want=[['move','Move the pointer'],['left','Left click'],['right','Right click'],['scroll','Scroll (wheel or two fingers)']],got=new Set();
   B.innerHTML=`<p>Draw in the box with the touchpad or mouse, click with the left and right buttons, and scroll.</p><canvas class='hwpc' width='440' height='180' style='width:100%;max-width:440px;border:1px solid #9ca3af;border-radius:6px;background:#fff;touch-action:none'></canvas><div class='hwpl' style='display:flex;gap:6px;flex-wrap:wrap'>${want.map(([k,n])=>`<span data-k='${k}' style='padding:3px 8px;border-radius:10px;background:#e5e7eb;color:#111'>${n}</span>`).join('')}</div>`;
   const cv=B.querySelector('canvas'),g=cv.getContext('2d');let last=null;
   const mark=k=>{if(got.has(k))return;got.add(k);const el=B.querySelector(`[data-k='${k}']`);el.style.background='#16a34a';el.style.color='#fff';el.textContent='✓ '+el.textContent;if(got.size==want.length){BT.innerHTML='';set('pointer','pass');btn('Next',next,1)}};
   const pos=e=>{const r=cv.getBoundingClientRect();return[(e.clientX-r.left)*cv.width/r.width,(e.clientY-r.top)*cv.height/r.height]};
   cv.onpointermove=e=>{const p=pos(e);if(last){g.strokeStyle='#2563eb';g.lineWidth=2;g.beginPath();g.moveTo(...last);g.lineTo(...p);g.stroke()}last=p;mark('move')};
   cv.onpointerleave=()=>last=null;cv.onpointerdown=e=>{if(e.button==0)mark('left');if(e.button==2)mark('right')};cv.oncontextmenu=e=>{e.preventDefault();mark('right')};
   cv.onwheel=e=>{e.preventDefault();mark('scroll')};
   btn('Skip',()=>{set('pointer','skip');next()});btn('Something does not work',()=>{set('pointer','fail',want.filter(([k])=>!got.has(k)).map(([,n])=>n.toLowerCase()).join(', ')+' not seen');next()})},
  async cam(){let sw=null;try{sw=RD?await rd('camera'):null}catch(e){}
   if(sw&&sw.on===false){B.innerHTML='<p>The camera is switched off in Settings › Privacy, so it is not checked.</p>';set('cam','skip','switched off');btn('Next',next,1);return}
   let ds=[];try{ds=(await navigator.mediaDevices.enumerateDevices()).filter(d=>d.kind=='videoinput')}catch(e){}
   if(!ds.length){B.innerHTML='<p>No camera was found, so this step is skipped.</p>';set('cam','skip','no camera');btn('Next',next,1);return}
   if(!await permAsk('camera','Hardware check')){B.innerHTML='<p>Camera access was not allowed (Settings › Privacy).</p>';set('cam','skip','not allowed');btn('Next',next,1);return}
   B.innerHTML=`<video autoplay playsinline muted style='width:100%;max-width:440px;background:#000;border-radius:6px'></video><p class='msg hwcm'>Starting the camera…</p><p class='msg'>Nothing is recorded or saved.</p>`;
   const v=B.querySelector('video'),M=B.querySelector('.hwcm');let st=null,idx=0,frames=0;cleanup=()=>{st&&st.getTracks().forEach(t=>t.stop());st=null};
   const count=()=>{if(!st)return;frames++;v.requestVideoFrameCallback?v.requestVideoFrameCallback(count):0};
   const start=async n=>{cleanup();frames=0;try{st=await navigator.mediaDevices.getUserMedia({video:{deviceId:{exact:ds[n].deviceId}},audio:false});v.srcObject=st;v.requestVideoFrameCallback&&v.requestVideoFrameCallback(count)}catch(e){M.textContent='The camera could not be started: '+(e.name=='NotReadableError'?'another app is using it':e.message);return}
    await new Promise(r=>setTimeout(r,2500));const ok=frames>0||v.videoWidth>0;const name=ds[n].label||'Camera '+(n+1);
    M.textContent=ok?`${name}: works (${v.videoWidth}×${v.videoHeight}).`:`${name}: no picture arrived.`;set('cam',ok?'pass':'fail',ok?v.videoWidth+'×'+v.videoHeight+(ds.length>1?', '+ds.length+' cameras':''):'no picture')};
   if(ds.length>1)btn('Next camera',()=>{idx=(idx+1)%ds.length;start(idx)});btn('Next',()=>{if(!R.cam)set('cam','fail','did not start');next()},1);await start(0)},
  async bat(){let bs=[];try{bs=((await rd('stats')).battery||[]).filter(b=>/battery/i.test(b.type||''))}catch(e){}
   if(!bs.length){B.innerHTML='<p>This computer has no battery, so this step is skipped.</p>';set('bat','skip','no battery');btn('Next',next,1);return}
   const out=[],bad=[];bs.forEach(b=>{const h=hwcBatHealth(b);const st={Charging:'Charging',Discharging:'On battery',Full:'Full','Not charging':'Plugged in, not charging'}[b.status]||b.status||'';
    out.push(`<li><b>${lxEsc([b.manufacturer,b.model_name].filter(Boolean).join(' ')||b.name)}</b>: ${b.capacity>=0?b.capacity+' %':''}${st?', '+lxEsc(st):''}</li>`);
    if(h.pct!=null){out.push(`<li>Health: ${h.pct} % of its design capacity (${h.full.toFixed(1)} of ${h.design.toFixed(1)} Wh)${h.cycles?', '+h.cycles+' charge cycles':''}</li>`);if(h.pct<70){out.push('<li>It holds noticeably less than when it was new.</li>');bad.push('health '+h.pct+' %')}}
    else out.push('<li>This battery does not report its health.</li>')});
   B.innerHTML=`<ul>${out.join('')}</ul>`;const h0=hwcBatHealth(bs[0]);set('bat',bad.length?'fail':'pass',bad.length?bad.join(', '):h0.pct!=null?'health '+h0.pct+' %'+(h0.cycles?', '+h0.cycles+' cycles':''):'health not reported');btn('Next',next,1)},
  async screen(){B.innerHTML=`<p>The whole screen shows one colour at a time: black, white, red, green and blue. Look for dots that stay dark or bright. Click or press a key for the next colour.</p>`;
   btn('Skip',()=>{set('screen','skip');next()});btn('Start',()=>{const C=['#000','#fff','#f00','#0f0','#00f'];let n=0;const f=document.createElement('div');f.className='hwcfs';f.tabIndex=0;f.style.cssText='position:fixed;inset:0;z-index:2147483647;cursor:none;background:'+C[0];document.body.appendChild(f);f.focus();
    const end=()=>{f.remove();BT.innerHTML='';B.innerHTML='<p>Did the colours look even, with no spots?</p>';btn('I saw spots',()=>{set('screen','fail','spots seen');next()});btn('Looks even',()=>{set('screen','pass');next()},1)};
    const adv=e=>{e&&e.preventDefault();e&&e.stopPropagation();n++;if(n>=C.length)end();else f.style.background=C[n]};f.onclick=adv;f.onkeydown=adv;cleanup=()=>f.remove();
    try{f.requestFullscreen&&f.requestFullscreen().catch(()=>{})}catch(e){}},1)},
  async sleep(){let bat=false;try{bat=((await rd('stats')).battery||[]).length>0}catch(e){}
   B.innerHTML=`<p>${bat?'On a laptop, check that it wakes up properly:':'Optional:'} the computer sleeps for 10 seconds and wakes by itself. Save your work first.</p>`;
   btn('Skip',()=>{set('sleep','skip');next()},!bat);btn('Sleep for 10 seconds',async()=>{BT.innerHTML='';B.innerHTML='<p>Going to sleep…</p>';const t0=Date.now();
    try{await rdPost('power?action=suspend&wake=10')}catch(e){B.innerHTML='<p></p>';B.firstChild.textContent='Sleep is not available: '+e.message;set('sleep','fail','not available');btn('Next',next,1);return}
    await new Promise(r=>setTimeout(r,4000));for(let k=0;k<40&&Date.now()-t0<8000;k++)await new Promise(r=>setTimeout(r,500));
    let s=null;try{s=await rd('system')}catch(e){}const sv=s&&s.services||{},slept=Date.now()-t0>=9000;
    B.innerHTML=`<p>${slept?'Awake again.':'The computer did not seem to sleep.'} Sound: ${sv.sound?'working':'not running'}. Network: ${typeof netUp=='function'&&netUp()?'connected':'not connected'}.</p>`;
    set('sleep',slept&&sv.sound!==false?'pass':'fail',slept?'woke':'did not sleep');btn('Next',next,1)},bat)},
  done(){T.textContent='Results';N.textContent='';const rec={at:Date.now(),results:R};svj(HWC_K,rec);
   B.innerHTML=`<table class='tmt'><tbody>${HWC_STEPS.map(([k,n])=>R[k]?`<tr><td>${n}</td><td><b class='hwr ${R[k].s}'>${{pass:'✓ works',fail:'✗ problem',skip:'– skipped'}[R[k].s]}</b></td><td class='msg'>${lxEsc(R[k].d)}</td></tr>`:'').join('')}</tbody></table><p class='msg'>The result is added to the system report. Nothing personal is in it.</p>`;
   if(Object.values(R).some(x=>x.s=='fail')&&typeof sysReportDialog=='function')btn('Report a problem…',()=>{close();sysReportDialog()});btn('Done',close,1)}};
 const show=()=>{BT.innerHTML='';if(i>=HWC_STEPS.length)return steps.done();const [k,n]=HWC_STEPS[i];N.textContent=`Step ${i+1} of ${HWC_STEPS.length}`;T.textContent=n;BT.appendChild(Object.assign(document.createElement('button'),{textContent:'Stop',onclick:close}));steps[k]()};
 show();return o}
