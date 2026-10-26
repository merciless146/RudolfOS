/* ===== Linux terminal: Debian 13 beside Rudolf OS =====
   rudolfd debian/* runs rudolf-debian, which keeps Debian in /persist/debian inside its own namespaces;
   term/* streams a shell there (term/read waits for output, term/write sends keys). The terminal is
   xterm.js (MIT), loaded from lib/ the first time it opens. Graphical apps installed with APT join the
   Linux apps of linux26.js: Start menu, search, Open with, taskbar buttons. */
const DEB={st:null,apps:[],appsAt:0,open:0};
const DEB_AK='bd95debapps';
async function debStatus(size){try{DEB.st=await lxApi('debian/status'+(size?'?size=1':''))}catch(e){DEB.st=null}return DEB.st}
function debLib(){if(window.Terminal&&window.FitAddon)return Promise.resolve();
 if(!debLib.p)debLib.p=new Promise((res,rej)=>{const l=document.createElement('link');l.rel='stylesheet';l.href='/lib/xterm.css';document.head.appendChild(l);
  const s=document.createElement('script');s.src='/lib/xterm.js';s.onload=()=>res();s.onerror=()=>{debLib.p=null;rej(new Error('The terminal could not be loaded.'))};document.head.appendChild(s)});return debLib.p}
/* ---------- Debian's graphical apps, listed with the Flatpak ones ---------- */
async function debApps(){if(!RD)return[];if(!DEB.st)await debStatus();if(!DEB.st||!DEB.st.installed){if(DEB.apps.length){DEB.apps=[];svj(DEB_AK,[]);lxRegister()}return[]}
 try{const j=await lxApi('debian/apps');const a=j.apps||[];const ch=JSON.stringify(a)!=JSON.stringify(DEB.apps);DEB.apps=a;DEB.appsAt=Date.now();if(ch){svj(DEB_AK,a);lxRegister()}}catch(e){}return DEB.apps}
{const _l=lxLaunchers;lxLaunchers=function(){const own=_l();const d=(DEB.apps.length?DEB.apps:ld(DEB_AK,[])).map(a=>({desktop:'deb-'+a.id,name:a.name,icon:a.icon,mime:a.mime,wmclass:a.wmclass,cats:a.categories,comment:'Debian',app:'deb:'+a.id,deb:a.id}));return own.concat(d)}}
{const _r=lxRun;lxRun=async function(x,file){if(!x.deb)return _r(x,file);if(!RD)return;
 try{await lxApi('debian/launch?id='+encodeURIComponent(x.deb)+(file?'&file='+encodeURIComponent(file):''));if(!lxRun.toldDeb){lxRun.toldDeb=1;notify('Starting '+x.name+'…','It opens above the desktop in a moment; its taskbar button switches back to it.')}}catch(e){notify(x.name,e.message,{kind:'error'})}}}
{const _i=lxImg;lxImg=function(el,id,icon){if(!String(id).startsWith('deb:'))return _i(el,id,icon);if(!icon)return;const k='deb|'+icon;
 if(!LX.icons.has(k))LX.icons.set(k,lxApi('debian/icon?path='+encodeURIComponent(icon)).then(r=>r.blob()).then(b=>URL.createObjectURL(b)).catch(()=>null));
 LX.icons.get(k).then(u=>{if(u&&el.isConnected){el.textContent='';const i=new Image();i.src=u;i.alt='';el.appendChild(i)}})}}
/* ---------- one terminal session ---------- */
function debB64(s){const b=atob(s),u=new Uint8Array(b.length);for(let i=0;i<b.length;i++)u[i]=b.charCodeAt(i);return u}
class DebSession{
 constructor(host,opts){this.host=host;this.alive=true;this.off=0;this.q=[];this.sending=false;this.ctrl=false;this.onexit=opts.onexit;this.ontitle=opts.ontitle;
  this.t=new Terminal({fontSize:ld('bd95termfs',15),fontFamily:"'DejaVu Sans Mono','Liberation Mono',monospace",cursorBlink:true,scrollback:5000,allowProposedApi:true,
   theme:{background:'#0b1020',foreground:'#e2e8f0',cursor:'#93c5fd',selectionBackground:'#334155'}});
  this.fit=new FitAddon();this.t.loadAddon(this.fit);this.t.open(host);
  this.t.attachCustomKeyEventHandler(e=>{if(e.type!='keydown')return true;
   if(e.ctrlKey&&e.shiftKey&&(e.key=='C'||e.key=='c')){const s=this.t.getSelection();if(s)navigator.clipboard.writeText(s).catch(()=>{});return false}
   if(e.ctrlKey&&e.shiftKey&&(e.key=='V'||e.key=='v')){navigator.clipboard.readText().then(t=>this.paste(t)).catch(()=>{});return false}return true});
  this.t.onData(d=>{if(this.ctrl&&d.length==1){const c=d.toUpperCase().charCodeAt(0);if(c>=64&&c<96)d=String.fromCharCode(c-64);else if(d=='?')d='\x7f';this.ctrl=false;opts.onctrl&&opts.onctrl(false)}this.send(d)});
  this.t.onTitleChange(t=>this.ontitle&&this.ontitle(t));
  this.start()}
 async start(){try{const j=await lxApi('term/open');this.id=j.id}catch(e){this.t.write('\r\n\x1b[31m'+e.message+'\x1b[0m\r\n');this.alive=false;return}
  this.resize();this.loop()}
 async loop(){while(this.alive){let j;try{j=await lxApi(`term/read?id=${this.id}&off=${this.off}`)}catch(e){await new Promise(r=>setTimeout(r,1500));if(!this.host.isConnected)break;continue}
   if(j.gone||j.exit!=null){this.alive=false;this.t.write('\r\n\x1b[2m[The shell has ended. Close this tab or open a new one.]\x1b[0m\r\n');this.onexit&&this.onexit();break}
   if(j.data)this.t.write(debB64(j.data));this.off=j.off}}
 send(d){if(!this.id||!this.alive)return;this.q.push(d);this.flush()}
 paste(t){if(t)this.send(t.replace(/\r?\n/g,'\r'))}
 async flush(){if(this.sending)return;this.sending=true;try{while(this.q.length){const d=this.q.join('');this.q=[];await lxApi('term/write?id='+this.id,new TextEncoder().encode(d))}}catch(e){}this.sending=false}
 resize(){try{this.fit.fit()}catch(e){return}if(this.id&&this.alive)lxApi(`term/resize?id=${this.id}&rows=${this.t.rows}&cols=${this.t.cols}`).catch(()=>{})}
 font(n){this.t.options.fontSize=n;this.resize()}
 close(){if(this.id&&this.alive)lxApi('term/close?id='+this.id).catch(()=>{});this.alive=false;try{this.t.dispose()}catch(e){}}
}
/* ---------- the Terminal app ---------- */
async function terminalApp(){
 if(!RD)return notify('Terminal','The Linux terminal needs the installed Rudolf OS (bootable disc or USB).',{kind:'error'});
 if(!USER||USER.r=='guest')return notify('Terminal','The Linux terminal is not available to guests. Sign in with your own account.',{kind:'error'});
 const W=win('Terminal','🐧',820,520);W.body.innerHTML="<div class='msg' style='padding:16px'>Starting…</div>";
 const st=await debStatus();
 if(!st)return W.body.innerHTML="<div class='msg' style='padding:16px'>This Rudolf OS cannot run Debian (the system is too old: update it in Settings › Update and recovery).</div>";
 if(!st.installed)return debSetupPage(W);
 try{await debLib()}catch(e){W.body.innerHTML=`<div class='msg' style='padding:16px'>${lxEsc(e.message)}</div>`;return}
 debTermUI(W);lxSync()}
function debTermUI(W){
 W.body.classList.add('dterm');
 W.body.innerHTML=`<div class='dtbar'><div class='dttabs' role='tablist'></div><button class='dtnew' title='New tab (Ctrl+Shift+T)'>＋</button><span style='flex:1'></span><button class='dtfm' title='Smaller text'>A−</button><button class='dtfp' title='Larger text'>A+</button></div>
 <div class='dtarea'></div><div class='dtkeys' aria-label='Extra keys'>${['Esc','Tab','Ctrl','↑','↓','←','→','|','~','/','-','Paste'].map(k=>`<button data-k='${k}'>${k}</button>`).join('')}</div>`;
 const tabsEl=W.body.querySelector('.dttabs'),area=W.body.querySelector('.dtarea');const tabs=[];let cur=null;DEB.open++;
 const ctrlBtn=W.body.querySelector("[data-k='Ctrl']");
 const show=t=>{cur=t;tabs.forEach(x=>{x.box.style.display=x==t?'':'none';x.btn.classList.toggle('on',x==t);x.btn.setAttribute('aria-selected',x==t)});setTimeout(()=>{t.s.resize();t.s.t.focus()},30);W.setTitle(t.title||'Terminal')};
 const add=()=>{const box=document.createElement('div');box.className='dtbox';area.appendChild(box);const btn=document.createElement('button');btn.className='dttab';btn.setAttribute('role','tab');btn.innerHTML="<span>Debian</span><b title='Close tab'>✕</b>";tabsEl.appendChild(btn);
  const t={box,btn,title:''};t.s=new DebSession(box,{onexit:()=>{btn.classList.add('ended')},ontitle:x=>{t.title=x;btn.firstChild.textContent=x.replace(/^rudolf@rudolf: ?/,'')||'Debian';if(cur==t)W.setTitle(x)},onctrl:v=>ctrlBtn.classList.toggle('on',v)});
  btn.onclick=e=>{if(e.target.tagName=='B'){closeTab(t);return}show(t)};tabs.push(t);show(t)};
 const closeTab=t=>{t.s.close();t.box.remove();t.btn.remove();tabs.splice(tabs.indexOf(t),1);if(!tabs.length){W.close();return}show(tabs.at(-1))};
 W.body.querySelector('.dtnew').onclick=add;
 const fs=d=>{const n=Math.max(10,Math.min(28,ld('bd95termfs',15)+d));svj('bd95termfs',n);tabs.forEach(t=>t.s.font(n))};
 W.body.querySelector('.dtfm').onclick=()=>fs(-1);W.body.querySelector('.dtfp').onclick=()=>fs(1);
 const K={Esc:'\x1b',Tab:'\t','↑':'\x1b[A','↓':'\x1b[B','→':'\x1b[C','←':'\x1b[D'};
 W.body.querySelector('.dtkeys').onclick=e=>{const k=e.target.dataset&&e.target.dataset.k;if(!k||!cur)return;
  if(k=='Ctrl'){cur.s.ctrl=!cur.s.ctrl;ctrlBtn.classList.toggle('on',cur.s.ctrl)}else if(k=='Paste')navigator.clipboard.readText().then(t=>cur.s.paste(t)).catch(()=>notify('Terminal','Use Ctrl+Shift+V to paste.'));else cur.s.send(K[k]||k);cur.s.t.focus()};
 /* the desktop's own shortcuts stay out of the terminal; Ctrl+Shift+T opens a tab */
 W.body.addEventListener('keydown',e=>{if(e.ctrlKey&&e.shiftKey&&(e.key=='T'||e.key=='t')){e.preventDefault();add()}e.stopPropagation()});
 W.body.addEventListener('keyup',e=>e.stopPropagation());
 let rt;new ResizeObserver(()=>{clearTimeout(rt);rt=setTimeout(()=>cur&&cur.s.resize(),80)}).observe(area);
 /* Documents, Pictures and Downloads are kept in step with drive C: while a terminal is open */
 const poll=setInterval(()=>{if(!W.el.isConnected){clearInterval(poll);return}debApps();lxSync()},15000);
 W.onclose=()=>{tabs.forEach(t=>t.s.close());DEB.open--;setTimeout(debApps,1500);lxSync()};
 W.term={add,tabs:()=>tabs,cur:()=>cur};add();return W}
function debSetupPage(W){const st=DEB.st;W.setTitle('Set up Linux (Debian 13)');W.body.classList.remove('dterm');
 W.body.innerHTML=`<div class='dsetup'><h3>🐧 Linux terminal with Debian 13</h3>
 <p>Debian is a complete Linux system that runs beside Rudolf OS. You get a terminal, and <code>sudo apt install</code> gives you Debian's 60,000 programs: command-line tools, programming languages and graphical apps, which appear in the Start menu.</p>
 <ul><li>Download: about 30 MB. Space used: about 130 MB to start, more for each program you install.</li><li>Debian cannot change Rudolf OS: it runs in a sandbox, and Settings › Apps can remove it at any time.</li><li>Your Documents, Pictures and Downloads are shared with it.</li></ul>
 <div class='dsform'></div><div class='dsprog' hidden><div class='dsstep'></div><div class='dsbar'><i></i></div></div><div class='msg dserr'></div></div>`;
 const f=W.body.querySelector('.dsform'),err=W.body.querySelector('.dserr');
 if(!st.persist){f.innerHTML="<p class='msg'>Debian is kept on the data partition, so it needs Rudolf OS installed (Settings › Update and recovery › Install here…). It cannot run from the live disc.</p>";return}
 if(!isAdmin()){f.innerHTML="<p class='msg'>An administrator sets Debian up once; after that every account (except Guest) can open the terminal.</p>";return}
 f.innerHTML=`<p><b>Choose a Linux password.</b> Debian asks for it when you use <code>sudo</code> (to install programs). It can be the same as your Rudolf OS password.</p>
 <div class='r2'><label>Linux password</label><input class='dp1' type='password' autocomplete='new-password'><label>Type it again</label><input class='dp2' type='password' autocomplete='new-password'></div>
 <div class='rmrow' style='justify-content:flex-start'><button class='dsgo pri'>Set up Debian</button></div>`;
 f.querySelector('.dsgo').onclick=async()=>{const a=f.querySelector('.dp1').value,b=f.querySelector('.dp2').value;err.textContent='';
  if(a.length<4)return err.textContent='The password needs at least 4 characters.';if(a!=b)return err.textContent='The two passwords are different.';if(/[:\n]/.test(a))return err.textContent='The password cannot contain : or a line break.';
  try{await lxApi('debian/setup',a)}catch(e){return err.textContent=e.message}
  f.hidden=true;const pg=W.body.querySelector('.dsprog');pg.hidden=false;const step=pg.querySelector('.dsstep'),bar=pg.querySelector('.dsbar i');
  const j=await lxJobWait(l=>{let m;if((m=/^step (.*)/.exec(l)))step.textContent=m[1]+'…';else if((m=/^progress (\d+)/.exec(l)))bar.style.width=m[1]+'%';else if(/^done /.test(l)){step.textContent=l.slice(5);bar.style.width='100%'}});
  if(j.status!=0){pg.hidden=true;f.hidden=false;err.textContent=((j.log||'').trim().split('\n').pop()||'Setup failed.').replace(/^rudolf-debian: /,'');return}
  notify('Debian is ready','Open the Terminal from the Start menu any time.');await debStatus();try{await debLib()}catch(e){err.textContent=e.message;return}W.setTitle('Terminal');debTermUI(W);lxSync()}}
regApp('terminal',{name:'Terminal',icon:'🐧',kw:'terminal linux debian apt shell bash command line console sudo',open:()=>terminalApp(),re:/^(Terminal|Set up Linux)/});
{const _sma=startMenuApps;startMenuApps=function(){_sma();const ul=$('#menu ul');if(!ul||!RD)return;ul.querySelectorAll('li.dtv').forEach(x=>x.remove());if(!USER||USER.r=='guest')return;
 const before=ul.querySelector('li[data-a=settings]')||ul.querySelector('li[data-a=run]');const li=document.createElement('li');li.className='rapp dtv';li.dataset.v25='terminal';li.tabIndex=0;li.textContent='🐧 Terminal (Linux)';
 li.onclick=e=>{e.stopPropagation();$('#menu').style.display='none';terminalApp()};li.onkeydown=e=>{if(e.key=='Enter')li.click()};ul.insertBefore(li,before)}}
addEventListener('keydown',e=>{if(RD&&e.ctrlKey&&e.altKey&&(e.key=='t'||e.key=='T')&&USER&&USER.r!='guest'){e.preventDefault();terminalApp()}});
/* ---------- Linux check (1.2.7): what Linux apps and the terminal need, from rudolf-fscheck (rudolfd debian/check) ---------- */
const LXCHK=[['persist','Data partition'],['persist_fs','File system'],['persist_kind','Kind'],['persist_features','Features'],['persist_inode_size','Inode size'],['persist_opts','Mounted with'],
 ['tmpfile_persist','Temporary files on the data partition'],['tmpfile_flatpak','…in Flatpak\'s folder'],['tmpfile_flatpak_repo','…in Flatpak\'s repository'],['tmpfile_var_tmp','…in /var/tmp'],['tmpfile_tmp','…in /tmp'],['tmpfile_debian','…in Debian'],
 ['flatpak_repo','Flatpak repository'],['debian_owner','Debian folder owner'],['userns','User namespaces'],['proc_mount','A new /proc for sandboxes'],['max_user_namespaces','User namespaces allowed'],['debian','Debian sandbox']];
const lxBad=v=>/^(failed|missing|not mounted|tmp folder missing|refused)|did not start|did not finish|not available|expected 200000/.test(String(v||''));
async function lxCheck(){try{return await lxApi('debian/check')}catch(e){return{error:e.message}}}
/* text lines for the system report */
function lxCheckLines(r){if(!r||r.error)return['Linux check: '+(r&&r.error||'not available')];return LXCHK.filter(([k])=>r[k]!=null&&r[k]!=='').map(([k,l])=>`Linux check: ${l.replace(/^…/,'temporary files ')}: ${r[k]}`)}
async function lxCheckShow(box){box.innerHTML="<div class='msg'>Checking…</div>";const r=await lxCheck();
 if(r.error){box.innerHTML='';const m=document.createElement('div');m.className='msg';m.textContent='The check could not run: '+r.error;box.appendChild(m);return r}
 const bad=LXCHK.filter(([k])=>lxBad(r[k]));box.innerHTML=`<p class='msg'></p><table class='tmt'><tbody></tbody></table>`;box.firstChild.textContent=bad.length?`${bad.length} problem(s): ${bad.map(([,l])=>l.replace(/^…/,'temporary files ')).join(', ')}. Copy the system report (Task Manager › System) to report it.`:'Everything Linux apps and the terminal need is working.';
 const tb=box.querySelector('tbody');LXCHK.forEach(([k,l])=>{if(r[k]==null||r[k]==='')return;const row=tb.insertRow();row.insertCell().textContent=l;const c=row.insertCell();c.textContent=(lxBad(r[k])?'⚠️ ':'')+r[k]});return r}
/* ---------- Settings › Apps › Linux (Debian) ---------- */
{const _ap=appsPanel;appsPanel=function(c,W){_ap(c,W);if(!RD)return;
 const d=sp(c,`<h4 class='sh4'>Linux (Debian 13)</h4><div class='dset'><div class='msg'>Checking…</div></div>`);const b=d.querySelector('.dset');
 const draw=async()=>{const st=await debStatus(true);if(!st){b.innerHTML="<div class='msg'>This Rudolf OS cannot run Debian.</div>";return}
  if(!st.installed){b.innerHTML=`<div class='msg'>Debian is not set up. It adds a Linux terminal with APT.</div><div class='rmrow' style='justify-content:flex-start'><button class='dsu'>Set up Debian…</button><button class='dck'>Check</button></div><div class='dckr'></div>`;b.querySelector('.dsu').onclick=()=>terminalApp();b.querySelector('.dck').onclick=()=>lxCheckShow(b.querySelector('.dckr'));return}
  const auto=ld('bd95debauto',true);
  b.innerHTML=`<table class='tmt'><tbody><tr><td>Version</td><td>Debian ${lxEsc(st.version)}</td></tr><tr><td>Space used</td><td>${fmtB(st.size||0)}</td></tr><tr><td>Free on the data partition</td><td>${fmtB(st.free||0)}</td></tr><tr><td>Apps with a window</td><td>${DEB.apps.length}</td></tr></tbody></table>
  <label class='chk'><input type='checkbox' class='dau'${auto?' checked':''}> Install Debian's updates automatically once a day</label>
  <div class='rmrow' style='justify-content:flex-start'><button class='dop'>Open Terminal</button><button class='dup'>Update now</button><button class='drm'>Remove Debian…</button><button class='dck'>Check</button></div><div class='msg dmsg'></div><div class='dckr'></div>`;b.querySelector('.dck').onclick=()=>lxCheckShow(b.querySelector('.dckr'));
  const msg=b.querySelector('.dmsg');b.querySelector('.dau').onchange=e=>svj('bd95debauto',e.target.checked);b.querySelector('.dop').onclick=()=>terminalApp();
  b.querySelector('.dup').onclick=async()=>{if(!isAdmin()&&!await elevate('Update Debian'))return;try{await lxApi('debian/update')}catch(e){return msg.textContent=e.message}msg.textContent='Updating Debian…';const j=await lxJobWait();msg.textContent=j.status==0?'Debian is up to date.':'The update did not finish: '+((j.log||'').trim().split('\n').pop()||'');debApps()};
  b.querySelector('.drm').onclick=async()=>{if(!await elevate('Remove Debian and every program installed in it (your Documents, Pictures and Downloads stay)'))return;try{await lxApi('debian/remove')}catch(e){return msg.textContent=e.message}msg.textContent='Removing…';const j=await lxJobWait();await debStatus();DEB.apps=[];svj(DEB_AK,[]);lxRegister();msg.textContent=j.status==0?'Debian was removed.':'Removing failed.';setTimeout(draw,800)}};
 draw()}}
/* ---------- daily updates (quiet; skipped while a terminal is open or another task runs) ---------- */
async function debAutoUpdate(){if(!RD||!USER||window.LOCKED||DEB.open>0||!ld('bd95debauto',true))return;if(Date.now()-ld('bd95debupd',0)<22*3600e3)return;
 const st=await debStatus();if(!st||!st.installed||!navigator.onLine)return;try{const j=await rd('job');if(j.running)return}catch(e){return}
 try{await lxApi('debian/update')}catch(e){return}svj('bd95debupd',Date.now());await lxJobWait();debApps()}
if(RD){(function(){const o=window.__afterLogin;window.__afterLogin=()=>{o&&o();setTimeout(debApps,2500)}})();setTimeout(debAutoUpdate,20*60e3);setInterval(debAutoUpdate,3*3600e3)}
