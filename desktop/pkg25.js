/* ===== Packages (rpk): the Rudolf OS package manager on the desktop =====
   rpk (usr/bin/rpk, see iso/sys/rpk.c) installs signed, read-only package images and mounts them at
   /pkg/<name>. This module shows them in a Packages app, adds an "rpk" command to the Command Prompt
   (with < FILE and > FILE for files on drive C:), puts installed apps in the Start menu and opens web-app
   packages in windows. Everything here needs the bootable Rudolf OS: the browser version has no rpk. */
const pkgEsc=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'})[c]);
const PKGKIND={cli:'Command-line tool',app:'App',webapp:'Web app',addon:'Add-on'};
let PKGS=null,PKGT=0;
const pkgFmt=b=>b<1024?b+' B':b<1048576?Math.round(b/1024)+' KB':(b/1048576).toFixed(1)+' MB';
async function pkgStatus(force){if(!RD)return null;if(PKGS&&!force&&Date.now()-PKGT<4000)return PKGS;
 try{PKGS=await rd('pkg/status');PKGT=Date.now();pkgRegister()}catch(e){PKGS={ok:false,error:e.message,installed:[],available:[],sources:[]}}return PKGS}
/* installed apps and web apps become ordinary desktop apps (Start menu, taskbar pins, search) */
function pkgRegister(){if(!PKGS||!PKGS.installed)return;const have=new Set();
 PKGS.installed.forEach(p=>{if(p.kind!='webapp'&&p.kind!='app')return;const k='pkg:'+p.name;have.add(k);
  if(!APPS[k])regApp(k,{name:p.title||p.name,icon:p.icon||'📦',pkg:p.name,kw:'package '+p.name+' '+(p.summary||''),open:()=>pkgOpen(p.name),re:new RegExp('^'+(p.title||p.name).replace(/[.*+?^${}()|[\]\\]/g,'\\$&')+'( |$)')})});
 Object.keys(APPS).filter(k=>APPS[k].pkg&&!have.has(k)).forEach(k=>delete APPS[k]);
 try{startMenuApps()}catch(e){}}
{const _sma=startMenuApps;startMenuApps=function(){_sma();const ul=$('#menu ul');if(!ul)return;const before=ul.querySelector('li[data-a=settings]')||ul.querySelector('li[data-a=run]');
 Object.keys(APPS).filter(k=>APPS[k].pkg).forEach(k=>{const a=APPS[k],li=document.createElement('li');li.className='rapp';li.dataset.v25=k;li.textContent=a.icon+' '+a.name;li.tabIndex=0;li.onclick=e=>{e.stopPropagation();$('#menu').style.display='none';launch(k)};ul.insertBefore(li,before)})}}
async function pkgOpen(name){const s=await pkgStatus();const p=s&&s.installed.find(x=>x.name==name);if(!p)return notify('Packages',name+' is not installed.',{kind:'error'});
 if(p.kind=='webapp'){const entry=(p.entry||'www/index.html').replace(/^\/+/,'');if(!/^www\//.test(entry)||entry.includes('..'))return notify(p.title,'This package has no page to open.',{kind:'error'});
  const W=win(p.title||name,p.icon||'📦',760,560);W.body.style.cssText='padding:0;display:flex;background:#fff';
  const f=document.createElement('iframe');f.setAttribute('sandbox','allow-scripts allow-forms allow-modals allow-downloads');f.style.cssText='border:0;flex:1;width:100%;height:100%';
  f.src='/pkg/'+encodeURIComponent(name)+'/'+entry.split('/').map(encodeURIComponent).join('/')+(document.documentElement.classList.contains('dark')?'?theme=dark':'');W.body.appendChild(f);return W}
 if(p.kind=='app'){try{await rdPost('pkg/run?name='+encodeURIComponent(name));notify('Starting '+(p.title||name)+'…','It opens full screen. Press Super+Esc to come back to the desktop.')}catch(e){notify(p.title||name,e.message,{kind:'error'})}return}
 if(p.kind=='cli'){const P=prompt();P.exec&&P.print?P.print('Type: '+(p.commands||name)+' --help   (or rpk info '+name+')'):0;return}
 notify(p.title||name,'This add-on is used by Rudolf OS itself; there is nothing to open.')}
/* runs a package job (install, remove, update, refresh) and reports its log lines as they come */
async function pkgJob(path,onLine){await rdPost(path);let seen=0;
 for(;;){await new Promise(r=>setTimeout(r,600));const j=await rd('job');const lines=(j.log||'').split('\n');
  if(onLine)for(;seen<lines.length-1;seen++)onLine(lines[seen]);
  if(!j.running){if(onLine&&lines[seen])onLine(lines[seen]);await pkgStatus(true);return j.status===0}}}
const pkgAdmin=why=>isAdmin()?elevate(why):(notify('Packages','Only an administrator can '+why.toLowerCase()+'.',{kind:'error'}),Promise.resolve(false));

/* ---------- Packages app ---------- */
function pkgApp(){
 const W=win('Packages','📥',720,520);W.body.classList.add('pkw');
 if(!RD){W.body.innerHTML=`<div class='pkempty'><div style='font-size:40px'>📥</div><h3>Packages need the installed Rudolf OS</h3><p>Packages add programs to Rudolf OS: command-line tools, apps and web apps. They work when Rudolf OS is started from its disc, a USB drive or a hard disk, not in a web browser.</p></div>`;return W}
 let tab='all',q='',busy=false;
 W.body.innerHTML=`<div class='pkbar'><input class='pkq' type='search' placeholder='Search packages' aria-label='Search packages'><div class='pktabs' role='tablist'><button data-t='all' class='on'>All</button><button data-t='inst'>Installed</button><button data-t='upd'>Updates</button></div>
<button class='pkref' title='Download the package lists again'>⟳ Refresh</button><button class='pkupall'>Update all</button><button class='pkmore' aria-label='More'>⋯</button></div>
<div class='pkinfo'></div><div class='pklist' role='list'></div><pre class='pklog' hidden></pre>`;
 const q$=s=>W.body.querySelector(s),list=q$('.pklist'),log=q$('.pklog'),info=q$('.pkinfo');
 const say=(t,bad)=>{info.textContent=t;info.classList.toggle('bad',!!bad)};
 const run=async(path,why,doneMsg)=>{if(busy)return;if(why&&!(await pkgAdmin(why)))return;busy=true;log.hidden=false;log.textContent='';draw();
  try{const ok=await pkgJob(path,l=>{log.textContent+=l+'\n';log.scrollTop=log.scrollHeight});say(ok?doneMsg:'Something went wrong; see the messages below.',!ok)}catch(e){say(e.message,true)}busy=false;draw()};
 async function draw(){const s=await pkgStatus();if(!wins.includes(W))return;
  if(!s||!s.ok){list.innerHTML='';say(s?s.error:'Packages are not available.',true);return}
  const inst=Object.fromEntries(s.installed.map(p=>[p.name,p]));
  const srcOk=s.sources.filter(x=>x.ok).length;
  if(!busy&&!info.classList.contains('bad'))say(`${s.installed.length} installed · ${s.available.length} available from ${srcOk} source${srcOk==1?'':'s'}`+(s.persistent?'':' · running live: packages are lost when you shut down'));
  let rows=s.available.map(a=>Object.assign({},a,{inst:inst[a.name]}));
  s.installed.filter(p=>!s.available.some(a=>a.name==p.name)).forEach(p=>rows.push(Object.assign({},p,{inst:p,size:0})));
  if(tab=='inst')rows=rows.filter(r=>r.inst);if(tab=='upd')rows=rows.filter(r=>r.inst&&r.inst.update);
  if(q)rows=rows.filter(r=>(r.name+' '+r.title+' '+r.summary).toLowerCase().includes(q));
  rows.sort((a,b)=>(a.title||a.name).localeCompare(b.title||b.name));
  q$('.pkupall').disabled=busy||!s.installed.some(p=>p.update);q$('.pkref').disabled=busy;
  list.innerHTML=rows.length?'':`<div class='pkempty'>${tab=='upd'?'Everything is up to date.':tab=='inst'?'Nothing installed yet.':q?'Nothing matches.':'No packages. Check the sources (⋯ → Sources).'}</div>`;
  rows.forEach(r=>{const d=document.createElement('div');d.className='pkrow';d.setAttribute('role','listitem');
   const up=r.inst&&r.inst.update;
   d.innerHTML=`<div class='pkic'>${pkgEsc(r.icon||'📦')}</div><div class='pkmain'><div><b>${pkgEsc(r.title||r.name)}</b> <span class='pkmeta'>${(r.title||r.name)!=r.name?pkgEsc(r.name)+' ':''}${pkgEsc(r.inst?r.inst.version:r.version)}${up?' → '+pkgEsc(up):''}</span></div><div class='pksum'>${pkgEsc(r.summary||'')}</div>
<div class='pkmeta'><span class='pkkind'>${pkgEsc(PKGKIND[r.kind]||r.kind)}</span>${r.size?' · '+pkgFmt(r.size):''}${r.license?' · '+pkgEsc(r.license):''}${r.kind=='cli'&&r.inst?' · in Command Prompt: '+pkgEsc(r.inst.commands||r.name):''}</div></div><div class='pkbtns'></div>`;
   const B=d.querySelector('.pkbtns'),btn=(t,f,cls)=>{const b=document.createElement('button');b.textContent=t;if(cls)b.className=cls;b.disabled=busy;b.onclick=f;B.appendChild(b)};
   if(!r.inst)btn('Install',()=>run('pkg/install?name='+encodeURIComponent(r.name),'Install '+(r.title||r.name),(r.title||r.name)+' is installed.'),'pkpri');
   else{if(up)btn('Update',()=>run('pkg/install?name='+encodeURIComponent(r.name),'Update '+(r.title||r.name),(r.title||r.name)+' is up to date.'),'pkpri');
    if(r.kind!='addon')btn(r.kind=='cli'?'Try it':'Open',()=>r.kind=='cli'?prompt('rpk info '+r.name):pkgOpen(r.name));
    btn('Remove',()=>run('pkg/remove?name='+encodeURIComponent(r.name),'Remove '+(r.title||r.name),(r.title||r.name)+' was removed.'))}
   list.appendChild(d)})}
 W.body.querySelectorAll('.pktabs button').forEach(b=>b.onclick=()=>{tab=b.dataset.t;W.body.querySelectorAll('.pktabs button').forEach(x=>x.classList.toggle('on',x===b));draw()});
 q$('.pkq').oninput=e=>{q=e.target.value.trim().toLowerCase();draw()};
 q$('.pkref').onclick=()=>run('pkg/refresh',null,'Package lists are up to date.');
 q$('.pkupall').onclick=()=>run('pkg/update','Update all packages','All packages are up to date.');
 q$('.pkmore').onclick=e=>{const r=e.target.getBoundingClientRect();cmenu(r.left,r.bottom,[['Install from a file…',()=>pkgFromFile(W)],['Sources and keys…',()=>pkgSources(draw)],'-',['Help',()=>{const H=launch('help');H&&H.helpGo&&H.helpGo('packages')}]],{title:'Packages'})};
 draw();pkgStatus(true).then(draw);return W}
function pkgFromFile(W){const i=document.createElement('input');i.type='file';i.accept='.rpk,.RPK';i.onchange=async()=>{const f=i.files[0];if(!f)return;
 if(f.size>256*1048576)return notify('Packages','That file is over 256 MB.',{kind:'error'});if(!(await pkgAdmin('Install '+f.name)))return;
 const L=W.body.querySelector('.pklog');L.hidden=false;L.textContent='Uploading '+f.name+'…\n';
 try{await rdPost('pkg/installfile',new Uint8Array(await f.arrayBuffer()))}catch(e){L.textContent+=e.message+'\n';return}
 let seen=0;for(;;){await new Promise(r=>setTimeout(r,600));const j=await rd('job');const ls=(j.log||'').split('\n');for(;seen<ls.length-1;seen++)L.textContent+=ls[seen]+'\n';if(!j.running){L.textContent+=ls[seen]||'';break}}
 await pkgStatus(true);W.body.querySelector('.pkq').dispatchEvent(new Event('input'))};i.click()}
async function pkgSources(after){const s=await pkgStatus(true);const W=win('Package sources','📥',520,400);W.body.classList.add('pkw');
 const draw=async()=>{const s=await pkgStatus(true);W.body.innerHTML=`<p class='pkmeta' style='margin:0 0 8px'>Packages come from these places. Every package list and package must be signed by a trusted key (${s.keys} trusted).</p><div class='pksrc'></div>
<div class='pkadd'><input class='pku' placeholder='https://example.org/rudolf-packages' aria-label='New source address'><button class='pka'>Add source</button></div>
<details class='pkkeys'><summary>Trust another signing key</summary><p class='pkmeta'>Only add a key from someone you trust: packages it signs can run on this computer.</p><div class='pkadd'><input class='pkk' placeholder='44 characters of base64' aria-label='Public key'><button class='pkka'>Add key</button></div></details>`;
  const box=W.body.querySelector('.pksrc');
  s.sources.forEach(x=>{const d=document.createElement('div');d.className='pkrow';d.innerHTML=`<div class='pkic'>${x.type=='disc'?'💿':x.type=='web'?'🌐':'📁'}</div><div class='pkmain'><b>${pkgEsc(x.type=='disc'?'This disc':x.loc)}</b><div class='pkmeta'>${x.ok?x.count+' packages':pkgEsc(x.error||'not checked yet')}</div></div><div class='pkbtns'></div>`;
   if(x.type!='disc'){const b=document.createElement('button');b.textContent='Remove';b.onclick=async()=>{if(!(await pkgAdmin('Remove a package source')))return;try{await rdPost('pkg/source?op=remove&v='+encodeURIComponent(x.loc))}catch(e){notify('Packages',e.message,{kind:'error'})}draw();after&&after()};d.querySelector('.pkbtns').appendChild(b)}box.appendChild(d)});
  if(!s.sources.length)box.innerHTML=`<div class='pkempty'>No sources yet.</div>`;
  W.body.querySelector('.pka').onclick=async()=>{const v=W.body.querySelector('.pku').value.trim();if(!/^(https?:\/\/|\/)/.test(v))return notify('Packages','An address starts with https://, http:// or /.',{kind:'error'});
   if(!(await pkgAdmin('Add a package source')))return;try{await rdPost('pkg/source?op=add&v='+encodeURIComponent(v));await pkgJob('pkg/refresh')}catch(e){notify('Packages',e.message,{kind:'error'})}draw();after&&after()};
  W.body.querySelector('.pkka').onclick=async()=>{const v=W.body.querySelector('.pkk').value.trim();if(!/^[A-Za-z0-9+/]{43}=$/.test(v))return notify('Packages','A key is 44 characters of base64, ending in =.',{kind:'error'});
   if(!(await pkgAdmin('Trust a signing key')))return;try{await rdPost('pkg/key?v='+encodeURIComponent(v));notify('Packages','Key added.')}catch(e){notify('Packages',e.message,{kind:'error'})}draw()}};
 draw();return W}
regApp('packages',{name:'Packages',icon:'📥',kw:'package manager rpk install software apps tools apt',open:()=>pkgApp(),re:/^Packages$|^Package sources$/});

/* ---------- "rpk" in the Command Prompt ---------- */
function pkgTokens(line){const out=[];let cur='',q=null,any=false;for(const ch of line){if(q){if(ch==q)q=null;else cur+=ch}else if(ch=='"'||ch=="'"){q=ch;any=true}else if(/\s/.test(ch)){if(cur||any)out.push(cur);cur='';any=false}else cur+=ch}if(cur||any)out.push(cur);return out}
const pkgB64=s=>{const b=atob(s),u=new Uint8Array(b.length);for(let i=0;i<b.length;i++)u[i]=b.charCodeAt(i);return u};
function pkgFileBytes(n){if(typeof n!='string')return null;const m=/^data:[^,]*?(;base64)?,(.*)$/s.exec(n);if(m)return m[1]?pkgB64(m[2]):new TextEncoder().encode(decodeURIComponent(m[2]));return new TextEncoder().encode(n)}
async function pkgPromptRun(P,args){
 /* < FILE feeds a file from drive C: to the program, > FILE saves what it prints */
 let inF=null,outF=null;const a=[];for(let i=0;i<args.length;i++){if(args[i]=='<'&&args[i+1]!=null)inF=args[++i];else if(args[i]=='>'&&args[i+1]!=null)outF=args[++i];else if(/^<./.test(args[i]))inF=args[i].slice(1);else if(/^>./.test(args[i]))outF=args[i].slice(1);else a.push(args[i])}
 const name=a.shift();if(!name)return P.print('Usage: rpk run NAME [COMMAND] [ARGUMENTS] [< FILE] [> FILE]');
 const s=await pkgStatus(true),p=s&&s.installed.find(x=>x.name==name);if(!p)return P.print(name+' is not installed. Try: rpk search');
 if(p.kind!='cli'){P.print('Opening '+(p.title||name)+'…');return pkgOpen(name)}
 let body=null;if(inF){const n=node(resolve(inF,P.cwd));body=pkgFileBytes(n);if(!body)return P.print('File not found: '+inF)}
 if(a.some(x=>x.includes('\x1f')))return P.print('Bad character in the arguments.');
 let r;try{r=await rd('pkg/exec?name='+encodeURIComponent(name)+'&args='+encodeURIComponent(a.join('\x1f')),{method:'POST',body:body||new Uint8Array(0)})}catch(e){return P.print('rpk: '+e.message)}
 const out=pkgB64(r.out64||'');let text=null;try{text=new TextDecoder('utf-8',{fatal:true}).decode(out)}catch(e){}
 if(outF){const t=resolve(outF,P.cwd),d=dirOf(t);if(!isDir(d))return P.print('Path not found: '+outF);
  if(text!=null)d[t.at(-1)]=text;else{let b='';for(let i=0;i<out.length;i+=32768)b+=String.fromCharCode.apply(null,out.subarray(i,i+32768));d[t.at(-1)]='data:application/octet-stream;base64,'+btoa(b)}
  try{fsSave()}catch(e){save()}try{EXPL.forEach(f=>f())}catch(e){}P.print('Saved '+pkgFmt(out.length)+' to '+t.join('\\'))}
 else if(text!=null){if(text)P.write(text.endsWith('\n')?text:text+'\n')}else P.print('('+pkgFmt(out.length)+' of binary output: add > FILE to save it)');
 if(r.err)P.write(r.err.endsWith('\n')?r.err:r.err+'\n');if(r.stopped)P.print('rpk: stopped: '+r.stopped);if(r.code)P.print('(exit code '+r.code+')')}
async function pkgPrompt(P,args){const c=(args[0]||'help').toLowerCase(),rest=args.slice(1);
 if(!RD)return P.print('rpk needs Rudolf OS started from its disc, a USB drive or a hard disk.');
 if(c=='help'||c=='-h'||c=='--help')return P.print('rpk - Rudolf OS packages\n  rpk search [WORD]        packages you can install\n  rpk list                 installed packages\n  rpk info NAME            details of a package\n  rpk install NAME...      install or update packages (administrators)\n  rpk remove NAME...       remove packages (administrators)\n  rpk update               update everything installed\n  rpk refresh              download the package lists again\n  rpk run NAME [ARGS] [< FILE] [> FILE]   run a program from a package\n  rpk source list|add|remove [ADDRESS]\nInstalled command-line tools can also be typed directly, for example: jq . < DATA.JSON');
 const s=await pkgStatus(true);if(!s||!s.ok)return P.print('rpk: '+(s?s.error:'not available'));
 const inst=Object.fromEntries(s.installed.map(p=>[p.name,p]));
 if(c=='search'){const w=(rest[0]||'').toLowerCase();const r=s.available.filter(a=>!w||(a.name+' '+a.title+' '+a.summary).toLowerCase().includes(w));
  if(!r.length)return P.print(s.available.length?'Nothing matches.':'No package lists. Add a source: rpk source add ADDRESS');r.forEach(a=>P.print(a.name.padEnd(14)+' '+a.version.padEnd(12)+' '+(a.summary||'')+(inst[a.name]?'  [installed]':'')));return}
 if(c=='list'){if(!s.installed.length)return P.print('No packages installed. See: rpk search');s.installed.forEach(p=>P.print(p.name.padEnd(14)+' '+p.version.padEnd(12)+' '+(PKGKIND[p.kind]||p.kind)+(p.update?'  (update: '+p.update+')':'')));return}
 if(c=='info'){const n=rest[0];const p=inst[n]||s.available.find(a=>a.name==n);if(!p)return P.print('No package called "'+(n||'')+'".');
  P.print((p.title||p.name)+' '+(inst[n]?inst[n].version:p.version)+' - '+(p.summary||''));P.print('Kind: '+(PKGKIND[p.kind]||p.kind)+'   Licence: '+(p.license||'?')+(inst[n]?'   Installed':'   Not installed'));
  if(inst[n]&&p.kind=='cli')P.print('Commands: '+(inst[n].commands||n)+'   Try: '+((inst[n].commands||n).split(' ')[0])+' --help');if(inst[n]&&inst[n].homepage)P.print('Home page: '+inst[n].homepage);return}
 if(c=='run')return pkgPromptRun(P,rest);
 if(c=='source'){const op=(rest[0]||'list').toLowerCase();if(op=='list'){s.sources.forEach(x=>P.print(x.type.padEnd(7)+(x.type=='disc'?'this disc':x.loc)+'  '+(x.ok?x.count+' packages':x.error)));return}
  if(!rest[1])return P.print('Usage: rpk source add|remove ADDRESS');if(!(await pkgAdmin('Change package sources')))return P.print('Cancelled.');
  try{const r=await rdPost('pkg/source?op='+encodeURIComponent(op)+'&v='+encodeURIComponent(rest[1]));P.print((r.message||'').trim())}catch(e){P.print('rpk: '+e.message)}return}
 const jobs={install:'pkg/install?name=',remove:'pkg/remove?name=',update:'pkg/update',refresh:'pkg/refresh'};
 if(jobs[c]){if((c=='install'||c=='remove')&&!rest.length)return P.print('Usage: rpk '+c+' NAME...');
  if(c!='refresh'&&!(await pkgAdmin((c=='update'?'Update':c[0].toUpperCase()+c.slice(1))+' packages')))return P.print('Cancelled.');
  try{await pkgJob(jobs[c]+(c=='install'||c=='remove'?encodeURIComponent(rest.map(x=>x.toLowerCase()).join(',')):''),l=>P.print(l))}catch(e){P.print('rpk: '+e.message)}return}
 P.print('Unknown rpk command "'+c+'". Type: rpk help')}
const PROMPT_BUILTIN=new Set('HELP VER CLS ECHO DATE TIME DIR CD MD RD TYPE DEL COPY REN EDIT LUA BLOCKLUA EXPLORER EXIT'.split(' '));
{const _p=prompt;prompt=function(startCmd){const go=startCmd&&/^\s*rpk\b/i.test(startCmd);const P=_p(go?undefined:startCmd);const W=wins[wins.length-1],inp=W&&W.body.querySelector('.term input');
 if(inp)inp.addEventListener('keydown',e=>{if(e.key!='Enter')return;const v=inp.value,t=pkgTokens(v.trim()),c=(t[0]||'').toLowerCase();
  if(c=='help'&&t.length==1){setTimeout(()=>P.print('RPK           packages: rpk help'),0);return}
  let args=null;if(c=='rpk')args=t.slice(1);
  else if(c&&!PROMPT_BUILTIN.has(c.toUpperCase())&&PKGS&&PKGS.installed){const p=PKGS.installed.find(x=>x.kind=='cli'&&(x.commands||'').split(' ').includes(c));if(p)args=['run',p.name,...t]}
  if(!args)return;e.preventDefault();e.stopImmediatePropagation();
  const pr=W.body.querySelector('.term .pr');P.print((pr?pr.textContent:'')+v);if(v)P.hist.push(v);P.hi=P.hist.length;inp.value='';inp.disabled=true;
  pkgPrompt(P,args).catch(er=>P.print('rpk: '+er.message)).finally(()=>{inp.disabled=false;inp.focus()})},true);
 if(go&&inp){inp.value=startCmd;inp.dispatchEvent(new KeyboardEvent('keydown',{key:'Enter',bubbles:true,cancelable:true}))}
 return P}}

/* the prompt's Unix layer (pipes, > and >>) calls shCmd for each stage: package commands join in, so
   "cat DATA.JSON | jq .name > NAMES.TXT" works; "rpk ..." output can be piped or saved too */
{const _sc=shCmd;shCmd=async function(c,a,inp,P){
 const own=c&&PKGS&&PKGS.installed&&PKGS.installed.find(x=>x.kind=='cli'&&(x.commands||'').split(' ').includes(c));
 if(c=='rpk'||own){const buf=[];const Q={cwd:P.cwd,print:t=>buf.push(String(t)),write:t=>buf.push(String(t).replace(/\n$/,''))};
  if(c=='rpk')await pkgPrompt(Q,a);
  else await pkgShExec(Q,own.name,[c,...a],inp)
  return buf.join('\n')+(buf.length?'\n':'')}
 return _sc(c,a,inp,P)}}
async function pkgShExec(P,name,a,inp){let inF=null;const args=[];for(let i=0;i<a.length;i++){if(a[i]=='<'&&a[i+1]!=null)inF=a[++i];else if(/^<./.test(a[i]))inF=a[i].slice(1);else args.push(a[i])}
 let body=inp!=null?new TextEncoder().encode(inp):null;if(inF){body=pkgFileBytes(node(resolve(inF,P.cwd)));if(!body)throw new Error('no such file: '+inF)}
 const r=await rd('pkg/exec?name='+encodeURIComponent(name)+'&args='+encodeURIComponent(args.join('\x1f')),{method:'POST',body:body||new Uint8Array(0)});
 const out=pkgB64(r.out64||'');let text;try{text=new TextDecoder('utf-8',{fatal:true}).decode(out)}catch(e){text='('+pkgFmt(out.length)+' of binary output: use rpk run '+name+' ... > FILE)'}
 if(text)P.write(text);if(r.err)P.write(r.err);if(r.stopped)P.print('stopped: '+r.stopped);if(r.code)P.print('(exit code '+r.code+')')}

/* ---------- Help ---------- */
HELP.splice(HELP.findIndex(h=>h[0]=='apps')+1,0,['packages','Packages (rpk)',()=>`<p><b>Packages</b> add programs to Rudolf OS: command-line tools, apps and web apps. Each package is a signed, read-only image; installing one never changes Rudolf OS itself, and removing it deletes a single file.</p>
<h4>Install and remove</h4><ol><li>Open <button data-app='packages'>📥 Packages</button> from the Start menu.</li><li>Find a package and click <b>Install</b>. Administrators confirm with their password.</li><li>Apps and web apps appear in the Start menu. Command-line tools work in the <button data-app='prompt'>⬛ Command Prompt</button>.</li></ol>
<h4>In the Command Prompt</h4><ul><li><code>rpk search</code>, <code>rpk install jq</code>, <code>rpk list</code>, <code>rpk remove jq</code>, <code>rpk update</code>.</li><li>Type a tool's name directly: <code>jq .name &lt; DATA.JSON</code> reads DATA.JSON from drive C:, and <code>&gt; OUT.TXT</code> saves what it prints.</li><li>A program can run for up to 30 seconds and print up to 8 MB.</li></ul>
<h4>Where packages come from</h4><p>The disc carries its own packages, so they install without a network. Packages ⋯ → <b>Sources and keys</b> adds a web address. Every package list and package must be signed by a trusted key; Rudolf OS refuses anything altered or unsigned.</p>
<h4>Good to know</h4><ul><li>Packages need Rudolf OS started from its disc, a USB drive or a hard disk. When it runs live from the disc, installed packages are lost at shutdown; install Rudolf OS to keep them.</li><li>Big apps such as Roblox come from Flathub through the App Store instead.</li></ul>`]);
/* once someone has signed in, list installed packages so their apps appear in the Start menu */
{const t=setInterval(()=>{if(!RD){clearInterval(t);return}if(USER){clearInterval(t);pkgStatus(true)}},2000)}
