/* ===== Rudolf OS 1.5.0: remote access (docs/plan-1.5.0.md, part 5) =====
   rudolf-remote serves this computer's screen on https://<address>:8443 to any web browser on the local network.
   Here: Settings › Remote access (on/off for administrators, the address, the pairing code, the certificate's
   fingerprint, which accounts may connect unattended), the accounts rudolf-remote checks (name, PBKDF2 hash, role,
   unattended: POST remote/accounts), the prompt that allows or refuses a connection, and the marker shown while
   someone is connected, with End. rudolfd: GET remote, POST remote?on=, remote/answer, remote/end. */
window.__remote50=1;
let RM50=null,RM50ask=null,RM50bar=null,RM50t=null;
async function rmStatus(){if(!RD)return null;try{return RM50=await rd('remote')}catch(e){return null}}
function rmAccounts(){return ACCTS.filter(a=>a.r!='guest'&&a.n&&a.h).map(a=>[String(a.n).replace(/[\t\n]/g,' '),a.h,a.r,a.ru?1:0].join('\t')).join('\n')+'\n'}
let rmSyncT=0;function rmSync(){if(!RD)return;clearTimeout(rmSyncT);rmSyncT=setTimeout(()=>{rdPost('remote/accounts',rmAccounts()).catch(()=>{})},700)}
/* accounts change (a new password, a new account, unattended): rudolf-remote gets the new list */
if(typeof saveUser=='function'){const _su50=saveUser;saveUser=function(){const r=_su50.apply(this,arguments);try{if(RM50&&RM50.on)rmSync()}catch(e){}return r}}
const rmFmtSince=t=>{const m=Math.max(0,Math.round((Date.now()/1000-t)/60));return m<1?'just now':m==1?'1 minute':m+' minutes'};
/* the prompt at the computer */
function rmAsk(p){if(RM50ask&&RM50ask.id==p.id)return;if(RM50ask)RM50ask.o.remove();
 const o=document.createElement('div');o.className='rmodal';o.style.zIndex=99999;
 o.innerHTML=`<div class='rmbox' role='alertdialog' aria-label='Remote connection' style='max-width:460px'><h3>🖥️ Allow a remote connection?</h3><p><b class='u'></b> wants to see and control this computer from <b class='f'></b>.</p><p class='msg' style='color:#6b7280;font-size:12px'>Allow it only if you know who it is. They can use everything you can see on the screen. <span class='cd'></span></p><div class='rmrow'><button class='n'>Refuse</button><button class='y pri'>Allow</button></div></div>`;
 o.querySelector('.u').textContent=p.user;o.querySelector('.f').textContent=p.from;document.body.appendChild(o);sfx&&sfx('notify');typeof promptFront=='function'&&promptFront(o);
 const ans=async a=>{o.remove();RM50ask=null;try{await rdPost('remote/answer?id='+p.id+'&allow='+(a?1:0))}catch(e){}};
 o.querySelector('.y').onclick=()=>ans(true);o.querySelector('.n').onclick=()=>ans(false);
 const cd=o.querySelector('.cd'),t0=p.time;const tick=()=>{if(!o.isConnected)return;const left=60-Math.round(Date.now()/1000-t0);cd.textContent=left>0?`Refused by itself in ${left} s.`:'';if(left<=0){o.remove();RM50ask=null;return}setTimeout(tick,1000)};tick();
 RM50ask={id:p.id,o}}
/* the marker while someone is connected */
function rmBar(s){if(!s){if(RM50bar){RM50bar.remove();RM50bar=null}return}
 if(!RM50bar){RM50bar=document.createElement('div');RM50bar.className='rm50bar';RM50bar.setAttribute('role','status');
  RM50bar.style.cssText='position:fixed;top:0;left:50%;transform:translateX(-50%);z-index:99998;background:#b91c1c;color:#fff;padding:4px 10px;border-radius:0 0 8px 8px;font:13px system-ui,sans-serif;display:flex;gap:8px;align-items:center;box-shadow:0 2px 8px rgba(0,0,0,.3)';
  RM50bar.innerHTML=`<span class='t'></span><button style='font:inherit;padding:1px 8px;border-radius:6px;border:1px solid #fff;background:transparent;color:#fff;cursor:pointer'>End</button>`;
  RM50bar.querySelector('button').onclick=async()=>{try{await rdPost('remote/end')}catch(e){}};document.body.appendChild(RM50bar)}
 RM50bar.querySelector('.t').textContent=`🖥️ ${s.user} is connected from ${s.from} (${rmFmtSince(s.since)})${s.unattended?' · unattended access':''}`}
async function rmPoll(){clearTimeout(RM50t);const s=await rmStatus();
 if(s&&s.on&&!rmPoll.synced){rmPoll.synced=1;rmSync()}
 if(s&&s.on){if(s.pending)rmAsk(s.pending);else if(RM50ask){RM50ask.o.remove();RM50ask=null}rmBar(s.session)}else{rmBar(null);if(RM50ask){RM50ask.o.remove();RM50ask=null}}
 RM50t=setTimeout(rmPoll,s&&s.on?2000:30000)}
if(RD)setTimeout(rmPoll,5000);
/* Settings › Remote access */
function remotePanel(c,W){const adm=isAdmin();
 c.innerHTML=`<h3>Remote access</h3><p class='msg' style='margin:0 0 10px'>See and use this computer from a phone, tablet or another computer on the same network, in any web browser. Nothing to install there. Someone at this computer is asked to allow each connection, unless the account may connect unattended.</p>
<div class='r2'><label>Remote access</label><div><label class='ck'><input type='checkbox' class='rmo'${adm?'':' disabled'}> Allow remote access to this computer</label> <span class='rms' style='color:#6b7280;font-size:12px'></span></div></div>
<div class='rmon'></div>${adm?'':`<div class='lockbar'>🔒 Only administrators can change remote access.</div>`}`;
 const cb=c.querySelector('.rmo'),st=c.querySelector('.rms'),box=c.querySelector('.rmon');
 const draw=async()=>{const s=await rmStatus();box.innerHTML='';
  if(!s){st.textContent='Remote access needs the installed Rudolf OS (bootable disc or USB).';cb.disabled=true;return}
  cb.checked=s.on;st.textContent=s.live?'needs Rudolf OS installed, or a USB drive that keeps files':s.on?(s.running?'On':'Starting…'):'Off';
  if(!s.on)return;
  const sec=(h,html)=>{const d=document.createElement('div');d.innerHTML=`<h4 class='sh4'>${h}</h4>${html}`;box.appendChild(d);return d};
  const addr=s.addresses.length?s.addresses.map(a=>`<div style='font:600 16px monospace;margin:2px 0'>https://${a}:${s.port}</div>`).join(''):'<i>This computer has no network address yet. Connect it to a network first.</i>';
  sec('How to connect',`<p class='msg' style='margin:0 0 6px'>On the other device, open this address in a web browser:</p>${addr}
<p class='msg' style='margin:8px 0 4px'>Pairing code (asked once in each browser):</p><div style='font:700 28px/1.2 monospace;letter-spacing:4px'>${s.code||'—'}</div>
<p class='msg' style='color:#6b7280;font-size:12px;margin-top:8px'>The browser warns that the connection's certificate is not known: it is this computer's own. To be sure it is this computer, compare the certificate's SHA-256 fingerprint in the browser with this one:<br><span style='font:11px monospace;word-break:break-all'>${s.fingerprint||'(made when remote access first starts)'}</span></p>
<p class='msg' style='color:#6b7280;font-size:12px'>Only devices on the local network can connect (the firewall allows private addresses only). ${s.devices} browser${s.devices==1?'':'s'} remembered; turning remote access off forgets them.</p>`);
  const ss=sec('Connected now',`<div class='rmss'></div>`).querySelector('.rmss');
  if(s.session){ss.innerHTML=`<span></span> <button class='rme'>End the session</button>`;ss.firstChild.textContent=`${s.session.user} from ${s.session.from}, for ${rmFmtSince(s.session.since)}.`;ss.querySelector('.rme').onclick=async()=>{try{await rdPost('remote/end')}catch(e){}setTimeout(draw,1200)}}
  else ss.innerHTML="<i style='color:#6b7280'>Nobody.</i>";
  const ul=sec('Unattended access',`<p class='msg' style='margin:0 0 6px'>These accounts can connect <b>without</b> anyone at the computer allowing it, so the computer can be reached when nobody is there. Every connection is in System health.</p><div class='rmu'></div>`).querySelector('.rmu');
  ACCTS.filter(a=>a.r!='guest').forEach(a=>{const r=document.createElement('label');r.className='ck';r.style.display='block';r.innerHTML=`<input type='checkbox'${adm?'':' disabled'}> <span></span>`;r.querySelector('span').textContent=a.n+(a.r=='admin'?' (administrator)':'');const x=r.querySelector('input');x.checked=!!a.ru;
   x.onchange=async()=>{const on=x.checked;x.checked=!on;if(!await elevate((on?'Let ':'Stop ')+a.n+(on?' connect remotely without anyone allowing it':' connecting remotely without anyone allowing it')))return;a.ru=on;saveUser();rmSync();hlNote&&hlNote('remote',`unattended access turned ${on?'on':'off'} for an account`);x.checked=on};ul.appendChild(r)})};
 cb.onchange=async()=>{const on=cb.checked;cb.checked=!on;
  if(!await elevate(on?'Turn on remote access: devices on the local network can then ask to see and control this computer.':'Turn off remote access'))return;
  try{if(on)await rdPost('remote/accounts',rmAccounts());await rdPost('remote?on='+(on?1:0));logEvt('sys','Remote access '+(on?'on':'off'));setTimeout(rmPoll,800)}catch(e){notify('Remote access',e.message||'It could not be changed.',{kind:'error'})}
  draw()};
 draw()}
