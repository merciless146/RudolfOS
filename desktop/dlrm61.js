/* ===== 1.6.1 (the owner, 2026-11-20): Oracle's Downloads page (about:downloads) =====
   - "Downloads should show a progress bar when you view the downloads page on Oracle": a download that is arriving
     shows how much has come, how fast, and (when the size is known) how much of it and the time left; one being
     checked shows that. The page is drawn again every second while something arrives (dl27.js __dlHook).
   - "The listed items should also have the option to "remove" them from the list next to where it says "open"":
     Remove takes it off the list; a file that is in this account's Downloads goes to the Trash with it (it can be put
     back from there). Otherwise the notification says it was not in Downloads, with Blitzen's button to help find it
     ("moved to the Trash, deleted, or never downloaded"): Blitzen reads the download's record, the Trash and where
     files of that name are now (it changes nothing). */
function dlFmtB(n){n=+n||0;return n<1024?n+' bytes':n<1048576?(n/1024).toFixed(n<10240?1:0)+' KB':n<1073741824?(n/1048576).toFixed(n<10485760?1:0)+' MB':(n/1073741824).toFixed(2)+' GB'}
function dlLeft(s){s=Math.max(1,Math.round(s));return s<60?s+' s left':s<3600?Math.round(s/60)+' min left':Math.floor(s/3600)+' h '+Math.round(s%3600/60)+' min left'}
/* the bar for a row of about:downloads (HTML: numbers and fixed words only) */
function dlBarHtml(d){if(d.st!='wait'&&d.st!='check')return'';
 const bar=(pct,moving)=>`<div role='progressbar' aria-valuemin='0' aria-valuemax='100'${pct!=null?` aria-valuenow='${Math.round(pct)}'`:''} style='height:8px;border-radius:4px;background:#e5e7eb;overflow:hidden;margin:6px 0 3px;max-width:420px'>`+
  (pct!=null?`<div style='height:100%;width:${Math.max(2,Math.min(100,pct)).toFixed(1)}%;background:#2563eb;transition:width .8s linear'></div>`
   :`<div style='height:100%;width:30%;background:#2563eb;border-radius:4px;animation:dlmv 1.6s linear infinite;animation-delay:-${Date.now()%1600}ms'></div>`)+`</div>`+
  (moving||pct==null?`<style>@keyframes dlmv{from{transform:translateX(-100%)}to{transform:translateX(340%)}}</style>`:'');
 const small=t=>`<div style='color:#6b7280;font-size:12px'>${t}</div>`;
 if(d.st=='check')return bar(null,1)+small('Checking the file (firewall, file type, antivirus)…');
 const got=+d.sz||0,tot=+d.tot||0,sp=+d.sp||0;
 if(tot>0){const pct=got/tot*100;return bar(pct)+small(`${dlFmtB(got)} of ${dlFmtB(tot)} (${Math.floor(pct)}%)${sp>0?' · '+dlFmtB(sp)+'/s · '+dlLeft((tot-got)/sp):''}`)}
 if(got>0)return bar(null,1)+small(`${dlFmtB(got)} so far${sp>0?' · '+dlFmtB(sp)+'/s':''} (the site did not say how large it is)`);
 return bar(null,1)+small('Starting…')}
/* a download's speed, from its growing partial file (dl27.js dlPoll) */
function dlSpeed(r,size){const now=Date.now();if(r.ps!=null&&now>r.pt&&size>=r.ps){const v=(size-r.ps)/((now-r.pt)/1000);r.sp=r.sp?r.sp*.6+v*.4:v}r.ps=size;r.pt=now}
/* Remove (act "rmdl", the record's time and name) */
function dlRemove(t,n){const i=DLS.findIndex(x=>x.t===t&&x.n===n);if(i<0)return;const d=DLS[i];
 if(d.st=='wait'||d.st=='check'){notify('Still downloading',d.n+' is still arriving. Remove it once it has finished.');return}
 let p=null;try{const DP=downloadsPath(),nm=d.path||d.n,v=node([...DP,nm]);if(d.st=='saved'&&nm&&v!=null&&!isDir(v))p=[...DP,nm]}catch(e){}
 DLS.splice(i,1);saveDl();
 if(p){const r=trashPut([p]);if(r<0){DLS.splice(i,0,d);saveDl();notify('The Trash is full','Empty the Trash, then remove '+d.n+' again.',{kind:'error'});return}
  try{EXPL.forEach(f=>f())}catch(e){}try{sfx('notify')}catch(e){}
  notify('Moved to the Trash',p[p.length-1]+' was removed from the list and moved to the Trash.',{action:()=>{try{launch('explorer');setTimeout(()=>{try{window.__trashOpen&&__trashOpen()}catch(e){}},300)}catch(e){}}})}
 else notify('File removed from list','Not in Downloads. Ask Blitzen if you need help finding this file.',{blitz:{n:d.n,path:d.path||'',u:String(d.u||'').slice(0,500),t:d.t,st:d.st,why:String(d.why||'').slice(0,300),disk:!!d.disk,sz:+d.sz||0}});
 try{window.__dlHook&&__dlHook()}catch(e){}}
/* where the file of a removed download is: what Blitzen reads (shown first; nothing is changed) */
function dlWhere(b){const N=String(b.path||b.n||'').toUpperCase(),base=N.replace(/(\s\(\d+\))?(\.[^.]*)?$/,'');const found=[],near=[];
 try{const walk=(o,pp,cb)=>{for(const k in o){const w=[...pp,k];cb(w,o[k]);if(isDir(o[k]))walk(o[k],w,cb)}};walk(fs,[],(p,v)=>{if(isDir(v))return;const k=String(p[p.length-1]).toUpperCase();if(k==N||k==String(b.n).toUpperCase())found.push(p.join('\\'));else if(base.length>3&&k.startsWith(base))near.push(p.join('\\'))})}catch(e){}
 const tr=(typeof TR!='undefined'?TR:[]).filter(e=>{const k=String(e.n).toUpperCase();return k==N||k==String(b.n).toUpperCase()||(base.length>3&&k.startsWith(base))}).map(e=>e.n+' (put in the Trash '+new Date(e.t).toLocaleString()+', from '+(e.from||[]).join('\\')+')');
 const st={saved:'saved to Downloads',failed:'failed (never saved)',blocked:'blocked (never saved)',quarantined:'quarantined by the antivirus (never put in Downloads; see Settings › Security › Quarantine)'}[b.st]||b.st;
 return {rec:`Name: ${b.n}${b.path&&b.path!=b.n?' (saved as '+b.path+')':''}\nFrom: ${b.u||'(not known)'}\nWhen: ${new Date(b.t).toLocaleString()}\nWhat happened: ${st}${b.why?' — '+b.why:''}${b.sz?'\nSize: '+dlFmtB(b.sz):''}${b.disk?'\nIt was kept on the disk (larger than 1.5 MB): Downloads held a link to it.':''}`,
  trash:tr.length?tr.join('\n'):'No file of that name is in the Trash.',
  where:(found.length?'Files of that name now:\n'+found.slice(0,20).join('\n'):'No file of that name is anywhere in the account\'s files (C:).')+(near.length?'\nFiles with a similar name:\n'+near.slice(0,20).join('\n'):''),
  dl:(()=>{try{return 'The account\'s Downloads folder: '+downloadsPath().join('\\')}catch(e){return''}})()}}
async function dlFindAsk(b){if(!b)return;if(typeof bzCan=='function'&&!bzCan())return notify('Blitzen is not available','Blitzen needs the installed Rudolf OS.');
 const w=dlWhere(b);const parts=[{label:'The download',text:w.rec},{label:'The Trash',text:w.trash},{label:'Where files of that name are',text:w.where+(w.dl?'\n'+w.dl:'')}];
 const keep=typeof bzReview=='function'?await bzReview('Where is '+String(b.n).slice(0,60)+'?',parts):parts;if(!keep)return;
 const ctx=keep.map(p=>'### '+p.label+'\n'+p.text).join('\n\n').slice(0,6000);
 blzAskFrom('the download list and your files',`I removed "${b.n}" from Oracle's download list and it was not in my Downloads folder. Where is this file now? Was it moved somewhere else, put in the Trash, deleted, or never downloaded? Tell me how to find it or get it back in Rudolf OS.`,ctx,{norun:true,lang:''})}
