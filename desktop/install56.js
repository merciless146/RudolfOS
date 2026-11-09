/* ---------- 1.5.6: installing apps from files (the installation wizard) ----------
   Opening an installable file opens "Install <name>": what it is, what it needs, installing (with progress) and
   done (Open, Put on the desktop). Built type by type (docs/plan-1.5.6.md, 5a): .deb first, through Debian
   (rudolfd debian/debinfo and debian/deb, rudolf-debian deb). .ipa only explains. .exe, .msi, .AppImage, .bin and
   .apk follow in their own stages. */
const WZ_KINDS={DEB:'deb',IPA:'ipa'};
/* where the file is: a file kept on the disk, or one in the account's folders; rudolfd turns an account path into
   the name Linux sees it under (the binds), or refuses one Linux cannot see */
function wzDiskPath(path){const v=node(path);
 if(typeof isDlRef=='function'&&isDlRef(v)){const i=dlRefInfo(v);return i?dlDiskPath(i):null}
 if(typeof hdOn=='function'&&hdOn()&&typeof v=='string'){const h=homePath();if(path.length>h.length+1&&h.every((x,i)=>String(path[i]).toUpperCase()==String(x).toUpperCase())){
  const rel=hdRel(path.slice(h.length)),top=rel.split('/')[0];
  if(['Documents','Pictures','Music','Videos','Downloads'].includes(top))return HD_ROOT+HD.acct+'/'+rel}}   /* rudolfd gives Linux's name for it (home_linux_path) */
 return null}
function wzWin(path,icon){const nm=path.at(-1);const W=win('Install '+nm,icon||'📦',520,420);W.body.classList.add('wz');
 W.body.innerHTML=`<div class='wzh'><span class='wzi'>${icon||'📦'}</span><div><b class='wzt'></b><div class='wzs'></div></div></div><div class='wzb'></div><div class='wzf'></div>`;
 W.body.querySelector('.wzt').textContent=nm;return W}
const wzEsc=s=>String(s==null?'':s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
function wzFoot(W,btns){const f=W.body.querySelector('.wzf');f.innerHTML='';btns.forEach(([t,fn,main])=>{const b=document.createElement('button');b.textContent=t;if(main)b.className='wzmain';b.onclick=fn;f.appendChild(b)});return f}
function wzBody(W,html){const b=W.body.querySelector('.wzb');b.innerHTML=html;return b}
function wzSub(W,t){W.body.querySelector('.wzs').textContent=t}
function installWizard(path){const e=ext(path.at(-1)),k=WZ_KINDS[e];if(k=='deb')return wzDeb(path);if(k=='ipa')return wzIpa(path);return null}

/* ---------- .ipa: cannot be installed ---------- */
function wzIpa(path){const W=wzWin(path,'📱');wzSub(W,'An app for iPhone and iPad');
 wzBody(W,`<p>This app cannot be installed. iPhone and iPad apps are made for Apple's own system and are locked to Apple's devices: nothing outside an iPhone, iPad or Mac can run them.</p><p>Many apps have a version for Linux in the App Store, or a web version that opens in Oracle.</p>`);
 wzFoot(W,[['Look in the App Store',()=>{W.close();launch('store')},1],['Close',()=>W.close()]]);return W}

/* ---------- .deb: a package for Debian ---------- */
async function wzDeb(path){const W=wzWin(path,'📦');wzSub(W,'A package for Linux (Debian)');
 if(!RD){wzBody(W,'<p>Packages can be installed only on an installed Rudolf OS (not in a web browser).</p>');wzFoot(W,[['Close',()=>W.close()]]);return W}
 const file=wzDiskPath(path);
 if(!file){wzBody(W,'<p>Linux cannot reach this file where it is. Move it into Downloads or Documents, then open it again.</p>');wzFoot(W,[['Close',()=>W.close()]]);return W}
 wzBody(W,'<p class="wzwait">Reading the package…</p>');
 const st=await debStatus();
 if(!st||!st.installed){wzBody(W,`<p>Packages like this one are installed into Linux (Debian 13), which runs beside Rudolf OS. Linux is not set up on this computer yet: setting it up downloads about 120 MB.</p><p>Set it up, then open this file again.</p>`);
  wzFoot(W,[['Set up Linux…',()=>{W.close();launch('terminal')},1],['Close',()=>W.close()]]);return W}
 let info;try{info=await lxApi('debian/debinfo?file='+encodeURIComponent(file))}catch(e){wzBody(W,/cannot reach/.test(e.message)?`<p>${wzEsc(e.message)}, then open it again.</p>`:`<p>This package cannot be read: ${wzEsc(e.message)}</p><p>It may be damaged or incomplete; download it again.</p>`);wzFoot(W,[['Close',()=>W.close()]]);return W}
 const arch=info.arch||'',okArch=!arch||arch=='all'||arch=='amd64'||arch=='i386';
 const kb=+info.size||0,sz=kb?(kb>=1024?(kb/1024).toFixed(1)+' MB':kb+' KB'):'';
 wzSub(W,[info.package,info.version].filter(Boolean).join(' ')||'A package for Linux (Debian)');
 wzBody(W,`<p class='wzd'>${wzEsc(info.description||'')}</p><table class='wztab'>
  ${info.maintainer?`<tr><td>From</td><td>${wzEsc(info.maintainer)}</td></tr>`:''}
  ${sz?`<tr><td>Size once installed</td><td>${sz} (and what it needs)</td></tr>`:''}
  <tr><td>Installed into</td><td>Linux (Debian 13), for every account on this computer</td></tr></table>
  ${okArch?'':`<p class='wzerr'>This package is made for another kind of processor (${wzEsc(arch)}) and cannot run on this computer.</p>`}
  <p class='wznote'>Installing gives the package's own installation scripts administrator rights inside Linux. Install packages only from sources you trust.</p>`);
 if(!okArch){wzFoot(W,[['Close',()=>W.close()]]);return W}
 wzFoot(W,[['Install',()=>wzDebRun(W,path,file,info),1],['Cancel',()=>W.close()]]);return W}
async function wzDebRun(W,path,file,info){const name=info.package||path.at(-1);
 if(!isAdmin()&&!await elevate('Install '+name+' (a Linux package)'))return;
 const before=new Set((await debApps().catch(()=>[])||[]).map(a=>a.id));
 const b=wzBody(W,`<p class='wzstep'>Starting…</p><div class='wzbar'><i></i></div><details><summary>Details</summary><pre class='wzlog'></pre></details>`);
 wzFoot(W,[]);const step=b.querySelector('.wzstep'),bar=b.querySelector('.wzbar i'),log=b.querySelector('.wzlog');
 try{await lxApi('debian/deb?file='+encodeURIComponent(file))}catch(e){step.textContent=e.message;wzFoot(W,[['Close',()=>W.close()]]);return}
 let n=0;const j=await lxJobWait(l=>{log.textContent+=l+'\n';let m;
  if((m=/^step (.*)/.exec(l))){step.textContent=m[1]+'…';n=Math.max(n,m[1]=='Installing'?30:5)}
  else if(/^(Get|Unpacking|Setting up|Selecting|Preparing)/.test(l)){n=Math.min(95,n+3);step.textContent=l.replace(/\s*\.\.\.$/,'').slice(0,90)}
  bar.style.width=n+'%'});
 if(j.status!=0){bar.style.width='100%';bar.style.background='#c0392b';
  const last=(j.log||'').trim().split('\n').filter(l=>/^E:|error|rudolf-debian/i.test(l)).slice(-2).join(' ')||(j.log||'').trim().split('\n').pop()||'';
  step.textContent=(j.crashed&&typeof jobCrashText=='function'?jobCrashText(j):name+' could not be installed: '+last);wzFoot(W,[['Close',()=>W.close()]]);return}
 bar.style.width='100%';DEB.appsAt=0;const after=await debApps().catch(()=>[])||[];
 const fresh=after.filter(a=>!before.has(a.id)&&a.menu!==false);
 step.textContent=name+' is installed.';
 const box=document.createElement('div');box.className='wzapps';
 if(!fresh.length)box.innerHTML='<p>It has no app for the Start menu (a tool for the Terminal, or a part other apps use).</p>';
 else{box.innerHTML=`<p>${fresh.length==1?'Its app is':'Its apps are'} in the Start menu:</p>`;
  try{lxRegister()}catch(e){}
  fresh.forEach(a=>{const k=APPS['lx:deb-'+a.id]?'lx:deb-'+a.id:null;const r=document.createElement('div');r.className='wzapp';
   r.innerHTML=`<span>🐧</span><b></b><button class='o'>Open</button><button class='d'>Put on the desktop</button>`;r.querySelector('b').textContent=a.name;
   if(k&&APPS[k].img)APPS[k].img(r.querySelector('span'));
   r.querySelector('.o').onclick=()=>{k?launch(k):lxRun({deb:a.id,name:a.name})};r.querySelector('.d').onclick=e=>{if(k&&deskAdd(k)){e.target.disabled=true;e.target.textContent='On the desktop'}};
   if(!k)r.querySelector('.d').remove();box.appendChild(r)})}
 b.insertBefore(box,b.querySelector('details'));logEvt('sys',`Installed ${name} from ${path.at(-1)}`);
 wzFoot(W,[['Close',()=>W.close(),1]]);return true}

/* opening one of these files starts the wizard (before File Explorer's own openers and dl27's for files on the disk) */
{const _op=openPath;openPath=function(path){try{if(WZ_KINDS[ext(path.at(-1))]&&!isDir(node(path))){installWizard(path);return}}catch(e){console.warn('install',e)}return _op.apply(this,arguments)}}
