/* ===== 1.6.0: the Rudolf sounds (the owner, 2026-11-19) =====
   "Make a branded start-up chime for Rudolf OS that matches its origins as a Linux system and its desktop theme. Make a
   matching shutdown chime as well as sound effects for other parts of the desktop." Tuned by the owner in the sound
   workshop artifact (key F, speed 90 %, brightness 40, space 20, warmth 55, console ticks 50, light sweep 60, each
   sound's notes, pitch, length and loudness), then: "Add these sounds to Rudolf's sound settings as options. If the
   sound would go into a new category that doesn't currently exist on the list, add that category along with the
   existing/classic sound option, as well as a few other suitable ones that might work with the desktop's other
   selectable visual themes."
   - Settings › Volume Controls: every category gets the style "Rudolf" (added last, so a computer's chosen styles keep
     their places). Nine categories are new: Sign out, Update ready, Warnings, Battery low, Device connected, Device
     removed, Start menu, Trash emptied and Screenshot, each with Classic (the desktop's own short tones), Rudolf, Retro
     (square waves, for the Retro style), Candy (round, bubbly tones, for Candy) and Minimal (one soft sine, for
     Monochrome and Wireframe). "Use one style everywhere" sets every category at once.
   - A Rudolf sound is rendered sample by sample (rsRender), never with Web Audio's own nodes, so the native desktop
     (rudolf-native.c rs_render, the same arithmetic in C) plays exactly the same samples: the page gives it the
     sound's events (rsEvents) with the style's tones. Its ingredients: console ticks (square waves through a band-pass,
     the Linux start-up), LitePoint's point of light (a sine gliding with a band of noise), glass bells (two-operator
     FM, ratio 3.5) and a warm pad (detuned saws, low-passed), in a small room (four combs and two all-passes).
   An event: [kind, t, ...]: 'B' bell [t,f,dur,amp]; 'P' pad [t,dur,amp,f1,f2,...]; 'K' tick [t,f,amp];
   'S' sweep [t,f0,f1,dur,amp]; 'N' noise [t,f,q,dur,amp,type 0 band-pass 1 high-pass]; 'T' tone [t,f,dur,amp,wave 0 sine
   1 square 2 triangle 3 saw, fEnd (0 none)]. The globals G: [bright, warmth, grain, spark, space] (0..100). */
const RS_SR = 44100;
/* the owner's settings (the artifact's "Copy the settings", 2026-11-19 06:29) */
const RS_TUNE = {global:{key:'F',speed:90,bright:40,space:20,warmth:55,grain:50,spark:60},
 sounds:{startup:{vol:85,pitch:0,len:59,notes:'A4 D5 F#5 D6'},shutdown:{vol:80,pitch:0,len:62,notes:'A5 F#5 D5 D4'},login:{vol:80,pitch:0,len:59,notes:'A4 A5 F#5'},
  logout:{vol:74,pitch:0,len:59,notes:'A4 D5 F#4'},notify:{vol:80,pitch:0,len:107,notes:'F#5 F#6 C#6'},update:{vol:80,pitch:0,len:50,notes:'D5 E5 F#5 D6'},
  error:{vol:80,pitch:0,len:69,notes:'A3 F#3'},warning:{vol:80,pitch:0,len:100,notes:'B5'},battery:{vol:80,pitch:0,len:70,notes:'A5 G#5 G5 F#5'},
  usbin:{vol:80,pitch:0,len:100,notes:'A5 F#6 E6'},usbout:{vol:80,pitch:0,len:100,notes:'F#6 E6 A5'},open:{vol:55,pitch:4,len:70},close:{vol:55,pitch:0,len:74},
  menu:{vol:55,pitch:2,len:100},click:{vol:45,pitch:-1,len:69},trash:{vol:80,pitch:0,len:61},shot:{vol:73,pitch:0,len:61},game:{vol:55,pitch:-3,len:64}}};

/* ---------- notes ---------- */
const RS_PC = {C:0,'C#':1,D:2,'D#':3,E:4,F:5,'F#':6,G:7,'G#':8,A:9,'A#':10,B:11};
function rsMidi(n){const m=/^([A-G])(#?)(-?\d)$/.exec(String(n).trim());return m?12*(+m[3]+1)+RS_PC[m[1]+m[2]]:null}
const rsHz = m => 440*Math.pow(2,(m-69)/12);
function rsKeyShift(key){let d=['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'].indexOf(key)-2;if(d>6)d-=12;if(d<-5)d+=12;return d}

/* ---------- the recipes: the workshop's sounds, as events ---------- */
function rsEvents(k, tune){
 tune = tune || RS_TUNE; const g = tune.global, s = tune.sounds[k]; if (!s) return null;
 const sh = rsKeyShift(g.key) + (+s.pitch||0), sp = 100/(g.speed||100), L = (s.len||100)/100, a = (s.vol||0)/100;
 const notes = String(s.notes||'').split(/\s+/).map(rsMidi).filter(x=>x!=null).map(m=>m+sh), f = rsHz, E = [];
 const B=(t,fr,d,am)=>E.push(['B',t,fr,d,am]), P=(t,d,am,fs)=>E.push(['P',t,d,am,...fs]), K=(t,fr,am)=>E.push(['K',t,fr,am]),
  S=(t,f0,f1,d,am)=>E.push(['S',t,f0,f1,d,am]), N=(t,fr,q,d,am,ty)=>E.push(['N',t,fr,q,d,am,ty||0]), T=(t,fr,d,am,w,fe)=>E.push(['T',t,fr,d,am,w,fe||0]);
 const X = 2**(sh/12);
 switch (k) {
  case 'startup': { [0,.11,.22].forEach((d,i)=>K(d*sp,[880,1175,1568][i]*X,.16*a)); const t1=.36*sp; S(t1,rsHz(50+sh),rsHz(86+sh),.55*sp,.18*a);
   const t2=t1+.5*sp; P(t2-.05,2.7*L,.5*a,[38,45,50,54].map(m=>rsHz(m+sh))); notes.forEach((m,i)=>B(t2+i*.075*sp,f(m),(1.9-i*.12)*L,.3*a*(i?.82:1))); break }
  case 'shutdown': { notes.forEach((m,i)=>B(i*.17*sp,f(m),(1.2+i*.25)*L,.28*a)); const tl=notes.length*.17*sp; S(tl-.25*sp,rsHz(84+sh),rsHz(55+sh),.6*sp,.14*a);
   P(tl-.1,2.1*L,.45*a,[38,45,50].map(m=>rsHz(m+sh))); K(tl+1.55*L*sp,rsHz(69+sh),.12*a); break }
  case 'login': notes.forEach((m,i)=>B(i*.11*sp,f(m),(.9+i*.4)*L,.3*a)); S(.02,f(notes[0]??69),f(notes.at(-1)??74)*2,.18*sp,.12*a); break;
  case 'logout': notes.forEach((m,i)=>B(i*.13*sp,f(m),(.9+i*.4)*L,.27*a)); break;
  case 'notify': notes.forEach((m,i)=>B(i*.085*sp,f(m),(.7+i*.25)*L,.24*a)); break;
  case 'update': notes.forEach((m,i)=>B(i*.09*sp,f(m),(.8+i*.2)*L,.22*a)); P(.1,1.6*L,.3*a,[rsHz(50+sh),rsHz(57+sh)]); break;
  case 'error': notes.forEach((m,i)=>{T(i*.16*sp,f(m),.28*L,.32*a,2);K(i*.16*sp,f(m)*2,.06*a)}); break;
  case 'warning': { const m=notes[0]??69; T(0,f(m),.4*L,.45*a,2,f(m)*.97); B(.01,f(m+12),.5*L,.1*a); break }
  case 'battery': notes.forEach((m,i)=>T(i*.2*sp,f(m),.16*L,.26*a,0)); break;
  case 'usbin': notes.forEach((m,i)=>T(i*.09*sp,f(m),.12*L,.22*a,0)); K(0,1600,.08*a); S(0,f(notes[0]??81)/2,f(notes.at(-1)??88),.16*sp,.08*a); break;
  case 'usbout': notes.forEach((m,i)=>T(i*.09*sp,f(m),.12*L,.2*a,0)); K(.2*sp,1200,.07*a); break;
  case 'open': S(0,rsHz(62+sh),rsHz(81+sh),.16*sp*L,.9*a); T(.12*sp,rsHz(86+sh),.12*L,.16*a,0); break;
  case 'close': S(0,rsHz(81+sh),rsHz(60+sh),.16*sp*L,.85*a); break;
  case 'menu': S(0,rsHz(69+sh),rsHz(88+sh),.12*sp*L,.7*a); [0,.035,.07].forEach((d,i)=>B(.06*sp+d*sp,rsHz([86,90,93][i]+sh),.25*L,.12*a)); break;
  case 'click': N(0,2600*X,4,.018*L,2*a,0); T(0,rsHz(86+sh),.02*L,.12*a,1); break;
  case 'trash': for(let i=0;i<11;i++){const d=(i*.045+((i*37)%7)*.006)*sp;N(d,900+((i*523)%7)*450,1.6,.05*L,.5*a*(1-i/14),0)} S(.25*sp,800,180,.3*sp,.07*a); break;
  case 'shot': N(0,3200,1.2,.03*L,.38*a,1); N(.075*sp,2200,1.4,.05*L,.32*a,1); B(.13*sp,rsHz(93+rsKeyShift(g.key)),.35*L,.05*a); break;
  case 'game': N(0,1800,1.1,.04*L,1.4*a,0); T(.01,rsHz(64+sh),.06*L,.25*a,2); break;
  default: return null }
 return {G:[g.bright,g.warmth,g.grain,g.spark,g.space], E: E.map(e=>e.map(x=>typeof x=='number'?+x.toFixed(6):x))};
}
/* how long a sound's events last (s), with the room's tail */
function rsLength(R){let L=0;for(const e of R.E){const t=e[1],d={B:e[3]+.1,P:e[2],K:.06,S:e[4]+.2,N:e[4]+.05,T:e[3]+.05}[e[0]]||0;L=Math.max(L,t+d)}return Math.min(8,L+.15+R.G[4]/100*1.2)}

/* ---------- the renderer: the same arithmetic as rudolf-native.c rs_render ---------- */
function rsRender(R){
 const sr=RS_SR, N=Math.ceil(rsLength(R)*sr), out=new Float64Array(N), [bright,warmth,grain,spark,space]=R.G.map(x=>x/100);
 let seed=12345; const rnd=()=>{seed=(seed*16807)%2147483647;return seed/2147483647*2-1};
 /* an envelope as Web Audio's: from .0001 up in a straight line to peak over a, then down exponentially to .0001 over d */
 const env=(x,t,a,pk,d)=>{if(x<t||pk<=1e-4)return 0;if(x<t+a)return .0001+(pk-.0001)*(x-t)/a;if(x<t+a+d)return pk*Math.pow(.0001/pk,(x-t-a)/d);return 0};
 /* a biquad (the RBJ cookbook's): type 0 low-pass, 1 band-pass, 2 high-pass; its coefficients set again every 32 samples */
 const biq=()=>({b0:0,b1:0,b2:0,a1:0,a2:0,x1:0,x2:0,y1:0,y2:0});
 const coef=(q,ty,f,Q)=>{f=Math.min(f,sr*.45);const w=2*Math.PI*f/sr,c=Math.cos(w),al=Math.sin(w)/(2*Q),a0=1+al;
  if(ty==0){q.b0=(1-c)/2/a0;q.b1=(1-c)/a0;q.b2=(1-c)/2/a0}else if(ty==1){q.b0=al/a0;q.b1=0;q.b2=-al/a0}else{q.b0=(1+c)/2/a0;q.b1=-(1+c)/a0;q.b2=(1+c)/2/a0}
  q.a1=-2*c/a0;q.a2=(1-al)/a0};
 const run=(q,x)=>{const y=q.b0*x+q.b1*q.x1+q.b2*q.x2-q.a1*q.y1-q.a2*q.y2;q.x2=q.x1;q.x1=x;q.y2=q.y1;q.y1=y;return y};
 const wave=(w,ph)=>w==1?(ph<.5?1:-1):w==2?1-4*Math.abs(ph-.5):w==3?2*ph-1:Math.sin(2*Math.PI*ph);
 for(const e of R.E){const k=e[0],t=e[1],s0=Math.max(0,Math.floor(t*sr));
  if(k=='B'){const [,,f,d,am]=e,i0=f*(.9+bright*2.6),i1=f*.05,end=Math.min(N,Math.ceil((t+d+.1)*sr));let pc=0,pm=0,ps=0;
   for(let s=s0;s<end;s++){const x=s/sr,u=x-t,idx=u<d*.7?i0*Math.pow(i1/i0,u/(d*.7)):i1;pm+=f*3.5/sr;pm-=Math.floor(pm);
    const fi=f+Math.sin(2*Math.PI*pm)*idx;pc+=fi/sr;pc-=Math.floor(pc);ps+=f*2.756/sr;ps-=Math.floor(ps);
    out[s]+=Math.sin(2*Math.PI*pc)*env(x,t,.004,am,d)+Math.sin(2*Math.PI*ps)*env(x,t,.002,am*.18*(.4+bright),d*.35)}}
  else if(k=='P'){if(warmth<=0||e[3]*warmth<=1e-4)continue;const [,,d,am,...fs]=e,q=biq(),pk=am*warmth,att=Math.min(.6,d*.3),end=Math.min(N,Math.ceil((t+d)*sr)),ph=fs.flatMap(()=>[0,0]);
   for(let s=s0;s<end;s++){const x=s/sr,u=x-t;if((s-s0)%32==0)coef(q,0,(380+warmth*900)+((260+warmth*500)-(380+warmth*900))*Math.min(1,u/d),.4);
    let v=0;fs.forEach((f,j)=>{for(let h=0;h<2;h++){const fr=f*Math.pow(2,(h?6:-7)/1200),i=j*2+h;ph[i]+=fr/sr;ph[i]-=Math.floor(ph[i]);v+=(2*ph[i]-1)*.22/fs.length}});
    const gn=u<att?.0001+(pk-.0001)*u/att:u<d*.55?pk:pk*Math.pow(.0001/pk,(u-d*.55)/(d*.45));out[s]+=run(q,v)*gn}}
  else if(k=='K'){if(grain<=0)continue;const [,,f,am]=e,q=biq();coef(q,1,f*2,3);const end=Math.min(N,Math.ceil((t+.06)*sr));let ph=0;
   for(let s=s0;s<end;s++){ph+=f/sr;ph-=Math.floor(ph);out[s]+=run(q,ph<.5?1:-1)*env(s/sr,t,.0015,am*grain,.035)}}
  else if(k=='S'){if(spark<=0||e[5]*spark*.55<=1e-4)continue;const [,,f0,f1,d,am]=e,q=biq(),pk=am*spark,end=Math.min(N,Math.ceil((t+d+.2)*sr));let ph=0;
   for(let s=s0;s<end;s++){const x=s/sr,u=x-t,r=Math.min(1,u/d),f=f0*Math.pow(f1/f0,r);ph+=f/sr;ph-=Math.floor(ph);
    if((s-s0)%32==0)coef(q,1,f*2,6);const n=run(q,rnd());
    const g1=u<d*.6?.0001+(pk*.55-.0001)*u/(d*.6):u<d+.08?pk*.55*Math.pow(.0001/(pk*.55),(u-d*.6)/(d*.4+.08)):0,
     g2=u<d*.7?.0001+(pk*.6-.0001)*u/(d*.7):u<d+.1?pk*.6*Math.pow(.0001/(pk*.6),(u-d*.7)/(d*.3+.1)):0;
    out[s]+=Math.sin(2*Math.PI*ph)*g1+n*g2}}
  else if(k=='N'){const [,,f,Q,d,am,ty]=e,q=biq();coef(q,ty?2:1,f,Q);const end=Math.min(N,Math.ceil((t+d+.05)*sr));
   for(let s=s0;s<end;s++)out[s]+=run(q,rnd())*env(s/sr,t,.001,am,d)}
  else if(k=='T'){const [,,f,d,am,w,fe]=e,q=biq();coef(q,0,900+bright*100*40,.7071);const end=Math.min(N,Math.ceil((t+d+.05)*sr));let ph=0;
   for(let s=s0;s<end;s++){const x=s/sr,fr=fe?f*Math.pow(fe/f,Math.min(1,(x-t)/d)):f;ph+=fr/sr;ph-=Math.floor(ph);out[s]+=run(q,wave(w,ph))*env(x,t,.008,am,d)}}}
 /* the room: four combs and two all-passes (Schroeder's), mixed in by Space */
 if(space>0){const C=[1116,1188,1277,1356],fb=.7+space*.25,wet=space*.55,cb=C.map(n=>({b:new Float64Array(n),i:0,lp:0})),A=[556,441].map(n=>({b:new Float64Array(n),i:0}));
  for(let s=0;s<N;s++){const x=out[s];let y=0;for(const c of cb){const o=c.b[c.i];c.lp=o*.8+c.lp*.2;c.b[c.i]=x+c.lp*fb;c.i=(c.i+1)%c.b.length;y+=o}
   y*=.25;for(const a of A){const o=a.b[a.i];const v=-y*.5+o;a.b[a.i]=y+o*.5;a.i=(a.i+1)%a.b.length;y=v}out[s]=x+y*wet}}
 let pk=0;for(let s=0;s<N;s++)pk=Math.max(pk,Math.abs(out[s]));const sc=pk>.95?.95/pk:1,f32=new Float32Array(N);for(let s=0;s<N;s++)f32[s]=out[s]*sc;
 return f32}
if(typeof module=='object')module.exports={rsEvents,rsRender,rsLength,RS_TUNE,RS_SR};

/* ---------- Settings › Volume Controls: the new categories and styles ---------- */
const RS_NEW = [['logout','Sign out & lock',60],['update','Update ready',70],['warning','Warnings',60],['battery','Battery low',70],
 ['usbin','Device connected',60],['usbout','Device removed',60],['menu','Start menu',40],['trash','Trash emptied',60],['shot','Screenshot',60]];
/* each new category's styles: Classic (the desktop's own short sine tones), Retro (square waves: the Retro style),
   Candy (round, sliding tones: Candy), Minimal (one soft sine: Monochrome and Wireframe); Rudolf is added after */
const RS_STY = {
 logout:{Classic:[[880,0,.1],[660,.1,.22]],Retro:[[784,0,.08,'square'],[523,.09,.08,'square'],[392,.18,.16,'square']],Candy:[[1047,0,.12,'triangle',700],[700,.12,.18,'triangle',520]],Minimal:[[660,0,.18]]},
 update:{Classic:[[660,0,.1],[880,.1,.1],[1175,.2,.24]],Retro:[[523,0,.07,'square'],[659,.07,.07,'square'],[784,.14,.07,'square'],[1047,.21,.18,'square']],Candy:[[784,0,.1,'triangle',1047],[1047,.1,.1,'triangle',1319],[1319,.2,.22,'triangle',1568]],Minimal:[[988,0,.25]]},
 warning:{Classic:[[660,0,.12,'triangle'],[660,.16,.18,'triangle']],Retro:[[440,0,.09,'square'],[440,.14,.09,'square']],Candy:[[600,0,.18,'triangle',450]],Minimal:[[587,0,.2]]},
 battery:{Classic:[[880,0,.1],[740,.14,.1],[587,.28,.2]],Retro:[[880,0,.06,'square'],[660,.1,.06,'square'],[440,.2,.14,'square']],Candy:[[880,0,.12,'triangle',740],[740,.14,.12,'triangle',620],[620,.28,.2,'triangle',440]],Minimal:[[523,0,.25]]},
 usbin:{Classic:[[700,0,.07],[1050,.08,.12]],Retro:[[523,0,.05,'square'],[1047,.06,.1,'square']],Candy:[[500,0,.09,'sine',1100]],Minimal:[[880,0,.08]]},
 usbout:{Classic:[[1050,0,.07],[700,.08,.12]],Retro:[[1047,0,.05,'square'],[523,.06,.1,'square']],Candy:[[1100,0,.09,'sine',500]],Minimal:[[660,0,.08]]},
 menu:{Classic:[[900,0,.04],[1200,.04,.06]],Retro:[[1200,0,.03,'square'],[1600,.035,.04,'square']],Candy:[[700,0,.07,'sine',1300]],Minimal:[[1047,0,.04]]},
 trash:{Classic:[[400,0,.18,'sawtooth',120]],Retro:[[300,0,.12,'square',80],[200,.1,.1,'square',60]],Candy:[[900,0,.06,'triangle',300],[600,.07,.06,'triangle',200],[400,.14,.1,'triangle',120]],Minimal:[[330,0,.12]]},
 shot:{Classic:[[1800,0,.03,'square'],[1200,.06,.04,'square']],Retro:[[2400,0,.02,'square'],[1600,.05,.03,'square']],Candy:[[1400,0,.05,'sine',2200],[2200,.06,.08,'sine',1600]],Minimal:[[1568,0,.05]]}};
/* "Use one style everywhere": the nearest style each older category has */
const RS_SCHEME = {Classic:{},Rudolf:{},
 Retro:{startup:'Retro boot',login:'Pulse',error:'Triple beep',click:'Typewriter',open:'Blip',close:'Blip down',notify:'Alert beep',game:'Coin',shutdown:'Beep off'},
 Candy:{startup:'Soft bells',login:'Harp sweep',error:'Soft bonk',click:'Bubble',open:'Soft pop',close:'Soft pop',notify:'Marimba',game:'Wood tap',shutdown:'Lullaby'},
 Minimal:{startup:'Deep resonance',login:'Bright ding',error:'Low thud',click:'Soft pop',open:'Rising whoosh',close:'Falling whoosh',notify:'Gentle bell',game:'Soft click',shutdown:'Slow fade'}};
/* a Rudolf style: one marker tone (its length) carrying the events, which sfx renders (and the native shell, rs_render) */
const rsPat = k => { const R=rsEvents(k); return R ? [[0,0,+rsLength(R).toFixed(3),'sine',0,{w:'rs',G:R.G,E:R.E}]] : null };
if(typeof module=='object')Object.assign(module.exports,{rsPat,RS_NEW,RS_STY,RS_SCHEME});
(function(){
 if (typeof SNDS!='object'||typeof SPRE!='object'||typeof PAT!='object') return;
 for (const [k,n,v] of RS_NEW) { if (SNDS.some(x=>x[0]==k)) continue; SNDS.push([k,n,v]); const st=RS_STY[k];
  PAT[k]=st.Classic; SPRE[k]=[['Classic',null],['Retro',st.Retro],['Candy',st.Candy],['Minimal',st.Minimal]];
  S.snd[k]=Object.assign({on:true,vol:v},S.snd[k]) }
 for (const [k] of SNDS) { const p=rsPat(k); if (p && SPRE[k] && !SPRE[k].some(x=>x[0]=='Rudolf')) SPRE[k].push(['Rudolf',p]) }   /* (last: an account's choice is kept as its place in the list, S.snd[k].p) */
})();
/* playing a Rudolf style: rendered once (a few milliseconds to 0.2 s), kept, played with the category's volume */
const RS_BUF = new Map();
function rsBuffer(k, ctx, pat){ const m=pat[0][5], key=k+'|'+JSON.stringify(m.E).length+'|'+m.G.join(); let b=RS_BUF.get(key);
 if (!b || b.ctx!==ctx) { const f=rsRender(m); const ab=ctx.createBuffer(1,f.length,RS_SR); ab.copyToChannel(f,0); b={ctx,ab}; RS_BUF.set(key,b) } return b.ab }
const rsIs = p => Array.isArray(p) && p[0] && p[0][5] && p[0][5].w=='rs';
if (typeof sfx=='function') { const _sfx=sfx; sfx=function(n,force){
 let p=null; try{ p=sndPat(n) }catch(e){} if (!rsIs(p)) return _sfx.apply(this,arguments);
 try{ window.__a11y&&a11ySound(n) }catch(e){} const m=S.snd.master,s=S.snd[n]; if (!s) return; if (!force&&(!m.on||!s.on)) return;
 const v=(m.vol/100)*(s.vol/100); if (v<=0) return; window.__sfx.push([n,+v.toFixed(3)]);
 const c=ac(); if (!c) return; try{ const src=c.createBufferSource(), g=c.createGain(); src.buffer=rsBuffer(n,c,p); g.gain.value=v; src.connect(g); g.connect(c.destination); src.start(c.currentTime+.01) }catch(e){ console.warn('sound',e) } } }
/* (2026-11-19, the owner's Vivobook: Settings › Volume Controls was empty) an account's saved settings from before 1.6.0 have only the
   first ten sound categories, and signing in puts them in place of the whole list (core25.js userLogin), so the page stopped at the
   first new one (S.snd[k] undefined). Every category the desktop has, and the master volume, are filled in with their defaults:
   after signing in, whenever settings are applied (another window's taken in), and before the page is drawn. */
function sndFill(){ try{ if (typeof S!='object'||!S) return; if (!S.snd||typeof S.snd!='object') S.snd={};
 S.snd.master=Object.assign({on:true,vol:70},S.snd.master);
 for (const [k,,v] of SNDS) { S.snd[k]=Object.assign({on:true,vol:v},S.snd[k]); if (S.snd[k].p==null||!SPRE[k]||!SPRE[k][S.snd[k].p]) S.snd[k].p=0 }
 if (typeof SC=='object'&&SC&&SC.snd&&typeof SC.snd=='object') for (const [k] of SNDS) if (!SC.snd[k]) SC.snd[k]=cln(S.snd[k]) }catch(e){ console.warn('sounds',e) } }
sndFill();
if (typeof userLogin=='function') { const _ul=userLogin; userLogin=async function(){ const r=await _ul.apply(this,arguments); sndFill(); return r } }
if (typeof applySet=='function') { const _as=applySet; applySet=function(){ sndFill(); return _as.apply(this,arguments) } }
if (typeof volPanelS=='function') { const _vf=volPanelS; volPanelS=function(){ sndFill(); return _vf.apply(this,arguments) } }
/* the Volume Controls page: the count of categories said right, and "Use one style everywhere" */
if (typeof volPanelS=='function') { const _v=volPanelS; volPanelS=function(c,W){ const r=_v.apply(this,arguments);
 try{ const msg=[...c.querySelectorAll('.msg')].find(x=>/sound categories/.test(x.textContent)); if (msg) msg.textContent=msg.textContent.replace(/the \d+ sound categories/,`the ${SNDS.length} sound categories`);
  const d=document.createElement('div'); d.className='r2 rsall'; d.innerHTML=`<label for='rsall'>Use one style everywhere</label><div><select id='rsall'><option value=''>Choose…</option>${Object.keys(RS_SCHEME).map(x=>`<option>${x}</option>`).join('')}</select> <span class='msg rsallm'></span></div>`;
  const first=c.querySelector('.vr2'); (first?first.parentNode:c).insertBefore(d, first ? first : null);
  d.querySelector('select').onchange=e=>{ const sch=e.target.value; if (!sch) return; let n=0;
   for (const [k] of SNDS) { const want=sch=='Classic'?0:SPRE[k].findIndex(x=>x[0]==(RS_SCHEME[sch][k]||sch)); if (want<0) continue; S.snd[k].p=want; n++;
    const sel=c.querySelector(`select[data-k='${k}'][data-t='p']`); if (sel) sel.value=String(want) }
   applySet(); d.querySelector('.rsallm').textContent=`${sch} for ${n} of ${SNDS.length} categories. Press Apply to keep it.`; e.target.value=''; sfx('startup',true) } }catch(e){ console.warn(e) } return r } }
/* ---------- where the new sounds play ---------- */
/* the start menu opening (#menu shown) */
(function(){ const m=typeof document=='object'&&document.getElementById('menu'); if (!m) return; let was=getComputedStyle(m).display!='none';
 new MutationObserver(()=>{ const on=m.style.display!='none'&&getComputedStyle(m).display!='none'; if (on&&!was) try{ sfx('menu') }catch(e){} was=on }).observe(m,{attributes:true,attributeFilter:['style','class']}) })();
/* the desktop's questions before something that cannot be undone (.dlg: delete, overwrite, empty the Trash) */
if (typeof document=='object') new MutationObserver(ms=>{ for (const r of ms) for (const n of r.addedNodes) if (n.nodeType==1&&n.classList&&n.classList.contains('dlg')) { try{ sfx('warning') }catch(e){} return } }).observe(document.body,{childList:true});
/* the computer's own battery: once when it falls to 10 % on battery (again after charging past 15 %) */
let RS_BATLOW=false;
if (typeof batDraw=='function') { const _b=batDraw; batDraw=function(){ const r=_b.apply(this,arguments); try{ const b=PWR&&PWR.battery; if (!b) return r;
 const lv=b.capacity, ch=b.status=='Charging'||(PWR.ac&&b.status!='Discharging');
 if (!ch&&lv<=10&&!RS_BATLOW) { RS_BATLOW=true; notify('🪫 Battery low',`${lv}% left. Plug in the charger soon.`,{app:'Power',kind:'warn',snd:'battery'}) }
 else if (ch||lv>15) RS_BATLOW=false }catch(e){} return r } }
