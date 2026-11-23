/* ===== Rudolf OS 1.6.2: Desert Camp, the third moving background =====
   Asked for on 2026-11-22. An open desert with sparse dry brush and a few clusters of tents, each round its campfire,
   and one tent in front (the picture: iso/systools/backgrounds62/desert.py, nothing taken from other pictures).
   - Light (sunrise): the sun rises as a mirage on the horizon, its foot joined to its own upside-down image and the
     whole of it shimmering in the heat; smoke rises from the campfires.
   - Dark (night): the campfires burn and flicker calmly, the lit tents glow softly through their fabric, the stars
     twinkle and now and then a shooting star streaks across the sky.
   Both have a very few high clouds (in the picture).
   How: as Glacier Lake's lights (aurora58.js), a canvas (800×534, half the picture) laid over the background exactly where
   the picture is drawn (center/cover), under the icons and the windows. Everything is worked out from the time alone (no
   state carried from frame to frame), so rudolf-native.c draws the same frames from the same numbers (DS_* there).
   Ten frames a second, eight in Lite; stops while it cannot be seen or in power saver (DYNOV); stands still with
   Reduce motion. */
const DS={W:1600,H:1067,hz:640,sun:[1030,640,44],
 fires:[[760,930,1.35],[312,714,.42],[1262,698,.36],[905,680,.26]],
 /* the lit tents (x, base, width, height): their glow at night */
 lit:[[548,890,340,190],[370,706,80,45],[1204,700,70,40],[1260,684,46,26],[872,679,36,20]]};
const DSV={el:null,cv:null,on:false,timer:0,t0:performance.now(),still:false,stars:null};
const DS_CW=800,DS_CH=534,DS_S=.5;
/* a small repeatable random (the same in rudolf-native.c): x(n+1) = x(n)*1103515245+12345, top 15 bits of 31 */
function dsRand(seed){let x=seed>>>0;return()=>{x=(Math.imul(x,1103515245)+12345)>>>0;return((x>>>16)&32767)/32768}}
function dsStars(){if(DSV.stars)return DSV.stars;const r=dsRand(62),a=[];
 for(let i=0;i<140;i++){const x=r()*DS.W,y=r()*(DS.hz-40),b=.35+.65*r()*r(),w=.6+2.2*r(),p=r()*6.283;a.push([x,y,b,w,p])}return DSV.stars=a}
/* the shooting stars: one every 7 to 25 s (a cycle of 16 that repeats), each 0.9 s across a part of the sky */
/* (each: its time, where it starts, how steeply it falls (radians below level), how far it goes; it heads towards the sky's
   middle from the side it starts on) */
const DS_MET=(()=>{const r=dsRand(1622),a=[];let t=4;for(let i=0;i<16;i++){t+=7+18*r();a.push([t,120+1300*r(),40+300*r(),.2+.4*r(),260+260*r()])}return a})();
const DS_METP=DS_MET[15][0]+9;
function dsWanted(){try{return S.dyn=='desert'&&!!DYN.desert&&dynShown()}catch(e){return false}}
function dsFit(){if(!DSV.cv)return;const vw=innerWidth,vh=innerHeight,s=Math.max(vw/DS.W,vh/DS.H),w=DS.W*s,h=DS.H*s;
 Object.assign(DSV.cv.style,{width:w+'px',height:h+'px',left:(vw-w)/2+'px',top:(vh-h)/2+'px'})}
/* ---- light: the sun's mirage. Drawn in rows 2 px high: above the horizon the sun's disc, its lower part cut off by
   the heat haze; below, its upside-down image, squashed and joined to it by a neck that narrows and widens; each row
   shifted a little sideways by the shimmer. */
function dsSun(g,t){const[sx,hz,r]=DS.sun,cy=hz-r*.42,lob=r*(.42+.1*Math.sin(t*.55)+.04*Math.sin(t*1.7)),neck=r*(.34+.14*Math.sin(t*.8+1));
 /* the glow round it first, breathing a little */
 const gr=g.createRadialGradient(sx,hz-6,r*.8,sx,hz-6,r*3.4);gr.addColorStop(0,`rgba(255,170,70,${.38+.05*Math.sin(t*.9)})`);gr.addColorStop(1,'rgba(255,130,40,0)');
 g.fillStyle=gr;g.fillRect(sx-r*3.6,hz-r*3.6,r*7.2,r*3.6);
 for(let y=Math.floor(cy-r);y<hz+lob;y+=2){let hw;
  if(y<hz){const dy=y-cy;hw=dy*dy<r*r?Math.sqrt(r*r-dy*dy):0;const k=Math.max(0,1-(hz-y)/(r*.5));hw=hw*(1-k)+Math.min(hw,neck)*k}
  else{const u=(y-hz)/lob;hw=neck+(r*.98-neck)*Math.sin(Math.PI*Math.min(1,.15+u*1.05));hw*=1-Math.pow(u,5)}
  if(hw<=.5)continue;
  const sh=1.6*Math.sin(y*.45+t*4.1)+.9*Math.sin(y*.13-t*2.3),u=Math.max(0,Math.min(1,(y-(cy-r))/(hz-(cy-r)))),
   c=y<hz?[255,Math.round(252-30*u),Math.round(230-90*u)]:[255,238,186],a=y<hz?1:.96-.4*(y-hz)/lob;
  g.fillStyle=`rgba(${c[0]},${c[1]},${c[2]},${a})`;g.fillRect(sx-hw+sh,y,hw*2,2.4)}}
/* ---- light: smoke from each fire. Puffs leave every 0.6 s; each rises for 9 s, drifting with the breeze, growing and fading */
function dsSmoke(g,t){const P=9,N=15;
 for(const[fx,fy,fs]of DS.fires)for(let k=0;k<N;k++){const age=((t+k*P/N+fx*.01)%P+P)%P,u=age/P;
  const x=fx+(age*7+Math.sin(age*.9+k)*5)*fs,y=fy-6*fs-age*20*fs,rad=(5+u*26)*fs,a=.42*Math.sin(Math.PI*Math.min(1,u*1.2))*(1-u*.8);
  if(a<=.01)continue;const gr=g.createRadialGradient(x,y,0,x,y,rad);gr.addColorStop(0,`rgba(232,224,216,${a})`);gr.addColorStop(1,'rgba(232,224,216,0)');
  g.fillStyle=gr;g.fillRect(x-rad,y-rad,rad*2,rad*2)}}
/* ---- dark: the flames. Five tongues per fire, each a teardrop whose height and lean follow a few slow waves; under them a
   glow that flickers with them; a few sparks rising */
function dsFlames(g,t){
 for(const[fx,fy,fs]of DS.fires){const fl=.85+.1*Math.sin(t*7.3+fx)+.05*Math.sin(t*13.1+fy);
  const gl=g.createRadialGradient(fx,fy-4*fs,0,fx,fy-4*fs,70*fs);gl.addColorStop(0,`rgba(255,150,60,${.32*fl})`);gl.addColorStop(1,'rgba(255,120,40,0)');
  g.fillStyle=gl;g.fillRect(fx-70*fs,fy-74*fs,140*fs,110*fs);
  for(let j=0;j<5;j++){const ox=(j-2)*4.2*fs,h=fs*(16+7*Math.sin(t*5.1+j*1.7)+4*Math.sin(t*8.7+j*2.9))*(j==2?1.25:1),w=fs*(4.2+1.2*Math.sin(t*4.3+j)),
   lean=fs*(2.5*Math.sin(t*2.1+j*1.3)),bx=fx+ox,by=fy-2*fs;
   const gr=g.createLinearGradient(bx,by,bx,by-h);gr.addColorStop(0,'rgba(255,236,170,.95)');gr.addColorStop(.35,'rgba(255,170,60,.85)');gr.addColorStop(1,'rgba(220,70,20,0)');
   g.fillStyle=gr;g.beginPath();g.moveTo(bx-w,by);g.quadraticCurveTo(bx-w*.9+lean*.3,by-h*.55,bx+lean,by-h);g.quadraticCurveTo(bx+w*.9+lean*.3,by-h*.55,bx+w,by);g.closePath();g.fill()}
  for(let k=0;k<4;k++){const P=2.4,age=((t+k*P/4+fx*.003)%P+P)%P,u=age/P,x=fx+Math.sin(age*3+k*2)*6*fs,y=fy-10*fs-age*28*fs;
   g.fillStyle=`rgba(255,190,90,${.8*(1-u)})`;g.fillRect(x,y,1.6*fs+.6,1.6*fs+.6)}}}
/* ---- dark: the lit tents breathing softly, as a lantern inside them does */
function dsTents(g,t){for(const[cx,by,tw,th]of DS.lit){const a=.08+.04*Math.sin(t*1.3+cx*.01)+.02*Math.sin(t*3.7+by);
 const gr=g.createRadialGradient(cx,by-th*.35,0,cx,by-th*.35,tw*.6);gr.addColorStop(0,`rgba(255,214,140,${a})`);gr.addColorStop(1,'rgba(255,200,120,0)');
 g.fillStyle=gr;g.fillRect(cx-tw*.6,by-th*.35-tw*.6,tw*1.2,tw*1.2)}}
/* ---- dark: twinkling stars over the picture's own, and the shooting stars */
function dsStarsDraw(g,t){for(const[x,y,b,w,p]of dsStars()){const s=Math.sin(t*w+p),a=b*(.15+.85*s*s);if(a<.05)continue;
 g.fillStyle=`rgba(240,244,255,${a})`;g.fillRect(x-1,y-1,2.2,2.2);if(b>.75&&a>.6){g.fillStyle=`rgba(240,244,255,${a*.35})`;g.fillRect(x-4,y-.4,8,.9);g.fillRect(x-.4,y-4,.9,8)}}}
function dsMeteor(g,t){if(DSV.still)return;const tc=t%DS_METP;for(const[t0,x0,y0,ang,len]of DS_MET){const u=(tc-t0)/.9;if(u<0||u>1)continue;
 const dx=(x0<DS.W/2?1:-1)*Math.cos(ang),dy=Math.sin(ang),hx=x0+dx*len*u,hy=y0+dy*len*u,tl=len*.4*Math.sin(Math.PI*u),ex=hx-dx*tl,ey=hy-dy*tl;
 if(hy>DS.hz-20)continue;const gr=g.createLinearGradient(hx,hy,ex,ey);gr.addColorStop(0,`rgba(255,255,255,${.95*Math.sin(Math.PI*u)})`);gr.addColorStop(1,'rgba(200,215,255,0)');
 g.strokeStyle=gr;g.lineWidth=2;g.beginPath();g.moveTo(hx,hy);g.lineTo(ex,ey);g.stroke()}}
function dsDraw(){const c=DSV.cv;if(!c)return;let mode='light';try{mode=dynResolve().mode}catch(e){}
 const t=DSV.still?11.3:(performance.now()-DSV.t0)/1000,g=c.getContext('2d');
 g.setTransform(1,0,0,1,0,0);g.clearRect(0,0,DS_CW,DS_CH);g.setTransform(DS_S,0,0,DS_S,0,0);
 if(mode=='light'){dsSun(g,t);dsSmoke(g,t)}else{dsStarsDraw(g,t);dsMeteor(g,t);dsTents(g,t);dsFlames(g,t)}
 g.setTransform(1,0,0,1,0,0)}
function dsTick(){if(document.hidden||!DYNOV.vis)return;if(DYNOV.slow&&(DYNOV.t^=1))return;dsDraw()}
function dsStop(){clearInterval(DSV.timer);DSV.timer=0}
function dsStart(){dsStop();DSV.still=!!(S.rmotion||(window.matchMedia&&matchMedia('(prefers-reduced-motion: reduce)').matches));
 dsDraw();if(!DSV.still)DSV.timer=setInterval(dsTick,document.documentElement.classList.contains('webkit')?125:100)}
function dsUpdate(){const want=dsWanted();
 if(!want){if(DSV.on){dsStop();DSV.el&&(DSV.el.style.display='none');DSV.on=false}return}
 if(!DSV.el){const d=document.createElement('div');d.id='desertfx';d.setAttribute('aria-hidden','true');
  d.style.cssText='position:fixed;inset:0;overflow:hidden;pointer-events:none';
  const c=document.createElement('canvas');c.width=DS_CW;c.height=DS_CH;c.style.position='absolute';d.appendChild(c);
  document.body.insertBefore(d,document.body.firstChild);DSV.el=d;DSV.cv=c;addEventListener('resize',dsFit)}
 if(document.body.firstChild!==DSV.el)document.body.insertBefore(DSV.el,document.body.firstChild);
 DSV.el.style.display='';dsFit();const still=!!(S.rmotion||(window.matchMedia&&matchMedia('(prefers-reduced-motion: reduce)').matches));
 if(!DSV.on||!DSV.timer&&!still||DSV.still!==still)dsStart();else dsDraw();DSV.on=true}
(function(){applySet=__after(applySet,function(){try{dsUpdate()}catch(e){}},"desert62.js");})();
(function(){themeApply=__after(themeApply,function(){try{if(DSV.on)dsDraw()}catch(e){}},"desert62.js");})();
document.addEventListener('visibilitychange',()=>{if(!document.hidden&&DSV.on)dsDraw()});
dsUpdate();
