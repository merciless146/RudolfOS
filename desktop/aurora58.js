/* ===== Rudolf OS 1.5.8: the northern lights over Glacier Lake at night =====
   Asked for on 2026-11-12. With the Glacier Lake dynamic background in dark mode, an aurora moves slowly across the
   sky behind the peaks and the clouds, and the lake reflects it. Its colour is the desktop's accent colour, by the
   same rules as the taskbar and the icons (Display Properties › Dynamic Backgrounds):
   - Normal: the accent colour;
   - Negative: its negative (red, green and blue each turned over);
   - Complementary: the colour opposite it on the colour wheel.
   The curtains' tops shade off to a neighbouring hue, as real aurorae do.
   How: a small canvas (400×267, a quarter of the picture) laid over the background exactly where the picture is
   drawn (the background is center/cover), behind the icons and the windows. The sky and the water's reflection
   come from a mask made with the picture (the reflection drawn twice over, as the mask dims the water a great deal) (DYN.lake.am: how much of the sky shows at each point, through the
   clouds, the moonlight and the mountains, and in the water upside down). Ten frames a second at most, eight in
   Lite; it stops while the desktop is hidden, and stands still with Reduce motion. */
Object.assign(DEFS.disp,{aur:true,aurm:'normal'});
['aur','aurm'].forEach(k=>{if(!DKEYS.includes(k))DKEYS.push(k);if(S[k]===undefined)S[k]=DEFS.disp[k];if(SC&&SC[k]===undefined)SC[k]=cln(S[k])});
/* 1.5.8 (2026-11-12): the moving parts of the dynamic backgrounds (these lights; Spirit Falls' petals and lanterns)
   run only when they can be seen and the computer can spare it. On the Vivobook they kept running behind a game in an
   Oracle tab (about 6 % of a core, and the window redrawn with them): Friday Night Funkin' ran choppy. And they ran
   over a picture of one's own when Glacier Lake was still the chosen dynamic background underneath it.
   - shown: a dynamic background is what is on the screen (not a picture of one's own, which wallpaper31 puts first);
   - seen: no window covers most of the screen, nothing is full screen, the desktop is not hidden;
   - power: stopped in power saver (and battery saver: "fewer animations"), half the frames on battery. */
const DYNOV={vis:true,slow:false,t:0};
function dynShown(){try{return !(typeof wpCss=='function'&&wpCss())&&!!S.dyn&&!!DYN[S.dyn]}catch(e){return false}}
function dynOvCheck(){let vis=!document.hidden&&!document.fullscreenElement&&dynShown();
 if(vis&&typeof wins!='undefined'){const A=innerWidth*innerHeight;for(const w of wins){const e=w&&w.el;if(!e||w.min||e.style.display=='none'||(w.ws&&typeof WS!='undefined'&&w.ws!=WS))continue;
  const r=e.getBoundingClientRect();if(r.width*r.height>=A*.6){vis=false;break}}}
 let saver=!!window.__bsaver,slow=false;try{const P=typeof PWR!='undefined'?PWR:null;if(P&&P.now&&(P.now.saver||P.now.profile=='quiet'||P.now.epp=='power'))saver=true;if(P&&P.battery&&P.battery.status=='Discharging')slow=true}catch(e){}
 DYNOV.vis=vis&&!saver;DYNOV.slow=slow}
setInterval(dynOvCheck,1000);
const AUR={el:null,cv:null,sky:null,mask:null,maskSrc:'',on:false,timer:0,t0:performance.now(),col:''};
const AUR_W=400,AUR_H=267,AUR_S=.25,AUR_WL=550;
/* the three curtains, in the picture's own coordinates (1600×1067, the water line at 550) */
const AUR_RIB=[{y0:250,amp:50,k:.0042,sp:.11,h:210,ph:0,a:.78},{y0:300,amp:34,k:.0061,sp:-.08,h:150,ph:2.1,a:.6},{y0:190,amp:30,k:.0029,sp:.06,h:130,ph:4.2,a:.45}];
function aurWanted(){try{return !!(S.aur&&S.dyn=='lake'&&DYN.lake&&DYN.lake.am&&dynShown()&&dynResolve().mode=='dark')}catch(e){return false}}
function aurColours(){const base=acVariant(S.accent||'#2563eb',S.aurm||'normal'),o=icHex2rgb(base)||[37,99,235];let[h,s,l]=icRgb2hsl(...o);
 s=Math.max(s,38);const lo=icHsl2hex(h,s,Math.max(52,Math.min(68,l+8))),hi=icHsl2hex(h+38,Math.max(30,s-8),Math.max(40,Math.min(58,l)));return{base,lo,hi}}
/* one vertical strip, bright at the curtain's lower edge, shading off upwards to the neighbouring hue */
function aurStrip(c){const s=document.createElement('canvas');s.width=1;s.height=64;const x=s.getContext('2d'),g=x.createLinearGradient(0,64,0,0);
 const rgba=(hx,a)=>{const o=icHex2rgb(hx);return`rgba(${o[0]},${o[1]},${o[2]},${a})`};
 g.addColorStop(0,rgba(c.lo,.0));g.addColorStop(.04,rgba(c.lo,1));g.addColorStop(.22,rgba(c.lo,.75));g.addColorStop(.6,rgba(c.hi,.32));g.addColorStop(1,rgba(c.hi,0));
 x.fillStyle=g;x.fillRect(0,0,1,64);return s}
function aurFit(){if(!AUR.cv)return;const vw=innerWidth,vh=innerHeight,s=Math.max(vw/1600,vh/1067),w=1600*s,h=1067*s;
 Object.assign(AUR.cv.style,{width:w+'px',height:h+'px',left:(vw-w)/2+'px',top:(vh-h)/2+'px'})}
function aurDraw(){const c=AUR.cv;if(!c||!AUR.mask||!AUR.mask.complete||!AUR.mask.naturalWidth)return;
 const col=aurColours();if(col.base!==AUR.col){AUR.col=col.base;AUR.strip=aurStrip(col)}
 const t=(performance.now()-AUR.t0)/1000*(AUR.still?0:1)+(AUR.still?40:0),k=AUR.sky.getContext('2d');
 k.setTransform(1,0,0,1,0,0);k.clearRect(0,0,AUR_W,AUR_H);k.setTransform(AUR_S,0,0,AUR_S,0,0);
 for(const r of AUR_RIB)for(let x=-24;x<1624;x+=8){
  const by=r.y0+r.amp*Math.sin(r.k*x+r.ph+t*r.sp)+12*Math.sin(.013*x-t*.4+r.ph),
   a=r.a*(.5+.5*Math.sin(.021*x+t*1.1+r.ph))*(.55+.45*Math.sin(.0057*x-t*.45))+.06,
   h=r.h*(.7+.3*Math.sin(.009*x+t*.5+r.ph));
  k.globalAlpha=Math.max(0,Math.min(1,a));k.drawImage(AUR.strip,x,by-h,9,h)}
 k.globalAlpha=1;
 const g=c.getContext('2d');g.globalCompositeOperation='source-over';g.setTransform(1,0,0,1,0,0);g.clearRect(0,0,AUR_W,AUR_H);
 g.drawImage(AUR.sky,0,0);
 /* the lake: the same light upside down about the water line (the mask keeps it to the open water and dims it) */
 g.globalCompositeOperation='lighter';g.setTransform(1,0,0,-1,0,2*AUR_WL*AUR_S);g.drawImage(AUR.sky,0,0);g.drawImage(AUR.sky,0,0);g.setTransform(1,0,0,1,0,0);
 g.globalCompositeOperation='destination-in';g.drawImage(AUR.mask,0,0,AUR_W,AUR_H);g.globalCompositeOperation='source-over'}
function aurTick(){if(document.hidden||!DYNOV.vis)return;if(DYNOV.slow&&(DYNOV.t^=1))return;aurDraw()}
function aurStop(){clearInterval(AUR.timer);AUR.timer=0}
function aurStart(){aurStop();AUR.still=!!(S.rmotion||(window.matchMedia&&matchMedia('(prefers-reduced-motion: reduce)').matches));
 aurDraw();if(!AUR.still)AUR.timer=setInterval(aurTick,document.documentElement.classList.contains('webkit')?125:100)}
function aurUpdate(){const want=aurWanted();
 if(!want){if(AUR.on){aurStop();AUR.el&&(AUR.el.style.display='none');AUR.on=false}return}
 if(!AUR.el){const d=document.createElement('div');d.id='aurora';d.setAttribute('aria-hidden','true');
  d.style.cssText='position:fixed;inset:0;overflow:hidden;pointer-events:none';
  const c=document.createElement('canvas');c.width=AUR_W;c.height=AUR_H;c.style.position='absolute';d.appendChild(c);
  document.body.insertBefore(d,document.body.firstChild);AUR.el=d;AUR.cv=c;
  AUR.sky=document.createElement('canvas');AUR.sky.width=AUR_W;AUR.sky.height=AUR_H;addEventListener('resize',aurFit)}
 if(AUR.maskSrc!==DYN.lake.am){AUR.maskSrc=DYN.lake.am;AUR.mask=new Image();AUR.mask.onload=()=>aurDraw();AUR.mask.src=DYN.lake.am}
 /* the overlay stays the first thing on the page, under everything else */
 if(document.body.firstChild!==AUR.el)document.body.insertBefore(AUR.el,document.body.firstChild);
 AUR.el.style.display='';aurFit();AUR.col='';if(!AUR.on||!AUR.timer||AUR.still!==!!(S.rmotion||(window.matchMedia&&matchMedia('(prefers-reduced-motion: reduce)').matches)))aurStart();else aurDraw();AUR.on=true}
(function(){const o=applySet;applySet=function(){const r=o.apply(this,arguments);try{aurUpdate()}catch(e){}return r}})();
(function(){const o=themeApply;themeApply=function(){const r=o.apply(this,arguments);try{if(AUR.on){AUR.col='';aurDraw()}}catch(e){}return r}})();
document.addEventListener('visibilitychange',()=>{if(!document.hidden&&AUR.on)aurDraw()});
/* Display Properties: under the dynamic backgrounds' Appearance */
(function(){const o=dispPanelS;dispPanelS=function(c,W){o(c,W);const m=c.querySelector('#dynm');if(!m)return;
 const d=document.createElement('div');d.className='aurrow';
 d.innerHTML=`<div class='acrow'>${ckH('daur','At night, with Glacier Lake',!!S.aur)} <select id='daurm' aria-label='Northern lights colour'>${ACMODES.map(([k,n])=>optH(k,n,S.aurm||'normal')).join('')}</select></div>
  <div class='msg'>The lights take the accent colour: Normal is the accent colour; Negative is its negative; Complementary is the colour opposite it on the colour wheel.</div>`;
 const lab=document.createElement('label');lab.textContent='Northern lights';m.after(lab);lab.after(d);const q=s=>d.querySelector(s),sync=()=>{q('#daurm').disabled=!q('#daur').checked};
 q('#daur').onchange=e=>{liveSet(c,'aur',e.target.checked);sync();aurUpdate()};
 q('#daurm').onchange=e=>{liveSet(c,'aurm',e.target.value);AUR.col='';aurUpdate()};sync()}})();
{const st=document.createElement('style');st.textContent='.aurrow{margin-top:6px}.aurrow .msg{margin-top:4px}';document.head.appendChild(st)}
aurUpdate();
