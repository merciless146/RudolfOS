/* ===== Rudolf OS 1.5.9: game mode puts Oracle's other tabs to sleep (docs/plan-1.5.9.md 9.2, step 1) =====
   On the Vivobook (3.4 GB) Friday Night Funkin' grows to about 2 GB and the memory guard ended it twice on
   2026-11-12. When game mode starts (a page full screen or with the pointer captured, a Linux game full screen),
   Oracle's tabs that are not their window's current tab are put to sleep: their page is closed and its memory
   freed, the address and title kept (💤 on the tab), and selecting the tab loads it again. A tab stays awake while
   it plays sound or video (music, a call) or holds text typed into it and not sent; a tab that does not answer in
   2 s stays too. Both desktops (the shell's tabs and rudolf-webkit's answer the same "busy" question).
   Settings › System health › Memory: "In game mode, free memory for the game" (on by default).
   Step 3 (9.2): background work waits while game mode is on (the system update check, the Linux apps' and Debian's
   daily updates: their own timers try again later), and Blitzen's model (1-2 GB) is unloaded unless Blitzen is the
   window in front; the next question loads it again. */
Object.assign(DEFS.disp,{gmsleep:true});
if(!DKEYS.includes('gmsleep'))DKEYS.push('gmsleep');if(S.gmsleep===undefined)S.gmsleep=true;
const GSL={on:false,busy:false};
/* the tabs that may sleep: real tabs that are not their window's current one, not the game's own */
function gslCandidates(){const out=[];if(typeof RT=='undefined')return out;
 for(const [tid,w] of RT){if(!w||typeof w.sleep!='function'||(typeof GAME_FS!='undefined'&&GAME_FS.has(tid)))continue;
  try{if(w.shown&&w.shown())continue}catch(e){continue}out.push([tid,w])}return out}
/* Blitzen's model out of memory (its next question loads it again) */
async function gslBlitzen(){if(typeof lxApi!='function')return 0;   /* (without rudolfd its call fails: 0) */try{if(typeof BLZ!='undefined'&&[...BLZ.wins].some(w=>w.el&&w.el.isConnected&&w.el.classList.contains('act')&&!w.min))return 0;
 const s=await lxApi('ai/status');if(!s||!s.loaded)return 0;await lxApi('ai/unload');return s.mem||1}catch(e){return 0}}
async function gslSleep(){if(GSL.busy||!S.gmsleep)return 0;GSL.busy=true;let n=0,ai=0;const names=[];
 try{for(const [tid,w] of gslCandidates()){const b=await rtBusy(tid);if(b.m||b.f)continue;
   let title='';try{title=w.title()}catch(e){}if(w.sleep()){n++;if(title)names.push(title)}}
  ai=await gslBlitzen()}finally{GSL.busy=false}
 const parts=[];if(n)parts.push(`${n==1?'a tab in Oracle was':n+' tabs in Oracle were'} put to sleep${names.length?' ('+names.slice(0,3).join(', ')+(names.length>3?', …':'')+')':''}; select a tab to load it again`);
 if(ai)parts.push("Blitzen's model was unloaded; it loads again with your next question");
 if(parts.length)notify('Game mode freed memory',parts.join('. ').replace(/^./,c=>c.toUpperCase())+'.',{quiet:true});
 return n}
/* when game mode starts (checked each second: game mode has several sources, the shell's tabs, the desktop's own
   full screen and pointer capture, a Linux game) */
setInterval(()=>{let on=false;try{on=gameMode()}catch(e){}const was=GSL.on;GSL.on=on;
 if(on&&!was)setTimeout(()=>{try{if(gameMode())gslSleep()}catch(e){}},1500)},1000);
/* Settings › System health › Memory */
{const _h=healthPanel;healthPanel=function(c,W){const r=_h.apply(this,arguments);try{
 const d=document.createElement('div');d.className='gsl59 r2';d.style.marginTop='6px';
 d.innerHTML=`<label>Game mode</label><div>${ckH('gsl59ck',"In game mode, free memory for the game",!!S.gmsleep)}<div class='msg'>Oracle's background tabs sleep (a tab that is not its window's current one closes its page and keeps its address; selecting it loads it again; tabs playing sound or holding typed text stay awake), and Blitzen's model is unloaded unless Blitzen is in front. Updates wait until the game ends.</div></div>`;
 const at=c.querySelector(':scope > .hlev');if(at)at.before(d);else c.appendChild(d);
 d.querySelector('#gsl59ck').onchange=e=>{liveSet(c,'gmsleep',e.target.checked)}}catch(e){console.warn(e)}return r}}
