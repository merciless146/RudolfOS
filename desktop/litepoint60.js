/* ===== Rudolf OS 1.6.0 (docs/plan-1.6.0.md Part 17): the window transitions, "Splitting–Merging LitePoint" =====
   The owner's design (2026-11-17): points of light split from one place on a window's frame, draw its edges and meet at
   the opposite corner or side in a red spark; the window then fills out from there. Minimizing and closing take the
   frame back; maximizing and restoring grow or gather it. At the speeds the owner chose in the demo ("that's the exact
   right speed it needs to run at"):
     open, corner to corner (from the start menu, the desktop, a pinned app, a file) 160 ms;
     open, side to side (a window another window opened: a dialog, Settings at a page)  220 ms;
     maximize 160 ms; minimize 160 ms (the point drops into the window's taskbar button; restoring plays it backward);
     restore from maximized 160 ms; close 160 ms (2026-11-19; were 200, 180, 200, 180 and 200).
   Display Properties › Colours and theme: "The window transitions (LitePoint)" among the places the accent colour goes
   (white when not ticked), Normal / Negative / Complementary as the others, and the light's brightness (0: no light, the
   window still fills, clears and drops on the same timing; 100: the demo's). The red spark stays red. Reduce motion
   (and the battery saver): no transitions.
   lpScene() says what is where at a moment of a transition (the lines, the points, the part of the window shown); the
   native desktop has the same, in C (iso/sys/litepoint.h), and iso/qt/shell2/litepoint60_t.js compares the two. */
Object.assign(DEFS.disp,{lpac:false,lpacm:'normal',lpbr:100});
['lpac','lpacm','lpbr'].forEach(k=>{if(!DKEYS.includes(k))DKEYS.push(k);if(S[k]===undefined)S[k]=DEFS.disp[k];if(SC&&SC[k]===undefined)SC[k]=cln(S[k])});
/* (Part 17b, 2026-11-18) the start menu opening (smo) and closing (smc), the taskbar's pop-ups (pop), right-click menus (ctx), a
   window's taskbar button (tbb): moving away from the taskbar's side, the menus where they are */
/* (2026-11-19, the owner: "Anything currently at 200 or 180 ms, bring it down to 160 ms. Anything currently at 160 or 140 down to 120,
   and anything at 120 or 100 down to 80." The side-to-side open, 220, was not in the list and stays) */
const LP_MS={c2c:160,s2s:220,max:160,min:160,res:160,cls:160,smo:120,smc:120,pop:120,ctx:80,tbb:80};
/* each transition's steps, as shares of its time */
const LP_W={c2c:[.07,.29,.29,.35],s2s:[.06,.2,.22,.2,.32],max:[.07,.3,.3,.33],min:[.3,.22,.18,.3],res:[.3,.36,.34],cls:[.32,.3,.38],
 smo:[.15,.22,.24,.14,.25],smc:[.3,.22,.18,.3],pop:[.26,.28,.16,.3],ctx:[.07,.29,.29,.35],tbb:[.3,.3,.4]};
const lpE=t=>t<.5?4*t*t*t:1-Math.pow(-2*t+2,3)/2;   /* in-out cubic */
const lpL=(a,b,t)=>a+(b-a)*t,lpC=(v,a=0,b=1)=>Math.max(a,Math.min(b,v)),lpP=(A,B,t)=>[lpL(A[0],B[0],t),lpL(A[1],B[1],t)];
const lpR=(A,B,t)=>({x:lpL(A.x,B.x,t),y:lpL(A.y,B.y,t),w:lpL(A.w,B.w,t),h:lpL(A.h,B.h,t)});
function lpStep(kind,t,k){const w=LP_W[kind];let a=0;for(let i=0;i<k;i++)a+=w[i];return lpC((t-a)/w[k])}
function lpAt(kind,t){const w=LP_W[kind];let a=0;for(let i=0;i<w.length;i++){const b=a+w[i];if(t<=b||i==w.length-1)return{i,p:lpC((t-a)/w[i])};a=b}}
/* the three drawn on a frame: where the points start (o), where they meet (m), each step's lines in the frame's 0..1 */
const LP_FR={
 c2c:{o:[[0,0]],m:[[1,1]],from:[1,1],segs:[null,[[[0,0],[1,0]],[[0,0],[0,1]]],[[[1,0],[1,1]],[[0,1],[1,1]]],null]},
 s2s:{o:[[0,.5]],m:[[1,.5]],from:[1,.5],segs:[null,[[[0,.5],[0,0]],[[0,.5],[0,1]]],[[[0,0],[1,0]],[[0,1],[1,1]]],[[[1,0],[1,.5]],[[1,1],[1,.5]]],null]},
 max:{o:[[.5,0],[.5,1]],m:[[0,.5],[1,.5]],segs:[null,[[[.5,0],[0,0]],[[.5,0],[1,0]],[[.5,1],[0,1]],[[.5,1],[1,1]]],[[[0,0],[0,.5]],[[0,1],[0,.5]],[[1,0],[1,.5]],[[1,1],[1,.5]]],null]}};
/* (Part 17b) a place in a menu or window as the taskbar sees it: a along the taskbar's side, b away from it (0 at the side facing
   the taskbar); dir: the taskbar's side, 0 bottom, 1 top, 2 left, 3 right */
function lpNP(R,dir,a,b){return dir==1?[R.x+a*R.w,R.y+b*R.h]:dir==2?[R.x+b*R.w,R.y+a*R.h]:dir==3?[R.x+(1-b)*R.w,R.y+a*R.h]:[R.x+a*R.w,R.y+(1-b)*R.h]}
function lpNR(R,dir,a0,a1,b0,b1){const[x0,y0]=lpNP(R,dir,a0,b0),[x1,y1]=lpNP(R,dir,a1,b1);return{x:Math.min(x0,x1),y:Math.min(y0,y1),w:Math.abs(x1-x0),h:Math.abs(y1-y0)}}
/* KIND at T (0..1), G: f the window's (or menu's) rectangle (for max: as it was; for res: as it will be), s the maximized one,
   b the button's middle it comes from or goes to, dir the taskbar's side, corner a right-click menu's corner at the pointer (0
   top-left, 1 top-right, 2 bottom-right, 3 bottom-left). L: lines [x1,y1,x2,y2,alpha]; P: points [x,y,size,red]; geo: the window's geometry, 'from'
   (as it was) or 'to' (as it is after); clip: the part of it shown (null: all of it); lit: the taskbar button lit (0..1) */
function lpScene(kind,t,G){
 const o={L:[],P:[],geo:'to',clip:null,lit:0},line=(A,B,a=1)=>{if(a>0)o.L.push([A[0],A[1],B[0],B[1],Math.min(1,a)])},light=(A,k=1,r=0)=>o.P.push([A[0],A[1],k,lpC(r)]);
 const rect=(R,a=1)=>{const A=[R.x,R.y],B=[R.x+R.w,R.y],C=[R.x+R.w,R.y+R.h],D=[R.x,R.y+R.h];line(A,B,a);line(B,C,a);line(C,D,a);line(D,A,a)};
 const Z=R=>({x:R.x,y:R.y,w:0,h:0});
 if(LP_FR[kind]||kind=='ctx'){const F=kind=='max'?G.s:G.f,d=LP_FR[kind=='ctx'?'c2c':kind],cn=kind=='ctx'?G.corner|0:0,fx=cn==1||cn==2,fy=cn==2||cn==3,
   pt=([a,b])=>[F.x+(fx?1-a:a)*F.w,F.y+(fy?1-b:b)*F.h],n=LP_W[kind].length,{i,p}=lpAt(kind,t),last=i==n-1;
  if(kind=='max'){o.geo=last?'to':'from';if(last&&t<1)o.clip=lpR(G.f,G.s,lpE(p))}
  else if(t<1){if(last){const[ox,oy]=pt(d.from),e=lpE(p),x0=lpL(ox,F.x,e),y0=lpL(oy,F.y,e),x1=lpL(ox,F.x+F.w,e),y1=lpL(oy,F.y+F.h,e);
   o.clip={x:Math.min(x0,x1),y:Math.min(y0,y1),w:Math.abs(x1-x0),h:Math.abs(y1-y0)}}else o.clip=Z(F)}
  if(t>=1)return o;
  const fade=last?1-lpC((p-.55)/.45):1;
  for(let k=1;k<=i;k++){const q=d.segs[k];if(!q)continue;const pp=k<i?1:lpE(p);for(const[a,b]of q){const A=pt(a);line(A,lpP(A,pt(b),pp),fade)}}
  if(i==0)d.o.forEach(a=>light(pt(a),lpE(p)*1.15));
  else if(!last){const e=lpE(p);d.segs[i].forEach(([a,b])=>light(lpP(pt(a),pt(b),e)));if(i==n-2&&p>.85)d.m.forEach(m=>light(pt(m),1+(p-.85)*6,(p-.85)/.15))}
  else{const k=1-lpC(p/.5);if(k>0)d.m.forEach(m=>light(pt(m),lpL(.4,1.9,k),k))}
  return o}
 const q=k=>lpStep(kind,t,k);
 if(kind=='min'||kind=='smc'){const f=G.f,dr=G.dir|0,N=(a,b)=>lpNP(f,dr,a,b),BM=N(.5,0),p1=lpE(q(0)),p2=lpE(q(1)),p3=lpE(q(2)),p4=lpE(q(3));
  if(t<=0)return o;if(t>=1){o.clip=Z(f);return o}
  o.clip=t<.3?lpNR(f,dr,.5*p1,1-.5*p1,0,1-p1):Z(f);
  if(t>=.7)o.lit=lpC((q(3)-.75)/.25)*(1-lpC((t-.985)/.015));
  if(t<.3)rect(f,lpC(t/.08));
  else if(t<.52){const b=lpL(1,0,p2),A=N(0,b),B=N(1,b);line(A,N(0,0));line(B,N(1,0));line(N(0,0),N(1,0));light(A);light(B)}
  else if(t<.7){const A=N(lpL(0,.5,p3),0),B=N(lpL(1,.5,p3),0);line(A,B);light(A);light(B);if(q(2)>.8)light(BM,1+(q(2)-.8)*4,(q(2)-.8)/.2)}
  else{const P=lpP(BM,G.b,p4);line(BM,P,1-p4);light(P,lpL(1.4,.8,p4),1-lpC(p4/.4))}
  return o}
 /* (Part 17b) the start menu and the taskbar's pop-ups opening, a taskbar button's outline: from the side toward the taskbar out to
    the corners, along both sides away from it, meeting in the middle of the far side; then filled from the taskbar's side */
 if(kind=='smo'||kind=='pop'||kind=='tbb'){const R=G.f,dr=G.dir|0,k0=kind=='smo'?1:0,tb=kind=='tbb',N=(a,b)=>lpNP(R,dr,a,b),w=LP_W[kind],st=[0];w.forEach((x,j)=>st.push(st[j]+x));
  const aA=kind=='pop'?lpC(dr<2?(R.w>0?(G.b[0]-R.x)/R.w:.5):(R.h>0?(G.b[1]-R.y)/R.h:.5)):.5,A=N(aA,0),kk=tb?.5:1;
  if(!tb)o.clip=Z(R);if(t<=0)return o;if(t>=1){o.clip=null;return o}
  if(k0&&t<st[1]){const p=lpE(q(0)),P=lpP(G.b,A,p);line(G.b,P,1-p*.4);light(P,lpL(1.4,1,p));return o}
  const s1=lpE(q(k0)),s2=lpE(q(k0+1)),s3=lpE(q(k0+2)),sf=tb?0:q(k0+3),fade=tb?1-lpC((q(2)-.7)/.3):1-lpC((sf-.55)/.45);
  if(!tb&&t>=st[k0+3])o.clip=lpNR(R,dr,0,1,0,lpE(sf));
  line(A,N(lpL(aA,0,s1),0),fade);line(A,N(lpL(aA,1,s1),0),fade);
  if(t>=st[k0+1]){line(N(0,0),N(0,s2),fade);line(N(1,0),N(1,s2),fade)}
  if(t>=st[k0+2]){line(N(0,1),N(lpL(0,.5,s3),1),fade);line(N(1,1),N(lpL(1,.5,s3),1),fade)}
  if(t<st[k0+1]){light(N(lpL(aA,0,s1),0),kk);light(N(lpL(aA,1,s1),0),kk)}
  else if(t<st[k0+2]){light(N(0,s2),kk);light(N(1,s2),kk)}
  else if(tb){const m=q(2);if(m<.6){light(N(lpL(0,.5,s3),1),kk);light(N(lpL(1,.5,s3),1),kk)}else{const z=(m-.6)/.4;light(N(.5,1),kk*lpL(1.4,.3,z),1-z*.3)}}
  else if(t<st[k0+3]){light(N(lpL(0,.5,s3),1));light(N(lpL(1,.5,s3),1));if(q(k0+2)>.8)light(N(.5,1),1+(q(k0+2)-.8)*4,(q(k0+2)-.8)/.2)}
  else{const z=1-lpC(sf/.5);if(z>0)light(N(.5,1),lpL(.4,1.6,z),z)}
  return o}
 if(kind=='res'){const S=G.s,N=G.f,p1=lpE(q(0)),p2=lpE(q(1)),p3=lpE(q(2));o.geo=t<1?'from':'to';
  if(t<=0||t>=1)return o;
  if(t>=.66)o.clip=lpR(S,N,p3);
  const Sc=[[S.x,S.y],[S.x+S.w,S.y],[S.x+S.w,S.y+S.h],[S.x,S.y+S.h]],Nc=[[N.x,N.y],[N.x+N.w,N.y],[N.x+N.w,N.y+N.h],[N.x,N.y+N.h]],ML=[S.x,S.y+S.h/2],MR=[S.x+S.w,S.y+S.h/2];
  if(t<.3){if(q(0)<.2){light(ML,q(0)*5);light(MR,q(0)*5)}const P=[lpP(ML,Sc[0],p1),lpP(ML,Sc[3],p1),lpP(MR,Sc[1],p1),lpP(MR,Sc[2],p1)];
   line(ML,P[0]);line(ML,P[1]);line(MR,P[2]);line(MR,P[3]);P.forEach(a=>light(a));if(q(0)>.9){line(Sc[0],Sc[1],(q(0)-.9)*10);line(Sc[3],Sc[2],(q(0)-.9)*10)}return o}
  const g=t<.66?p2:1,Q=Sc.map((a,i)=>lpP(a,Nc[i],g)),fade=t<.66?1:1-lpC((q(2)-.5)/.5);
  for(let i=0;i<4;i++)line(Q[i],Q[(i+1)%4],fade);
  if(t<.66){Q.forEach(a=>light(a));if(q(1)>.85)Q.forEach(a=>light(a,1+(q(1)-.85)*4,(q(1)-.85)/.15))}
  else{const k=1-lpC(q(2)/.5);if(k>0)Nc.forEach(a=>light(a,lpL(.4,1.5,k),k))}
  return o}
 if(kind=='cls'){const f=G.f,my=f.y+f.h/2,ML=[f.x,my],MR=[f.x+f.w,my],C=[f.x+f.w/2,my],p1=lpE(q(0)),p2=lpE(q(1)),p3=lpE(q(2));
  if(t<=0)return o;o.clip=Z(f);if(t>=1)return o;
  if(t<.32){o.clip={x:f.x,y:lpL(f.y,my,p1),w:f.w,h:lpL(f.h,0,p1)};rect(f,lpC(t/.08));
   if(q(0)>.8){const k=(q(0)-.8)*5;[[f.x,f.y],[f.x+f.w,f.y],[f.x,f.y+f.h],[f.x+f.w,f.y+f.h]].forEach(a=>light(a,k))}return o}
  if(t<.62){const top=lpL(f.y,my,p2),bot=lpL(f.y+f.h,my,p2);rect({x:f.x,y:top,w:f.w,h:bot-top});[[f.x,top],[f.x+f.w,top],[f.x,bot],[f.x+f.w,bot]].forEach(a=>light(a));return o}
  const A=lpP(ML,C,Math.min(1,p3/.7)),B=lpP(MR,C,Math.min(1,p3/.7));
  if(p3<.7){line(A,B);light(A);light(B)}else{const k=1-lpC((q(2)-.75)/.25);light(C,lpL(.3,1.9,k),Math.min(1,(p3-.7)/.1))}
  return o}
 return o}
/* ---------- the colours: white, or the accent colour (lifted to glow on a dark desktop) ---------- */
function lpCols(){if(!S.lpac||typeof acVariant!='function')return{glow:[205,228,255],core:'#f2f7ff'};
 const hex=acVariant(S.accent||'#2563eb',S.lpacm||'normal'),[h,s,l]=icRgb2hsl(...icHex2rgb(hex)),g=icHex2rgb(icHsl2hex(h,s,Math.max(l,58)));
 return{glow:g,core:'#'+g.map(v=>Math.round(lpL(v,255,.45)).toString(16).padStart(2,'0')).join(''),hex}}
const lpBr=()=>{const v=+S.lpbr;return lpC((isFinite(v)?v:100)/100)};
/* none: Reduce motion, the battery saver, a window alone in its own page (the native desktop's app windows: it draws them) */
const lpOff=()=>!!(S.rmotion||document.documentElement.classList.contains('rmotion')||(window.matchMedia&&matchMedia('(prefers-reduced-motion: reduce)').matches)||(typeof ONEAPP!='undefined'&&ONEAPP)||document.hidden);
/* ---------- drawing them on the page: one layer over the windows, one clock for all that are running ---------- */
const LPT={cv:null,runs:new Set(),raf:0,n:0};
function lpCanvas(){const D=$('#desk');if(!LPT.cv){LPT.cv=document.createElement('canvas');LPT.cv.id='lpcv';LPT.cv.setAttribute('aria-hidden','true');
  LPT.cv.style.cssText='position:absolute;left:0;top:0;pointer-events:none;z-index:2147483000'}
 if(LPT.cv.parentNode!==D)D.appendChild(LPT.cv);const d=Math.min(2,devicePixelRatio||1),w=D.clientWidth,h=D.clientHeight;
 if(LPT.cv.width!=Math.round(w*d)||LPT.cv.height!=Math.round(h*d)){LPT.cv.width=Math.round(w*d);LPT.cv.height=Math.round(h*d);LPT.cv.style.width=w+'px';LPT.cv.style.height=h+'px'}
 const c=LPT.cv.getContext('2d');c.setTransform(d,0,0,d,0,0);return c}
function lpPaint(c,sc,col,br){if(br<=0)return;
 for(const[x1,y1,x2,y2,a]of sc.L){c.save();c.globalAlpha=a*br;c.lineCap='square';c.strokeStyle=`rgba(${col.glow.join(',')},.35)`;c.lineWidth=4;c.beginPath();c.moveTo(x1,y1);c.lineTo(x2,y2);c.stroke();
  c.strokeStyle=col.core;c.lineWidth=1.4;c.beginPath();c.moveTo(x1,y1);c.lineTo(x2,y2);c.stroke();c.restore()}
 for(const[x,y,k,red]of sc.P){if(k<=0)continue;c.save();c.globalAlpha=br;const R=13*k,g=col.glow,rgb=red>0?[lpL(g[0],255,red),lpL(g[1],70,red),lpL(g[2],80,red)]:g,s=`rgba(${rgb.map(Math.round).join(',')},`;
  const gr=c.createRadialGradient(x,y,0,x,y,R);gr.addColorStop(0,s+'.95)');gr.addColorStop(.25,s+'.55)');gr.addColorStop(1,s+'0)');c.fillStyle=gr;c.beginPath();c.arc(x,y,R,0,7);c.fill();
  c.fillStyle='#ffffff';c.beginPath();c.arc(x,y,2.4*Math.max(.4,k),0,7);c.fill();c.restore()}}
/* the part of the window shown, as its clip-path (its own box, in #desk's pixels) */
function lpClip(el,R){if(!R){el.style.clipPath='';return}const x=el.offsetLeft,y=el.offsetTop,w=el.offsetWidth,h=el.offsetHeight;
 const t=lpC(R.y-y,0,h),l=lpC(R.x-x,0,w),b=lpC(y+h-(R.y+R.h),0,h),r=lpC(x+w-(R.x+R.w),0,w);
 el.style.clipPath=R.w<=.5||R.h<=.5?'inset(50% 50% 50% 50%)':`inset(${t}px ${r}px ${b}px ${l}px)`}
function lpFrame(now){LPT.raf=0;const c=lpCanvas(),col=lpCols(),br=lpBr();c.clearRect(0,0,LPT.cv.width,LPT.cv.height);
 const mc=[...LPT.runs].some(R=>R.page)?lpPageCanvas():null;if(mc)mc.clearRect(0,0,LPT.pv.width,LPT.pv.height);
 for(const R of[...LPT.runs]){if(R.page){let t=lpC((now-R.t0)/LP_MS[R.kind]);const sc=lpScene(R.kind,t,R.G);if(t>=1){lpEnd(R);continue}
   if(R.el)lpClipBox(R.el,R.G.f,sc.clip);lpPaint(mc,sc,col,br);continue}let t=lpC((now-R.t0)/LP_MS[R.kind]);if(R.back)t=1-t;const sc=lpScene(R.kind,t,R.G),end=R.back?t<=0:t>=1;
  if(R.css&&sc.geo!=R.geo){R.geo=sc.geo;R.el.style.cssText=R.css[sc.geo]+';clip-path:inset(50% 50% 50% 50%)'}
  if(end){lpEnd(R);continue}
  if(R.el)lpClip(R.el,sc.clip);lpPaint(c,sc,col,br);
  if(R.tb){R.tb.classList.toggle('lplit',sc.lit>0);if(sc.lit>0)R.tb.style.setProperty('--lplit',(sc.lit*br).toFixed(2))}}
 if(LPT.runs.size)LPT.raf=requestAnimationFrame(lpFrame);else{c.clearRect(0,0,LPT.cv.width,LPT.cv.height);LPT.cv.remove()}
 if(LPT.pv&&![...LPT.runs].some(R=>R.page))LPT.pv.remove()}
function lpEnd(R){if(!LPT.runs.delete(R))return;const el=R.el;if(R.page){if(el)el.style.clipPath='';LPT.n++;try{R.done&&R.done()}catch(e){}return}if(!el){LPT.n++;try{R.done&&R.done()}catch(e){}return}if(R.css&&R.geo!='to')el.style.cssText=R.css.to;el.style.clipPath='';if(R.pe!==undefined)el.style.pointerEvents=R.pe;
 if(R.tb){R.tb.classList.remove('lplit');R.tb.style.removeProperty('--lplit')}LPT.n++;try{R.done&&R.done()}catch(e){console.warn(e)}}
/* (a window's transition still running when another starts: the old one ends at once) */
function lpStop(el){if(el)for(const R of[...LPT.runs])if(R.el===el)lpEnd(R)}
function lpRun(kind,el,G,o={}){lpStop(el);if(lpOff()){o.done&&o.done();return null}
 const R=Object.assign({kind,el,G,t0:performance.now(),geo:'to'},o);if(R.css){R.geo='from';el.style.cssText=R.css.from}
 if(R.still){R.pe=el.style.pointerEvents;el.style.pointerEvents='none'}
 const sc=lpScene(kind,R.back?1:0,G);if(el)lpClip(el,sc.clip);LPT.runs.add(R);if(!LPT.raf)LPT.raf=requestAnimationFrame(lpFrame);return R}
/* where things are, in #desk's pixels */
const lpBox=el=>({x:el.offsetLeft,y:el.offsetTop,w:el.offsetWidth,h:el.offsetHeight});
function lpButton(W,f){const D=$('#desk'),b=W.tb&&W.tb.offsetParent?W.tb:document.querySelector('#bar #pins .pin[data-k="'+W.app+'"]');
 if(!b)return[f.x+f.w/2,D.clientHeight+20];const r=b.getBoundingClientRect(),d=D.getBoundingClientRect(),k=D.clientWidth/(d.width||1);
 return[(r.left+r.width/2-d.left)*k,(r.top+r.height/2-d.top)*k]}
/* (what opened it: a click or a key in another window, other than File Explorer opening a file, makes it side to side) */
function lpKind(W){const ev=window.event,t=ev&&ev.target&&ev.target.closest?ev.target:null,w=t&&t.closest('.win');
 if(!w||w===W.el)return'c2c';const o=wins.find(x=>x.el===w);return o&&o.app=='explorer'?'c2c':'s2s'}
/* ---------- each window: opening, minimizing and restoring, maximizing and restoring, closing ---------- */
function lpOpen(W){const el=W.el;if(lpOff())return;
 const kind=lpKind(W);requestAnimationFrame(()=>{if(!el.isConnected||W.min||el.style.display=='none')return;lpRun(kind,el,{f:lpBox(el)})});lpClip(el,{x:0,y:0,w:0,h:0});
 setTimeout(()=>{if(![...LPT.runs].some(R=>R.el===el))el.style.clipPath=''},400)}
function lpHook(W){const el=W.el,tb=W.el.querySelector('.tb');if(!tb||el.__lp)return;el.__lp=1;
 const mn=tb.querySelector('.mn'),mx=tb.querySelector('.mx');
 /* minimize: shown until it has dropped into its taskbar button; then hidden as before */
 const omn=mn.onclick;mn.onclick=function(e){const was=el.style.display!='none',f=lpBox(el);const r=omn&&omn.call(this,e);
  if(was&&W.min&&!lpOff()){el.style.display='flex';lpRun('min',el,{f,b:lpButton(W,f),dir:lpDir()},{tb:W.tb,still:1,done:()=>{if(W.min)el.style.display='none'}})}return r};
 /* restoring a minimized window: the drop played backward */
 const of=W.focus;W.focus=function(){const was=W.min,ev=window.event,fromBar=ev&&ev.type=='click'&&ev.target&&ev.target.closest&&ev.target.closest('#bar button');const r=of.apply(this,arguments);
  if(!was&&fromBar&&!W.min)lpTbb(fromBar);
  if(was&&!W.min&&el.style.display!='none'){const f=lpBox(el);lpRun('min',el,{f,b:lpButton(W,f),dir:lpDir()},{tb:W.tb,back:1})}return r};
 /* maximize and restore: the frame grows (or gathers), the window changes its size once */
 const omx=mx.onclick;mx.onclick=function(e){const before=el.style.cssText.replace(/;?\s*clip-path:[^;]*/,''),f=lpBox(el),wasMax=el.classList.contains('max');lpStop(el);
  const r=omx&&omx.call(this,e);const after=el.style.cssText,isMax=el.classList.contains('max');
  if(isMax&&!wasMax)lpRun('max',el,{f,s:lpBox(el)},{css:{from:before,to:after}});
  else if(wasMax&&!isMax){const n=lpBox(el);lpRun('res',el,{f:n,s:f},{css:{from:before,to:after}})}
  return r};
 /* close: the window goes at once (nothing of it is left for the page to find); its frame folds away where it was, as on the
    native desktop (where the window has gone by then too) */
 el.remove=function(){const f=el.isConnected&&el.style.display!='none'?lpBox(el):null;lpStop(el);Element.prototype.remove.call(el);if(f&&f.w>0)lpRun('cls',null,{f})}}
/* the taskbar button lit as the point lands in it */
{const st=document.createElement('style');st.textContent=`#bar button.lplit{box-shadow:inset 0 0 0 999px rgba(242,247,255,calc(var(--lplit,1)*.55)),0 2px 0 0 rgba(205,228,255,var(--lplit,1))!important}`;document.head.appendChild(st)}
/* ---------- Display Properties › Colours and theme: the transitions among the accent colour's places; their brightness ---------- */
(function(){const o=dispPanelS;dispPanelS=function(c,W){o(c,W);const d=c.querySelector('.acplaces');if(!d)return;const m=d.querySelector('.acmsg');
 const row=document.createElement('div');row.className='acrow lprow';
 row.innerHTML=`${ckH('dlpac','The window transitions (LitePoint)',!!S.lpac)} <select id='dlpacm' aria-label='Which colour for the window transitions'>${ACMODES.map(([k,n])=>optH(k,n,S.lpacm||'normal')).join('')}</select>`;
 const br=document.createElement('div');br.className='acrow lpbrrow';
 br.innerHTML=`<label for='dlpbr'>Window transitions' light</label><input type='range' id='dlpbr' min='0' max='100' step='5' value='${Math.round(lpBr()*100)}' style='width:180px' aria-describedby='dlpbrv'> <output id='dlpbrv'></output>`;
 m?m.before(row,br):d.append(row,br);
 const q=s=>d.querySelector(s),out=()=>{const v=+q('#dlpbr').value;q('#dlpbrv').textContent=v?v+'%':'Off (no light)'};
 const sync=()=>{q('#dlpacm').disabled=!q('#dlpac').checked};
 q('#dlpac').onchange=e=>{liveSet(c,'lpac',e.target.checked);sync()};q('#dlpacm').onchange=e=>liveSet(c,'lpacm',e.target.value);
 q('#dlpbr').oninput=e=>{liveSet(c,'lpbr',+e.target.value);out()};sync();out();
 if(m)m.textContent+=' The window transitions are white unless ticked; their light from Off to full (Reduce motion turns them off).'}})();
/* ---------- (Part 17b, the owner, 2026-11-18) the start menu, the taskbar's pop-ups, right-click menus and its buttons ----------
   "Keep the positions/alignment for the start menu and other taskbar menus the way they currently are, but implement the
   transitions in an upward-moving direction if the taskbar is on the bottom, downward-moving if the taskbar is on the top,
   etc." Drawn on a layer over the whole page (menus and the taskbar are outside the windows' #desk), in its own pixels. */
function lpPageCanvas(){if(!LPT.pv){LPT.pv=document.createElement('canvas');LPT.pv.id='lppv';LPT.pv.setAttribute('aria-hidden','true');
  LPT.pv.style.cssText='position:fixed;left:0;top:0;width:100%;height:100%;pointer-events:none;z-index:2147483001'}
 if(!LPT.pv.isConnected)document.body.appendChild(LPT.pv);const d=Math.min(2,devicePixelRatio||1),w=LPT.pv.clientWidth||innerWidth,h=LPT.pv.clientHeight||innerHeight;
 if(LPT.pv.width!=Math.round(w*d)||LPT.pv.height!=Math.round(h*d)){LPT.pv.width=Math.round(w*d);LPT.pv.height=Math.round(h*d)}
 const c=LPT.pv.getContext('2d');c.setTransform(d,0,0,d,0,0);return c}
/* an element's box on that layer */
function lpPageBox(el){const r=el.getBoundingClientRect();if(!LPT.pv||!LPT.pv.isConnected)lpPageCanvas();const c=LPT.pv.getBoundingClientRect(),k=(LPT.pv.clientWidth||innerWidth)/(c.width||1);
 return{x:(r.left-c.left)*k,y:(r.top-c.top)*k,w:r.width*k,h:r.height*k}}
function lpPagePt(x,y){if(!LPT.pv||!LPT.pv.isConnected)lpPageCanvas();const c=LPT.pv.getBoundingClientRect(),k=(LPT.pv.clientWidth||innerWidth)/(c.width||1);return[(x-c.left)*k,(y-c.top)*k]}
/* the part of a menu shown: its clip-path in shares of its own box */
function lpClipBox(el,B,R){if(!R){el.style.clipPath='';return}if(R.w<=.5||R.h<=.5){el.style.clipPath='inset(50% 50% 50% 50%)';return}
 const p=v=>lpC(v,0,1)*100+'%';el.style.clipPath=`inset(${p((R.y-B.y)/B.h)} ${p((B.x+B.w-R.x-R.w)/B.w)} ${p((B.y+B.h-R.y-R.h)/B.h)} ${p((R.x-B.x)/B.w)})`}
/* the taskbar's side: 0 bottom, 1 top, 2 left, 3 right */
const lpDir=()=>{const b=document.body.classList;return b.contains('top')?1:b.contains('left')?2:b.contains('right')?3:0};
function lpPageRun(kind,el,G,o={}){if(lpOff())return null;const R=Object.assign({kind,el,G,t0:performance.now(),page:1},o);
 for(const x of[...LPT.runs])if(x.page&&(el?x.el===el:x.kind==kind))lpEnd(x);
 if(el){const sc=lpScene(kind,0,G);lpClipBox(el,G.f,sc.clip)}LPT.runs.add(R);if(!LPT.raf)LPT.raf=requestAnimationFrame(lpFrame);return R}
const lpMid=el=>{const b=lpPageBox(el);return[b.x+b.w/2,b.y+b.h/2]};
/* where the pointer last went down (a pop-up comes from above the button clicked; a right-click menu from its corner there) */
const LPPTR={x:-1,y:-1,t:0,bar:0};
addEventListener('pointerdown',e=>{LPPTR.x=e.clientX;LPPTR.y=e.clientY;LPPTR.t=performance.now();LPPTR.bar=!!(e.target&&e.target.closest&&e.target.closest('#bar'))},true);
addEventListener('contextmenu',e=>{LPPTR.x=e.clientX;LPPTR.y=e.clientY;LPPTR.t=performance.now()},true);
/* the start menu: opening from the start button; closing (it is gone at once) its frame folds back into the button */
{const M=$('#menu');if(M){let was=getComputedStyle(M).display!='none',box=null;
 new MutationObserver(()=>{const on=M.style.display!='none'&&getComputedStyle(M).display!='none';if(on==was)return;was=on;const st=$('#start');
  if(on){box=lpPageBox(M);if(box.w<2)return;lpPageRun('smo',M,{f:box,b:st?lpMid(st):[box.x,box.y+box.h],dir:lpDir()})}
  else{for(const x of[...LPT.runs])if(x.page&&x.el===M)lpEnd(x);if(box&&box.w>2)lpPageRun('smc',null,{f:box,b:st?lpMid(st):[box.x,box.y+box.h],dir:lpDir()})}}).observe(M,{attributes:true,attributeFilter:['style','class']})}}
/* the taskbar's pop-ups (.flyout: quick settings, the calendar, notifications) and right-click menus (.cmenu) as they come */
new MutationObserver(ms=>{for(const m of ms)for(const n of m.addedNodes){if(n.nodeType!=1)continue;
 if(n.classList.contains('flyout')){const f=lpPageBox(n);if(f.w<2)continue;const fresh=performance.now()-LPPTR.t<1500&&LPPTR.bar,b=fresh?lpPagePt(LPPTR.x,LPPTR.y):[f.x+f.w/2,f.y+f.h/2];lpPageRun('pop',n,{f,b,dir:lpDir()})}
 else if(n.classList.contains('cmenu')){const f=lpPageBox(n);if(f.w<2)continue;const[px,py]=performance.now()-LPPTR.t<1500?lpPagePt(LPPTR.x,LPPTR.y):[f.x,f.y];
  const right=px>f.x+f.w/2,below=py>f.y+f.h/2;lpPageRun('ctx',n,{f,corner:below?(right?2:3):(right?1:0)})}}}).observe(document.body,{childList:true});
/* a window's taskbar button, clicked to switch to it: its outline traced once */
function lpTbb(b){if(!b||!b.isConnected)return;const f=lpPageBox(b);if(f.w<2)return;lpPageRun('tbb',null,{f,dir:lpDir()})}
