/* 1.5.7 (plan stage 7, Part 3 items 1, 2 and 5): apps that run in the background, and memory.
   - Task Manager's Apps lists the apps that run, also with no window open (Flatpak apps such as Discord in the tray,
     Windows programs in Wine, Linux programs in Debian), with their memory and "End" (rudolfd's apps/running and
     apps/end, through rudolf-memguard: asked to quit, ended 5 s later). Steam stuck half-closed could only be ended
     over the development link before.
   - When the memory guard quits an app it says so at once (the event "memory closed NAME MB").
   - Before a game starts with little memory free, apps open in the background are offered to be closed.
   - Settings › System health › Memory: the compressed memory (Automatic, Smaller or Off, at the next restart), the
     memory now, what the memory guard closed in the last 7 days, and whether it may close apps with no window open. */
function appName57(x){if(x.kind=='flatpak'){const a=APPS['lx:'+x.app];if(a&&a.name)return a.name}return String(x.name||'').replace(/\.exe$/i,'')}
const APP57_KIND={flatpak:'Linux app',wine:'Windows program',debian:'Linux program'};
async function apps57(){try{return (await rd('apps/running')).apps||[]}catch(e){return null}}
async function appEnd57(x){const n=appName57(x);try{const r=await rdPost('apps/end?id='+encodeURIComponent(x.id));notify(n+' was ended',r&&r.forced?'It did not quit when asked, so it was stopped.':'',{quiet:true});return true}
 catch(e){notify(n+' was not ended',e.message,{kind:'error'});return false}}
/* the memory guard closed an app (rudolf-memguard writes "memory closed NAME MB" to rudolfd's events) */
evOn(t=>{for(const m of String(t).matchAll(/^memory closed (.+) (\d+)$/gm)){const n=m[1],mb=+m[2];
 notify(n+' was closed to free memory',`The computer ran out of memory, so Rudolf OS closed ${n}, which had no window open and used ${mb>=1024?(mb/1024).toFixed(1)+' GB':mb+' MB'}. Open it again when you need it.`,{kind:'warn',action:()=>settings('health')})}});
/* Task Manager › Apps: the apps that run, below the windows */
function tmApps57(B,list,redraw){if(!RD||!list)return;const box=document.createElement('div');box.className='tma57';
 const h=document.createElement('h4');h.className='sh4';h.textContent='Apps and programs running';box.appendChild(h);
 if(!list.length){const m=document.createElement('div');m.className='msg';m.textContent='No Linux apps or Windows programs are running.';box.appendChild(m);B.appendChild(box);return}
 const t=document.createElement('table');t.className='tmt';t.innerHTML='<thead><tr><th>App</th><th>Kind</th><th>Window</th><th style="text-align:right">Memory</th><th></th></tr></thead><tbody></tbody>';
 list.slice().sort((a,b)=>b.rss-a.rss).forEach(x=>{const r=t.tBodies[0].insertRow();r.innerHTML=`<td></td><td></td><td></td><td style='text-align:right'></td><td><button class='et'>End</button> <button class='fq'>Force quit</button></td>`;
  r.cells[0].textContent=appName57(x);r.cells[1].textContent=APP57_KIND[x.kind]||x.kind;r.cells[2].textContent=x.window?'Open':'None (in the background)';r.cells[3].textContent=fmtB(x.rss);
  const b=r.querySelector('.et');b.title='Asks it to quit; if it has not quit after 5 seconds, it is stopped';b.onclick=async()=>{b.disabled=true;b.textContent='Ending…';await appEnd57(x);redraw&&redraw()};
  const f=r.querySelector('.fq');f.title='Ends it and all its background processes at once, without asking it to quit';f.onclick=async()=>{f.disabled=true;await forceQuit57(x,appName57(x));redraw&&redraw()}});
 box.appendChild(t);B.appendChild(box)}
/* a game is about to start: with little memory free, offer to close what runs in the background */
async function gameMem57(name,appId){if(!RD)return true;try{const s=await rd('stats');if(!s.memTotal||s.memAvailable/s.memTotal>=0.35)return true}catch(e){return true}
 const list=(await apps57())||[];const bg=list.filter(x=>!x.window&&x.app!==appId&&x.rss>=100*1048576).sort((a,b)=>b.rss-a.rss);if(!bg.length)return true;
 const names=bg.map(appName57),tot=bg.reduce((a,x)=>a+x.rss,0),lst=names.length==1?names[0]:names.slice(0,-1).join(', ')+' and '+names.at(-1);
 return new Promise(res=>{const o=document.createElement('div');o.className='rmodal gm57';o.innerHTML=`<div class='rmbox' role='alertdialog'><h3>Close apps for the game?</h3><p></p><div class='rmrow'><button class='no'>Play anyway</button><button class='ok pri'></button></div></div>`;
  o.querySelector('p').textContent=`${lst} ${bg.length==1?'is':'are'} running in the background and use${bg.length==1?'s':''} ${fmtB(tot)}. Little memory is free, so ${name} may run slowly or be closed. Close ${bg.length==1?'it':'them'} for the game?`;
  o.querySelector('.ok').textContent=bg.length==1?'Close '+names[0]+' and play':'Close them and play';
  const done=async c=>{o.remove();if(c)for(const x of bg)await appEnd57(x);res(true)};o.querySelector('.no').onclick=()=>done(false);o.querySelector('.ok').onclick=()=>done(true);document.body.appendChild(o);o.querySelector('.ok').focus()})}
{const _r=lxRun;lxRun=async function(x,file){try{const a=(LX.inst||ld(LXK,[])).find(i=>i.id==x.app);if(a&&lxIsGame(a))await gameMem57(x.name,x.app)}catch(e){}return _r.apply(this,arguments)}}
/* Settings › System health › Memory */
async function memPanel57(c){if(!RD)return;const d=document.createElement('div');d.className='mem57';const at=c.querySelector(':scope > .hlev');if(at)at.before(d);else c.appendChild(d);
 const draw=async()=>{let m;try{m=await rd('memory/settings')}catch(e){d.innerHTML='';return}const h=await hlGet(500);const wk=(h&&h.events||[]).filter(e=>e.k=='memory'&&h.now-e.t<7*86400);
  const used=m.total-m.available,cz=m.compressed||{},adm=isAdmin();
  d.innerHTML=`<h4 class='sh4 sgrp' style='font-size:15px;margin-top:18px'>Memory</h4><div class='r2'><label>In use now</label><div class='mnow'></div><label>Compressed memory</label><div class='mcz'></div>
<label>Size of compressed memory</label><div><select class='mzs'${adm&&m.persistent?'':' disabled'}><option value='auto'>Automatic (recommended)</option><option value='small'>Smaller</option><option value='off'>Off</option></select><div class='msg mzn'></div></div>
<label>When memory runs out</label><label class='ck'><input type='checkbox' class='mca'${adm&&m.persistent?'':' disabled'}> Close apps with no window open (such as Discord in the tray)</label>
<label>Closed to free memory</label><div class='mcl'></div>
<label>Video problems</label><div><label class='ck'><input type='checkbox' class='mvl'${adm&&m.persistent?'':' disabled'}> Log video problems (the Lite desktop)</label><div class='msg'>Writes the video and sound system's warnings to a log that the system report includes. Starts at the next restart; leave it off unless you are looking into a problem.</div></div></div>`;
  const q=s=>d.querySelector(s);q('.mnow').textContent=`${fmtB(used)} of ${fmtB(m.total)} (${Math.round(used*100/m.total)} %)`;
  q('.mcz').textContent=cz.on?`${fmtB(cz.size)}, holding ${fmtB(cz.stored)} in ${fmtB(cz.used)}${cz.size?' ('+Math.round(cz.stored*100/cz.size)+' % full)':''}`:'Off';
  q('.mzs').value=m.zram;q('.mzn').textContent='Automatic: as large as the memory (at most 8 GB). Too small runs out sooner; too large spends the computer\'s time compressing. A change applies at the next restart.'+(m.persistent?'':' (Kept only on an installed Rudolf OS.)');
  q('.mca').checked=!!m.closeApps;q('.mvl').checked=!!m.videoLog;
  q('.mcl').textContent=wk.length?'':'Nothing in the last 7 days.';if(wk.length){const ul=document.createElement('ul');ul.style.margin='0';wk.slice(0,20).forEach(e=>{const li=document.createElement('li');li.textContent=hlWhen(e.t)+': '+e.m.replace(/^memory ran out: /,'');ul.appendChild(li)});q('.mcl').appendChild(ul)}
  const save=async qs=>{if(!await elevate('Change the memory settings'))return draw();try{await rdPost('memory/settings?'+qs);notify('Memory settings saved',qs.startsWith('zram')?'The new size applies at the next restart.':'',{quiet:!qs.startsWith('zram')})}catch(e){notify('Memory settings',e.message,{kind:'error'})}draw()};
  q('.mzs').onchange=e=>save('zram='+e.target.value);q('.mca').onchange=e=>save('closeApps='+(e.target.checked?1:0));q('.mvl').onchange=e=>save('videoLog='+(e.target.checked?1:0))};
 draw()}
{const _h=healthPanel;healthPanel=function(c,W){const r=_h.apply(this,arguments);try{memPanel57(c)}catch(e){console.warn(e)}return r}}
/* 1.5.7: Force quit, on an app's right-click menu below Close (a window's title bar and taskbar button, a Linux or
   Windows program's taskbar button, the Start menu): the app and every process of it, its background ones included
   (Steam's helpers, a tray app, a game's crash handler), ended at once without asking it to quit (rudolf-memguard
   --end ID --force), as Task Manager's End process tree does for one process. For an app of the desktop itself
   (Notepad, Paint...): its windows closed at once, without asking to save. */
let FQ57={list:null,t:0};
async function fqList(){if(!RD)return [];if(FQ57.list&&Date.now()-FQ57.t<4000)return FQ57.list;FQ57.list=(await apps57())||[];FQ57.t=Date.now();return FQ57.list}
async function forceQuit57(x,name){try{const r=await rdPost('apps/end?id='+encodeURIComponent(x.id)+'&force=1');FQ57.list=null;
  notify(name+' was force-quit',r&&r.processes?`${r.processes} process${r.processes==1?'':'es'} ended, background ones included.`:'',{quiet:true});return true}
 catch(e){notify(name+' was not force-quit',e.message,{kind:'error'});return false}}
/* a Linux or Windows program's window (taskbar button): its app, by the window's id; else the program behind the window */
function fqXItem(win,name){return['Force quit '+name,async()=>{const l=await fqList(),x=l.find(a=>(a.wins||[]).includes(win));
 if(x)return forceQuit57(x,appName57(x)||name);
 try{await rdPost('apps/kill?win='+encodeURIComponent(win));notify(name+' was force-quit','Its program was ended.',{quiet:true})}catch(e){notify(name+' was not force-quit',e.message,{kind:'error'})}},true,{icon:'⛔'}]}
/* a window of the desktop's own apps (or of a web app): all of that app's windows, closed at once */
function fqWinItem(W){const k=W.app,a=k&&APPS[k];const name=a?a.name:W.el.querySelector('.t').textContent;if(W===window.__tmW)return null;
 if(a&&a.lxApp)return fqAppItem(k);
 return['Force quit '+name,()=>{const all=k?wins.filter(o=>o.app===k):[W];all.forEach(o=>{try{o.onclose=null}catch(e){}try{o.close()}catch(e){}if(wins.includes(o)){o.el.remove();o.tb&&o.tb.remove();wins.splice(wins.indexOf(o),1)}});
  notify(name+' was force-quit',all.length>1?all.length+' windows closed.':'',{quiet:true})},true,{icon:'⛔'}]}
/* the Start menu: a Linux app (Flatpak) that runs, with a window or only in the background */
function fqAppItem(k){const a=APPS[k];if(!a||!a.lxApp||!RD)return null;const x=(FQ57.list||[]).find(e=>e.id=='flatpak:'+a.lxApp);
 return['Force quit '+a.name,async()=>{const l=await fqList(),y=l.find(e=>e.id=='flatpak:'+a.lxApp);if(!y){notify(a.name+' is not running','',{quiet:true});return}$('#menu').style.display='none';forceQuit57(y,a.name)},!!x||!FQ57.list,{icon:'⛔'}]}
/* (the list kept fresh while the Start menu is open, so its Force quit knows what runs) */
if(RD)addEventListener('pointerover',e=>{if(e.target.closest&&e.target.closest('#menu'))fqList().catch(()=>{})},{passive:true});
