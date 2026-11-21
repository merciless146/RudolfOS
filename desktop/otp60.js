/* ===== Rudolf OS 1.6.0: two-step sign-in with authenticator-app codes (plan-1.6.1 Part 17, moved to 1.6.0; the owner,
   2026-11-19: "Codes now, keys after") =====
   Six-digit codes every 30 seconds (RFC 6238, HMAC-SHA-1) from an app such as Google or Microsoft Authenticator, Aegis or
   2FAS. User Settings › Sign-in options › Authenticator app: set up with the QR code (1.6.1, qr61.js) or the setup key, confirmed with
   a code. The secret is kept encrypted with the account's password key (a.otp, AES-256-GCM), so it guards the sign-in, not
   the files: Settings says so. Signing in, unlocking and the native sign-in (rudolfd, desktop/login) ask for a code after
   the password; one step either side of now is accepted, a code used once is not taken again, and rudolfd counts wrong
   codes and passwords per account (a wait of 30 s after 5, doubling up to 15 minutes: desktop/second). Security Settings ›
   Two-step sign-in: an administrator may require it for administrators or for every account; an account without it is
   asked to set it up before its desktop opens. Security keys that unlock the files follow in their own beta. ===== */
const OTP_B32='ABCDEFGHIJKLMNOPQRSTUVWXYZ234567';
function otpB32(u){let b=0,v=0,o='';for(const x of u){v=v<<8|x;b+=8;while(b>=5){o+=OTP_B32[(v>>>(b-5))&31];b-=5}}if(b>0)o+=OTP_B32[(v<<(5-b))&31];return o}
async function otpCode(sec,step){const m=new Uint8Array(8);let s=step;for(let i=7;i>=0;i--){m[i]=s&255;s=Math.floor(s/256)}
 const k=await crypto.subtle.importKey('raw',sec,{name:'HMAC',hash:'SHA-1'},false,['sign']);const h=new Uint8Array(await crypto.subtle.sign('HMAC',k,m)),o=h[19]&15;
 return String((((h[o]&127)<<24)|(h[o+1]<<16)|(h[o+2]<<8)|h[o+3])%1e6).padStart(6,'0')}
/* the step a code belongs to (now and one either side), or -1 */
async function otpMatch(sec,code,now){code=String(code||'').replace(/\s/g,'');if(!/^\d{6}$/.test(code))return -1;const t=Math.floor((now||Date.now())/30000);
 for(const s of [t,t-1,t+1])if(await otpCode(sec,s)===code)return s;return -1}
/* rudolfd keeps each account's last step used and its wrong attempts (the same as the native sign-in's) */
async function otpServer(id,step){if(typeof RD=='undefined'||!RD)return{ok:true};try{return await rd('desktop/second',{method:'POST',body:JSON.stringify({id,step})})}catch(e){return{ok:false,error:e.message}}}
async function otpSecret(a,pw){return acDec(await deriveKey(pw,a.ks),a.otp)}
/* a code asked for (a small dialog): the code, or null */
function otpAsk(who,msg){return new Promise(res=>{const o=document.createElement('div');o.className='rmodal otpdlg';
 o.innerHTML="<div class='rmbox' role='dialog' aria-modal='true' aria-labelledby='otpt' style='max-width:380px'><h3 id='otpt'>Two-step sign-in</h3><p class='msg otpw'></p><input class='otpc' inputmode='numeric' autocomplete='one-time-code' maxlength='7' aria-label='Code from your authenticator app' placeholder='Code from your authenticator app' style='width:100%;font-size:20px;letter-spacing:4px;text-align:center'><p class='msg otpm' role='alert'></p><div class='rmrow'><button class='n'>Cancel</button><button class='pri y'>Continue</button></div></div>";
 o.querySelector('.otpw').textContent=`${who}: enter the 6-digit code your authenticator app shows for Rudolf OS.`;o.querySelector('.otpm').textContent=msg||'';
 const done=v=>{o.remove();res(v)},c=o.querySelector('.otpc');o.querySelector('.n').onclick=()=>done(null);o.querySelector('.y').onclick=()=>done(c.value);
 c.addEventListener('keydown',e=>{e.stopPropagation();if(e.key=='Enter')done(c.value);if(e.key=='Escape')done(null)});document.body.appendChild(o);setTimeout(()=>c.focus(),30)})}
/* the second step for account A after its password PW was right: true when a right, unused code is given */
async function otpCheck(a,pw){if(!a||!a.otp)return true;let sec;try{sec=await otpSecret(a,pw)}catch(e){return false}let msg='';
 for(;;){const c=await otpAsk(a.n,msg);if(c==null)return false;const s=await otpMatch(sec,c);
  const r=await otpServer(a.id||a.n,s);if(s>=0&&r.ok)return true;
  try{sfx('error')}catch(e){}msg=r&&r.error&&/Please wait/i.test(r.error)?r.error:s>=0?'That code was used already: wait for the next one.':'That code is not right. Check the time on your phone, and try the newest code.';
  if(r&&r.error&&/Please wait/i.test(r.error))return false}}
/* ---- User Settings › Sign-in options › Authenticator app ---- */
function otpRow(d){const box=document.createElement('div');box.className='r2 otprow';box.innerHTML="<label>Authenticator app</label><div><span class='otps'></span> <button class='otpa'>Set up…</button><button class='otpr'>Turn off</button><div class='msg'>Codes from an app on your phone, asked for after your password when you sign in or unlock. They guard the sign-in; your files stay protected by your password (security keys that also protect the files are coming).</div></div>";
 d.appendChild(box);const q=s=>box.querySelector(s),draw=()=>{q('.otps').textContent=USER.otp?'On.':'Off.';q('.otpr').style.display=USER.otp?'':'none';q('.otpa').textContent=USER.otp?'Set up again…':'Set up…'};draw();
 q('.otpa').onclick=()=>otpSetup(USER).then(draw);
 q('.otpr').onclick=async()=>{if(otpRequired(USER))return notify('Two-step sign-in','An administrator requires two-step sign-in for this account, so it cannot be turned off.',{kind:'error'});
  if(!await elevate('Turn off authenticator codes for '+USER.n))return;delete USER.otp;saveUser();logEvt&&logEvt('sys',`${USER.n} turned off authenticator codes`);draw()}}
async function otpSetup(a,must){return new Promise(res=>{const sec=crypto.getRandomValues(new Uint8Array(20)),key=otpB32(sec),grp=key.match(/.{1,4}/g).join(' ');
 const o=document.createElement('div');o.className='rmodal otpset';
 o.innerHTML="<div class='rmbox' role='dialog' aria-modal='true' aria-labelledby='otst' style='max-width:520px'><h3 id='otst'>Set up an authenticator app</h3><p class='msg otpwhy'></p><ol class='msg'><li>In the app, scan the QR code with your phone's camera, or add an account with a <b>setup key</b> (sometimes \"Enter a setup key\" or \"Manual entry\"): the name <b class='otpn'></b>, the key below, time-based.</li><li>Type the 6-digit code the app shows, and your password.</li></ol><div class='otpkr' style='display:flex;gap:14px;align-items:center;flex-wrap:wrap'><div class='otpqr' aria-label='QR code of the setup link' style='flex:none;padding:6px;background:#fff;border-radius:6px'></div><div class='otpk' style='flex:1;min-width:180px;font:600 18px monospace;letter-spacing:2px;user-select:all;padding:8px;background:var(--bg2,#0002);border-radius:6px'></div></div><div class='msg otpu' style='word-break:break-all;font-size:11px'></div><input class='otpc' inputmode='numeric' maxlength='7' placeholder='Code from the app' aria-label='Code from the app'> <input class='otpp' type='password' placeholder='Your password' aria-label='Your password'><p class='msg otpm' role='alert'></p><div class='rmrow'><button class='n'></button><button class='pri y'>Turn on</button></div></div>";
 const q=s=>o.querySelector(s);q('.otpwhy').textContent=must?'An administrator requires two-step sign-in for this account. Set it up to continue.':'';q('.otpn').textContent='Rudolf OS ('+a.n+')';q('.otpk').textContent=grp;
 const link=otpLink(a.n,key);q('.otpu').textContent='Or a link the app may open: '+link;otpQr(q('.otpqr'),link);q('.n').textContent=must?'Sign out':'Cancel';
 const done=v=>{o.remove();res(v)};q('.n').onclick=()=>{done(false);if(must)try{A.logoff()}catch(e){}};
 q('.y').onclick=async()=>{const pw=q('.otpp').value;if(!(await pwCheck(a,pw))){q('.otpm').textContent='Wrong password.';return}
  const s=await otpMatch(sec,q('.otpc').value);if(s<0){q('.otpm').textContent='That code is not right: check that the app has the key above, and that your phone\'s time is set automatically.';return}
  await otpServer(a.id||a.n,s);a.otp=await acEnc(await deriveKey(pw,a.ks),sec);saveUser();try{logEvt('sys',`${a.n} turned on authenticator codes`)}catch(e){}
  notify('Two-step sign-in','Authenticator codes are on: you will be asked for a code after your password.',{app:'Settings'});done(true)};
 o.addEventListener('keydown',e=>e.stopPropagation());document.body.appendChild(o);setTimeout(()=>q('.otpc').focus(),30)})}
/* the setup link, and its QR code beside the setup key (qr61.js; level M, or the link alone if it is ever too long) */
const otpLink=(n,key)=>'otpauth://totp/'+encodeURIComponent('Rudolf OS:'+n)+'?secret='+key+'&issuer=Rudolf%20OS';
function otpQr(box,link){try{const c=typeof qrMake=='function'&&qrMake(link,'M');if(!c){box.style.display='none';return}const s=qrSvg(c);s.setAttribute('aria-label','QR code of the setup link');s.classList.add('otpqrs');box.appendChild(s)}catch(e){box.style.display='none'}}
/* ---- Security Settings › Two-step sign-in (an administrator requires it) ---- */
const otpRequired=a=>{const m=(typeof SYSC=='object'&&SYSC.mfa)||'off';return a&&a.r!='guest'&&(m=='all'||(m=='admin'&&a.r=='admin'))};
function otpPolicy(c){const box=document.createElement('div');box.className='otppol';box.innerHTML="<h4 class='sh4'>Two-step sign-in</h4><div class='r2'><label>Required</label><select class='otpq'><option value='off'>Not required</option><option value='admin'>Required for administrators</option><option value='all'>Required for every account</option></select></div><div class='msg'>An account without it is asked to set up an authenticator app before its desktop opens. The Guest account cannot use it, so it is turned off while every account must.</div>";
 c.appendChild(box);const s=box.querySelector('.otpq');s.value=SYSC.mfa||'off';s.disabled=!isAdmin();
 s.onchange=async()=>{const v=s.value;if(!await elevate('Change who must use two-step sign-in')){s.value=SYSC.mfa||'off';return}SYSC.mfa=v;if(v=='all')SYSC.guest=false;sysSave();try{logEvt('sys',`${USER.n} set two-step sign-in: ${v}`)}catch(e){}}}
if(typeof secPanelS=='function'){const _sp=secPanelS;secPanelS=function(c,W){const r=_sp.apply(this,arguments);try{otpPolicy(c)}catch(e){}return r}}
if(typeof usrPanelS=='function'){const _up=usrPanelS;usrPanelS=function(c,W){const r=_up.apply(this,arguments);try{if(USER&&USER.r!='guest'){const d=c.querySelector('.pka')&&c.querySelector('.pka').closest('.r2');if(d)otpRow(d.parentElement);else otpRow(c)}}catch(e){}return r}}
/* after signing in: the second step required and not set up: set up first */
(function(){const o=window.__afterLogin;window.__afterLogin=()=>{o&&o();try{if(USER&&otpRequired(USER)&&!USER.otp&&!USER.pk)otpSetup(USER,true)}catch(e){}}})();
