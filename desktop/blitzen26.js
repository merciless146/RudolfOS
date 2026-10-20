/* ===================== Blitzen: the offline assistant =====================
   Chat and code help (BlockLua, JavaScript, C++, Python) from a small model that runs on this computer.
   rudolfd starts the engine (rudolf-ai + llama.cpp) on demand; this file is the app around it: context packs
   (blitzpacks26.js), streaming, running code to check it, fixing it from the error, BlockLua blocks (blk2lua26.js),
   and "Ask Blitzen" in Notepad, Code Editor, BlockLua Builder and the Terminal. */
const BLZ={st:null,wins:new Set(),retries:2};
const BLZ_LANGS=[['','Auto'],['blocklua','BlockLua'],['lua','Lua'],['luau','Luau'],['pilot','Pilot.lua'],['js','JavaScript'],['cpp','C++'],['python','Python']];
async function blzStatus(){if(!RD)return null;try{BLZ.st=await lxApi('ai/status')}catch(e){BLZ.st=null}return BLZ.st}
function blzGameRunning(){return (typeof EMU!='undefined'&&EMU.players.size>0)||wins.some(w=>{const t=w.el.querySelector('.t');return t&&/Quake/i.test(t.textContent)})}
/* Lua from this page's own copy of Fengari first (works offline), then the CDN as before */
{const _lf=loadFengari;loadFengari=function(){if(window.fengari)return Promise.resolve(true);return new Promise(res=>{const s=document.createElement('script');s.src=(RD?'/':'')+'lib/fengari-web.js';s.onload=()=>window.fengari?res(true):_lf().then(res);s.onerror=()=>_lf().then(res);document.head.appendChild(s)})}}

/* ---------- talking to the model ---------- */
async function blzStream(messages,opt,onText,signal){
 const body={messages,stream:true,temperature:opt.temperature??0.6,top_p:0.9,max_tokens:opt.max||1536,cache_prompt:true};if(opt.grammar)body.grammar=opt.grammar;
 const r=await fetch((window.BLZ_API||'/api/')+'ai/chat',{method:'POST',headers:{'X-Rudolf-Token':RD.token,'Content-Type':'application/json'},body:JSON.stringify(body),signal});
 const ct=r.headers.get('content-type')||'';
 if(!ct.includes('event-stream')){let m='Blitzen did not answer (HTTP '+r.status+')';try{const j=await r.json();m=typeof j.error=='string'?j.error:(j.error&&j.error.message)||j.message||m}catch(e){}throw new Error(m)}
 const rdr=r.body.getReader(),dec=new TextDecoder();let buf='',text='',n=0,first=0;const t0=performance.now();
 for(;;){const {done,value}=await rdr.read();if(done)break;buf+=dec.decode(value,{stream:true});let i;
  while((i=buf.indexOf('\n'))>=0){const l=buf.slice(0,i).trim();buf=buf.slice(i+1);if(!l.startsWith('data:'))continue;const d=l.slice(5).trim();if(d=='[DONE]')continue;
   let j;try{j=JSON.parse(d)}catch(e){continue}if(j.error)throw new Error(j.error.message||'Blitzen stopped');
   const c=j.choices&&j.choices[0]&&j.choices[0].delta&&j.choices[0].delta.content;if(c){if(!first)first=performance.now();text+=c;n++;onText(text)}}}
 const ms=performance.now()-t0;return{text,tokens:n,ms,first:first?first-t0:ms,tps:n>1&&first?(n-1)/((performance.now()-first)/1000):0}}

/* ---------- answers: text and code blocks ---------- */
function blzLang(l){l=(l||'').toLowerCase();return /^(lua|blocklua|lua5\.?3)$/.test(l)?'lua':/^(luau|roblox|rbx|rbxlua)$/.test(l)?'luau':/^(pilot|pilotlua|pilot\.lua|pilot-lua|wos)$/.test(l)?'pilot':/^(js|javascript|mjs|node)$/.test(l)?'js':/^(cpp|c\+\+|cc|cxx|hpp|c)$/.test(l)?'cpp':/^(py|python|python3)$/.test(l)?'python':/^html?$/.test(l)?'html':l||'text'}
/* a ```lua block that is really Luau or Pilot.lua (or asked for as one) is run as that */
function blzLuaKind(lang,code,hint){if(lang!='lua'||typeof luauDialect!='function')return lang;if(hint=='luau'||hint=='pilot')return hint;const d=luauDialect(code);return d=='lua'?'lua':d=='pilot'?'pilot':'luau'}
function blzParts(text,hint){const out=[];const re=/```([\w+#.-]*)[^\n]*\n([\s\S]*?)(```|$)/g;let last=0,m;
 while((m=re.exec(text))){if(m.index>last)out.push({t:'text',s:text.slice(last,m.index)});out.push({t:'code',lang:blzLuaKind(blzLang(m[1]),m[2],hint),s:m[2].replace(/\n$/,''),done:m[3]=='```'});last=re.lastIndex;if(!m[0].length)break}
 if(last<text.length)out.push({t:'text',s:text.slice(last)});return out}
function blzMd(s){return lxEsc(s).replace(/`([^`\n]+)`/g,'<code>$1</code>').replace(/\*\*([^*\n]+)\*\*/g,'<b>$1</b>').replace(/^#{1,4} (.*)$/gm,'<b>$1</b>').replace(/^[-*] /gm,'• ').replace(/\n/g,'<br>')}
const BLZ_LNAME={lua:'BlockLua / Lua',luau:'Luau',pilot:'Pilot.lua',js:'JavaScript',cpp:'C++',python:'Python',html:'HTML',text:'Text'};
function blzNeedsInput(lang,code){return lang=='luau'||lang=='pilot'?false:lang=='lua'?/io\.read|sys\.input/.test(code):lang=='python'?/\binput\s*\(|sys\.stdin/.test(code):lang=='cpp'?/std::cin|\bcin\s*>>|getline\s*\(\s*(std::)?cin|scanf/.test(code):lang=='js'?/\bprompt\s*\(/.test(code):false}

/* ---------- running code ---------- */
async function blzRunLua(code){const out=[];const P={cwd:['C:'],write(s){out.push(String(s))},print(s=''){out.push(s+'\n')}};
 if(!window.fengari&&!(await loadFengari()))return{ok:false,error:'The Lua runtime (Fengari) could not be loaded.'};
 if(!KRUN)KRUN=fengari.load(KERNEL,'=kernel')();
 const {lua,lauxlib}=fengari,L=fengari.L,t0=Date.now();let err=null;
 /* a program that runs too long is stopped: a hook every 100,000 instructions checks the clock */
 try{lua.lua_sethook(L,L2=>{if(Date.now()-t0>10000)lauxlib.luaL_error(L2,fengari.to_luastring('time limit: the program ran for more than 10 seconds'))},lua.LUA_MASKCOUNT,100000)}catch(e){}
 try{err=KRUN(String(code),'BLITZEN',{w:s=>P.write(s)},makeSys(P))}catch(e){err=String(e&&e.message||e)}finally{try{lua.lua_sethook(L,null,0,0)}catch(e){}}
 if(P.__dirty){P.__dirty=0;fsSave();EXPL.forEach(f=>f())}
 return{ok:true,code:err?1:0,timeout:/time limit/.test(err||''),out:out.join('')+(err?String(err)+'\n':'')}}
function blzRunJS(code){return new Promise(res=>{const src=`const __o=[];const __s=v=>typeof v=='string'?v:(()=>{try{return JSON.stringify(v)}catch(e){return String(v)}})();
 console.log=console.info=console.warn=console.error=(...a)=>__o.push(a.map(__s).join(' '));self.fetch=undefined;self.XMLHttpRequest=undefined;self.WebSocket=undefined;self.importScripts=undefined;
 onmessage=async e=>{let err=null;try{await (0,eval)('(async()=>{'+e.data+'\\n})()')}catch(x){err=(x&&x.name?x.name+': ':'')+(x&&x.message||x)}postMessage({out:__o.join('\\n'),err})}`;
 const w=new Worker(URL.createObjectURL(new Blob([src],{type:'text/javascript'})));const tm=setTimeout(()=>{w.terminate();res({ok:true,code:124,timeout:true,out:'time limit: the program ran for more than 10 seconds\n'})},10000);
 w.onmessage=e=>{clearTimeout(tm);w.terminate();const o=e.data.out+(e.data.out?'\n':'');res({ok:true,code:e.data.err?1:0,out:o+(e.data.err?e.data.err+'\n':'')})};
 w.onerror=e=>{clearTimeout(tm);w.terminate();res({ok:true,code:1,out:(e.message||'error')+'\n'})};w.postMessage(code)})}
async function blzRunServer(lang,code){const r=await fetch((window.BLZ_API||'/api/')+'ai/run?lang='+lang,{method:'POST',headers:{'X-Rudolf-Token':RD.token},body:code});let j;try{j=await r.json()}catch(e){return{ok:false,error:'HTTP '+r.status}}return j}
async function blzRun(lang,code){
 if(lang=='lua')return blzRunLua(code);if(lang=='js')return blzRunJS(code);
 if(lang=='luau'||lang=='pilot')return luauBlitzen(code,lang);
 if(lang=='python'||lang=='cpp'){if(!RD)return{ok:false,error:'Python and C++ run on the installed Rudolf OS.'};return blzRunServer(lang,code)}
 if(lang=='html'){rappRunCode('Blitzen preview',code);return{ok:true,code:0,out:'(opened in a preview window)\n',preview:true}}
 return{ok:false,error:'Blitzen cannot run '+(BLZ_LNAME[lang]||lang)+' here.'}}

/* ---------- saving chats (Documents › Blitzen, plain text) ---------- */
function blzChatDir(make){if(!USER)return null;const d=node(homePath('DOCUMENTS'));if(!isDir(d))return null;if(!isDir(d.BLITZEN)){if(!make)return null;d.BLITZEN={}}return d.BLITZEN}
function blzSaveChat(c){if(!c.msgs.length||!ld('bd95blzkeep',true))return;const d=blzChatDir(true);if(!d)return;
 if(!c.file){const t=new Date(),p=n=>String(n).padStart(2,'0');c.file=`CHAT ${t.getFullYear()}-${p(t.getMonth()+1)}-${p(t.getDate())} ${p(t.getHours())}${p(t.getMinutes())}${p(t.getSeconds())}.TXT`}
 d[c.file]='Blitzen chat\n'+c.msgs.map(m=>`### ${m.role=='user'?'You':'Blitzen'}\n${m.content}`).join('\n')+'\n';fsSave();EXPL.forEach(f=>f())}
function blzLoadChat(name){const d=blzChatDir(false);const t=d&&d[name];if(typeof t!='string')return null;const msgs=[];
 t.split(/^### (You|Blitzen)\n/m).slice(1).forEach((x,i,a)=>{if(i%2==0)msgs.push({role:x=='You'?'user':'assistant',content:(a[i+1]||'').replace(/\n$/,'')})});return{msgs,file:name}}

/* ---------- the Blitzen app ---------- */
async function blitzenApp(ask){
 if(!RD)return notify('Blitzen','Blitzen runs on the installed Rudolf OS (bootable disc or USB), where its model can be downloaded.',{kind:'error'});
 if(ld('bd95blzoff',false))return notify('Blitzen','An administrator has turned Blitzen off (Settings › Apps › Blitzen).',{kind:'error'});
 const open=[...BLZ.wins].find(w=>w.el.isConnected);if(open&&!ask){open.focus&&open.focus();return open}
 const W=win('Blitzen','⚡',620,600);BLZ.wins.add(W);W.body.classList.add('blzw');W.body.innerHTML="<div class='msg' style='padding:16px'>Starting…</div>";
 W.onclose=()=>{BLZ.wins.delete(W);W.abort&&W.abort.abort()};
 const st=await blzStatus();
 if(!st)return W.body.innerHTML="<div class='msg' style='padding:16px'>This Rudolf OS has no Blitzen (update it in Settings › Update and recovery).</div>",W;
 if(st.cpu===0)return W.body.innerHTML="<div class='msg' style='padding:16px'>This computer's processor is too old for Blitzen: it needs AVX2, found in Intel and AMD processors made since about 2015.</div>",W;
 if(!st.installed){blzSetupPage(W,ask);return W}
 blzChatUI(W);if(ask)W.blz.ask(ask);return W}
function blzSetupPage(W,ask){const st=BLZ.st;W.setTitle('Set up Blitzen');
 W.body.innerHTML=`<div class='dsetup blzsetup'><h3>⚡ Blitzen, the assistant on this computer</h3>
 <p>Blitzen writes and fixes BlockLua, Lua, Luau, Pilot.lua, JavaScript, C++ and Python, explains code and answers everyday questions. It runs entirely on this computer: nothing you type is sent anywhere.</p>
 <ul><li>Download: the model is about ${st.size_hint||'0.7 GB'}, once. It is kept on the data partition.</li><li>While it answers it uses about 1 GB of memory; it stops by itself after 10 minutes without questions.</li><li>Python and C++ answers can be run once Linux (Debian) is set up.</li></ul>
 <div class='dsform'></div><div class='dsprog' hidden><div class='dsstep'></div><div class='dsbar'><i></i></div></div><div class='msg dserr'></div></div>`;
 const f=W.body.querySelector('.dsform'),err=W.body.querySelector('.dserr');
 if(!st.persistent){f.innerHTML="<p class='msg'>Blitzen's model is kept on the data partition, so it needs Rudolf OS installed (Settings › Update and recovery › Install here…). It cannot be kept on the live disc.</p>";return}
 if(!isAdmin()){f.innerHTML="<p class='msg'>An administrator downloads Blitzen once; after that every account can use it.</p>";return}
 f.innerHTML=`<div class='rmrow' style='justify-content:flex-start'><button class='dsgo pri'>${st.partial?'Continue the download':'Download Blitzen'}</button></div>`;
 f.querySelector('.dsgo').onclick=async()=>{err.textContent='';try{await lxApi('ai/download')}catch(e){return err.textContent=e.message}
  f.hidden=true;const pg=W.body.querySelector('.dsprog');pg.hidden=false;const step=pg.querySelector('.dsstep'),bar=pg.querySelector('.dsbar i');
  const j=await lxJobWait(l=>{let m;if((m=/^step (.*)/.exec(l)))step.textContent=m[1]+'…';else if((m=/^progress (\d+)/.exec(l)))bar.style.width=m[1]+'%';else if(/^done /.test(l)){step.textContent=l.slice(5);bar.style.width='100%'}});
  if(j.status!=0){pg.hidden=true;f.hidden=false;err.textContent=((j.log||'').trim().split('\n').pop()||'The download failed.').replace(/^rudolf-ai: /,'');await blzStatus();if(BLZ.st&&BLZ.st.partial)f.querySelector('.dsgo').textContent='Continue the download';return}
  notify('Blitzen is ready','Open it from the Start menu, or press Ctrl+Alt+B.');await blzStatus();W.setTitle('Blitzen');blzChatUI(W);if(ask)W.blz.ask(ask)}}
function blzChatUI(W){
 W.setTitle('Blitzen');
 W.body.innerHTML=`<div class='blzbar'><select class='blzlang' aria-label='Language'>${BLZ_LANGS.map(([v,n])=>`<option value='${v}'>${n}</option>`).join('')}</select>
  <label class='chk blzchk'><input type='checkbox' class='blzrun'${ld('bd95blzcheck',true)?' checked':''}> Check code by running it</label><span style='flex:1'></span>
  <button class='blzhist' title='Earlier chats'>History</button><button class='blznew' title='Start a new chat'>New chat</button></div>
  <div class='blzlog' role='log' aria-live='polite'></div><div class='blzhl' hidden></div>
  <div class='blzin'><textarea class='blzq' rows='3' placeholder='Ask Blitzen… (Enter sends, Shift+Enter makes a new line)' aria-label='Your question'></textarea><div class='blzbtns'><button class='blzgo pri'>Send</button><button class='blzstop' hidden>Stop</button></div></div>
  <div class='msg blzst'></div>`;
 const q=s=>W.body.querySelector(s),log=q('.blzlog'),ta=q('.blzq'),stEl=q('.blzst'),langSel=q('.blzlang');
 const chat={msgs:[],file:null};let busy=false;
 q('.blzrun').onchange=e=>svj('bd95blzcheck',e.target.checked);
 const scroll=()=>{log.scrollTop=log.scrollHeight};
 const bubble=(role,label)=>{const d=document.createElement('div');d.className='blzm '+(role=='user'?'blzu':'blza');d.innerHTML=`<div class='blzwho'>${label||(role=='user'?'You':'⚡ Blitzen')}</div><div class='blzbody'></div>`;log.appendChild(d);scroll();return d};
 const empty=()=>{log.innerHTML=`<div class='blzhello'><b>⚡ Hi, I'm Blitzen.</b><br>I run on this computer and help with BlockLua, JavaScript, C++ and Python, or anything else you want to talk about.
  <div class='blzsugg'>${['Make blocks that count down from 10 and beep','Write a Python script that lists the files in Documents','Explain what a C++ vector is with a short example','Write JavaScript that sorts a list of names'].map(s=>`<button>${lxEsc(s)}</button>`).join('')}</div></div>`;
  log.querySelectorAll('.blzsugg button').forEach(b=>b.onclick=()=>{ta.value=b.textContent;ta.focus()})};
 /* draw an answer: text, and each code block with its buttons */
 const drawAnswer=(el,text,opts={})=>{const body=el.querySelector('.blzbody');const parts=opts.raw?[{t:'code',lang:'lua',s:text,done:!opts.streaming}]:blzParts(text,langSel.value);body.innerHTML='';
  parts.forEach((p,i)=>{if(p.t=='text'){const d=document.createElement('div');d.className='blztxt';d.innerHTML=blzMd(p.s);body.appendChild(d);return}
   const c=document.createElement('div');c.className='blzcode';c.dataset.lang=p.lang;c.innerHTML=`<div class='blzch'><span class='blzcl'>${lxEsc(BLZ_LNAME[p.lang]||p.lang)}</span><span class='blzbadge'></span><span style='flex:1'></span>${p.done?`<button class='bcr' title='Run it'>▶ Run</button>${p.lang=='lua'?"<button class='bcb' title='Open as blocks in BlockLua Builder'>🧱 Blocks</button>":''}<button class='bci' title='Open in Code Editor'>Insert</button><button class='bcc' title='Copy'>Copy</button>`:''}</div><pre class='blzpre'></pre><div class='blzout' hidden></div>`;
   c.querySelector('pre').textContent=p.s;c.code=p.s;body.appendChild(c);
   if(p.done){c.querySelector('.bcr').onclick=()=>runBlock(c,p.lang,p.s,false);c.querySelector('.bcc').onclick=()=>{navigator.clipboard.writeText(p.s).then(()=>notify('Copied','',{quiet:true})).catch(()=>{})};
    c.querySelector('.bci').onclick=()=>blzInsert(p.lang,p.s);const bb=c.querySelector('.bcb');if(bb)bb.onclick=()=>blzToBlocks(p.s)}});
  scroll()};
 const setBadge=(c,kind,txt)=>{const b=c.querySelector('.blzbadge');b.className='blzbadge '+kind;b.textContent=txt};
 const showOut=(c,res)=>{const o=c.querySelector('.blzout');o.hidden=false;o.innerHTML='';
  if(!res.ok){o.innerHTML=`<div class='msg'>${lxEsc(res.error||'Could not run it.')}</div>`;
   if(res.missing=='python'||res.missing=='cpp'){const b=document.createElement('button');b.textContent=res.missing=='python'?'Install Python':'Install the C++ compiler';b.onclick=()=>blzInstall(res.missing,b,o);o.appendChild(b)}
   else if(res.missing=='debian'){const b=document.createElement('button');b.textContent='Set up Linux (Debian)…';b.onclick=()=>terminalApp();o.appendChild(b)}return}
  const pre=document.createElement('pre');pre.className='blzres'+(res.code?' bad':'');pre.textContent=(res.out||'').trim()||(res.code?'(no output)':'(finished with no output)');o.appendChild(pre)};
 /* run one block; "auto" runs are the check after an answer */
 async function runBlock(c,lang,code,auto){
  if(!['lua','luau','pilot','js','python','cpp','html'].includes(lang)){setBadge(c,'nr','not run');return null}
  if(auto&&lang=='html'){setBadge(c,'nr','not run: a page');return null}
  if(auto&&blzNeedsInput(lang,code)){setBadge(c,'nr','not run: it asks for input');return null}
  setBadge(c,'run','running…');let res;try{res=await blzRun(lang,code)}catch(e){res={ok:false,error:e.message}}
  showOut(c,res);
  if(!res.ok){setBadge(c,'nr','not run');return null}
  if(res.preview){setBadge(c,'ok','opened');return res}
  if(res.checkedOnly){setBadge(c,'ok','✓ types checked');return res}
  setBadge(c,res.code?'bad':'ok',res.code?(res.timeout?'stopped: too slow':'failed'):'✓ checked');return res}
 const stopBtn=q('.blzstop'),goBtn=q('.blzgo');
 const setBusy=b=>{busy=b;stopBtn.hidden=!b;goBtn.disabled=b;ta.disabled=false};
 /* one model answer, streamed into a new bubble */
 async function answer(label,o={}){
  const lang=langSel.value||null,last=chat.msgs.filter(m=>m.role=='user').at(-1);
  const pack=o.pack||blzPickPack(lang,(last&&last.content)||'');
  const sys=BLZ_SYSTEM+(pack?'\n\n'+BLZ_PACKS[pack]:'')+(o.blocks?'\n\nAnswer with Lua only, one block-shaped statement per line, no explanation and no code fence.':'');
  const req=[{role:'system',content:sys},...chat.msgs.slice(-12)];
  const el=bubble('assistant',label);el.querySelector('.blzbody').innerHTML="<span class='blzdots'>…</span>";
  W.abort=new AbortController();setBusy(true);stEl.textContent='Thinking…';let res;
  try{res=await blzStream(req,{grammar:o.blocks?BLZ_BLOCK_GRAMMAR:null,temperature:pack&&pack!='help'?0.5:0.7},t=>drawAnswer(el,t,{raw:o.blocks,streaming:true}),W.abort.signal)}
  catch(e){setBusy(false);if(e.name=='AbortError'){stEl.textContent='Stopped.';const t=el.querySelector('.blzbody').textContent;if(t&&t!='…')chat.msgs.push({role:'assistant',content:el.dataset.text||t});else el.remove();return null}
   el.querySelector('.blzbody').innerHTML=`<div class='msg blzerr'>${lxEsc(e.message)}</div>`;stEl.textContent='';return null}
  setBusy(false);const text=o.blocks?'```lua\n'+res.text.trim()+'\n```':res.text;
  chat.msgs.push({role:'assistant',content:text});drawAnswer(el,text);blzSaveChat(chat);
  stEl.textContent=res.tokens?`${res.tokens} tokens in ${(res.ms/1000).toFixed(1)} s · ${res.tps?res.tps.toFixed(1):'–'} tokens per second · first word after ${(res.first/1000).toFixed(1)} s`:'';
  BLZ.lastSpeed=res.tps;svj('bd95blzspeed',{tps:res.tps,at:Date.now()});return el}
 /* check the answer's first runnable block; on failure ask for a fix, at most BLZ.retries times */
 async function check(el,tries){if(!q('.blzrun').checked||!el)return;const c=[...el.querySelectorAll('.blzcode')].find(x=>['lua','luau','pilot','js','python','cpp'].includes(x.dataset.lang));if(!c)return;
  const res=await runBlock(c,c.dataset.lang,c.code,true);if(!res||!res.code)return;
  if(tries>=BLZ.retries){stEl.textContent='The code still fails after '+BLZ.retries+' fixes; you can edit it or ask again.';return}
  const err=(res.out||'').trim().split('\n').slice(-25).join('\n');
  chat.msgs.push({role:'user',content:`That program ${res.timeout?'ran too long and was stopped':'failed'}. Output:\n\`\`\`\n${err}\n\`\`\`\nFix it and send the whole corrected program.`});
  const fx=await answer(`⚡ Blitzen · fix ${tries+1} of ${BLZ.retries}`,{pack:{lua:langSel.value=='lua'?'lua':'blocklua',luau:'luau',pilot:'pilot',js:'js',python:'python',cpp:'cpp'}[c.dataset.lang]});await check(fx,tries+1)}
 async function ask(text,o={}){text=String(text||'').trim();if(!text||busy)return;
  if(blzGameRunning()){stEl.textContent='Close the game first: Blitzen needs the memory a game is using.';return}
  if(o.lang!==undefined)langSel.value=o.lang;
  if(!chat.msgs.length)log.innerHTML='';chat.msgs.push({role:'user',content:text+(o.context?'\n\n'+o.context:'')});
  const u=bubble('user');u.querySelector('.blzbody').innerHTML=blzMd(text)+(o.context?`<details class='blzctx'><summary>with ${lxEsc(o.contextName||'context')}</summary><pre></pre></details>`:'');if(o.context)u.querySelector('.blzctx pre').textContent=o.context;
  ta.value='';const el=await answer(null,{blocks:o.blocks,pack:o.blocks?'blocklua':o.pack});
  if(el&&o.blocks&&o.onBlocks){const t=chat.msgs.at(-1).content.replace(/^```lua\n|\n```$/g,'');o.onBlocks(t)}
  if(el&&!o.blocks)await check(el,0)}
 goBtn.onclick=()=>ask(ta.value);stopBtn.onclick=()=>W.abort&&W.abort.abort();
 ta.addEventListener('keydown',e=>{e.stopPropagation();if(e.key=='Enter'&&!e.shiftKey){e.preventDefault();ask(ta.value)}});
 q('.blznew').onclick=()=>{if(busy)return;chat.msgs=[];chat.file=null;empty();stEl.textContent=''};
 q('.blzhist').onclick=()=>{const h=q('.blzhl');if(!h.hidden){h.hidden=true;return}const d=blzChatDir(false);const names=d?Object.keys(d).filter(k=>/^CHAT .*\.TXT$/.test(k)).sort().reverse():[];
  h.innerHTML=names.length?names.slice(0,30).map(n=>`<button data-n='${lxEsc(n)}'>${lxEsc(n.replace(/^CHAT |\.TXT$/g,''))}</button>`).join(''):"<div class='msg'>No saved chats yet. Chats are kept in Documents › Blitzen.</div>";h.hidden=false;
  h.querySelectorAll('button').forEach(b=>b.onclick=()=>{const c=blzLoadChat(b.dataset.n);if(!c)return;chat.msgs=c.msgs;chat.file=c.file;log.innerHTML='';c.msgs.forEach(m=>{const el=bubble(m.role);if(m.role=='user')el.querySelector('.blzbody').innerHTML=blzMd(m.content);else drawAnswer(el,m.content)});h.hidden=true})};
 empty();setTimeout(()=>ta.focus(),50);
 W.blz={ask,chat,busy:()=>busy,stop:()=>W.abort&&W.abort.abort(),log};return W}
async function blzInstall(what,btn,box){if(!isAdmin()&&!await elevate(what=='python'?'Install Python in Debian':'Install the C++ compiler in Debian'))return;btn.disabled=true;
 const m=document.createElement('div');m.className='msg';box.appendChild(m);try{await lxApi('ai/install?what='+what)}catch(e){m.textContent=e.message;btn.disabled=false;return}
 m.textContent='Installing… (Debian downloads it)';const j=await lxJobWait();m.textContent=j.status==0?'Installed. Press ▶ Run again.':'Installing failed: '+((j.log||'').trim().split('\n').pop()||'');btn.disabled=j.status==0}
/* code from an answer into the Code Editor */
function blzInsert(lang,code){const W=codeEditor();setTimeout(()=>{const ta=W.body.querySelector('.ceta'),lg=W.body.querySelector('.lg');if(!ta)return;
 const v={lua:'lua',luau:'luau',pilot:'luau',js:'js',python:'py',cpp:'c',html:'html'}[lang]||'txt';if(lg){lg.value=v;lg.onchange&&lg.onchange({target:lg})}ta.value=code;ta.dispatchEvent(new Event('input'))},60)}
/* Lua from an answer into BlockLua Builder as blocks */
function blzToBlocks(code){const bl=blzLuaToBlocks(code);if(!bl.length)return notify('No blocks','The answer has no Lua to turn into blocks.');
 wins.filter(w=>{const t=w.el.querySelector('.t');return t&&t.textContent=='BlockLua Builder'}).forEach(w=>w.close());saved=bl;blocklua();
 const raw=bl.filter(b=>b.t=='lua').length;if(raw)notify('Blocks made',`${bl.length} blocks; ${raw} line${raw>1?'s':''} kept as plain Lua blocks.`,{quiet:true})}

/* ---------- "Ask Blitzen" in other apps ---------- */
function blzAskFrom(app,question,context,o={}){const W=wins.find(w=>BLZ.wins.has(w)&&w.blz);const go=w=>w&&w.blz&&w.blz.ask(question,{context,contextName:app,...o});
 if(W){go(W);return}blitzenApp().then(w=>{if(w&&w.blz)go(w)})}
function blzAskDialog(title,ph){return new Promise(res=>{const o=document.createElement('div');o.className='rmodal';o.innerHTML=`<div class='rbox raise' role='dialog' aria-label='${lxEsc(title)}'><b>⚡ ${lxEsc(title)}</b><textarea rows='3' class='sunk' style='width:100%;margin:8px 0' placeholder='${lxEsc(ph)}'></textarea><div class='rmrow'><button class='ok pri'>Ask</button><button class='no'>Cancel</button></div></div>`;
 document.body.appendChild(o);const t=o.querySelector('textarea');t.focus();const done=v=>{o.remove();res(v)};o.querySelector('.ok').onclick=()=>done(t.value.trim()||null);o.querySelector('.no').onclick=()=>done(null);t.onkeydown=e=>{e.stopPropagation();if(e.key=='Enter'&&!e.shiftKey){e.preventDefault();done(t.value.trim()||null)}if(e.key=='Escape')done(null)}})}
function blzAskButton(label){const b=document.createElement('button');b.className='blzask';b.textContent=label||'⚡ Ask Blitzen';b.title='Ask Blitzen about this';return b}
function blzHook(W,title){if(!RD||!W||!W.body||BLZ.wins.has(W))return;
 if(title=='Notepad'){const bar=W.body.querySelector('.menubar');if(!bar||bar.querySelector('.blzask'))return;const b=blzAskButton();bar.appendChild(b);
  b.onclick=async()=>{const ta=W.body.querySelector('textarea');const sel=ta.value.slice(ta.selectionStart,ta.selectionEnd)||ta.value;const n=(W.el.querySelector('.t').textContent||'').replace(/ - Notepad$/,'');
   const qn=await blzAskDialog('Ask Blitzen about '+(sel==ta.value?'this file':'the selection'),'For example: explain this, or find the mistake');if(!qn)return;
   blzAskFrom(n,qn,'```'+({PY:'python',LUA:'lua',JS:'js',CPP:'cpp',C:'cpp',HTML:'html'}[(n.split('.').pop()||'').toUpperCase()]||'')+'\n'+sel.slice(0,12000)+'\n```')}}
 else if(title=='Code Editor'){const bar=W.body.querySelector('.ceb');if(!bar||bar.querySelector('.blzask'))return;const b=blzAskButton();bar.appendChild(b);
  b.onclick=async()=>{const ta=W.body.querySelector('.ceta'),lg=(W.body.querySelector('.lg')||{}).value;const sel=ta.value.slice(ta.selectionStart,ta.selectionEnd)||ta.value;
   const qn=await blzAskDialog('Ask Blitzen about '+(sel==ta.value?'this file':'the selection'),'For example: explain this, add comments, or find the mistake');if(!qn)return;
   const lang=lg=='luau'?(luauDialect(sel)=='pilot'?'pilot':'luau'):{lua:'blocklua',js:'js',py:'python',c:'cpp'}[lg]||'';blzAskFrom('the code',qn,'```'+({lua:'lua',luau:lang=='pilot'?'pilot':'luau',js:'js',py:'python',c:'cpp',html:'html'}[lg]||'')+'\n'+sel.slice(0,12000)+'\n```',{lang})}}
 else if(title=='BlockLua Builder'){const row=W.body.querySelector('.r')&&W.body.querySelector('.r').parentElement;if(!row||row.querySelector('.blzask'))return;
  const b=blzAskButton('⚡ Make blocks…');row.appendChild(b);
  b.onclick=async()=>{const qn=await blzAskDialog('What should the blocks do?','For example: ask for a name and greet it three times');if(!qn)return;
   blzAskFrom('blocks',qn,null,{lang:'blocklua',blocks:true,onBlocks:t=>blzToBlocks(t)})}}}
{const _w3=win;win=function(...a){const W=_w3.apply(this,a);try{const t=String(a[0]);if(/^(Notepad|Code Editor|BlockLua Builder)$/.test(t))queueMicrotask(()=>blzHook(W,t))}catch(e){}return W}}
/* the Terminal: "Ask Blitzen" sends the last screenful (a command and its error) */
{const _dt=debTermUI;debTermUI=function(W){const r=_dt(W);try{const bar=W.body.querySelector('.dtbar');if(RD&&bar&&!bar.querySelector('.blzask')){const b=blzAskButton('⚡ Ask');b.title='Ask Blitzen about what the terminal shows';bar.insertBefore(b,bar.querySelector('.dtfm'));
  b.onclick=async()=>{const t=W.term&&W.term.cur();if(!t)return;const buf=t.s.t.buffer.active;const lines=[];for(let i=Math.max(0,buf.length-60);i<buf.length;i++){const l=buf.getLine(i);if(l){const s=l.translateToString(true);if(s.trim())lines.push(s)}}
   const qn=await blzAskDialog('Ask Blitzen about the terminal','For example: why did this fail?');if(!qn)return;blzAskFrom('the terminal',qn,'```\n'+lines.slice(-40).join('\n')+'\n```')}}}catch(e){}return r}}

/* ---------- Start menu, keyboard, Settings › Apps › Blitzen ---------- */
regApp('blitzen',{name:'Blitzen',icon:'⚡',kw:'blitzen assistant ai chat code help lua blocklua javascript python c++ offline',open:()=>blitzenApp(),re:/^(Blitzen|Set up Blitzen)/});
{const _sma=startMenuApps;startMenuApps=function(){_sma();const ul=$('#menu ul');if(!ul||!RD)return;ul.querySelectorAll('li.blzv').forEach(x=>x.remove());if(!USER||ld('bd95blzoff',false))return;
 const before=ul.querySelector('li[data-a=settings]')||ul.querySelector('li[data-a=run]');const li=document.createElement('li');li.className='rapp blzv';li.dataset.v25='blitzen';li.tabIndex=0;li.textContent='⚡ Blitzen';
 li.onclick=e=>{e.stopPropagation();$('#menu').style.display='none';blitzenApp()};li.onkeydown=e=>{if(e.key=='Enter')li.click()};ul.insertBefore(li,before)}}
addEventListener('keydown',e=>{if(RD&&e.ctrlKey&&e.altKey&&(e.key=='b'||e.key=='B')&&USER){e.preventDefault();blitzenApp()}});
{const _ap=appsPanel;appsPanel=function(c,W){_ap(c,W);if(!RD)return;
 const d=sp(c,`<h4 class='sh4'>Blitzen (assistant)</h4><div class='bset'><div class='msg'>Checking…</div></div>`);const b=d.querySelector('.bset');
 const draw=async()=>{const st=await blzStatus();if(!st){b.innerHTML="<div class='msg'>This Rudolf OS has no Blitzen.</div>";return}
  const off=ld('bd95blzoff',false),keep=ld('bd95blzkeep',true),sp2=ld('bd95blzspeed',null);
  b.innerHTML=`<table class='tmt'><tbody><tr><td>Model</td><td>${st.installed?lxEsc(st.model||'Blitzen'):'not downloaded'}</td></tr>${st.installed?`<tr><td>Size</td><td>${fmtB(st.size||0)}</td></tr><tr><td>Licence</td><td>${lxEsc(st.licence||'')}</td></tr>`:''}
   <tr><td>Running now</td><td>${st.loaded?'yes':'no (starts when you ask)'}</td></tr><tr><td>Free memory</td><td>${fmtB(st.mem||0)} (needs ${fmtB(st.need||0)})</td></tr>
   <tr><td>Last answer</td><td>${sp2&&sp2.tps?sp2.tps.toFixed(1)+' tokens per second':'–'}</td></tr><tr><td>Python in Debian</td><td>${st.python?'installed':st.debian?'not installed':'Debian not set up'}</td></tr><tr><td>C++ compiler in Debian</td><td>${st.cpp?'installed':st.debian?'not installed':'Debian not set up'}</td></tr></tbody></table>
   <label class='chk'><input type='checkbox' class='bkp'${keep?' checked':''}> Keep chats in Documents › Blitzen</label>
   <label class='chk'><input type='checkbox' class='bof'${off?' checked':''}> Turn Blitzen off for all accounts</label>
   <div class='rmrow' style='justify-content:flex-start'><button class='bop'>${st.installed?'Open Blitzen':'Download…'}</button>${st.loaded?"<button class='bun'>Unload now</button>":''}${st.installed?"<button class='brm'>Remove the model…</button>":''}</div><div class='msg bmsg'></div>`;
  const msg=b.querySelector('.bmsg');b.querySelector('.bkp').onchange=e=>svj('bd95blzkeep',e.target.checked);
  b.querySelector('.bof').onchange=async e=>{if(!isAdmin()&&!await elevate('Turn Blitzen on or off')){e.target.checked=!e.target.checked;return}svj('bd95blzoff',e.target.checked);startMenuApps()};
  b.querySelector('.bop').onclick=()=>blitzenApp();
  const un=b.querySelector('.bun');if(un)un.onclick=async()=>{try{await lxApi('ai/unload')}catch(e){return msg.textContent=e.message}draw()};
  const rm=b.querySelector('.brm');if(rm)rm.onclick=async()=>{if(!await elevate('Remove Blitzen\'s model (your saved chats stay)'))return;try{await lxApi('ai/remove')}catch(e){return msg.textContent=e.message}msg.textContent='Removed.';setTimeout(draw,600)}};
 draw()}}
