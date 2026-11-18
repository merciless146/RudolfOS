/* ===== Rudolf OS 1.6.0 (docs/plan-1.6.0.md Part 17): the window transitions, "Splitting–Merging LitePoint" =====
   The owner's design (2026-11-17): points of light split from one place on a window's frame, draw its edges and meet at
   the opposite corner or side in a red spark; the window then fills out from there. Minimizing and closing take the
   frame back; maximizing and restoring grow or gather it. At the speeds the owner chose in the demo ("that's the exact
   right speed it needs to run at"):
     open, corner to corner (from the start menu, the desktop, a pinned app, a file) 200 ms;
     open, side to side (a window another window opened: a dialog, Settings at a page)  220 ms;
     maximize 180 ms; minimize 200 ms (the point drops into the window's taskbar button; restoring plays it backward);
     restore from maximized 180 ms; close 200 ms.
   Display Properties › Colours and theme: "The window transitions (LitePoint)" among the places the accent colour goes
   (white when not ticked), Normal / Negative / Complementary as the others, and the light's brightness (0: no light, the
   window still fills, clears and drops on the same timing; 100: the demo's). The red spark stays red. Reduce motion
   (and the battery saver): no transitions.
   lpScene() says what is where at a moment of a transition (the lines, the points, the part of the window shown); the
   native desktop has the same, in C (iso/sys/litepoint.h), and iso/qt/shell2/litepoint60_t.js compares the two. */
Object.assign(DEFS.disp,{lpac:false,lpacm:'normal',lpbr:100});
['lpac','lpacm','lpbr'].forEach(k=>{if(!DKEYS.includes(k))DKEYS.push(k);if(S[k]===undefined)S[k]=DEFS.disp[k];if(SC&&SC[k]===undefined)SC[k]=cln(S[k])});
const LP_MS={c2c:200,s2s:220,max:180,min:200,res:180,cls:200};
/* each transition's steps, as shares of its time */
const LP_W={c2c:[.07,.29,.29,.35],s2s:[.06,.2,.22,.2,.32],max:[.07,.3,.3,.33],min:[.3,.22,.18,.3],res:[.3,.36,.34],cls:[.32,.3,.38]};
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
/* KIND at T (0..1), G: f the window's rectangle (for max: as it was; for res: as it will be), s the maximized one, b the
   taskbar button's middle. L: lines [x1,y1,x2,y2,alpha]; P: points [x,y,size,red]; geo: the window's geometry, 'from'
   (as it was) or 'to' (as it is after); clip: the part of it shown (null: all of it); lit: the taskbar button lit (0..1) */
function lpScene(kind,t,G){
 const o={L:[],P:[],geo:'to',clip:null,lit:0},line=(A,B,a=1)=>{if(a>0)o.L.push([A[0],A[1],B[0],B[1],Math.min(1,a)])},light=(A,k=1,r=0)=>o.P.push([A[0],A[1],k,lpC(r)]);
 const rect=(R,a=1)=>{const A=[R.x,R.y],B=[R.x+R.w,R.y],C=[R.x+R.w,R.y+R.h],D=[R.x,R.y+R.h];line(A,B,a);line(B,C,a);line(C,D,a);line(D,A,a)};
 const Z=R=>({x:R.x,y:R.y,w:0,h:0});
 if(LP_FR[kind]){const F=kind=='max'?G.s:G.f,d=LP_FR[kind],pt=([a,b])=>[F.x+a*F.w,F.y+b*F.h],n=LP_W[kind].length,{i,p}=lpAt(kind,t),last=i==n-1;
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
 if(kind=='min'){const f=G.f,BM=[f.x+f.w/2,f.y+f.h],TL=[f.x,f.y],TR=[f.x+f.w,f.y],BL=[f.x,f.y+f.h],BR=[f.x+f.w,f.y+f.h],p1=lpE(q(0)),p2=lpE(q(1)),p3=lpE(q(2)),p4=lpE(q(3));
  if(t<=0)return o;if(t>=1){o.clip=Z(f);return o}
  o.clip=t<.3?{x:lpL(f.x,BM[0],p1),y:lpL(f.y,BM[1],p1),w:lpL(f.w,0,p1),h:lpL(f.h,0,p1)}:Z(f);
  if(t>=.7)o.lit=lpC((q(3)-.75)/.25)*(1-lpC((t-.985)/.015));
  if(t<.3)rect(f,lpC(t/.08));
  else if(t<.52){const A=lpP(TL,BL,p2),B=lpP(TR,BR,p2);line(A,BL);line(B,BR);line(BL,BR);light(A);light(B)}
  else if(t<.7){const A=lpP(BL,BM,p3),B=lpP(BR,BM,p3);line(A,B);light(A);light(B);if(q(2)>.8)light(BM,1+(q(2)-.8)*4,(q(2)-.8)/.2)}
  else{const P=lpP(BM,G.b,p4);line(BM,P,1-p4);light(P,lpL(1.4,.8,p4),1-lpC(p4/.4))}
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
 for(const R of[...LPT.runs]){let t=lpC((now-R.t0)/LP_MS[R.kind]);if(R.back)t=1-t;const sc=lpScene(R.kind,t,R.G),end=R.back?t<=0:t>=1;
  if(R.css&&sc.geo!=R.geo){R.geo=sc.geo;R.el.style.cssText=R.css[sc.geo]+';clip-path:inset(50% 50% 50% 50%)'}
  if(end){lpEnd(R);continue}
  lpClip(R.el,sc.clip);lpPaint(c,sc,col,br);
  if(R.tb){R.tb.classList.toggle('lplit',sc.lit>0);if(sc.lit>0)R.tb.style.setProperty('--lplit',(sc.lit*br).toFixed(2))}}
 if(LPT.runs.size)LPT.raf=requestAnimationFrame(lpFrame);else{c.clearRect(0,0,LPT.cv.width,LPT.cv.height);LPT.cv.remove()}}
function lpEnd(R){if(!LPT.runs.delete(R))return;const el=R.el;if(R.css&&R.geo!='to')el.style.cssText=R.css.to;el.style.clipPath='';if(R.pe!==undefined)el.style.pointerEvents=R.pe;
 if(R.tb){R.tb.classList.remove('lplit');R.tb.style.removeProperty('--lplit')}LPT.n++;try{R.done&&R.done()}catch(e){console.warn(e)}}
/* (a window's transition still running when another starts: the old one ends at once) */
function lpStop(el){for(const R of[...LPT.runs])if(R.el===el)lpEnd(R)}
function lpRun(kind,el,G,o={}){lpStop(el);if(lpOff()){o.done&&o.done();return null}
 const R=Object.assign({kind,el,G,t0:performance.now(),geo:'to'},o);if(R.css){R.geo='from';el.style.cssText=R.css.from}
 if(R.still){R.pe=el.style.pointerEvents;el.style.pointerEvents='none'}
 const sc=lpScene(kind,R.back?1:0,G);lpClip(el,sc.clip);LPT.runs.add(R);if(!LPT.raf)LPT.raf=requestAnimationFrame(lpFrame);return R}
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
  if(was&&W.min&&!lpOff()){el.style.display='flex';lpRun('min',el,{f,b:lpButton(W,f)},{tb:W.tb,still:1,done:()=>{if(W.min)el.style.display='none'}})}return r};
 /* restoring a minimized window: the drop played backward */
 const of=W.focus;W.focus=function(){const was=W.min;const r=of.apply(this,arguments);
  if(was&&!W.min&&el.style.display!='none'){const f=lpBox(el);lpRun('min',el,{f,b:lpButton(W,f)},{tb:W.tb,back:1})}return r};
 /* maximize and restore: the frame grows (or gathers), the window changes its size once */
 const omx=mx.onclick;mx.onclick=function(e){const before=el.style.cssText.replace(/;?\s*clip-path:[^;]*/,''),f=lpBox(el),wasMax=el.classList.contains('max');lpStop(el);
  const r=omx&&omx.call(this,e);const after=el.style.cssText,isMax=el.classList.contains('max');
  if(isMax&&!wasMax)lpRun('max',el,{f,s:lpBox(el)},{css:{from:before,to:after}});
  else if(wasMax&&!isMax){const n=lpBox(el);lpRun('res',el,{f:n,s:f},{css:{from:before,to:after}})}
  return r};
 /* close: the window stays on the screen (not in the list of windows) until its frame has gone */
 el.remove=function(){if(lpOff()||!el.isConnected||el.style.display=='none'){lpStop(el);return Element.prototype.remove.call(el)}
  lpRun('cls',el,{f:lpBox(el)},{still:1,done:()=>Element.prototype.remove.call(el)})}}
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
