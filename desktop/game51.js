/* ===== Rudolf OS 1.5.1: cloud gaming and game mode =====
   - Xbox Cloud Gaming and GeForce NOW in the Start menu and the Games tab: each opens in Oracle (a real tab: the
     stream is WebRTC H.264, which Oracle's Chromium decodes). Controllers reach the page (the shell's gamepad
     permission, 1.5.1), and full screen and pointer capture work there as for any page.
   - Game mode: while a page is full screen or has the pointer captured (a cloud game, a browser game), notifications
     wait in the notification centre instead of popping up and sounding; afterwards one notification says how many
     came. (Controller presses count as input since 1.5.1, rudolf-display: the screen no longer locks mid-game.) */
window.__game51=1;
const CLOUD_GAMES=[
 {k:'xcloud',name:'Xbox Cloud Gaming',icon:'🎮',url:'https://www.xbox.com/play',desc:'Play Xbox games streamed from Microsoft\'s servers: Fortnite is free with a Microsoft account, others need Game Pass Ultimate. Plug in a controller, or use the keyboard and mouse where the game allows it. Needs a fast connection (about 20 Mbps).'},
 {k:'gfn',name:'GeForce NOW',icon:'☁️',url:'https://play.geforcenow.com/',desc:'Play games you own on Steam, Epic and others, streamed from NVIDIA\'s servers. The free tier has a queue and sessions of about an hour. Needs a fast connection (about 20 Mbps).'}];
CLOUD_GAMES.forEach(g=>{regApp(g.k,{name:g.name,icon:g.icon,open:()=>browser(g.url),re:new RegExp('^'+g.name)});
 if(typeof GAMES_BUILTIN!='undefined'&&!GAMES_BUILTIN.some(x=>x[0]==g.k))GAMES_BUILTIN.unshift([g.k,g.desc])});
(function(){const ul=typeof $=='function'&&$('#menu ul');if(!ul)return;const ref=ul.querySelector('li[data-a=settings]');
 CLOUD_GAMES.forEach(g=>{if(ul.querySelector(`li[data-a=${g.k}]`))return;const li=document.createElement('li');li.dataset.a=g.k;li.textContent=g.icon+' '+g.name;ul.insertBefore(li,ref||null)})})();
/* which real tabs are full screen (the shell says so: tabev fs) */
const GAME_FS=new Set();
addEventListener('message',e=>{const m=e.data;if(!m||m.rudolfShell!='tabev'||m.ev!='fs')return;if(m.on)GAME_FS.add(m.tid);else GAME_FS.delete(m.tid);gameModeTick()});
/* 1.5.4: and while a Linux app's window in front is full screen (a Steam game, Big Picture, a game in Wine) */
function gameModeNative(){return typeof LX!='undefined'&&!!LX.mode&&LX.mode.startsWith('win')&&(LX.wins||[]).some(w=>w.shown&&w.fullscreen)}
function gameMode(){return GAME_FS.size>0||!!document.fullscreenElement||!!document.pointerLockElement||gameModeNative()}
let GAME_HELD=0,GAME_WAS=false;
function gameModeTick(){const on=gameMode();if(GAME_WAS&&!on&&GAME_HELD){const n=GAME_HELD;GAME_HELD=0;
  try{__notifCentre.add('While you were playing',`${n} notification${n==1?'':'s'} came in. They are in the notification centre.`,{kind:'info',action:()=>ncToggle(true)})}catch(e){}}
 GAME_WAS=on}
document.addEventListener('fullscreenchange',gameModeTick);document.addEventListener('pointerlockchange',gameModeTick);
(function(){const nc=window.__notifCentre;if(!nc||nc.__g51)return;const add=nc.add.bind(nc);
 nc.add=function(t,b,o){o=o||{};if(!gameMode()||o.kind=='error')return add(t,b,o);
  /* kept in the centre, without the pop-up and the sound */
  GAME_HELD++;GAME_WAS=true;const d=S.dnd;S.dnd=true;try{return add(t,b,Object.assign({},o,{sound:false}))}finally{S.dnd=d}};nc.__g51=1})();
