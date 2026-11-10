/* ===== Rudolf OS 1.5.0: security (docs/plan-1.5.0.md) =====
   1. The firewall for incoming connections (rudolfd: GET/POST firewall; /usr/lib/rudolf/firewall). The desktop's URL
      rules for Oracle tabs, which were called "Firewall", are "Website blocking" from now on (they did not change).
   2. System health: the health log kept on the computer (rudolfd: GET health), in Settings and in the system report;
      the desktop adds its own events (POST health): ready after start-up, sign-in failures. */
window.__sec50=1;
const FW_TXT='Blocks connections that other computers start to this one. Programs on this computer can still reach the internet, and replies to them come in as usual.';
async function fwStatus(){if(!RD)return null;try{return window.FW50=await rd('firewall')}catch(e){return null}}
if(RD)setTimeout(()=>fwStatus(),6000);
function fwSection(c,W){
 /* the old URL rules: their real name */
 const stat=c.querySelector('.stat>div:first-child>b');if(stat&&stat.textContent.trim()=='Firewall')stat.textContent='Website blocking';
 const h=[...c.querySelectorAll('h4')].find(x=>x.textContent.trim()=='Firewall');if(h)h.textContent='Website blocking';
 const ck=c.querySelector('#fwo');if(ck&&ck.parentElement){const t=[...ck.parentElement.childNodes].find(n=>n.nodeType==3&&/firewall/i.test(n.textContent));if(t)t.textContent=' Turn on website blocking (in Oracle)';
  const l=ck.closest('.r2')&&ck.closest('.r2').querySelector('label');if(l&&l.textContent.trim()=='Firewall')l.textContent='Website blocking'}
 if(!RD)return;
 const box=document.createElement('div');box.className='fw50';
 box.innerHTML=`<h4 class='sh4'>Firewall</h4><p class='msg' style='margin:2px 0 8px'>${FW_TXT}</p><div class='r2'><label>Firewall</label><div><label class='ck'><input type='checkbox' class='fwn'${isAdmin()?'':' disabled'}> Block incoming connections</label> <span class='fws' style='color:#6b7280;font-size:12px'></span></div></div>`;
 const at=h||c.querySelector('h4');if(at)c.insertBefore(box,at);else c.appendChild(box);
 const cb=box.querySelector('.fwn'),st=box.querySelector('.fws');
 const draw=async()=>{const s=await fwStatus();if(!s){st.textContent='not available';cb.disabled=true;return}
  cb.checked=s.on;st.textContent=s.on?`On · ${s.blocked} incoming connection${s.blocked==1?'':'s'} blocked since start-up${s.remote?' · remote access allowed from the local network':''}`:'Off ⚠ other computers can reach programs that listen for connections'};
 cb.onchange=async()=>{const on=cb.checked;cb.checked=!on;
  if(!await elevate(on?'Turn on the firewall':'Turn off the firewall: other computers on the network can then reach programs that listen for connections.'))return;
  try{await rdPost('firewall?on='+(on?1:0));notify('Firewall',on?'The firewall is on.':'The firewall is off.',{kind:on?'info':'error'});logEvt('fw','Firewall (incoming connections) '+(on?'on':'off'))}catch(e){notify('Firewall',e.message||'It could not be changed.',{kind:'error'})}
  draw()};
 draw()}
if(typeof secPanelS=='function'){const _sp50=secPanelS;secPanelS=function(c,W){const r=_sp50.apply(this,arguments);try{fwSection(c,W)}catch(e){console.warn('fw50',e)}return r}}

/* ---------- System health ---------- */
const HL_KIND={start:['🔌','Start-up'],recover:['♻️','Kept in Recovered'],ready:['✅','Ready'],fallback:['↩️','Update fallback'],memory:['🧠','Memory'],crash:['💥','Crash'],browser:['🔄','Desktop restart'],signin:['🔑','Sign-in'],disk:['💾','Storage'],heat:['🌡️','Heat'],firewall:['🛡️','Firewall'],remote:['🖥️','Remote access'],vault:['🔐','Encryption'],update:['⬆️','Update'],account:['👤','Accounts'],bluetooth:['🔵','Bluetooth'],hardware:['🧩','Hardware'],library:['📚','Rudolf OS Library']};
const hlWhen=t=>{const d=new Date(t*1000);return sysDateStr(d)+' '+d.toLocaleTimeString(window.__a11y?LOC():[],{hour:'numeric',minute:'2-digit',timeZone:S.tz})};
async function hlGet(n){if(!RD)return null;try{const h=await rd('health?n='+(n||300));return h&&Array.isArray(h.events)?h:null}catch(e){return null}}
function hlNote(k,m){if(!RD)return;rdPost('health',JSON.stringify({k,m:String(m).slice(0,380)})).catch(()=>{})}
/* summary for the last 7 days */
function hlSummary(ev,now){const wk=ev.filter(e=>now-e.t<7*86400),cnt=k=>wk.filter(e=>e.k==k).length;
 const rd1=ev.find(e=>e.k=='ready'),sec=rd1&&/(\d+(?:\.\d+)?) s/.exec(rd1.m);
 return[['Last start-up',rd1?(sec?sec[1]+' s to the desktop':'')+' · '+hlWhen(rd1.t):'—'],['Start-ups this week',String(cnt('start'))],
  ['Crashes this week',String(cnt('crash')+cnt('browser'))],['Pages closed to free memory',String(cnt('memory'))],['Update fallbacks',String(cnt('fallback'))],['Failed sign-ins this week',String(cnt('signin'))]]}
function healthPanel(c,W){
 c.innerHTML=`<h3>System health</h3><p class='msg' style='margin:0 0 10px'>What happened on this computer: start-ups, crashes, memory, updates, sign-in failures, the firewall and remote access. It is kept on this computer only and is never sent anywhere. Power and battery is here too, below the summary. <b>Report a problem</b> includes the latest events, after you have read the report.</p>
<div class='hls r2'></div><h4 class='sh4 hlev'>Events</h4><div class='rmrow' style='justify-content:flex-start;gap:6px;margin-bottom:6px'><select class='hlk'><option value=''>All events</option></select><button class='hlr'>Refresh</button><button class='hlp'>Report a problem…</button><button class='hlw' hidden>⚡ What's wrong with this computer?</button></div><div class='hll' style='font-size:13px'></div>`;
 const sel=c.querySelector('.hlk');for(const k in HL_KIND){const o=document.createElement('option');o.value=k;o.textContent=HL_KIND[k][0]+' '+HL_KIND[k][1];sel.appendChild(o)}
 const L=c.querySelector('.hll'),Sm=c.querySelector('.hls');
 c.querySelector('.hlp').onclick=()=>{if(typeof sysReportDialog=='function')sysReportDialog()};
 /* 1.5.7: Blitzen reads the logs and explains (blitzdiag57.js) */
 if(typeof bzCan=='function'&&bzCan()){const w=c.querySelector('.hlw');w.hidden=false;w.onclick=()=>bzWhy("What's wrong with this computer?",{all:1})}
 const draw=async()=>{L.textContent='Loading…';const h=await hlGet(500);
  if(!h){L.innerHTML='<i style="color:#6b7280">The health log is kept by the installed Rudolf OS (bootable disc or USB).</i>';Sm.innerHTML='';return}
  Sm.innerHTML='';hlSummary(h.events,h.now).forEach(([a,b])=>{const l=document.createElement('label');l.textContent=a;const v=document.createElement('div');v.textContent=b;Sm.append(l,v)});
  const ev=h.events.filter(e=>!sel.value||e.k==sel.value);L.innerHTML='';
  if(!ev.length){L.innerHTML='<i style="color:#6b7280">Nothing recorded.</i>';return}
  ev.slice(0,300).forEach(e=>{const r=document.createElement('div');r.className='acr';r.style.cssText='display:flex;gap:8px;align-items:baseline;padding:3px 0;border-bottom:1px solid rgba(128,128,128,.15)';
   const k=HL_KIND[e.k]||['•',e.k];r.innerHTML=`<span style='min-width:1.4em'></span><span style='min-width:150px;color:#6b7280;font-size:12px'></span><span></span>`;
   r.children[0].textContent=k[0];r.children[0].title=k[1];r.children[1].textContent=hlWhen(e.t);r.children[2].textContent=e.m;
   if(typeof bzCan=='function'&&bzCan()&&(['crash','browser','memory','fallback','signin'].includes(e.k)||bzIsError(e.m)))bzWhyBtn(r,{t:k[1]+': '+e.m,b:'',k:'error'},{when:e.t*1000});L.appendChild(r)})};
 sel.onchange=draw;c.querySelector('.hlr').onclick=draw;draw();
 /* 1.5.6: Power and battery (power31.js), moved here from System Settings */
 if(typeof pwrPanel=='function')try{pwrPanel(c,W)}catch(e){console.warn(e)}}
/* lines for the system report (account names and addresses are scrubbed by srScrub like the rest) */
async function hlReportLines(n){const h=await hlGet(n||50);if(!h||!h.events)return[];return h.events.map(e=>`${new Date(e.t*1000).toISOString().replace(/\.\d+Z$/,'Z')} ${(HL_KIND[e.k]||['',e.k])[1]}: ${e.m}`)}
/* the desktop is up: how long the start took (uptime when the desktop page first came up after start-up) */
(function(){if(!RD)return;setTimeout(async()=>{try{if(sessionStorage.getItem('hl50ready'))return;sessionStorage.setItem('hl50ready','1')}catch(e){}
 const h=await hlGet(5);if(!h||!Array.isArray(h.events))return;const last=h.events.find(e=>e.k=='start'),rdy=h.events.find(e=>e.k=='ready');
 if(last&&(!rdy||rdy.t<last.t)&&h.uptime<900)hlNote('ready',`the desktop was ready ${h.uptime} s after start-up`)},4000)})();
