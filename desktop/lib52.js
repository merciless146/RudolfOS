/* ===== Rudolf OS 1.5.2: the Rudolf OS Library =====
   Apps made for Rudolf OS that not every computer needs live in a signed package repository of their own,
   github.com/merciless146/rudolfos-library (published at LIB_URL). rpk (iso/sys/rpk.c) has it as a system source
   (/etc/rudolf/rpk-sources) and trusts its key (/etc/rudolf/library-keys) for files for the desktop only: never for
   programs, never in place of a package that came with Rudolf OS, and never for a system update.
   - App Store › Rudolf OS Library: each app with its icon, sizes, description and licence; Get, Open, Update, Remove.
   - libNeed(name): an app whose files are in the Library asks to download them, then carries on (Quake's free game
     data, Emu's DS player).
   - Computers that played DS games before 1.5.2 get Emu's player back by themselves, once, in the background. */
window.__lib52=1;
const LIB_URL='https://merciless146.github.io/rudolfos-library';
const LIB_APPS={
 'rudolf-quake':{app:'quake',title:'Quake',size:21e6,why:'Quake’s free game data (LibreQuake)'},
 'rudolf-emu':{app:'gameboy',title:'Emu',size:0.4e6,why:'Emu’s Nintendo DS player'}};
const libBase=n=>'/pkg/'+n+'/www/';
const libSize=b=>b>=1e6?(b/1e6).toFixed(b<1e7?1:0)+' MB':Math.max(1,Math.round(b/1e3))+' KB';
async function libHas(n){if(!RD)return false;try{const s=await pkgStatus(true);const p=s&&(s.installed||[]).find(x=>x.name==n);return !!(p&&p.mounted)}catch(e){return false}}
/* true once the package is there: asks first, then downloads it (rpk install, one job at a time) */
async function libNeed(n){if(!RD)return false;if(await libHas(n))return true;
 const a=LIB_APPS[n]||{title:n,size:0,why:n};
 /* the size from the signed package list when rpk has it */
 let size=a.size;try{const s=await pkgStatus();const v=s&&(s.available||[]).find(x=>x.name==n);if(v&&v.size)size=v.size}catch(e){}
 if(!(await appAskDlg(`${a.title} needs a download`,`${a.why} is in the Rudolf OS Library (${libSize(size)}). Download it now?`,'Download')))return false;
 notify(a.title,'Downloading from the Rudolf OS Library…');let good=false;
 try{good=await pkgJob('pkg/install?name='+encodeURIComponent(n))}catch(e){}
 if(!good){notify(a.title,'The download did not work. Check the internet connection, then try again from App Store › Rudolf OS Library.',{kind:'error'});return false}
 notify(a.title,'Downloaded from the Rudolf OS Library.');return true}
/* the catalog's descriptions (the signed package list has titles, sizes and licences; this adds the longer text) */
let LIBCAT=null;
async function libCatalog(){if(LIBCAT)return LIBCAT;const c={};let got=false;try{const r=await fetch(LIB_URL+'/catalog.json',{cache:'no-store'});if(r.ok){((await r.json()).apps||[]).forEach(a=>c[a.name]=a);got=true}}catch(e){}
 /* 1.5.7: a list that could not be had (offline) is not kept: it is asked for again next time */
 if(got)LIBCAT=c;return c}
async function libTab(B){
 B.innerHTML=`<p class='msg' style='margin:0 0 10px'>Apps made for Rudolf OS that not every computer needs. They come from the <b>Rudolf OS Library</b>, signed with its own key: it can add apps, but it can never change Rudolf OS itself.</p><div class='libl'></div><pre class='jl liblog' hidden></pre>`;
 const L=B.querySelector('.libl'),log=B.querySelector('.liblog');
 if(!RD){L.innerHTML="<p class='msg'>The Library needs Rudolf OS itself (the bootable disc, a USB drive or the hard disk).</p>";return}
 L.innerHTML="<p class='msg'>Loading…</p>";
 /* 1.5.7: offline: the Library's list waits for the internet, then shows */
 if(typeof netLive=='function'&&!netLive()){L.innerHTML="<p class='msg'>Waiting for the internet to get the Library’s list… It shows by itself once this computer is online.</p>";await netWait(10000);   /* (after 10 s it is tried anyway: rudolfd may not know the network) */
 if(!B.isConnected)return;L.innerHTML="<p class='msg'>Loading…</p>"}
 let s=await pkgStatus(true);const src=s&&(s.sources||[]).find(x=>x.loc==LIB_URL);
 if(src&&!src.ok){L.innerHTML="<p class='msg'>Getting the Library’s list…</p>";try{await pkgJob('pkg/refresh')}catch(e){}s=await pkgStatus(true)}
 const cat=await libCatalog();if(!B.isConnected)return;
 const av=(s&&s.available||[]).filter(p=>p.library),inst=(s&&s.installed||[]).filter(p=>p.signed=='library'||p.signed=='developer');   /* (1.5.9: and outside developers' apps, plan 8.2) */
 const names=[...new Set([...av.map(p=>p.name),...inst.map(p=>p.name)])];L.innerHTML='';
 if(!names.length){L.innerHTML=`<p class='msg'>${src&&src.ok===false&&src.error?'The Library could not be reached: '+src.error+'. ':''}The Library’s list could not be downloaded. Check the internet connection and open this tab again.</p>`;return}
 let busy=false;
 for(const n of names){const a=av.find(p=>p.name==n)||{},i=inst.find(p=>p.name==n),c=cat[n]||{},app=LIB_APPS[n];
  const d=document.createElement('div');d.className='stc lib52c';
  d.innerHTML=`<div class='sti'></div><div style='flex:1;min-width:0'><b></b> <span class='natbadge'>Rudolf OS Library</span> <span class='msg lib52s'></span><div class='std'></div><div class='msg lib52l' style='font-size:12px'></div></div><div class='stbtn'></div>`;
  d.querySelector('.sti').textContent=a.icon||(i&&i.icon)||c.icon||'📦';d.querySelector('b').textContent=a.title||(i&&i.title)||c.title||n;
  d.querySelector('.std').textContent=c.description||a.summary||(i&&i.summary)||'';
  d.querySelector('.lib52l').textContent=[a.license||(i&&i.license)||c.license,'version '+((i&&i.version)||a.version)].filter(Boolean).join(' · ');
  d.querySelector('.lib52s').textContent=i?(i.disabled?'turned off':i.update?`update to ${i.update}`:'installed'):`${libSize(a.size||c.download||0)} download`;
  /* 1.5.9 (plan 8.2): an outside developer's app says who made it; one the Library revoked says why it is off */
  if(i&&i.signed=='developer'){const bd=d.querySelector('.natbadge');bd.textContent='by '+(i.developer||'a developer')+(i.github?' (github.com/'+i.github+')':'');bd.title='Checked and signed through the Rudolf OS Library';}
  if(i&&i.disabled){const w=document.createElement('div');w.className='msg lib52x';w.style.cssText='color:#b91c1c;font-size:12px';w.textContent='⚠ '+i.disabled.replace(/^./,x=>x.toUpperCase())+'. It no longer opens; remove it.';d.querySelector('.std').after(w)}
  const bx=d.querySelector('.stbtn'),btn=(l,f,pri)=>{const b=document.createElement('button');b.textContent=l;if(pri)b.className='pri';b.onclick=async()=>{if(busy)return;busy=true;B.querySelectorAll('.lib52c button').forEach(x=>x.disabled=true);log.hidden=false;log.textContent='';
   try{const ok=await f();if(!ok)log.textContent+='\nThat did not work.'}catch(e){log.textContent+='\n'+e.message}busy=false;libTab(B)};bx.appendChild(b)};
  const job=(what)=>pkgJob(`pkg/${what}?name=${encodeURIComponent(n)}`,l=>{log.textContent+=l+'\n';log.scrollTop=log.scrollHeight});
  if(!i)btn('Get',()=>job('install'),1);
  else{if(i.update)btn('Update',()=>job('install'),1);
   if(app&&typeof APPS!='undefined'&&APPS[app.app]){const o=document.createElement('button');o.textContent='Open';o.onclick=()=>launch(app.app);bx.appendChild(o)}
   else if(!app&&i.kind=='webapp'&&!i.disabled&&typeof pkgOpen=='function'){const o=document.createElement('button');o.textContent='Open';o.onclick=()=>pkgOpen(n);bx.appendChild(o)}
   btn('Remove',async()=>{if(!(await appAskDlg(`Remove ${a.title||i.title||n}?`,'It can be downloaded again from here at any time. Games and saves on drive C: and in the Games folder stay.','Remove')))return true;return job('remove')})}
  L.appendChild(d)}}
/* the tab, after Games (or Get apps) */
{const _as=appStore;appStore=function(){const W=_as();const tabs=W.body.querySelector('.tmtabs'),B=W.body.querySelector('.stb');if(!tabs||tabs.querySelector("[data-t='library']"))return W;
 const b=document.createElement('button');b.dataset.t='library';b.textContent='Rudolf OS Library';(tabs.querySelector("[data-t='games']")||tabs.querySelector("[data-t='get']")).after(b);
 const show=()=>{tabs.querySelectorAll('button').forEach(x=>x.classList.toggle('on',x===b));libTab(B)};b.onclick=show;W.showLibrary=show;return W}}
/* nobody loses an app: a computer with DS games in its Games folder gets Emu's DS player back after the update to
   1.5.2, once, in the background, when the Library can be reached */
async function libMigrate(){if(!RD)return;let st;try{st=JSON.parse(localStorage.getItem('bd95lib52')||'null')}catch(e){}if(st&&st.done)return;
 let items=[];try{items=(await rd('fs/list?path='+encodeURIComponent('/home/rudolf/Games/DS'))).items||[]}catch(e){}
 const used=items.some(it=>!it.dir&&/\.nds$/i.test(it.name));
 const save=v=>{try{localStorage.setItem('bd95lib52',JSON.stringify(v))}catch(e){}};
 if(!used){save({done:1});return}
 if(await libHas('rudolf-emu')){save({done:1});return}
 if(typeof netLive=='function'?!netLive():!navigator.onLine){typeof netWait=='function'?netWait().then(()=>setTimeout(libMigrate,15000)):setTimeout(libMigrate,600000);return}
 let ok=false;try{ok=await pkgJob('pkg/install?name=rudolf-emu')}catch(e){}
 if(ok){save({done:1});notify('🕹️ Emu','Emu’s Nintendo DS player now comes from the Rudolf OS Library. It was downloaded, so your DS games play as before.')}
 else setTimeout(libMigrate,1800000)}
if(RD)setTimeout(libMigrate,60000);
