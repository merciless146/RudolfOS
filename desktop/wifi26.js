/* ---------- Wi-Fi (Settings › Network): networks in range, password entry, saved networks ----------
   rudolfd wifi/* runs /usr/lib/rudolf/wifi.sh, which drives wpa_supplicant. */
function wfDecode(s){/* wpa_cli escapes names: \xNN, \\, \", \e, \n, \r, \t */
 const b=[];for(let i=0;i<s.length;i++){const c=s[i];if(c=='\\'&&i+1<s.length){const n=s[++i];if(n=='x'){b.push(parseInt(s.substr(i+1,2),16));i+=2}else b.push({n:10,r:13,t:9,e:27}[n]||n.charCodeAt(0))}else{const e=new TextEncoder().encode(c);e.forEach(x=>b.push(x))}}
 try{return new TextDecoder('utf-8',{fatal:true}).decode(new Uint8Array(b))}catch(e){return String.fromCharCode(...b)}}
function wfBars(dbm){const n=dbm>=-55?4:dbm>=-67?3:dbm>=-75?2:1;return `<span class='wfb' title='${dbm} dBm'>${[1,2,3,4].map(i=>`<i class='${i<=n?'on':''}' style='height:${i*4+2}px'></i>`).join('')}</span>`}
function wfKind(f){if(/EAP/.test(f))return 'eap';if(/OWE/.test(f)&&!/PSK|SAE/.test(f))return 'owe';if(/PSK|SAE/.test(f))return 'psk';if(/WEP/.test(f))return 'wep';return 'open'}
function wfStatus(raw){const o={saved:[]};(raw||'').split('\n').forEach(l=>{const i=l.indexOf('=');if(i<0)return;const k=l.slice(0,i),v=l.slice(i+1);if(k=='saved'){const p=v.split('\t');o.saved.push({id:p[0],ssid:wfDecode(p[1]||''),flags:p[3]||''})}else o[k]=v});if(o.ssid)o.ssid=wfDecode(o.ssid);return o}
function wifiPanel(wf,ifn){
 wf.innerHTML=`<div class='wfcur'></div><div class='wfhd'><b>Networks in range</b><button class='wfre'>↻ Refresh</button></div><div class='wfl'><div class='msg'>Looking for networks…</div></div>
 <div class='rmrow' style='justify-content:flex-start'><button class='wfhid'>Join a hidden network…</button></div>
 <h4 class='sh4'>Saved networks</h4><div class='wfsv'></div>
 <details class='wfadv'><summary>Country</summary><div class='msg'>Which channels and transmit power your Wi-Fi may use depend on the country. Set yours if some networks do not show up.</div><div class='r2'><label>Country code</label><input class='wfcc' maxlength='2' placeholder='e.g. US, GB, DE' style='width:80px;text-transform:uppercase'><button class='wfccs'>Set</button></div></details>`;
 const q=s=>wf.querySelector(s);const qs=ifn?'?if='+encodeURIComponent(ifn):'';let st={saved:[]},open='',busy=false;
 const savedHas=n=>st.saved.some(x=>x.ssid==n);
 const showCur=()=>{const c=q('.wfcur');const on=st.wpa_state=='COMPLETED';const ing=/ASSOC|AUTH|HANDSHAKE|SCANNING/.test(st.wpa_state||'')&&busy;
  c.className='wfcur '+(on?'on':'');
  c.innerHTML=on?`<div class='wfci'>${wfBars(+st.RSSI||-70)}<div style='flex:1'><b></b><div class='msg'>${st.ip_address?'Connected · '+esc2(st.ip_address):'Connected · getting an address…'}${st.LINKSPEED?' · '+esc2(st.LINKSPEED)+' Mbit/s':''}</div></div><button class='wfdis'>Disconnect</button></div>`
   :`<div class='wfci'><span style='font-size:22px'>📶</span><div style='flex:1'><b>${ing?'Connecting…':'Not connected'}</b><div class='msg'>${st.saved.length?'Saved networks are joined automatically when they are in range.':'Choose a network below.'}</div></div>${st.saved.length?"<button class='wfrc'>Reconnect</button>":''}</div>`;
  if(on)c.querySelector('b').textContent=st.ssid||'';
  const d=c.querySelector('.wfdis');if(d)d.onclick=()=>rdPost('wifi/disconnect'+qs).then(refresh).catch(e=>notify('Wi-Fi',e.message,{kind:'error'}));
  const r=c.querySelector('.wfrc');if(r)r.onclick=()=>rdPost('wifi/reconnect'+qs).then(()=>setTimeout(refresh,3000)).catch(e=>notify('Wi-Fi',e.message,{kind:'error'}));
  const sv=q('.wfsv');sv.innerHTML=st.saved.length?'':'<div class="msg">No saved networks yet.</div>';
  st.saved.forEach(x=>{const r=document.createElement('div');r.className='acr wfrow';r.innerHTML=`<span style='flex:1'></span><label class='ck wfmt' title='Ask before large downloads on this network (a phone hotspot or a capped plan)'><input type='checkbox'> Metered</label><button>Forget</button>`;r.firstChild.textContent=x.ssid+(/CURRENT/.test(x.flags)?'  ✓ connected':'');
   {const mc=r.querySelector('.wfmt input');mc.checked=typeof appMetered=='function'&&appMetered().includes(x.ssid);mc.onchange=()=>appSetMetered(x.ssid,mc.checked)}
   r.lastChild.onclick=e=>{const b=e.target;if(!b.dataset.sure){b.dataset.sure=1;b.textContent='Tap again to forget';b.classList.add('pri');setTimeout(()=>{if(b.isConnected){delete b.dataset.sure;b.textContent='Forget';b.classList.remove('pri')}},4000);return}rdPost('wifi/forget'+qs,x.ssid).then(m=>{notify('Wi-Fi',m.message);refresh()}).catch(e=>notify('Wi-Fi',e.message,{kind:'error'}))};sv.appendChild(r)});
  if(st.country&&!q('.wfcc').value)q('.wfcc').value=st.country};
 const refresh=async()=>{try{st=wfStatus((await rd('wifi/status'+qs)).raw);showCur()}catch(e){}};
 const connect=async(ssid,pw,kind,hidden)=>{busy=true;open='';showCur();q('.wfl').querySelectorAll('button').forEach(b=>b.disabled=true);
  const sec=kind=='saved'?'saved':(hidden?'hidden-':'')+(kind=='psk'?'psk':kind=='owe'?'owe':'open');
  try{await rdPost('wifi/connect'+qs,ssid+'\n'+(pw||'')+'\n'+sec);notify('Wi-Fi','Connecting to '+ssid+'…',{quiet:true});
   let j;for(let i=0;i<100;i++){await new Promise(r=>setTimeout(r,1000));j=await rd('job');if(!j.running&&j.name=='wifi')break;if(i%3==2)refresh()}
   const msg=(j.log||'').trim().split('\n').pop()||'';if(j.status==0)notify('Wi-Fi',msg||('Connected to '+ssid));else notify('Wi-Fi',msg||'Could not connect',{kind:'error'})}
  catch(e){notify('Wi-Fi',e.message,{kind:'error'})}busy=false;await refresh();scan(true)};
 const pwForm=(row,ssid,kind,hidden)=>{const f=document.createElement('div');f.className='wfpw';f.innerHTML=`${hidden?"<input class='wfn' placeholder='Network name' autocomplete='off'>":''}<div class='wfpr'><input type='password' class='wfp' placeholder='Password' autocomplete='off'${kind=='open'||kind=='owe'?' style="display:none"':''}><label${kind=='open'||kind=='owe'?' style="display:none"':''}><input type='checkbox' class='wfsh'> Show</label>${hidden?"<select class='wfk'><option value='psk'>WPA2/WPA3 password</option><option value='open'>No password</option></select>":''}</div><div class='msg wfe'></div><div class='rmrow' style='justify-content:flex-start'><button class='wfgo'>Connect</button><button class='wfx'>Cancel</button></div>`;
  row.after(f);const p=f.querySelector('.wfp'),n=f.querySelector('.wfn'),k=f.querySelector('.wfk');(n||p).focus();
  if(k)k.onchange=()=>{p.style.display=f.querySelector('.wfpr label').style.display=k.value=='psk'?'':'none'};
  f.querySelector('.wfsh').onchange=e=>p.type=e.target.checked?'text':'password';
  const go=()=>{const s=n?n.value.trim():ssid,kk=k?k.value:kind;const e=f.querySelector('.wfe');
   if(!s){e.textContent='Type the network name.';return}if(kk=='psk'&&(p.value.length<8||p.value.length>63)){e.textContent='A Wi-Fi password has 8 to 63 characters.';return}
   if(/[\n\r]/.test(s+p.value)){e.textContent='Line breaks cannot be used.';return}connect(s,kk=='psk'?p.value:'',kk,!!n)};
  f.querySelector('.wfgo').onclick=go;f.addEventListener('keydown',e=>{if(e.key=='Enter')go();if(e.key=='Escape')f.querySelector('.wfx').click()});
  f.querySelector('.wfx').onclick=()=>{open='';f.remove()};return f};
 const scan=async quiet=>{const l=q('.wfl');if(!quiet)l.innerHTML="<div class='msg'>Looking for networks…</div>";let r;try{r=await rdPost('wifi/scan'+qs)}catch(e){l.innerHTML=`<div class='msg'>${esc2(e.message)}</div>`;return}
  const best={};(r.raw||'').split('\n').slice(1).map(x=>x.split('\t')).filter(x=>x.length>=5&&x[4]).forEach(x=>{const n=wfDecode(x[4]),s=+x[2];if(!best[n]||s>best[n].s)best[n]={n,s,f:x[3],fr:+x[1]}});
  const list=Object.values(best).sort((a,b)=>(b.n==st.ssid&&st.wpa_state=='COMPLETED')-(a.n==st.ssid&&st.wpa_state=='COMPLETED')||savedHas(b.n)-savedHas(a.n)||b.s-a.s);
  if(!list.length){l.innerHTML="<div class='msg'>No networks found. Check that the router is on and nearby, then press Refresh.</div>";if(!scan.again&&wf.isConnected){scan.again=1;setTimeout(()=>scan(true),4000)}return}scan.again=0;
  l.innerHTML='';list.forEach(x=>{const kind=wfKind(x.f),cur=x.n==st.ssid&&st.wpa_state=='COMPLETED';const row=document.createElement('div');row.className='acr wfrow'+(cur?' cur':'');
   row.innerHTML=`${wfBars(x.s)}<span class='wfnm'></span><span class='msg'>${kind=='psk'?'🔒 '+(/SAE/.test(x.f)?(/PSK/.test(x.f)?'WPA2/WPA3':'WPA3'):'WPA2'):kind=='eap'?'🔒 Enterprise':kind=='wep'?'⚠️ WEP':kind=='owe'?'Enhanced open':'Open'}${x.fr>=5900?' · 6 GHz':x.fr>=4900?' · 5 GHz':' · 2.4 GHz'}${savedHas(x.n)&&!cur?' · Saved':''}</span><button>${cur?'Connected':'Connect'}</button>`;
   row.querySelector('.wfnm').textContent=x.n;const b=row.querySelector('button');b.disabled=cur;
   b.onclick=()=>{wf.querySelectorAll('.wfpw').forEach(f=>f.remove());if(kind=='eap'){notify('Wi-Fi',x.n+' needs a school or company sign-in (802.1X), which Rudolf OS does not support yet.',{kind:'error'});return}if(kind=='wep'){notify('Wi-Fi',x.n+' uses WEP, an old encryption that is no longer safe and is not supported. Switch the router to WPA2 or WPA3.',{kind:'error'});return}
    if(kind=='psk'&&!savedHas(x.n)){open=x.n;pwForm(row,x.n,kind,false)}else connect(x.n,'',savedHas(x.n)?'saved':kind,false)};
   l.appendChild(row);if(open==x.n)pwForm(row,x.n,kind,false)})};
 q('.wfre').onclick=()=>scan(false);
 q('.wfhid').onclick=e=>{wf.querySelectorAll('.wfpw').forEach(f=>f.remove());pwForm(e.target.parentNode,'','psk',true)};
 q('.wfccs').onclick=()=>{const c=q('.wfcc').value.trim().toUpperCase();if(!/^[A-Z]{2}$/.test(c)){notify('Wi-Fi','Type a two-letter country code, like US or DE.',{kind:'error'});return}rdPost('wifi/country'+qs+(qs?'&':'?')+'cc='+c).then(m=>{notify('Wi-Fi',m.message);setTimeout(()=>scan(false),1500)}).catch(e=>notify('Wi-Fi',e.message,{kind:'error'}))};
 refresh().then(()=>scan(false));
 const t=setInterval(()=>{if(!wf.isConnected){clearInterval(t);return}if(!busy)refresh()},5000)}
/* taskbar: Wi-Fi signal next to the volume button (installed system with a Wi-Fi adapter); opens Settings › Network */
(function(){if(typeof RD=='undefined')return;let b=null,miss=0;
 const tick=async()=>{if(!RD)return;let st;try{st=wfStatus((await rd('wifi/status')).raw)}catch(e){return}
  if(!st.adapter){if(b&&++miss>3){b.remove();b=null}return}miss=0;
  if(!b){const v=document.getElementById('vol');if(!v)return;b=document.createElement('button');b.id='wftray';b.onclick=()=>settings('net');v.before(b)}
  const on=st.wpa_state=='COMPLETED';b.className=on?'on':'';b.innerHTML=on?wfBars(+st.RSSI||-70):"<span class='wfb'>"+[1,2,3,4].map(i=>`<i style='height:${i*4+2}px'></i>`).join('')+"</span><span class='wfx0'>✕</span>";
  b.title=on?'Wi-Fi: '+st.ssid+(st.ip_address?' · '+st.ip_address:''):'Wi-Fi: not connected'};
 setTimeout(tick,8000);setInterval(tick,15000)})();
