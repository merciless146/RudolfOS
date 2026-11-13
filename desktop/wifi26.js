/* ---------- Wi-Fi (Settings › Network): networks in range, password entry, saved networks ----------
   rudolfd wifi/* runs /usr/lib/rudolf/wifi.sh, which drives wpa_supplicant. */
function wfDecode(s){/* wpa_cli escapes names: \xNN, \\, \", \e, \n, \r, \t */
 const b=[];for(let i=0;i<s.length;i++){const c=s[i];if(c=='\\'&&i+1<s.length){const n=s[++i];if(n=='x'){b.push(parseInt(s.substr(i+1,2),16));i+=2}else b.push({n:10,r:13,t:9,e:27}[n]||n.charCodeAt(0))}else{const e=new TextEncoder().encode(c);e.forEach(x=>b.push(x))}}
 try{return new TextDecoder('utf-8',{fatal:true}).decode(new Uint8Array(b))}catch(e){return String.fromCharCode(...b)}}
function wfBars(dbm){const n=dbm>=-55?4:dbm>=-67?3:dbm>=-75?2:1;return `<span class='wfb' title='${dbm} dBm'>${[1,2,3,4].map(i=>`<i class='${i<=n?'on':''}' style='height:${i*4+2}px'></i>`).join('')}</span>`}
function wfKind(f){if(/EAP/.test(f))return 'eap';if(/OWE/.test(f)&&!/PSK|SAE/.test(f))return 'owe';if(/PSK|SAE/.test(f))return 'psk';if(/WEP/.test(f))return 'wep';return 'open'}
function wfStatus(raw){const o={saved:[],wlans:[]};(raw||'').split('\n').forEach(l=>{const i=l.indexOf('=');if(i<0)return;const k=l.slice(0,i),v=l.slice(i+1);if(k=='saved'){const p=v.split('\t');o.saved.push({id:p[0],ssid:wfDecode(p[1]||''),flags:p[3]||''})}
  /* 1.5.3: every Wi-Fi adapter: name, MAC, usb|pci, driver, in words */
  else if(k=='wlan'){const p=v.split('\t');o.wlans.push({ifn:p[0],mac:p[1],bus:p[2],driver:p[3],name:p[4]||p[3]||p[0]})}else o[k]=v});if(o.ssid)o.ssid=wfDecode(o.ssid);return o}
/* the adapter in words: "USB: Ralink 802.11 n WLAN", "Built-in (rtw88_8821ce)" */
function wfAdapterName(a){return a.bus=='usb'?'USB: '+a.name:'Built-in'+(a.driver?' ('+a.driver+')':'')}
function wifiPanel(wf,ifn){
 wf.innerHTML=`<div class='wfad'></div><div class='wfcur'></div><div class='wfhd'><b>Networks in range</b><button class='wfre'>↻ Refresh</button></div><div class='wfl'><div class='msg'>Looking for networks…</div></div>
 <div class='rmrow' style='justify-content:flex-start'><button class='wfhid'>Join a hidden network…</button></div>
 <details class='wfrad'><summary>Radios of this network</summary><div class='wfradb'><div class='msg'>Connect to a network to see its radios.</div></div></details>
 <div class='wfbd'><span class='wfbdw'><button class='wfbdb' aria-haspopup='menu' aria-expanded='false'>📡 This computer's Wi-Fi radios ▾</button><div class='wfbdm' role='menu' aria-label="This computer's Wi-Fi radios" hidden></div></span><div class='msg wfbdn'></div></div>
 <div class='rmrow' style='justify-content:flex-start'><button class='wfwhy'>⚡ Why does the Wi-Fi drop?</button></div>
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
  if(st.country&&!q('.wfcc').value)q('.wfcc').value=st.country;showAdapters()};
 /* 1.5.3: with two or more Wi-Fi adapters (a USB dongle next to the built-in one), which one to use; one at a time */
 let adKey='';const showAdapters=()=>{const a=q('.wfad');if(!a||ifn)return;const k=JSON.stringify([st.wlans,st.adapter,st.chosen_missing]);if(k==adKey)return;adKey=k;
  if(st.wlans.length<2&&!st.chosen_missing){a.innerHTML='';return}
  a.innerHTML=`<div class='r2'><label>Adapter</label><select class='wfas'></select></div>${st.chosen_missing?"<div class='msg'>The USB Wi-Fi adapter you chose is not plugged in, so the built-in Wi-Fi is used. Plug it in to use it again.</div>":''}`;
  const sel=a.querySelector('select');st.wlans.forEach(w=>{const o=document.createElement('option');o.value=w.ifn;o.textContent=wfAdapterName(w);o.selected=w.ifn==st.adapter;sel.appendChild(o)});
  sel.disabled=st.wlans.length<2;
  sel.onchange=async()=>{sel.disabled=true;try{const m=await rdPost('wifi/adapter?if='+encodeURIComponent(sel.value));notify('Wi-Fi',m.message)}catch(e){notify('Wi-Fi',e.message,{kind:'error'})}adKey='';await refresh();scan(false)}};
 /* 1.5.9: the radios of the network in use: one router often has a 2.4 GHz and a 5 GHz radio (and 6 GHz) under one
    name. Each with its band, channel, signal and state, and a switch: turned off, this computer never joins that radio
    (kept, restarts included). A radio that did not answer is left out for a day by itself (wifi-event). */
 let radKey='';const radios=async()=>{const b=q('.wfradb');if(!b||!q('.wfrad').open)return;let r;try{r=await rd('wifi/radios'+qs)}catch(e){b.innerHTML=`<div class='msg'>${esc2(e.message)}</div>`;return}
  const o={radios:[]};(r.raw||'').split('\n').forEach(l=>{const i=l.indexOf('=');if(i<0)return;const k=l.slice(0,i),v=l.slice(i+1);if(k=='radio'){const p=v.split('\t');o.radios.push({b:p[0],f:+p[1],s:+p[2],st:p[3]||''})}else o[k]=v.trim()});
  const key=JSON.stringify(o);if(key==radKey)return;radKey=key;
  const bands=(o.bands||'').split(' ').filter(Boolean).map(x=>x+' GHz');
  const boffs=(o.bandsoff||'').split(' ').filter(Boolean);
  let h=`<div class='msg'>This computer's Wi-Fi: ${bands.length?esc2(bands.join(', ')):'its bands are not known'}${boffs.length?' ('+esc2(boffs.map(x=>x+' GHz').join(', '))+' turned off by an administrator)':''}.</div>`;
  if(!o.ssid){b.innerHTML=h+"<div class='msg'>Connect to a network to see its radios.</div>";return}
  if(!o.radios.length){b.innerHTML=h+"<div class='msg'>No radios found yet: press Refresh above to look again.</div>";return}
  b.innerHTML=h;o.radios.sort((x,y)=>x.f-y.f).forEach(x=>{const band=x.f>=5925?'6 GHz':x.f>=4900?'5 GHz':'2.4 GHz',ch=x.f>=5925?Math.round((x.f-5950)/5):x.f>=4900?Math.round((x.f-5000)/5):x.f==2484?14:Math.round((x.f-2407)/5);
   const fail=x.st.startsWith('failed:')?new Date(+x.st.slice(7)*1000):null,boff=x.st=='bandoff',st=x.st=='inuse'?'In use':x.st=='off'?'Turned off':boff?`This computer's ${band} radio is off`:fail?"Didn't answer: left out until "+fail.toLocaleTimeString([], {hour:'numeric',minute:'2-digit'}):'Available';
   const d=document.createElement('div');d.className='acr wfrow wfradr';d.innerHTML=`${wfBars(x.s)}<span style='flex:1'><b>${band}</b> · channel ${ch} <span class='msg'>· ${esc2(st)} · ${esc2(x.b)}</span></span><label class='ck' title='Turned off, this computer never joins this radio'><input type='checkbox'> Use</label>`;
   const c=d.querySelector('input');c.checked=x.st!='off'&&!fail&&!boff;if(boff){c.disabled=true;c.parentNode.title="This computer's "+band+" radio is turned off (This computer's Wi-Fi radios, below)"}c.onchange=async()=>{c.disabled=true;try{const m=await rdPost('wifi/radio'+qs+(qs?'&':'?')+'bssid='+encodeURIComponent(x.b)+'&on='+(c.checked?'on':'off'));notify('Wi-Fi',band+' radio: '+(c.checked?'turned on':'turned off')+(x.st=='inuse'&&!c.checked?'; moving to another radio of this network':''))}catch(e){notify('Wi-Fi',e.message,{kind:'error'});c.checked=!c.checked}c.disabled=false;radKey='';setTimeout(radios,x.st=='inuse'?4000:300)};
   b.appendChild(d)});
  if(o.radios.filter(x=>x.st!='off').length<=1&&o.radios.length>1)b.insertAdjacentHTML('beforeend',"<div class='msg'>Only one radio of this network is left on: if it fails, this computer cannot reach the network until you turn another on.</div>")};
 /* 1.5.9: this computer's own Wi-Fi radios (its adapter's bands: 2.4, 5, 6 GHz), in a drop-down menu: a check by each
    one that is on, a blank space by one that is off. Only an administrator changes it, typing their password each time
    (no "remembered for two minutes"); the system keeps the choice as it is (wifi.sh band: nothing automatic changes it,
    and a router's radio on a band that is off cannot be turned on in "Radios of this network"). */
 const bdm=q('.wfbdm'),bdb=q('.wfbdb');let bd=null;
 const bdClose=()=>{bdm.hidden=true;bdb.setAttribute('aria-expanded','false')};
 const bdShow=()=>{bdm.innerHTML='';const adm=typeof isAdmin=='function'&&isAdmin(),n=q('.wfbdn');
  if(!bd||!bd.bands.length){bdm.innerHTML="<div class='msg' style='padding:8px'>This computer's Wi-Fi radios are not known yet.</div>";n.textContent='';return}
  bd.bands.forEach(x=>{const it=document.createElement('button');it.className='wfbdi';it.setAttribute('role','menuitemcheckbox');it.setAttribute('aria-checked',x.on?'true':'false');
   it.innerHTML=`<span class='wfbdc' aria-hidden='true'>${x.on?'✓':''}</span><span style='flex:1'>${esc2(x.b)} GHz radio</span><span class='msg'>${bd.cur==x.b?'in use':''}</span>`;
   it.disabled=!adm;it.onclick=()=>bdSet(x);bdm.appendChild(it)});
  const off=bd.bands.filter(x=>!x.on).map(x=>x.b+' GHz');
  n.textContent=(bd.name?bd.name+': ':'')+(off.length?off.join(', ')+' turned off by an administrator; Rudolf OS does not turn '+(off.length>1?'them':'it')+' back on by itself.':'every radio is on.')+(adm?'':' Only an administrator can change this.')};
 const bdLoad=async()=>{try{const r=await rd('wifi/bands'+qs);const o={bands:[]};(r.raw||'').split('\n').forEach(l=>{const i=l.indexOf('=');if(i<0)return;const k=l.slice(0,i),v=l.slice(i+1);
   if(k=='band'){const p=v.split('\t');o.bands.push({b:p[0],on:p[1]!='off'})}else if(k=='adapter'){const p=v.split('\t');o.name=wfAdapterName({bus:p[2],name:p[3],driver:p[3]})}else if(k=='freq'){const f=+v;o.cur=f>=5925?'6':f>=4900?'5':f?'2.4':''}});bd=o}catch(e){bd=null}bdShow()};
 const bdSet=async x=>{if(!(typeof isAdmin=='function'&&isAdmin())){notify('Wi-Fi','Only an administrator can change which Wi-Fi radios this computer uses.',{kind:'error'});return}
  const on=!x.on;if(!on&&bd.bands.filter(y=>y.on).length<=1){notify('Wi-Fi',"One of this computer's Wi-Fi radios has to stay on. To stop using Wi-Fi, turn Wi-Fi off.",{kind:'error'});return}
  bdClose();window.__elevTrust=0;if(!await elevate((on?'Turn on':'Turn off')+" this computer's "+x.b+' GHz Wi-Fi radio'+(on?'':': this computer will not use '+x.b+' GHz networks or radios until an administrator turns it on again')))return;
  try{const m=await rdPost('wifi/band'+qs+(qs?'&':'?')+'band='+encodeURIComponent(x.b)+'&on='+(on?'on':'off'));notify('Wi-Fi',m.message);typeof logEvt=='function'&&logEvt('sys',`${USER?USER.n:'?'}: ${m.message}`)}catch(e){notify('Wi-Fi',e.message,{kind:'error'})}
  await bdLoad();radKey='';setTimeout(radios,x.b==bd?.cur?4000:300)};
 bdb.onclick=e=>{e.stopPropagation();if(!bdm.hidden){bdClose();return}bdm.hidden=false;bdb.setAttribute('aria-expanded','true');bdShow();bdLoad().then(()=>{const f=bdm.querySelector('.wfbdi:not(:disabled)');if(f)f.focus()})};
 bdm.addEventListener('keydown',e=>{const it=[...bdm.querySelectorAll('.wfbdi')],i=it.indexOf(document.activeElement);if(e.key=='Escape'){bdClose();bdb.focus()}else if(e.key=='ArrowDown'&&it.length){e.preventDefault();it[(i+1)%it.length].focus()}else if(e.key=='ArrowUp'&&it.length){e.preventDefault();it[(i-1+it.length)%it.length].focus()}});
 {const oc=e=>{if(!wf.isConnected){document.removeEventListener('click',oc);return}if(bdm.hidden)return;if(!q('.wfbd').contains(e.target))bdClose()};document.addEventListener('click',oc)}
 bdLoad();
 q('.wfrad').ontoggle=()=>{radKey='';radios()};
 q('.wfwhy').onclick=()=>{if(typeof bzWhy=='function')bzWhy('The Wi-Fi keeps dropping or disconnecting',{wifi:true});else notify('Blitzen is not available','Blitzen needs the installed Rudolf OS.')};
 q('.wfwhy').hidden=typeof bzCan!='function'||!bzCan();
 const refresh=async()=>{try{st=wfStatus((await rd('wifi/status'+qs)).raw);showCur();radios()}catch(e){}};
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
/* (1.4.4) the taskbar's own Wi-Fi icon is gone: the network icon (net31.js) shows the Wi-Fi and joins networks */
