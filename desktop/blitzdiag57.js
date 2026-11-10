/* 1.5.7 (plan stage 2): Blitzen's reference material and this computer's facts, for the diagnostics of stage 3.
   bzDocs(): Rudolf OS's documents (blitzen-docs.json beside the desktop, built from docs/ and the CHANGELOG by
   docsindex.py) and the Help pages, as sections. bzSearch(q,n): the best n sections for a question, by words (BM25).
   bzFacts(): rudolfd's ai/facts (the last lines of the logs, with passwords, tokens and keys left out) and the
   desktop's own events (the Event Viewer). Nothing here changes anything. */
const BZD={docs:null,idx:null,loading:null};
const BZ_STOP=new Set('a an and are as at be by can do does for from has have how i if in into is it its me my no not of on or so that the then there this to was what when which why will with you your'.split(' '));
function bzWords(t){return String(t||'').toLowerCase().replace(/<[^>]*>/g,' ').match(/[a-z0-9][a-z0-9.+#-]*[a-z0-9+#]|[a-z0-9]/g)?.filter(w=>!BZ_STOP.has(w))||[]}
function bzHelpSecs(){const out=[];if(typeof HELP=='undefined')return out;
 for(const [k,t,body] of HELP){if(k=='new')continue;let x='';try{x=body()}catch(e){continue}
  const d=document.createElement('div');d.innerHTML=x;const w=(d.textContent||'').replace(/\s+/g,' ').trim().split(' ');
  for(let i=0;i<w.length;i+=150)out.push({s:'Help: '+t,h:t,t:w.slice(i,i+150).join(' '),help:k})}
 return out}
async function bzDocs(){if(BZD.docs)return BZD.docs;if(BZD.loading)return BZD.loading;
 BZD.loading=(async()=>{let docs=[];try{const r=await fetch('blitzen-docs.json',{cache:'no-cache'});if(r.ok){const j=await r.json();if(Array.isArray(j.docs))docs=j.docs}}catch(e){}
  docs=docs.concat(bzHelpSecs());BZD.docs=docs;BZD.idx=bzIndex(docs);BZD.loading=null;return docs})();
 return BZD.loading}
function bzIndex(docs){const df=new Map(),tf=[];let total=0;
 docs.forEach(d=>{const m=new Map();for(const w of bzWords(d.h+' '+d.h+' '+d.t))m.set(w,(m.get(w)||0)+1);let len=0;m.forEach(v=>len+=v);tf.push({m,len});total+=len;m.forEach((v,w)=>df.set(w,(df.get(w)||0)+1))});
 return {df,tf,avg:total/Math.max(1,docs.length),n:docs.length}}
/* BM25 (k1 1.2, b 0.75); headings count twice. The Help pages count 1.5 times. Returns [{s,h,t,score}] best first, only sections that match. */
function bzSearch(q,n){if(!BZD.idx)return[];const {df,tf,avg,n:N}=BZD.idx,qs=[...new Set(bzWords(q))],out=[];
 tf.forEach((x,i)=>{let sc=0;for(const w of qs){const f=x.m.get(w);if(!f)continue;const d=df.get(w);sc+=Math.log(1+(N-d+.5)/(d+.5))*f*2.2/(f+1.2*(.25+.75*x.len/avg))}
  if(sc>0)out.push(Object.assign({score:BZD.docs[i].help?sc*1.5:sc},BZD.docs[i]))});   /* the Help pages are written for the person: ahead on a tie */
 return out.sort((a,b)=>b.score-a.score).slice(0,n||4)}
/* this computer's state: rudolfd's facts and the desktop's events of the last days (newest last) */
async function bzFacts(days){const f={};try{Object.assign(f,await rd('ai/facts'))}catch(e){f.error=e.message}
 const since=Date.now()-(days||3)*864e5,ev=[];
 if(typeof SLOG!='undefined')for(const k of ['sys','fw','av'])for(const e of SLOG[k]||[])if(e.t>=since)ev.push({t:e.t,k,m:e.m});
 f.events=ev.sort((a,b)=>a.t-b.t).slice(-60).map(e=>new Date(e.t).toISOString().slice(0,19).replace('T',' ')+' '+e.k+' '+e.m).join('\n');
 return f}

/* ---------- 1.5.7 (plan stage 3): Blitzen diagnostics: "Why?", "What's wrong with this computer?", "Draft a report".
   Blitzen reads what the person allows (shown first, each part can be left out), answers from the logs and the
   documents, and changes nothing: no commands are run, nothing is sent. */
if(typeof BLZ_PACKS!='undefined')BLZ_PACKS.diag=`You are helping the person understand a problem on their Rudolf OS computer. You get the problem, lines from the computer's logs and sections of Rudolf OS's documents.
- Say in plain words what happened and when, and the likely cause. Quote the log line your answer rests on, in backticks.
- Say how sure you are (sure, likely, or a guess). If nothing in the lines explains it, say so plainly and suggest Report a problem.
- Say what the person can do: a setting to change (name the place, like Settings › System health), something to install, or report it when it is a fault in Rudolf OS.
- Never tell the person to run commands, edit system files or turn off security. You cannot change anything on the computer.
- Be short: at most about 150 words.`;
const BZ_ERRW=/could ?n[o']t|cannot|can't|failed|failure|error|not found|refused|denied|crash|stopped|timed out|timeout|no answer|did not|went wrong|not respond|white screen|restarted/i;
function bzIsError(t,kind){return kind=='error'||kind=='Error'||kind=='Warning'||BZ_ERRW.test(String(t||''))}
function bzCan(){return !!RD&&typeof blzAskFrom=='function'}
/* the log lines, as {src,l}, from ai/facts and the desktop (newest last in each source) */
function bzLines(f){const out=[],add=(src,t)=>String(t||'').split('\n').forEach(l=>{l=l.trim();if(l)out.push({src,l})});
 String(f.health||'').split('\n').forEach(l=>{l=l.trim();if(!l)return;try{const j=JSON.parse(l);out.push({src:'health log',l:new Date(j.t*1000).toISOString().slice(0,19).replace('T',' ')+' '+j.k+': '+j.m})}catch(e){out.push({src:'health log',l})}});
 add('rudolfd',f.rudolfd);add('rudolfd, the start before',f.rudolfdBefore);add('session',f.session);add('desktop errors',f.page);add('shutdown',f.shutdown);
 if(f.job)add('last task'+(f.jobName?' ('+f.jobName+')':''),f.job);add('Event Viewer',f.events);
 if(typeof SYSLOG!='undefined')SYSLOG.slice(-40).forEach(x=>out.push({src:'desktop warnings',l:new Date(x.t).toISOString().slice(0,19).replace('T',' ')+' '+x.k+': '+x.m}));
 return out}
/* the lines that matter for a problem: those sharing its words (best first), errors, then the latest of the health log */
function bzPick(lines,what,max){max=max||14;const qw=new Set(bzWords(what).filter(w=>w.length>2)),sc=lines.map((x,i)=>{let s=0;for(const w of bzWords(x.l))if(qw.has(w))s++;if(BZ_ERRW.test(x.l))s+=.6;if(x.src=='health log')s+=.3;return {x,i,s}});
 const top=sc.filter(o=>o.s>=1).sort((a,b)=>b.s-a.s||b.i-a.i).slice(0,max);
 if(top.length<max)lines.map((x,i)=>({x,i})).filter(o=>o.x.src=='health log').slice(-4).forEach(o=>{if(!top.some(t=>t.i==o.i))top.push({x:o.x,i:o.i,s:0})});
 return top.slice(0,max).sort((a,b)=>a.i-b.i).map(o=>o.x)}
/* the person sees what Blitzen will read and can leave parts out; resolves to the parts kept, or null */
function bzReview(title,parts){return new Promise(res=>{const o=document.createElement('div');o.className='rmodal';
 o.innerHTML=`<div class='rmbox bzrev' role='dialog' aria-label='${lxEsc(title)}' style='max-width:640px;width:92vw'><h3>⚡ ${lxEsc(title)}</h3><p class='msg'>Blitzen will read the parts below to answer. It stays on this computer, and Blitzen changes nothing. Untick anything you would rather it did not read.</p><div class='bzparts' style='max-height:48vh;overflow:auto'></div><div class='rmrow'><button class='no'>Cancel</button><button class='ok pri'>Ask Blitzen</button></div></div>`;
 const box=o.querySelector('.bzparts');parts.forEach((p,i)=>{const d=document.createElement('div');d.style.cssText='border-bottom:1px solid rgba(128,128,128,.2);padding:4px 0';
  d.innerHTML=`<label class='ck'><input type='checkbox' data-i='${i}' checked> <b></b></label><pre style='white-space:pre-wrap;margin:2px 0 0 22px;font-size:12px;max-height:120px;overflow:auto'></pre>`;d.querySelector('b').textContent=p.label;d.querySelector('pre').textContent=p.text;box.appendChild(d)});
 const done=v=>{o.remove();res(v)};o.querySelector('.no').onclick=()=>done(null);
 o.querySelector('.ok').onclick=()=>done(parts.filter((p,i)=>o.querySelector(`input[data-i='${i}']`).checked));
 o.addEventListener('keydown',e=>{if(e.key=='Escape')done(null)});document.body.appendChild(o);o.querySelector('.ok').focus()})}
/* gather, show, ask. what: the problem in words; o.all: the whole computer (System health's question) */
async function bzWhy(what,o){o=o||{};if(!bzCan())return notify('Blitzen is not available','Blitzen needs the installed Rudolf OS.');
 if(typeof blzGameRunning=='function'&&blzGameRunning())return notify('Close the game first','Blitzen needs the memory a game is using.');
 const [f]=await Promise.all([bzFacts(o.all?7:3),bzDocs()]);
 const lines=bzLines(f),picked=o.all?lines.filter(x=>BZ_ERRW.test(x.l)||x.src=='health log'&&!/ (start|ready|shutdown):/.test(x.l)).slice(-24):bzPick(lines,what);   /* the whole computer: its latest problems */
 const docs=bzSearch(what+' '+picked.map(x=>x.l).join(' ').slice(0,400),3);
 const parts=[];if(o.detail)parts.push({label:'What failed',text:String(o.detail).slice(0,1500)});
 const bySrc={};picked.forEach(x=>(bySrc[x.src]=bySrc[x.src]||[]).push(x.l));
 for(const s in bySrc)parts.push({label:'Log: '+s+' ('+bySrc[s].length+' line'+(bySrc[s].length>1?'s':'')+')',text:bySrc[s].join('\n'),log:1});
 docs.forEach(d=>parts.push({label:'Rudolf OS: '+d.s.replace(/^docs\//,'')+(d.h?' › '+d.h:''),text:d.t}));
 parts.push({label:'This computer',text:'Rudolf OS '+(f.version||RVER)+', up for '+Math.round((f.uptime||0)/60)+' minutes'});
 const keep=await bzReview(o.all?"What's wrong with this computer?":'Why? '+what.slice(0,80),parts);if(!keep)return null;
 const ctx=keep.map(p=>'### '+p.label+'\n'+p.text).join('\n\n').slice(0,9000),logs=keep.filter(p=>p.log).map(p=>p.text).join('\n');
 const q=o.all?"What's wrong with this computer? Look at the problems in the logs and explain the most important ones.":'Why did this happen: '+what;
 blzAskFrom('the logs and documents you allowed',q,ctx,{pack:'diag',norun:true,lang:'',onAnswer:(el,text)=>bzAnswerButtons(el,o.all?'A problem on this computer':what,text,logs)});
 return keep}
function bzAnswerButtons(el,what,text,logs){const r=document.createElement('div');r.className='rmrow bzact';r.style.cssText='justify-content:flex-start;gap:6px;margin-top:6px';
 const b=document.createElement('button');b.textContent='🐞 Draft a report';b.title='Report a problem, filled in with this answer and the log lines (you read it before anything is sent)';
 b.onclick=()=>bzDraftReport(what,text,logs);r.appendChild(b);
 const s=document.createElement('button');s.textContent='⚙️ System health';s.onclick=()=>settings('health');r.appendChild(s);
 (el.querySelector('.blzbody')||el).appendChild(r)}
/* Report a problem, filled in: the title, what happened (with Blitzen's explanation), and the log lines */
function bzDraftReport(what,answer,logs){if(typeof sysReportDialog!='function')return;
 const t=String(answer||'').replace(/```[\s\S]*?```/g,'').trim();
 sysReportDialog({title:String(what||'').replace(/\s+/g,' ').slice(0,80),text:'What happened: '+what+'\n\nWhat I was doing: \n\nBlitzen\'s explanation (from this computer\'s logs):\n'+t,logs:String(logs||'').split('\n').filter(Boolean)})}
/* a small "⚡ Why?" button for a notification or a log line that tells of a failure (n: {t,b,k}) */
function bzWhyBtn(host,n,o){try{if(!host||!bzCan()||!bzIsError(n.t+' '+n.b,n.k))return;const b=document.createElement('button');b.className='bzwhy';b.textContent='⚡ Why?';
 b.title='Ask Blitzen why this happened (it reads the logs you allow; it changes nothing)';b.style.cssText='margin-left:6px;font-size:11px;padding:0 6px';
 b.onclick=e=>{e.stopPropagation();const t=host.closest&&host.closest('.ntoast');if(t)t.remove();bzWhy((n.t+(n.b?' — '+n.b:'')).slice(0,300),Object.assign({detail:n.t+(n.b?'\n'+n.b:'')},o||{}))};host.appendChild(b)}catch(e){}}
