/* ===== 2026-11-14: Settings › Volume Controls › Microphones: which microphone, and a test =====
   A list of the microphones (on the installed system the sound server's own, rudolfd audio/mics; choosing one makes it
   the default for every app and moves a call's microphone to it, audio/mic; in the browser version the browser's
   microphones) and a Test button: while it is pressed, an equalizer shows what the chosen microphone hears (its
   frequencies, 24 bars, and a level meter), so a silent or wrong microphone is seen at once. Found with the Vivobook's
   Meet test: the default was the empty headset jack. The test stops by itself when Settings closes, when another
   microphone is chosen (it starts again on that one) and after two minutes. Nothing heard is kept or sent anywhere. */
window.__mictest59=1;
const MT={stream:null,ctx:null,raf:0,timer:0,peak:0};
function micTestStop(){try{cancelAnimationFrame(MT.raf)}catch(e){}clearTimeout(MT.timer);MT.raf=0;
 if(MT.stream){MT.stream.getTracks().forEach(t=>{try{t.stop()}catch(e){}});MT.stream=null}
 if(MT.ctx){try{MT.ctx.close()}catch(e){}MT.ctx=null}}
/* the bars: low to high frequencies, the level meter below; drawn from the analyser 30 times a second */
async function micTestStart(box,devId){micTestStop();const cv=box.querySelector('.mteq'),lv=box.querySelector('.mtlv'),st=box.querySelector('.mtst');
 const c={echoCancellation:false,noiseSuppression:false,autoGainControl:false};if(devId)c.deviceId={exact:devId};
 try{MT.stream=await navigator.mediaDevices.getUserMedia({audio:c})}catch(e){st.textContent='The microphone could not be opened: '+(e&&e.message||e);return false}
 const AC=window.AudioContext||window.webkitAudioContext;MT.ctx=new AC();const an=MT.ctx.createAnalyser();an.fftSize=512;an.smoothingTimeConstant=.6;
 MT.ctx.createMediaStreamSource(MT.stream).connect(an);const fr=new Uint8Array(an.frequencyBinCount),td=new Uint8Array(an.fftSize);
 const g=cv.getContext('2d'),N=24;let last=0;MT.peak=0;const tr=MT.stream.getAudioTracks()[0];st.textContent='Listening to '+(tr&&tr.label?tr.label:'the microphone')+'. Speak or tap near it.';
 const draw=t=>{if(!MT.stream||!cv.isConnected){micTestStop();return}MT.raf=requestAnimationFrame(draw);if(t-last<33)return;last=t;
  an.getByteFrequencyData(fr);an.getByteTimeDomainData(td);let rms=0;for(const v of td){const x=(v-128)/128;rms+=x*x}rms=Math.sqrt(rms/td.length);
  const W=cv.width,H=cv.height,bw=W/N;g.clearRect(0,0,W,H);
  /* (bands spread logarithmically over the speech range: about 80 Hz to 8 kHz) */
  const ny=MT.ctx.sampleRate/2,bin=f=>Math.min(fr.length-1,Math.max(1,Math.round(f/ny*fr.length)));
  for(let i=0;i<N;i++){const f0=80*Math.pow(100,i/N),f1=80*Math.pow(100,(i+1)/N);let m=0;for(let k=bin(f0);k<=bin(f1);k++)m=Math.max(m,fr[k]);
   const h=Math.max(2,m/255*(H-4));g.fillStyle=m>200?'#ef4444':m>140?'#f59e0b':'#22c55e';g.fillRect(i*bw+2,H-h,bw-4,h)}
  const pct=Math.min(100,Math.round(rms*300));MT.peak=Math.max(MT.peak,pct);lv.style.width=pct+'%';lv.dataset.v=pct;
  box.dataset.heard=MT.peak>=3?'1':'0'};
 MT.raf=requestAnimationFrame(draw);MT.timer=setTimeout(()=>{const b=box.querySelector('.mtgo');micTestStop();if(b){b.textContent='Test';b.setAttribute('aria-pressed','false')}st.textContent=MT.peak>=3?'Test ended: the microphone was heard.':'Test ended: nothing was heard. Choose another microphone, or check that it is not muted.'},120000);
 return true}
function micTestBox(host){
 const box=document.createElement('div');box.className='mtbox';box.style.cssText='margin-top:6px';
 box.innerHTML=`<div class='r2'><label>Microphone</label><div style='display:flex;gap:6px;align-items:center'><select class='mtsel' style='flex:1;min-width:0'><option value=''>Looking…</option></select><button class='mtgo' aria-pressed='false' title='See what the chosen microphone hears'>Test</button></div></div>
<canvas class='mteq' width='360' height='70' style='display:block;width:100%;max-width:360px;height:70px;margin:6px 0 2px;background:#0b1220;border-radius:4px' aria-label='What the microphone hears, by frequency'></canvas>
<div style='max-width:360px;height:6px;background:#1f2937;border-radius:3px;overflow:hidden'><div class='mtlv' style='height:100%;width:0;background:#22c55e;transition:width .05s'></div></div>
<div class='msg mtst' style='margin-top:4px'>Press Test and speak: the bars move with what the microphone hears.</div>`;
 host.after(box);const sel=box.querySelector('.mtsel'),go=box.querySelector('.mtgo'),st=box.querySelector('.mtst');let browserIds=false;
 const running=()=>!!MT.stream;
 const start=async()=>{go.textContent='Stop test';go.setAttribute('aria-pressed','true');const ok=await micTestStart(box,browserIds?sel.value:'');if(!ok){go.textContent='Test';go.setAttribute('aria-pressed','false')}};
 const stop=()=>{micTestStop();go.textContent='Test';go.setAttribute('aria-pressed','false');st.textContent=MT.peak>=3?'Test stopped: the microphone was heard.':'Test stopped.'};
 go.onclick=()=>running()?stop():start();
 /* the list */
 const fill=async()=>{sel.innerHTML='';
  if(RD){let r=null;try{r=await rd('audio/mics')}catch(e){}const L=(r&&r.mics)||[];
   if(L.length){L.forEach(m=>{const o=document.createElement('option');o.value=m.name;o.textContent=m.desc||m.name;if(m.default)o.selected=true;sel.appendChild(o)});return}}
  browserIds=true;let ds=[];try{ds=(await navigator.mediaDevices.enumerateDevices()).filter(d=>d.kind=='audioinput')}catch(e){}
  if(!ds.length){const o=document.createElement('option');o.value='';o.textContent='No microphone found';sel.appendChild(o);sel.disabled=true;go.disabled=true;return}
  ds.forEach((d,i)=>{const o=document.createElement('option');o.value=d.deviceId;o.textContent=d.label||'Microphone '+(i+1);sel.appendChild(o)})};
 sel.onchange=async()=>{const was=running();micTestStop();
  if(!browserIds){try{await rdPost('audio/mic?name='+encodeURIComponent(sel.value))}catch(e){st.textContent=e.message||'That microphone could not be chosen.';return}}
  st.textContent='Chosen: '+sel.options[sel.selectedIndex].textContent+(browserIds?'':' (for every app)')+'.';if(was)start()};
 fill();return box}
if(typeof volPanelS=='function'){volPanelS=__after(volPanelS,function(c,W){try{micTestStop();const h=c.querySelector('.mics');
 if(h)micTestBox(h);else{const d=document.createElement('div');d.innerHTML="<h4 class='sh4'>Microphone</h4>";c.appendChild(d);micTestBox(d)}}catch(e){console.warn('mictest59',e)}},"mictest59.js");}
