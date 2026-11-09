/* ===== 1.5.6 (1.5.5's plan, item 7): rename several files at once, and find duplicate files =====
   Rename… with several items selected opens a window: a pattern ("HOLIDAY ###" gives HOLIDAY 001, 002…, each keeping
   its extension) or find and replace in the names. Every old and new name is shown before anything changes; a name
   that would clash (with another new name, or with an item already in that folder) is shown in red and nothing is
   done until it is fixed. One Undo in File Explorer puts all the names back. (Names on C: are kept in capitals, as
   File Explorer always has, so there is no upper/lower case choice.)
   Find duplicates (a folder's menu, and Settings › Storage and backup): files of the same size are compared by
   SHA-256, in the desktop for what it holds and through rudolfd (home/hash) for files kept in the account's folder
   on the disk; the groups are listed with where and when, the newest of each is suggested to keep, and only "Move
   the others to the Trash" moves anything, so it can be undone from the Trash. */
window.__rename56=1;
/* the plan: for each path its new name, and why it cannot be used (err) */
function brPlan(paths,o){const out=[];let i=+o.start||1;
 const pad=(n,w)=>String(n).padStart(w,'0');
 for(const p of paths){const old=p.at(-1),v=node(p),dir=isDir(v),dot=dir?-1:old.lastIndexOf('.'),base=dot>0?old.slice(0,dot):old,ex=dot>0?old.slice(dot):'';let nb=base;
  if(o.mode=='pattern'){const pat=String(o.pattern||'').toUpperCase().trim();const m=/#+/.exec(pat);nb=m?pat.slice(0,m.index)+pad(i,m[0].length)+pat.slice(m.index+m[0].length):(pat?pat+' '+i:base);i++}
  else if(o.mode=='replace'&&o.find){const f=String(o.find).toUpperCase(),r=String(o.repl||'').toUpperCase();nb=base.split(f).join(r)}
  out.push({p,old,nv:(nb+ex).trim(),err:''})}
 /* clashes: two new names in one folder, or a new name that an item staying in that folder already has */
 const byDir=new Map();out.forEach(x=>{const k=pkey(x.p.slice(0,-1));if(!byDir.has(k))byDir.set(k,[]);byDir.get(k).push(x)});
 byDir.forEach(list=>{const d=dirOf(list[0].p),moving=new Set(list.map(x=>x.old)),seen=new Map();
  list.forEach(x=>{if(!validName(x.nv)){x.err='Names cannot be empty, longer than 60 characters or contain \\ / : * ? " < > |';return}
   if(seen.has(x.nv)){x.err='Two items would be called '+x.nv;seen.get(x.nv).err=x.err;return}seen.set(x.nv,x);
   if(x.nv!=x.old&&d&&x.nv in d&&!moving.has(x.nv))x.err='Something in this folder is already called '+x.nv})});
 return out}
/* renames as planned (all, or nothing when any has an error); returns what Undo needs */
function brApply(plan){if(plan.some(x=>x.err))return null;const done=[];const byDir=new Map();
 plan.filter(x=>x.nv!=x.old).forEach(x=>{const k=pkey(x.p.slice(0,-1));if(!byDir.has(k))byDir.set(k,{dir:x.p.slice(0,-1),m:new Map()});byDir.get(k).m.set(x.old,x.nv)});
 byDir.forEach(({dir,m})=>{const d=node(dir),ent=Object.entries(d);Object.keys(d).forEach(k=>delete d[k]);ent.forEach(([k,v])=>d[m.has(k)?m.get(k):k]=v);
  m.forEach((nv,old)=>{done.push({dir,old,nv});if(typeof favMoved=='function')favMoved([...dir,old],[...dir,nv])})});
 if(done.length)fsSave();return done}
function brUndo(items){const byDir=new Map();items.forEach(x=>{const k=pkey(x.dir);if(!byDir.has(k))byDir.set(k,{dir:x.dir,m:new Map()});byDir.get(k).m.set(x.nv,x.old)});
 byDir.forEach(({dir,m})=>{const d=node(dir);if(!isDir(d))return;const ent=Object.entries(d);Object.keys(d).forEach(k=>delete d[k]);ent.forEach(([k,v])=>d[m.has(k)?m.get(k):k]=v);
  m.forEach((old,nv)=>{if(typeof favMoved=='function')favMoved([...dir,nv],[...dir,old])})});fsSave()}
function brWindow(paths,applied){const W=win('Rename '+paths.length+' items','✏',640,480);W.body.style.cssText='display:flex;flex-direction:column;padding:10px;gap:8px';
 W.body.innerHTML=`<div class='rmrow' style='justify-content:flex-start;gap:12px'><label><input type='radio' name='brm' value='pattern' checked> A pattern</label><label><input type='radio' name='brm' value='replace'> Find and replace</label></div>
<div class='brp'><label>New names <input class='bpat' value='${esc2(String(paths[0].at(-1)).replace(/\.[^.]*$/,'').replace(/\s*\d+$/,'')||'FILE')} ###' spellcheck='false' style='width:260px'></label> <label>starting at <input class='bst' type='number' value='1' min='0' style='width:70px'></label><div class='msg' style='font-size:12px'># is the number (### gives 001, 002…); each file keeps its extension.</div></div>
<div class='brr' hidden><label>Find <input class='bf' spellcheck='false'></label> <label>Replace with <input class='br' spellcheck='false'></label></div>
<div style='flex:1;overflow:auto;border:1px solid #0002'><table class='tmt brt' style='width:100%'><thead><tr><th>Now</th><th>Becomes</th></tr></thead><tbody></tbody></table></div>
<div class='msg bre' role='status'></div><div class='rmrow'><button class='bc'>Cancel</button><button class='pri ba'>Rename</button></div>`;
 const q=s=>W.body.querySelector(s);let plan=[];
 const mode=()=>W.body.querySelector("input[name='brm']:checked").value;
 const upd=()=>{q('.brp').hidden=mode()!='pattern';q('.brr').hidden=mode()!='replace';
  plan=brPlan(paths,{mode:mode(),pattern:q('.bpat').value,start:q('.bst').value,find:q('.bf').value,repl:q('.br').value});
  const tb=q('.brt tbody');tb.innerHTML='';plan.forEach(x=>{const r=tb.insertRow();r.insertCell().textContent=x.old;const c=r.insertCell();c.textContent=x.nv;if(x.err){c.style.color='#b91c1c';c.title=x.err;r.classList.add('brerr')}});
  const bad=plan.filter(x=>x.err),same=plan.every(x=>x.nv==x.old);q('.ba').disabled=!!bad.length||same;
  q('.bre').textContent=bad.length?bad[0].err+(bad.length>1?' (and '+(bad.length-1)+' more in red)':''):same?'Nothing would change.':''};
 W.body.querySelectorAll('input').forEach(i=>i.oninput=i.onchange=upd);
 q('.bc').onclick=()=>W.close();q('.ba').onclick=()=>{const d=brApply(plan);if(!d){upd();return}W.close();applied&&applied(d)};upd();return W}
/* ---------- duplicates ---------- */
async function dupHash(p,v){
 if(typeof hdRefRel=='function'&&hdOn()){const rel=hdRefRel(v);if(rel!=null){const j=await rd('home/hash?'+hdQ()+'&path='+encodeURIComponent(rel));return j.sha256}}
 if(typeof v!='string'||(typeof isDlRef=='function'&&isDlRef(v))||v.startsWith('rudolfrom:'))return null;
 let bytes;if(v.startsWith('data:')){const b=atob(v.slice(v.indexOf(',')+1));bytes=new Uint8Array(b.length);for(let i=0;i<b.length;i++)bytes[i]=b.charCodeAt(i)}else bytes=new TextEncoder().encode(v);
 const h=new Uint8Array(await crypto.subtle.digest('SHA-256',bytes));return[...h].map(x=>x.toString(16).padStart(2,'0')).join('')}
/* groups of identical files below root: [[{p,size,m}, ...], ...], the newest first in each; skipped: files that
   could not be compared */
async function dupFind(root,prog){const files=[];const walk=(nd,p)=>{for(const k in nd){const v=nd[k],pp=[...p,k];if(isDir(v)){if(p.length==2&&p[1]=='USERS'&&!fsCanWrite(pp))continue;walk(v,pp)}else{const s=sizeOf(v);if(s>0)files.push({p:pp,v,size:s,m:(fileMeta(pp)||{}).m||0})}}};
 const r=node(root);if(isDir(r))walk(r,root);const bySize=new Map();files.forEach(f=>{if(!bySize.has(f.size))bySize.set(f.size,[]);bySize.get(f.size).push(f)});
 const cand=[...bySize.values()].filter(l=>l.length>1).flat();const byHash=new Map();let skipped=0,n=0;
 for(const f of cand){let h=null;try{h=await dupHash(f.p,f.v)}catch(e){}n++;prog&&prog(n,cand.length);if(!h){skipped++;continue}const k=f.size+':'+h;if(!byHash.has(k))byHash.set(k,[]);byHash.get(k).push(f)}
 const groups=[...byHash.values()].filter(l=>l.length>1).map(l=>l.sort((a,b)=>b.m-a.m)).sort((a,b)=>b[0].size*(b.length-1)-a[0].size*(a.length-1));
 return{groups,skipped,files:files.length}}
async function dupWindow(root){const W=win('Duplicate files','🧬',700,520);W.body.style.cssText='display:flex;flex-direction:column;padding:10px;gap:8px';
 W.body.innerHTML=`<div class='msg dps'>Looking for duplicates in ${esc2(root.join('\\\\'))}…</div><div class='dpl' style='flex:1;overflow:auto'></div><div class='rmrow'><button class='dpc'>Close</button><button class='pri dpt' disabled>Move the others to the Trash</button></div>`;
 const q=s=>W.body.querySelector(s);q('.dpc').onclick=()=>W.close();
 const res=await dupFind(root,(i,n)=>{if(W.el.isConnected)q('.dps').textContent=`Comparing ${i} of ${n} files of the same size…`});if(!W.el.isConnected)return W;
 const L=q('.dpl');const sel=new Set();const wasted=res.groups.reduce((a,g)=>a+g[0].size*(g.length-1),0);
 q('.dps').textContent=res.groups.length?`${res.groups.length} group${res.groups.length==1?'':'s'} of identical files; ${fmtB(wasted)} could be freed. The newest of each is kept unless you choose otherwise; the others are ticked to go to the Trash.`:'No duplicates found among '+res.files+' files.';
 if(res.skipped)q('.dps').textContent+=` (${res.skipped} file${res.skipped==1?'':'s'} could not be compared.)`;
 res.groups.forEach((g,gi)=>{const box=document.createElement('fieldset');box.style.cssText='margin:0 0 8px;border:1px solid #0002';box.innerHTML=`<legend>${fmtB(g[0].size)} × ${g.length}</legend>`;
  g.forEach((f,i)=>{const l=document.createElement('label');l.style.cssText='display:flex;gap:8px;align-items:center;padding:2px 0';const k=pkey(f.p);if(i>0)sel.add(k);
   l.innerHTML=`<input type='checkbox' ${i>0?'checked':''}><span style='flex:1'></span><small></small><em style='font-size:11px;opacity:.7'></em>`;l.querySelector('span').textContent=f.p.join('\\\\');
   l.querySelector('small').textContent=f.m?new Date(f.m).toLocaleString():'';l.querySelector('em').textContent=i==0?'newest: keep':'';
   const cb=l.querySelector('input');cb.setAttribute('aria-label','Move '+f.p.at(-1)+' to the Trash');cb.onchange=()=>{cb.checked?sel.add(k):sel.delete(k);
    /* never every copy of a group */
    if(g.every(x=>sel.has(pkey(x.p)))){cb.checked=false;sel.delete(k);q('.dps').textContent='One copy of each file is always kept.'}q('.dpt').disabled=!sel.size};box.appendChild(l)});L.appendChild(box)});
 q('.dpt').disabled=!sel.size;
 q('.dpt').onclick=()=>{const ps=[...sel].map(k=>k.split('/')).filter(p=>node(p)!=null);const n=trashPut(ps);if(n<0){q('.dps').textContent='The Trash is full. Empty it first.';return}
  EXPL.forEach(f=>f());notify('Duplicates moved to the Trash',`${n} file${n==1?'':'s'}. They can be restored from the Trash.`);W.close()};return W}
{const _s=storPanel;storPanel=function(c){_s.apply(this,arguments);if(!USER)return;const ex=c.querySelector('.ex');if(!ex)return;const b=document.createElement('button');b.textContent='Find duplicates…';b.className='dup56';b.onclick=()=>dupWindow(homePath());ex.after(b)}}
