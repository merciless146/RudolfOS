/* ===== 1.5.6 (1.5.5's plan, items 4 and 5): what fills the disk, and the Trash's size and age =====
   Settings › Storage and backup gets "On this computer's disk": rudolfd's storage-measure counts each place once
   (GET storage, POST storage/measure; a Steam library takes a few seconds), largest first, each with what can be done
   about it. The page measures again when the last count is over 10 minutes old, and on "Measure again". The "disk
   almost full" notification opens it.
   The Trash shows its total size (its window and the desktop's tooltip; large files at their size on the disk), and
   each account can have items deleted for good once they are older than 1, 14, 30 or 60 days (bd95trauto:<id>,
   Never by default): checked at sign-in and every hour, with one notification a day at most. */
window.__storage56=1;
/* sizes in the Trash: a file kept on the disk counts at its own size, not at the size of its reference */
trSize=function(v){if(isDir(v))return Object.values(v).reduce((a,x)=>a+trSize(x),0);const s=sizeOf(v);return s>0?s:String(v).length};
const trTotal=()=>TR.reduce((a,x)=>a+trSize(x.v),0);
{const _b=trBadge;trBadge=function(){_b();const d=document.querySelector('.ic.tr');if(!d||!TR.length)return;
 const t=TR.length+(TR.length==1?' item':' items')+' in the Trash ('+fmtB(trTotal())+')';if(d.dataset.noted)d.dataset.t0=t;else d.title=t}}
/* the Trash window's status line says the total too (TRW: the open Trash windows, redrawn on each change) */
{const _t=trash;trash=A.trash=function(){const r=_t.apply(this,arguments);const W=wins.at(-1);const st=W&&W.body.querySelector('.st');
 if(st){const f=()=>{if(!TR.length)return;const base=st.textContent.split('  ·  ')[0].split(' · ').filter(x=>!/B$/.test(x)).join(' · ');
  st.textContent=base+' · '+fmtB(trTotal())+'  ·  '+(trAutoDays()?'Items older than '+trAutoDays()+(trAutoDays()==1?' day':' days')+' are deleted for good.':'Items stay here until you delete them forever.')};
  f();TRW.add(f);const sel=W.body.querySelector('.tl');if(sel)sel.addEventListener('click',()=>setTimeout(f,0));const oc=W.onclose;W.onclose=()=>{TRW.delete(f);oc&&oc()}}return r}}
/* emptying old items */
const trAutoK=()=>'bd95trauto:'+uid();
const trAutoDays=()=>USER&&USER.r!='guest'?(+ld(trAutoK(),0)||0):0;
function trAutoEmpty(now){if(!USER)return 0;const days=trAutoDays();if(!days)return 0;now=now||Date.now();const cut=now-days*864e5;
 const before=TR.length;for(let i=TR.length-1;i>=0;i--)if((TR[i].t||now)<cut)TR.splice(i,1);const n=before-TR.length;if(!n)return 0;trSave();
 const lk='bd95trautoN:'+uid();if(now-(+ld(lk,0)||0)>864e5-6e4){svj(lk,now);notify('Old items deleted from the Trash',`${n} item${n==1?' was':'s were'} in the Trash for over ${days} day${days==1?'':'s'}. Settings › Storage and backup decides how long items stay.`,{action:()=>settings('stor')})}
 return n}
(function(){const o=window.__afterLogin;window.__afterLogin=()=>{o&&o();setTimeout(()=>{try{trAutoEmpty()}catch(e){}},4000)}})();
setInterval(()=>{try{trAutoEmpty()}catch(e){}},36e5);
/* "the disk is almost full" opens the page that says what fills it */
{const _n=notify;notify=function(t,b,o){if(t=='The disk is almost full'&&o&&RD)o=Object.assign({},o,{action:()=>settings('stor')});return _n.call(this,t,b,o)}}
/* the section on the page */
const STOR_LINES=[['files','📄','Your files','Desktop, Documents, Pictures, Music and Videos'],['others','👥','Other accounts\' files',''],['downloads','⬇️','Downloads','yours, and what Linux apps download'],
 ['trash','🗑️','The Trash','files waiting until the Trash is emptied'],['flatpak','🐧','Linux apps','App Store apps and their data'],['steam','🎮','Steam\'s games','inside the Steam app'],
 ['debian','🐧','Linux (Debian)','.deb packages and Windows programs (Wine)'],['games','🕹️','Emu\'s games',''],['ai','🦌','Blitzen\'s model','the offline assistant'],
 ['updates','🔄','The system','Rudolf OS, and its spare copy for updates and going back'],['rest','⚙️','Everything else','the browser\'s cache, settings and logs']];
function storLines(res){const it=res.items||{},me=(res.accounts||[]).find(a=>typeof HD!='undefined'&&a.id==HD.acct),others=(res.accounts||[]).filter(a=>a!==me);
 const v={...it};if(me){v.files=me.files;v.trash=me.trash;v.others=others.reduce((a,x)=>a+x.files+x.trash+(x.downloads||0),0)}else v.others=0;
 return STOR_LINES.map(([k,i,l,s])=>({k,i,l,s,b:+v[k]||0})).filter(x=>x.b>0).sort((a,b)=>b.b-a.b)}
function storAction(k){
 if(k=='files')return['Open',()=>explorer(homePath())];
 if(k=='downloads')return['Open',()=>explorer(downloadsPath())];
 if(k=='trash')return['Open the Trash',()=>trash()];
 if(k=='flatpak')return['Manage',()=>{const W=appStore();const b=W&&W.body.querySelector(".tmtabs button[data-t='linux']");b&&b.click()}];
 if(k=='steam'){const s=Object.keys(APPS).find(x=>/^lx:.*valvesoftware\.Steam/i.test(x));return s?['Open Steam',()=>launch(s)]:null}
 if(k=='debian')return['Manage',()=>settings('apps')];
 if(k=='games'){const e=Object.keys(APPS).find(x=>APPS[x].name=='Emu');return e?['Open Emu',()=>launch(e)]:null}
 return null}
async function storDisk(box){if(!RD)return;box.innerHTML="<h4 class='sh4'>On this computer's disk</h4><div class='msg sdm'>Measuring…</div><div class='stbar sdb'><div class='stfill'></div></div><table class='tmt sdt'><tbody></tbody></table><div class='rmrow' style='justify-content:flex-start'><button class='sda'>Measure again</button></div>";
 const q=s=>box.querySelector(s),msg=q('.sdm');let alive=true;
 const show=j=>{const r=j&&j.result;if(!r){msg.textContent=j&&j.measuring?'Measuring… (a large Steam library takes a little while)':'Not measured yet.';return}
  const d=r.disk||{};q('.stfill').style.width=Math.max(1,d.total?d.used/d.total*100:0)+'%';
  msg.textContent=`${fmtB(d.used||0)} used of ${fmtB(d.total||0)} · ${fmtB(d.free||0)} free · measured ${new Date(r.time*1000).toLocaleTimeString([], {hour:'2-digit',minute:'2-digit'})}`+(j.measuring?' · measuring again…':'');
  const tb=q('.sdt tbody');tb.innerHTML='';const L=storLines(r),max=L.length?L[0].b:1;
  L.forEach(x=>{const tr=tb.insertRow();tr.dataset.k=x.k;const c1=tr.insertCell(),c2=tr.insertCell(),c3=tr.insertCell(),c4=tr.insertCell();
   c1.innerHTML=`<span></span> <b></b><br><small style='opacity:.7'></small>`;c1.querySelector('span').textContent=x.i;c1.querySelector('b').textContent=x.l;c1.querySelector('small').textContent=x.s;
   c2.innerHTML="<div class='stbar' style='min-width:80px;height:8px'><div class='stfill'></div></div>";c2.querySelector('.stfill').style.width=Math.max(1,x.b/max*100)+'%';
   c3.textContent=fmtB(x.b);c3.style.textAlign='right';const a=storAction(x.k);if(a){const b=document.createElement('button');b.textContent=a[0];b.onclick=a[1];c4.appendChild(b)}})};
 const poll=async(start)=>{let j=null;try{j=start?await rdPost('storage/measure'):await rd('storage')}catch(e){msg.textContent='Could not measure: '+e.message;return}
  if(!alive||!box.isConnected)return;show(j);if(j.measuring)setTimeout(()=>poll(false),1500)};
 q('.sda').onclick=()=>poll(true);
 let j=null;try{j=await rd('storage')}catch(e){}const old=!j||!j.result||Date.now()/1000-j.result.time>600;show(j);poll(old)}
{const _s=storPanel;storPanel=function(c){_s.apply(this,arguments);if(!RD)return;
 const ta=document.createElement('div');ta.className='tra56';
 ta.innerHTML=`<h4 class='sh4'>The Trash</h4><p class='msg tras'></p><label>Delete items in the Trash for good once they are older than <select class='trad'><option value='0'>Never</option><option value='1'>1 day</option><option value='14'>14 days</option><option value='30'>30 days</option><option value='60'>60 days</option></select></label>`;
 ta.querySelector('.tras').textContent=TR.length?`${TR.length} item${TR.length==1?'':'s'} in your Trash, ${fmtB(trTotal())}.`:'Your Trash is empty.';
 const sel=ta.querySelector('.trad');sel.value=String(trAutoDays());sel.disabled=!USER||USER.r=='guest';sel.onchange=()=>{svj(trAutoK(),+sel.value);const n=trAutoEmpty();ta.querySelector('.tras').textContent=(n?n+' old item'+(n==1?' was':'s were')+' deleted. ':'')+(TR.length?`${TR.length} item${TR.length==1?'':'s'} in your Trash, ${fmtB(trTotal())}.`:'Your Trash is empty.')};
 const dk=document.createElement('div');dk.className='sd56';
 const bk=[...c.querySelectorAll('h4.sh4')].find(h=>/Backup/.test(h.textContent));if(bk){c.insertBefore(dk,bk);c.insertBefore(ta,bk)}else{c.appendChild(dk);c.appendChild(ta)}
 storDisk(dk)}}
