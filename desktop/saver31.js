/* ===== 1.4.4 (saver31): the screen saver: eight styles, set in Display Properties › Screen saver. Each draws into a
   canvas from getContext('2d',{willReadFrequently:true}), which Chromium keeps in memory and draws with the processor
   (Skia's software renderer), not on the graphics card: no WebGL, no CSS animation or filters. At most 30 frames a
   second and 1280 pixels wide (a larger screen shows it scaled up); nothing is drawn while the page is hidden. ===== */
const SS_STYLES=[['stars','Starfield'],['clock','Clock'],['rudolf','Rudolf'],['rain','Code rain'],['mystify','Mystify'],['bubbles','Bubbles'],['aurora','Aurora'],['fireworks','Fireworks']];
const SS_MINS=[1,2,3,5,10,15,20,30,45,60];
const ssCtx=c=>c.getContext('2d',{willReadFrequently:true,alpha:false});
const ssRnd=(a,b)=>a+Math.random()*(b-a);
const ssTime=()=>{const d=sysNow?sysNow():new Date();return d.toLocaleTimeString([],{hour:'2-digit',minute:'2-digit',hour12:typeof S=='object'&&S.h24?false:undefined})};
/* each style: (g, w, h) => frame(t ms, dt ms); w/h are the canvas's own pixels */
const SS_DRAW={
 stars:(g,w,h)=>{const st=[...Array(Math.round(w*h/4200))].map(()=>({x:ssRnd(-1,1),y:ssRnd(-1,1),z:Math.random()}));g.fillStyle='#020617';g.fillRect(0,0,w,h);
  return(t,dt)=>{g.fillStyle='rgba(2,6,23,.35)';g.fillRect(0,0,w,h);const v=dt*0.00018;for(const s of st){s.z-=v;if(s.z<=0.01){s.x=ssRnd(-1,1);s.y=ssRnd(-1,1);s.z=1}const k=0.5/s.z,x=w/2+s.x*k*w/2,y=h/2+s.y*k*h/2,r=Math.max(1,1.6/s.z*w/1280);g.fillStyle=`rgba(226,232,240,${(1-s.z).toFixed(2)})`;g.fillRect(x,y,r,r)}}},
 clock:(g,w,h)=>{let x=w/2,y=h/2,vx=w*0.00004,vy=h*0.00003;
  return(t,dt)=>{g.fillStyle='#020617';g.fillRect(0,0,w,h);const fs=Math.round(h/6);x+=vx*dt;y+=vy*dt;const hw=fs*1.6;if(x<hw||x>w-hw)vx=-vx;if(y<fs||y>h-fs*0.8)vy=-vy;x=Math.max(hw,Math.min(w-hw,x));y=Math.max(fs,Math.min(h-fs*0.8,y));
   const d=sysNow?sysNow():new Date();g.textAlign='center';g.fillStyle='#e2e8f0';g.font=`300 ${fs}px system-ui,sans-serif`;g.fillText(ssTime(),x,y);g.fillStyle='#94a3b8';g.font=`${Math.round(fs/4)}px system-ui,sans-serif`;g.fillText(d.toLocaleDateString([],{weekday:'long',month:'long',day:'numeric'}),x,y+fs*0.45)}},
 rudolf:(g,w,h)=>{const img=new Image();const src=document.querySelector('#start img');if(src)img.src=src.src;const sz=Math.round(h/4);let x=ssRnd(0,w-sz),y=ssRnd(0,h-sz),vx=w*0.00022,vy=h*0.00019,hue=ssRnd(0,360);
  return(t,dt)=>{g.fillStyle='#000';g.fillRect(0,0,w,h);x+=vx*dt;y+=vy*dt;let b=false;if(x<0||x>w-sz){vx=-vx;b=true}if(y<0||y>h-sz){vy=-vy;b=true}x=Math.max(0,Math.min(w-sz,x));y=Math.max(0,Math.min(h-sz,y));if(b)hue=(hue+67)%360;
   g.fillStyle=`hsl(${hue},80%,55%)`;g.beginPath();g.arc(x+sz/2,y+sz/2,sz*0.62,0,Math.PI*2);g.fill();if(img.complete&&img.naturalWidth)g.drawImage(img,x,y,sz,sz);else{g.fillStyle='#fff';g.font=`bold ${sz/2}px sans-serif`;g.textAlign='center';g.fillText('R',x+sz/2,y+sz*0.68)}}},
 rain:(g,w,h)=>{const fs=Math.max(10,Math.round(h/40)),cols=Math.ceil(w/fs),yv=[...Array(cols)].map(()=>ssRnd(-h,0)),ch='ｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿ0123456789ABCDEF<>{}=+*';let acc=0;g.fillStyle='#000';g.fillRect(0,0,w,h);
  return(t,dt)=>{acc+=dt;if(acc<50)return;acc=0;g.fillStyle='rgba(0,0,0,.12)';g.fillRect(0,0,w,h);g.font=`${fs}px monospace`;g.textAlign='left';for(let i=0;i<cols;i++){const c=ch[Math.floor(Math.random()*ch.length)];g.fillStyle='#d1fae5';g.fillText(c,i*fs,yv[i]);g.fillStyle='#22c55e';g.fillText(ch[Math.floor(Math.random()*ch.length)],i*fs,yv[i]-fs);yv[i]+=fs;if(yv[i]>h&&Math.random()>0.97)yv[i]=0}}},
 mystify:(g,w,h)=>{const mk=()=>({p:[...Array(4)].map(()=>({x:ssRnd(0,w),y:ssRnd(0,h),vx:ssRnd(-1,1)*w*0.00025,vy:ssRnd(-1,1)*h*0.00025})),hue:ssRnd(0,360),trail:[]});const sh=[mk(),mk()];
  return(t,dt)=>{g.fillStyle='#000';g.fillRect(0,0,w,h);g.lineWidth=Math.max(1,w/900);for(const s of sh){for(const p of s.p){p.x+=p.vx*dt;p.y+=p.vy*dt;if(p.x<0||p.x>w)p.vx=-p.vx;if(p.y<0||p.y>h)p.vy=-p.vy}s.hue=(s.hue+dt*0.02)%360;s.trail.unshift(s.p.map(p=>[p.x,p.y]));if(s.trail.length>7)s.trail.pop();
   s.trail.forEach((q,i)=>{g.strokeStyle=`hsla(${s.hue},90%,60%,${1-i/7})`;g.beginPath();q.forEach(([x,y],k)=>k?g.lineTo(x,y):g.moveTo(x,y));g.closePath();g.stroke()})}}},
 bubbles:(g,w,h)=>{const n=Math.round(w/40),bs=[...Array(n)].map(()=>({x:ssRnd(0,w),y:ssRnd(0,h),r:ssRnd(h/60,h/14),v:ssRnd(0.02,0.06)*h/1000,ph:ssRnd(0,6),hue:ssRnd(180,300)}));
  return(t,dt)=>{const gr=g.createLinearGradient(0,0,0,h);gr.addColorStop(0,'#082f49');gr.addColorStop(1,'#020617');g.fillStyle=gr;g.fillRect(0,0,w,h);for(const b of bs){b.y-=b.v*dt;if(b.y<-b.r){b.y=h+b.r;b.x=ssRnd(0,w)}const x=b.x+Math.sin(t/1400+b.ph)*b.r*0.8;
   g.fillStyle=`hsla(${b.hue},70%,70%,.18)`;g.beginPath();g.arc(x,b.y,b.r,0,Math.PI*2);g.fill();g.strokeStyle=`hsla(${b.hue},80%,80%,.55)`;g.lineWidth=1.5;g.stroke();g.fillStyle='rgba(255,255,255,.5)';g.beginPath();g.arc(x-b.r*0.35,b.y-b.r*0.35,b.r*0.18,0,Math.PI*2);g.fill()}}},
 aurora:(g,w,h)=>{const st=[...Array(120)].map(()=>[ssRnd(0,w),ssRnd(0,h*0.7),ssRnd(0.3,1)]);
  return(t,dt)=>{g.fillStyle='#020617';g.fillRect(0,0,w,h);for(const[x,y,a]of st){g.fillStyle=`rgba(255,255,255,${(a*(0.6+0.4*Math.sin(t/900+x))).toFixed(2)})`;g.fillRect(x,y,1.5,1.5)}
   for(let k=0;k<3;k++){const hue=[140,170,280][k];g.beginPath();g.moveTo(0,h);for(let x=0;x<=w;x+=w/48){const y=h*(0.35+0.08*k)+Math.sin(x/w*6+t/(2600+k*700)+k)*h*0.07+Math.sin(x/w*13-t/1900)*h*0.025;g.lineTo(x,y)}g.lineTo(w,h);g.closePath();
    const gr=g.createLinearGradient(0,h*0.25,0,h*0.8);gr.addColorStop(0,`hsla(${hue},85%,60%,.0)`);gr.addColorStop(0.25,`hsla(${hue},85%,60%,.32)`);gr.addColorStop(1,'hsla(220,60%,8%,0)');g.fillStyle=gr;g.fill()}}},
 fireworks:(g,w,h)=>{const rk=[],sp=[];let next=0;g.fillStyle='#000';g.fillRect(0,0,w,h);
  return(t,dt)=>{g.fillStyle='rgba(0,0,0,.2)';g.fillRect(0,0,w,h);if(t>next){rk.push({x:ssRnd(w*0.15,w*0.85),y:h,vy:-ssRnd(0.5,0.75)*h/1000,top:ssRnd(h*0.15,h*0.45),hue:ssRnd(0,360)});next=t+ssRnd(500,1300)}
   for(let i=rk.length-1;i>=0;i--){const r=rk[i];r.y+=r.vy*dt;g.fillStyle='#fde68a';g.fillRect(r.x,r.y,2,4);if(r.y<=r.top){rk.splice(i,1);const n=60;for(let k=0;k<n;k++){const a=k/n*Math.PI*2,v=ssRnd(0.05,0.18)*h/1000;sp.push({x:r.x,y:r.y,vx:Math.cos(a)*v,vy:Math.sin(a)*v,life:1,hue:r.hue})}}}
   for(let i=sp.length-1;i>=0;i--){const p=sp[i];p.vy+=0.00012*h/1000*dt;p.x+=p.vx*dt;p.y+=p.vy*dt;p.life-=dt/1600;if(p.life<=0){sp.splice(i,1);continue}g.fillStyle=`hsla(${p.hue},90%,65%,${p.life.toFixed(2)})`;g.fillRect(p.x,p.y,2,2)}}},
};
/* runs a style in canvas c until stopped: {stop, frames} */
function ssLoop(c,style,opt){opt=opt||{};const g=ssCtx(c),w=c.width,h=c.height,f=(SS_DRAW[style]||SS_DRAW.stars)(g,w,h);let raf=0,last=0,t0=0,run=true;const st={frames:0,stop:()=>{run=false;cancelAnimationFrame(raf)}};
 const fr=t=>{if(!run)return;raf=requestAnimationFrame(fr);if(!t0)t0=last=t;const dt=t-last;if(dt<32&&st.frames)return;last=t;try{f(t-t0,Math.min(100,dt||16))}catch(e){console.warn(e)}
  if(opt.time&&style!='clock'){const fs=Math.round(h/12);g.textAlign='center';g.font=`600 ${fs}px system-ui,sans-serif`;g.fillStyle='rgba(2,6,23,.55)';g.fillText(ssTime(),w/2+2,h-fs*0.9+2);g.fillStyle='#e2e8f0';g.fillText(ssTime(),w/2,h-fs*0.9)}st.frames++};
 raf=requestAnimationFrame(fr);return st}
/* the full-screen saver (saverStart in core25) */
function ssRun(opt){opt=opt||{};const c=document.createElement('canvas');c.id='saver';const k=Math.min(1,1280/Math.max(1,innerWidth));c.width=Math.round(innerWidth*k);c.height=Math.round(innerHeight*k);c.style.cssText='width:100vw;height:100vh;background:#000';document.body.appendChild(c);
 const style=opt.style||SYSC.ssstyle||'stars',lp=ssLoop(c,style,{time:SYSC.sstime!==false});let iv=0;
 /* on installed Rudolf OS, input in a game or a Linux app (which the page does not see) also ends it */
 if(RD)iv=setInterval(async()=>{try{const j=await rd('idle');if(j.idle!=null&&j.idle<1500&&SAVER&&Date.now()-SAVER.t>1500)saverStop()}catch(e){}},1000);
 return{c,style,preview:!!opt.preview,t:Date.now(),lp,stop:()=>{lp.stop();clearInterval(iv);c.remove()}}}
/* Display Properties › Screen saver */
(function(){const o=dispPanelS;dispPanelS=function(c,W){o(c,W);const adm=isAdmin();const D=adm?'':' disabled';
 const d=document.createElement('div');d.className='ss31';
 d.innerHTML=`<h4 class='sh4'>Screen saver</h4><div class='r2'><label>Screen saver</label><div>${ckH('sson','On',SYSC.sson)}</div>
  <label for='ssmin'>Start after</label><select id='ssmin'${D}>${SS_MINS.map(m=>optH(m,m+(m==1?' minute':' minutes')+' without input',SYSC.ssmin)).join('')}</select>
  <label for='ssst'>Style</label><div><select id='ssst'${D}>${SS_STYLES.map(([k,n])=>optH(k,n,SYSC.ssstyle||'stars')).join('')}</select> <button class='ssprev'>Preview</button><div><canvas class='sspc' width='240' height='135' style='margin-top:6px;border-radius:6px;background:#000;width:240px;height:135px'></canvas></div></div>
  <label>Options</label><div>${ckH('sstime','Show the time (except with Clock)',SYSC.sstime!==false)}<br>${ckH('sslock','Ask for the password when it ends',SYSC.lockss)}</div></div>
  <div class='msg'>Drawn by the processor, not the graphics card. It does not start while something plays full screen, a game holds the mouse or a video plays${RD?', or while you use a game or a Linux app':''}.${adm?'':' Only administrators can change it.'}</div>`;
 const at=c.querySelector(':scope > .sfoot');if(at)at.before(d);else c.appendChild(d);
 const q=s=>d.querySelector(s);if(!adm)['#sson','#sstime','#sslock'].forEach(s=>q(s).disabled=true);
 let pv=null;const prev=()=>{pv&&pv.stop();pv=ssLoop(q('.sspc'),q('#ssst').value,{time:q('#sstime').checked})};prev();
 const kill=()=>{if(!d.isConnected){pv&&pv.stop();clearInterval(kt)}};const kt=setInterval(kill,1000);
 const save=()=>{SYSC.sson=q('#sson').checked;SYSC.ssmin=+q('#ssmin').value;SYSC.ssstyle=q('#ssst').value;SYSC.sstime=q('#sstime').checked;SYSC.lockss=q('#sslock').checked;sysSave()};
 ['#sson','#ssmin','#ssst','#sstime','#sslock'].forEach(s=>q(s).addEventListener('change',e=>{e.stopPropagation();if(adm)save();prev()}));
 q('.ssprev').onclick=e=>{e.preventDefault();if(SAVER)return;SAVER=ssRun({preview:true,style:q('#ssst').value})}}})();
/* User Settings › Sign-in options keeps Lock; the screen saver's settings are in Display Properties now */
(function(){const o=usrPanelS;usrPanelS=function(c,W){o(c,W);const s=c.querySelector('#uss');if(!s)return;const l=s.previousElementSibling;
 const n=document.createElement('div');n.className='msg';n.innerHTML=`In <a href='#'>Display Properties › Screen saver</a>.`;s.replaceWith(n);if(l)l.textContent='Screen saver';n.querySelector('a').onclick=e=>{e.preventDefault();W.show('disp')};
 const k=c.querySelector('#ulk');if(k){const lb=k.closest('label');if(lb)lb.remove()}}})();
