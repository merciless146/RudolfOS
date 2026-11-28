/* ===== 1.6.2 (docs/plan-1.6.2.md Part 40): Windows programs from before 1.5.9, and programs that do not start =====
   The owner's Vivobook, 2026-11-23: Starport GE's icon was there, and clicking it did nothing. Its launcher could not find its
   folder: the game was in the Wine prefix every account shared before 1.5.9 (the desktop user's, /home/rudolf/.wine), and since
   1.5.9 Wine runs as the account's own Linux user with a prefix of its own (rudolfd-users.inc), into which nothing was carried.
   1. Never a silent failure: a Debian or Windows program that ends within a few seconds with an error is said by rudolf-debian
      as a portal request of its own kind ("lxfail"), which the desktop collects as every other (Chromium's and Lite's desktop
      page: lxPortalPoll; the native desktop: its hidden page, oneapp59.js oaPortalRun, the same lxPortal): a message with the
      program's name, how it ended, the first real error and the last lines of its log, and, for a Windows program, what it runs
      with (Wine's version, its 64- and 32-bit GL and Vulkan packages, the renderer, /dev/dri, the room left, the newest logs; a
      game that ends at once is nearly always one of these); for a Windows program whose folder is only in the old prefix, the
      offer below with it.
   2. The offer (rudolfd-wineold.inc, wine/old and wine/bring): the Windows programs from before, copied into the account,
      owned by it, the old prefix untouched, room checked first, in the background with its progress; an administrator decides
      when the computer has several accounts (the old prefix was all of theirs). Shown once after sign-in (never copied without
      the person's choice), in Settings › Apps › Linux beside Wine, and in System health while it is pending.
   Everything here is the page's own, so the three engines show it alike (the owner's rule). */
window.__wineold62=1;
const WO62={st:null,dlg:null,poll:0,said:0};
function woSize(b){b=+b||0;return b>=1e9?(b/1e9).toFixed(1)+' GB':Math.max(1,Math.round(b/1e6))+' MB'}
async function woGet(dir){if(!RD)return null;try{const j=await rd('wine/old'+(dir?'?dir='+encodeURIComponent(dir):''));WO62.st=j;return j}catch(e){return null}}
function woProgress(j){if(!j)return'';if(j.phase=='finish')return'Putting the copy in its place…';if(j.phase=='launchers')return'Bringing their Start menu entries and icons…';
 const t=+j.total||0;return'Copying… '+woSize(j.copied)+(t?' of '+woSize(t)+' ('+Math.min(100,Math.floor(100*(+j.copied||0)/t))+' %)':'')+', '+(+j.files||0)+' files'}
function woWhat(s){if(s.several)return s.count+' program'+(s.count==1?'':'s')+' (this computer has '+s.accounts+' accounts, so their names are shown only to whoever an administrator lets bring them in)';
 return(s.programs||[]).join(', ')}
function woModeText(s){return s.mode=='beside'?'Your own Wine folder has programs of its own, so it stays as it is: the copy goes beside it (.wine-shared in your folder), and the Start menu entries brought with it use it.'
 :s.mode=='launchers'?'The programs are in your account already: their Start menu entries and icons are brought in.'
 :'Your own Wine folder has no programs of its own'+(s.own?'':' yet')+', so the copy takes its place; yours is kept beside it (.wine.own- and the date, in your folder), so nothing is lost.'}
/* the offer: what there is, room, who decides; Bring them in, Not now, Don't ask again */
async function woOffer(from){if(WO62.dlg&&WO62.dlg.isConnected)return;const s=await woGet();
 if(!s||!s.on||(!s.offer&&!s.running)){if(from!='notice')notify('Windows programs','There is nothing from before Rudolf OS 1.5.9 to bring into your account.');return}
 const o=document.createElement('div');o.className='rmodal wo62';WO62.dlg=o;
 o.innerHTML=`<div class='rmbox' role='alertdialog' aria-modal='true' aria-labelledby='wo62t' style='max-width:560px'><h3 id='wo62t'>Windows programs from before</h3>
<p class='wo62a'>Before Rudolf OS 1.5.9 the Windows programs of every account were kept together, in one Wine folder. Since 1.5.9 each account has a Wine folder of its own, so these are not in yours yet:</p>
<p class='wo62n' style='font-weight:600'></p><p class='wo62m'></p><p class='wo62s'></p><p class='wo62x' style='color:#b91c1c'></p>
<div class='wo62p' hidden><div style='height:8px;border-radius:4px;background:rgba(128,128,128,.25);overflow:hidden'><div class='wo62b' style='height:100%;width:0;background:#16a34a'></div></div><p class='wo62pt'></p></div>
<div class='rmrow'><button class='wo62later'>Not now</button><button class='wo62no'>Don’t ask again</button><button class='wo62go pri'>Bring them in</button></div></div>`;
 const q=c=>o.querySelector(c),done=()=>{o.remove();if(WO62.dlg===o)WO62.dlg=null};
 const draw=s=>{q('.wo62n').textContent=s.count?woWhat(s)+(s.launchers?' (and '+s.launchers+' Start menu entr'+(s.launchers==1?'y':'ies')+')':''):s.launchers?s.launchers+' Start menu entr'+(s.launchers==1?'y':'ies')+' of Windows programs':'A finished copy is waiting to take its place.';
  q('.wo62m').textContent=woModeText(s);
  q('.wo62s').textContent=(s.mode=='launchers'?'':'A copy of '+woSize(s.bytes)+' ('+woSize(s.free)+' free on your disk), owned by your account. The folder from before stays as it was. ')
   +(s.several?'The folder from before belonged to all '+s.accounts+' accounts of this computer, so an administrator decides, with their password. ':'')+(s.cut?'An earlier copy was cut off: it starts again from the beginning (your own Wine folder was not changed). ':'');
  q('.wo62x').textContent=s.room===false?'Not enough room: the copy needs '+woSize(s.bytes+268435456)+' and '+woSize(s.free)+' is free. Make some room, then try again.':'';
  q('.wo62go').disabled=s.room===false||!!s.running;q('.wo62no').hidden=!!s.declined||!!s.running};
 draw(s);document.body.appendChild(o);setTimeout(()=>{const b=q('.wo62go');if(b&&!b.disabled)b.focus()},30);
 const watch=()=>{q('.wo62p').hidden=false;q('.wo62go').disabled=true;q('.wo62no').hidden=true;q('.wo62later').textContent='Close (it goes on)';woWatch(j=>{if(!o.isConnected)return;
  q('.wo62pt').textContent=j.phase=='done'?'Done.':j.phase=='error'?'':woProgress(j);const t=+j.total||0;q('.wo62b').style.width=(j.phase=='done'?100:t?Math.min(100,100*(+j.copied||0)/t):5)+'%';
  if(j.phase=='error'){q('.wo62x').textContent=j.error||'It did not finish.';q('.wo62go').disabled=false;q('.wo62go').textContent='Try again'}
  if(j.phase=='done'){q('.wo62later').textContent='Close';q('.wo62m').textContent='They are in your account now: their Start menu entries come back in a moment.'+(j.own?' Your own Wine folder from before is kept as '+j.own+' in your folder.':'')}})};
 if(s.running)watch();
 q('.wo62later').onclick=done;o.addEventListener('keydown',e=>{if(e.key=='Escape')done()});
 q('.wo62no').onclick=async()=>{try{await rdPost('wine/decline','{}')}catch(e){}done();notify('Windows programs','You will not be asked again. Settings › Apps › Linux still offers to bring them in.')};
 q('.wo62go').onclick=async()=>{q('.wo62x').textContent='';let body='{}';
  if(s.several){const cr=await elevate('Bring the Windows programs from before Rudolf OS 1.5.9 into this account (they belonged to every account of this computer)',{cred:1});if(!cr)return;body=JSON.stringify({n:cr.n,p:cr.p})}
  q('.wo62go').disabled=true;try{await rdPost('wine/bring',body)}catch(e){q('.wo62x').textContent=e.message;q('.wo62go').disabled=false;return}watch()}}
/* the copy's progress, every second until it ends (on: each answer's job); when it ends, the Start menu asked again and said */
function woWatch(on){const tick=async()=>{const s=await woGet();const j=s&&s.job;if(j)try{on(j)}catch(e){}
  if(s&&!s.running&&j&&(j.phase=='done'||j.phase=='error'||!j.alive)){clearInterval(WO62.poll);WO62.poll=0;if(j.phase=='done'&&!WO62.said){WO62.said=1;
   notify('Windows programs','The Windows programs from before are in your account now.');try{DEB.appsAt=0;DEB.st=null;await debApps();lxRegister()}catch(e){}}
   if(j.phase!='done'&&j.phase!='error')try{on(Object.assign({},j,{phase:'error',error:'The copy was cut off; your own Wine folder was not changed. Try again.'}))}catch(e){}}};
 WO62.said=0;clearInterval(WO62.poll);WO62.poll=setInterval(tick,1000);tick()}
/* a program that did not start (rudolf-debian's "lxfail") */
async function woFail(id,r){try{await lxAnswer(id,{})}catch(e){}
 /* (one left from more than 10 minutes ago, when no desktop was there to say it: not said now) */
 const at=+(String(id).split('-')[1]||0);if(at&&Date.now()/1000-at>600)return;
 const name=String(r.name||'A program'),st=+r.status||0,secs=+r.secs||0;
 const how=st>=128?'it was stopped (signal '+(st-128)+')':st==127?'its program could not be found':'exit status '+st;
 const s=r.wine?await woGet(String(r.dir||'')):null;const offer=s&&s.on&&(s.inOld||(s.offer&&s.count));
 const o=document.createElement('div');o.className='rmodal wo62f';
 o.innerHTML=`<div class='rmbox' role='alertdialog' aria-modal='true' aria-labelledby='wo62ft' style='max-width:600px'><h3 id='wo62ft'></h3><p class='wo62fa'></p>
<pre class='wo62fl' style='white-space:pre-wrap;word-break:break-word;max-height:9em;overflow:auto;font-size:12px;background:rgba(128,128,128,.12);padding:6px;border-radius:4px'></pre>
<pre class='wo62fd' hidden style='white-space:pre-wrap;word-break:break-word;max-height:12em;overflow:auto;font-size:11px;background:rgba(128,128,128,.12);padding:6px;border-radius:4px;margin-top:6px'></pre>
<p class='wo62fo'></p><p class='wo62fg' style='font-size:12px;opacity:.75'></p><div class='rmrow'><button class='wo62fc'>Close</button><button class='wo62fb pri' hidden>Bring them in…</button></div></div>`;
 const q=c=>o.querySelector(c);q('h3').textContent=name+' did not start';
 q('.wo62fa').textContent='It ended '+(secs<1?'at once':'after '+Math.round(secs)+' s')+' ('+how+') without opening a window. The last lines of its log:';
 const L=(r.lines||[]).map(String).filter(Boolean);q('.wo62fl').textContent=L.length?L.join('\n'):'(its log is empty)';
 const D=(r.diag||[]).map(String).filter(Boolean);   /* (what it runs with: rudolf-debian read it when the program ended) */
 if(D.length){const d=q('.wo62fd');d.hidden=false;d.textContent='What it runs with (Wine, its 32- and 64-bit GL and Vulkan, the renderer, the GPU, room):\n'+D.join('\n')}
 q('.wo62fg').textContent=(r.log?'Its whole log: '+r.log:'')+(r.diaglog?' · its whole check: '+r.diaglog:'');
 if(offer){const f=String(r.dir||'').split('/').filter(Boolean).pop();
  q('.wo62fo').textContent=(s.inOld&&f?'Its folder ('+f+') is among the Windows programs from before Rudolf OS 1.5.9, which are not in your account yet.':'Your account does not have the Windows programs from before Rudolf OS 1.5.9 yet: it may be one of them.')+' They can be brought in: a copy, owned by your account.';
  const b=q('.wo62fb');b.hidden=false;b.onclick=()=>{o.remove();woOffer('fail')}}
 const done=()=>o.remove();q('.wo62fc').onclick=done;o.addEventListener('keydown',e=>{if(e.key=='Escape')done()});
 document.body.appendChild(o);setTimeout(()=>{(offer?q('.wo62fb'):q('.wo62fc')).focus()},30)}
if(typeof lxPortal=='function'){const _p=lxPortal;lxPortal=async function(id,r){if(r&&r.kind=='lxfail')return woFail(id,r);return _p.apply(this,arguments)}}
/* Settings › Apps › Linux: beside Wine */
if(typeof debWineCell=='function')debWineCell=__after(debWineCell,function(cell,st){if(!cell||!st||!st.wine||!RD)return;woGet().then(s=>{if(!s||!s.on||!(s.offer||s.running)||!cell.isConnected||cell.querySelector('.wo62c'))return;
 const d=document.createElement('div');d.className='wo62c';d.style.marginTop='6px';d.append(document.createTextNode(s.running?'The Windows programs from before Rudolf OS 1.5.9 are being brought into your account. ':
  'The Windows programs from before Rudolf OS 1.5.9 ('+(s.count?(s.several?s.count+' program'+(s.count==1?'':'s'):woWhat(s)):'their Start menu entries')+') are not in your account yet. '));
 const b=document.createElement('button');b.className='wo62cb';b.textContent=s.running?'Show progress…':'Bring them in…';b.onclick=()=>woOffer('settings');d.appendChild(b);cell.appendChild(d)})},'wineold62.js');
/* System health: named while it is pending */
if(typeof hlUsersRow=='function')hlUsersRow=__after(hlUsersRow,function(c){if(!RD)return;woGet().then(s=>{if(!s||!s.on||!(s.pending||s.running)||!c.isConnected||c.querySelector('.wo62h'))return;
 const d=document.createElement('div');d.className='wo62h r2';d.style.marginTop='6px';d.innerHTML="<label>Windows programs from before</label><div class='wo62ht'></div>";
 const t=d.querySelector('.wo62ht');t.textContent=s.running?'Being brought into your account: '+woProgress(s.job)+' ':'Not in your account yet: '+(s.count?(s.several?s.count+' program'+(s.count==1?'':'s'):woWhat(s)):'their Start menu entries')+(s.bytes?' ('+woSize(s.bytes)+')':'')+
  ', from before Rudolf OS 1.5.9, when every account shared one Wine folder. Their Start menu entries do not start until they are brought in. ';
 t.style.color='#b45309';const b=document.createElement('button');b.textContent=s.running?'Show progress…':'Bring them in…';b.onclick=()=>woOffer('health');t.appendChild(b);
 const at=c.querySelector('.hl59u');if(at)at.after(d);else c.appendChild(d)})},'wineold62.js');
/* once after the account's sign-in (the desktop's page; on the native desktop its hidden page only), never again for that account */
function woMainPage(){return typeof ONEAPP=='undefined'||!ONEAPP||ONEAPP.k=='do-bg'}
async function woNotice(){if(!RD||!USER||!woMainPage())return;const k='bd95wo62:'+uid();if(ld(k,0))return;const s=await woGet();if(!s||!s.on||!s.pending)return;
 if(ld(k,0))return;svj(k,1);woOffer('notice')}
if(typeof userLogin=='function'){const o=userLogin;userLogin=async function(){const r=await o.apply(this,arguments);if(RD)setTimeout(woNotice,12000);return r}}
