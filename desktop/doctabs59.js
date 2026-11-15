/* ===== 2026-11-14: document tabs for Notepad and WordLite =====
   Asked for: Notepad and WordLite open documents (the Open dialog, filedlg59.js) and keep several in tabs under the
   title bar and buttons. docTabs(W,{app,get,set,types,defName,status,title}) puts the tab row after the window's
   buttons and keeps each tab's file and text; the editor (one textarea or one editable area) shows the chosen tab's.
   A file already open is switched to, not opened twice; a file opened from elsewhere (the File Explorer, Open with)
   goes into the open window as a new tab. Unsaved tabs show "•" and are asked about before they close.
   Keys: Ctrl+S save, Ctrl+Shift+S save as, Ctrl+O open, Ctrl+T new tab, Ctrl+W close the tab, Ctrl+Tab next tab. */
window.__doctabs59=1;
function docTabsFind(app){return wins.slice().reverse().find(w=>w.dtabs&&w.dtabs.app==app&&w.el&&w.el.isConnected)}
function docAsk(text,yes,no){return new Promise(res=>{const o=document.createElement('div');o.className='rmodal';
 o.innerHTML="<div class='rmbox' role='alertdialog'><p class='dqa'></p><div class='rmrow'><button class='n'></button><button class='pri y'></button></div></div>";
 o.querySelector('.dqa').textContent=text;o.querySelector('.y').textContent=yes;o.querySelector('.n').textContent=no;document.body.appendChild(o);typeof promptFront=='function'&&promptFront(o);
 const d=v=>{o.remove();res(v)};o.querySelector('.y').onclick=()=>d(true);o.querySelector('.n').onclick=()=>d(false);o.addEventListener('keydown',e=>{e.stopPropagation();if(e.key=='Escape')d(false)});o.querySelector('.y').focus()})}
function docTabs(W,o){
 const T={app:o.app,tabs:[],i:-1};W.dtabs=T;
 const bar=document.createElement('div');bar.className='dtabs';bar.setAttribute('role','tablist');
 bar.style.cssText='display:flex;gap:2px;align-items:flex-end;padding:3px 4px 0;overflow-x:auto;flex:0 0 auto;border-bottom:1px solid var(--bd)';
 const mb=W.body.querySelector('.menubar');mb?mb.after(bar):W.body.prepend(bar);
 const cur=()=>T.tabs[T.i];
 const keep=()=>{const t=cur();if(t)t.content=o.get()};
 const dirty=t=>t.content!==t.saved;
 const name=t=>t.p?t.p.at(-1):'Untitled';
 const title=()=>{const t=cur();W.setTitle((t?name(t):'Untitled')+' - '+o.title)};
 const draw=()=>{bar.innerHTML='';T.tabs.forEach((t,k)=>{const b=document.createElement('div');b.className='dtab'+(k==T.i?' on':'');b.setAttribute('role','tab');b.setAttribute('aria-selected',k==T.i);
   b.style.cssText='display:flex;align-items:center;gap:6px;max-width:170px;padding:3px 6px 3px 9px;border:1px solid var(--bd);border-bottom:0;border-radius:5px 5px 0 0;cursor:default;font-size:12px;'+(k==T.i?'background:var(--wbg,#fff);font-weight:600':'opacity:.8');
   const s=document.createElement('span');s.style.cssText='overflow:hidden;text-overflow:ellipsis;white-space:nowrap';s.textContent=(dirty(t)?'• ':'')+name(t);b.title=t.p?t.p.join('\\'):'Not saved yet';
   const x=document.createElement('u');x.textContent='✕';x.title='Close this tab';x.style.cssText='text-decoration:none;cursor:pointer;opacity:.7';
   b.append(s,x);b.onclick=e=>{if(e.target===x){closeTab(k);return}show(k)};b.onauxclick=e=>{if(e.button==1)closeTab(k)};bar.appendChild(b)});
  const pl=document.createElement('button');pl.textContent='+';pl.title='New tab (Ctrl+T)';pl.style.cssText='border:0;background:transparent;padding:2px 8px';pl.onclick=()=>newTab();bar.appendChild(pl);title()};
 const show=k=>{keep();T.i=k;const t=cur();o.set(t.content);o.status('');draw()};
 const add=(p,content)=>{keep();T.tabs.push({p,content,saved:content});T.i=T.tabs.length-1;o.set(content);o.status('');draw()};
 const newTab=()=>add(null,'');
 const open=p=>{if(!p)return;const k=T.tabs.findIndex(t=>t.p&&t.p.join('\\')==p.join('\\'));if(k>=0){show(k);return}const n=node(p);if(typeof n!='string')return o.status('That file cannot be opened here.');
  const t=cur();if(t&&!t.p&&!dirty(t)&&t.content===''){keep();t.p=p;t.content=t.saved=n;o.set(n);o.status('');draw();return}add(p,n)};
 const write=(t,p)=>{const d=dirOf(p);if(!isDir(d)){o.status('That folder no longer exists.');return false}d[p.at(-1)]=t.content;try{save()}catch(e){}t.p=p;t.saved=t.content;
  try{typeof sfx=='function'&&sfx('notify')}catch(e){}try{EXPL.forEach(f=>f())}catch(e){}o.status('Saved '+p.join('\\'));draw();return true};
 const saveAs=async()=>{keep();const t=cur();const p=await fileDlg({mode:'save',title:'Save as',name:t.p?t.p.at(-1):o.defName,types:o.types,start:t.p?t.p.slice(0,-1):null});if(!p)return false;
  if(o.convert)t.content=o.convert(t.content,p);return write(t,p)};
 const saveIt=async()=>{keep();const t=cur();if(!t.p)return saveAs();return write(t,t.p)};
 const openDlg=async()=>{const t=cur();const p=await fileDlg({mode:'open',title:'Open',types:o.types,start:t&&t.p?t.p.slice(0,-1):null});if(p)open(p)};
 const closeTab=async k=>{if(k!=T.i)show(k);keep();const t=cur();if(dirty(t)&&t.content!==''){const s=await docAsk(name(t)+' has changes that are not saved. Save them?','Save','Don\'t save');if(s&&!(await saveIt()))return}
  T.tabs.splice(T.i,1);if(!T.tabs.length){W.onclose=null;W.close();return}T.i=Math.min(T.i,T.tabs.length-1);o.set(cur().content);draw()};
 W.onclose=()=>{keep();const n=T.tabs.filter(t=>dirty(t)&&t.content!=='').length;if(!n)return;
  docAsk((n==1?'1 document has':n+' documents have')+' changes that are not saved. Close anyway?','Close','Keep open').then(y=>{if(y){W.onclose=null;W.close()}});return false};
 W.body.addEventListener('keydown',e=>{if(!(e.ctrlKey||e.metaKey))return;const k=e.key.toLowerCase();
  if(k=='s'){e.preventDefault();e.shiftKey?saveAs():saveIt()}else if(k=='o'){e.preventDefault();openDlg()}else if(k=='t'){e.preventDefault();newTab()}else if(k=='w'){e.preventDefault();closeTab(T.i)}
  else if(e.key=='Tab'&&T.tabs.length>1){e.preventDefault();show((T.i+(e.shiftKey?T.tabs.length-1:1))%T.tabs.length)}});
 Object.assign(T,{open,newTab,saveIt,saveAs,openDlg,closeTab,cur,refresh:()=>{keep();draw()}});
 return T}
