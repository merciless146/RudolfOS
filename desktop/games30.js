/* ===================== Games in the App Store (1.2.5) =====================
   A Games tab: Roblox (Sober) and Linux games from Flathub (Steam, Prism Launcher for Minecraft, RetroArch), and the
   games built into Rudolf OS, and (1.3.0) every game on Flathub with a search box and genres. Each says how much it downloads and whether it needs the graphics card. On a computer
   where Rudolf OS draws with the processor (Task Manager › System › Drawn by), a note says which will be slow. */
const GAMES_FLAT=[
 {id:'com.valvesoftware.Steam',name:'Steam',icon:'🚂',gpu:1,size:1.5e9,desc:'Valve’s game store and launcher. Many Windows games run through its Proton layer. Steam on Flathub is packaged by the community, not by Valve: an administrator allows it in App Store › Installed (unverified apps).'},
 {id:'org.prismlauncher.PrismLauncher',name:'Prism Launcher (Minecraft)',icon:'⛏️',gpu:1,size:0.6e9,desc:'Plays Minecraft: Java Edition with your Microsoft account, and keeps several versions and mod packs apart.'},
 {id:'org.libretro.RetroArch',name:'RetroArch',icon:'🕹️',gpu:0,size:0.5e9,desc:'Emulators for many old consoles in one app. Most cores run on any computer; the newest consoles need the graphics card.'}];
const GAMES_BUILTIN=[['gameboy','Game Boy, GBA, SNES and DS games from Games on drive C:'],['quake','Quake (the shareware episode is included)'],['boards','Checkers, chess and other board games'],['poker','Card games']];
/* is the screen drawn by the graphics card? true / false / null (not known: the browser version) */
async function gamesGPU(){if(!RD)return null;try{const s=await rd('system');if(s&&s.renderer)return true;return false}catch(e){return null}}
function gamesLabel(gpu,soft){return gpu?`<span class='gmlab ${soft?'gmslow':''}'>Needs the graphics card</span>`:`<span class='gmlab gmany'>Runs anywhere</span>`}
async function gamesTab(B,back,W){B.innerHTML=`<div class='gmnote' hidden></div><h4 class='sh4'>Games to download</h4><div class='gmdl'></div><h4 class='sh4'>Built into Rudolf OS</h4><div class='gmbi'></div><h4 class='sh4'>All games on Flathub</h4><div class='gmall'></div>`;
 const hw=await gamesGPU(),soft=hw===false;if(!B.isConnected)return;
 if(soft){const n=B.querySelector('.gmnote');n.hidden=false;n.innerHTML=`<b>This computer draws with the processor.</b> Rudolf OS is not using a graphics card, so games marked “Needs the graphics card” will be slow or will not start. The built-in games run well. <button class='gmtm'>Task Manager › System</button> shows what draws the screen; Settings › Display › Graphics card has the choices.`;
  n.querySelector('.gmtm').onclick=()=>launch('taskmgr')}
 const dl=B.querySelector('.gmdl');
 /* Roblox: the same card as on Get apps, with the label */
 if(typeof nativeCard=='function')NATIVE.forEach(a=>{const c=nativeCard(dl,a);c.querySelector('.natbadge').insertAdjacentHTML('afterend',' '+gamesLabel(1,soft)+` <span class='msg gmsz'>about ${appGB(1e9)}</span>`)});
 let inst=[];if(RD&&typeof lxInstalled=='function')try{inst=(await lxInstalled())||[]}catch(e){}
 GAMES_FLAT.forEach(g=>{const d=document.createElement('div');d.className='stc gmc';const has=inst.some(a=>a.id==g.id);
  d.innerHTML=`<div class='sti'></div><div style='flex:1;min-width:0'><b></b> <span class='natbadge'>Linux app</span> ${gamesLabel(g.gpu,soft)} <span class='msg gmsz'>about ${appGB(g.size)}</span><div class='std'></div></div><div class='stbtn'><button class='pri'>${has?'Open':'Get'}</button></div>`;
  d.querySelector('.sti').textContent=g.icon;d.querySelector('b').textContent=g.name;d.querySelector('.std').textContent=g.desc;dl.appendChild(d);
  d.querySelector('button').onclick=()=>has?gamesOpenFlat(g):gamesFlatPage(B,g,back,W)});
 /* the real download sizes, once Flathub answers (each is a separate question to Flathub) */
 if(RD)GAMES_FLAT.forEach((g,i)=>{if(inst.some(a=>a.id==g.id))return;lxApi('apps/size?id='+encodeURIComponent(g.id)).then(s=>{const el=dl.querySelectorAll('.gmc .gmsz')[i];if(el&&s&&s.ok&&s.download)el.textContent=appGB(s.download+(s.runtimeDownload||0))+' to download'}).catch(()=>{})});
 const bi=B.querySelector('.gmbi');GAMES_BUILTIN.filter(([k])=>APPS[k]).forEach(([k,desc])=>{const a=APPS[k],d=document.createElement('div');d.className='stc gmc';
  d.innerHTML=`<div class='sti'></div><div style='flex:1;min-width:0'><b></b> ${gamesLabel(0)}<div class='std'></div></div><div class='stbtn'><button class='pri'>Play</button></div>`;
  d.querySelector('.sti').textContent=a.icon;d.querySelector('b').textContent=a.name;d.querySelector('.std').textContent=desc;d.querySelector('button').onclick=()=>launch(k);bi.appendChild(d)});
 gamesAll(B.querySelector('.gmall'),B,back)}
function gamesOpenFlat(g){const x=typeof lxLaunchers=='function'&&lxLaunchers().find(l=>l.app==g.id);if(x)lxRun(x);else notify(g.name,'It is installed: open it from the Start menu.')}
/* Get: the Linux apps page for it (permissions, consent, the download question) */
async function gamesFlatPage(B,g,back,W){if(!RD)return notify(g.name,'Linux games run when Rudolf OS is started from its disc, USB drive or hard disk.',{kind:'error'});
 let s;try{s=await lxApi('apps/status')}catch(e){return notify(g.name,e.message,{kind:'error'})}
 if(!s.persistent)return notify(g.name,'Linux apps need Rudolf OS installed on this computer, or a USB drive with a RUDOLFDATA partition: they need somewhere to be kept.',{kind:'error'});
 let cat=LX.cat;if(!cat)try{cat=LX.cat=(await lxApi('apps/catalog')).apps||[]}catch(e){cat=null}
 const a=cat&&cat.find(x=>x.id==g.id);if(!a){notify(g.name,'First download Flathub’s app list (App Store › Linux apps).');const t=W&&W.body.querySelector(".tmtabs [data-t='linux']");t&&t.click();return}
 flatAppPage(B,a,back,'Games')}
/* the tab, after "Get apps" */
{const _as=appStore;appStore=function(){const W=_as();const tabs=W.body.querySelector('.tmtabs'),B=W.body.querySelector('.stb');if(!tabs||tabs.querySelector("[data-t='games']"))return W;
 const b=document.createElement('button');b.dataset.t='games';b.textContent='Games';tabs.querySelector("[data-t='get']").after(b);
 const show=()=>{tabs.querySelectorAll('button').forEach(x=>x.classList.toggle('on',x===b));gamesTab(B,show,W)};b.onclick=show;W.showGames=show;return W}}

/* 1.3.0: every game in Flathub's app list, with a search box and genres, as App Store › Linux apps has for all apps */
async function gamesAll(box,B,back){if(!box)return;const msg=t=>{box.innerHTML=`<p class='msg'>${t}</p>`};
 if(!RD)return msg('Linux games run when Rudolf OS is started from its disc, USB drive or hard disk.');
 let s;try{s=await lxApi('apps/status')}catch(e){return msg(lxEsc(e.message))}
 if(!s.flatpak)return msg('This Rudolf OS build cannot run Linux apps.');
 if(!s.persistent)return msg('Linux games need Rudolf OS installed on this computer, or a USB drive with a RUDOLFDATA partition: they need somewhere to be kept.');
 let cat=LX.cat;if(!cat)try{cat=LX.cat=(await lxApi('apps/catalog')).apps||[]}catch(e){cat=null}
 if(!box.isConnected)return;
 if(!cat){box.innerHTML=`<p class='msg'>Thousands of games are on Flathub. First Rudolf OS downloads Flathub's app list (about 10 MB).</p><button class='pri gmdlb'>Download the app list</button><pre class='jl' hidden></pre>`;
  box.querySelector('.gmdlb').onclick=async e=>{e.target.disabled=true;const pre=box.querySelector('pre');pre.hidden=false;try{await lxApi('apps/refresh')}catch(er){pre.textContent=er.message;e.target.disabled=false;return}
   const j=await lxJobWait(l=>{pre.textContent+=l+'\n'});if(j.status==0){LX.cat=null;gamesAll(box,B,back)}else{pre.textContent+='\n'+(j.crashed?jobCrashText(j):'That did not work. Settings › Apps › Linux › Check says why.');e.target.disabled=false}};return}
 await lxInstalled();const inst=new Set((LX.inst||[]).map(a=>a.id));const games=cat.filter(lxIsGame);
 const genres=LX_GENRES.filter(([g])=>games.some(a=>(a.cats||[]).includes(g)));
 box.innerHTML=`<div class='lxbar'><input type='search' class='lxq gmq' placeholder='Search games' aria-label='Search games'><select class='lxc gmg' aria-label='Genre'><option value=''>All genres</option>${genres.map(([k,n])=>`<option value='${k}'>${n}</option>`).join('')}</select><label class='ck'><input type='checkbox' class='lxv gmv' checked> Verified only</label></div><div class='lxlist gml'></div><p class='msg lxfoot gmfoot'></p>`;
 const L=box.querySelector('.gml'),Q=box.querySelector('.gmq'),G=box.querySelector('.gmg'),V=box.querySelector('.gmv');let limit=60;
 const io=new IntersectionObserver(es=>es.forEach(en=>{if(en.isIntersecting){const el=en.target;io.unobserve(el);lxImg(el,el.dataset.id)}}),{root:B});
 function draw(){const q=Q.value.trim().toLowerCase(),g=G.value;
  const l=games.filter(a=>(!V.checked||a.verified||inst.has(a.id))&&(!g||(a.cats||[]).includes(g))&&(!q||(a.name+' '+a.summary+' '+a.id+' '+(a.dev||'')).toLowerCase().includes(q)))
   .sort((a,b)=>(inst.has(b.id)-inst.has(a.id))||(b.verified-a.verified)||a.name.localeCompare(b.name));
  L.innerHTML='';l.slice(0,limit).forEach(a=>{const d=document.createElement('div');d.className='stc lxrow';d.tabIndex=0;
   const gn=LX_GENRES.filter(([k])=>(a.cats||[]).includes(k)).map(([,n])=>n).slice(0,2).join(', ');
   d.innerHTML=`<div class='sti lxi' data-id='${lxEsc(a.id)}'>🎮</div><div style='flex:1;min-width:0'><b>${lxEsc(a.name)}</b>${a.verified?` <span class='lxver' title='Verified by Flathub: published by the game's own developer'>✔ verified</span>`:''}${inst.has(a.id)?` <span class='natbadge'>Installed</span>`:''}${gn?` <span class='msg'>· ${lxEsc(gn)}</span>`:''}<div class='std'>${lxEsc(a.summary)}</div></div>`;
   d.onclick=d.onkeydown=e=>{if(e.type=='keydown'&&e.key!='Enter')return;flatAppPage(B,a,back,'Games')};L.appendChild(d);io.observe(d.querySelector('.lxi'))});
  box.querySelector('.gmfoot').textContent=l.length>limit?`Showing ${limit} of ${l.length} games. Search or choose a genre to find more.`:`${l.length} game${l.length==1?'':'s'} from Flathub; they run in a sandbox.`;
  if(l.length>limit){const m=document.createElement('button');m.textContent='Show more';m.onclick=()=>{limit+=60;draw()};L.appendChild(m)}}
 let t;Q.oninput=()=>{clearTimeout(t);t=setTimeout(()=>{limit=60;draw()},200)};G.onchange=V.onchange=()=>{limit=60;draw()};draw()}
