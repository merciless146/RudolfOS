/* ===== Rudolf OS 1.6.2 (docs/plan-1.6.2.md Part 35): network shares in File Explorer
   File Explorer's Network: the computers and NAS boxes found on the home network (rudolfd's shares/found: WS-Discovery and
   multicast DNS), a share's address typed in the address bar (\\server\share or smb://server/share), the account's recent
   shares (kept in its own settings, without passwords; reconnected only when opened, never at start-up) and its open ones.
   A share opens as a folder of its own (files25.js's drive panes: browse, open, copy to and from, drag onto the desktop), on
   Native, Chromium and Lite alike (the same page). Signing in asks for the name, the password (kept in the account's keyring
   only when "Remember the password" is ticked) and, under More options, the domain and SMB 2.1; or as a guest. The password
   goes from its box into the one request and the box is emptied: it is never kept in the page. Disconnect closes the share.
   Hooks in files25.js: xNetSide (the places), xNetPane (the Network view), xNetAddr (the address bar), xNetRemoteBar (a
   share's own bar: Disconnect). Names from the network are only ever text (textContent), never HTML. ===== */
const NET={found:null,looking:null,open:[],t:0};
const netRecK=()=>'bd95netrecent:'+(typeof uid=='function'&&USER?uid():'');
function netRecent(){try{const v=JSON.parse(localStorage.getItem(netRecK())||'[]');return Array.isArray(v)?v.filter(r=>r&&r.server&&r.share).slice(0,12):[]}catch(e){return[]}}
function netRecentAdd(r){const l=netRecent().filter(x=>!(x.server.toLowerCase()==r.server.toLowerCase()&&x.share==r.share&&(x.user||'')==(r.user||'')));
 l.unshift({server:r.server,share:r.share,user:r.user||'',ver:r.ver||'3',guest:!!r.guest,ip:r.ip||'',dom:r.dom||'',t:Date.now()});try{localStorage.setItem(netRecK(),JSON.stringify(l.slice(0,12)))}catch(e){}}
function netRecentDel(r){try{localStorage.setItem(netRecK(),JSON.stringify(netRecent().filter(x=>!(x.server==r.server&&x.share==r.share&&(x.user||'')==(r.user||'')))))}catch(e){}}
/* rudolfd's answer with its reason (why) kept, which rd() leaves out */
async function netRd(p,body){if(!RD)throw new Error('Network shares need the installed Rudolf OS.');
 const r=await fetch('/api/'+p,{method:body!=null?'POST':'GET',headers:{'X-Rudolf-Token':RD.token},body});let j={};try{j=await r.json()}catch(e){}
 if(!j.ok){const e=new Error(j.error||('HTTP '+r.status));e.why=j.why||'';e.status=r.status;throw e}return j}
const netEnc=encodeURIComponent;
const netLabel=r=>'\\\\'+r.server+'\\'+r.share;
/* \\server\share, \\server, smb://server/share, smb://server (a user: smb://user@server/share) */
function netParse(v){v=String(v||'').trim();let m=v.match(/^\\\\([^\\\/]+)(?:\\([^\\\/]+))?(?:[\\\/].*)?$/)||v.match(/^smb:\/\/(?:([^@\/]+)@)?([^\/]+)(?:\/([^\/]+))?(?:\/.*)?$/i);if(!m)return null;
 if(/^smb:/i.test(v))return{user:m[1]?decodeURIComponent(m[1]):'',server:m[2],share:m[3]?decodeURIComponent(m[3]):''};return{server:m[1],share:m[2]||'',user:''}}
const netQ=r=>'server='+netEnc(r.server)+(r.ip?'&ip='+netEnc(r.ip):'')+(r.ver=='2.1'?'&ver=2.1':'')+(r.guest?'&guest=1':'');
async function netOpenList(){try{const j=await netRd('shares/open');NET.open=j.shares||[]}catch(e){NET.open=[]}return NET.open}
async function netFind(force){if(NET.looking)return NET.looking;if(NET.found&&!force&&Date.now()-NET.t<60000)return NET.found;
 NET.looking=netRd('shares/found').then(j=>{NET.found=j.servers||[];NET.t=Date.now();return NET.found},e=>{NET.found=[];throw e}).finally(()=>{NET.looking=null});return NET.looking}
/* the sign-in box: resolves {user,pass,dom,guest,keep,ver} or null; the password box emptied when it closes */
function netAsk(r,why){return new Promise(res=>{const o=document.createElement('div');o.className='rmodal';
 o.innerHTML=`<div class='rmbox' role='dialog' aria-label='Connect to a share' style='min-width:360px'><h3>Connect to <span class='nl'></span></h3><div class='nw' style='color:#b91c1c;margin:0 0 8px' role='alert'></div>
<label style='display:block;margin:6px 0'>Name <input class='nu' autocomplete='username' style='width:100%'></label><label style='display:block;margin:6px 0'>Password <input class='np' type='password' autocomplete='current-password' style='width:100%'></label>
<label class='ck'><input type='checkbox' class='ng'> Connect as a guest (no name or password)</label><br><label class='ck'><input type='checkbox' class='nk'> Remember the password (in your keyring)</label>
<details style='margin:8px 0'><summary>More options</summary><label style='display:block;margin:6px 0'>Domain or workgroup <input class='nd' style='width:100%' placeholder='(usually empty)'></label><label class='ck'><input type='checkbox' class='n21'> Use SMB 2.1 (for an older server; SMB 3 is used otherwise)</label></details>
<div class='rmrow'><button class='c'>Cancel</button> <button class='o'>Connect</button></div></div>`;
 const q=s=>o.querySelector(s),u=q('.nu'),p=q('.np'),g=q('.ng');q('.nl').textContent=r.share?netLabel(r):'\\\\'+r.server;q('.nw').textContent=why||'';
 u.value=r.user||'';g.checked=!!r.guest;q('.nd').value=r.dom||'';q('.n21').checked=r.ver=='2.1';
 const sync=()=>{u.disabled=p.disabled=q('.nk').disabled=g.checked};g.onchange=sync;sync();
 const done=v=>{const pw=p.value;p.value='';o.remove();res(v?Object.assign(v,{pass:pw}):null)};
 q('.c').onclick=()=>done(null);q('.o').onclick=()=>{if(!g.checked&&!u.value.trim()){q('.nw').textContent='Type a name, or connect as a guest.';u.focus();return}done({user:u.value.trim(),guest:g.checked,keep:q('.nk').checked,dom:q('.nd').value.trim(),ver:q('.n21').checked?'2.1':'3'})};
 o.onkeydown=e=>{if(e.key=='Escape')done(null);if(e.key=='Enter'&&e.target.tagName=='INPUT'&&e.target.type!='checkbox')q('.o').click()};
 document.body.appendChild(o);(u.value&&!g.checked?p:u).focus()})}
const netBody=(a)=>(a.guest?'':a.user)+'\n'+(a.guest?'':a.pass||'')+'\n'+(a.dom||'')+'\n';
/* a share opened (asking for the password when none is kept, or the kept one no longer works); its folder shown */
async function netOpen(api,r,a){r=Object.assign({ver:'3'},r);
 for(let round=0;round<4;round++){
  if(!a&&!r.guest&&!r.user)a=await netAsk(r);
  if(!a&&!r.guest&&r.user)a={user:r.user,pass:'',dom:r.dom||'',guest:false,keep:false,ver:r.ver};
  if(!a&&r.guest)a={guest:true,ver:r.ver,dom:''};
  if(!a)return null;
  Object.assign(r,{user:a.guest?'':a.user,guest:!!a.guest,ver:a.ver||r.ver,dom:a.dom||''});
  api.say('Connecting to '+netLabel(r)+'…');
  try{const j=await netRd('shares/open?'+netQ(r)+'&share='+netEnc(r.share)+(a.pass?'&keep='+(a.keep?1:0):''),netBody(a));a.pass='';
   netRecentAdd(Object.assign({},r,{ip:j.ip||r.ip}));
   const rem={dev:'share:'+j.id,root:j.path,path:j.path,label:netLabel(r),share:{id:j.id,server:r.server,share:r.share,encrypted:!!j.encrypted}};
   api.openRemote(rem);api.say('Connected to '+netLabel(r)+(j.encrypted?' (encrypted).':'.'));netOpenList().then(()=>api.redraw());return rem}
  catch(e){a.pass='';const why=e.why;
   if(why=='password'&&!r.guest){a=await netAsk(r,e.message);continue}
   if(why=='smb21'&&r.ver!='2.1'){a=await netAsk(Object.assign({},r,{ver:'2.1'}),e.message);continue}
   api.say(e.message,true);return null}}
 return null}
/* a server's shares (after signing in to it), then one opened */
async function netServer(api,s,box){const r={server:s.name&&!/^\d+\.\d+\.\d+\.\d+$/.test(s.name)?(s.host||s.name):s.ip,ip:s.ip,share:'',ver:'3'};
 let a=await netAsk(r);if(!a)return;
 for(let round=0;round<3;round++){Object.assign(r,{user:a.guest?'':a.user,guest:!!a.guest,ver:a.ver||'3',dom:a.dom||''});api.say('Asking '+r.server+' for its shared folders…');
  try{const j=await netRd('shares/list?'+netQ(r),netBody(Object.assign({},a,{keep:false})));const keep=a;
   const list=(j.shares||[]).map(x=>x.name);if(!list.length){keep.pass='';api.say(r.server+' shares no folders with this name.',true);return}
   netPick(api,r,j.shares,keep);return}
  catch(e){if(e.why=='password'||e.why=='smb21'){a.pass='';a=await netAsk(Object.assign({},r,{ver:e.why=='smb21'?'2.1':r.ver}),e.message);if(!a)return;continue}a.pass='';api.say(e.message,true);return}}}
/* the list of a server's shares: one clicked is opened with the same sign-in (its password used once more, then dropped) */
function netPick(api,r,shares,a){const o=document.createElement('div');o.className='rmodal';
 o.innerHTML=`<div class='rmbox' role='dialog' aria-label='Shared folders'><h3>Shared folders on <span></span></h3><div class='nsl' style='display:flex;flex-direction:column;gap:4px;max-height:50vh;overflow:auto'></div><div class='rmrow'><button class='c'>Cancel</button></div></div>`;
 o.querySelector('h3 span').textContent=r.server;const l=o.querySelector('.nsl');
 const close=()=>{a.pass='';o.remove()};
 shares.forEach(s=>{const b=document.createElement('button');b.className='owb';b.textContent='📁  '+s.name+(s.remark?' — '+s.remark:'');b.onclick=()=>{o.remove();const pw=a.pass;a.pass='';netOpen(api,Object.assign({},r,{share:s.name}),Object.assign({},a,{pass:pw}))};l.appendChild(b)});
 o.querySelector('.c').onclick=close;o.onkeydown=e=>{if(e.key=='Escape')close()};document.body.appendChild(o);const f=l.querySelector('button');f&&f.focus()}
async function netClose(api,rem){try{await netRd('shares/close?id='+netEnc(rem.share.id),'');api.say('Disconnected from '+rem.label+'.');await netOpenList();
  try{dispatchEvent(new CustomEvent('rudolf-drive',{detail:{a:'remove',dev:rem.dev}}))}catch(e){}}catch(e){api.say(e.message,true)}}
/* ---- File Explorer's hooks */
/* the places: Network, then the account's open shares */
function xNetSide(sec,item,pn,api){if(!RD||!api)return;sec('Network');item('🌐','Network',pn.special=='network',()=>api.network());
 NET.open.forEach(s=>{const lab='\\\\'+s.server+'\\'+s.share;const d=item(s.encrypted?'🔒':'🗂️',lab,pn.remote&&pn.remote.dev=='share:'+s.id,()=>api.openRemote({dev:'share:'+s.id,root:s.path,path:s.path,label:lab,share:{id:s.id,server:s.server,share:s.share,encrypted:s.encrypted}}));d.title=lab+(s.encrypted?' (encrypted)':'')+(s.guest?' (as a guest)':s.user&&s.user!='-'?' as '+s.user:'')});
 if(!NET.t0){NET.t0=1;netOpenList().then(l=>{if(l.length)api.redraw()})}}
/* the Network view */
function xNetPane(pn,box,api){box.innerHTML='';const h=(t,txt)=>{const e=document.createElement(t);if(txt!=null)e.textContent=txt;return e};
 const wrap=h('div');wrap.className='xnet';wrap.style.cssText='padding:12px;display:flex;flex-direction:column;gap:10px';box.appendChild(wrap);
 const row=h('div');row.style.cssText='display:flex;gap:6px;align-items:center';const inp=h('input');inp.className='xna';inp.placeholder='\\\\server\\share or smb://server/share';inp.setAttribute('aria-label','A share\'s address');inp.style.flex='1';inp.spellcheck=false;
 const go=h('button','Connect');go.className='xngo';const rf=h('button','⟳ Look again');rf.className='xnrf';row.append(inp,go,rf);wrap.appendChild(row);
 go.onclick=()=>{const r=netParse(inp.value);if(!r){api.say('Type a share\'s address as \\\\server\\share or smb://server/share.',true);return}if(!r.share){netServer(api,{name:r.server,host:r.server,ip:''});return}netOpen(api,r)};
 inp.onkeydown=e=>{if(e.key=='Enter')go.click()};
 const sec=t=>{const d=h('div',t);d.className='xsh';d.style.marginTop='6px';wrap.appendChild(d)};
 const card=(icon,title,sub,act,menu)=>{const b=h('div');b.className='xnc';b.tabIndex=0;b.setAttribute('role','button');b.style.cssText='display:flex;gap:10px;align-items:center;padding:8px 10px;border-radius:8px;cursor:pointer;border:1px solid rgba(127,127,127,.25)';
  const i=h('span',icon);i.style.fontSize='22px';const t=h('div');const t1=h('div',title);t1.style.fontWeight='600';t.appendChild(t1);if(sub){const t2=h('div',sub);t2.style.cssText='font-size:12px;opacity:.7';t.appendChild(t2)}
  b.append(i,t);b.onclick=act;b.onkeydown=e=>{if(e.key=='Enter')act()};if(menu)b.oncontextmenu=e=>{e.preventDefault();e.stopPropagation();cmenu(e.clientX,e.clientY,menu(),{title})};wrap.appendChild(b);return b};
 const rec=netRecent();
 if(rec.length){sec('Recent shares');rec.forEach(r=>{const isOpen=NET.open.find(s=>s.server.toLowerCase()==r.server.toLowerCase()&&s.share==r.share);
   card(isOpen?'🗂️':'🕘',netLabel(r),(r.guest?'As a guest':r.user?'As '+r.user:'')+(isOpen?' · connected':' · not connected (opened when you click it)'),()=>isOpen?api.openRemote({dev:'share:'+isOpen.id,root:isOpen.path,path:isOpen.path,label:netLabel(r),share:{id:isOpen.id,server:r.server,share:r.share,encrypted:isOpen.encrypted}}):netOpen(api,r),
    ()=>[['Open',()=>isOpen?null:netOpen(api,r),!isOpen,{icon:'↗'}],isOpen?['Disconnect',()=>netClose(api,{dev:'share:'+isOpen.id,label:netLabel(r),share:{id:isOpen.id}}).then(()=>api.redraw()),true,{icon:'⏏'}]:null,
     r.user&&!r.guest?['Forget the remembered password',()=>netRd('shares/forget?server='+netEnc(r.server)+'&share='+netEnc(r.share)+'&user='+netEnc(r.user)+(r.ver=='2.1'?'&ver=2.1':''),'').then(j=>api.say(j.message=='forgotten'?'The password for '+netLabel(r)+' is forgotten.':'No password was remembered for '+netLabel(r)+'.'),e=>api.say(e.message,true)),true,{icon:'🔑'}]:null,
     ['Remove from this list',()=>{netRecentDel(r);api.redraw()},true,{icon:'✕'}]].filter(Boolean))})}
 sec('On this network');const fl=h('div');fl.className='xnf';fl.style.cssText='display:flex;flex-direction:column;gap:6px';wrap.appendChild(fl);
 const st=h('div','Looking for computers and NAS boxes…');st.className='xempty';fl.appendChild(st);
 const show=list=>{fl.innerHTML='';if(!list.length){const e=h('div','None found. A Windows computer shows up when its network is set to private with file sharing on; a NAS box when its SMB service is on. You can also type a share\'s address above.');e.className='xempty';fl.appendChild(e);return}
  const p=wrap;list.forEach(s=>{const b=card(s.how=='wsd'?'🖥️':'🗄️',s.name,(s.how=='wsd'?'Windows computer':'NAS or computer')+' · '+s.ip+(s.group?' · '+s.group:''),()=>netServer(api,s));fl.appendChild(b)});void p};
 rf.onclick=()=>{fl.innerHTML='';fl.appendChild(st);netFind(true).then(show,e=>{st.textContent='The network could not be looked at: '+e.message})};
 netFind(false).then(show,e=>{st.textContent='The network could not be looked at: '+e.message});
 api.say('Network: computers and NAS boxes on this network, and your shares.')}
/* the address bar: \\server\share or smb://... */
function xNetAddr(v,api){if(!/^(\\\\|smb:\/\/)/i.test(String(v||'').trim()))return false;const r=netParse(v);if(!r){api.say('Type a share\'s address as \\\\server\\share or smb://server/share.',true);return true}
 if(!r.share)netServer(api,{name:r.server,host:r.server,ip:''});else netOpen(api,r);return true}
/* a share's own bar: Disconnect (in place of Safely remove) */
function xNetRemoteBar(pn,bar,api){const b=bar.querySelector('.rej');if(!b)return;b.textContent='⏏ Disconnect';b.title='Disconnect from '+pn.remote.label;b.onclick=()=>{const rem=pn.remote;netClose(api,rem).then(()=>api.home())}}
