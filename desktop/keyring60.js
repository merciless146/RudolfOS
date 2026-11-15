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
