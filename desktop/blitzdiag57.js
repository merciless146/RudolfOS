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
 add('rudolfd',f.rudolfd);add('rudolfd, the start before',f.rudolfdBefore);add('session',f.session);add('session, the start before',f.sessionBefore);add('Lite desktop, the start before',f.liteBefore);add('start-up decisions',f.bootDecisions);add('desktop errors',f.page);add('shutdown',f.shutdown);
 if(f.job)add('last task'+(f.jobName?' ('+f.jobName+')':''),f.job);add('Event Viewer',f.events);
 add('Wi-Fi log',f.wifi);add('Wi-Fi (kernel)',f.wifiKernel);   /* 1.5.9 */
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
 const lines=bzLines(f),picked=o.wifi?lines.filter(x=>/^Wi-Fi/.test(x.src)).slice(-50):o.all?lines.filter(x=>BZ_ERRW.test(x.l)||x.src=='health log'&&!/ (start|ready|shutdown):/.test(x.l)).slice(-24):bzPick(lines,what);   /* the whole computer: its latest problems */
 /* (1.5.9: for the Wi-Fi, no documents: on the Vivobook Blitzen retold 1.5.6's notes from them instead of reading the
    logs. The findings worked out from the lines come first instead, with the settings they point to.) */
 const docs=o.wifi?[]:bzSearch(what+' '+picked.map(x=>x.l).join(' ').slice(0,400),3);
 const parts=[];if(o.detail)parts.push({label:'What failed',text:String(o.detail).slice(0,1500)});
 if(o.wifi){const w=bzWifiFindings(f);parts.push({label:'Findings (worked out from the lines below)',text:w.findings},{label:'Settings these findings point to',text:w.recs})}
 const bySrc={};picked.forEach(x=>(bySrc[x.src]=bySrc[x.src]||[]).push(x.l));
 for(const s in bySrc)parts.push({label:'Log: '+s+' ('+bySrc[s].length+' line'+(bySrc[s].length>1?'s':'')+')',text:bySrc[s].join('\n'),log:1});
 docs.forEach(d=>parts.push({label:'Rudolf OS: '+d.s.replace(/^docs\//,'')+(d.h?' › '+d.h:''),text:d.t}));
 parts.push({label:'This computer',text:'Rudolf OS '+(f.version||RVER)+', up for '+Math.round((f.uptime||0)/60)+' minutes'});
 if(o.wifi&&f.wifiNow)parts.push({label:'The Wi-Fi now (adapter, network, radios)',text:String(f.wifiNow).slice(0,1500)});
 const keep=await bzReview(o.all?"What's wrong with this computer?":'Why? '+what.slice(0,80),parts);if(!keep)return null;
 const ctx=keep.map(p=>'### '+p.label+'\n'+p.text).join('\n\n').slice(0,9000),logs=keep.filter(p=>p.log).map(p=>p.text).join('\n');
 const q=o.wifi?(what&&what!='The Wi-Fi keeps dropping or disconnecting'?what+' (Which Wi-Fi settings should I change?)':'Why does the Wi-Fi keep dropping, and which Wi-Fi settings should I change?'):o.all?"What's wrong with this computer? Look at the problems in the logs and explain the most important ones.":'Why did this happen: '+what;
 blzAskFrom('the logs and documents you allowed',q,ctx,{pack:o.wifi?'wifi':'diag',norun:true,lang:'',onAnswer:(el,text)=>bzAnswerButtons(el,o.all?'A problem on this computer':what,text,logs)});
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
/* 1.5.9: Wi-Fi diagnostics (Settings › Network › Wi-Fi › "Why does the Wi-Fi drop?"): the Wi-Fi's own log, the kernel's
   Wi-Fi messages and the Wi-Fi now (adapter, network, radios with band, signal and state), with what Rudolf OS's
   Wi-Fi settings can and cannot do, so the advice names settings that exist. */
if(typeof BLZ_PACKS!='undefined')BLZ_PACKS.wifi=BLZ_PACKS.diag+`

This is about the Wi-Fi. How to read the lines:
- "wlanN: authenticate with X … authentication with X timed out": the radio X (a router has one per band, 2.4 / 5 / 6 GHz, under one network name) did not answer this computer.
- "disconnect from AP X for new auth to Y": the computer left radio X to try radio Y of the same network; if Y then timed out, that is a drop caused by Y.
- "wifi: network N: left out (did not answer): X": Rudolf OS already left radio X out for a day by itself.
- In "The Wi-Fi now", radio=ADDRESS MHz dBm STATE: below 2500 MHz is 2.4 GHz, 4900 to 5900 is 5 GHz, above 5925 is 6 GHz. dBm: -30 to -55 strong, -55 to -67 good, -67 to -75 weak, below -75 very weak. STATE: inuse, on, off (turned off by the person), failed:TIME (left out by Rudolf OS until then).
You get "Findings (worked out from the lines below)" and "Settings these findings point to": base your answer on them. Say what happened (the drops, which radio), then recommend the settings listed there, in their words; do not add others, and do not describe how Rudolf OS worked in earlier versions.
The Wi-Fi settings that exist, in Settings › Network › Wi-Fi (recommend only these):
- "Radios of this network": each radio of the network in use, with a "Use" switch. Turning off a radio that does not answer, or one that is very weak, keeps the computer on the good one. Warn before suggesting to turn off the radio in use, and never all of them.
- "Adapter" (only with two Wi-Fi adapters, like a USB dongle): which one to use.
- "Country": the channels and power allowed; set it when networks are missing.
- "Forget" a saved network and join it again with its password: when the password changed or joining fails every time.
- Disconnect / Reconnect.
If the lines show the signal is very weak on every radio, say the computer is too far from the router (or a wall is in the way) and no setting fixes that. If nothing in the lines explains the drops, say so.`;
/* a question about a Wi-Fi or internet problem (not how to set it up): "why does my wifi keep dropping", "internet keeps cutting out" */
function bzWifiQ(t){t=String(t||'').toLowerCase();return /\b(wi-?fi|wlan|internet|connection|network|router|signal)\b/.test(t)&&/\b(drops?|dropping|dropped|disconnect\w*|cut(s|ting)? out|keeps?|lost|loses|losing|slow|unstable|not work\w*|won'?t|can'?t|cannot|fails?|failing|stops?|stopping|goes? (out|off|away)|weak)\b/.test(t)}
/* 1.5.9: what the Wi-Fi's lines say, worked out here (a small model reads plain findings far better than raw lines):
   the radios (band, channel, signal in words, state), each drop (the radio left and the one tried, and whether it
   answered), and the settings that follow from them: only settings that exist. */
function bzWifiBand(mhz){return mhz>=5925?'6 GHz':mhz>=4900?'5 GHz':'2.4 GHz'}
function bzWifiCh(mhz){return mhz>=5925?Math.round((mhz-5950)/5):mhz>=4900?Math.round((mhz-5000)/5):mhz==2484?14:Math.round((mhz-2407)/5)}
function bzWifiSig(d){return d>=-55?'strong':d>=-67?'good':d>=-75?'weak':'very weak'}
function bzWifiFindings(f){const now=String(f.wifiNow||''),k=String(f.wifiKernel||''),log=String(f.wifi||''),F=[],R=[],rad={};
 for(const l of now.split('\n')){const m=l.match(/^radio=([0-9a-f:]{17})\t(\d+)\t(-?\d+)\t(\S+)/i);if(m)rad[m[1].toLowerCase()]={b:m[1].toLowerCase(),f:+m[2],s:+m[3],st:m[4]}}
 const ssid=(now.match(/^ssid=(.*)$/m)||[])[1]||'',rs=Object.values(rad),name=b=>{const r=rad[b];return r?`the ${bzWifiBand(r.f)} radio (channel ${bzWifiCh(r.f)}, ${b})`:`radio ${b}`};
 if(ssid)F.push(`Network in use: ${ssid}.`);else F.push('The Wi-Fi is not connected to a network now.');
 const bands=(now.match(/^bands=(.*)$/m)||[])[1];if(bands&&bands.trim())F.push(`This computer's Wi-Fi works on ${bands.trim().split(/\s+/).map(x=>x+' GHz').join(' and ')}.`);
 rs.sort((a,b)=>a.f-b.f).forEach(r=>F.push(`Radio: ${bzWifiBand(r.f)}, channel ${bzWifiCh(r.f)}, signal ${r.s} dBm (${bzWifiSig(r.s)}), ${r.st=='inuse'?'in use now':r.st=='off'?'turned off by the person':r.st.startsWith('failed:')?"left out by Rudolf OS until "+new Date(+r.st.slice(7)*1000).toLocaleTimeString([], {hour:'numeric',minute:'2-digit'})+" because it did not answer":'available'}.`));
 /* the drops, from the kernel's lines */
 const failed=new Set();let drops=0;
 for(const l of k.split('\n')){let m;const t=(l.match(/^\[\s*([\d.]+)\]/)||[])[1],at=t?` (${Math.round(+t)} s after the start)`:'';
  if(m=l.match(/disconnect from AP ([0-9a-f:]{17}) for new auth to ([0-9a-f:]{17})/i)){drops++;F.push(`Drop${at}: the computer left ${name(m[1].toLowerCase())} to try ${name(m[2].toLowerCase())}.`)}
  else if(m=l.match(/(authentication|association) with ([0-9a-f:]{17}) timed out/i)){failed.add(m[2].toLowerCase());F.push(`${name(m[2].toLowerCase()).replace(/^t/,'T')} did not answer${at}: the computer was off the network until it joined a radio again.`)}
  else if(m=l.match(/(deauthenticated|disassociated) from ([0-9a-f:]{17}).*reason[=: ]+(\d+)/i)){drops++;F.push(`The router sent the computer away${at} (${m[1]}, reason ${m[3]}).`)}}
 for(const l of log.split('\n'))if(/left out|turned (on|off) by hand|set aside|chosen again/.test(l))F.push('Rudolf OS: '+l.replace(/^.*wifi: /,''));
 if(!drops&&!failed.size)F.push(`No drop is in the kernel's Wi-Fi lines since this start (up ${Math.round((f.uptime||0)/60)} minutes).`);
 /* the settings that follow */
 for(const b of failed){const r=rad[b];if(r&&r.st=='off')continue;R.push(`Turn off ${name(b)} in Settings › Network › Wi-Fi › Radios of this network (untick Use): it did not answer this computer${r&&r.st.startsWith('failed:')?'; Rudolf OS already leaves it out for a day, turning it off keeps it out for good':''}.`)}
 const on=rs.filter(r=>r.st!='off');
 if(on.length&&on.every(r=>r.s<-75))R.push('Every radio of this network is very weak here: the computer is too far from the router, or walls are in the way. No setting fixes that: move closer, or move the router.');
 else{const cur=rs.find(r=>r.st=='inuse');if(cur&&cur.s<-72){const better=rs.find(r=>r!==cur&&r.st=='on'&&r.s>cur.s+8);if(better)R.push(`The radio in use is weak (${cur.s} dBm) and ${name(better.b)} is stronger (${better.s} dBm): turning off the weak one in Radios of this network moves the computer to the stronger one.`)}}
 if(!R.length)R.push(drops||failed.size?'Nothing to change: Rudolf OS already handles what the lines show.':'Nothing to change from these lines. If the Wi-Fi drops again, ask again right after it happens, while the lines are fresh.');
 return {findings:F.join('\n'),recs:R.map(x=>'- '+x).join('\n')}}
