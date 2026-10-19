/* ===== 1.2.2: Secure Boot =====
   The state of Secure Boot (rudolfd: GET secureboot/status) for Settings > Security Settings, Task Manager and
   the system report, and "Prepare Secure Boot": rudolfd asks shim's MokManager to enroll the Rudolf OS key at
   the next start, protected by a one-time password shown here. */
window.__sb28=1;
let SBST=null;
async function sbStatus(force){if(!RD)return null;if(SBST&&!force)return SBST;try{SBST=await rd('secureboot/status')}catch(e){SBST=null}return SBST}
const sbLock=s=>{const m=/\[(\w+)\]/.exec(s&&s.lockdown||'');return m?m[1]:''};
function sbSummary(s){if(!s)return'not reported';if(!s.efi)return'not available (started in BIOS mode)';
 const l=sbLock(s);return(s.on?'on':'off')+(s.on&&l&&l!='none'?', kernel locked down ('+l+')':'')+(s.enrolled?', Rudolf OS key enrolled':'')+(s.pending?', key waiting to be enrolled':'')+(s.unverified?', some layers unchecked':'')}
function sbRestart(){try{fsFlush()}catch(e){}rdPost('power?action=reboot').catch(()=>{})}
async function sbPrepare(W,redraw){
 if(!await elevate('Prepare Secure Boot: at the next start the computer asks to enroll the Rudolf OS key.'))return;
 let r;try{r=await rdPost('secureboot/prepare')}catch(e){sdlg(W,'Secure Boot',e.message||'The request could not be stored.',[['OK']]);return}
 SBST=null;redraw&&redraw();
 const o=document.createElement('div');o.className='rmodal';
 o.innerHTML=`<div class='rmbox' role='alertdialog' aria-label='Prepare Secure Boot' style='max-width:520px'><h3>🔐 Secure Boot: one more step at the next start</h3>
<p>When the computer starts again, a blue screen appears (<b>Perform MOK management</b>):</p>
<ol style='margin:6px 0 10px 18px;padding:0'><li>Press any key within 10 seconds.</li><li>Choose <b>Enroll MOK</b>, then <b>Continue</b>, then <b>Yes</b>.</li><li>Type this one-time password and press Enter:<div class='sbpw' style='font:600 26px monospace;letter-spacing:4px;margin:6px 0'></div></li><li>Choose <b>Reboot</b>.</li></ol>
<p>Then turn <b>Secure Boot</b> on in the computer's firmware settings (often F2, F10, Del or Esc while it starts). Rudolf OS starts as before, now with Secure Boot.</p>
<p class='msg'>Write the password down: it is shown only now. If the blue screen does not appear in time, prepare again.</p>
<div class='rmrow'><button class='c'>Later</button><button class='pri rs'>Restart now</button></div></div>`;
 o.querySelector('.sbpw').textContent=r.password;o.querySelector('.c').onclick=()=>o.remove();o.querySelector('.rs').onclick=()=>{o.remove();sbRestart()};
 document.body.appendChild(o);o.querySelector('.rs').focus()}
async function sbSection(c,W){
 const box=document.createElement('div');box.className='sb28';c.appendChild(box);
 const draw=async force=>{const s=await sbStatus(force);box.innerHTML="<h4 class='sh4'>Secure Boot</h4><p class='msg sbt'></p><div class='rmrow' style='justify-content:flex-start'></div>";
  const t=box.querySelector('.sbt'),row=box.querySelector('.rmrow'),adm=isAdmin();
  const btn=(l,f,pri)=>{const b=document.createElement('button');b.textContent=l;if(pri)b.className='pri';b.disabled=!adm;b.onclick=f;row.appendChild(b)};
  if(!RD){t.textContent='Secure Boot applies to the installed Rudolf OS (bootable disc or USB drive).';return}
  if(!s){t.textContent='The state of Secure Boot could not be read.';return}
  if(!s.efi){t.textContent='This computer started in BIOS (legacy) mode, which has no Secure Boot. Start it in UEFI mode to use Secure Boot.';return}
  if(s.on)t.textContent='On. The computer started only software signed for Rudolf OS, the kernel is locked down'+(sbLock(s)&&sbLock(s)!='none'?'':' (lockdown is not reported)')+(s.unverified?'. Some system layers could not be checked: '+s.unverified.replace(/\n/g,'; '):', and every system layer is checked as it is read.');
  else if(s.enrolled)t.textContent='Off, but ready: the Rudolf OS key is enrolled. Turn Secure Boot on in the computer\'s firmware settings (often F2, F10, Del or Esc while it starts).';
  else if(s.pending){t.textContent='Off. The Rudolf OS key will be offered for enrolment at the next start (the blue MokManager screen); you need the one-time password that was shown.';btn('Restart now',sbRestart,1);btn('Cancel the request',async()=>{if(!await elevate('Cancel the Secure Boot request.'))return;try{await rdPost('secureboot/cancel')}catch(e){}draw(true)})}
  else{t.textContent='Off. With Secure Boot on, the computer starts only signed software, so nothing can be slipped in before Rudolf OS starts. Rudolf OS needs its key enrolled once: prepare it here, then confirm it at the next start.';btn('Prepare Secure Boot…',()=>sbPrepare(W,()=>draw(true)),1)}
  if(!adm&&row.children.length){const n=document.createElement('span');n.className='msg';n.textContent=' An administrator can change this.';row.appendChild(n)}};
 draw()}
if(typeof secPanelS=='function'){const _sp=secPanelS;secPanelS=function(c,W){const r=_sp.apply(this,arguments);try{sbSection(c,W)}catch(e){}return r}}
