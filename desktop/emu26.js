/* ===== Emu: Super Nintendo (LakeSnes, MIT) and Nintendo DS (DeSmuME, GPL-2.0+); 1.6.1: Nintendo 64 and Dreamcast
   (native Linux apps: Rosalie's Mupen GUI, Flycast); 1.6.1: Genesis / Mega Drive (ClownMDEmu, AGPL-3.0+) and PlayStation
   (PCSX-ReARMed, GPL-2.0+), libretro cores as WebAssembly with Emu's frontend (sys/emu/lr_api.c) =====
   The cores are WebAssembly (built by systools/mkemu.sh). 1.5.2: on Rudolf OS they come from the rudolf-emu package
   of the Rudolf OS Library (/pkg/rudolf-emu/www, lib52.js), downloaded when a DS game is first played; the SNES core is
   also inlined, so SNES games play without it. Games live in the game library /home/rudolf/Games/<GB|SNES|DS> (rudolfd fs/*, games/*;
   1.5.2: Game Boy, Color and Advance games in GB play in the Game Boy player), or
   come from drive C: or a file picker. Saves and save states go next to the game (library: Saves, States). */
const EMU_SYS={
 snes:{name:'Super Nintendo',short:'SNES',dir:'SNES',re:/\.(sfc|smc)$/i,core:'lakesnes',fps:60.0988,ic:'🎮'},
 ds:{name:'Nintendo DS',short:'DS',dir:'DS',re:/\.nds$/i,core:'desmume',fps:59.8261,ic:'📱'},
 /* 1.6.1: native emulators, as Linux apps from Flathub on the graphics card (rudolfd native/*): Rosalie's Mupen GUI
    (Mupen64Plus, GPL-3.0) for the Nintendo 64 and Flycast (GPL-2.0) for the Dreamcast. They open full screen with the
    game; controllers are theirs (Flycast sets up a plugged-in controller by itself; 1.6.2: Rosalie's Mupen GUI plugged
    none in until its settings were saved, so rudolfd's emu-native-setup gives it one: the first controller, else the
    keyboard), and so are saves and states */
 n64:{name:'Nintendo 64',short:'N64',dir:'N64',re:/\.(z64|n64|v64)$/i,native:'com.github.Rosalie241.RMG',app:'Rosalie’s Mupen GUI',ic:'🕹️',
  how:'F12 ends the game. Player 1 is the first controller, or the keyboard: the arrow keys are the stick, X is A, C is B, Z is Z, Return is Start. Ctrl+I opens its controller settings.'},
 dc:{name:'Sega Dreamcast',short:'Dreamcast',dir:'DC',re:/\.(gdi|cdi|chd)$/i,native:'org.flycast.Flycast',app:'Flycast',ic:'🌀',
  how:'Tab opens Flycast’s menu (controllers, saves, Exit). Most games start without the Dreamcast’s BIOS files; Flycast’s settings say where they go, for the rest.'},
 /* 1.6.1: libretro cores (lr: the frontend's pixel formats, core options, the controller). The Genesis takes the game
    from memory; a PlayStation disc is read from the library as the core asks for it (emuDiscFs). Saves: the Genesis
    cartridge's battery memory, the PlayStation's memory card 1 */
 md:{name:'Sega Genesis / Mega Drive',short:'Genesis',dir:'MD',re:/\.(md|gen|smd|bin)$/i,core:'clownmd',ic:'🔷',lr:{fmt:1}},
 ps1:{name:'PlayStation',short:'PlayStation',dir:'PS1',re:/\.(cue|chd|pbp)$/i,core:'pcsx',ic:'💿',
  lr:{fmt:7,disc:true,dev:0x205,opts:['pcsx_rearmed_memcard2=none','pcsx_rearmed_show_bios_bootlogo=disabled']}}};
const emuNat=sys=>!!(EMU_SYS[sys]&&EMU_SYS[sys].native);
const EMU_BTN=['Up','Down','Left','Right','A','B','X','Y','L','R','Start','Select'];
const EMU_KEYS={Up:'ArrowUp',Down:'ArrowDown',Left:'ArrowLeft',Right:'ArrowRight',A:'KeyX',B:'KeyZ',X:'KeyS',Y:'KeyA',L:'KeyQ',R:'KeyW',Start:'Enter',Select:'ShiftRight'};
const EMU_PAD={A:1,B:0,X:3,Y:2,L:4,R:5,Start:9,Select:8,Up:12,Down:13,Left:14,Right:15};   /* standard gamepad layout, Nintendo positions */
const EMU_SNESIDX={B:0,Y:1,Select:2,Start:3,Up:4,Down:5,Left:6,Right:7,A:8,X:9,L:10,R:11};
const EMU_DSBIT={R:0,L:1,Down:2,Up:3,Left:4,Right:5,B:6,A:7,Y:8,X:9,Start:10,Select:11};
const EMU_GAMES='/home/rudolf/Games';
/* 1.6.1: each console's buttons: their keys and gamepad buttons (standard layout: 0 bottom, 1 right, 2 left, 3 top,
   4/5 shoulders, 6/7 triggers, 8 Select, 9 Start, 10/11 stick clicks, 12-15 the d-pad), and for the libretro cores the
   RetroPad bit each one is (B 0, Y 1, Select 2, Start 3, Up-Right 4-7, A 8, X 9, L 10, R 11, L2 12, R2 13, L3 14, R3 15) */
const EMU_DIR={Up:'ArrowUp',Down:'ArrowDown',Left:'ArrowLeft',Right:'ArrowRight'},EMU_DIRP={Up:12,Down:13,Left:14,Right:15},EMU_DIRB={Up:4,Down:5,Left:6,Right:7};
const EMU_LAY={
 md:{btn:['Up','Down','Left','Right','A','B','C','X','Y','Z','Start','Mode'],
  keys:Object.assign({A:'KeyA',B:'KeyS',C:'KeyD',X:'KeyQ',Y:'KeyW',Z:'KeyE',Start:'Enter',Mode:'ShiftRight'},EMU_DIR),
  pad:Object.assign({A:2,B:0,C:1,X:4,Y:3,Z:5,Start:9,Mode:8},EMU_DIRP),
  bit:Object.assign({A:1,B:0,C:8,X:10,Y:9,Z:11,Start:3,Mode:2},EMU_DIRB)},
 ps1:{btn:['Up','Down','Left','Right','Cross','Circle','Square','Triangle','L1','R1','L2','R2','L3','R3','Start','Select'],
  keys:Object.assign({Cross:'KeyZ',Circle:'KeyX',Square:'KeyA',Triangle:'KeyS',L1:'KeyQ',R1:'KeyW',L2:'Digit1',R2:'Digit2',L3:'KeyC',R3:'KeyV',Start:'Enter',Select:'ShiftRight'},EMU_DIR),
  pad:Object.assign({Cross:0,Circle:1,Square:2,Triangle:3,L1:4,R1:5,L2:6,R2:7,L3:10,R3:11,Start:9,Select:8},EMU_DIRP),
  bit:Object.assign({Cross:0,Circle:8,Square:1,Triangle:9,L1:10,R1:11,L2:12,R2:13,L3:14,R3:15,Start:3,Select:2},EMU_DIRB),
  /* the left stick from the keyboard (full tilt), for games that need it */
  stick:{SUp:'KeyI',SDown:'KeyK',SLeft:'KeyJ',SRight:'KeyL'},sticks:true}};
const emuLay=s=>EMU_LAY[s]||{btn:EMU_BTN,keys:EMU_KEYS,pad:EMU_PAD};
const EMU_BNAME={Cross:'✕ Cross',Circle:'○ Circle',Square:'□ Square',Triangle:'△ Triangle',SUp:'Left stick up',SDown:'Left stick down',SLeft:'Left stick left',SRight:'Left stick right'};
const emuCtlK=s=>'bd95emuctl:'+s;
function emuCtl(s){const c=ld(emuCtlK(s),null)||{},L=emuLay(s);return {btn:L.btn,keys:Object.assign({},L.keys,L.stick||{},c.keys||{}),pad:Object.assign({},L.pad,c.pad||{})}}
/* ---------- WASI: the few system calls the cores make ---------- */
function emuWasi(getMem,vfs){let out='';const u8=()=>new Uint8Array(getMem().buffer),dv=()=>new DataView(getMem().buffer);
 const W={fd_write(fd,iov,n,nw){let t=0;const d=dv();for(let i=0;i<n;i++){const p=d.getUint32(iov+8*i,true),l=d.getUint32(iov+8*i+4,true);out+=new TextDecoder().decode(u8().subarray(p,p+l));t+=l}d.setUint32(nw,t,true);let k;while((k=out.indexOf('\n'))>=0){if(window.__emuLog)console.log('[emu]',out.slice(0,k));out=out.slice(k+1)}return 0},
  fd_close:()=>0,fd_seek:()=>70,fd_fdstat_get(fd,p){const d=dv();d.setUint8(p,fd<3?2:0);d.setUint16(p+2,0,true);d.setBigUint64(p+8,0n,true);d.setBigUint64(p+16,0n,true);return fd<3?0:8},
  environ_sizes_get(c,s){const d=dv();d.setUint32(c,0,true);d.setUint32(s,0,true);return 0},environ_get:()=>0,args_sizes_get(c,s){const d=dv();d.setUint32(c,0,true);d.setUint32(s,0,true);return 0},args_get:()=>0,
  clock_time_get(id,pr,p){dv().setBigUint64(p,BigInt(Math.round((id==0?Date.now():performance.now())*1e6)),true);return 0},clock_res_get(id,p){dv().setBigUint64(p,1000n,true);return 0},
  random_get(p,l){crypto.getRandomValues(u8().subarray(p,p+l));return 0},proc_exit(c){throw new Error('the emulator stopped ('+c+')')},sched_yield:()=>0,
  /* no files: the C library must hear "bad descriptor" / "no such file", not "not supported" */
  fd_prestat_get:()=>8,fd_prestat_dir_name:()=>8,fd_read:()=>8,fd_filestat_get:()=>8,fd_readdir:()=>8,path_open:()=>44,path_filestat_get:()=>44,path_unlink_file:()=>44,path_create_directory:()=>44,poll_oneoff:()=>52};
 /* 1.6.1: read-only files for a core that opens its game by name (vfs: {size(name), read(name, offset, length)}): "/" is
    descriptor 3, and the files it names are opened from 10 on */
 if(vfs){const fds={};let nfd=10;const str=(p,l)=>'/'+new TextDecoder().decode(u8().subarray(p,p+l)).replace(/^\/+/,'');
  const stat=(b,size,dir)=>{const d=dv();for(let i=0;i<64;i+=8)d.setBigUint64(b+i,0n,true);d.setUint8(b+16,dir?3:4);d.setBigUint64(b+32,BigInt(size),true)};
  Object.assign(W,{fd_prestat_get(fd,p){if(fd!=3)return 8;const d=dv();d.setUint8(p,0);d.setUint32(p+4,1,true);return 0},fd_prestat_dir_name(fd,p){if(fd!=3)return 8;u8()[p]=47;return 0},
   path_open(dirfd,df,p,l,of,rb,ri,ff,fdp){const n=str(p,l);if(of&(1|4|8))return 76;const z=vfs.size(n);if(z==null)return 44;if(of&2)return 54;fds[nfd]={n,pos:0,size:z};dv().setUint32(fdp,nfd++,true);return 0},
   fd_read(fd,iov,n,nr){const f=fds[fd];if(!f)return 8;let t=0;const d=dv();
    for(let i=0;i<n;i++){const p=d.getUint32(iov+8*i,true),l=Math.min(d.getUint32(iov+8*i+4,true),Math.max(0,f.size-f.pos));if(l<=0)break;const b=vfs.read(f.n,f.pos,l);u8().set(b,p);f.pos+=b.length;t+=b.length;if(b.length<l)break}
    d.setUint32(nr,t,true);return 0},
   fd_seek(fd,off,wh,np){const f=fds[fd];if(!f)return fd<3?70:8;const o=Number(off),v=wh==0?o:wh==1?f.pos+o:f.size+o;if(v<0)return 28;f.pos=v;dv().setBigUint64(np,BigInt(v),true);return 0},
   fd_tell(fd,np){const f=fds[fd];if(!f)return 8;dv().setBigUint64(np,BigInt(f.pos),true);return 0},
   fd_close(fd){delete fds[fd];return 0},fd_filestat_get(fd,b){const f=fds[fd];if(!f)return 8;stat(b,f.size,0);return 0},
   path_filestat_get(dirfd,fl,p,l,b){const n=str(p,l),z=vfs.size(n);if(z==null)return 44;stat(b,z,z<0);return 0},
   fd_fdstat_get(fd,p){const d=dv(),ok=fd<4||fds[fd];d.setUint8(p,fd<3?2:fd==3?3:4);d.setUint16(p+2,0,true);d.setBigUint64(p+8,0xffffffffn,true);d.setBigUint64(p+16,0xffffffffn,true);return ok?0:8}})}
 return {wasi_snapshot_preview1:new Proxy(W,{get:(o,k)=>o[k]||(()=>52)})}}
/* 1.6.1: a PlayStation disc in the game library, read while it plays: /game/<name> are the files beside the game (its
   .cue's tracks), /bios/<name> those in PS1/BIOS. The core reads synchronously, so a missing piece is fetched with a
   synchronous range request (256 KB at a time, kept in a cache of 64), and the piece after it is fetched ahead */
async function emuDiscFs(path,biosDir){const dir=path.replace(/\/[^/]+$/,''),F={};
 const list=async(d,pre,lower)=>{try{for(const it of (await rd('fs/list?path='+encodeURIComponent(d))).items)if(!it.dir)F[pre+(lower?it.name.toLowerCase():it.name)]={real:d+'/'+it.name,size:it.size}}catch(e){}};
 await list(dir,'/game/');if(biosDir)await list(biosDir,'/bios/',true);
 const BS=262144,cache=new Map(),ahead=new Set();let fetched=0;
 const url=r=>'/api/fs/read?path='+encodeURIComponent(r);
 const keep=(k,u)=>{cache.set(k,u);if(cache.size>64)cache.delete(cache.keys().next().value)};
 const sync=(f,a,b)=>{const x=new XMLHttpRequest();x.open('GET',url(f.real),false);x.setRequestHeader('X-Rudolf-Token',RD.token);x.setRequestHeader('Range','bytes='+a+'-'+b);
  x.overrideMimeType('text/plain; charset=x-user-defined');x.send();if(x.status!=206&&x.status!=200)throw new Error('the disc could not be read (HTTP '+x.status+')');
  const t=x.responseText,o=x.status==200?a:0,n=Math.min(b-a+1,t.length-o),u=new Uint8Array(Math.max(0,n));for(let i=0;i<u.length;i++)u[i]=t.charCodeAt(o+i)&255;fetched+=u.length;return u};
 const pre=(f,k,bi)=>{const a=bi*BS;if(a>=f.size||cache.has(k)||ahead.has(k))return;ahead.add(k);
  fetch(url(f.real),{headers:{'X-Rudolf-Token':RD.token,Range:'bytes='+a+'-'+(Math.min(f.size,a+BS)-1)}}).then(r=>r.ok?r.arrayBuffer():null).then(b=>{ahead.delete(k);if(b&&!cache.has(k)){keep(k,new Uint8Array(b));fetched+=b.byteLength}}).catch(()=>ahead.delete(k))};
 const block=(n,f,bi)=>{const k=n+'#'+bi;let u=cache.get(k);if(!u){u=sync(f,bi*BS,Math.min(f.size,(bi+1)*BS)-1);keep(k,u)}else{cache.delete(k);cache.set(k,u)}pre(f,n+'#'+(bi+1),bi+1);return u};
 const look=n=>F[n]||(n.startsWith('/bios/')?F[n.toLowerCase()]:null);
 return {files:F,fetched:()=>fetched,
  size(n){if(n=='/'||n=='/game'||n=='/bios')return -1;const f=look(n);return f?f.size:null},
  read(n,pos,len){const f=look(n);if(!f)return new Uint8Array(0);len=Math.min(len,f.size-pos);const out=new Uint8Array(Math.max(0,len));let d=0;
   while(d<len){const bi=Math.floor((pos+d)/BS),u=block(n,f,bi),o=pos+d-bi*BS,c=Math.min(len-d,u.length-o);if(c<=0)break;out.set(u.subarray(o,o+c),d);d+=c}return d<len?out.subarray(0,d):out}}}
const EMU_MOD={};
async function emuModule(sys){const c=EMU_SYS[sys].core;if(EMU_MOD[c])return EMU_MOD[c];
 let bytes=null;
 const get=async u=>{try{const r=await fetch(u);if(r.ok){const b=await r.arrayBuffer();if(b.byteLength>4&&new Uint32Array(b,0,1)[0]==0x6d736100)return b}}catch(e){}return null};
 if(RD&&typeof libBase=='function'){bytes=await get(libBase('rudolf-emu')+c+'.wasm');
  /* the DS core is only in the package: ask to download it (SNES falls back to the inlined core) */
  if(!bytes&&c!='lakesnes'&&await libNeed('rudolf-emu'))bytes=await get(libBase('rudolf-emu')+c+'.wasm')}
 if(!bytes&&!RD)bytes=await get('lib/emu/'+c+'.wasm');   /* the browser version finds the cores next to the page */
 /* (1.5.9: the SNES core is a file beside the page, fetched when a SNES game is first played: plan 10.1) */
 if(!bytes&&c=='lakesnes'&&typeof EMU_SNES_URL=='string')bytes=await get(EMU_SNES_URL);
 if(!bytes&&c=='lakesnes'&&typeof EMU_SNES_WASM=='string'){const b=atob(EMU_SNES_WASM);bytes=new Uint8Array(b.length);for(let i=0;i<b.length;i++)bytes[i]=b.charCodeAt(i)}
 /* (1.6.1: the Genesis and PlayStation cores came with the package's 1.1.0) */
 if(!bytes&&RD&&c!='lakesnes'&&typeof libHas=='function'&&await libHas('rudolf-emu').catch(()=>false))throw new Error(EMU_SYS[sys].name+' games need a newer Emu player: update “Emu” in the App Store (Rudolf OS Library).');
 if(!bytes)throw new Error(RD?EMU_SYS[sys].name+' games need Emu’s player from the Rudolf OS Library (App Store › Rudolf OS Library).':EMU_SYS[sys].name+' games need Rudolf OS itself (the bootable disc or USB stick).');
 return EMU_MOD[c]=await WebAssembly.compile(bytes)}
async function emuInstance(sys,vfs){const m=await emuModule(sys);let inst;inst=await WebAssembly.instantiate(m,emuWasi(()=>inst.exports.memory,vfs));if(inst.exports._initialize)inst.exports._initialize();return inst.exports}
/* ---------- where a game's saves and states are kept ---------- */
function emuStore(g){
 if(g.real){const dir=g.real.replace(/\/[^/]+$/,''),base=g.real.split('/').pop().replace(/\.[^.]+$/,''),lib=dir.startsWith(EMU_GAMES+'/'),top=lib?EMU_GAMES+'/'+dir.slice(EMU_GAMES.length+1).split('/')[0]:dir,sd=lib?top+'/Saves':dir,td=lib?top+'/States':dir;   /* (1.6.1: a disc in a folder of its own, PS1/<name>/, saves in PS1/Saves) */
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
async function emuPlay(g){   /* g: {sys, name, bytes, real?, drive?} (1.6.1: a disc has no bytes: it is read from g.real as it plays) */
 const sy=EMU_SYS[g.sys],lr=sy.lr;let x,disc=null;
 try{if(lr&&lr.disc){if(!RD||!g.real)throw new Error(sy.name+' games play from the game library of Rudolf OS: add it there first (Game library › Add games…).');disc=await emuDiscFs(g.real,EMU_GAMES+'/'+sy.dir+'/BIOS')}
  x=await emuInstance(g.sys,disc)}catch(e){notify('Emu',e.message,{kind:'error'});return}
 const st=emuStore(g),title=g.title||g.name.replace(/\.[^.]+$/,'');
 const W=win('Emu — '+title,'🦤',g.sys=='ds'?520:lr?680:640,g.sys=='ds'?720:lr?580:560);W.body.classList.add('emuw');W.emu=true;EMU.players.add(W);
 W.body.innerHTML=`<div class='emutb'><button class='elib' title='Game library'>🎮 Library</button><button class='epp' title='Pause (P)'>⏸</button><button class='ers' title='Reset'>↺</button>
  <button class='ess' title='Save state'>💾 Save</button><button class='els' title='Load state'>📂 Load</button><select class='eslot' title='State slot'>${[1,2,3,4,5,6,7,8,9].map(i=>`<option>${i}</option>`).join('')}</select>
  ${g.sys=='ds'?"<select class='elay' title='Screen layout'><option value='stack'>Stacked</option><option value='side'>Side by side</option><option value='large'>Large + small</option></select><button class='eswap' title='Swap screens (large + small)'>⇅</button>":''}
  <button class='ectl' title='Controls'>🎮 Controls</button><button class='ebl' title='Borderless (Alt+Enter)'>⛶</button><button class='emt' title='Mute'>🔊</button><span class='emsg'></span></div>
  <div class='emust'><canvas class='emucv' tabindex='0'></canvas><div class='emupad' hidden></div><div class='emumenu' hidden></div><div class='emucorner' title='Menu'></div></div>`;
 const q=s=>W.body.querySelector(s),cv=q('.emucv'),ctx=cv.getContext('2d'),stage=q('.emust'),msg=t=>{q('.emsg').textContent=t};
 let ctl=emuCtl(g.sys),held=new Set(),pads=new Set(),axes=[0,0,0,0],touch=null,paused=false,muted=false,ff=false,alive=true,acc=0,last=0,raf=0,frames=0;
 let layout=g.sys=='ds'?(ld('bd95emulayout','stack')):'', swap=false;
 /* load the game (and its battery save) */
 const rom=disc?null:g.bytes instanceof Uint8Array?g.bytes:new Uint8Array(g.bytes);
 const sav=await st.get('sav');let ok;
 const put=(b,p)=>new Uint8Array(x.memory.buffer,p,b.length).set(b);
 if(lr){x.lr_formats(lr.fmt);for(const o of lr.opts||[]){const b=new TextEncoder().encode(o);put(b,x.lr_alloc(b.length));x.lr_option(b.length)}x.lr_init();
  if(disc){const b=new TextEncoder().encode('/game/'+g.real.split('/').pop());put(b,x.lr_alloc(b.length));try{ok=x.lr_load(b.length,1)}catch(e){ok=0;g.err='This disc could not be started: '+e.message}}
  else{const r=g.sys=='md'?emuMdRom(rom):rom;put(r,x.lr_alloc(r.length));ok=x.lr_load(r.length,0)}
  if(ok){if(lr.dev)x.lr_device(0,lr.dev);const n=x.lr_save_size();if(n&&sav&&sav.length)put(sav.subarray(0,n),x.lr_save_ptr())}
  else if(!g.err)g.err=disc?'This does not look like a '+sy.name+' disc (or a file it names is missing beside it).':'This does not look like a '+sy.name+' game.'}
 else if(g.sys=='snes'){const p=x.ls_alloc(rom.length);new Uint8Array(x.memory.buffer,p,rom.length).set(rom);ok=x.ls_load(rom.length);
  if(ok&&sav&&sav.length){const b=x.ls_alloc(sav.length);new Uint8Array(x.memory.buffer,b,sav.length).set(sav);x.ls_battery_load(sav.length)}}
 else{x.ds_init(48000);if(sav&&sav.length){const p=x.ds_sav_ptr(sav.length);new Uint8Array(x.memory.buffer,p,sav.length).set(sav)}
  const p=x.ds_rom_buffer(rom.length);new Uint8Array(x.memory.buffer,p,rom.length).set(rom);const r=x.ds_load();ok=r==0;
  if(!ok)g.err=r==-1001?'This DS game image is encrypted (not a usable dump). Make a decrypted dump of your game.':'This DS game could not be started.'}
 if(!ok){W.close();notify('Emu',g.err||'This does not look like a '+sy.name+' game.',{kind:'error'});return}
 emuRecent(g);msg(sav&&sav.length?'Save loaded':'');
 /* picture */
 const SW=256,SH=g.sys=='ds'?384:224;let img=null;
 const size=()=>{let cw,ch;if(lr){cw=x.lr_w()||320;ch=x.lr_h()||240}else if(g.sys=='snes'){cw=512;ch=448}else if(layout=='side'){cw=512;ch=192}else if(layout=='large'){cw=341;ch=192}else{cw=256;ch=384}
  if(cv.width!=cw||cv.height!=ch){cv.width=cw;cv.height=ch}
  const r=stage.getBoundingClientRect(),ar=lr?x.lr_aspect():g.sys=='snes'?(ld('bd95emusnesar','4:3')=='8:7'?8/7:4/3):cw/ch;let w=r.width,h=w/ar;if(h>r.height){h=r.height;w=h*ar}
  if(ld('bd95emuint',false)&&g.sys=='ds'){const k=Math.max(1,Math.floor(Math.min(r.width/cw,r.height/ch)));w=cw*k;h=ch*k}
  cv.style.width=Math.floor(w)+'px';cv.style.height=Math.floor(h)+'px'};
 const off=document.createElement('canvas');off.width=256;off.height=384;const octx=off.getContext('2d');
 let lrKey='';
 const draw=()=>{
  if(lr){const w=x.lr_w(),h=x.lr_h(),pp=x.lr_pixels();if(!pp||!w||!h)return;const k=pp+'x'+w+'x'+h;if(cv.width!=w||cv.height!=h)size();
   if(k!=lrKey||!img||img.data.buffer!==x.memory.buffer){lrKey=k;img=new ImageData(new Uint8ClampedArray(x.memory.buffer,pp,w*h*4),w,h)}ctx.putImageData(img,0,0);return}
  if(g.sys=='snes'){const px=new Uint8ClampedArray(x.memory.buffer,x.ls_pixels(),512*448*4);for(let i=3;i<px.length;i+=4)px[i]=255;if(!img||img.data.buffer!==x.memory.buffer)img=new ImageData(px,512,448);ctx.putImageData(img,0,0);return}
  const px=new Uint8ClampedArray(x.memory.buffer,x.ds_frame_ptr(),256*384*4);if(!img||img.data.buffer!==x.memory.buffer)img=new ImageData(px,256,384);
  if(layout=='stack')ctx.putImageData(img,0,0);
  else if(layout=='side'){ctx.putImageData(img,0,0,0,0,256,192);ctx.putImageData(img,256,-192,0,192,256,192)}
  else{octx.putImageData(img,0,0);const big=swap?0:192,small=swap?192:0;ctx.fillStyle='#000';ctx.fillRect(0,0,341,192);ctx.drawImage(off,0,big,256,192,85,0,256,192);ctx.drawImage(off,0,small,256,192,0,0,85,64)}};
 /* where the DS's lower screen is on the canvas (for touch) */
 const lowerRect=()=>layout=='side'?[256,0,256,192]:layout=='large'?(swap?[0,0,85,64]:[85,0,256,192]):[0,192,256,192];
 /* sound */
 let ac=null,gain=null,nextT=0;const ensureAudio=()=>{if(ac)return;try{ac=new (window.AudioContext||window.webkitAudioContext)({sampleRate:48000});gain=ac.createGain();gain.connect(ac.destination);gain.gain.value=muted?0:1}catch(e){ac=null}};
 const rate=lr?x.lr_rate():48000;
 const play=(s16,n)=>{if(!ac||muted||n<=0)return;if(ac.state=='suspended')ac.resume().catch(()=>{});const t=ac.currentTime;if(nextT<t)nextT=t+0.04;if(nextT>t+0.25)return;
  const b=ac.createBuffer(2,n,rate),l=b.getChannelData(0),r=b.getChannelData(1);for(let i=0;i<n;i++){l[i]=s16[2*i]/32768;r[i]=s16[2*i+1]/32768}const s=ac.createBufferSource();s.buffer=b;s.connect(gain);s.start(nextT);nextT+=n/rate};
 /* input */
 const down=b=>held.has(b)||pads.has(b);
 /* 1.6.1: a console with sticks (the PlayStation's DualShock) gets them as they are; for the others the left stick is the d-pad.
    Button 10 (the left stick's click) fast-forwards when the console does not use it */
 const LAY=emuLay(g.sys),dz=v=>Math.abs(v)<.15?0:Math.max(-32767,Math.min(32767,Math.round(v*32767)));
 const pollPads=()=>{pads=new Set();axes=[0,0,0,0];const used=new Set(ctl.btn.map(b=>ctl.pad[b]));
  for(const p of (navigator.getGamepads?navigator.getGamepads():[])){if(!p)continue;ctl.btn.forEach(b=>{const i=ctl.pad[b];if(p.buttons[i]&&p.buttons[i].pressed)pads.add(b)});
   const ax=p.axes||[];if(LAY.sticks){for(let i=0;i<4;i++)if(dz(ax[i]||0))axes[i]=dz(ax[i])}else{if(ax[0]<-.5)pads.add('Left');if(ax[0]>.5)pads.add('Right');if(ax[1]<-.5)pads.add('Up');if(ax[1]>.5)pads.add('Down')}
   if(!used.has(10)&&p.buttons[10]&&p.buttons[10].pressed)ff=true;if(p.buttons[16]&&p.buttons[16].pressed&&!W._gm){W._gm=1;menu()}else if(!(p.buttons[16]&&p.buttons[16].pressed))W._gm=0}
  if(pads.size)q('.emupad').hidden=true};
 let lastBits=0;
 const stickKeys=()=>{if(!LAY.stick)return;if(held.has('SLeft'))axes[0]=-32767;if(held.has('SRight'))axes[0]=32767;if(held.has('SUp'))axes[1]=-32767;if(held.has('SDown'))axes[1]=32767};
 const step=draw1=>{
  if(lr){let k=0;ctl.btn.forEach(b=>{if(down(b))k|=1<<LAY.bit[b]});lastBits=k;stickKeys();x.lr_input(0,k,axes[0],axes[1],axes[2],axes[3]);x.lr_frame();if(draw1)draw();if(!ff)play(new Int16Array(x.memory.buffer,x.lr_sound(),x.lr_samples()*2),x.lr_samples())}
  else if(g.sys=='snes'){EMU_BTN.forEach(b=>x.ls_button(1,EMU_SNESIDX[b],down(b)?1:0));x.ls_frame(800);if(draw1)draw();if(!ff)play(new Int16Array(x.memory.buffer,x.ls_sound(),1600),800)}
  else{let k=0;EMU_BTN.forEach(b=>{if(down(b))k|=1<<EMU_DSBIT[b]});x.ds_frame(draw1?1:0,k,touch?1:0,touch?touch[0]:0,touch?touch[1]:0);const n=x.ds_audio(802);if(draw1)draw();if(!ff)play(new Int16Array(x.memory.buffer,x.ds_audio_ptr(),n*2),n)}
  frames++};
 const fps=lr?x.lr_fps()||60:sy.fps;
 const loop=t=>{if(!alive)return;raf=requestAnimationFrame(loop);if(paused||!W.el.isConnected){last=t;return}pollPads();
  if(!last)last=t;acc+=Math.min(100,t-last)*(ff?4:1);last=t;const fd=1000/fps,mx=ff?8:3;let n=0;while(acc>=fd&&n<mx){acc-=fd;n++;step(acc<fd||n==mx)}if(n==mx)acc=Math.min(acc,fd);if(n==0)return;   /* too slow: draw the last frame and drop the backlog */
  if(frames%60==0)lastInput=Date.now()};   /* no screensaver while playing */
 /* battery saves: written when they change (checked every 3 s) */
 /* (1.6.1: compared by a checksum: a memory card is 128 KB) */
 const sum=b=>{let h=2166136261;for(let i=0;i<b.length;i++)h=Math.imul(h^b[i],16777619);return b.length+':'+(h>>>0)};
 let lastSav=sav?sum(sav):'';
 if(lr&&!lastSav){const n=x.lr_save_size();if(n)lastSav=sum(new Uint8Array(x.memory.buffer,x.lr_save_ptr(),n))}   /* an empty card or memory is not saved */
 const saveBattery=async()=>{try{let b=null;if(lr){const n=x.lr_save_size(),p=x.lr_save_ptr();if(n>0&&p)b=new Uint8Array(x.memory.buffer,p,n).slice()}else if(g.sys=='snes'){const n=x.ls_battery_save();if(n>0)b=new Uint8Array(x.memory.buffer,x.ls_buf(),n).slice()}else if(x.ds_sav_changed()){const n=x.ds_sav_size();if(n>0)b=new Uint8Array(x.memory.buffer,x.ds_sav_ptr(0),n).slice()}
  if(!b)return;const s=sum(b);if(s==lastSav)return;lastSav=s;await st.put('sav',b);msg('Saved')}catch(e){msg('Save failed: '+e.message)}};
 const savIv=setInterval(saveBattery,3000);
 /* save states */
 const slot=()=>q('.eslot').value;
 const saveState=async()=>{try{const n=lr?x.lr_state_save():g.sys=='snes'?x.ls_state_save():x.ds_state_save();if(!n)throw new Error('the emulator could not save');const b=new Uint8Array(x.memory.buffer,lr?x.lr_buf():g.sys=='snes'?x.ls_buf():x.ds_state_buf(0),n).slice();
  await st.put('st'+slot(),b);const th=await new Promise(r=>cv.toBlob(r,'image/png'));if(th)await st.put('st'+slot()+'png',th);msg('State '+slot()+' saved')}catch(e){msg('Could not save the state: '+e.message)}};
 const loadState=async()=>{const b=await st.get('st'+slot());if(!b){msg('Slot '+slot()+' is empty');return}let r;
  if(lr){put(b,x.lr_alloc(b.length));r=x.lr_state_load(b.length)}else if(g.sys=='snes'){const p=x.ls_alloc(b.length);new Uint8Array(x.memory.buffer,p,b.length).set(b);r=x.ls_state_load(b.length)}else{const p=x.ds_state_buf(b.length);new Uint8Array(x.memory.buffer,p,b.length).set(b);r=x.ds_state_load()}
  msg(r?'State '+slot()+' loaded':'That state could not be loaded');draw()};
 /* borderless mode and its menu */
 const setBl=on=>{W.el.classList.toggle('emubl',on);document.body.classList.toggle('emu-borderless',on);W.bl=on;setTimeout(size,30);if(on)cv.focus();else q('.emumenu').hidden=true};
 const menu=()=>{const m=q('.emumenu');if(!m.hidden){m.hidden=true;paused=false;return}paused=true;
  m.innerHTML=`<div class='emumbox'><b></b><button data-a='res'>▶ Resume</button><button data-a='ss'>💾 Save state ${slot()}</button><button data-a='ls'>📂 Load state ${slot()}</button><button data-a='bl'>${W.bl?'⬜ Leave borderless':'⛶ Borderless'}</button><button data-a='ctl'>🎮 Controls</button><button data-a='quit'>✕ Close the game</button></div>`;
  m.querySelector('b').textContent=title;m.hidden=false;
  m.onclick=e=>{const a=e.target.dataset&&e.target.dataset.a;if(!a)return;m.hidden=true;paused=false;if(a=='ss')saveState();if(a=='ls')loadState();if(a=='bl')setBl(!W.bl);if(a=='ctl')emuControls(g.sys,()=>{ctl=emuCtl(g.sys)});if(a=='quit')W.close()}};
 q('.emucorner').onclick=menu;
 /* on-screen buttons for the touchscreen */
 const padEl=q('.emupad');padEl.innerHTML=emuPadHtml(g.sys);
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
 const keyOf=c=>ctl.btn.concat(Object.keys(LAY.stick||{})).find(b=>ctl.keys[b]==c);
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
 q('.ers').onclick=()=>{lr?x.lr_reset():g.sys=='snes'?x.ls_reset(0):x.ds_reset();cv.focus()};
 q('.ess').onclick=()=>{saveState();cv.focus()};q('.els').onclick=()=>{loadState();cv.focus()};
 q('.ectl').onclick=()=>emuControls(g.sys,()=>{ctl=emuCtl(g.sys)});
 q('.ebl').onclick=()=>setBl(true);
 q('.emt').onclick=()=>{muted=!muted;q('.emt').textContent=muted?'🔇':'🔊';if(gain)gain.gain.value=muted?0:1};
 if(g.sys=='ds'){const sl=q('.elay');sl.value=layout;sl.onchange=()=>{layout=sl.value;svj('bd95emulayout',layout);img=null;size();draw();cv.focus()};q('.eswap').onclick=()=>{swap=!swap;if(layout!='large'){layout='large';sl.value='large';svj('bd95emulayout',layout)}size();draw();cv.focus()}}
 const ro=new ResizeObserver(()=>size());ro.observe(stage);
 W.onclose=()=>{alive=false;cancelAnimationFrame(raf);clearInterval(savIv);saveBattery();ro.disconnect();if(W.bl)setBl(false);if(ac)ac.close().catch(()=>{});EMU.players.delete(W)};
 W.emuApi={x,disc,axes:()=>axes,bits:()=>lastBits,step,draw,setBl,menu,saveState,loadState,slot:v=>{q('.eslot').value=v},touch:v=>{touch=v},hold:(b,v)=>{v?held.add(b):held.delete(b)},layout:v=>{q('.elay').value=v;q('.elay').onchange()},paused:()=>paused,frames:()=>frames,store:st,lowerRect,canvas:cv};
 if(ld('bd95emubl',false))setBl(true);
 size();draw();cv.focus();raf=requestAnimationFrame(loop);
 if('ontouchstart' in window&&!(navigator.getGamepads&&[...navigator.getGamepads()].some(Boolean)))showPad();
 return W}
/* 1.6.1: the on-screen buttons of each console */
function emuPadHtml(sys){const dp=`<div class='edp'><b data-b='Up'>▲</b><b data-b='Left'>◀</b><b data-b='Right'>▶</b><b data-b='Down'>▼</b></div>`;
 if(sys=='md')return `<div class='epl'>${dp}</div><div class='epm'><b data-b='Mode'>MODE</b><b data-b='Start'>START</b></div>
  <div class='epr'><div class='eab6'><b data-b='X'>X</b><b data-b='Y'>Y</b><b data-b='Z'>Z</b><b data-b='A'>A</b><b data-b='B'>B</b><b data-b='C'>C</b></div></div>`;
 if(sys=='ps1')return `<div class='epl'><div class='eshd'><b data-b='L2'>L2</b><b data-b='L1'>L1</b></div>${dp}</div><div class='epm'><b data-b='Select'>SELECT</b><b data-b='Start'>START</b></div>
  <div class='epr'><div class='eshd'><b data-b='R2'>R2</b><b data-b='R1'>R1</b></div><div class='eab eps'><b data-b='Triangle'>△</b><b data-b='Square'>□</b><b data-b='Circle'>○</b><b data-b='Cross'>✕</b></div></div>`;
 return `<div class='epl'><b data-b='L'>L</b>${dp}</div>
  <div class='epm'><b data-b='Select'>SELECT</b><b data-b='Start'>START</b></div><div class='epr'><b data-b='R'>R</b><div class='eab'><b data-b='X'>X</b><b data-b='Y'>Y</b><b data-b='A'>A</b><b data-b='B'>B</b></div></div>`}
/* 1.6.1: a Genesis game as the core takes it: an .smd (a 512-byte header, then 16 KB blocks with the odd bytes first)
   is put back in order */
function emuMdRom(r){if(r.length%16384!=512||r.length<16896)return r;const at=(o,t)=>String.fromCharCode(...r.subarray(o,o+t.length))==t;if(at(0x100,'SEGA')||at(0x300,'SEGA'))return r;
 const n=r.length-512,o=new Uint8Array(n);for(let b=0;b<n;b+=16384)for(let i=0;i<8192;i++){o[b+2*i]=r[512+b+8192+i];o[b+2*i+1]=r[512+b+i]}return o}
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
/* 1.6.1: a Genesis game's name, from its header (0x150 the overseas name, 0x120 the Japanese one) */
function emuMdTitle(b){const at=(o,t)=>String.fromCharCode(...b.subarray(o,o+t.length))==t;if(!at(0x100,'SEGA')&&!at(0x101,'SEGA'))return '';
 const nm=o=>String.fromCharCode(...b.subarray(o,o+48)).replace(/[^\x20-\x7e]/g,' ').replace(/\s+/g,' ').trim();const t=nm(0x150)||nm(0x120);
 return t&&t==t.toUpperCase()?t.toLowerCase().replace(/\b[a-z]/g,c=>c.toUpperCase()):t}
async function emuInfo(sys,path,size){if(sys=='md'){const b=await emuRange(path,0,Math.min(size,0x200)-1);return {title:emuMdTitle(b)}}
 if(sys=='snes'){const b=await emuRange(path,0,Math.min(size,0x10200)-1);return {title:emuSnesTitle(b,size)}}
 const h=await emuRange(path,0,0x1FF);const bo=h[0x68]|h[0x69]<<8|h[0x6A]<<16|h[0x6B]<<24;let ban=null;if(bo&&bo+0x840<=size)try{ban=await emuRange(path,bo,bo+0x83F)}catch(e){}return emuDsInfo(h,ban)}
/* ---------- the library ---------- */
const emuLibK='bd95emulib';
async function emuScan(){const cache=ld(emuLibK,{}),out=[];
 /* 1.6.1: Nintendo 64 games (DC/ and N64/ hold no saves: the native emulators keep their own); a Dreamcast disc is a
    .cdi or .chd in DC/, or a folder there with its .gdi and tracks */
 const nice=n=>n.replace(/\.[^.]+$/,'').replace(/_/g,' ').replace(/ {2,}/g,' ').trim();
 /* (1.6.1: PlayStation discs the same way: a .chd or .pbp in PS1/, or a folder there with its .cue and tracks) */
 for(const sys of ['n64','dc','ps1']){const dir=EMU_GAMES+'/'+EMU_SYS[sys].dir,set=sys=='ps1'?/\.cue$/i:/\.gdi$/i;let items=[];try{items=(await rd('fs/list?path='+encodeURIComponent(dir))).items}catch(e){}
  for(const it of items){
   if(!it.dir&&EMU_SYS[sys].re.test(it.name)&&!set.test(it.name)){const path=dir+'/'+it.name;out.push({sys,path,real:path,name:it.name,size:it.size,title:nice(it.name),icon:''});continue}
   if(it.dir&&sys!='n64'&&!/^(Saves|States|BIOS)$/.test(it.name)){let sub=[];try{sub=(await rd('fs/list?path='+encodeURIComponent(dir+'/'+it.name))).items}catch(e){}
    const gdi=sub.find(x=>!x.dir&&set.test(x.name));if(gdi){const path=dir+'/'+it.name+'/'+gdi.name;out.push({sys,path,real:path,name:gdi.name,size:sub.reduce((t,x)=>t+(x.dir?0:x.size||0),0),title:nice(it.name),icon:''})}}}}
 for(const sys of ['snes','ds','md']){const dir=EMU_GAMES+'/'+EMU_SYS[sys].dir;let items=[];try{items=(await rd('fs/list?path='+encodeURIComponent(dir))).items}catch(e){}
  for(const it of items){if(it.dir||!EMU_SYS[sys].re.test(it.name))continue;const path=dir+'/'+it.name;let c=cache[path];
   if(!c||c.size!=it.size||c.mtime!=it.mtime){c={size:it.size,mtime:it.mtime,title:'',icon:''};try{Object.assign(c,await emuInfo(sys,path,it.size))}catch(e){}cache[path]=c}
   out.push({sys,path,name:it.name,size:it.size,title:c.title||it.name.replace(/\.[^.]+$/,''),icon:c.icon||''})}}
 /* 1.5.2: Game Boy games added to the library play in the Game Boy player (base_v24.html gbPlayer) */
 {const dir=EMU_GAMES+'/GB';let items=[];try{items=(await rd('fs/list?path='+encodeURIComponent(dir))).items}catch(e){}
  for(const it of items)if(!it.dir&&EMU_GB_RE.test(it.name)){const path=dir+'/'+it.name;out.push({sys:'gb',path,real:path,name:it.name,size:it.size,title:it.name.replace(/\.[^.]+$/,'').replace(/_/g,' ').replace(/ {2,}/g,' ').trim(),icon:''})}}
 for(const k of Object.keys(cache))if(!out.some(o=>o.path==k))delete cache[k];try{svj(emuLibK,cache)}catch(e){}return out}
const EMU_GB_RE=/\.(gb|gbc|gba)$/i;
async function emuReadReal(path){const r=await fetch('/api/fs/read?path='+encodeURIComponent(path),{headers:{'X-Rudolf-Token':RD.token}});if(!r.ok)throw new Error('could not read the game (HTTP '+r.status+')');return new Uint8Array(await r.arrayBuffer())}
/* 1.6.1: which console a game is for, by its name (and, for a .chd or a .bin, where it is in the library) */
function emuSysOf(p){const l=p.toLowerCase(),top=p.startsWith(EMU_GAMES+'/')?p.slice(EMU_GAMES.length+1).split('/')[0]:'';
 if(/\.nds$/.test(l))return 'ds';if(/\.(z64|n64|v64)$/.test(l))return 'n64';if(/\.(gdi|cdi)$/.test(l))return 'dc';if(/\.(md|gen|smd)$/.test(l))return 'md';if(/\.(cue|pbp)$/.test(l))return 'ps1';
 if(/\.chd$/.test(l))return top=='PS1'?'ps1':'dc';if(/\.bin$/.test(l))return top=='PS1'?'ps1':'md';return 'snes'}
async function emuOpenReal(path){const sys=emuSysOf(path),name=path.split('/').pop();
 if(emuNat(sys))return emuNative({sys,path,real:path,name,title:name.replace(/\.[^.]+$/,'')});
 if(EMU_SYS[sys].lr&&EMU_SYS[sys].lr.disc)return emuPlay({sys,name,real:path,title:path.startsWith(EMU_GAMES+'/PS1/')&&path.split('/').length>6?path.split('/').at(-2):name.replace(/\.[^.]+$/,'')});
 try{const r=await fetch('/api/fs/read?path='+encodeURIComponent(path),{headers:{'X-Rudolf-Token':RD.token}});if(!r.ok)throw new Error('could not read the game (HTTP '+r.status+')');
  const c=(ld(emuLibK,{})[path]||{});return emuPlay({sys,name:path.split('/').pop(),title:c.title,bytes:new Uint8Array(await r.arrayBuffer()),real:path})}catch(e){notify('Emu',e.message,{kind:'error'})}}
async function emuOpenDrive(p){const v=node(p);if(typeof v!='string')return;const sys=emuSysOf(p.at(-1));
 const u=v.startsWith('data:')?new Uint8Array(await (await fetch(v)).arrayBuffer()):Uint8Array.from(v,c=>c.charCodeAt(0)&255);
 return emuPlay({sys,name:p.at(-1),bytes:u,drive:p})}
/* 1.6.1 (the owner, 2026-11-20: a 693 MB ZIP was refused, "larger than 512 MB; copy it in Files instead"): discs, ZIPs and
   large files chosen with "Open a file…" are not read into the page: the picker hands over their place on the disk
   (picker26.js data-rf-real), and Emu adds them to the game library (rudolfd games/import: a ZIP is unpacked, a .gdi or
   .cue with its tracks) and plays one that was added; a cartridge in a usual place plays from where it is */
const EMU_REAL_RE='\\.(zip|chd|cdi|gdi|cue|pbp|z64|n64|v64|7z)$';
async function emuOpenPaths(paths){const p=paths[0],name=p.split('/').pop(),guest=USER&&USER.r=='guest';
 if(paths.every(x=>x.startsWith(EMU_GAMES+'/')))return emuOpenReal(p);
 if(/\.7z$/i.test(name)){notify('Emu',name+' is a 7-Zip file, which Emu cannot unpack. Unpack it first (or use a ZIP), then add the game.',{kind:'error'});return}
 if(paths.length==1&&/\.(gb|gbc|gba|sfc|smc|nds|md|gen|smd)$/i.test(name))return emuOpenReal(p);
 if(guest){notify('Emu','Discs and ZIP files play from the game library, which the Guest account cannot add to. Sign in to your own account.',{kind:'error'});return}
 if(!paths.every(x=>/^\/(home\/rudolf\/(Downloads|Documents)|media)\//.test(x))){notify('Emu','Games are added from Downloads, Documents or a USB drive. Copy '+name+' to Downloads first.',{kind:'error'});return}
 if(!await emuAsk('Add to the game library?',`<p>${lxEsc(paths.length>1?paths.length+' files':name)} ${paths.length>1?'are':'is'} added to Emu's game library${/\.zip$/i.test(name)?' (the ZIP is unpacked)':''}, then ${paths.length>1?'they play':'it plays'} from there. The original stays where it is.</p>`,'Add'))return;
 try{await lxApi('games/import',paths.join('\n'))}catch(e){notify('Emu',e.message,{kind:'error'});return}
 const W=win('Adding to the game library','🦤',460,190);W.body.innerHTML=`<p class='msg eanote'>Starting…</p><div class='dsbar'><i style='width:3%'></i></div><pre class='msg eares' style='white-space:pre-wrap;max-height:70px;overflow:auto'></pre>`;
 const note=W.body.querySelector('.eanote'),bar=W.body.querySelector('.dsbar i'),res=[],added=[];
 const j=await lxJobWait(l=>{let m;if((m=/^progress (\d+) (\d+) (.*)/.exec(l))){note.textContent=`Copying ${m[3]} (${m[1]} of ${m[2]})…`;bar.style.width=Math.max(3,m[1]/m[2]*90)+'%'}
  else if((m=/^added (.*)/.exec(l))){added.push(m[1]);res.push('✓ '+m[1])}else if(/^skip /.test(l))res.push('– '+l.slice(5))});
 bar.style.width='100%';W.body.querySelector('.eares').textContent=res.join('\n');const last=(j.log||'').trim().split('\n').pop()||'';
 note.textContent=/^done /.test(last)?(added.length?'Added: '+last.slice(5)+'.':'Nothing was added: '+(res[0]||last.slice(5))):'Adding stopped: '+last;
 try{EMU_LIBW&&wins.includes(EMU_LIBW)&&EMU_LIBW.refresh()}catch(e){}
 if(added.length==1){const a=added[0],full=EMU_GAMES+'/'+a;setTimeout(()=>{try{W.close()}catch(e){}emuOpenReal(full)},900)}}
function emuPickFile(){const i=document.createElement('input');i.type='file';i.accept='.sfc,.smc,.nds,.gb,.gbc,.gba,.md,.gen,.smd,.bin,.zip,.chd,.cdi,.gdi,.cue,.pbp,.z64,.n64,.v64';if(RD){i.dataset.rfReal=EMU_REAL_RE;i.addEventListener('rfreal',e=>emuOpenPaths(e.detail.paths))}i.onchange=async()=>{const f=i.files[0];if(!f)return;if(/\.(zip|7z)$/i.test(f.name)){notify('Emu','Unpack '+f.name+' first, then open the game inside it.',{kind:'error'});return}
 if(EMU_GB_RE.test(f.name)){const P=typeof gbPlayer=='function'&&gbPlayer();if(P&&P.loadAny)P.loadAny(new Uint8Array(await f.arrayBuffer()),f.name);return}
 const sys=emuSysOf(f.name);if(emuNat(sys)||EMU_SYS[sys].lr&&EMU_SYS[sys].lr.disc){notify('Emu',EMU_SYS[sys].name+' games play from the game library: add them with “Add games…”.',{kind:'error'});return}
 emuPlay({sys,name:f.name,bytes:new Uint8Array(await f.arrayBuffer())})};i.click()}
let EMU_LIBW=null;
async function emuLibrary(){if(EMU_LIBW&&wins.includes(EMU_LIBW)){EMU_LIBW.focus();EMU_LIBW.refresh();return EMU_LIBW}
 const W=win('Emu — Game library','🦤',760,560);EMU_LIBW=W;W.body.classList.add('emulib');
 const guest=!USER||USER.r=='guest';
 W.body.innerHTML=`<div class='elhead'><div class='eltabs' role='tablist'>${[['all','All'],['recent','Recently played'],['fav','★ Favourites'],['gb','Game Boy / GBA'],['snes','Super Nintendo'],['ds','Nintendo DS'],['n64','Nintendo 64'],['md','Genesis'],['ps1','PlayStation'],['dc','Dreamcast']].map(([k,t])=>`<button data-t='${k}' role='tab'>${t}</button>`).join('')}</div>
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
  if(!l.length){grid.innerHTML=`<div class='msg elempty'>${tab=='recent'?'Games you play show up here.':tab=='fav'?'Press and hold a game (or right-click) to make it a favourite.':(RD?'No games here yet. '+(guest?'':'Use “Add games…” to copy your Game Boy, SNES, DS, Nintendo 64, Genesis, PlayStation and Dreamcast games from a USB stick or Downloads.'):'In this browser, open a SNES or Genesis game with “Open a file…”. The other consoles need Rudolf OS itself.')}</div>`;return}
  l.forEach(g=>{const t=document.createElement('button');t.className='eltile';const last=rec.find(r=>r.k==emuKey(g)||r.k==g.path);
   t.innerHTML=`<span class='elic'>${g.icon?`<img src='${g.icon}' alt=''>`:g.sys=='gb'?'🟩':EMU_SYS[g.sys].ic}</span><b></b><small></small>`;t.querySelector('b').textContent=g.title;
   t.querySelector('small').textContent=(g.sys=='gb'?'Game Boy':EMU_SYS[g.sys].short)+(g.size?' · '+fmtB(g.size):'')+(last?' · '+new Date(last.t).toLocaleDateString():'')+(fav.includes(g.path)?' · ★':'');t.title=g.name;
   t.onclick=()=>open(g);let lt;t.onpointerdown=e=>{lt=setTimeout(()=>{lt='done';ctxm(g,e.clientX,e.clientY)},600)};t.onpointerup=t.onpointerleave=()=>{if(lt!='done')clearTimeout(lt)};
   t.addEventListener('click',e=>{if(lt=='done'){e.stopImmediatePropagation();lt=null}},true);t.oncontextmenu=e=>{e.preventDefault();ctxm(g,e.clientX,e.clientY)};grid.appendChild(t)})};
 const open=g=>{if(g.sys=='gb'){const P=typeof gbPlayer=='function'&&gbPlayer();if(P&&P.loadAny){(g.real?emuReadReal(g.real):mmBytes(node(g.drive))).then(u=>P.loadAny(u,g.name)).catch(e=>notify('Emu',e.message||String(e),{kind:'error'}));emuRecent(g)}return}if(emuNat(g.sys))return emuNative(g);if(EMU_SYS[g.sys].lr&&EMU_SYS[g.sys].lr.disc)return emuPlay({sys:g.sys,name:g.name,real:g.path,title:g.title});emuOpenReal(g.path)};
 const ctxm=(g,X,Y)=>{const fav=ld(emuFavK(),[]),isF=fav.includes(g.path);const it=[['Play',()=>open(g),true,{icon:'▶'}],[isF?'Remove from favourites':'Add to favourites',()=>{svj(emuFavK(),isF?fav.filter(x=>x!=g.path):fav.concat(g.path));draw()}]];
  if((g.sys!='gb'||g.real)&&RD){it.push(['Show in Files',()=>{launch('explorer');setTimeout(()=>{const W=wins.filter(w=>w.el.querySelector('.xside')).pop();const it=W&&[...W.el.querySelectorAll('.xside *')].find(e=>e.children.length<3&&/Games \(Emu library\)/.test(e.textContent));it&&it.click()},500)}]);
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
  const ok=it.filter(i=>i.dir||/\.(gb|gbc|gba|sfc|smc|nds|z64|n64|v64|gdi|cdi|chd|md|gen|smd|bin|cue|pbp|zip)$/i.test(i.name));if(!ok.length)L.innerHTML+="<div class='msg'>No Game Boy (.gb, .gbc, .gba), SNES (.sfc, .smc), DS (.nds), Nintendo 64 (.z64, .n64, .v64), Genesis (.md, .gen, .smd, .bin), PlayStation (.cue with its tracks, .chd, .pbp; BIOS files scph*.bin), Dreamcast (.gdi with its tracks, .cdi, .chd) or ZIP files here.</div>";
  ok.forEach(i=>{const r=document.createElement('label');r.className='earow';if(i.dir){r.textContent='📁 '+i.name;r.onclick=()=>show(p+'/'+i.name)}else{r.innerHTML="<input type='checkbox'> <span></span><small></small>";r.querySelector('span').textContent=i.name;r.querySelector('small').textContent=fmtB(i.size);
   r.querySelector('input').onchange=e=>{e.target.checked?sel.add(p+'/'+i.name):sel.delete(p+'/'+i.name);q('.eago').disabled=!sel.size}}L.appendChild(r)});q('.eago').disabled=true};
 q('.eaall').onclick=()=>{W.body.querySelectorAll('.ealist input').forEach(c=>{if(!c.checked){c.checked=true;c.onchange({target:c})}})};
 q('.eago').onclick=async()=>{if(!sel.size)return;try{await lxApi('games/import',[...sel].join('\n'))}catch(e){q('.eares').textContent=e.message;return}
  const pg=q('.eaprog');pg.hidden=false;q('.eago').disabled=true;const res=[];
  const j=await lxJobWait(l=>{let m;if((m=/^progress (\d+) (\d+) (.*)/.exec(l))){q('.eastep').textContent=`Copying ${m[3]} (${m[1]} of ${m[2]})…`;pg.querySelector('i').style.width=(m[1]/m[2]*100)+'%'}else if(/^(added|skip) /.test(l))res.push(l.replace(/^added /,'✓ ').replace(/^skip /,'– '));else if(/^done /.test(l))q('.eastep').textContent=l.slice(5)});
  pg.querySelector('i').style.width='100%';const last=(j.log||'').trim().split('\n').pop()||'';q('.eastep').textContent=/^done /.test(last)?'Finished: '+last.slice(5)+'.':'Adding stopped: '+last;
  q('.eares').textContent=res.join('\n');done&&done()};
 show('/home/rudolf/Downloads');return W}
/* ---------- 1.6.1: the native emulators (Dreamcast, Nintendo 64) ---------- */
function emuAsk(title,html,ok){return new Promise(res=>{const o=document.createElement('div');o.className='natdlg';
 o.innerHTML=`<div class='natbox' role='dialog' aria-modal='true' aria-labelledby='emuaskt'><h3 id='emuaskt'></h3><div class='emuaskb'>${html}</div><div class='stbtn' style='justify-content:flex-end'><button class='c'>Cancel</button><button class='pri g'></button></div></div>`;
 o.querySelector('h3').textContent=title;o.querySelector('.g').textContent=ok;const done=v=>{o.remove();res(v)};
 o.querySelector('.c').onclick=()=>done(false);o.querySelector('.g').onclick=()=>done(true);o.onkeydown=e=>{if(e.key=='Escape')done(false)};document.body.appendChild(o);o.querySelector('.g').focus()})}
/* its emulator from Flathub, once: asked, then downloaded with the progress in a small window */
async function emuNativeGet(sys,st){const sy=EMU_SYS[sys];
 if(!st.persistent){notify('Emu','Install Rudolf OS on this computer first (Settings → Update and recovery): '+sy.app+' needs a place to be kept.',{kind:'error'});return false}
 if(!await emuAsk('Play '+sy.name+' games?',`<p>${sy.name} games play in <b>${lxEsc(sy.app)}</b>, a free emulator (open source), downloaded once from Flathub over the internet. It runs on the graphics card, full screen.</p><p class='msg'>${lxEsc(sy.how)}</p>`,'Download'))return false;
 if(typeof appDownloadOk=='function'&&!await appDownloadOk(sy.app,()=>rdPost('native/size?id='+sy.native)))return false;
 if(!await elevate('Install '+sy.app+' from Flathub, for '+sy.name+' games'))return false;
 try{await rdPost('native/install?id='+sy.native)}catch(e){notify('Emu',e.message,{kind:'error'});return false}
 const W=win('Getting '+sy.app,'🦤',460,180);W.body.innerHTML=`<p class='msg ennote'>Downloading ${lxEsc(sy.app)}…</p><div class='dsbar'><i style='width:5%'></i></div>`;
 const note=W.body.querySelector('.ennote'),bar=W.body.querySelector('.dsbar i');let n=5;
 const j=await lxJobWait(l=>{if(l.trim()){note.textContent=l.trim().slice(0,160);n=Math.min(95,n+3);bar.style.width=n+'%'}});
 const okay=j.status==0;bar.style.width='100%';note.textContent=okay?sy.app+' is ready.':'The download did not finish: '+((j.log||'').trim().split('\n').pop()||'unknown');
 setTimeout(()=>W.close&&W.close(),okay?1200:8000);return okay}
async function emuNative(g){const sy=EMU_SYS[g.sys],title=g.title||g.name.replace(/\.[^.]+$/,'');
 if(!RD){notify('Emu',sy.name+' games need Rudolf OS itself (its disc, USB stick or hard disk).',{kind:'error'});return}
 if(!g.path||!g.path.startsWith(EMU_GAMES+'/')){notify('Emu','Add '+title+' to the game library first (Game library › Add games…): '+sy.name+' games play from there.',{kind:'error'});return}
 let st;try{st=await rd('native/status?id='+sy.native)}catch(e){notify('Emu',e.message,{kind:'error'});return}
 if(!st.flatpak){notify('Emu','This Rudolf OS build cannot run Linux apps, which '+sy.name+' games need.',{kind:'error'});return}
 if(!st.installed&&!await emuNativeGet(g.sys,st))return;
 if(st.running){if(!await emuAsk('Close what is open full screen?',`<p>Another game or app is open full screen (press <kbd>Super</kbd>+<kbd>Esc</kbd> to see it). Close it and start ${lxEsc(title)}?</p>`,'Close it and play'))return;
  try{await rdPost('native/stop?id='+sy.native)}catch(e){}await new Promise(r=>setTimeout(r,1500))}
 try{await rdPost('native/run?id='+sy.native+'&file='+encodeURIComponent(g.path))}catch(e){notify('Emu',e.message,{kind:'error'});return}
 emuRecent(Object.assign({real:g.path},g));
 notify('Starting '+title+'…',sy.app+' opens it full screen in a moment. '+sy.how+' Super+Esc comes back to the desktop.')}
/* ---------- controls ---------- */
function emuControls(sys,changed){const W=win('Emu controls — '+EMU_SYS[sys].name,'🎮',520,560);W.body.classList.add('emuctlw');
 let c=emuCtl(sys),wait=null;const L=emuLay(sys),KD=Object.assign({},L.keys,L.stick||{}),all=c.btn.concat(Object.keys(L.stick||{}));
 const kn=k=>({ArrowUp:'↑',ArrowDown:'↓',ArrowLeft:'←',ArrowRight:'→',ShiftRight:'Right Shift',ShiftLeft:'Left Shift',Enter:'Enter',Space:'Space'})[k]||k.replace(/^Key|^Digit/,'');
 const draw=()=>{W.body.innerHTML=`<p class='msg'>Click a key or pad button, then press the one you want. Gamepads use the standard layout${sys=='snes'||sys=='ds'?' (Nintendo positions)':''}${L.sticks?'':'; the left stick also moves'}. Tab fast-forwards, Alt+Enter switches borderless mode, Esc opens the menu in borderless mode.</p>
  ${L.sticks?"<p class='msg'>The gamepad’s sticks are the DualShock’s sticks (games that use them switch it to analog by themselves).</p>":''}
  <table class='tmt ectab'><thead><tr><th>Button</th><th>Keyboard</th><th>Gamepad</th></tr></thead><tbody>${c.btn.map(b=>`<tr><td>${lxEsc(EMU_BNAME[b]||b)}</td><td><button data-k='${b}'>${lxEsc(kn(c.keys[b]))}</button></td><td><button data-p='${b}'>Button ${c.pad[b]}</button></td></tr>`).join('')}
  ${Object.keys(L.stick||{}).map(b=>`<tr><td>${lxEsc(EMU_BNAME[b])}</td><td><button data-k='${b}'>${lxEsc(kn(c.keys[b]))}</button></td><td class='msg'>the left stick</td></tr>`).join('')}</tbody></table>
  <label class='chk'><input type='checkbox' class='ecpad'${ld('bd95emupad',true)?' checked':''}> Show on-screen buttons when the touchscreen is used</label>
  ${sys=='ds'?`<label class='chk'><input type='checkbox' class='ecint'${ld('bd95emuint',false)?' checked':''}> Whole-pixel scaling (sharper, with black edges)</label>`:sys!='snes'?'':`<div class='r2'><label>Picture shape</label><select class='ecar'><option value='4:3'>4:3 (as on a TV)</option><option value='8:7'>8:7 (square pixels)</option></select></div>`}
  <label class='chk'><input type='checkbox' class='ecbl'${ld('bd95emubl',false)?' checked':''}> Always start games borderless</label>
  <div class='rmrow' style='justify-content:flex-start'><button class='ecdef'>Default controls</button></div><div class='msg ecmsg'></div>`;
  W.body.querySelectorAll('[data-k],[data-p]').forEach(b=>b.onclick=()=>{wait={k:b.dataset.k,p:b.dataset.p,el:b};b.textContent='Press…';W.body.querySelector('.ecmsg').textContent='Waiting for '+(b.dataset.k?'a key':'a gamepad button')+'… (Esc cancels)';if(b.dataset.p)padWait()});
  W.body.querySelector('.ecpad').onchange=e=>svj('bd95emupad',e.target.checked);W.body.querySelector('.ecbl').onchange=e=>svj('bd95emubl',e.target.checked);
  if(sys=='ds')W.body.querySelector('.ecint').onchange=e=>svj('bd95emuint',e.target.checked);else if(sys=='snes'){const a=W.body.querySelector('.ecar');a.value=ld('bd95emusnesar','4:3');a.onchange=()=>svj('bd95emusnesar',a.value)}
  W.body.querySelector('.ecdef').onclick=()=>{svj(emuCtlK(sys),{});c=emuCtl(sys);draw();changed&&changed()}};
 const store=()=>{const d={keys:{},pad:{}};all.forEach(b=>{if(c.keys[b]!=KD[b])d.keys[b]=c.keys[b];if(b in L.pad&&c.pad[b]!=L.pad[b])d.pad[b]=c.pad[b]});svj(emuCtlK(sys),d);changed&&changed()};
 const padWait=()=>{const t0=Date.now();const iv=setInterval(()=>{if(!wait||!wait.p||!W.el.isConnected||Date.now()-t0>10000){clearInterval(iv);if(wait&&wait.p){wait=null;draw()}return}
  for(const p of navigator.getGamepads?navigator.getGamepads():[]){if(!p)continue;const i=p.buttons.findIndex(b=>b.pressed);if(i>=0){c.pad[wait.p]=i;wait=null;clearInterval(iv);store();draw();return}}},50)};
 W.body.addEventListener('keydown',e=>{e.stopPropagation();if(!wait||!wait.k)return;e.preventDefault();if(e.code!='Escape'){all.forEach(b=>{if(c.keys[b]==e.code)c.keys[b]=c.keys[wait.k]});c.keys[wait.k]=e.code;store()}wait=null;draw()});
 draw();return W}
/* ---------- Emu's launchers open the library; .sfc/.smc/.nds files open here ---------- */
if(typeof MMLN!='undefined'&&MMLN.gb)MMLN.gb.fn=()=>emuLibrary();
if(APPS.gameboy){APPS.gameboy.open=()=>emuLibrary();APPS.gameboy.kw=(APPS.gameboy.kw||'')+' snes super nintendo ds nintendo ds emulator games library genesis mega drive sega playstation ps1 psx'}
window.emuOpenReal=emuOpenReal;window.emuOpenDrive=emuOpenDrive;
