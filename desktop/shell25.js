/* ===== V25 shell: app registry, shortcuts, Alt+Tab, overview, workspaces, Start search, notifications,
   quick settings, calendar flyout, taskbar pins/groups, session restore, window memory, desktop files, widgets ===== */
/* ---------- app registry ---------- */
const APPS={};
function regApp(k,o){APPS[k]=Object.assign({k},o);return APPS[k]}
[['explorer','File Explorer','📁',()=>explorer(),/^File Explorer|^Exploring/],['browser','Oracle Browser','🌐',()=>browser(),/^Oracle/],['notepad','Notepad','🗒️',()=>notepad(),/^Notepad|Notepad$/],
 ['prompt','Rudolf Prompt','⬛',()=>prompt(),/^Rudolf Prompt/],['blocklua','BlockLua Builder','🧱',()=>blocklua(),/^BlockLua/],['calc','Calculator','🧮',()=>calc(),/^Calculator/],
 ['wordlite','WordLite','📄',()=>A.wordlite?A.wordlite():wordpad(),/^WordLite/],['wordpro','WordPro','📝',()=>wordpro(),/^WordPro/],['paint','Paint','🎨',()=>paint(),/^Paint/],
 ['boards','Board Games','♟️',()=>boards(),/^Board Games|^Checkers|^Chess/],['poker','Card Games','🃏',()=>poker(),/^Card Games/],['galaxy','WOS Galaxy Map','🌌',()=>galaxy(),/^WOS Galaxy Map/],
 ['media','Media Player','🎵',()=>media(),/^Media Player/],['trash','Trash','🗑️',()=>trash(),/^Trash/],['settings','Settings','⚙️',()=>settings(),/^Settings|^Display Properties/],
 ['gameboy','Emu','🦤',()=>typeof mmLaunch=='function'?mmLaunch('gb'):gbPlayer(),/^Emu|Emu$|Game Boy|GBA/],['flash','Flash Player','⚡',()=>typeof mmLaunch=='function'?mmLaunch('ruffle'):flashPlayer(),/Flash/],
 ['quake','Quake Engine','🔫',()=>quakeApp(),/^Quake Engine/],['gpulab','GPU Lab','🧊',()=>gpuLab(),/^GPU Lab/]].forEach(([k,n,i,f,re])=>regApp(k,{name:n,icon:i,open:f,re}));
let LAUNCHING=null;
function launch(k){const a=APPS[k];if(!a)return;LAUNCHING=k;try{return a.open()}catch(e){console.warn(e);notify('Could not open '+a.name,String(e.message||e),{kind:'error'})}finally{LAUNCHING=null}}
const appOfTitle=t=>{for(const k in APPS){const a=APPS[k];if(a.re&&a.re.test(t))return k}return null};
/* ---------- window tagging, MRU order, workspaces, remembered geometry ---------- */
let MRU=[],WS=1;const NWS=3;
const geoK=()=>'bd95geo:'+uid();
(function(){const _w=win;win=function(title,icon,w,h,...r){const W=_w(title,icon,w,h,...r);W.app=LAUNCHING||appOfTitle(String(title))||null;W.ws=WS;W.icon=icon;
 W.el.setAttribute('role','dialog');W.el.setAttribute('aria-label',String(title));
 const tb=W.el.querySelector('.tb');[['.mn','Minimize'],['.mx','Maximize'],['.cl','Close']].forEach(([s,l])=>{const b=tb.querySelector(s);b.setAttribute('aria-label',l);b.title=l});
 /* remembered size/position for this app */
 if(W.app&&!W.el.classList.contains('max')){const g=ld(geoK(),{})[W.app];if(g&&g.w>=200&&g.h>=120){const D=$('#desk'),B=$('#bar');const avail=B&&B.offsetTop>D.offsetTop+100?B.offsetTop-D.offsetTop:D.clientHeight;const ww=Math.min(g.w,D.clientWidth-8),hh=Math.min(g.h,avail-8);
  /* the whole window stays on screen and above the taskbar, even at a larger display scale */
  W.el.style.width=ww+'px';W.el.style.height=hh+'px';W.el.style.left=Math.max(0,Math.min(g.x,D.clientWidth-ww-4))+'px';W.el.style.top=Math.max(0,Math.min(g.y,avail-hh-4))+'px'}}
 const f=W.focus;W.focus=()=>{if(W.ws!=WS)wsGo(W.ws);f();MRU=[W,...MRU.filter(x=>x!==W)];tbRefresh()};
 const c=W.close;W.close=()=>{const el=W.el;if(W.app&&!el.classList.contains('max')&&el.offsetWidth>0){const g=ld(geoK(),{});g[W.app]={x:el.offsetLeft,y:el.offsetTop,w:el.offsetWidth,h:el.offsetHeight};svj(geoK(),g)}
  const before=wins.length;c();if(wins.length<before){MRU=MRU.filter(x=>x!==W);tbRefresh()}};
 MRU=[W,...MRU.filter(x=>x!==W)];
 if(!(window.matchMedia&&matchMedia('(prefers-reduced-motion: reduce)').matches)&&!S.rmotion){W.el.classList.add('wopen');setTimeout(()=>W.el.classList.remove('wopen'),220)}
 setTimeout(tbRefresh,0);return W}})();
function wsGo(n){n=Math.max(1,Math.min(NWS,n));if(n==WS){tbRefresh();return}WS=n;wins.forEach(o=>{o.el.style.display=o.ws==WS&&!o.min?'flex':'none'});tbRefresh();
 const top=MRU.find(o=>o.ws==WS&&!o.min);top&&top.focus();notify('Workspace '+WS,'',{quiet:true,toast:1200})}
function wsMove(W,n){W.ws=Math.max(1,Math.min(NWS,n));if(W.ws!=WS)W.el.style.display='none';tbRefresh()}
/* ---------- taskbar: pinned apps, grouping, reorder, workspace switcher ---------- */
const BAR=$('#bar'),TASKS=$('#tasks');
const pinsK=()=>'bd95pins:'+uid();const pinList=()=>ld(pinsK(),['explorer','browser','notepad','settings']).filter(k=>APPS[k]);
const PINS=document.createElement('div');PINS.id='pins';PINS.setAttribute('role','toolbar');PINS.setAttribute('aria-label','Pinned apps');TASKS.before(PINS);
const WSW=document.createElement('div');WSW.id='wsw';WSW.setAttribute('role','toolbar');WSW.setAttribute('aria-label','Workspaces');TASKS.after(WSW);
function pinDraw(){PINS.innerHTML='';if(!USER)return;pinList().forEach(k=>{const a=APPS[k],b=document.createElement('button');b.className='pin';b.textContent=a.icon;b.title=a.name;b.setAttribute('aria-label',a.name);b.dataset.k=k;
 const run=wins.filter(o=>o.app==k);if(run.length)b.classList.add('run');if(run.some(o=>o.el.classList.contains('act')&&!o.min))b.classList.add('on');
 b.onclick=()=>{const r=wins.filter(o=>o.app==k&&o.ws==WS);if(!r.length)return launch(k);const top=MRU.find(o=>o.app==k&&o.ws==WS);if(top&&top.el.classList.contains('act')&&!top.min&&r.length==1)top.el.querySelector('.mn').click();else if(r.length>1)groupPop(b,k);else top.focus()};
 b.onauxclick=e=>{if(e.button==1){e.preventDefault();launch(k)}};
 b.oncontextmenu=e=>{e.preventDefault();e.stopPropagation();cmenu(e.clientX,e.clientY,[['Open a new window',()=>launch(k)],['Close all windows',()=>wins.filter(o=>o.app==k).forEach(o=>o.close()),run.length>0],'-',['Unpin from taskbar',()=>pinSet(k,false)]],{title:a.name})};PINS.appendChild(b)})}
function pinSet(k,on){let p=pinList().filter(x=>x!=k);if(on)p.push(k);svj(pinsK(),p);pinDraw()}
function wswDraw(){WSW.innerHTML='';for(let i=1;i<=NWS;i++){const b=document.createElement('button');b.textContent=i;b.title='Workspace '+i+(wins.some(o=>o.ws==i)?'':' (empty)');b.setAttribute('aria-label','Workspace '+i);if(i==WS)b.classList.add('on');if(wins.some(o=>o.ws==i))b.classList.add('has');b.onclick=()=>wsGo(i);WSW.appendChild(b)}
 const ov=document.createElement('button');ov.textContent='▦';ov.title='Show all windows (Super+Tab)';ov.setAttribute('aria-label','Show all windows');ov.onclick=()=>overview();WSW.appendChild(ov)}
let tbT=0;function tbRefresh(){clearTimeout(tbT);tbT=setTimeout(tbRefreshNow,0)}
function tbRefreshNow(){const group=S.tbgroup!==false;const seen={};
 wins.forEach(o=>{const b=o.tb;if(!b)return;b.setAttribute('aria-pressed',o.el.classList.contains('act')&&!o.min?'true':'false');
  let show=o.ws==WS;if(show&&group&&o.app){const peers=wins.filter(x=>x.app==o.app&&x.ws==WS);if(peers.length>1){if(seen[o.app]){show=false}else{seen[o.app]=1;b.dataset.count=peers.length;b.classList.add('grp')}}else{delete b.dataset.count;b.classList.remove('grp')}}else{delete b.dataset.count;b.classList.remove('grp')}
  b.style.display=show?'':'none'});
 pinDraw();wswDraw()}
function groupPop(anchor,k){const list=wins.filter(o=>o.app==k&&o.ws==WS);const r=anchor.getBoundingClientRect();cmenu(r.left,r.top-8-list.length*30,list.map(o=>[o.el.querySelector('.t').textContent,()=>o.focus(),true,{icon:o.min?'–':'▢'}]),{title:APPS[k]?APPS[k].name:''})}
/* grouped buttons open a chooser; tooltips list the windows */
TASKS.addEventListener('click',e=>{const b=e.target.closest('button');if(!b||!b.classList.contains('grp'))return;const W=wins.find(o=>o.tb===b);if(!W||!W.app)return;e.stopImmediatePropagation();groupPop(b,W.app)},true);
/* drag taskbar buttons to reorder them */
(function(){let drag=null,sx=0,moved=false;TASKS.addEventListener('pointerdown',e=>{const b=e.target.closest('button');if(!b||e.button!==0)return;drag=b;sx=e.clientX;moved=false});
 addEventListener('pointermove',e=>{if(!drag)return;if(!moved&&Math.abs(e.clientX-sx)<8)return;moved=true;drag.classList.add('drag');const over=document.elementFromPoint(e.clientX,e.clientY);const ob=over&&over.closest&&over.closest('#tasks > button');if(ob&&ob!==drag){const r=ob.getBoundingClientRect();if(e.clientX<r.left+r.width/2)ob.before(drag);else ob.after(drag)}});
 addEventListener('pointerup',()=>{if(drag){drag.classList.remove('drag');if(moved){const b=drag;const sup=ev=>{ev.stopImmediatePropagation();ev.preventDefault()};b.addEventListener('click',sup,{capture:true,once:true});setTimeout(()=>b.removeEventListener('click',sup,true),50)}}drag=null})})();
new MutationObserver(tbRefresh).observe(TASKS,{childList:true});
/* window menu with workspaces and pinning (replaces the V24 menu) */
function winMenu(W,x,y){const mx=W.el.classList.contains('max'),top=W.el.classList.contains('ontop');const items=[['Restore',()=>{if(W.min)W.focus();else if(mx)maxToggle(W)},W.min||mx,{icon:'❐'}],['Minimize',()=>W.el.querySelector('.mn').click(),!W.min,{icon:'_'}],['Maximize',()=>{W.focus();maxToggle(W)},!mx,{icon:'□'}],'-',
 ['Always on top',()=>{W.el.classList.toggle('ontop');W.focus()},true,{check:top}],['Center on screen',()=>{if(mx)maxToggle(W);W.focus();const d=$('#desk');W.el.style.left=Math.max(0,(d.clientWidth-W.el.offsetWidth)/2)+'px';W.el.style.top=Math.max(0,(d.clientHeight-W.el.offsetHeight)/2)+'px'},true],'-'];
 for(let i=1;i<=NWS;i++)if(i!=W.ws)items.push(['Move to workspace '+i,()=>wsMove(W,i)]);
 if(W.app&&APPS[W.app])items.push('-',[pinList().includes(W.app)?'Unpin from taskbar':'Pin to taskbar',()=>pinSet(W.app,!pinList().includes(W.app))]);
 items.push('-',['Close',()=>W.close(),true,{icon:'✕',key:'Alt+F4'}]);cmenu(x,y,items,{title:W.el.querySelector('.t').textContent})}
/* ---------- resize from every edge and corner ---------- */
function addResize(el){[['n','top:-3px;left:8px;right:8px;height:6px;cursor:ns-resize'],['s','bottom:-3px;left:8px;right:8px;height:6px;cursor:ns-resize'],['e','right:-3px;top:8px;bottom:8px;width:6px;cursor:ew-resize'],['w','left:-3px;top:8px;bottom:8px;width:6px;cursor:ew-resize'],
 ['ne','right:-4px;top:-4px;width:12px;height:12px;cursor:nesw-resize'],['nw','left:-4px;top:-4px;width:12px;height:12px;cursor:nwse-resize'],['se','right:-4px;bottom:-4px;width:14px;height:14px;cursor:nwse-resize'],['sw','left:-4px;bottom:-4px;width:12px;height:12px;cursor:nesw-resize']].forEach(([k,css])=>{
 const h=document.createElement('div');h.className='rsz rsz-'+k;h.style.cssText='position:absolute;z-index:5;touch-action:none;'+css;el.appendChild(h);let sx,sy,w,ht,l,t,on=0;
 h.onpointerdown=e=>{if(e.button!==0||el.classList.contains('max'))return;e.preventDefault();h.setPointerCapture(e.pointerId);sx=e.clientX;sy=e.clientY;w=el.offsetWidth;ht=el.offsetHeight;l=el.offsetLeft;t=el.offsetTop;on=1};
 h.onpointermove=e=>{if(!on)return;const dx=e.clientX-sx,dy=e.clientY-sy;
  if(k.includes('e'))el.style.width=Math.max(200,w+dx)+'px';if(k.includes('w')){const nw=Math.max(200,w-dx);el.style.width=nw+'px';el.style.left=l+(w-nw)+'px'}
  if(k.includes('s'))el.style.height=Math.max(120,ht+dy)+'px';if(k.includes('n')){const nh=Math.max(120,ht-dy),nt=Math.max(0,t+(ht-nh));el.style.height=(ht+t-nt)+'px';el.style.top=nt+'px'}};
 h.onpointerup=()=>on=0})}
/* ---------- keyboard shortcuts (registry + per-user rebinding) ---------- */
const SHORT=[];
function shortcut(id,label,def,fn,group){SHORT.push({id,label,def,fn,group:group||'System'})}
const kbK=()=>'bd95kb:'+uid();const kbMap=()=>ld(kbK(),{});
const comboOf=e=>{const k=e.key==' '?'Space':e.key.length==1?e.key.toUpperCase():e.key;if(['Control','Alt','Shift','Meta','OS'].includes(e.key))return'';return(e.ctrlKey?'Ctrl+':'')+(e.altKey?'Alt+':'')+(e.shiftKey?'Shift+':'')+(e.metaKey?'Meta+':'')+k};
const kbOf=s=>kbMap()[s.id]||s.def;
const showDesktop=()=>wins.forEach(o=>{if(!o.min&&o.ws==WS)o.el.querySelector('.mn').click()});
const actWin=()=>MRU.find(o=>!o.min&&o.ws==WS&&wins.includes(o));
shortcut('files','Open File Explorer','Meta+E',()=>launch('explorer'));
shortcut('desktop','Show the desktop','Meta+D',showDesktop);
shortcut('run','Run a command','Meta+R',()=>A.run());
shortcut('lock','Lock the screen','Meta+L',()=>lockScreen());
shortcut('settings','Open Settings','Meta+I',()=>settings());
shortcut('start','Open the Start menu','Ctrl+Escape',()=>startToggle());
shortcut('overview','Show all windows','Meta+Tab',()=>overview());
shortcut('close','Close the window','Alt+F4',()=>{const w=actWin();w&&w.close()},'Windows');
shortcut('max','Maximize the window','Meta+ArrowUp',()=>{const w=actWin();w&&!w.el.classList.contains('max')&&maxToggle(w)},'Windows');
shortcut('min','Restore or minimize the window','Meta+ArrowDown',()=>{const w=actWin();if(!w)return;w.el.classList.contains('max')?maxToggle(w):w.el.querySelector('.mn').click()},'Windows');
shortcut('wsl','Previous workspace','Ctrl+Alt+ArrowLeft',()=>wsGo(WS-1),'Workspaces');
shortcut('wsr','Next workspace','Ctrl+Alt+ArrowRight',()=>wsGo(WS+1),'Workspaces');
shortcut('wsml','Move window to the previous workspace','Ctrl+Alt+Shift+ArrowLeft',()=>{const w=actWin();if(w){wsMove(w,w.ws-1);wsGo(w.ws)}},'Workspaces');
shortcut('wsmr','Move window to the next workspace','Ctrl+Alt+Shift+ArrowRight',()=>{const w=actWin();if(w){wsMove(w,w.ws+1);wsGo(w.ws)}},'Workspaces');
shortcut('notif','Open notifications','Meta+N',()=>ncToggle());
shortcut('quick','Open quick settings','Meta+A',()=>qsToggle());
shortcut('help','Open Help','F1',()=>launch('help'));
shortcut('fullscreen','Full screen on or off','F11',()=>fullToggle());
function fullToggle(){if(document.fullscreenElement)document.exitFullscreen().catch(()=>{});else document.documentElement.requestFullscreen().then(()=>{try{navigator.keyboard&&navigator.keyboard.lock&&navigator.keyboard.lock()}catch(e){}}).catch(()=>notify('Full screen is not available here',''))}
let metaAlone=false;
addEventListener('keydown',e=>{if(LOCKED||document.querySelector('#lockov,.rmodal'))return;
 if(e.key=='Meta'||e.key=='OS'){metaAlone=!e.repeat;return}metaAlone=false;
 if(e.target&&e.target.closest&&e.target.closest('.kbcap'))return;
 const c=comboOf(e);if(!c)return;const s=SHORT.find(x=>kbOf(x)==c);if(!s)return;
 if(document.pointerLockElement&&!c.startsWith('Ctrl+Alt'))return;
 e.preventDefault();e.stopPropagation();try{s.fn()}catch(x){console.warn(x)}},true);
addEventListener('keyup',e=>{if((e.key=='Meta'||e.key=='OS')&&metaAlone&&!LOCKED){metaAlone=false;startToggle()}},true);
function startToggle(){const m=$('#menu');if(m.style.display=='flex'){m.style.display='none'}else{m.style.display='flex';setTimeout(()=>{const s=$('#ssearch');s&&s.focus()},20)}}
/* ---------- Alt+Tab switcher ---------- */
let SWT=null;
addEventListener('keydown',e=>{if(LOCKED)return;if(e.altKey&&(e.key=='Tab'||e.key=='`')&&!e.ctrlKey){e.preventDefault();e.stopPropagation();const list=MRU.filter(o=>wins.includes(o));if(!list.length)return;
  if(!SWT){SWT={i:0,list,el:document.createElement('div')};SWT.el.id='altab';SWT.el.setAttribute('role','listbox');SWT.el.setAttribute('aria-label','Switch windows');document.body.appendChild(SWT.el)}
  SWT.i=(SWT.i+(e.shiftKey?-1:1)+SWT.list.length)%SWT.list.length;altDraw()}
 else if(SWT&&e.key=='Escape'){SWT.el.remove();SWT=null;e.preventDefault()}},true);
addEventListener('keyup',e=>{if(SWT&&(e.key=='Alt'||!e.altKey)){const W=SWT.list[SWT.i];SWT.el.remove();SWT=null;W&&W.focus()}},true);
function altDraw(){SWT.el.innerHTML='';SWT.list.forEach((o,i)=>{const d=document.createElement('div');d.className='atc'+(i==SWT.i?' on':'');d.setAttribute('role','option');d.setAttribute('aria-selected',i==SWT.i);
 d.innerHTML=`<div class='ati'></div><div class='att'></div><div class='atw'></div>`;d.querySelector('.ati').textContent=o.icon||'▢';d.querySelector('.att').textContent=o.el.querySelector('.t').textContent;d.querySelector('.atw').textContent=(o.min?'minimized · ':'')+'workspace '+o.ws;
 d.onclick=()=>{SWT.el.remove();SWT=null;o.focus()};SWT.el.appendChild(d)});try{speak&&S.sr&&speak(SWT.list[SWT.i].el.querySelector('.t').textContent)}catch(x){}}
/* ---------- overview of all windows ---------- */
let OVR=null;
function overview(){if(OVR)return ovEnd();const vis=wins.filter(o=>o.ws==WS);if(!vis.length){notify('No windows are open on this workspace','');return}
 const D=$('#desk'),dw=D.clientWidth,dh=D.clientHeight,n=vis.length,cols=Math.ceil(Math.sqrt(n)),rows=Math.ceil(n/cols),cw=dw/cols,ch=dh/rows;
 const sh=document.createElement('div');sh.id='ovsh';D.appendChild(sh);OVR={vis,sh,saved:vis.map(o=>({o,d:o.el.style.display,t:o.el.style.transform}))};
 vis.forEach((o,i)=>{o.el.style.display='flex';const r={l:o.el.offsetLeft,t:o.el.offsetTop,w:o.el.offsetWidth,h:o.el.offsetHeight};const s=Math.min((cw-40)/r.w,(ch-60)/r.h,1);const cx=(i%cols)*cw+cw/2,cy=Math.floor(i/cols)*ch+ch/2;
  o.el.classList.add('ovw');o.el.style.transform=`translate(${cx-(r.l+r.w/2)}px,${cy-(r.t+r.h/2)}px) scale(${s})`;o.el.style.zIndex=99990+i;
  o._ov=e=>{e.stopPropagation();e.preventDefault();ovEnd(o)};o.el.addEventListener('pointerdown',o._ov,true)});
 sh.onclick=()=>ovEnd()}
function ovEnd(pick){if(!OVR)return;const R=OVR;OVR=null;R.vis.forEach(o=>{o.el.removeEventListener('pointerdown',o._ov,true);o.el.style.transform='';setTimeout(()=>o.el.classList.remove('ovw'),200)});R.saved.forEach(x=>{if(x.o!==pick&&x.o.min)x.o.el.style.display='none'});R.sh.remove();if(pick){pick.min=false;pick.focus()}}
addEventListener('keydown',e=>{if(OVR&&e.key=='Escape'){e.preventDefault();ovEnd()}},true);
/* ---------- Start menu search ---------- */
const SETPAGES=[['disp','Display: background, theme, colours, fonts, icons','display wallpaper theme dark accent font taskbar scale resolution night'],['vol','Sound: volume, alerts, output device','sound volume audio mute speaker'],['mm','Multimedia: emulators, Flash, Quake','game boy gba flash ruffle quake emulator'],['net','Network: Wi-Fi, Ethernet, proxy, firewall','network wifi ethernet internet proxy vpn'],['sys','System: clock, date, time zone, language region','date time clock zone region'],['acc','Accessibility: mouse, cursor, screen reader, magnifier, keyboard','accessibility mouse cursor left-handed screen reader magnifier contrast sticky keys captions'],['sec','Security: firewall, antivirus','security firewall antivirus virus'],['usr','Users: accounts, passwords, guest, family','user account password guest parental family'],['kbd','Keyboard: shortcuts, layouts, input methods','keyboard shortcut layout input method ime compose'],['priv','Privacy: app permissions, activity','privacy permission camera microphone location'],['stor','Storage and backup','storage disk space backup restore'],['apps','Apps: default apps, installed apps','default apps open with install uninstall'],['notif','Notifications','notifications do not disturb'],['upd','Update and recovery','update version recovery reset install disk'],['about','About this computer','about version hardware cpu memory licence']];
function startSearch(q){q=q.trim().toLowerCase();const out=[];if(!q)return out;
 for(const k in APPS){const a=APPS[k];if(a.name.toLowerCase().includes(q)||(a.kw||'').includes(q))out.push({g:'Apps',t:a.name,i:a.icon,f:()=>launch(k)})}
 SETPAGES.forEach(([k,t,kw])=>{if(t.toLowerCase().includes(q)||kw.includes(q))out.push({g:'Settings',t,i:'⚙️',f:()=>settings(k)})});
 const files=[];const walk=(n,p,d)=>{if(files.length>=12||d>8)return;for(const k in n){const v=n[k],pp=[...p,k];if(k.toLowerCase().includes(q))files.push(pp);else if(typeof v=='string'&&v.length<200000&&!v.startsWith('data:')&&v.toLowerCase().includes(q))files.push(pp);if(isDir(v))walk(v,pp,d+1);if(files.length>=12)return}};walk(node(['C:']),['C:'],0);
 files.forEach(p=>out.push({g:'Files',t:p.at(-1),sub:p.slice(0,-1).join('\\'),i:fIcon(p.at(-1),node(p)),f:()=>openPath(p)}));
 if(/^[0-9+\-*/(). ]+$/.test(q)&&/[0-9]/.test(q)){try{const v=Function('"use strict";return('+q+')')();if(isFinite(v))out.unshift({g:'Calculator',t:q+' = '+v,i:'🧮',f:()=>{navigator.clipboard&&navigator.clipboard.writeText(String(v)).catch(()=>{})}})}catch(e){}}
 return out.slice(0,24)}
(function(){const M=$('#menu');const box=document.createElement('div');box.className='ssbox';box.innerHTML=`<input id='ssearch' type='search' placeholder='Search apps, settings and files' aria-label='Search apps, settings and files' autocomplete='off'><div id='ssres' role='listbox'></div>`;
 const ul=M.querySelector('ul');ul.before(box);const inp=box.querySelector('input'),res=box.querySelector('#ssres');let items=[],cur=0;
 const draw=()=>{res.innerHTML='';ul.style.display=inp.value.trim()?'none':'';let g='';items.forEach((it,i)=>{if(it.g!=g){g=it.g;const h=document.createElement('div');h.className='ssg';h.textContent=g;res.appendChild(h)}const d=document.createElement('div');d.className='ssi'+(i==cur?' on':'');d.setAttribute('role','option');d.innerHTML=`<i></i><span></span><small></small>`;d.querySelector('i').textContent=it.i;d.querySelector('span').textContent=it.t;d.querySelector('small').textContent=it.sub||'';d.onclick=()=>{M.style.display='none';inp.value='';draw();it.f()};res.appendChild(d)});
  if(inp.value.trim()&&!items.length){const d=document.createElement('div');d.className='ssg';d.textContent='No results';res.appendChild(d)}};
 inp.oninput=()=>{items=startSearch(inp.value);cur=0;draw()};
 inp.onkeydown=e=>{if(e.key=='ArrowDown'){e.preventDefault();cur=Math.min(items.length-1,cur+1);draw()}else if(e.key=='ArrowUp'){e.preventDefault();cur=Math.max(0,cur-1);draw()}else if(e.key=='Enter'&&items[cur]){M.style.display='none';const it=items[cur];inp.value='';draw();it.f()}else if(e.key=='Escape'){M.style.display='none'}};
 inp.onclick=e=>e.stopPropagation();
 /* typing while the Start menu is open goes to the search box */
 M.addEventListener('keydown',e=>{if(e.target!==inp&&e.key.length==1&&!e.ctrlKey&&!e.altKey){inp.focus()}});
 new MutationObserver(()=>{if(M.style.display=='flex')setTimeout(()=>inp.focus(),10);else{inp.value='';items=[];draw()}}).observe(M,{attributes:true,attributeFilter:['style']});
 /* right-click on a Start item can pin it */
 M.addEventListener('contextmenu',e=>{const li=e.target.closest('li[data-a]');if(!li)return;const k=li.dataset.a=='docs'?null:(APPS[li.dataset.a]?li.dataset.a:null);if(!k)return;e.preventDefault();e.stopImmediatePropagation();
  cmenu(e.clientX,e.clientY,[['Open',()=>{M.style.display='none';launch(k)},true,{icon:'↗'}],[pinList().includes(k)?'Unpin from taskbar':'Pin to taskbar',()=>pinSet(k,!pinList().includes(k))],['Add to desktop',()=>{notify('Already on the desktop','')}]],{title:APPS[k].name})},true)})();
/* ---------- notification centre ---------- */
const NCK=()=>'bd95notif:'+uid();let NC=[],ncUnread=0;
const NCB=document.createElement('button');NCB.id='ncb';NCB.title='Notifications (Super+N)';NCB.setAttribute('aria-label','Notifications');NCB.textContent='🔔';
const QSB=document.createElement('button');QSB.id='qsb';QSB.title='Quick settings (Super+A)';QSB.setAttribute('aria-label','Quick settings');QSB.textContent='☰';
$('#tray').before(QSB);$('#tray').before(NCB);
function ncBadge(){NCB.dataset.n=ncUnread||'';NCB.classList.toggle('dnd',!!S.dnd);NCB.textContent=S.dnd?'🔕':'🔔'}
window.__notifCentre={add(title,body,opt){opt=opt||{};const n={id:Date.now()+Math.random(),t:String(title),b:String(body||''),k:opt.kind||'info',app:opt.app||'',at:Date.now(),act:opt.action||null};
 if(!opt.quiet){NC.unshift(n);NC=NC.slice(0,60);ncUnread++;ncBadge();try{USER&&svj(NCK(),NC.map(x=>({...x,act:null})))}catch(e){}}
 if(!S.dnd||n.k=='error'){const d=document.createElement('div');d.className='ntoast '+n.k;d.setAttribute('role',n.k=='error'?'alert':'status');d.innerHTML=`<b></b><div></div>`;d.querySelector('b').textContent=n.t;d.querySelector('div').textContent=n.b;
  d.onclick=()=>{d.remove();n.act?n.act():ncToggle(true)};let host=$('#ntoasts');if(!host){host=document.createElement('div');host.id='ntoasts';document.body.appendChild(host)}host.appendChild(d);setTimeout(()=>d.classList.add('out'),opt.toast||5000);setTimeout(()=>d.remove(),(opt.toast||5000)+400);
  try{if(S.sr||S.tts)speak(n.t+'. '+n.b)}catch(e){}}
 if(!opt.quiet&&!S.dnd&&opt.sound!==false)try{sfx('notify')}catch(e){}return n}};
function ncToggle(force){let p=$('#ncp');if(p&&!force){p.remove();return}if(p)p.remove();qsClose();p=document.createElement('div');p.id='ncp';p.className='flyout';p.setAttribute('role','dialog');p.setAttribute('aria-label','Notifications');
 p.innerHTML=`<div class='fh'><b>Notifications</b><span style='flex:1'></span><label class='ck'><input type='checkbox' class='dnd'> Do not disturb</label><button class='clr'>Clear all</button></div><div class='nl'></div>`;
 const nl=p.querySelector('.nl');const draw=()=>{nl.innerHTML='';if(!NC.length){nl.innerHTML=`<div class='nempty'>No new notifications</div>`;return}NC.forEach(n=>{const d=document.createElement('div');d.className='nitem '+n.k;d.innerHTML=`<div class='nt'><b></b><small></small><button class='nx' aria-label='Dismiss'>✕</button></div><div class='nb'></div>`;d.querySelector('b').textContent=n.t;d.querySelector('small').textContent=new Date(n.at).toLocaleTimeString([], {hour:'2-digit',minute:'2-digit'});d.querySelector('.nb').textContent=n.b;
  d.querySelector('.nx').onclick=e=>{e.stopPropagation();NC=NC.filter(x=>x!==n);svj(NCK(),NC);draw()};d.onclick=()=>{n.act&&n.act()};nl.appendChild(d)})};draw();
 p.querySelector('.dnd').checked=!!S.dnd;p.querySelector('.dnd').onchange=e=>{S.dnd=e.target.checked;if(SC)SC.dnd=S.dnd;persistS();ncBadge()};
 p.querySelector('.clr').onclick=()=>{NC=[];svj(NCK(),[]);draw()};document.body.appendChild(p);ncUnread=0;ncBadge();flyClose(p,NCB)}
NCB.onclick=e=>{e.stopPropagation();ncToggle()};
function flyClose(p,btn){setTimeout(()=>{const k=e=>{if(!p.contains(e.target)&&e.target!==btn&&!(e.target.closest&&e.target.closest('.cmenu'))){p.remove();document.removeEventListener('pointerdown',k,true)}};document.addEventListener('pointerdown',k,true)},0)}
/* route older pop-up helpers into the centre */
(function(){if(typeof toast2=='function'){toast2=function(t){notify(t,'')}}})();
/* ---------- quick settings ---------- */
function qsClose(){const p=$('#qsp');p&&p.remove()}
function qsToggle(){if($('#qsp'))return qsClose();const n=$('#ncp');n&&n.remove();const p=document.createElement('div');p.id='qsp';p.className='flyout';p.setAttribute('role','dialog');p.setAttribute('aria-label','Quick settings');
 const m=S.snd&&S.snd.master||{vol:50,on:true};
 const T=[['dm','🌙','Dark mode',()=>S.dm=='dark',v=>{S.dm=v?'dark':'light';if(SC)SC.dm=S.dm;window.__dyn&&dynFix();themeApply()}],['nl','🌅','Night light',()=>!!S.nl,v=>{S.nl=v;themeApply()}],['dnd','🔕','Do not disturb',()=>!!S.dnd,v=>{S.dnd=v;ncBadge()}],
  ['sr','🗣️','Screen reader',()=>!!S.sr,v=>{S.sr=v;window.__srApply&&__srApply()}],['mag','🔍','Magnifier',()=>!!S.mag,v=>{S.mag=v;window.__magApply&&__magApply()}],['hc','◐','High contrast',()=>S.cb=='contrast',v=>{S.cb=v?'contrast':'none';a11yApply()}],['vk','⌨️','On-screen keyboard',()=>!!S.vk,v=>{S.vk=v;a11yApply()}],['tm','🖐️','Touch mode',()=>!!S.touch,v=>{S.touch=v;themeApply()}]];
 p.innerHTML=`<div class='qsg'></div><div class='qsr'><span>🔊</span><input type='range' min='0' max='100' class='qv' aria-label='Volume'><span class='qvv'></span></div><div class='qsr qbl' style='display:none'><span>☀️</span><input type='range' min='5' max='100' class='qb' aria-label='Brightness'></div><div class='qsinfo'></div><div class='qsf'><button class='qlock'>🔒 Lock</button><button class='qset'>⚙️ Settings</button><button class='qoff'>⏻ Power</button></div>`;
 const g=p.querySelector('.qsg');T.forEach(([k,i,l,get,set])=>{const b=document.createElement('button');b.className='qst'+(get()?' on':'');b.setAttribute('aria-pressed',get());b.innerHTML=`<i></i><span></span>`;b.querySelector('i').textContent=i;b.querySelector('span').textContent=l;
  b.onclick=()=>{set(!get());if(SC)SC[k]=S[k];persistS();b.classList.toggle('on',get());b.setAttribute('aria-pressed',get())};g.appendChild(b)});
 const qv=p.querySelector('.qv'),qvv=p.querySelector('.qvv');qv.value=m.on?m.vol:0;qvv.textContent=qv.value+'%';qv.oninput=()=>{m.vol=+qv.value;m.on=m.vol>0;qvv.textContent=qv.value+'%';trayVol()};
 const info=p.querySelector('.qsinfo');
 (async()=>{const bits=[];try{if(RD){const s=await rd('stats');const b=(s.battery||[]).find(x=>x.type=='Battery');if(b)bits.push(`🔋 ${b.capacity}% ${b.status}`);if(s.brightness!=null&&s.brightness>=0){const r=p.querySelector('.qbl');r.style.display='';const qb=p.querySelector('.qb');qb.value=s.brightness;qb.onchange=()=>rdPost('brightness?value='+qb.value).catch(e=>notify(e.message,''))}
   const n=await rd('net');const up=n.interfaces.filter(i=>i.ip);bits.push(up.length?'🌐 '+up.map(i=>i.name+' '+i.ip).join(', '):'🌐 Not connected')}
  else{if(navigator.getBattery){const b=await navigator.getBattery();bits.push(`🔋 ${Math.round(b.level*100)}%${b.charging?' charging':''}`)}bits.push(navigator.onLine?'🌐 Online':'🌐 Offline')}}catch(e){}info.textContent=bits.join('   ')})();
 p.querySelector('.qlock').onclick=()=>{qsClose();lockScreen()};p.querySelector('.qset').onclick=()=>{qsClose();settings()};p.querySelector('.qoff').onclick=()=>{qsClose();A.off()};
 document.body.appendChild(p);flyClose(p,QSB)}
QSB.onclick=e=>{e.stopPropagation();qsToggle()};
/* ---------- calendar flyout from the clock ---------- */
$('#tray').addEventListener('click',e=>{e.stopPropagation();let p=$('#calp');if(p){p.remove();return}p=document.createElement('div');p.id='calp';p.className='flyout';p.setAttribute('role','dialog');p.setAttribute('aria-label','Calendar');
 let d=sysNow?sysNow():new Date(),y=d.getFullYear(),mo=d.getMonth();
 const draw=()=>{const f=new Date(y,mo,1),days=new Date(y,mo+1,0).getDate(),st=(f.getDay()+6)%7,today=sysNow?sysNow():new Date();
  let h=`<div class='cbig'>${today.toLocaleTimeString([], {hour:'2-digit',minute:'2-digit'})}</div><div class='cdate'>${today.toLocaleDateString([], {weekday:'long',year:'numeric',month:'long',day:'numeric'})}</div><div class='fh'><button class='cp' aria-label='Previous month'>◀</button><b style='flex:1;text-align:center'>${f.toLocaleDateString([], {month:'long',year:'numeric'})}</b><button class='cn' aria-label='Next month'>▶</button></div><table class='calt'><tr>${['Mo','Tu','We','Th','Fr','Sa','Su'].map(x=>`<th>${x}</th>`).join('')}</tr><tr>`;
  const ev=window.calEvents?calEvents():[];for(let i=0;i<st;i++)h+='<td></td>';for(let dd=1;dd<=days;dd++){const key=`${y}-${String(mo+1).padStart(2,'0')}-${String(dd).padStart(2,'0')}`;const isT=dd==today.getDate()&&mo==today.getMonth()&&y==today.getFullYear();const has=ev.some(e=>e.d==key);h+=`<td class='${isT?'tod':''}${has?' hasev':''}' data-d='${key}'>${dd}</td>`;if((st+dd)%7==0)h+='</tr><tr>'}h+='</tr></table>';
  const up=ev.filter(e=>e.d>=today.toISOString().slice(0,10)).sort((a,b)=>(a.d+a.t).localeCompare(b.d+b.t)).slice(0,4);h+=`<div class='cev'>${up.length?up.map(e=>`<div>📅 ${e.d} ${e.t||''} — <span></span></div>`).join(''):'<div style="opacity:.7">No upcoming events</div>'}</div><button class='copen'>Open Calendar</button>`;
  p.innerHTML=h;p.querySelectorAll('.cev span').forEach((s,i)=>s.textContent=up[i].title);p.querySelector('.cp').onclick=ev2=>{ev2.stopPropagation();mo--;if(mo<0){mo=11;y--}draw()};p.querySelector('.cn').onclick=ev2=>{ev2.stopPropagation();mo++;if(mo>11){mo=0;y++}draw()};
  p.querySelector('.copen').onclick=()=>{p.remove();launch('calendar')};p.querySelectorAll('td[data-d]').forEach(td=>td.ondblclick=()=>{p.remove();launch('calendar');setTimeout(()=>window.__calGo&&__calGo(td.dataset.d),300)})};draw();document.body.appendChild(p);flyClose(p,$('#tray'))});
/* ---------- tray API for background apps ---------- */
const TRAYX={};function trayAdd(id,o){trayRemove(id);const b=document.createElement('button');b.className='tapp';b.textContent=o.icon;b.title=o.title||'';b.setAttribute('aria-label',o.title||id);b.onclick=e=>{e.stopPropagation();o.onclick&&o.onclick(e)};if(o.menu)b.oncontextmenu=e=>{e.preventDefault();e.stopPropagation();cmenu(e.clientX,e.clientY,o.menu(),{title:o.title})};QSB.before(b);TRAYX[id]=b;return b}
function trayRemove(id){if(TRAYX[id]){TRAYX[id].remove();delete TRAYX[id]}}
/* ---------- session restore ---------- */
const sessK=()=>'bd95sess:'+uid();
window.__sessionSave=()=>{if(!USER)return;const list=[...new Set(wins.filter(o=>o.app&&!['settings','trash'].includes(o.app)).map(o=>o.app))].slice(0,8);svj(sessK(),list)};
window.__sessionRestore=()=>{pinDraw();try{NC=ld(NCK(),[]);ncUnread=0;ncBadge()}catch(e){}if(S.restore===false)return;const list=ld(sessK(),[]);if(!list.length)return;list.forEach((k,i)=>setTimeout(()=>{if(APPS[k]&&!wins.some(o=>o.app==k))launch(k)},150*i));notify('Welcome back','Your apps from last time were reopened.',{quiet:true,toast:3000})};
setInterval(()=>{try{window.__sessionSave()}catch(e){}},20000);
/* ---------- desktop files and folders (C:\USERS\<you>\DESKTOP) ---------- */
function deskFiles(){const d=node(homePath('DESKTOP'));return isDir(d)?d:null}
function deskSync(){if(!USER)return;const d=deskFiles();const want=new Set(d?Object.keys(d):[]);
 [...DESK.querySelectorAll(':scope > .ic.fic')].forEach(el=>{if(!want.has(el.dataset.fn)){el.remove();IC=IC.filter(x=>x!==el);delete cells[el.dataset.n]}});
 want.forEach(n=>{let el=DESK.querySelector(`:scope > .ic.fic[data-fn="${CSS.escape(n)}"]`);const v=d[n];
  if(!el){el=document.createElement('div');el.className='ic fic';el.dataset.fn=n;el.dataset.n='file:'+n;el.innerHTML='<b></b><span></span>';el.style.cssText+=';position:absolute;margin:0;touch-action:none;transition:left .1s,top .1s';DESK.appendChild(el);IC.push(el);
   let t=0;el.onclick=e=>{if(e.ctrlKey)return;DESK.querySelectorAll('.ic').forEach(x=>x.classList.remove('sel'));el.classList.add('sel');if(Date.now()-t<dblMs()||S.msingle)openPath(homePath('DESKTOP').concat(el.dataset.fn));t=Date.now()};
   el.onkeydown=e=>{if(e.key=='Enter')openPath(homePath('DESKTOP').concat(el.dataset.fn));if(e.key=='Delete')trashPut([homePath('DESKTOP').concat(el.dataset.fn)])&&EXPL.forEach(f=>f())};
   deskIconDrag(el)}
  el.querySelector('b').textContent=fIcon(n,v);el.querySelector('span').textContent=n;el.title=n+' — '+fType(n,v)});
 try{layIcons()}catch(e){}}
function deskIconDrag(d){let sx,sy,ox,oy,mv=0,dn=0;d.addEventListener('pointerdown',e=>{if(e.button!==0)return;dn=1;mv=0;sx=e.clientX;sy=e.clientY;ox=d.offsetLeft;oy=d.offsetTop;d.setPointerCapture(e.pointerId)});
 d.addEventListener('pointermove',e=>{if(!dn)return;const dx=e.clientX-sx,dy=e.clientY-sy;if(!mv&&Math.hypot(dx,dy)<5)return;mv=1;d.style.transition='none';d.style.zIndex=5;d.style.left=ox+dx+'px';d.style.top=oy+dy+'px'});
 d.addEventListener('pointerup',e=>{if(!dn)return;dn=0;if(!mv)return;d.style.transition='left .1s,top .1s';d.style.zIndex='';
  const tr=[...DESK.querySelectorAll(':scope > .ic.tr')].find(t=>{const r=t.getBoundingClientRect();return e.clientX>r.left&&e.clientX<r.right&&e.clientY>r.top&&e.clientY<r.bottom});
  if(tr){trashPut([homePath('DESKTOP').concat(d.dataset.fn)]);EXPL.forEach(f=>f());return}
  const w=wins.find(o=>{if(o.el.style.display=='none')return false;const r=o.el.getBoundingClientRect();return e.clientX>r.left&&e.clientX<r.right&&e.clientY>r.top&&e.clientY<r.bottom});
  if(w&&window.__dropFileOnWindow&&__dropFileOnWindow(w,homePath('DESKTOP').concat(d.dataset.fn))){layIcons();return}
  const g=gridInfo(),used=new Set();IC.forEach(o=>{if(o!==d&&cells[o.dataset.n])used.add(cells[o.dataset.n]+'')});const c=Math.max(0,Math.min(g.nc-1,Math.round((d.offsetLeft-8)/g.w))),r=Math.max(0,Math.min(g.nr-1,Math.round((d.offsetTop-8)/g.h)));cells[d.dataset.n]=freeCell(c,r,used,g.nc,g.nr);layIcons()});
 d.addEventListener('click',e=>{if(mv){e.stopImmediatePropagation();mv=0}},true)}
EXPL.add(deskSync);
DESK.addEventListener('contextmenu',e=>{const el=e.target.closest&&e.target.closest('#desk > .ic.fic');if(!el||e.shiftKey)return;e.preventDefault();e.stopImmediatePropagation();const p=homePath('DESKTOP').concat(el.dataset.fn);
 DESK.querySelectorAll('.ic').forEach(x=>x.classList.remove('sel'));el.classList.add('sel');
 cmenu(e.clientX,e.clientY,[['Open',()=>openPath(p),true,{icon:'↗'}],['Open with…',()=>window.openWith&&openWith(p)],'-',['Rename',()=>renameDlg(p),true,{key:'F2'}],['Move to Trash',()=>{trashPut([p]);EXPL.forEach(f=>f())},true,{key:'Del'}],'-',['Properties',()=>window.fileProps&&fileProps(p)]],{title:el.dataset.fn})},true);
function renameDlg(p){const o=document.createElement('div');o.className='rmodal';o.innerHTML=`<div class='rmbox'><h3>Rename</h3><input class='rn' style='width:100%' maxlength='60'><div class='rmmsg'></div><div class='rmrow'><button class='c'>Cancel</button><button class='ok pri'>Rename</button></div></div>`;document.body.appendChild(o);const i=o.querySelector('.rn');i.value=p.at(-1);i.focus();i.select();
 const ok=()=>{const nv=i.value.trim().toUpperCase(),d=dirOf(p);if(!validName(nv))return o.querySelector('.rmmsg').textContent='That name is not allowed.';if(nv!=p.at(-1)&&nv in d)return o.querySelector('.rmmsg').textContent='That name is already used.';const v=d[p.at(-1)];delete d[p.at(-1)];d[nv]=v;fsSave();o.remove();EXPL.forEach(f=>f())};
 o.querySelector('.ok').onclick=ok;o.querySelector('.c').onclick=()=>o.remove();i.onkeydown=e=>{e.stopPropagation();if(e.key=='Enter')ok();if(e.key=='Escape')o.remove()}}
/* desktop menu with new items and widgets (replaces the V24 menu) */
function deskNew(dirFlag){const d=deskFiles();if(!d)return;const n=uniq(d,dirFlag?'NEW FOLDER':'NEW TEXT DOCUMENT.TXT');d[n]=dirFlag?{}:'';fsSave();EXPL.forEach(f=>f());setTimeout(()=>renameDlg(homePath('DESKTOP').concat(n)),50)}
function deskMenu(x,y){
 cmenu(x,y,[['New folder',()=>deskNew(1),!!deskFiles(),{icon:'📁'}],['New text document',()=>deskNew(0),!!deskFiles(),{icon:'📄'}],['Add a widget…',()=>widgetPicker(),!!USER,{icon:'🧩'}],'-',
  ['Arrange icons by name',arrangeIcons],['Refresh',()=>{layIcons();EXPL.forEach(f=>f())},true,{key:'F5'}],'-',
  ['Open File Explorer',()=>launch('explorer')],['Open Rudolf Prompt',()=>launch('prompt')],['Show all windows',()=>overview(),true,{key:'Super+Tab'}],'-',['Change background…',()=>settings('disp')]],{title:'Desktop'})}
DESK.addEventListener('dragover',e=>{if(e.dataTransfer&&[...e.dataTransfer.types].includes('Files')){e.preventDefault();e.dataTransfer.dropEffect='copy'}});
DESK.addEventListener('drop',async e=>{const fl=e.dataTransfer&&[...e.dataTransfer.files];if(!fl||!fl.length)return;if(e.target.closest&&e.target.closest('.win'))return;e.preventDefault();const d=deskFiles();if(!d)return;
 for(const f of fl){if(f.size>64*1048576){notify('Too large',f.name+' is over 64 MB.',{kind:'error'});continue}const nm=f.name.toUpperCase().replace(/[\\\/:*?"<>|]/g,'_').slice(0,60);const bin=!/^text\/|json|xml|javascript/.test(f.type)&&!/\.(txt|lua|md|csv|json|html?|css|js|bat|cfg)$/i.test(f.name);
  const val=await new Promise(r=>{const fr=new FileReader();fr.onload=()=>r(fr.result);bin?fr.readAsDataURL(f):fr.readAsText(f)});const g=window.__sys&&secIngress(nm,val,'upload');if(g&&!g.ok){notify('Blocked',g.msg,{kind:'error'});continue}d[uniq(d,nm)]=val}fsSave();EXPL.forEach(f=>f())});
/* ---------- desktop widgets ---------- */
const WGK=()=>'bd95widgets:'+uid();let WG=[];
const WTYPES={clock:['🕒','Clock'],cal:['📅','Calendar'],note:['🗒️','Sticky note'],mon:['📈','System monitor'],weather:['⛅','Weather']};
function widgetPicker(){cmenu(innerWidth/2-90,innerHeight/3,Object.keys(WTYPES).map(k=>[WTYPES[k][1],()=>{WG.push({t:k,x:Math.max(20,innerWidth-300-WG.length*20),y:40+WG.length*30,txt:''});wgSave();wgDraw()},true,{icon:WTYPES[k][0]}]),{title:'Add a widget'})}
const wgSave=()=>{if(USER)svj(WGK(),WG)};
let wgTimers=[];
function wgDraw(){wgTimers.forEach(clearInterval);wgTimers=[];DESK.querySelectorAll('.wgt').forEach(x=>x.remove());if(!USER)return;
 WG.forEach((w,i)=>{const el=document.createElement('div');el.className='wgt wgt-'+w.t;el.style.left=w.x+'px';el.style.top=w.y+'px';el.setAttribute('role','region');el.setAttribute('aria-label',WTYPES[w.t][1]+' widget');el.innerHTML=`<div class='wgh'><span></span><button aria-label='Remove widget'>✕</button></div><div class='wgb'></div>`;el.querySelector('.wgh span').textContent=WTYPES[w.t][0]+' '+WTYPES[w.t][1];
  el.querySelector('.wgh button').onclick=()=>{WG.splice(i,1);wgSave();wgDraw()};const b=el.querySelector('.wgb');
  const h=el.querySelector('.wgh');let sx,sy,ox,oy,on=0;h.onpointerdown=e=>{if(e.target.tagName=='BUTTON')return;on=1;sx=e.clientX;sy=e.clientY;ox=w.x;oy=w.y;h.setPointerCapture(e.pointerId)};h.onpointermove=e=>{if(!on)return;w.x=Math.max(0,ox+e.clientX-sx);w.y=Math.max(0,oy+e.clientY-sy);el.style.left=w.x+'px';el.style.top=w.y+'px'};h.onpointerup=()=>{if(on){on=0;wgSave()}};
  if(w.t=='clock'){const f=()=>{const d=sysNow?sysNow():new Date();b.innerHTML=`<div class='wgbig'>${d.toLocaleTimeString([], {hour:'2-digit',minute:'2-digit'})}</div><div>${d.toLocaleDateString([], {weekday:'long',month:'long',day:'numeric'})}</div>`};f();wgTimers.push(setInterval(f,10000))}
  else if(w.t=='cal'){const d=sysNow?sysNow():new Date(),f=new Date(d.getFullYear(),d.getMonth(),1),st=(f.getDay()+6)%7,n=new Date(d.getFullYear(),d.getMonth()+1,0).getDate();let h2=`<b>${f.toLocaleDateString([], {month:'long',year:'numeric'})}</b><table class='calt'><tr>${'MTWTFSS'.split('').map(x=>`<th>${x}</th>`).join('')}</tr><tr>`;for(let k=0;k<st;k++)h2+='<td></td>';for(let k=1;k<=n;k++){h2+=`<td class='${k==d.getDate()?'tod':''}'>${k}</td>`;if((st+k)%7==0)h2+='</tr><tr>'}b.innerHTML=h2+'</tr></table>'}
  else if(w.t=='note'){const ta=document.createElement('textarea');ta.value=w.txt||'';ta.placeholder='Type a note…';ta.setAttribute('aria-label','Sticky note');ta.oninput=()=>{w.txt=ta.value;clearTimeout(ta._t);ta._t=setTimeout(wgSave,500)};b.appendChild(ta)}
  else if(w.t=='mon'){const f=async()=>{let cpu='—',mem='—';try{if(RD){const s=await rd('stats');cpu=s.cpu.toFixed(0)+'%';mem=Math.round((s.memTotal-s.memAvailable)/1048576)+' / '+Math.round(s.memTotal/1048576)+' MB'}else{const lag=window.__lagAvg||0;cpu=Math.min(100,Math.round(lag*2))+'% (estimate)';if(performance.memory)mem=Math.round(performance.memory.usedJSHeapSize/1048576)+' MB used'}}catch(e){}b.innerHTML=`<div>CPU: <b>${cpu}</b></div><div>Memory: <b>${mem}</b></div><div>Windows: <b>${wins.length}</b></div>`};f();wgTimers.push(setInterval(f,3000))}
  else if(w.t=='weather'){b.textContent='Loading…';weatherNow().then(t=>b.innerHTML=t).catch(()=>b.textContent='Weather needs an internet connection.')}
  DESK.appendChild(el)})}
async function weatherNow(){const loc=ld('bd95wloc',null)||{lat:52.52,lon:13.41,name:'Berlin'};const r=await fetch(`https://api.open-meteo.com/v1/forecast?latitude=${loc.lat}&longitude=${loc.lon}&current=temperature_2m,weather_code,wind_speed_10m&timezone=auto`);const j=await r.json();const c=j.current;
 const code={0:'☀️ Clear',1:'🌤️ Mostly clear',2:'⛅ Partly cloudy',3:'☁️ Cloudy',45:'🌫️ Fog',51:'🌦️ Drizzle',61:'🌧️ Rain',63:'🌧️ Rain',65:'🌧️ Heavy rain',71:'🌨️ Snow',80:'🌦️ Showers',95:'⛈️ Thunderstorm'}[c.weather_code]||'🌡️';
 return`<div class='wgbig'>${Math.round(c.temperature_2m)}°C</div><div>${code}</div><div style='opacity:.75'>${esc2(loc.name)} · wind ${Math.round(c.wind_speed_10m)} km/h</div>`}
const esc2=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'})[c]);
/* main-thread responsiveness estimate (used by widgets and Task Manager in the browser build) */
(function(){let last=performance.now(),acc=[];setInterval(()=>{const n=performance.now(),lag=Math.max(0,n-last-500);last=n;acc.push(lag);if(acc.length>10)acc.shift();window.__lagAvg=acc.reduce((a,b)=>a+b,0)/acc.length},500)})();
/* after sign-in */
(function(){const o=window.__afterLogin;window.__afterLogin=()=>{o&&o();WG=ld(WGK(),[]);wgDraw();deskSync();pinDraw();tbRefresh()}})();
(function(){const o=A.logoff;A.logoff=()=>{o();WG=[];wgDraw();DESK.querySelectorAll('.ic.fic').forEach(el=>{el.remove();IC=IC.filter(x=>x!==el)});PINS.innerHTML='';WS=1}})();
