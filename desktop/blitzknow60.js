/* ===================== 1.6.0 (docs/plan-1.6.0.md Part 18): Blitzen knows the system =====================
   The owner, 2026-11-18: "Can Blitzen be given the ability to train itself based on the system's source code, system
   logs and user activity, and research sources thru Oracle for the user's questions or requests?" Not by training its
   model: by looking things up before it answers (rudolfd's ai/know/..., rudolf-know):
   - Rudolf OS's own source and docs (the running release's, fetched once while the computer is idle) and the system's
     logs (passwords, tokens and keys left out), for every account;
   - what this account does on the computer (apps opened, files opened, settings changed, errors met), only when
     "Learn from what I do on this computer" is on (off at first, per account), kept on this computer, listed and
     forgotten in "See what Blitzen knows";
   - notes the person asks it to keep ("Remember that ..."), per account, never sent anywhere;
   - the web, only for a question with "Search the web" on: only the question goes out (DuckDuckGo, as Oracle's search;
     through Tor when asked), the first pages read as text, their links numbered and opened in Oracle.
   blzKnowFor (called by blitzen26.js's ask) gathers what fits a question; blzKnowDraw puts its sources under the answer. */
const BLZK={look:true,web:false,tor:false,last:0};
const blzkMemKey=()=>'bd95blzmem:'+uid();
function blzkNotes(){const v=ld(blzkMemKey(),[]);return Array.isArray(v)?v.filter(x=>typeof x=='string'&&x.trim()):[]}
function blzkSetNotes(a){svj(blzkMemKey(),a.slice(0,50).map(x=>String(x).slice(0,300)))}
async function blzkApi(path){return lxApi('ai/know/'+path)}
const blzkWords=(s,n)=>{const w=String(s||'').split(/\s+/).filter(Boolean);return w.length>n?w.slice(0,n).join(' ')+' …':w.join(' ')};
function blzkWhere(it){if(it.kind=='activity'){const t=new Date((+it.where||0)*1000);return 'what you did, '+t.toLocaleString([], {dateStyle:'medium',timeStyle:'short'})}
 return it.kind=='source'?`${it.src}, ${it.where}`:`${it.src}, ${it.where}`}
/* "Remember that ..." in a question: kept as a note (and answered as usual: the note goes with it) */
function blzkRemember(text){const m=/^\s*(?:please\s+)?remember(?:\s+that)?[\s,:]+(.{3,300})$/i.exec(String(text||''));if(!m||!USER)return null;
 const n=m[1].trim().replace(/[.!]+$/,'');const a=blzkNotes();if(!a.includes(n)){a.push(n);blzkSetNotes(a)}return n}
/* what fits a question: {prompt, sources:[{kind,label,...}], web:[...]}; null when nothing is to be added */
async function blzKnowFor(W,text,stEl){
 const out={prompt:'',sources:[]},parts=[];const look=W&&W.blzk?W.blzk.look():BLZK.look,web=W&&W.blzk?W.blzk.web():false,tor=W&&W.blzk?W.blzk.tor():false;
 const kept=blzkRemember(text);const notes=USER?blzkNotes():[];
 if(notes.length)parts.push('Notes the person asked you to remember (use them when they matter):\n'+notes.map(n=>'- '+n).join('\n'));
 if(look&&RD){if(stEl)stEl.textContent='Looking it up on this computer…';
  try{const j=await blzkApi('search?q='+encodeURIComponent(text.slice(0,900))+'&n=8');let words=0,k=0;const got=[];
   for(const it of j.items||[]){const t=blzkWords(it.text,450),n=t.split(/\s+/).length;if(words+n>1600&&got.length)break;words+=n;k++;
    got.push(`[${k}] ${blzkWhere(it)}${it.kind=='source'?' (Rudolf OS\'s source)':''}\n${t}`);out.sources.push({n:k,kind:it.kind,src:it.src,where:it.where,text:it.text,label:blzkWhere(it)})}
   if(got.length)parts.push('What this computer\'s own records say (Rudolf OS\'s source and documents, its logs'+(j.items.some(x=>x.kind=='activity')?', what the person did':'')+'). Use them when they fit the question and name each one you use as [1], [2]; when they do not answer it, say so and do not guess:\n\n'+got.join('\n\n'))}
  catch(e){console.warn('Blitzen look-up',e)}}
 if(web&&RD){if(stEl)stEl.textContent=tor?'Searching the web through Tor…':'Searching the web…';
  try{const j=await blzkApi('web?q='+encodeURIComponent(text.slice(0,380))+(tor?'&tor=1':''));const pages=(j.items||[]).slice(0,3);
   if(pages.length){parts.push('Web pages read for this question. Answer from them where they fit, naming each one you use as [W1], [W2]:\n\n'+pages.map((p,i)=>`[W${i+1}] ${p.title||p.url}\n${blzkWords(p.text,400)}`).join('\n\n'));
    pages.forEach((p,i)=>out.sources.push({n:'W'+(i+1),kind:'web',url:p.url,label:p.title||p.url}))}
   else out.webNote='The web search found nothing to read.'}
  catch(e){out.webNote='The web search did not work: '+e.message}}
 if(stEl)stEl.textContent='';
 if(kept)out.kept=kept;
 out.prompt=parts.join('\n\n');return out.prompt||out.kept||out.webNote?out:null}
/* the sources under an answer: each opens where it came from */
function blzKnowDraw(el,k){if(!el||!k)return;const body=el.querySelector('.blzbody');if(!body)return;
 const d=document.createElement('div');d.className='blzsrc';
 if(k.kept){const m=document.createElement('div');m.className='msg';m.textContent='📝 Kept as a note: '+k.kept+' (Settings › Apps › Blitzen › Memory)';d.appendChild(m)}
 if(k.webNote){const m=document.createElement('div');m.className='msg';m.textContent=k.webNote;d.appendChild(m)}
 if(k.sources.length){const h=document.createElement('div');h.className='blzsrch';h.textContent='Looked up:';d.appendChild(h);
  k.sources.forEach(s=>{const b=document.createElement('button');b.className='blzsrcb';b.title=s.kind=='web'?s.url:'Open it';
   b.textContent=`[${s.n}] ${s.kind=='web'?'🌐 ':s.kind=='source'?'📄 ':s.kind=='log'?'📋 ':'🕘 '}${s.label}`;b.onclick=()=>blzkOpen(s);d.appendChild(b)})}
 if(d.childNodes.length)body.appendChild(d)}
function blzkOpen(s){
 if(s.kind=='web')return typeof webOpen=='function'?webOpen(s.url):window.open(s.url,'_blank','noopener');
 if(s.kind=='activity')return blzkSeeWin();
 const W=win(s.label,s.kind=='source'?'📄':'📋',640,440);W.body.innerHTML="<pre class='blzview sunk'>…</pre>";const pre=W.body.querySelector('pre');
 if(s.kind=='source'){const line=parseInt(String(s.where).replace(/\D+/g,''))||1;
  blzkApi('show?src='+encodeURIComponent(s.src)+'&line='+line).then(j=>{pre.textContent=j.text}).catch(e=>{pre.textContent=s.text+'\n\n('+e.message+')'})}
 else pre.textContent=s.text}
/* the chat's own switches: "Look it up first" (kept), "Search the web" (each question; off again after it) */
{const _ui=blzChatUI;blzChatUI=function(W){const R=_ui(W);try{const bar=W.body.querySelector('.blzbar'),chk=bar&&bar.querySelector('.blzchk');if(!bar)return R;
  const s=document.createElement('span');s.className='blzk';
  s.innerHTML=`<label class='chk blzchk' title='Before answering, Blitzen looks in Rudolf OS’s source, the logs${USER?' and your notes':''}'><input type='checkbox' class='blzlook'${ld('bd95blzlook',true)?' checked':''}> Look it up first</label>
   <label class='chk blzchk' title='Only your question goes out (DuckDuckGo, as Oracle searches); Blitzen reads the first pages. Off again after the question.'><input type='checkbox' class='blzweb'> Search the web</label>
   <label class='chk blzchk blztor' hidden title='Through Tor: open a Tor window in Oracle first'><input type='checkbox' class='blztorc'> through Tor</label>`;
  if(chk)chk.after(s);else bar.prepend(s);
  const lk=s.querySelector('.blzlook'),wb=s.querySelector('.blzweb'),tr=s.querySelector('.blztor'),tc=s.querySelector('.blztorc');
  lk.onchange=()=>svj('bd95blzlook',lk.checked);wb.onchange=()=>{tr.hidden=!wb.checked};
  W.blzk={look:()=>lk.checked,web:()=>wb.checked,tor:()=>wb.checked&&tc.checked,done:()=>{wb.checked=false;tc.checked=false;tr.hidden=true}};
 }catch(e){console.warn(e)}return R}}
/* ---------- what this account does, while its learning is on (rudolfd keeps it only then) */
let blzkLearn=null;
async function blzkLearnOn(){if(!RD||!USER)return false;if(blzkLearn&&blzkLearn.u==uid()&&Date.now()-blzkLearn.t<60000)return blzkLearn.on;
 try{const j=await blzkApi('learn');blzkLearn={u:uid(),t:Date.now(),on:!!j.learn}}catch(e){blzkLearn={u:uid(),t:Date.now(),on:false}}return blzkLearn.on}
const BLZK_Q=[];let blzkSending=false;
function blzkNote(kind,text){if(!RD||!USER||!text)return;BLZK_Q.push([kind,String(text).replace(/[\t\r\n]+/g,' ').slice(0,300)]);if(BLZK_Q.length>50)BLZK_Q.shift();setTimeout(blzkFlush,1500)}
async function blzkFlush(){if(blzkSending||!BLZK_Q.length)return;blzkSending=true;
 try{if(!await blzkLearnOn()){BLZK_Q.length=0;return}
  while(BLZK_Q.length){const [k,t]=BLZK_Q.shift();await fetch('/api/ai/know/activity',{method:'POST',headers:{'X-Rudolf-Token':RD.token},body:k+'\t'+t}).catch(()=>{})}}
 finally{blzkSending=false}}
{const _l=launch;let inWith=0;launch=function(k){const a=APPS[k];if(a&&!inWith)blzkNote('launch','Opened '+a.name);return _l.apply(this,arguments)};
 if(typeof launchWith=='function'){const _lw=launchWith;launchWith=function(k,p){inWith++;try{const a=APPS[k],f=Array.isArray(p)?p:[];if(f.length)blzkNote('file',`Opened ${f.at(-1)} in ${f.slice(0,-1).join('\\')||'C:'}${a?' with '+a.name:''}`);return _lw.apply(this,arguments)}finally{inWith--}}}}
{const _n=notify;notify=function(t,b,o){try{if(typeof bzIsError=='function'&&bzIsError((t||'')+' '+(b||''),o&&o.kind))blzkNote('error',(t||'')+(b?': '+b:''))}catch(e){}return _n.apply(this,arguments)}}
if(typeof liveSet=='function'){const _s=liveSet;liveSet=function(c,k,v){const r=_s.apply(this,arguments);try{blzkNote('settings',`Changed the setting ${k} to ${typeof v=='boolean'?(v?'on':'off'):String(v).slice(0,40)}`)}catch(e){}return r}}
/* ---------- "See what Blitzen knows" */
function blzkSeeWin(){const W=win('What Blitzen knows','⚡',620,480);W.body.innerHTML="<div class='blzsee'><div class='msg'>Checking…</div></div>";const b=W.body.querySelector('.blzsee');
 const draw=async()=>{let st=null,items=[];try{st=await blzkApi('status')}catch(e){b.innerHTML=`<div class='msg'>${lxEsc(e.message)}</div>`;return}
  if(st.account&&st.learn)try{items=(await blzkApi('list')).items||[]}catch(e){}
  const days=[...new Set(items.map(x=>new Date(x.t*1000).toLocaleDateString('sv')))];
  b.innerHTML=`<table class='tmt'><tbody><tr><td>Rudolf OS's source and documents</td><td>${st.source?'release '+lxEsc(st.serial)+' ('+fmtB(st.source_bytes)+')':'not yet (fetched once, while the computer is idle on mains power)'}</td></tr>
   <tr><td>The system's logs</td><td>${st.logs?'read '+new Date(st.built*1000).toLocaleString()+' ('+fmtB(st.logs_bytes)+'), passwords, tokens and keys left out':'not yet'}</td></tr>
   <tr><td>What you do on this computer</td><td>${!st.account?'not for this account (sign in)':st.learn?items.length+' things kept':'not kept (off)'}</td></tr>
   <tr><td>Your notes</td><td>${blzkNotes().length}</td></tr></tbody></table>
   ${st.account&&st.learn?`<div class='rmrow' style='justify-content:flex-start'>${days.length?`<select class='bkday'>${days.map(d=>`<option>${d}</option>`).join('')}</select><button class='bkfd'>Forget that day</button>`:''}<button class='bkfa'>Forget everything</button></div>
   <div class='blzseel'>${items.length?items.slice(0,300).map(x=>`<div class='blzsee1'><span>${lxEsc(new Date(x.t*1000).toLocaleString())}</span><span>${lxEsc(x.text)}</span><button data-i='${x.item}' title='Forget this'>✕</button></div>`).join(''):"<div class='msg'>Nothing kept yet.</div>"}</div>`:''}<div class='msg bkm'></div>`;
  const m=b.querySelector('.bkm'),go=async(q,what)=>{if(!await blzkConfirm('Forget '+what+'?','Blitzen will no longer know it. This cannot be undone.'))return;try{await blzkApi('forget?'+q);draw()}catch(e){m.textContent=e.message}};
  const fa=b.querySelector('.bkfa');if(fa)fa.onclick=()=>go('all=1','everything Blitzen kept of what you did');
  const fd=b.querySelector('.bkfd');if(fd)fd.onclick=()=>go('day='+b.querySelector('.bkday').value,'what you did on '+b.querySelector('.bkday').value);
  b.querySelectorAll('.blzsee1 button').forEach(x=>x.onclick=async()=>{try{await blzkApi('forget?item='+x.dataset.i);draw()}catch(e){m.textContent=e.message}})};
 draw();return W}
/* a question with Yes and No, in the desktop's own dialog (as blzAskDialog) */
function blzkConfirm(title,text,yes){return new Promise(res=>{const o=document.createElement('div');o.className='rmodal';
 o.innerHTML=`<div class='rbox raise' role='dialog' aria-label='${lxEsc(title)}'><b>⚡ ${lxEsc(title)}</b><div class='msg' style='margin:8px 0'>${lxEsc(text)}</div><div class='rmrow'><button class='ok pri'>${lxEsc(yes||'Forget')}</button><button class='no'>Cancel</button></div></div>`;
 const end=v=>{o.remove();res(v)};o.querySelector('.ok').onclick=()=>end(true);o.querySelector('.no').onclick=()=>end(false);
 o.addEventListener('keydown',e=>{e.stopPropagation();if(e.key=='Escape')end(false)});document.body.appendChild(o);o.querySelector('.no').focus()})}
/* ---------- Settings › Apps › Blitzen: what it knows */
{appsPanel=__after(appsPanel,function(c,W){if(!RD)return;
 const d=sp(c,`<h4 class='sh4'>Blitzen: what it knows</h4><div class='bkset'></div>`);const b=d.querySelector('.bkset');
 const draw=async()=>{const on=USER?await blzkLearnOn():false,notes=USER?blzkNotes():[];
  b.innerHTML=`<div class='msg'>Before answering, Blitzen looks in Rudolf OS's own source and documents and the system's logs (with passwords, tokens and keys left out), and in what you ask it to remember. It does not learn by changing its model: what it uses can always be seen and forgotten here. It is a small model: good at “where in the system is …” and “what does this log line mean”, less so at weighing several web pages against each other; an answer that reads the web takes about a minute.</div>
   ${USER?`<label class='chk'><input type='checkbox' class='bklearn'${on?' checked':''}> Learn from what I do on this computer</label><div class='msg'>The apps and files you open (names and folders, not what is in them), the settings you change and the errors you meet. Kept on this computer for this account only; never sent anywhere.</div>
   <b>Memory</b><div class='bkmem'>${notes.length?notes.map((n,i)=>`<div class='blzsee1'><span>📝</span><span>${lxEsc(n)}</span><button data-i='${i}' title='Forget this note'>✕</button></div>`).join(''):"<div class='msg'>No notes. Say “Remember that …” to Blitzen, or add one here.</div>"}</div>
   <div class='rmrow' style='justify-content:flex-start'><input class='bknew sunk' maxlength='300' placeholder='Something Blitzen should know about you' style='flex:1'><button class='bkadd'>Add</button></div>`:"<div class='msg'>Sign in to an account to have Blitzen learn what you do and keep notes.</div>"}
   <div class='rmrow' style='justify-content:flex-start'><button class='bksee'>See what Blitzen knows…</button></div><div class='msg bkm'></div>`;
  const m=b.querySelector('.bkm');b.querySelector('.bksee').onclick=()=>blzkSeeWin();
  const l=b.querySelector('.bklearn');if(l)l.onchange=async()=>{try{const j=await blzkApi('learn?on='+(l.checked?1:0));blzkLearn={u:uid(),t:Date.now(),on:!!j.learn};
    if(!l.checked&&await blzkConfirm('Forget what Blitzen kept too?','Learning is off. What it already kept of what you did can be forgotten now, or later in See what Blitzen knows.'))await blzkApi('forget?all=1');m.textContent=l.checked?'Blitzen will learn from what you do from now on.':'Blitzen no longer learns from what you do.'}
   catch(e){l.checked=!l.checked;m.textContent=e.message}};
  b.querySelectorAll('.bkmem button').forEach(x=>x.onclick=()=>{const a=blzkNotes();a.splice(+x.dataset.i,1);blzkSetNotes(a);draw()});
  const add=b.querySelector('.bkadd');if(add)add.onclick=()=>{const v=b.querySelector('.bknew').value.trim();if(!v)return;const a=blzkNotes();if(!a.includes(v))a.push(v);blzkSetNotes(a);draw()}};
 draw()},"blitzknow60.js");}
