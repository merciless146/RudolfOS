/* ===== 2026-11-14: the Save and Open dialogs of Notepad, WordLite and WordPro =====
   Asked for: saving opens a dialog like the File Explorer: browse the folders, type a name, and choose the file type
   from a list; opening works the same way. fileDlg({mode:'save'|'open', title, name, types:[[label,[EXT,...]],...],
   start}) answers the chosen path (['C:',...]) or null. Folders open with a click (or Enter); a file's click puts its
   name in the box; the type list filters what is shown, and in a save puts its extension on the name (the first of the
   type's), replacing another type's; a name typed with an extension of its own keeps it; an existing file is replaced only after asking. Names are checked as the File
   Explorer checks them. Everything is drawn as text (never HTML from a file's name). */
window.__filedlg59=1;
function fileDlgStart(start){let c=Array.isArray(start)&&start.length?start.slice():(USER&&typeof homePath=='function'?homePath('DOCUMENTS'):['C:']);
 while(c.length>1&&!isDir(node(c)))c=c.slice(0,-1);return isDir(node(c))?c:['C:']}
function fileDlg(o){o=o||{};const save=o.mode=='save',types=(o.types&&o.types.length?o.types:[]).concat([['All files',['*']]]);
 return new Promise(res=>{const ov=document.createElement('div');ov.className='rmodal fdlg';
 ov.innerHTML=`<div class='rmbox' role='dialog' style='max-width:620px;width:94vw;display:flex;flex-direction:column;gap:6px'><h3 class='fdt'></h3>
<div style='display:flex;gap:4px;align-items:center'><button class='fdup' title='Up one folder'>⬆</button><button class='fdhome' title='Your Documents'>🏠</button><input class='fdpath' readonly style='flex:1;min-width:0' aria-label='Folder'>${save?"<button class='fdnew' title='New folder'>📁+</button>":''}</div>
<div class='fdl' role='listbox' tabindex='0' style='height:240px;overflow:auto;border:1px solid var(--bd);background:var(--wbg,#fff);border-radius:3px'></div>
<div class='r2' style='grid-template-columns:auto 1fr;gap:6px 10px'><label>File name</label><input class='fdname' spellcheck='false' autocomplete='off'><label>Type</label><select class='fdtype'></select></div>
<p class='msg fderr' style='color:#b91c1c;min-height:1em;margin:0'></p>
<div class='rmrow'><button class='fdx'>Cancel</button><button class='pri fdok'></button></div></div>`;
 document.body.appendChild(ov);typeof promptFront=='function'&&promptFront(ov);
 const $=s=>ov.querySelector(s),nm=$('.fdname'),ty=$('.fdtype'),L=$('.fdl'),er=t=>{$('.fderr').textContent=t||''};
 $('.fdt').textContent=o.title||(save?'Save as':'Open');$('.fdok').textContent=save?'Save':'Open';
 types.forEach((t,i)=>{const op=document.createElement('option');op.value=i;op.textContent=t[0]+(t[1][0]=='*'?'':' ('+t[1].map(e=>'.'+e.toLowerCase()).join(', ')+')');ty.appendChild(op)});
 let cur=fileDlgStart(o.start);const T=()=>types[+ty.value]||types[0];
 const match=n=>{const e=T()[1];if(e[0]=='*')return true;const x=(n.split('.').pop()||'').toUpperCase();return n.includes('.')&&e.includes(x)};
 const withExt=n=>{const e=T()[1];if(e[0]=='*'||!n)return n;const x=n.includes('.')?n.split('.').pop().toUpperCase():'';if(e.includes(x))return n;
  const known=types.some(t=>t[1].includes(x));return(known?n.replace(/\.[^.]*$/,''):n)+'.'+e[0]};
 const done=v=>{ov.remove();res(v)};
 const draw=()=>{$('.fdpath').value=cur.join('\\');L.innerHTML='';const d=node(cur)||{};
  const ks=Object.keys(d).sort((a,b)=>(isDir(d[b])-isDir(d[a]))||a.localeCompare(b));let n=0;
  ks.forEach(k=>{const dir=isDir(d[k]);if(!dir&&!match(k))return;n++;const r=document.createElement('div');r.className='fdr';r.setAttribute('role','option');r.tabIndex=-1;
   r.style.cssText='padding:3px 8px;cursor:default;white-space:nowrap;overflow:hidden;text-overflow:ellipsis';r.textContent=(typeof fIcon=='function'?fIcon(k,d[k]):dir?'📁':'📄')+' '+k;r.dataset.n=k;r.dataset.dir=dir?1:0;
   r.onclick=()=>{L.querySelectorAll('.fdr').forEach(x=>x.style.background='');r.style.background='var(--sel,#cfe0ff)';if(dir){cur=[...cur,k];draw()}else{nm.value=k;er('')}};
   r.ondblclick=()=>{if(!dir){nm.value=k;ok()}};L.appendChild(r)});
  if(!n){const e=document.createElement('div');e.className='msg';e.style.padding='8px';e.textContent='This folder is empty.';L.appendChild(e)}
  $('.fdup').disabled=cur.length<=1};
 const ok=()=>{er('');let v=nm.value.trim();if(!v)return er(save?'Type a name for the file.':'Choose a file.');
  if(/[\\]/.test(v)){const p=resolve(v,cur);if(isDir(node(p))){cur=p;nm.value='';draw();return}v=p.at(-1);const dp=p.slice(0,-1);if(!isDir(node(dp)))return er('That folder does not exist.');cur=dp}
  if(isDir(node([...cur,v.toUpperCase()]))){cur=[...cur,v.toUpperCase()];nm.value='';draw();return}
  /* (a name typed with an extension keeps it; one without gets the chosen type's) */
  if(save&&!/\.[A-Za-z0-9]{1,5}$/.test(v))v=withExt(v);v=v.toUpperCase();if(!(typeof validName=='function'?validName(v):v.length<=60))return er('That name is not allowed (no \\ / : * ? " < > |, at most 60 characters).');
  const p=[...cur,v],n=node(p);
  if(save){if(n!==undefined){if(isDir(n))return er('A folder has that name.');if(!ov.dataset.ask||ov.dataset.ask!=p.join('\\')){ov.dataset.ask=p.join('\\');return er(v+' already exists. Press Save again to replace it.')}}done(p)}
  else{if(n===undefined||isDir(n))return er('That file does not exist here.');done(p)}};
 $('.fdup').onclick=()=>{if(cur.length>1){cur=cur.slice(0,-1);draw()}};$('.fdhome').onclick=()=>{cur=fileDlgStart(null);draw()};
 if(save)$('.fdnew').onclick=()=>{const d=node(cur);let k='NEW FOLDER',i=2;while(d[k])k='NEW FOLDER ('+(i++)+')';d[k]={};try{window.save&&window.save()}catch(e){}try{EXPL.forEach(f=>f())}catch(e){}draw()};
 ty.onchange=()=>{if(save&&nm.value.trim())nm.value=withExt(nm.value.trim());draw()};
 $('.fdok').onclick=ok;$('.fdx').onclick=()=>done(null);
 ov.addEventListener('keydown',e=>{e.stopPropagation();if(e.key=='Escape'){e.preventDefault();done(null)}else if(e.key=='Enter'&&e.target.tagName!='BUTTON'&&e.target.tagName!='SELECT'){e.preventDefault();ok()}});
 if(o.name){nm.value=o.name;const x=(o.name.split('.').pop()||'').toUpperCase();const i=types.findIndex(t=>t[1].includes(x));if(i>=0)ty.value=i}
 draw();setTimeout(()=>{nm.focus();if(save){const k=nm.value.lastIndexOf('.');nm.setSelectionRange(0,k>0?k:nm.value.length)}},0)})}
/* the types each editor offers */
const FD_TEXT=[['Text',['TXT']],['Markdown',['MD']],['Web page',['HTM','HTML']],['Lua program',['LUA']],['CSV table',['CSV']],['JSON',['JSON']],['Batch file',['BAT']]];
const FD_DOC=[['Rich document',['DOC']],['Web page',['HTM','HTML']],['Text (no formatting)',['TXT']]];
