/* ===== Rudolf OS 1.5.6: every app in the Start menu has Open, Pin to taskbar, Add to desktop and Properties =====
   (asked for on 2026-11-10: Linux apps and Windows programs had no right-click menu there, Emu only Open). Properties
   shows that app's extended information, for whatever kind of app it is: built into Rudolf OS, a Linux app from
   Flathub, a Linux program or Windows program in Debian (Wine), an app installed from the App Store's own packages
   (a web app), or one of the Linux apps that come with Rudolf OS (Sober). Two tabs, as a desktop shortcut's
   Properties has: General (what it is, where it is, its windows, pinned, on the desktop, the buttons) and Details
   (its ID, version, publisher, size, what it opens, and how Rudolf OS knows its windows). */
function appKind(k){const A=APPS[k]||{};
 if(k.startsWith('lx:deb-'))return{t:'deb',name:'Debian program'};
 if(k.startsWith('lx:'))return{t:'flat',name:'Linux app (Flathub)'};
 if(k.startsWith('native:'))return{t:'native',name:'Linux app (comes with Rudolf OS)'};
 if(A.rapp)return{t:'rapp',name:'Installed app (App Store)'};
 if(A.pkg)return{t:'pkg',name:'Installed package'};
 return{t:'builtin',name:'Built into Rudolf OS'}}
/* what is known of an app: its launcher (Linux), its Flathub entry, Debian's entry, its package */
function appInfo(k){const A=APPS[k]||{},K=appKind(k),o={k,A,K};
 if(K.t=='flat'||K.t=='deb'){o.x=lxLaunchers().find(x=>'lx:'+x.desktop==k)||null;
  if(K.t=='flat')o.f=(LX.inst||[]).find(a=>o.x&&a.id==o.x.app)||null;
  else o.d=(typeof DEB=='object'&&(DEB.apps.length?DEB.apps:ld(DEB_AK,[])).find(a=>o.x&&a.id==o.x.deb))||null;
  if(K.t=='deb'&&o.d&&/\.exe$/i.test(o.d.wmclass||''))o.K={t:'deb',name:'Windows program (Debian, Wine)'}}
 if(K.t=='native')o.n=(typeof NATIVE!='undefined'&&NATIVE.find(n=>'native:'+n.k==k))||null;
 if(K.t=='rapp')o.r=(typeof rappList=='function'&&rappList()[k.slice(5)])||null;
 return o}
/* 1.5.6 (asked for on 2026-11-10): where the app is (an address File Explorer opens), how much it takes on the disk,
   and the files it uses (their extensions) */
const APPEXT={notepad:['.txt','any text file'],code:['.js','.lua','.py','.html','.css','.json','.md','.txt'],wordpro:['.doc','.txt','.rtf'],wordlite:['.txt'],
 paint:['.png','.jpg','.gif','.webp','.bmp'],viewer:['.png','.jpg','.gif','.webp','.svg','.bmp'],pdf:['.pdf'],media:['.mp3','.ogg','.wav','.flac','.m4a','.mp4','.webm','.mkv','.mov'],
 sheet:['.csv','.tsv','.xlsx','.xlsm','.rsheet'],deck:['.pptx','.rdeck'],archive:['.zip','.tar'],browser:['.html','.htm','.svg'],prompt:['.lua','.bat','.wasm'],
 fonts:['.ttf','.otf','.woff','.woff2'],gameboy:['.gb','.gbc','.gba','.sfc','.smc','.nds'],flash:['.swf'],blocklua:['.lua'],store:['.rpk']};
function appExts(I){const x=I.x||{},m=(x.mime||'').split(';').filter(Boolean);if(!m.length)return APPEXT[I.k]||[];
 const inv={};Object.entries(typeof LX_MIME=='object'?LX_MIME:{}).forEach(([e,t])=>{(inv[t]=inv[t]||[]).push('.'+e)});
 Object.entries(typeof LX_MIME_ALIAS=='object'?LX_MIME_ALIAS:{}).forEach(([t,al])=>al.forEach(a=>{if(inv[t])inv[a]=inv[t]}));
 const out=[];m.forEach(t=>{if(t.startsWith('x-scheme-handler/'))out.push(t.slice(17)+': links');else(inv[t]||[]).forEach(e=>out.includes(e)||out.push(e))});return out}
function appWhere(I){const K=I.K.t,x=I.x||{};
 if(K=='flat'||K=='native'){const id=K=='flat'?x.app:(I.n||{}).id;return id?lxApi('apps/where?id='+encodeURIComponent(id)).catch(e=>({error:e.message})):Promise.resolve({})}
 if(K=='deb')return lxApi('debian/where?id='+encodeURIComponent(x.deb)).catch(e=>({error:e.message}));
 if(K=='builtin'){const n=(performance.getEntriesByType('navigation')[0]||{}).decodedBodySize||0;return Promise.resolve({path:'/usr/share/rudolf/www',bytes:n,shared:1})}
 if(K=='rapp')return Promise.resolve({bytes:new Blob([JSON.stringify(I.r||{})]).size,store:1});
 return Promise.resolve({})}
function explorerAt(pth){explorer(homePath());const W=wins.at(-1);if(W&&W.openRemote)W.openRemote({dev:'path',root:pth,path:pth,label:pth.split('/').pop(),ro:true})}
function appProps(k){const I=appInfo(k),A=I.A;if(!APPS[k])return;
 const W=win(A.name+' Properties','ℹ️',460,560);W.app=null;W.body.classList.add('apw');
 W.body.innerHTML=`<div class='aptabs' role='tablist'></div><div class='appane'></div><div class='apfoot'><button class='ok'>Close</button></div>`;
 const tabs=W.body.querySelector('.aptabs'),pane=W.body.querySelector('.appane');W.body.querySelector('.ok').onclick=()=>W.close();
 const row=(t,l,v)=>{if(v==null||v==='')return;const tr=document.createElement('tr');tr.innerHTML='<td></td><td></td>';tr.firstChild.textContent=l;tr.lastChild.textContent=v;t.appendChild(tr)};
 const tbl=()=>{const t=document.createElement('table');t.className='ptab aptab';pane.appendChild(t);return t};
 const sep=()=>{const h=document.createElement('hr');h.className='apsep';pane.appendChild(h)};
 const head=()=>{const h=document.createElement('div');h.className='aphead';const p=document.createElement('div');p.className='apic';p.style.cssText='font-size:36px;width:44px;height:44px';
  p.textContent=A.icon||'🔗';if(A.img)try{A.img(p)}catch(e){}else{const im=typeof icImg=='function'&&S.icons!=='emoji'&&icImg(A.icon||'');if(im){p.textContent='';im.style.cssText='width:36px;height:36px';p.appendChild(im)}}
  h.appendChild(p);const n=document.createElement('div');n.innerHTML='<b></b><div class="apsub"></div>';n.querySelector('b').textContent=A.name;n.querySelector('.apsub').textContent=I.K.name;h.appendChild(n);pane.appendChild(h);
  setTimeout(()=>{const i=p.querySelector('img');if(i)i.style.cssText='width:36px;height:36px'},400)};
 const desc=()=>{const x=I.x||{},c=x.comment&&x.comment!='Debian'?x.comment:'';return APPDESC[k]||(I.n&&I.n.desc)||(I.r&&I.r.description)||c||(I.K.t=='deb'?(/Wine/.test(I.K.name)?'A Windows program, run through Wine in Linux (Debian).':'A program installed in Linux (Debian).'):'')||'—'};
 const pinned=()=>pinList().includes(k),onDesk=()=>typeof deskHasApp=='function'&&deskHasApp(k);
 const pages={
  General(){head();sep();const t=tbl();const acts=document.createElement('div');acts.className='pact apacts';
   row(t,'Type',I.K.name);row(t,'Description',desc());
   row(t,'Location',I.K.t=='builtin'?'Rudolf OS (built in)':I.K.t=='flat'?'Linux apps (/var/lib/flatpak'+(I.f&&I.f.origin?', from '+I.f.origin:'')+')':I.K.t=='deb'?'Linux (Debian)':I.K.t=='native'?'Linux apps (comes with Rudolf OS)':'This computer (installed from the App Store)');
   /* the address, the size and the files: filled in when known (rudolfd measures the app's folder) */
   const ad=document.createElement('tr');ad.innerHTML="<td>Address</td><td class='apwhere'>…</td>";t.appendChild(ad);
   const sz=document.createElement('tr');sz.innerHTML="<td>Size on disk</td><td class='apsize'>…</td>";t.appendChild(sz);
   const ex=appExts(I);row(t,'File types',ex.length?ex.join(', '):'None (it opens no files)');
   (I.where=I.where||appWhere(I)).then(w=>{if(!ad.isConnected)return;const a=ad.lastChild,z=sz.lastChild;
    if(w.error){a.textContent=w.error;z.textContent='—';return}
    if(w.store){a.textContent="Rudolf OS's own storage (it has no folder)";z.textContent=fmtB(w.bytes)||'—';return}
    if(!w.path){a.textContent='—';z.textContent='—';return}
    a.textContent=w.path;a.title='Type it in File Explorer\'s address bar, or press Open in File Explorer';
    z.textContent=w.shared?`Part of Rudolf OS's desktop (${fmtB(w.bytes)}, shared by all its built-in apps)`:(fmtB(w.bytes)||'—');
    if(w.program||w.package){const r2=document.createElement('tr');r2.innerHTML='<td></td><td></td>';r2.firstChild.textContent=w.program?'Program file':'Installed from';r2.lastChild.textContent=w.program||w.package.replace(/^(.*?)(\.[a-z]+)$/,'$1 ($2)');sz.after(r2)}
    if(RD&&w.path&&acts&&!acts.querySelector('.apfe')){const b=document.createElement('button');b.className='apfe';b.textContent='Open in File Explorer';b.onclick=()=>{W.close();explorerAt(w.path)};acts.appendChild(b)}});
   const n=wins.filter(o=>o.app==k).length;row(t,'Open windows',n?String(n):'None');
   row(t,'Pinned to taskbar',pinned()?'Yes':'No');row(t,'On the desktop',onDesk()?'Yes':'No');
   pane.appendChild(acts);
   const btn=(l,f)=>{const b=document.createElement('button');b.textContent=l;b.onclick=f;acts.appendChild(b);return b};
   btn('Open',()=>{W.close();launch(k)});
   btn(pinned()?'Unpin from taskbar':'Pin to taskbar',()=>{pinSet(k,!pinned());show('General')});
   if(typeof deskAdd=='function')btn(onDesk()?'Remove from desktop':'Add to desktop',()=>{if(onDesk()){const d=deskIconFor(k);d&&deskRemove(d)}else deskAdd(k);show('General')});
   if(A.lxUninstall)btn('Uninstall…',()=>{W.close();A.lxUninstall()})},
  Details(){const t=tbl();t.classList.add('apdet');t.innerHTML='<tr><th>Property</th><th>Value</th></tr>';
   const x=I.x||{},f=I.f||{},d=I.d||{},nat=I.n||{},r=I.r||{};
   row(t,'Name',A.name);row(t,'Type',I.K.name);
   row(t,'App ID',I.K.t=='flat'?x.app:I.K.t=='deb'?d.id||x.deb:I.K.t=='native'?nat.id:I.K.t=='rapp'?r.id:k);
   row(t,'Version',I.K.t=='builtin'?'Rudolf OS '+RVER:I.K.t=='flat'?f.version:I.K.t=='rapp'?r.version:I.K.t=='native'?'From Flathub':I.K.t=='deb'?'Installed in Debian':'');
   if(I.K.t=='builtin')row(t,'Released',RVERDATE);
   row(t,'Publisher',I.K.t=='builtin'?'Rudolf OS':I.K.t=='flat'?(f.origin=='flathub'?'Flathub':f.origin):I.K.t=='native'?(nat.id=='org.vinegarhq.Sober'?'Vinegar team (Sober)':'Flathub'):I.K.t=='rapp'?r.author||r.publisher:'');
   row(t,'Size',f.size);
   row(t,'Built in',I.K.t=='builtin'?'Yes':'No (installed)');
   row(t,'Launcher',I.K.t=='flat'||I.K.t=='deb'?x.desktop:'');
   row(t,'Categories',(x.cats||d.categories||'').replace(/;+$/,'').replace(/;/g,', '));
   row(t,'Opens',(x.mime||'').replace(/;+$/,'').split(';').filter(Boolean).join(', '));
   row(t,'Window class',x.wmclass||d.wmclass);
   row(t,'Icon',I.K.t=='deb'?d.icon||x.icon:I.K.t=='flat'?x.icon:'');
   (APPEXTRA[k]||[]).forEach(([l,v])=>row(t,l,v));
   const key=(apGet()[k]||{}).key;row(t,'Shortcut key',keyName(key));
   row(t,'Start menu name',k)}};
 const show=n=>{pane.innerHTML='';tabs.querySelectorAll('button').forEach(b=>b.classList.toggle('on',b.textContent==n));pages[n]()};
 Object.keys(pages).forEach(n=>{const b=document.createElement('button');b.textContent=n;b.setAttribute('role','tab');b.onclick=()=>show(n);tabs.appendChild(b)});
 show('General');return W}
/* the Start menu's right-click menus: Properties at the end of every app's menu (the menus are built in apps25a and
   shell25; the menu shown is completed as it opens) */
{const st=document.createElement('style');st.textContent='.apwhere{word-break:break-all}';document.head.appendChild(st)}
(function(){const o=cmenu;cmenu=function(x,y,items,opt){const k=cmenu.next;cmenu.next=null;
 if(k&&APPS[k]&&Array.isArray(items)&&!items.some(i=>i&&i[0]=='Properties')){items=items.concat([['Properties',()=>{$('#menu').style.display='none';appProps(k)},true,{icon:'ℹ️'}]])}
 return o.call(this,x,y,items,opt)}})();
/* on the document, before the menu's own handlers (they stop the event from going further) */
document.addEventListener('contextmenu',e=>{const li=e.target.closest&&e.target.closest('#menu li[data-v25],#menu li[data-a]');if(!li){cmenu.next=null;return}
 const MA={gbplayer:'gameboy'},a=li.dataset.v25||li.dataset.a;cmenu.next=APPS[a]?a:APPS[MA[a]]?MA[a]:null},true);
