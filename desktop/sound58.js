/* ===== Rudolf OS 1.5.8: orchestral sound cues (asked on 2026-11-11).
   Settings › Sound: Classic chime and Soft bells (Startup), Classic and Bright ding (Login success), Classic ding and
   Gentle bell (Saves & notifications), Classic chime and Slow fade (Shutdown chime) are now orchestral: the melody on
   bells or a horn, over a string chord in bass, tenor and alto, in a small hall; each a little longer than before.
   The earlier ones stay, listed last as "… (legacy)". They keep their places in the list, so a computer that had one
   of them chosen gets its new version; the legacy ones are added at the end.
   A note: [Hz, start s, length s, wave, slide-to Hz] as before, or with a 6th element {w, g, a} for an orchestral
   voice: w 'bell' (struck partials), 'str' (strings: two detuned voices, slow attack), 'horn' (a soft brass tone);
   g its loudness (1 = a plain note's); a its attack (s). ===== */
const ORCH=(()=>{
 const C3=130.81,D3=146.83,E3=164.81,F3=174.61,G2=98,A2=110,G3=196,A3=220,B3=246.94,C4=261.63,Cs4=277.18,E4=329.63,Fs4=369.99,G4=392,
  C5=523.25,E5=659.25,G5=783.99,A5=880,D6=1174.66,C6=1046.5,E6=1318.51,A6=1760,G6=1567.98,B4=493.88,D5=587.33,A4=440,E2=82.41;
 const S_=(f,t,d,g,a)=>[f,t,d,'sine',0,{w:'str',g:g||.32,a:a||.12}],B_=(f,t,d,g)=>[f,t,d,'sine',0,{w:'bell',g:g||1}],H_=(f,t,d,g)=>[f,t,d,'sine',0,{w:'horn',g:g||.55,a:.05}];
 return{
  /* Startup: Classic chime. C-E-G-C on bells, C major in the strings, the last C held (1.9 s; was 0.8 s) */
  startup:[S_(C3,0,1.9,.4),S_(G3,.05,1.85),S_(E4,.12,1.75,.26),B_(C5,0,.6),B_(E5,.16,.6),B_(G5,.32,.7),B_(C6,.5,1.35),H_(C4,.5,1.2,.35)],
  /* Startup: Soft bells. A-E-A on bells over A major (2.1 s; was 0.85 s) */
  bells:[S_(A2,0,2.1,.4,.2),S_(E3,.08,2,.32,.2),S_(Cs4,.16,1.9,.24,.2),B_(A5,0,1.1,.9),B_(E6,.18,1.1,.8),B_(A6,.4,1.6,.7)],
  /* Login: Classic. E-A rising on a horn and bells over A major (1.2 s; was 0.3 s) */
  login:[S_(A2,0,1.2,.36,.06),S_(E3,.02,1.15,.28,.06),S_(Cs4,.05,1.1,.22,.06),H_(E5,0,.22,.5),B_(A5,.16,.95),H_(A4,.16,.9,.35)],
  /* Login: Bright ding. G on a bell over G major (1 s; was 0.3 s) */
  ding:[S_(G2,0,1,.34,.04),S_(D3,.02,.95,.26,.04),S_(B3,.04,.9,.2,.04),B_(G6,0,.9,.85),B_(G5,0,.6,.45)],
  /* Saves and notifications: Classic ding. A-D on bells over D major (0.9 s; was 0.26 s): kept short, it comes often */
  notify:[S_(D3,0,.9,.3,.03),S_(A3,.01,.88,.24,.03),S_(Fs4,.02,.85,.18,.03),B_(A5,0,.35,.8),B_(D6,.11,.75,.85)],
  /* Saves and notifications: Gentle bell. C on a bell over C major (1.3 s; was 0.5 s) */
  gentle:[S_(C3,0,1.3,.28,.08),S_(G3,.03,1.25,.22,.08),S_(E4,.06,1.2,.16,.08),B_(C6,0,1.2,.8),B_(C5,.02,.8,.35)],
  /* Shutdown: Classic chime. G-E-C-G falling on bells, strings from C to a low G under it, ending on C (2.2 s; was 0.9 s) */
  shutdown:[S_(C3,0,1.1,.36,.1),S_(G3,0,1.1,.28,.1),S_(E4,0,1.1,.2,.1),S_(G2,1,1.2,.36,.15),S_(D3,1,1.2,.26,.15),S_(B3,1,.6,.18,.15),S_(C4,1.5,.7,.2,.1),
   B_(G5,0,.45),B_(E5,.22,.45),B_(C5,.44,.5),B_(G4,.66,.7),H_(C4,1.4,.8,.35)],
  /* Shutdown: Slow fade. E-C-G slowly, over C major fading into a low C (2.4 s; was 1.2 s) */
  fade:[S_(C3,0,2.4,.38,.25),S_(G3,.05,2.2,.28,.25),S_(E4,.1,1.6,.2,.25),B_(E5,0,.6,.8),B_(C5,.4,.6,.75),B_(G4,.8,1.4,.7),S_(C3/2,1.2,1.2,.3,.3)]};
})();
(function(){
 const L=(name,pat)=>[name+' (legacy)',pat];
 const at=(k,n)=>SPRE[k].findIndex(x=>x[0]==n);
 const up=(k,n,pat,legacy)=>{const i=at(k,n);if(i<0)return;const old=SPRE[k][i][1]||PAT[k];SPRE[k][i]=[n,pat];SPRE[k].push(L(legacy||n,old))};
 up('startup','Classic chime',ORCH.startup);up('startup','Soft bells',ORCH.bells);
 up('login','Classic',ORCH.login);up('login','Bright ding',ORCH.ding);
 up('notify','Classic ding',ORCH.notify);up('notify','Gentle bell',ORCH.gentle);
 up('shutdown','Classic chime',ORCH.shutdown);up('shutdown','Slow fade',ORCH.fade);
})();
/* the small hall: a reverb made once (decaying noise, 1.6 s) */
let ORCH_HALL=null;
function orchHall(c){if(ORCH_HALL&&ORCH_HALL.context===c)return ORCH_HALL;try{const n=Math.floor(c.sampleRate*1.6),b=c.createBuffer(2,n,c.sampleRate);
  for(let ch=0;ch<2;ch++){const d=b.getChannelData(ch);let s=1234567+ch*7;for(let i=0;i<n;i++){s=(s*1103515245+12345)&0x7fffffff;d[i]=((s/0x7fffffff)*2-1)*Math.pow(1-i/n,3)}}
  const cv=c.createConvolver();cv.buffer=b;const wet=c.createGain();wet.gain.value=.28;cv.connect(wet);wet.connect(c.destination);ORCH_HALL={context:c,input:cv}}catch(e){ORCH_HALL={context:c,input:null}}return ORCH_HALL}
/* the waves: strings (a softened sawtooth), a horn (the low partials strong) */
const ORCH_WAVES=new WeakMap();
function orchWave(c,w){let m=ORCH_WAVES.get(c);if(!m){m={};ORCH_WAVES.set(c,m)}if(m[w])return m[w];const N=24,re=new Float32Array(N),im=new Float32Array(N);
 for(let k=1;k<N;k++)im[k]=w=='str'?(1/k)*Math.exp(-k/9):w=='horn'?(k<=6?[0,1,.7,.5,.35,.2,.12][k]:.06/k):0;
 return m[w]=c.createPeriodicWave(re,im)}
/* one orchestral voice into out (and the hall) */
function orchVoice(c,out,t0,[f,t,d,,,o],v){const s=t0+t,g0=.35*v*(o.g||1),hall=orchHall(c);
 const env=(g,peak,a,dec)=>{g.gain.setValueAtTime(.0001,s);g.gain.linearRampToValueAtTime(peak,s+a);g.gain.setValueAtTime(peak,s+Math.max(a,d*.55));g.gain.exponentialRampToValueAtTime(.0001,s+d+dec)};
 const send=g=>{g.connect(out);if(hall.input)g.connect(hall.input)};
 if(o.w=='bell'){
  /* struck: partials at 1, 2, 3.01 and 4.2 times, the higher ones quieter and shorter */
  [[1,1,1],[2,.42,.7],[3.01,.22,.5],[4.2,.12,.35]].forEach(([r,a,len])=>{const osc=c.createOscillator(),g=c.createGain();osc.frequency.value=f*r;
   g.gain.setValueAtTime(.0001,s);g.gain.linearRampToValueAtTime(g0*a*.6,s+.004);g.gain.exponentialRampToValueAtTime(.0001,s+d*len+.15);osc.connect(g);send(g);osc.start(s);osc.stop(s+d*len+.2)});return}
 const wave=orchWave(c,o.w),lp=c.createBiquadFilter();lp.type='lowpass';lp.frequency.value=Math.min(8000,f*(o.w=='str'?6:4));lp.Q.value=.5;
 const g=c.createGain();env(g,g0*(o.w=='str'?.55:.7),o.a||.08,d,.25);lp.connect(g);send(g);
 (o.w=='str'?[-7,7]:[0]).forEach(cents=>{const osc=c.createOscillator();osc.setPeriodicWave(wave);osc.frequency.value=f;osc.detune.value=cents;osc.connect(lp);osc.start(s);osc.stop(s+d+.3)})}
/* sfx: as before, with the orchestral voices */
(function(){const plain=sfx;sfx=function(n,force){const p=window.__a11y?sndPat(n):PAT[n];
 if(!Array.isArray(p)||!p.some(x=>x&&x[5]))return plain(n,force);
 window.__a11y&&a11ySound(n);const m=S.snd.master,s=S.snd[n];if(!s||!PAT[n])return;if(!force&&(!m.on||!s.on))return;const v=(m.vol/100)*(s.vol/100);if(v<=0)return;
 window.__sfx.push([n,+v.toFixed(3)]);const c=ac();if(!c)return;const t0=c.currentTime+.02;
 /* a gentle limiter, so the chords stay as loud as a plain note, not louder */
 const out=c.createDynamicsCompressor();out.threshold.value=-14;out.ratio.value=4;out.attack.value=.003;out.release.value=.25;out.connect(c.destination);
 for(const x of p){if(x[5])orchVoice(c,out,t0,x,v);else{const[f,t,d,ty,fe]=x;const o=c.createOscillator(),g=c.createGain();o.type=ty||'sine';o.frequency.value=f;if(fe){o.frequency.setValueAtTime(f,t0+t);o.frequency.exponentialRampToValueAtTime(fe,t0+t+d)}
  g.gain.setValueAtTime(.0001,t0+t);g.gain.linearRampToValueAtTime(.35*v,t0+t+.006);g.gain.exponentialRampToValueAtTime(.0001,t0+t+d);o.connect(g);g.connect(out);o.start(t0+t);o.stop(t0+t+d+.03)}}}})();
