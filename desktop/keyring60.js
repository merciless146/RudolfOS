/* ===== 1.6.0 (docs/plan-1.6.0.md Part 14): Settings › Privacy › Saved passwords for apps =====
   The signed-in account's keyring (the Secret Service: the Claude app, VS Code, Electron and Chromium-based apps, mail
   and chat apps keep their sign-ins in it) and the secrets its Flatpak apps were given (the Secret portal). rudolfd
   reads them as the account's own user (GET keyring/list): only names, never a secret. Remove (POST keyring/forget)
   signs that app out of what it kept; Remove all does each. Drawn as text (never HTML from an app's label). */
window.__keyring60=1;
const KR_SCHEMA={'chrome_libsecret_os_crypt_password_v2':'Chromium-based app (its saved logins)','chrome_libsecret_os_crypt_password_v1':'Chromium-based app (its saved logins)',
 'org.freedesktop.Secret.Generic':'An app','org.gnome.keyring.Note':'A note','org.gnome.keyring.NetworkPassword':'A network password'};
function krWho(it){if(it.kind=='flatpak')return it.app+' (Flatpak app)';if(it.app)return it.app;return KR_SCHEMA[it.schema]||(it.schema?it.schema:'An app')}
async function krSection(box,said){
 box.innerHTML=`<h4 class='sh4'>Saved passwords for apps</h4><p class='msg'>Linux apps keep their sign-ins and tokens in this account's keyring, unlocked when you sign in. Only this account's apps can read it. Removing one signs that app out of it.</p>
<table class='tmt pkt'><thead><tr><th>Saved by</th><th>What</th><th></th></tr></thead><tbody></tbody></table><div class='rmrow' style='justify-content:flex-start'><button class='pka'>Remove all</button></div><p class='msg pkm'></p>`;
 const tb=box.querySelector('tbody'),m=box.querySelector('.pkm'),all=box.querySelector('.pka');m.textContent=said||'';
 const row=t=>{tb.innerHTML='';const r=tb.insertRow(),d=r.insertCell();d.colSpan=3;d.style.opacity='.6';d.textContent=t};
 let L=null;try{L=await rd('keyring/list')}catch(e){row('The saved passwords could not be read: '+e.message);all.disabled=true;return}
 if(!L.signedIn){row('Sign in to an account to see its saved passwords.');all.disabled=true;return}
 const items=L.items||[];all.disabled=!items.length;
 if(!items.length){row(L.keyring?'No app has saved a password yet.':'The keyring is not running now (it starts when you sign in).');return}
 tb.innerHTML='';items.forEach(it=>{const r=tb.insertRow();r.dataset.id=it.id;r.insertCell().textContent=krWho(it);r.insertCell().textContent=it.label||'(no name)';
  const b=document.createElement('button');b.textContent='Remove';b.className='pkr';b.title='Remove it: the app is signed out of what it kept';
  b.onclick=async()=>{if(!await docAsk('Remove "'+(it.label||krWho(it))+'"? '+krWho(it)+' will be signed out of what it kept here.','Remove','Cancel'))return;
   let t='Removed.';try{await rdPost('keyring/forget?id='+encodeURIComponent(it.id))}catch(e){t='Not removed: '+e.message}krSection(box,t)};
  r.insertCell().appendChild(b)});
 all.onclick=async()=>{if(!await docAsk('Remove all '+items.length+' saved passwords for apps? Each app will be signed out of what it kept here.','Remove all','Cancel'))return;
  let bad=0;for(const it of items){try{await rdPost('keyring/forget?id='+encodeURIComponent(it.id))}catch(e){bad++}}krSection(box,bad?bad+' could not be removed.':'All removed.')}}
(function(){if(typeof privPanel!='function')return;const P0=privPanel;privPanel=function(c){P0(c);if(typeof RD=='undefined'||!RD)return;
 const box=document.createElement('div');box.className='pkr60';const cam=c.querySelector('.pcam');cam?cam.after(box):c.appendChild(box);krSection(box)}})();
/* ===== 1.6.0 (Part 14, third step): the keyring's own questions. An app creating a keyring or needing one unlocked makes
   gnome-keyring-daemon ask gcr's prompter, which is rudolf-portal here (never a GNOME window): its question arrives with
   the portal's (portal/poll, kind "keyring"), drawn as Settings' prompts are (the administrator's), with who asked and
   the daemon's own words as text; the answer (yes with the password, or no) goes back the same way (portal/answer),
   and the portal hands the password to the daemon through gcr's secret exchange. A question the daemon withdraws (the
   app went away) closes by itself. */
const KR_PROC={claude:'Claude',code:'Visual Studio Code','code-oss':'Visual Studio Code',chrome:'Google Chrome',chromium:'Chromium',brave:'Brave',thunderbird:'Thunderbird',evolution:'Evolution',geary:'Geary',slack:'Slack',discord:'Discord',element:'Element',signal:'Signal'};
function krAsker(r){if(r.app)return lxAppName(r.app);const p=String(r.proc||'');return KR_PROC[p.toLowerCase()]||p||'An app'}
function krPrompt(id,r){return new Promise(done=>{const pw=r.type=='password',nw=pw&&!!r.newpass,who=krAsker(r);
 const o=document.createElement('div');o.className='rmodal krp60';
 o.innerHTML=`<div class='rmbox' role='alertdialog' aria-label='Keyring'><h3>🔑 <span class='krt'></span></h3><p class='rmr'><b class='kra'></b>: <span class='krm'></span></p><p class='msg krd'></p>${pw?`<div class='r2'><label>${nw?'New password':'Password'}</label><input class='kp1'>${nw?`<label>Confirm</label><input class='kp2'>`:''}</div>`:''}${r.choice?`<label class='ck krc'><input type='checkbox' class='kc'> <span></span></label>`:''}<div class='rmmsg'></div><div class='rmrow'><button class='en'></button><button class='ey pri'></button></div></div>`;
 const g=s=>o.querySelector(s);g('.krt').textContent=r.title||(nw?'New keyring password':pw?'Unlock the keyring':'Keyring');g('.kra').textContent=who;g('.krm').textContent=r.message||(pw?'A password is needed.':'Continue?');
 g('.krd').textContent=r.description||'';g('.krd').hidden=!r.description;g('.rmmsg').textContent=r.warning||'';g('.en').textContent=r.cancel||'Cancel';g('.ey').textContent=r.cont||'Continue';
 if(r.choice){g('.krc span').textContent=r.choice;g('.kc').checked=!!r.chosen}
 const P1=pw?maskInput(g('.kp1')):null,P2=nw?maskInput(g('.kp2')):null;document.body.appendChild(o);if(typeof promptFront=='function')promptFront(o);
 setTimeout(()=>(pw?g('.kp1'):g('.ey')).focus(),30);let over=false;
 const end=a=>{if(over)return;over=true;clearInterval(iv);o.remove();if(P1)P1.clear();if(P2)P2.clear();done(a)};
 const yes=()=>{if(nw&&P1.get()!==P2.get()){g('.rmmsg').textContent='The passwords do not match.';P2.clear();g('.kp2').focus();sfx('error');return}
  const a={response:0};if(pw)a.password=P1.get();if(r.choice)a.chosen=g('.kc').checked;end(a)};
 g('.en').onclick=()=>end({response:1});g('.ey').onclick=yes;
 o.addEventListener('keydown',e=>{e.stopPropagation();if(e.key=='Escape')end({response:1});else if(e.key=='Enter'&&e.target.tagName!='BUTTON')yes()});
 /* (withdrawn: its question no longer waits) */
 const iv=setInterval(async()=>{let j;try{j=await lxApi('portal/poll')}catch(e){return}if(!(j.requests||[]).some(q=>q.id==id))end(null)},1000)})}
(function(){if(typeof lxPortal!='function')return;const L0=lxPortal;
 lxPortal=async function(id,r){if(!r||r.kind!="keyring")return L0(id,r);const a=await krPrompt(id,r);if(a)await lxAnswer(id,a);setTimeout(lxWinPoll,300)}})();
