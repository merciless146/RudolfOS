/* ===== 1.6.3 (FreeBuff's second round, passed on by the owner): shutting down, restarting and signing out ask about
   unsaved work first, and the ☰ menu's ⏻ Power always gives a choice =====
   Before, closeAll() ran each window's onclose but closed the window whatever it answered, so a Notepad, Paint or
   WordPro with unsaved changes was closed without asking; and in the browser edition ⏻ Power went straight to "It's now
   safe to turn off your computer" with no choice. Now:
   - closeAllAsk() closes the windows one by one as their ✕ does; a window that asks (Save / Don't save / Cancel) is
     waited for: saved or not saved, it closes and the next one goes; Cancel stops the shutdown, restart or sign-out,
     and the desktop stays as it is.
   - Shut down and Restart (shutdownDlg, core25.js) and Sign out (A.logoff) call it first.
   - Without the system service (the browser edition), ⏻ Power shows the same choice (Shut down, Restart, Sign out,
     Cancel) instead of shutting down at once.
   The native desktop has its own power menu and already asks each window as its ✕ does (rudolf-native.c). */
function closeAllAsk(){return new Promise(async res=>{
 for(const w of wins.slice()){if(!wins.includes(w))continue;const before=new Set(document.querySelectorAll('.rmodal'));
  try{w.close()}catch(e){try{w.el.remove();w.tb&&w.tb.remove()}catch(x){}const i=wins.indexOf(w);if(i>=0)wins.splice(i,1)}
  if(!wins.includes(w))continue;
  /* the window asked: wait for its answer (10 minutes at most) */
  const t0=Date.now();const asked=()=>[...document.querySelectorAll('.rmodal')].some(m=>!before.has(m)&&m.isConnected);
  const ok=await new Promise(r=>{const tick=()=>{if(!wins.includes(w))return r(true);if(!asked()||Date.now()-t0>600000)return r(false);setTimeout(tick,150)};setTimeout(tick,150)});
  if(!ok){try{w.focus&&w.focus()}catch(e){}return res(false)}}
 res(true)})}
function powerDlgWeb(){const o=document.createElement('div');o.className='rmodal';
 o.innerHTML=`<div class='rmbox' role='dialog' aria-label='Power'><h3>Power</h3><p>What do you want to do?</p><div class='rmrow'><button data-a='poweroff'>⏻ Shut down</button><button data-a='reboot'>↻ Restart</button><button data-a='logoff'>🚪 Sign out</button><button data-a='x'>Cancel</button></div></div>`;
 document.body.appendChild(o);o.querySelector('button').focus();
 o.addEventListener('keydown',e=>{if(e.key=='Escape'){e.stopPropagation();o.remove()}});
 o.onclick=async e=>{const a=e.target.dataset&&e.target.dataset.a;if(!a)return;if(a=='x')return o.remove();
  o.querySelectorAll('button').forEach(b=>b.disabled=true);o.style.visibility='hidden';
  if(!(await closeAllAsk())){o.remove();return}o.remove();
  if(a=='logoff')return A.logoff();
  try{userLogout()}catch(x){}
  if(a=='reboot'){try{fsFlush()}catch(x){}location.reload();return}
  POWER63_OFF()}}
let POWER63_OFF=null;
(function(){
 /* Sign out: ask about unsaved work first (Start › Sign out, the power dialogs' Sign out) */
 const lo=A.logoff;A.logoff=async()=>{if(SESSION&&!(await closeAllAsk()))return;lo()};
 /* ⏻ Power: the choice. The base A.off (base_v24.html) is kept as the browser edition's "Shut down" without a second sign-out */
 POWER63_OFF=()=>{closeAll();sfx('shutdown');$('#off').style.display='flex'};
 A.off=()=>{if(RD){shutdownDlg();return}powerDlgWeb()};
})();
