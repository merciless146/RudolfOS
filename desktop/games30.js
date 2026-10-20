/* ===================== Games in the App Store (1.2.5) =====================
   A Games tab: Roblox (Sober) and Linux games from Flathub (Steam, Prism Launcher for Minecraft, RetroArch), and the
   games built into Rudolf OS. Each says how much it downloads and whether it needs the graphics card. On a computer
   where Rudolf OS draws with the processor (Task Manager › System › Drawn by), a note says which will be slow. */
const GAMES_FLAT=[
 {id:'com.valvesoftware.Steam',name:'Steam',icon:'🚂',gpu:1,size:1.5e9,desc:'Valve’s game store and launcher. Many Windows games run through its Proton layer. Steam on Flathub is packaged by the community, not by Valve: an administrator allows it in App Store › Installed (unverified apps).'},
 {id:'org.prismlauncher.PrismLauncher',name:'Prism Launcher (Minecraft)',icon:'⛏️',gpu:1,size:0.6e9,desc:'Plays Minecraft: Java Edition with your Microsoft account, and keeps several versions and mod packs apart.'},
 {id:'org.libretro.RetroArch',name:'RetroArch',icon:'🕹️',gpu:0,size:0.5e9,desc:'Emulators for many old consoles in one app. Most cores run on any computer; the newest consoles need the graphics card.'}];
const GAMES_BUILTIN=[['gameboy','Game Boy, GBA, SNES and DS games from Games on drive C:'],['quake','Quake (the shareware episode is included)'],['boards','Checkers, chess and other board games'],['poker','Card games']];
/* is the screen drawn by the graphics card? true / false / null (not known: the browser version) */
async function gamesGPU(){if(!RD)return null;try{const s=await rd('system');if(s&&s.renderer)return true;return false}catch(e){return null}}
function gamesLabel(gpu,soft){return gpu?`<span class='gmlab ${soft?'gmslow':''}'>Needs the graphics card</span>`:`<span class='gmlab gmany'>Runs anywhere</span>`}
async function gamesTab(B,back,W){B.innerHTML=`<div class='gmnote' hidden></div><h4 class='sh4'>Games to download</h4><div class='gmdl'></div><h4 class='sh4'>Built into Rudolf OS</h4><div class='gmbi'></div>`;
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
  d.querySelector('.sti').textContent=a.icon;d.querySelector('b').textContent=a.name;d.querySelector('.std').textContent=desc;d.querySelector('button').onclick=()=>launch(k);bi.appendChild(d)})}
function gamesOpenFlat(g){const x=typeof lxLaunchers=='function'&&lxLaunchers().find(l=>l.app==g.id);if(x)lxRun(x);else notify(g.name,'It is installed: open it from the Start menu.')}
/* Get: the Linux apps page for it (permissions, consent, the download question) */
async function gamesFlatPage(B,g,back,W){if(!RD)return notify(g.name,'Linux games run when Rudolf OS is started from its disc, USB drive or hard disk.',{kind:'error'});
 let s;try{s=await lxApi('apps/status')}catch(e){return notify(g.name,e.message,{kind:'error'})}
 if(!s.persistent)return notify(g.name,'Linux apps need Rudolf OS installed on this computer, or a USB drive with a RUDOLFDATA partition: they need somewhere to be kept.',{kind:'error'});
 let cat=LX.cat;if(!cat)try{cat=LX.cat=(await lxApi('apps/catalog')).apps||[]}catch(e){cat=null}
 const a=cat&&cat.find(x=>x.id==g.id);if(!a){notify(g.name,'First download Flathub’s app list (App Store › Linux apps).');const t=W&&W.body.querySelector(".tmtabs [data-t='linux']");t&&t.click();return}
 flatAppPage(B,a,back)}
/* the tab, after "Get apps" */
{const _as=appStore;appStore=function(){const W=_as();const tabs=W.body.querySelector('.tmtabs'),B=W.body.querySelector('.stb');if(!tabs||tabs.querySelector("[data-t='games']"))return W;
 const b=document.createElement('button');b.dataset.t='games';b.textContent='Games';tabs.querySelector("[data-t='get']").after(b);
 const show=()=>{tabs.querySelectorAll('button').forEach(x=>x.classList.toggle('on',x===b));gamesTab(B,show,W)};b.onclick=show;W.showGames=show;return W}}
