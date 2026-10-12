/* ===== Emu: Super Nintendo (LakeSnes, MIT) and Nintendo DS (DeSmuME, GPL-2.0+) =====
   The cores are WebAssembly (lib/emu, built by systools/mkemu.sh); the SNES core is also inlined for the
   browser version. Games live in the game library /home/rudolf/Games/<SNES|DS> (rudolfd fs/*, games/*), or
   come from drive C: or a file picker. Saves and save states go next to the game (library: Saves, States). */
const EMU_SYS={
 snes:{name:'Super Nintendo',short:'SNES',dir:'SNES',re:/\.(sfc|smc)$/i,core:'lakesnes',fps:60.0988,ic:'🎮'},
 ds:{name:'Nintendo DS',short:'DS',dir:'DS',re:/\.nds$/i,core:'desmume',fps:59.8261,ic:'📱'}};
const EMU_BTN=['Up','Down','Left','Right','A','B','X','Y','L','R','Start','Select'];
const EMU_KEYS={Up:'ArrowUp',Down:'ArrowDown',Left:'ArrowLeft',Right:'ArrowRight',A:'KeyX',B:'KeyZ',X:'KeyS',Y:'KeyA',L:'KeyQ',R:'KeyW',Start:'Enter',Select:'ShiftRight'};
const EMU_PAD={A:1,B:0,X:3,Y:2,L:4,R:5,Start:9,Select:8,Up:12,Down:13,Left:14,Right:15};   /* standard gamepad layout, Nintendo positions */
const EMU_SNESIDX={B:0,Y:1,Select:2,Start:3,Up:4,Down:5,Left:6,Right:7,A:8,X:9,L:10,R:11};
const EMU_DSBIT={R:0,L:1,Down:2,Up:3,Left:4,Right:5,B:6,A:7,Y:8,X:9,Start:10,Select:11};
const EMU_GAMES='/home/rudolf/Games';
const emuCtlK=s=>'bd95emuctl:'+s;
function emuCtl(s){const c=ld(emuCtlK(s),null)||{};return {keys:Object.assign({},EMU_KEYS,c.keys||{}),pad:Object.assign({},EMU_PAD,c.pad||{})}}
/* ---------- WASI: the few system calls the cores make ---------- */
function emuWasi(getMem){let out='';const u8=()=>new Uint8Array(getMem().buffer),dv=()=>new DataView(getMem().buffer);
 const W={fd_write(fd,iov,n,nw){let t=0;const d=dv();for(let i=0;i<n;i++){const p=d.getUint32(iov+8*i,true),l=d.getUint32(iov+8*i+4,true);out+=new TextDecoder().decode(u8().subarray(p,p+l));t+=l}d.setUint32(nw,t,true);let k;while((k=out.indexOf('\n'))>=0){if(window.__emuLog)console.log('[emu]',out.slice(0,k));out=out.slice(k+1)}return 0},
  fd_close:()=>0,fd_seek:()=>70,fd_fdstat_get(fd,p){const d=dv();d.setUint8(p,fd<3?2:0);d.setUint16(p+2,0,true);d.setBigUint64(p+8,0n,true);d.setBigUint64(p+16,0n,true);return fd<3?0:8},
  environ_sizes_get(c,s){const d=dv();d.setUint32(c,0,true);d.setUint32(s,0,true);return 0},environ_get:()=>0,args_sizes_get(c,s){const d=dv();d.setUint32(c,0,true);d.setUint32(s,0,true);return 0},args_get:()=>0,
  clock_time_get(id,pr,p){dv().setBigUint64(p,BigInt(Math.round((id==0?Date.now():performance.now())*1e6)),true);return 0},clock_res_get(id,p){dv().setBigUint64(p,1000n,true);return 0},
  random_get(p,l){crypto.getRandomValues(u8().subarray(p,p+l));return 0},proc_exit(c){throw new Error('the emulator stopped ('+c+')')},sched_yield:()=>0,
  /* no files: the C library must hear "bad descriptor" / "no such file", not "not supported" */
  fd_prestat_get:()=>8,fd_prestat_dir_name:()=>8,fd_read:()=>8,fd_filestat_get:()=>8,fd_readdir:()=>8,path_open:()=>44,path_filestat_get:()=>44,path_unlink_file:()=>44,path_create_directory:()=>44,poll_oneoff:()=>52};
 return {wasi_snapshot_preview1:new Proxy(W,{get:(o,k)=>o[k]||(()=>52)})}}
const EMU_MOD={};
async function emuModule(sys){const c=EMU_SYS[sys].core;if(EMU_MOD[c])return EMU_MOD[c];
 let bytes=null;
 try{const r=await fetch((RD?'/':'')+'lib/emu/'+c+'.wasm');if(r.ok){bytes=await r.arrayBuffer();if(new Uint32Array(bytes,0,1)[0]!=0x6d736100)bytes=null}}catch(e){bytes=null}   /* the browser version finds the cores next to the page */
 if(!bytes&&c=='lakesnes'&&typeof EMU_SNES_WASM=='string'){const b=atob(EMU_SNES_WASM);bytes=new Uint8Array(b.length);for(let i=0;i<b.length;i++)bytes[i]=b.charCodeAt(i)}
 if(!bytes)throw new Error(EMU_SYS[sys].name+' games need Rudolf OS itself (the bootable disc or USB stick).');
 return EMU_MOD[c]=await WebAssembly.compile(bytes)}
async function emuInstance(sys){const m=await emuModule(sys);let inst;inst=await WebAssembly.instantiate(m,emuWasi(()=>inst.exports.memory));if(inst.exports._initialize)inst.exports._initialize();return inst.exports}
/* ---------- where a game's saves and states are kept ---------- */
function emuStore(g){
 if(g.real){const dir=g.real.replace(/\/[^/]+$/,''),base=g.real.split('/').pop().replace(/\.[^.]+$/,''),lib=dir.startsWith(EMU_GAMES+'/'),sd=lib?dir+'/Saves':dir,td=lib?dir+'/States':dir;
  const p=(k)=>k=='sav'?sd+'/'+base+'.sav':td+'/'+base+'.'+k;
  return {where:lib?'the game library':'next to the game',
   async get(k){try{const r=await fetch('/api/fs/read?path='+encodeURIComponent(p(k)),{headers:{'X-Rudolf-Token':RD.token}});if(!r.ok)return null;const ct=r.headers.get('content-type')||'';if(ct.includes('json'))return null;return new Uint8Array(await r.arrayBuffer())}catch(e){return null}},
   async put(k,b){await rd('fs/write?path='+encodeURIComponent(p(k)),{method:'POST',body:b})},
   async has(k){try{const j=await rd('fs/list?path='+encodeURIComponent(k=='sav'?sd:td));return j.items.some(i=>i.name==p(k).split('/').pop())}catch(e){return false}}}}
 if(g.drive){const dir=g.drive.slice(0,-1),base=g.drive.at(-1).replace(/\.[^.]+$/,'');const nm=k=>base+(k=='sav'?'.SAV':'.ST'+k.replace(/\D/g,'')+(k.endsWith('png')?'P':''));
  return {where:'next to the game on drive C:',
   async get(k){const v=(node(dir)||{})[nm(k)];if(typeof v!='string')return null;try{return new Uint8Array(await (await fetch(v.startsWith('data:')?v:'data:application/octet-stream;base64,'+btoa(v))).arrayBuffer())}catch(e){return null}},
   async put(k,b){const d=node(dir);if(!isDir(d))return;let s='';const u=new Uint8Array(b instanceof Blob?await b.arrayBuffer():b);for(let i=0;i<u.length;i+=32768)s+=String.fromCharCode.apply(null,u.subarray(i,i+32768));d[nm(k)]='data:application/octet-stream;base64,'+btoa(s);fsSave()},
   async has(k){return typeof (node(dir)||{})[nm(k)]=='string'}}}
 const K='bd95emust:'+g.sys+':'+g.name;   /* a game picked from this computer's files: kept in this browser */
 return {where:'in this browser',async get(k){const v=ld(K+':'+k,null);if(!v)return null;const b=atob(v),u=new Uint8Array(b.length);for(let i=0;i<b.length;i++)u[i]=b.charCodeAt(i);return u},
  async put(k,b){const u=new Uint8Array(b instanceof Blob?await b.arrayBuffer():b);let s='';for(let i=0;i<u.length;i+=32768)s+=String.fromCharCode.apply(null,u.subarray(i,i+32768));try{svj(K+':'+k,btoa(s))}catch(e){throw new Error('Not enough browser storage for this save.')}},async has(k){return !!ld(K+':'+k,null)}}}
/* ---------- the player ---------- */
const EMU={players:new Set()};
async function emuPlay(g){   /* g: {sys, name, bytes, real?, drive?} */
 const sy=EMU_SYS[g.sys];let x;try{x=await emuInstance(g.sys)}catch(e){notify('Emu',e.message,{kind:'error'});return}
 const st=emuStore(g),title=g.title||g.name.replace(/\.[^.]+$/,'');
 const W=win('Emu — '+title,'🦤',g.sys=='ds'?520:640,g.sys=='ds'?720:560);W.body.classList.add('emuw');W.emu=true;EMU.players.add(W);
 W.body.innerHTML=`<div class='emutb'><button class='elib' title='Game library'>🎮 Library</button><button class='epp' title='Pause (P)'>⏸</button><button class='ers' title='Reset'>↺</button>
  <button class='ess' title='Save state'>💾 Save</button><button class='els' title='Load state'>📂 Load</button><select class='eslot' title='State slot'>${[1,2,3,4,5,6,7,8,9].map(i=>`<option>${i}</option>`).join('')}</select>
  ${g.sys=='ds'?"<select class='elay' title='Screen layout'><option value='stack'>Stacked</option><option value='side'>Side by side</option><option value='large'>Large + small</option></select><button class='eswap' title='Swap screens (large + small)'>⇅</button>":''}
  <button class='ectl' title='Controls'>🎮 Controls</button><button class='ebl' title='Borderless (Alt+Enter)'>⛶</button><button class='emt' title='Mute'>🔊</button><span class='emsg'></span></div>
  <div class='emust'><canvas class='emucv' tabindex='0'></canvas><div class='emupad' hidden></div><div class='emumenu' hidden></div><div class='emucorner' title='Menu'></div></div>`;
 const q=s=>W.body.querySelector(s),cv=q('.emucv'),ctx=cv.getContext('2d'),stage=q('.emust'),msg=t=>{q('.emsg').textContent=t};
 let ctl=emuCtl(g.sys),held=new Set(),pads=new Set(),touch=null,paused=false,muted=false,ff=false,alive=true,acc=0,last=0,raf=0,frames=0;
 let layout=g.sys=='ds'?(ld('bd95emulayout','stack')):'', swap=false;
 /* load the game (and its battery save) */
 const rom=g.bytes instanceof Uint8Array?g.bytes:new Uint8Array(g.bytes);
 const sav=await st.get('sav');let ok;
 if(g.sys=='snes'){const p=x.ls_alloc(rom.length);new Uint8Array(x.memory.buffer,p,rom.length).set(rom);ok=x.ls_load(rom.length);
  if(ok&&sav&&sav.length){const b=x.ls_alloc(sav.length);new Uint8Array(x.memory.buffer,b,sav.length).set(sav);x.ls_battery_load(sav.length)}}
 else{x.ds_init(48000);if(sav&&sav.length){const p=x.ds_sav_ptr(sav.length);new Uint8Array(x.memory.buffer,p,sav.length).set(sav)}
  const p=x.ds_rom_buffer(rom.length);new Uint8Array(x.memory.buffer,p,rom.length).set(rom);const r=x.ds_load();ok=r==0;
  if(!ok)g.err=r==-1001?'This DS game image is encrypted (not a usable dump). Make a decrypted dump of your game.':'This DS game could not be started.'}
 if(!ok){W.close();notify('Emu',g.err||'This does not look like a '+sy.name+' game.',{kind:'error'});return}
 emuRecent(g);msg(sav&&sav.length?'Save loaded':'');
 /* picture */
 const SW=256,SH=g.sys=='ds'?384:224;let img=null;
 const size=()=>{let cw,ch;if(g.sys=='snes'){cw=512;ch=448}else if(layout=='side'){cw=512;ch=192}else if(layout=='large'){cw=341;ch=192}else{cw=256;ch=384}
  if(cv.width!=cw||cv.height!=ch){cv.width=cw;cv.height=ch}
  const r=stage.getBoundingClientRect(),ar=g.sys=='snes'?(ld('bd95emusnesar','4:3')=='8:7'?8/7:4/3):cw/ch;let w=r.width,h=w/ar;if(h>r.height){h=r.height;w=h*ar}
  if(ld('bd95emuint',false)&&g.sys=='ds'){const k=Math.max(1,Math.floor(Math.min(r.width/cw,r.height/ch)));w=cw*k;h=ch*k}
  cv.style.width=Math.floor(w)+'px';cv.style.height=Math.floor(h)+'px'};
 const off=document.createElement('canvas');off.width=256;off.height=384;const octx=off.getContext('2d');
 const draw=()=>{
  if(g.sys=='snes'){const px=new Uint8ClampedArray(x.memory.buffer,x.ls_pixels(),512*448*4);for(let i=3;i<px.length;i+=4)px[i]=255;if(!img||img.data.buffer!==x.memory.buffer)img=new ImageData(px,512,448);ctx.putImageData(img,0,0);return}
  const px=new Uint8ClampedArray(x.memory.buffer,x.ds_frame_ptr(),256*384*4);if(!img||img.data.buffer!==x.memory.buffer)img=new ImageData(px,256,384);
  if(layout=='stack')ctx.putImageData(img,0,0);
  else if(layout=='side'){ctx.putImageData(img,0,0,0,0,256,192);ctx.putImageData(img,256,-192,0,192,256,192)}
  else{octx.putImageData(img,0,0);const big=swap?0:192,small=swap?192:0;ctx.fillStyle='#000';ctx.fillRect(0,0,341,192);ctx.drawImage(off,0,big,256,192,85,0,256,192);ctx.drawImage(off,0,small,256,192,0,0,85,64)}};
 /* where the DS's lower screen is on the canvas (for touch) */
 const lowerRect=()=>layout=='side'?[256,0,256,192]:layout=='large'?(swap?[0,0,85,64]:[85,0,256,192]):[0,192,256,192];
 /* sound */
 let ac=null,gain=null,nextT=0;const ensureAudio=()=>{if(ac)return;try{ac=new (window.AudioContext||window.webkitAudioContext)({sampleRate:48000});gain=ac.createGain();gain.connect(ac.destination);gain.gain.value=muted?0:1}catch(e){ac=null}};
 const play=(s16,n)=>{if(!ac||muted||n<=0)return;if(ac.state=='suspended')ac.resume().catch(()=>{});const t=ac.currentTime;if(nextT<t)nextT=t+0.04;if(nextT>t+0.25)return;
  const b=ac.createBuffer(2,n,48000),l=b.getChannelData(0),r=b.getChannelData(1);for(let i=0;i<n;i++){l[i]=s16[2*i]/32768;r[i]=s16[2*i+1]/32768}const s=ac.createBufferSource();s.buffer=b;s.connect(gain);s.start(nextT);nextT+=n/48000};
 /* input */
 const down=b=>held.has(b)||pads.has(b);
 const pollPads=()=>{pads=new Set();for(const p of (navigator.getGamepads?navigator.getGamepads():[])){if(!p)continue;EMU_BTN.forEach(b=>{const i=ctl.pad[b];if(p.buttons[i]&&p.buttons[i].pressed)pads.add(b)});
   const ax=p.axes||[];if(ax[0]<-.5)pads.add('Left');if(ax[0]>.5)pads.add('Right');if(ax[1]<-.5)pads.add('Up');if(ax[1]>.5)pads.add('Down');
   if(p.buttons[10]&&p.buttons[10].pressed)ff=true;if(p.buttons[16]&&p.buttons[16].pressed&&!W._gm){W._gm=1;menu()}else if(!(p.buttons[16]&&p.buttons[16].pressed))W._gm=0}
  if(pads.size)q('.emupad').hidden=true};
 const step=draw1=>{
  if(g.sys=='snes'){EMU_BTN.forEach(b=>x.ls_button(1,EMU_SNESIDX[b],down(b)?1:0));x.ls_frame(800);if(draw1)draw();if(!ff)play(new Int16Array(x.memory.buffer,x.ls_sound(),1600),800)}
  else{let k=0;EMU_BTN.forEach(b=>{if(down(b))k|=1<<EMU_DSBIT[b]});x.ds_frame(draw1?1:0,k,touch?1:0,touch?touch[0]:0,touch?touch[1]:0);const n=x.ds_audio(802);if(draw1)draw();if(!ff)play(new Int16Array(x.memory.buffer,x.ds_audio_ptr(),n*2),n)}
  frames++};
 const loop=t=>{if(!alive)return;raf=requestAnimationFrame(loop);if(paused||!W.el.isConnected){last=t;return}pollPads();
  if(!last)last=t;acc+=Math.min(100,t-last)*(ff?4:1);last=t;const fd=1000/sy.fps,mx=ff?8:3;let n=0;while(acc>=fd&&n<mx){acc-=fd;n++;step(acc<fd||n==mx)}if(n==mx)acc=Math.min(acc,fd);if(n==0)return;   /* too slow: draw the last frame and drop the backlog */
  if(frames%60==0)lastInput=Date.now()};   /* no screensaver while playing */
 /* battery saves: written when they change (checked every 3 s) */
 let lastSav=sav?sav.join(','):'';
 const saveBattery=async()=>{try{let b=null;if(g.sys=='snes'){const n=x.ls_battery_save();if(n>0)b=new Uint8Array(x.memory.buffer,x.ls_buf(),n).slice()}else if(x.ds_sav_changed()){const n=x.ds_sav_size();if(n>0)b=new Uint8Array(x.memory.buffer,x.ds_sav_ptr(0),n).slice()}
  if(!b)return;const s=b.join(',');if(s==lastSav)return;lastSav=s;await st.put('sav',b);msg('Saved')}catch(e){msg('Save failed: '+e.message)}};
 const savIv=setInterval(saveBattery,3000);
 /* save states */
 const slot=()=>q('.eslot').value;
 const saveState=async()=>{try{const n=g.sys=='snes'?x.ls_state_save():x.ds_state_save();if(!n)throw new Error('the emulator could not save');const b=new Uint8Array(x.memory.buffer,g.sys=='snes'?x.ls_buf():x.ds_state_buf(0),n).slice();
  await st.put('st'+slot(),b);const th=await new Promise(r=>cv.toBlob(r,'image/png'));if(th)await st.put('st'+slot()+'png',th);msg('State '+slot()+' saved')}catch(e){msg('Could not save the state: '+e.message)}};
 const loadState=async()=>{const b=await st.get('st'+slot());if(!b){msg('Slot '+slot()+' is empty');return}let r;
  if(g.sys=='snes'){const p=x.ls_alloc(b.length);new Uint8Array(x.memory.buffer,p,b.length).set(b);r=x.ls_state_load(b.length)}else{const p=x.ds_state_buf(b.length);new Uint8Array(x.memory.buffer,p,b.length).set(b);r=x.ds_state_load()}
  msg(r?'State '+slot()+' loaded':'That state could not be loaded');draw()};
 /* borderless mode and its menu */
 const setBl=on=>{W.el.classList.toggle('emubl',on);document.body.classList.toggle('emu-borderless',on);W.bl=on;setTimeout(size,30);if(on)cv.focus();else q('.emumenu').hidden=true};
 const menu=()=>{const m=q('.emumenu');if(!m.hidden){m.hidden=true;paused=false;return}paused=true;
  m.innerHTML=`<div class='emumbox'><b></b><button data-a='res'>▶ Resume</button><button data-a='ss'>💾 Save state ${slot()}</button><button data-a='ls'>📂 Load state ${slot()}</button><button data-a='bl'>${W.bl?'⬜ Leave borderless':'⛶ Borderless'}</button><button data-a='ctl'>🎮 Controls</button><button data-a='quit'>✕ Close the game</button></div>`;
  m.querySelector('b').textContent=title;m.hidden=false;
  m.onclick=e=>{const a=e.target.dataset&&e.target.dataset.a;if(!a)return;m.hidden=true;paused=false;if(a=='ss')saveState();if(a=='ls')loadState();if(a=='bl')setBl(!W.bl);if(a=='ctl')emuControls(g.sys,()=>{ctl=emuCtl(g.sys)});if(a=='quit')W.close()}};
 q('.emucorner').onclick=menu;
 /* on-screen buttons for the touchscreen */
 const padEl=q('.emupad');padEl.innerHTML=`<div class='epl'><b data-b='L'>L</b><div class='edp'><b data-b='Up'>▲</b><b data-b='Left'>◀</b><b data-b='Right'>▶</b><b data-b='Down'>▼</b></div></div>
  <div class='epm'><b data-b='Select'>SELECT</b><b data-b='Start'>START</b></div><div class='epr'><b data-b='R'>R</b><div class='eab'><b data-b='X'>X</b><b data-b='Y'>Y</b><b data-b='A'>A</b><b data-b='B'>B</b></div></div>`;
 const pdown=new Map();padEl.querySelectorAll('[data-b]').forEach(el=>{const b=el.dataset.b;
  el.onpointerdown=e=>{e.preventDefault();e.stopPropagation();el.setPointerCapture(e.pointerId);pdown.set(e.pointerId,b);held.add(b);el.classList.add('on')};
  const up=e=>{if(pdown.get(e.pointerId)==b){pdown.delete(e.pointerId);if(![...pdown.values()].includes(b))held.delete(b);el.classList.remove('on')}};el.onpointerup=up;el.onpointercancel=up;el.onlostpointercapture=up});
 const showPad=()=>{if(padEl.hidden&&ld('bd95emupad',true)){padEl.hidden=false}};
 /* DS touch: a tap or drag on the lower screen */
 const toDs=e=>{const r=cv.getBoundingClientRect(),cx=(e.clientX-r.left)*cv.width/r.width,cy=(e.clientY-r.top)*cv.height/r.height,[lx,ly,lw,lh]=lowerRect();
  const tx=Math.floor((cx-lx)*256/lw),ty=Math.floor((cy-ly)*192/lh);return tx>=0&&tx<256&&ty>=0&&ty<192?[tx,ty]:null};
 let tid=null;
 cv.onpointerdown=e=>{cv.focus();ensureAudio();if(e.pointerType=='touch')showPad();if(g.sys!='ds')return;const p=toDs(e);if(p){tid=e.pointerId;touch=p;cv.setPointerCapture(e.pointerId);e.preventDefault()}};
 cv.onpointermove=e=>{if(e.pointerId===tid){const p=toDs(e);if(p)touch=p}};
 const tup=e=>{if(e.pointerId===tid){tid=null;touch=null}};cv.onpointerup=tup;cv.onpointercancel=tup;
 stage.addEventListener('touchstart',showPad,{passive:true});
 /* keyboard */
 const keyOf=c=>EMU_BTN.find(b=>ctl.keys[b]==c);
 W.body.addEventListener('keydown',e=>{if(e.target.tagName=='SELECT')return;ensureAudio();
  if(e.code=='Escape'){e.preventDefault();e.stopPropagation();if(W.bl&&!q('.emumenu').hidden){q('.emumenu').hidden=true;paused=false;setBl(false);return}if(W.bl){menu();return}return}
  if(e.code=='F11'||(e.altKey&&e.code=='Enter')){e.preventDefault();setBl(!W.bl);return}
  if(e.code=='Tab'){e.preventDefault();ff=true;return}if(e.code=='KeyP'&&!keyOf('KeyP')){paused=!paused;q('.epp').textContent=paused?'▶':'⏸';return}
  const b=keyOf(e.code);if(b){e.preventDefault();held.add(b);padEl.hidden=true}e.stopPropagation()});
 W.body.addEventListener('keyup',e=>{if(e.code=='Tab')ff=false;const b=keyOf(e.code);if(b)held.delete(b);e.stopPropagation()});
 W.el.addEventListener('pointerdown',()=>{if(document.activeElement!==cv&&!W.body.querySelector('.emutb').contains(document.activeElement))setTimeout(()=>cv.focus(),0)});
 /* toolbar */
 q('.elib').onclick=()=>emuLibrary();
 q('.epp').onclick=()=>{paused=!paused;q('.epp').textContent=paused?'▶':'⏸';cv.focus()};
 q('.ers').onclick=()=>{g.sys=='snes'?x.ls_reset(0):x.ds_reset();cv.focus()};
 q('.ess').onclick=()=>{saveState();cv.focus()};q('.els').onclick=()=>{loadState();cv.focus()};
 q('.ectl').onclick=()=>emuControls(g.sys,()=>{ctl=emuCtl(g.sys)});
 q('.ebl').onclick=()=>setBl(true);
 q('.emt').onclick=()=>{muted=!muted;q('.emt').textContent=muted?'🔇':'🔊';if(gain)gain.gain.value=muted?0:1};
 if(g.sys=='ds'){const sl=q('.elay');sl.value=layout;sl.onchange=()=>{layout=sl.value;svj('bd95emulayout',layout);img=null;size();draw();cv.focus()};q('.eswap').onclick=()=>{swap=!swap;if(layout!='large'){layout='large';sl.value='large';svj('bd95emulayout',layout)}size();draw();cv.focus()}}
 const ro=new ResizeObserver(()=>size());ro.observe(stage);
 W.onclose=()=>{alive=false;cancelAnimationFrame(raf);clearInterval(savIv);saveBattery();ro.disconnect();if(W.bl)setBl(false);if(ac)ac.close().catch(()=>{});EMU.players.delete(W)};
 W.emuApi={x,step,draw,setBl,menu,saveState,loadState,slot:v=>{q('.eslot').value=v},touch:v=>{touch=v},hold:(b,v)=>{v?held.add(b):held.delete(b)},layout:v=>{q('.elay').value=v;q('.elay').onchange()},paused:()=>paused,frames:()=>frames,store:st,lowerRect,canvas:cv};
 if(ld('bd95emubl',false))setBl(true);
 size();draw();cv.focus();raf=requestAnimationFrame(loop);
 if('ontouchstart' in window&&!(navigator.getGamepads&&[...navigator.getGamepads()].some(Boolean)))showPad();
 return W}
/* ---------- recently played, favourites ---------- */
const emuRecK=()=>'bd95emurecent:'+uid(),emuFavK=()=>'bd95emufav:'+uid();
function emuKey(g){return g.real||(g.drive?g.drive.join('/'):g.sys+':'+g.name)}
function emuRecent(g){const l=ld(emuRecK(),[]).filter(x=>x.k!=emuKey(g));l.unshift({k:emuKey(g),t:Date.now()});svj(emuRecK(),l.slice(0,30))}
/* ---------- titles and icons, read from the games themselves ---------- */
function emuSnesTitle(b,size){const off=(size||b.length)%1024==512?512:0;let best=null;
 for(const [a,hi] of [[0x7FC0,0],[0xFFC0,1]]){const h=a+off;if(h+0x20>b.length)continue;const ck=b[h+0x1C]|b[h+0x1D]<<8,cs=b[h+0x1E]|b[h+0x1F]<<8,raw=b.subarray(h,h+21),mode=b[h+0x15];
  const printable=[...raw].every(c=>c>=0x20&&c<0x7f),t=String.fromCharCode(...raw).trim();
  const score=((ck^cs)==0xFFFF?2:0)+(((mode&0xEF)==(hi?0x21:0x20))||((mode&0xEF)==(hi?0x25:0x20))?2:0)+(printable&&t.length>2?2:0);if(!best||score>best.s)best={s:score,t:printable?t:''}}
 return best&&best.t?best.t.replace(/\s+/g,' '):''}
function emuDsInfo(h,ban){const title=String.fromCharCode(...h.subarray(0,12)).replace(/\0.*$/,'').trim();let name='',icon='';
 if(ban&&ban.length>=0x840){const u16=o=>ban[o]|ban[o+1]<<8;let t='';for(let i=0;i<128;i++){const c=u16(0x340+2*i);if(!c)break;t+=String.fromCharCode(c)}name=t.split('\n')[0].trim();
  const c=document.createElement('canvas');c.width=c.height=32;const x=c.getContext('2d'),im=x.createImageData(32,32);const pal=[];for(let i=0;i<16;i++){const v=u16(0x220+2*i);pal.push([(v&31)*8.2|0,(v>>5&31)*8.2|0,(v>>10&31)*8.2|0])}
  for(let ty=0;ty<4;ty++)for(let tx=0;tx<4;tx++)for(let y=0;y<8;y++)for(let xx=0;xx<8;xx+=2){const byte=ban[0x20+(ty*4+tx)*32+y*4+xx/2];[byte&15,byte>>4].forEach((ci,k)=>{const o=((ty*8+y)*32+tx*8+xx+k)*4;if(ci){const p=pal[ci];im.data[o]=p[0];im.data[o+1]=p[1];im.data[o+2]=p[2];im.data[o+3]=255}})}
  x.putImageData(im,0,0);icon=c.toDataURL()}
 return {title:name||title,icon}}
async function emuRange(path,a,b){const r=await fetch('/api/fs/read?path='+encodeURIComponent(path),{headers:{'X-Rudolf-Token':RD.token,Range:`bytes=${a}-${b}`}});if(!r.ok)throw new Error('HTTP '+r.status);return new Uint8Array(await r.arrayBuffer())}
async function emuInfo(sys,path,size){if(sys=='snes'){const b=await emuRange(path,0,Math.min(size,0x10200)-1);return {title:emuSnesTitle(b,size)}}
 const h=await emuRange(path,0,0x1FF);const bo=h[0x68]|h[0x69]<<8|h[0x6A]<<16|h[0x6B]<<24;let ban=null;if(bo&&bo+0x840<=size)try{ban=await emuRange(path,bo,bo+0x83F)}catch(e){}return emuDsInfo(h,ban)}
/* ---------- the library ---------- */
const emuLibK='bd95emulib';
async function emuScan(){const cache=ld(emuLibK,{}),out=[];
 for(const sys of ['snes','ds']){const dir=EMU_GAMES+'/'+EMU_SYS[sys].dir;let items=[];try{items=(await rd('fs/list?path='+encodeURIComponent(dir))).items}catch(e){}
  for(const it of items){if(it.dir||!EMU_SYS[sys].re.test(it.name))continue;const path=dir+'/'+it.name;let c=cache[path];
   if(!c||c.size!=it.size||c.mtime!=it.mtime){c={size:it.size,mtime:it.mtime,title:'',icon:''};try{Object.assign(c,await emuInfo(sys,path,it.size))}catch(e){}cache[path]=c}
   out.push({sys,path,name:it.name,size:it.size,title:c.title||it.name.replace(/\.[^.]+$/,''),icon:c.icon||''})}}
 for(const k of Object.keys(cache))if(!out.some(o=>o.path==k))delete cache[k];try{svj(emuLibK,cache)}catch(e){}return out}
async function emuOpenReal(path){const sys=/\.nds$/i.test(path)?'ds':'snes';
 try{const r=await fetch('/api/fs/read?path='+encodeURIComponent(path),{headers:{'X-Rudolf-Token':RD.token}});if(!r.ok)throw new Error('could not read the game (HTTP '+r.status+')');
  const c=(ld(emuLibK,{})[path]||{});return emuPlay({sys,name:path.split('/').pop(),title:c.title,bytes:new Uint8Array(await r.arrayBuffer()),real:path})}catch(e){notify('Emu',e.message,{kind:'error'})}}
async function emuOpenDrive(p){const v=node(p);if(typeof v!='string')return;const sys=/\.nds$/i.test(p.at(-1))?'ds':'snes';
 const u=v.startsWith('data:')?new Uint8Array(await (await fetch(v)).arrayBuffer()):Uint8Array.from(v,c=>c.charCodeAt(0)&255);
 return emuPlay({sys,name:p.at(-1),bytes:u,drive:p})}
function emuPickFile(){const i=document.createElement('input');i.type='file';i.accept='.sfc,.smc,.nds';i.onchange=async()=>{const f=i.files[0];if(!f)return;
 emuPlay({sys:/\.nds$/i.test(f.name)?'ds':'snes',name:f.name,bytes:new Uint8Array(await f.arrayBuffer())})};i.click()}
let EMU_LIBW=null;
async function emuLibrary(){if(EMU_LIBW&&wins.includes(EMU_LIBW)){EMU_LIBW.focus();EMU_LIBW.refresh();return EMU_LIBW}
 const W=win('Emu — Game library','🦤',760,560);EMU_LIBW=W;W.body.classList.add('emulib');
 const guest=!USER||USER.r=='guest';
 W.body.innerHTML=`<div class='elhead'><div class='eltabs' role='tablist'>${[['all','All'],['recent','Recently played'],['fav','★ Favourites'],['gb','Game Boy / GBA'],['snes','Super Nintendo'],['ds','Nintendo DS']].map(([k,t])=>`<button data-t='${k}' role='tab'>${t}</button>`).join('')}</div>
  <div class='elbar'><input class='elq' placeholder='Search games' aria-label='Search games'><span style='flex:1'></span>${RD&&!guest?"<button class='eladd'>＋ Add games…</button>":''}<button class='elopen'>📂 Open a file…</button></div></div>
  <div class='elgrid'></div><div class='msg elfoot'></div>`;
 const q=s=>W.body.querySelector(s),grid=q('.elgrid');let tab=ld('bd95emutab','all'),games=[];
 const gbGames=()=>(typeof mmGames=='function'?mmGames('gb'):[]).map(x=>({sys:'gb',path:x.path.join('/'),drive:x.path,title:x.label.replace(/\.[^.]+$/,''),name:x.label}));
 const draw=()=>{W.body.querySelectorAll('.eltabs button').forEach(b=>{b.classList.toggle('on',b.dataset.t==tab);b.setAttribute('aria-selected',b.dataset.t==tab)});
  const rec=ld(emuRecK(),[]),fav=ld(emuFavK(),[]),f=q('.elq').value.trim().toLowerCase();
  let l=games.concat(gbGames());
  if(tab=='recent')l=rec.map(r=>l.find(g=>emuKey(g)==r.k||g.path==r.k)).filter(Boolean);else if(tab=='fav')l=l.filter(g=>fav.includes(g.path));else if(tab!='all')l=l.filter(g=>g.sys==tab);
  if(f)l=l.filter(g=>(g.title+' '+g.name).toLowerCase().includes(f));
  grid.innerHTML='';
  if(!l.length){grid.innerHTML=`<div class='msg elempty'>${tab=='recent'?'Games you play show up here.':tab=='fav'?'Press and hold a game (or right-click) to make it a favourite.':(RD?'No games here yet. '+(guest?'':'Use “Add games…” to copy your SNES and DS games from a USB stick or Downloads.'):'In this browser, open a SNES game with “Open a file…”. DS games need Rudolf OS itself.')}</div>`;return}
  l.forEach(g=>{const t=document.createElement('button');t.className='eltile';const last=rec.find(r=>r.k==emuKey(g)||r.k==g.path);
   t.innerHTML=`<span class='elic'>${g.icon?`<img src='${g.icon}' alt=''>`:g.sys=='gb'?'🟩':EMU_SYS[g.sys].ic}</span><b></b><small></small>`;t.querySelector('b').textContent=g.title;
   t.querySelector('small').textContent=(g.sys=='gb'?'Game Boy':EMU_SYS[g.sys].short)+(g.size?' · '+fmtB(g.size):'')+(last?' · '+new Date(last.t).toLocaleDateString():'')+(fav.includes(g.path)?' · ★':'');t.title=g.name;
   t.onclick=()=>open(g);let lt;t.onpointerdown=e=>{lt=setTimeout(()=>{lt='done';ctxm(g,e.clientX,e.clientY)},600)};t.onpointerup=t.onpointerleave=()=>{if(lt!='done')clearTimeout(lt)};
   t.addEventListener('click',e=>{if(lt=='done'){e.stopImmediatePropagation();lt=null}},true);t.oncontextmenu=e=>{e.preventDefault();ctxm(g,e.clientX,e.clientY)};grid.appendChild(t)})};
 const open=g=>{if(g.sys=='gb'){const P=typeof gbPlayer=='function'&&gbPlayer();if(P&&P.loadAny){const n=node(g.drive);mmBytes(n).then(u=>P.loadAny(u,g.name));emuRecent(g)}return}emuOpenReal(g.path)};
 const ctxm=(g,X,Y)=>{const fav=ld(emuFavK(),[]),isF=fav.includes(g.path);const it=[['Play',()=>open(g),true,{icon:'▶'}],[isF?'Remove from favourites':'Add to favourites',()=>{svj(emuFavK(),isF?fav.filter(x=>x!=g.path):fav.concat(g.path));draw()}]];
  if(g.sys!='gb'&&RD){it.push(['Show in Files',()=>{launch('explorer');setTimeout(()=>{const W=wins.filter(w=>w.el.querySelector('.xside')).pop();const it=W&&[...W.el.querySelectorAll('.xside *')].find(e=>e.children.length<3&&/Games \(Emu library\)/.test(e.textContent));it&&it.click()},500)}]);
   if(!guest)it.push('-',['Delete from the library…',async()=>{if(!await elevate('Delete '+g.title+' from the game library (its saves stay)'))return;try{await rdPost('fs/delete?path='+encodeURIComponent(g.path));notify('Emu',g.title+' was deleted.');refresh()}catch(e){notify('Emu',e.message,{kind:'error'})}}])}
  cmenu(X,Y,it,{title:g.title})};
 const refresh=async()=>{q('.elfoot').textContent=RD?'Reading the library…':'';if(RD){try{games=await emuScan()}catch(e){games=[]}
   try{const i=await lxApi('games/info');q('.elfoot').textContent=`Game library: ${fmtB(i.used)} used · ${fmtB(i.free)} free${i.free<1073741824?' — space is running low':''}${i.persistent?'':' · Rudolf OS is not installed, so games added now are gone after shutdown'}`}catch(e){q('.elfoot').textContent=''}}draw()};
 W.refresh=refresh;
 W.body.querySelector('.eltabs').onclick=e=>{const t=e.target.dataset&&e.target.dataset.t;if(t){tab=t;svj('bd95emutab',t);draw()}};
 q('.elq').oninput=draw;q('.elopen').onclick=emuPickFile;if(q('.eladd'))q('.eladd').onclick=()=>emuAddGames(refresh);
 W.body.addEventListener('keydown',e=>e.stopPropagation());
 if(RD)lxApi('games/info').catch(()=>{});draw();refresh();return W}
/* ---------- adding games: copied on the Rudolf OS side, so big DS games never pass through the page ---------- */
async function emuAddGames(done){const W=win('Add games to the library','🦤',620,520);W.body.classList.add('emuadd');
 W.body.innerHTML=`<p class='msg'>Choose where your games are, tick them, then press Add. ZIP files are unpacked; games already in the library are skipped.</p><div class='eaplaces'></div><div class='eapath msg'></div><div class='ealist'></div>
  <div class='rmrow' style='justify-content:flex-start'><button class='eaall'>Select all</button><button class='eago pri' disabled>Add</button></div><div class='eaprog' hidden><div class='eastep'></div><div class='dsbar'><i></i></div></div><div class='msg eares'></div>`;
 const q=s=>W.body.querySelector(s);let cur='',sel=new Set();
 const places=[['⬇️ Downloads','/home/rudolf/Downloads'],['📄 Documents','/home/rudolf/Documents']];
 try{const r=await rd('disks');r.disks.forEach(dk=>{if(dk.partitions.some(x=>x.label=='RUDOLFDATA'||x.fs=='iso9660'))return;dk.partitions.filter(x=>x.fs&&x.fs!='swap').forEach(pt=>places.push([(dk.removable?'🔌 ':'💽 ')+(pt.label||pt.name),pt.mount||'',pt.name]))})}catch(e){}
 q('.eaplaces').innerHTML='';places.forEach(([l,p,dev])=>{const b=document.createElement('button');b.textContent=l;b.onclick=async()=>{if(!p&&dev){try{await rdPost('mount?dev='+dev);p='/media/'+dev}catch(e){q('.eares').textContent=e.message;return}}show(p)};q('.eaplaces').appendChild(b)});
 const show=async p=>{cur=p;sel=new Set();q('.eapath').textContent=p;const L=q('.ealist');L.innerHTML="<div class='msg'>Reading…</div>";let it=[];try{it=(await rd('fs/list?path='+encodeURIComponent(p))).items}catch(e){L.innerHTML=`<div class='msg'>${lxEsc(e.message)}</div>`;return}
  it.sort((a,b)=>(b.dir-a.dir)||a.name.localeCompare(b.name));L.innerHTML='';
  if(p!='/home/rudolf/Downloads'&&p!='/home/rudolf/Documents'&&p.split('/').length>3){const up=document.createElement('div');up.className='earow';up.textContent='⬆️ ..';up.onclick=()=>show(p.replace(/\/[^/]+$/,''));L.appendChild(up)}
  const ok=it.filter(i=>i.dir||/\.(sfc|smc|nds|zip)$/i.test(i.name));if(!ok.length)L.innerHTML+="<div class='msg'>No SNES (.sfc, .smc), DS (.nds) or ZIP files here.</div>";
  ok.forEach(i=>{const r=document.createElement('label');r.className='earow';if(i.dir){r.textContent='📁 '+i.name;r.onclick=()=>show(p+'/'+i.name)}else{r.innerHTML="<input type='checkbox'> <span></span><small></small>";r.querySelector('span').textContent=i.name;r.querySelector('small').textContent=fmtB(i.size);
   r.querySelector('input').onchange=e=>{e.target.checked?sel.add(p+'/'+i.name):sel.delete(p+'/'+i.name);q('.eago').disabled=!sel.size}}L.appendChild(r)});q('.eago').disabled=true};
 q('.eaall').onclick=()=>{W.body.querySelectorAll('.ealist input').forEach(c=>{if(!c.checked){c.checked=true;c.onchange({target:c})}})};
 q('.eago').onclick=async()=>{if(!sel.size)return;try{await lxApi('games/import',[...sel].join('\n'))}catch(e){q('.eares').textContent=e.message;return}
  const pg=q('.eaprog');pg.hidden=false;q('.eago').disabled=true;const res=[];
  const j=await lxJobWait(l=>{let m;if((m=/^progress (\d+) (\d+) (.*)/.exec(l))){q('.eastep').textContent=`Copying ${m[3]} (${m[1]} of ${m[2]})…`;pg.querySelector('i').style.width=(m[1]/m[2]*100)+'%'}else if(/^(added|skip) /.test(l))res.push(l.replace(/^added /,'✓ ').replace(/^skip /,'– '));else if(/^done /.test(l))q('.eastep').textContent=l.slice(5)});
  pg.querySelector('i').style.width='100%';const last=(j.log||'').trim().split('\n').pop()||'';q('.eastep').textContent=/^done /.test(last)?'Finished: '+last.slice(5)+'.':'Adding stopped: '+last;
  q('.eares').textContent=res.join('\n');done&&done()};
 show('/home/rudolf/Downloads');return W}
/* ---------- controls ---------- */
function emuControls(sys,changed){const W=win('Emu controls — '+EMU_SYS[sys].name,'🎮',520,560);W.body.classList.add('emuctlw');
 let c=emuCtl(sys),wait=null;
 const kn=k=>({ArrowUp:'↑',ArrowDown:'↓',ArrowLeft:'←',ArrowRight:'→',ShiftRight:'Right Shift',ShiftLeft:'Left Shift',Enter:'Enter',Space:'Space'})[k]||k.replace(/^Key|^Digit/,'');
 const draw=()=>{W.body.innerHTML=`<p class='msg'>Click a key or pad button, then press the one you want. Gamepads use the standard layout (Nintendo positions); the left stick also moves. Tab fast-forwards, Alt+Enter switches borderless mode, Esc opens the menu in borderless mode.</p>
  <table class='tmt ectab'><thead><tr><th>Button</th><th>Keyboard</th><th>Gamepad</th></tr></thead><tbody>${EMU_BTN.map(b=>`<tr><td>${b}</td><td><button data-k='${b}'>${lxEsc(kn(c.keys[b]))}</button></td><td><button data-p='${b}'>Button ${c.pad[b]}</button></td></tr>`).join('')}</tbody></table>
  <label class='chk'><input type='checkbox' class='ecpad'${ld('bd95emupad',true)?' checked':''}> Show on-screen buttons when the touchscreen is used</label>
  ${sys=='ds'?`<label class='chk'><input type='checkbox' class='ecint'${ld('bd95emuint',false)?' checked':''}> Whole-pixel scaling (sharper, with black edges)</label>`:`<div class='r2'><label>Picture shape</label><select class='ecar'><option value='4:3'>4:3 (as on a TV)</option><option value='8:7'>8:7 (square pixels)</option></select></div>`}
  <label class='chk'><input type='checkbox' class='ecbl'${ld('bd95emubl',false)?' checked':''}> Always start games borderless</label>
  <div class='rmrow' style='justify-content:flex-start'><button class='ecdef'>Default controls</button></div><div class='msg ecmsg'></div>`;
  W.body.querySelectorAll('[data-k],[data-p]').forEach(b=>b.onclick=()=>{wait={k:b.dataset.k,p:b.dataset.p,el:b};b.textContent='Press…';W.body.querySelector('.ecmsg').textContent='Waiting for '+(b.dataset.k?'a key':'a gamepad button')+'… (Esc cancels)';if(b.dataset.p)padWait()});
  W.body.querySelector('.ecpad').onchange=e=>svj('bd95emupad',e.target.checked);W.body.querySelector('.ecbl').onchange=e=>svj('bd95emubl',e.target.checked);
  if(sys=='ds')W.body.querySelector('.ecint').onchange=e=>svj('bd95emuint',e.target.checked);else{const a=W.body.querySelector('.ecar');a.value=ld('bd95emusnesar','4:3');a.onchange=()=>svj('bd95emusnesar',a.value)}
  W.body.querySelector('.ecdef').onclick=()=>{svj(emuCtlK(sys),{});c=emuCtl(sys);draw();changed&&changed()}};
 const store=()=>{const d={keys:{},pad:{}};EMU_BTN.forEach(b=>{if(c.keys[b]!=EMU_KEYS[b])d.keys[b]=c.keys[b];if(c.pad[b]!=EMU_PAD[b])d.pad[b]=c.pad[b]});svj(emuCtlK(sys),d);changed&&changed()};
 const padWait=()=>{const t0=Date.now();const iv=setInterval(()=>{if(!wait||!wait.p||!W.el.isConnected||Date.now()-t0>10000){clearInterval(iv);if(wait&&wait.p){wait=null;draw()}return}
  for(const p of navigator.getGamepads?navigator.getGamepads():[]){if(!p)continue;const i=p.buttons.findIndex(b=>b.pressed);if(i>=0){c.pad[wait.p]=i;wait=null;clearInterval(iv);store();draw();return}}},50)};
 W.body.addEventListener('keydown',e=>{e.stopPropagation();if(!wait||!wait.k)return;e.preventDefault();if(e.code!='Escape'){EMU_BTN.forEach(b=>{if(c.keys[b]==e.code)c.keys[b]=c.keys[wait.k]});c.keys[wait.k]=e.code;store()}wait=null;draw()});
 draw();return W}
/* ---------- Emu's launchers open the library; .sfc/.smc/.nds files open here ---------- */
if(typeof MMLN!='undefined'&&MMLN.gb)MMLN.gb.fn=()=>emuLibrary();
if(APPS.gameboy){APPS.gameboy.open=()=>emuLibrary();APPS.gameboy.kw=(APPS.gameboy.kw||'')+' snes super nintendo ds nintendo ds emulator games library'}
window.emuOpenReal=emuOpenReal;window.emuOpenDrive=emuOpenDrive;
