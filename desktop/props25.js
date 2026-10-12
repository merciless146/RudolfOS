/* ===== Desktop icons: no native image dragging =====
   Desktop icons are moved by their own pointer handler. The icon picture is an <img>, so Chromium also
   started its built-in image drag, and dropping it on the desktop went through the "files dropped from
   another program" path and saved the picture as DOWNLOAD.SVG. Drags that start inside Rudolf OS are never
   treated as files from outside. */
let PAGEDRAG=false;
document.addEventListener('dragstart',e=>{const t=e.target,el=t&&t.nodeType==1?t:t&&t.parentElement;
 if(el&&el.closest&&el.closest('#desk > .ic,#bar,#menu,.cmenu,.wg')){e.preventDefault();return}PAGEDRAG=true},true);
document.addEventListener('dragend',()=>{PAGEDRAG=false},true);
const pageDragFiles=e=>PAGEDRAG&&!FC.drag&&e.dataTransfer&&[...e.dataTransfer.types].includes('Files');
document.addEventListener('dragover',e=>{if(pageDragFiles(e)){e.stopPropagation();e.dataTransfer.dropEffect='none'}},true);
document.addEventListener('drop',e=>{if(pageDragFiles(e)){e.preventDefault();e.stopPropagation();PAGEDRAG=false}},true);

/* ===== Properties for desktop app shortcuts: General, Shortcut and Details tabs, like Windows ===== */
const APPDESC={explorer:'Browse, open, copy and move the files and folders on this computer.',browser:'Visit web pages, with tabs, bookmarks and a list of downloads.',
 notepad:'Write and edit plain text files.',prompt:'Type commands to work with files, folders and the system.',blocklua:'Build programs by snapping blocks together and see them as Lua code.',
 calc:'Do sums, from everyday arithmetic to scientific functions.',wordlite:'Write letters, notes and simple documents.',wordpro:'Write longer documents with styles, tables and pictures.',
 paint:'Draw pictures and edit images.',boards:'Play board games such as Checkers and Chess.',poker:'Play card games.',galaxy:'Explore an interactive map of space.',
 media:'Play music and videos.',trash:'Holds deleted files until you restore them or empty it.',settings:'Change how Rudolf OS looks, sounds and works.',
 gameboy:'Play Game Boy, Game Boy Color, Game Boy Advance, Super Nintendo and Nintendo DS games that you own.',flash:'Play Flash (SWF) games and animations, using the Ruffle player.',
 quake:'Play Quake with game data that you own.',gpulab:'Test and show what the graphics processor can do.',taskmgr:'See running apps and how hard the computer is working, and close apps that stop responding.',
 viewer:'Look at pictures and photos.',pdf:'Read PDF documents.',capture:'Take pictures of the screen.',code:'Write and run code, with syntax colouring.',archive:'Open and make ZIP archives.',
 store:'Find and install more apps.',calendar:'Keep track of dates and events.',clock:'Alarms, timers, a stopwatch and world clocks.',sheet:'Work with tables of numbers and formulas.',
 deck:'Make slide presentations.',contacts:'Keep names, phone numbers and addresses.',weather:'See the weather forecast.',charmap:'Find and copy special characters and emoji.',
 events:'See what the system has recorded, such as errors and warnings.',camera:'Take photos and videos with a camera.',recorder:'Record sound from a microphone.',
 fonts:'Look at the fonts on this computer.',devcon:'Tools for people who build web pages and apps.',help:'Answers to questions about Rudolf OS.'};
const APPEXTRA={flash:[['Uses','Ruffle, an open-source Flash Player emulator']],quake:[['License','GNU GPL (the engine; game data is not included)']]};
const apK=()=>'bd95appprefs:'+uid();
const apGet=()=>ld(apK(),{});
const apSave=p=>svj(apK(),p);
/* Which app (or other target) a desktop icon stands for */
function icTarget(d){const name=d.querySelector('span').textContent.trim();
 if(d.classList.contains('fic'))return{kind:'file',name,path:homePath('DESKTOP').concat(d.dataset.fn)};
 if(d.classList.contains('tr'))return{kind:'trash',k:'trash',name};
 if(d.dataset.mm)return{kind:'app',k:d.dataset.mm=='gb'?'gameboy':'flash',name};
 if(name=='My Computer')return{kind:'computer',k:'explorer',name};
 if(name=='Programs')return{kind:'folder',k:'explorer',name,path:['C:','PROGRAMS']};
 const alias={'Oracle':'browser','BlockLua':'blocklua'};let k=alias[name]||Object.keys(APPS).find(x=>APPS[x].name==name)||appOfTitle(name);
 if(d.classList.contains('qic'))k='quake';
 return{kind:k&&APPS[k]?'app':'other',k,name}}
const icPic=(d,px)=>{const b=document.createElement('div');b.className='apic';b.style.cssText=`font-size:${px}px;width:${px+8}px;height:${px+8}px`;
 [...d.querySelector('b').childNodes].forEach(n=>{const c=n.cloneNode(true);if(c.tagName=='IMG'){c.removeAttribute('class');c.style.cssText=`width:${px}px;height:${px}px`}b.appendChild(c)});return b};
const keyName=c=>c?'Ctrl + Alt + '+c:'None';
function iconProps(d){
 const sel=deskIcons().filter(i=>i.classList.contains('sel'));
 if(sel.length>1&&sel.includes(d))return multiProps(sel);
 const T=icTarget(d);if(T.kind=='file')return window.fileProps&&fileProps(T.path);
 const A=T.k&&APPS[T.k],W=win(T.name+' Properties','ℹ️',420,470);W.app=null;
 const P=apGet(),mine=Object.assign({run:'normal',key:'',note:''},P[T.k]||{}),cur=Object.assign({},mine);
 W.body.classList.add('apw');
 W.body.innerHTML=`<div class='aptabs' role='tablist'></div><div class='appane'></div><div class='apfoot'><button class='ok'>OK</button><button class='cn'>Cancel</button><button class='ap' disabled>Apply</button></div>`;
 const tabs=W.body.querySelector('.aptabs'),pane=W.body.querySelector('.appane'),bApply=W.body.querySelector('.ap');
 const dirty=()=>{bApply.disabled=JSON.stringify(cur)==JSON.stringify(mine)&&pinNow==pinWas};
 const pinWas=!!(A&&pinList().includes(T.k));let pinNow=pinWas;
 const row=(t,l,v,html)=>{const tr=document.createElement('tr');tr.innerHTML='<td></td><td></td>';tr.firstChild.textContent=l;if(html)tr.lastChild.innerHTML=v;else tr.lastChild.textContent=v;t.appendChild(tr);return tr.lastChild};
 const head=()=>{const h=document.createElement('div');h.className='aphead';h.appendChild(icPic(d,36));const n=document.createElement('div');n.innerHTML='<b></b><div class="apsub"></div>';n.querySelector('b').textContent=T.name;n.querySelector('.apsub').textContent=typeName();h.appendChild(n);return h};
 const typeName=()=>T.kind=='computer'?'System':T.kind=='folder'?'Shortcut to a folder':T.kind=='trash'?'System folder':A&&A.k.startsWith('native:')?'Linux app (Flatpak)':'Shortcut to an app';
 const runningN=()=>T.kind=='computer'||T.kind=='folder'?0:wins.filter(o=>o.app==T.k).length;
 const tbl=()=>{const t=document.createElement('table');t.className='ptab aptab';return t};
 const sep=()=>{const h=document.createElement('hr');h.className='apsep';return h};
 const pages={
  General(){pane.appendChild(head());pane.appendChild(sep());const t=tbl();pane.appendChild(t);
   row(t,'Type',typeName());
   if(T.kind=='computer'){row(t,'Description','This computer: its drives, folders and system information.');sysRows(t)}
   else if(T.kind=='folder'){row(t,'Description','Programs installed on this computer.');row(t,'Target',T.path.join('\\'))}
   else row(t,'Description',APPDESC[T.k]||(A&&A.k.startsWith('native:')?(NATIVE.find(n=>'native:'+n.k==A.k)||{}).desc:'')||'—');
   row(t,'Location','Desktop ('+homePath('DESKTOP').join('\\')+')');
   if(T.kind=='trash'){const n=(window.TR||[]).length,sz=(window.TR||[]).reduce((s,e)=>s+Math.max(0,sizeOf(e.v)),0);row(t,'Contains',n?`${n} item${n==1?'':'s'} (${fmtB(sz)||'0 bytes'})`:'Empty')}
   if(A&&T.kind!='computer'&&T.kind!='folder'){const n=runningN();row(t,'Open windows',n?String(n):'None')}
   if(A&&T.kind=='app'||T.kind=='trash'){pane.appendChild(sep());const l=document.createElement('label');l.className='apchk';l.innerHTML='<input type="checkbox"> Pin to taskbar';const c=l.firstChild;c.checked=pinNow;c.onchange=()=>{pinNow=c.checked;dirty()};pane.appendChild(l)}
   const acts=document.createElement('div');acts.className='pact apacts';pane.appendChild(acts);
   const btn=(l,f)=>{const b=document.createElement('button');b.textContent=l;b.onclick=f;acts.appendChild(b);return b};
   btn('Open',()=>openIcon(d));
   if(T.kind=='folder')btn('Open file location',()=>explorer(T.path.slice(0,-1)));
   if(T.kind=='trash'){const b=btn('Empty Trash…',()=>{W.close();trash();setTimeout(()=>{const x=[...document.querySelectorAll('.win button')].find(b=>/Empty/i.test(b.textContent));x&&x.click()},50)});b.disabled=!(window.TR&&TR.length)}},
  Shortcut(){pane.appendChild(head());pane.appendChild(sep());const t=tbl();pane.appendChild(t);
   row(t,'Target type',T.kind=='folder'?'Folder':T.kind=='computer'||T.kind=='trash'?'System folder':'Application');
   row(t,'Target',T.kind=='folder'?T.path.join('\\'):T.kind=='computer'?'This computer':'app:'+T.k);
   row(t,'Start in',homePath('DOCUMENTS').join('\\'));
   const kc=row(t,'Shortcut key','<input class="apkey" readonly aria-label="Shortcut key. Press a letter or number; Backspace clears it.">',true).firstChild;
   const kn=document.createElement('div');kn.className='apnote';kc.after(kn);
   const show=()=>{kc.value=keyName(cur.key);const o=cur.key&&Object.entries(apGet()).find(([k,v])=>k!=T.k&&v.key==cur.key);kn.textContent=o?`${keyName(cur.key)} is used by ${(APPS[o[0]]||{}).name||o[0]}. Applying moves it here.`:'Click the box and press a letter or number. Backspace clears it.'};show();
   kc.onkeydown=e=>{if(e.key=='Tab')return;e.preventDefault();e.stopPropagation();if(e.key=='Backspace'||e.key=='Delete'||e.key=='Escape')cur.key='';else if(/^[a-z0-9]$/i.test(e.key))cur.key=e.key.toUpperCase();else return;show();dirty()};
   const rs=row(t,'Run',`<select class="aprun"><option value="normal">Normal window</option><option value="min">Minimised</option><option value="max">Maximised</option></select>`,true).firstChild;
   rs.value=cur.run;rs.onchange=()=>{cur.run=rs.value;dirty()};
   const cm=row(t,'Comment','<input class="apcm" maxlength="120">',true).firstChild;cm.value=cur.note;cm.placeholder=APPDESC[T.k]||'';cm.oninput=()=>{cur.note=cm.value;dirty()};
   if(T.kind=='folder'){const a=document.createElement('div');a.className='pact apacts';const b=document.createElement('button');b.textContent='Open file location';b.onclick=()=>explorer(T.path.slice(0,-1));a.appendChild(b);pane.appendChild(a)}},
  Details(){const t=tbl();t.classList.add('apdet');pane.appendChild(t);t.innerHTML='<tr><th>Property</th><th>Value</th></tr>';
   row(t,'Name',T.name);row(t,'Type',typeName());
   if(T.kind=='computer')sysRows(t);
   else{const nat=A&&A.k.startsWith('native:')&&NATIVE.find(n=>'native:'+n.k==A.k);
    row(t,'App',A?A.name:'—');row(t,'App ID',nat?nat.id:T.k||'—');
    row(t,'Version',nat?'From Flathub':'Rudolf OS '+RVER);row(t,'Released',nat?'—':RVERDATE);
    row(t,'Publisher',nat?(nat.id=='org.vinegarhq.Sober'?'Vinegar team (Sober)':'Flathub'):'Rudolf OS');
    row(t,'Built in',nat?'No (installed from the App Store)':'Yes');(APPEXTRA[T.k]||[]).forEach(([l,v])=>row(t,l,v))}
   row(t,'Shortcut key',keyName(mine.key));row(t,'Runs as',{normal:'Normal window',min:'Minimised',max:'Maximised'}[mine.run]);
   row(t,'Pinned to taskbar',A?(pinWas?'Yes':'No'):'—');row(t,'Position on desktop',`${Math.round(d.offsetLeft)}, ${Math.round(d.offsetTop)}`)}};
 function sysRows(t){
  row(t,'Rudolf OS','Version '+RVER+' ('+RVERDATE+')');
  const cpu=row(t,'Processor',(navigator.hardwareConcurrency||'?')+' threads');const mem=row(t,'Memory',navigator.deviceMemory?'About '+navigator.deviceMemory+' GB':'—');
  const st=row(t,'Storage','Checking…');const mode=row(t,'Running',RD?'Checking…':'In a web browser');
  if(navigator.storage&&navigator.storage.estimate&&!RD)navigator.storage.estimate().then(e=>{st.textContent=`${fmtB(e.usage||0)} used of ${fmtB(e.quota||0)} available to Rudolf OS`}).catch(()=>st.textContent='—');
  if(RD)rd('info').then(i=>{const dev=[i.vendor,i.product].filter(Boolean).join(' ');if(dev)row(t,'Device',dev);cpu.textContent=(i.cpu||'Processor')+' · '+(i.threads||navigator.hardwareConcurrency)+' threads';if(i.memTotal)mem.textContent=fmtB(i.memTotal);
   mode.textContent=(i.live?'Live from '+(i.media=='disk'?'a USB drive':'a disc'):'Installed on this computer')+(i.boot?' · '+String(i.boot).toUpperCase():'');if(i.kernel)row(t,'Kernel',i.kernel);
   if(i.uptime){const h=Math.floor(i.uptime/3600),m=Math.floor(i.uptime%3600/60);row(t,'Up time',h?`${h} h ${m} min`:`${m} min`)}}).catch(()=>{mode.textContent='Installed system'});
  if(RD)rd('disks').then(r=>{const l=(r.disks||r||[]);st.textContent=Array.isArray(l)&&l.length?l.length+' drive'+(l.length==1?'':'s')+' (see File Explorer)':'—'}).catch(()=>st.textContent='—')}
 let curTab='General';
 const draw=()=>{tabs.innerHTML='';Object.keys(pages).forEach(n=>{if(n=='Shortcut'&&T.kind=='other')return;const b=document.createElement('button');b.textContent=n;b.setAttribute('role','tab');b.setAttribute('aria-selected',n==curTab);if(n==curTab)b.className='on';b.onclick=()=>{curTab=n;draw()};tabs.appendChild(b)});pane.innerHTML='';pages[curTab]()};
 const apply=()=>{if(A||T.kind!='other'){const p=apGet();if(cur.key)Object.keys(p).forEach(k=>{if(k!=T.k&&p[k].key==cur.key)p[k].key=''});
   if(cur.run=='normal'&&!cur.key&&!cur.note)delete p[T.k];else p[T.k]=Object.assign({},cur);apSave(p);Object.assign(mine,cur)}
  if(A&&pinNow!=pinWas)pinSet(T.k,pinNow);dirty();icNotes()};
 W.body.querySelector('.ok').onclick=()=>{apply();W.close()};W.body.querySelector('.cn').onclick=()=>W.close();bApply.onclick=()=>{apply();if(curTab=='Details')draw()};
 draw();return W}
function multiProps(sel){const W=win(sel.length+' items Properties','ℹ️',380,380);W.app=null;W.body.classList.add('apw');
 W.body.innerHTML=`<div class='appane'><div class='aphead'><div class='apic' style='font-size:36px;width:44px;height:44px'>🗂️</div><div><b></b><div class='apsub'></div></div></div><hr class='apsep'><table class='ptab aptab'></table></div><div class='apfoot'><button class='ok'>OK</button></div>`;
 const ts=sel.map(icTarget),apps=ts.filter(t=>t.kind=='app').length,files=ts.filter(t=>t.kind=='file').length,other=ts.length-apps-files;
 W.body.querySelector('b').textContent=sel.length+' items selected';W.body.querySelector('.apsub').textContent=[apps&&apps+' app shortcut'+(apps>1?'s':''),files&&files+' file'+(files>1?'s':''),other&&other+' system item'+(other>1?'s':'')].filter(Boolean).join(', ');
 const t=W.body.querySelector('table');ts.forEach((x,i)=>{const tr=document.createElement('tr');tr.innerHTML='<td></td><td></td>';tr.firstChild.appendChild(icPic(sel[i],18));tr.lastChild.textContent=x.name;t.appendChild(tr)});
 W.body.querySelector('.ok').onclick=()=>W.close();return W}
/* Comments become the icon's tooltip */
function icNotes(){if(!USER)return;const p=apGet();deskIcons().forEach(d=>{if(d.classList.contains('fic'))return;const T=icTarget(d),n=T.k&&p[T.k]&&p[T.k].note;if(!d.dataset.t0)d.dataset.t0=d.title||'-';d.title=n||(d.dataset.t0=='-'?'':d.dataset.t0)})}
DESK.addEventListener('pointerover',e=>{if(e.target.closest&&e.target.closest('#desk > .ic:not(.fic)')&&!DESK.dataset.notes){DESK.dataset.notes=1;icNotes();setTimeout(()=>delete DESK.dataset.notes,4000)}});
/* Run mode: open the app's window minimised or maximised */
{const _w2=win;win=function(...a){const W=_w2.apply(this,a);setTimeout(()=>{try{const m=USER&&W.app&&(apGet()[W.app]||{}).run;if(m=='max'&&!W.el.classList.contains('max'))W.el.querySelector('.mx').click();else if(m=='min'&&!W.min)W.el.querySelector('.mn').click()}catch(e){}},0);return W}}
/* Shortcut keys: Ctrl+Alt+letter or number opens the app */
document.addEventListener('keydown',e=>{if(!USER||window.LOCKED||!e.ctrlKey||!e.altKey||e.shiftKey||e.metaKey||e.repeat)return;if(e.getModifierState&&e.getModifierState('AltGraph'))return;
 const c=(/^Key([A-Z])$/.exec(e.code)||/^Digit([0-9])$/.exec(e.code)||[])[1];if(!c)return;const hit=Object.entries(apGet()).find(([k,v])=>v.key==c&&APPS[k]);if(!hit)return;e.preventDefault();e.stopPropagation();launch(hit[0])},true);
