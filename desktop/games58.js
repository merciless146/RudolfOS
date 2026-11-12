/* ===== Rudolf OS: games from the Rudolf OS Library (docs/plan-games.md), the desktop's side.
   - Each game is a web-app package in the Library (rudolfos-library, apps/rudolf-<game>). Here it is a few lines in
     LIB_GAMES: the Start menu, the App Store's Games tab, search. Opened without its package, it offers to download
     it (libNeed, as Quake does); the system download does not grow with games.
   - The games use the game kit (rudolfos-library lib/gamekit), which runs in the package's sandboxed frame and talks
     to the desktop with messages {rudolfGame: 1, op, id, game}. Any web-app package frame may use them:
       hello  -> {dark, reduceFlash}             the desktop's theme and Reduce motion
       load   -> {data}                          the game's save, for this account
       save   {data} -> {}                       kept in the desktop's database (mirrored by rudolfd, backed up),
                                                 one record per package, game and account; up to 256 KB
     and the desktop sends pause / resume (the window minimised or on another workspace, the computer locked, the
     screen saver on) and settings (the theme or Reduce motion changed).
   ===== */
window.__games58=1;
const LIB_GAMES=[
 {k:'frostbarricade',pkg:'rudolf-frostbarricade',name:'25 Days',icon:'🛡️',size:31e3,desc:'For older players (gritty cartoon horror and combat): hold a barricade against corrupted elves for twenty-five days and nights. Move Rudolf behind it and shoot with winter weapons that never run out of ammunition; train your survivors, wait for the sleigh. (Formerly Frost Barricade.)'},
 {k:'nightflight',pkg:'rudolf-nightflight',name:'Night Flight',icon:'🌙',size:31e3,desc:'Rudolf’s bullet-hell flight through a winter night: lasers from his glowing nose, a charged beam, friends to save. Stages 1 and 2 so far.'},
 {k:'logicpack',pkg:'rudolf-logicpack',name:'Logic Pack',icon:'🧩',size:25e3,desc:'Sudoku, Picture Grid (nonograms) and Cages puzzles, made fresh every time with one answer each. Easy to Hard and a Daily puzzle of each; notes, undo and hints that explain the next step.'},
 {k:'blockdrop',pkg:'rudolf-blockdrop',name:'Block Drop',icon:'🧱',size:21e3,desc:'A falling-blocks puzzle: fill rows to clear them. Prisms burst every block of their colour and set off chains. Classic, Relaxed and a 3-minute Daily game; best scores kept with your account.'},
 {k:'faultlines',pkg:'rudolf-faultlines',name:'Fault Lines',icon:'⛏️',size:17e3,desc:'A logic puzzle: clear the ground without opening a fault. Three fields and a Daily one; best times kept with your account.'}];
LIB_GAMES.forEach(g=>{
 if(typeof LIB_APPS!='undefined')LIB_APPS[g.pkg]={app:g.k,title:g.name,size:g.size,why:g.name};
 regApp(g.k,{name:g.name,icon:g.icon,kw:'game games '+g.name+' library',open:()=>gameLibOpen(g),re:new RegExp('^'+g.name.replace(/[.*+?^${}()|[\]\\]/g,'\\$&')+'( |$)')});
 if(typeof GAMES_BUILTIN!='undefined'&&!GAMES_BUILTIN.some(x=>x[0]==g.k))GAMES_BUILTIN.push([g.k,g.desc])});
(function(){const ul=typeof $=='function'&&$('#menu ul');if(!ul)return;const ref=ul.querySelector('li[data-a=settings]');
 LIB_GAMES.forEach(g=>{if(ul.querySelector(`li[data-a=${g.k}]`))return;const li=document.createElement('li');li.dataset.a=g.k;li.textContent=g.icon+' '+g.name;ul.insertBefore(li,ref||null)})})();
/* opens the game: downloads its package first when it is not there */
async function gameLibOpen(g){
 if(!RD)return notify(g.name,'Games from the Rudolf OS Library need Rudolf OS itself (the bootable disc, a USB drive or the hard disk).',{kind:'error'});
 if(!(await libNeed(g.pkg)))return;
 return pkgOpen(g.pkg)}
/* the Packages list does not show a Library game a second time (it has its own entry) */
{const _pr=pkgRegister;pkgRegister=function(){_pr();let gone=false;LIB_GAMES.forEach(g=>{if(APPS['pkg:'+g.pkg]){delete APPS['pkg:'+g.pkg];gone=true}});
 if(gone){const ul=$('#menu ul');ul&&ul.querySelectorAll('li.rapp').forEach(li=>{if(LIB_GAMES.some(g=>li.dataset.v25=='pkg:'+g.pkg))li.remove()})}}}

/* ---------- the game kit's messages ---------- */
const GAME_MAX=256*1024;
/* the package frame a message came from, and its package's name */
function gameFrame(src){for(const f of document.querySelectorAll('iframe')){let w=null;try{w=f.contentWindow}catch(e){}if(w&&w===src){const m=/^\/pkg\/([a-z0-9][a-z0-9+._-]*)\/www\//.exec(new URL(f.src,location.href).pathname);return m?{f,pkg:decodeURIComponent(m[1])}:null}}return null}
const gameKey=(pkg,game)=>'game:'+pkg+':'+game+':'+uid();
const gameDark=()=>document.documentElement.classList.contains('dark');
const gameRM=()=>!!(typeof S!='undefined'&&S&&S.rmotion);
const GAME_FRAMES=new Map();   /* frame -> {pkg, paused} */
addEventListener('message',async e=>{const m=e.data;if(!m||m.rudolfGame!==1||typeof m.op!='string')return;
 const at=gameFrame(e.source);if(!at)return;
 const reply=o=>{try{e.source.postMessage(Object.assign({rudolfGame:1,op:'reply',id:m.id},o),'*')}catch(x){}};
 if(typeof m.game!='string'||!/^[a-z0-9-]{2,40}$/.test(m.game))return reply({ok:false,error:'bad game id'});
 if(!GAME_FRAMES.has(at.f))GAME_FRAMES.set(at.f,{pkg:at.pkg,paused:false});
 if(m.op=='hello')return reply({ok:true,dark:gameDark(),reduceFlash:gameRM()});
 if(m.op=='load'){let v;try{v=await kvGet(gameKey(at.pkg,m.game))}catch(x){}return reply({ok:true,data:v===undefined?null:v})}
 if(m.op=='save'){let s;try{s=JSON.stringify(m.data===undefined?null:m.data)}catch(x){return reply({ok:false,error:'not JSON'})}
  if(s.length>GAME_MAX)return reply({ok:false,error:'too large'});
  if(typeof USER!='undefined'&&USER&&USER.r=='guest')return reply({ok:true,kept:'session'});   /* (a guest's things go when they sign out) */
  try{await kvSet(gameKey(at.pkg,m.game),JSON.parse(s));reply({ok:true})}catch(x){reply({ok:false,error:'not kept'})}
  return}
 reply({ok:false,error:'unknown op'})});
/* pausing: once a second, each game frame's window seen or not */
function gameTick(){if(!GAME_FRAMES.size)return;const all=!!(document.hidden||window.LOCKED||(typeof SAVER!='undefined'&&SAVER));
 for(const[f,st]of GAME_FRAMES){if(!f.isConnected){GAME_FRAMES.delete(f);continue}
  const hide=all||!f.offsetParent||f.clientWidth<2;
  if(hide!=st.paused){st.paused=hide;try{f.contentWindow.postMessage({rudolfGame:1,op:hide?'pause':'resume'},'*')}catch(e){}}}}
setInterval(gameTick,1000);document.addEventListener('visibilitychange',gameTick);
/* the theme or Reduce motion changed */
let GAME_SET='';
setInterval(()=>{const k=gameDark()+'/'+gameRM();if(k==GAME_SET)return;const first=!GAME_SET;GAME_SET=k;if(first)return;
 for(const f of GAME_FRAMES.keys())try{f.contentWindow.postMessage({rudolfGame:1,op:'settings',dark:gameDark(),reduceFlash:gameRM()},'*')}catch(e){}},2000);
