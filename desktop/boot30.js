/* ===================== Boot menu and Windows (1.2.5) =====================
   Settings › Update and recovery › Boot menu: which entry of the UEFI boot menu starts when nobody chooses, and
   how long the menu waits. rudolfd stores both in the firmware (systemd-boot's LoaderEntryDefault and
   LoaderConfigTimeout), so the disc or stick is not changed. "Restart into Windows" asks the firmware to start
   its Windows Boot Manager once (BootNext): Windows on another drive cannot be started from Rudolf OS's own menu. */
const BOOT_TITLES={'rudolf-start.conf':'Start Rudolf OS','rudolf-recovery.conf':'Recovery mode','rudolf-check.conf':'Check this disc or USB drive','rudolf-ram.conf':'Start from memory','rudolf-debug.conf':'Debug start','auto-windows':'Windows Boot Manager'};
const bootTitle=id=>BOOT_TITLES[id]||id.replace(/\.conf$/,'');
/* the entries worth offering as the default (not the firmware-setup or reboot items systemd-boot adds) */
const bootChoices=m=>(m.entries||[]).filter(e=>/^rudolf-.*\.conf$/.test(e)&&e!='rudolf-debug.conf'||e=='auto-windows');
function bootRestart(){try{fsFlush()}catch(e){}rdPost('power?action=reboot').catch(()=>{})}
async function bootWindows(w){
 if(!await elevate('Restart into Windows: the computer starts '+w.title+' once, then this menu again as before.'))return false;
 try{await rdPost('bootmenu/windows?n='+w.n)}catch(e){notify('Restart into Windows',e.message,{kind:'error'});return false}
 notify('Restarting into Windows','The next start goes to '+w.title+'.');setTimeout(bootRestart,1200);return true}
async function bootSection(c){if(!RD)return;const box=document.createElement('div');box.className='boot30';c.appendChild(box);
 const draw=async()=>{let m;try{m=await rd('bootmenu')}catch(e){box.innerHTML='';return}
  box.innerHTML="<h4 class='sh4'>Boot menu</h4><div class='bmb'></div>";const b=box.querySelector('.bmb'),adm=isAdmin();
  if(!m.uefi){b.innerHTML="<p class='msg'>This computer started in BIOS (legacy) mode. To start Windows, choose <b>Boot from hard disk</b> in the Rudolf Boot menu.</p>";return}
  if(!m.loader){b.innerHTML="<p class='msg'>Rudolf OS was not started from its own boot menu, so there is nothing to set here.</p>";return}
  const ch=bootChoices(m),def=m.default||'',to=m.timeout||'';
  b.innerHTML=`<div class='r2'><label>Starts by default</label><select class='bmd'><option value=''>As on the disc (${lxEsc(bootTitle('rudolf-start.conf'))})</option>${ch.map(e=>`<option value='${lxEsc(e)}'>${lxEsc(bootTitle(e))}</option>`).join('')}</select></div>
  <div class='r2'><label>Wait before starting</label><select class='bmt'><option value=''>As on the disc (4 seconds)</option>${[2,3,4,5,6,8,10].map(n=>`<option value='${n}'>${n} seconds</option>`).join('')}</select></div>
  <div class='rmrow' style='justify-content:flex-start'><button class='bms'>Save</button><span class='msg bmm'></span></div>
  <p class='msg'>These choices are kept in this computer's firmware, not on the disc or stick. They apply to every Rudolf OS disc or stick started on this computer. ${m.selected?'This time Rudolf OS started with “'+lxEsc(bootTitle(m.selected))+'”.':''}</p><div class='bmw'></div>`;
  const sd=b.querySelector('.bmd'),st=b.querySelector('.bmt'),sv=b.querySelector('.bms');sd.value=ch.includes(def)?def:'';st.value=/^(2|3|4|5|6|8|10)$/.test(to)?to:'';
  [sd,st,sv].forEach(x=>x.disabled=!adm);
  sv.onclick=async()=>{if(!await elevate('Change the boot menu: what starts by default and how long it waits.'))return;
   try{await rdPost('bootmenu/set?default='+encodeURIComponent(sd.value)+'&timeout='+encodeURIComponent(st.value));b.querySelector('.bmm').textContent=' Saved. It applies from the next start.'}catch(e){b.querySelector('.bmm').textContent=' '+e.message}};
  const wb=b.querySelector('.bmw');
  if(m.windows&&m.windows.length){wb.innerHTML="<h4 class='sh4'>Windows</h4><p class='msg'>Windows is on this computer. Restart into it once; the next start after that comes back to this menu.</p><div class='rmrow' style='justify-content:flex-start'></div>";
   const row=wb.querySelector('.rmrow');m.windows.forEach(w=>{const x=document.createElement('button');x.textContent=m.windows.length>1?'Restart into '+w.title+' ('+w.n+')':'Restart into Windows';x.disabled=!adm;x.onclick=()=>bootWindows(w);row.appendChild(x)})}
  else wb.innerHTML="<p class='msg'>No Windows was found in this computer's start-up list.</p>";
  if(!adm)b.insertAdjacentHTML('beforeend',"<p class='msg'>An administrator can change this.</p>")};
 draw()}
{const _up=updPanel;updPanel=function(c){const r=_up.apply(this,arguments);try{bootSection(c)}catch(e){}return r}}
/* the power menu: "Restart into Windows" when the firmware has a Windows entry */
async function bootWindowsList(){if(!RD)return[];try{const m=await rd('bootmenu');return m.uefi&&m.windows||[]}catch(e){return[]}}
{const _sd=shutdownDlg;shutdownDlg=function(){const r=_sd.apply(this,arguments);const o=[...document.querySelectorAll('.rmodal')].at(-1);
 bootWindowsList().then(l=>{if(!l.length||!o||!o.isConnected)return;const rb=o.querySelector("[data-a='reboot']");if(!rb)return;
  const b=document.createElement('button');b.className='bmwin';b.textContent='⊞ Restart into Windows';b.onclick=async e=>{e.stopPropagation();
   if(!await elevate('Restart into Windows: the computer starts '+l[0].title+' once.'))return;
   try{await rdPost('bootmenu/windows?n='+l[0].n)}catch(x){o.querySelector('.rmmsg').textContent=x.message;return}rb.click()};rb.after(b)});return r}}
