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
