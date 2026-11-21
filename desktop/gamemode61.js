/* ===== Rudolf OS 1.6.1 (docs/plan-1.6.1.md Part 19): game mode, all of it =====
   Game mode (1.5.1, 1.5.9) held notifications, put Oracle's other tabs to sleep and made the updates wait while a page or a
   Linux game was full screen. Now it is also kept by root (/usr/lib/rudolf/gamemode, rudolfd's game/): while a game is full
   screen the processor's energy preference is raised (the power settings: amd_pstate and intel_pstate, nothing elsewhere)
   and, for a Linux game, the desktop's own processor time is lowered (its group's cpu.weight), and everything is put back
   when the game leaves full screen or ends, a crash of the game, the page or the native desktop included (a full screen
   counts only while it is said again every 2 s). Here:
   - Standard's page says so (game/on?src=page every 2 s while its own game mode is on; game/off when it ends);
   - every page asks what root knows (game/status), so a full screen only the native desktop sees (src=native) holds this
     page's notifications and background work too (gameMode() is on);
   - while a game other than the page itself is in front, the page's own animations stop (html.gm61) and so do the
     dynamic backgrounds' moving parts (as behind a window);
   - Settings › System health › Memory › Game mode: "Game mode" (on by default) turns all of it off and on, for measuring
     (docs/plan-1.6.1.md Part 19's checklist), kept by root too (game/set) so games asking through GameMode's D-Bus API
     get nothing while it is off.
   GameMode's D-Bus requests (rudolf-portal) raise the processor's preference only: they are not a full screen, so they do
   not hold notifications or slow the desktop. */
window.__gamemode61=1;
Object.assign(DEFS.disp,{gamemode:true});if(!DKEYS.includes('gamemode'))DKEYS.push('gamemode');if(S.gamemode===undefined)S.gamemode=true;
const GM61={st:null,fsOther:false,sent:false,kind:'',t:0};
const gm61Local=gameMode;   /* (game51.js's: this page's own full screens, pointer capture, a Linux window in front) */
gameMode=function(){if(S.gamemode===false)return false;return gm61Local()||GM61.fsOther};
const gm61Main=()=>!(typeof ONEAPP!='undefined'&&ONEAPP);
const gm61Svc=()=>typeof OA_SVC!='undefined'&&OA_SVC;
function gm61Take(st){if(!st||!st.ok)return;GM61.st=st;GM61.fsOther=!!st.enabled&&(st.sources||[]).some(s=>s.src!='page');gm61Throttle()}
async function gm61Poll(){if(!RD||typeof USER=='undefined'||!USER)return;try{gm61Take(await rd('game/status'))}catch(e){}}
/* Standard's page: its own full screen, said again every 2 s while it lasts */
async function gm61Say(){if(!RD||!gm61Main())return;let on=false,kind='page';
 try{on=S.gamemode!==false&&gm61Local();if(on&&typeof gameModeNative=='function'&&gameModeNative()&&!(typeof GAME_FS!='undefined'&&GAME_FS.size)&&!document.fullscreenElement)kind='linux'}catch(e){}
 try{if(on){gm61Take(await rdPost(`game/on?src=page&kind=${kind}`));GM61.sent=true;GM61.kind=kind}
  else if(GM61.sent){GM61.sent=false;GM61.kind='';gm61Take(await rdPost('game/off?src=page'))}}catch(e){}}
/* the page's own work stops while another program's game is in front (not the page's own full screen) */
function gm61Throttle(){let t=false;try{t=gameMode()&&!document.fullscreenElement&&!(typeof GAME_FS!='undefined'&&GAME_FS.size)&&(GM61.kind=='linux'||GM61.fsOther)}catch(e){}
 const h=document.documentElement;if(h.classList.contains('gm61')!=t){h.classList.toggle('gm61',t);try{if(typeof dynOvCheck=='function')dynOvCheck()}catch(e){}}}
if(typeof dynOvCheck=='function'){const _d=dynOvCheck;dynOvCheck=function(){const r=_d.apply(this,arguments);try{if(document.documentElement.classList.contains('gm61'))DYNOV.vis=false}catch(e){}return r}}
{const st=document.createElement('style');st.textContent='html.gm61 *,html.gm61 *::before,html.gm61 *::after{animation-play-state:paused!important}';document.head.appendChild(st)}
setInterval(()=>{gm61Say();gm61Throttle()},2000);
setInterval(()=>{if(gm61Main()||gm61Svc()||!document.hidden)gm61Poll()},3000);
/* Settings › System health › Memory › Game mode: the switch, above gamesleep59.js's "free memory for the game" */
{const _h=healthPanel;healthPanel=function(c,W){const r=_h.apply(this,arguments);try{const g=c.querySelector('.gsl59 > div');if(!g)return r;
 const d=document.createElement('div');d.className='gm61s';
 d.innerHTML=`${ckH('gm61ck','Game mode',S.gamemode!==false)}<div class='msg'>While a game is full screen: the processor runs at full speed (not with battery saver), the desktop's own work waits behind a Linux game, and notifications are held and shown after. Everything goes back as it was when the game leaves full screen or ends. Games that ask for GameMode get the processor's speed only.</div><div class='msg gm61n'></div>`;
 g.prepend(d);const n=d.querySelector('.gm61n');
 const say=()=>{const s=GM61.st;if(!s){n.textContent='';return}n.textContent=!s.enabled?'Game mode is off.':s.on?`On now${s.throttled?'; the desktop waits behind the game':''}${(s.games||[]).length?'; asked by '+s.games.map(x=>x.name||x.pid).join(', '):''}.`:''};
 say();gm61Poll().then(say);
 d.querySelector('#gm61ck').onchange=e=>{const on=e.target.checked;liveSet(c,'gamemode',on);if(RD)rdPost('game/set?enabled='+(on?1:0)).then(s=>{gm61Take(s);say()}).catch(()=>{})}}catch(e){console.warn(e)}return r}}
/* the setting kept by root as well, once after the page starts (an older page's choice, or one made in the browser) */
setTimeout(()=>{if(RD&&gm61Main()&&S.gamemode===false)rdPost('game/set?enabled=0').catch(()=>{})},4000);
