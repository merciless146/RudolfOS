/* ===================== 1.6.1 (docs/plan-1.6.1.md Part 27): Blitzen's code runs, links and actions sandboxed =====================
   The owner, 2026-11-21: "Add items 1, 2, and 3, as well as the 'learn from what I do' and 'developer mode's test-computer commands'
   options to the next beta". Code Blitzen writes is the model's, and the model reads what it is given (a web page from Search the web,
   a file, a pasted text): whatever such a text makes it write must not act with the desktop's rights.
   1. JavaScript Blitzen writes runs in a frame of its own with no origin (sandbox="allow-scripts", never allow-same-origin) whose
      policy (BLZBOX_CSP) allows no request to anywhere, and inside it in a worker: no page, no window to post to, no storage of the
      desktop's, no network (also for workers it starts, which take the policy with them). Output and the 10-second limit as before.
   2. Lua Blitzen writes runs there too (Fengari's own copy, the kernel's sys over a copy of the account's own files, in memory):
      "Check code by running it" throws the copy away; a run started with ▶ Run that changed files asks, in the desktop's own
      dialog, before anything of the person's is changed, listing each file, and only within the account's own folder.
      Graphics, Emu, Quake and opening apps are not there (Insert puts the program in Code Editor, where they are).
      An HTML answer's preview: a frame with no origin inside a frame that may load nothing (no address it could send data to:
      not an image, not a form, not a link, not itself).
   3. Links in answers: only the addresses of the pages the web search read (the numbered sources) and Rudolf OS's own documents
      open with a click; any other address is shown whole and opens only after "Open this address?" (its site in bold, what it
      would carry listed). Answers stay text (blzMd): no image, nothing that loads an address by itself. */
const BLZBOX={fengari:null,runs:0};
const BLZBOX_CSP="default-src 'none'; script-src 'unsafe-inline' 'unsafe-eval'; worker-src blob:; form-action 'none'; base-uri 'none'";
/* (the frame's own script: one run's worker started from what the desktop sends, its one answer sent back) */
function blzBoxFrame(){let done=false;
 onmessage=e=>{if(e.source!==parent||done)return;const m=e.data;if(!m||typeof m.worker!='string')return;onmessage=null;
  const end=d=>{if(done)return;done=true;try{w&&w.terminate()}catch(x){}parent.postMessage(Object.assign({nonce:m.nonce},d),'*')};let w=null;
  try{w=new Worker(URL.createObjectURL(new Blob([m.worker],{type:'text/javascript'})))}catch(x){return end({out:'',err:'The sandbox could not start: '+x})}
  w.onmessage=r=>{const d=r.data||{};end({out:String(d.out==null?'':d.out).slice(0,200000),err:d.err==null?null:String(d.err).slice(0,4000),files:d.files||null})};
  w.onerror=r=>{r.preventDefault&&r.preventDefault();end({out:'',err:String(r.message||'error')})};
  const g=Object.assign({},m);delete g.worker;delete g.nonce;w.postMessage(g)}}
/* (JavaScript, in the worker: console output collected, the program run as an async function) */
function blzBoxJS(){const __o=[];let __n=0;const __s=v=>typeof v=='string'?v:(()=>{try{return JSON.stringify(v)}catch(e){return String(v)}})();
 console.log=console.info=console.warn=console.error=console.debug=(...a)=>{if(__n>200000)return;const t=a.map(__s).join(' ');__n+=t.length;__o.push(t)};
 onmessage=async e=>{onmessage=null;let err=null;try{await (0,eval)('(async()=>{'+e.data.code+'\n})()')}catch(x){err=(x&&x.name?x.name+': ':'')+(x&&x.message||x)}postMessage({out:__o.join('\n'),err})}}
/* (Lua, in the worker: Fengari, the kernel and a sys over the copy of the files; what the copy is at the end goes back when changed) */
function blzBoxLua(){onmessage=e=>{onmessage=null;const m=e.data,out=[];let n=0,dirty=false;const w=s=>{s=String(s);if(n>200000)return;n+=s.length;out.push(s)};
 console.log=console.warn=console.error=()=>{};self.window=self;
 try{(0,eval)(m.fengari)}catch(x){postMessage({out:'',err:'The Lua runtime (Fengari) could not start: '+x});return}
 const F=self.fengari;if(!F){postMessage({out:'',err:'The Lua runtime (Fengari) could not start.'});return}
 const fs=m.files&&typeof m.files=='object'?m.files:{'C:':{}},cwd=m.cwd;
 const isDir=x=>x&&typeof x=='object';
 const resolve=(p,c)=>{let a=/^[A-Z]:/i.test(p)?[]:c.slice();for(const x of p.split('\\').filter(Boolean)){if(x=='..')a.length>1&&a.pop();else if(x!='.'&&!/^C:$/i.test(x))a.push(x.toUpperCase())}return a[0]=='C:'?a:['C:',...a]};
 const node=a=>{let x=fs;for(const k of a){if(!isDir(x)||!(k in x))return;x=x[k]}return x},dirOf=a=>node(a.slice(0,-1));
 const rp=p=>resolve(String(p).replace(/\//g,'\\'),cwd),last=a=>a[a.length-1];
 const NA='ERR:Not available when Blitzen runs a program (Insert it into Code Editor and run it there).';const inp=(m.input||[]).slice();
 const sys={version:()=>m.version,user:()=>m.user,cwd:()=>cwd.join('\\'),env:k=>({USER:m.user,OS:'Rudolf OS',COMSPEC:'C:\\WINDOWS\\COMMAND.COM',PATH:'C:\\PROGRAMS',CD:cwd.join('\\')})[String(k).toUpperCase()],
  read:p=>{const x=node(rp(p));return typeof x=='string'?x:undefined},exists:p=>node(rp(p))!==undefined,isdir:p=>isDir(node(rp(p))),
  write:(p,d)=>{const a=rp(p),dir=dirOf(a);if(!isDir(dir)||a.length<2||isDir(dir[last(a)]))return false;dir[last(a)]=String(d);dirty=true;return true},
  append:(p,d)=>{const a=rp(p),dir=dirOf(a);if(!isDir(dir)||a.length<2||isDir(dir[last(a)]))return false;const k=last(a);dir[k]=(typeof dir[k]=='string'?dir[k]:'')+String(d);dirty=true;return true},
  list:p=>{const x=node(rp(p));if(!isDir(x))return undefined;return Object.keys(x).sort().map(k=>isDir(x[k])?k+'\\':k).join('\n')},
  mkdir:p=>{const a=rp(p),dir=dirOf(a);if(!isDir(dir)||a.length<2||(last(a) in dir))return false;dir[last(a)]={};dirty=true;return true},
  remove:p=>{const a=rp(p),dir=dirOf(a);if(a.length<2||!isDir(dir)||!(last(a) in dir))return false;delete dir[last(a)];dirty=true;return true},
  rename:(x,y)=>{const s=rp(x),t=rp(y),d1=dirOf(s),d2=dirOf(t);if(!isDir(d1)||!isDir(d2)||!(last(s) in d1)||(last(t) in d2))return false;d2[last(t)]=d1[last(s)];delete d1[last(s)];dirty=true;return true},
  emu:()=>NA,gl:()=>NA,quake:()=>NA,glrun:()=>NA,beep:()=>{},openapp:()=>false,
  input:()=>{if(!inp.length)return undefined;const v=String(inp.shift());w(v+'\n');return v}};
 let err=null;const {lua,lauxlib}=F,L=F.L,t0=Date.now();
 try{lua.lua_sethook(L,L2=>{if(Date.now()-t0>10000)lauxlib.luaL_error(L2,F.to_luastring('time limit: the program ran for more than 10 seconds'))},lua.LUA_MASKCOUNT,100000)}catch(x){}
 try{const K=F.load(m.kernel,'=kernel')();err=K(String(m.code),'BLITZEN',{w},sys)}catch(x){err=String(x&&x.message||x)}
 postMessage({out:out.join(''),err:err==null?null:String(err),files:dirty?fs:null})}}
/* one run in a new sandbox: {out, err, files (the copy when the program changed it), timeout} */
function blzBoxRun(kind,msg,limit){return new Promise(res=>{const fr=document.createElement('iframe'),nonce=Math.random().toString(36).slice(2)+(++BLZBOX.runs);
 fr.setAttribute('sandbox','allow-scripts');fr.className='blzbox';fr.title='Blitzen’s sandbox';fr.setAttribute('aria-hidden','true');fr.tabIndex=-1;
 fr.style.cssText='position:fixed;left:-10000px;top:0;width:1px;height:1px;border:0;visibility:hidden';
 fr.srcdoc=`<!doctype html><meta http-equiv="Content-Security-Policy" content="${BLZBOX_CSP}"><script>(${blzBoxFrame})()<\/script>`;
 let over=false;const end=r=>{if(over)return;over=true;clearTimeout(tm);removeEventListener('message',h);fr.remove();res(r)};
 const h=e=>{if(e.source!==fr.contentWindow||!e.data||e.data.nonce!==nonce)return;end({out:e.data.out||'',err:e.data.err,files:e.data.files||null})};
 const tm=setTimeout(()=>end({out:'',err:null,timeout:true}),limit);addEventListener('message',h);
 fr.onload=()=>{if(!over&&fr.contentWindow)fr.contentWindow.postMessage(Object.assign({worker:`(${kind})()`,nonce},msg),'*')};document.body.appendChild(fr)})}
async function blzRunJSBox(code){const r=await blzBoxRun(blzBoxJS,{code:String(code)},10000);
 if(r.timeout)return{ok:true,code:124,timeout:true,out:'time limit: the program ran for more than 10 seconds\n'};
 const o=r.out+(r.out?'\n':'');return{ok:true,code:r.err?1:0,out:o+(r.err?r.err+'\n':'')}}
/* Fengari's source, for the sandbox (the page's own copy first, as loadFengari) */
async function blzBoxFengari(){if(BLZBOX.fengari)return BLZBOX.fengari;
 for(const u of [(RD?'/':'')+'lib/fengari-web.js','https://cdn.jsdelivr.net/npm/fengari-web@0.1.4/dist/fengari-web.js']){try{const r=await fetch(u);if(r.ok){const t=await r.text();if(/window\.fengari=/.test(t))return BLZBOX.fengari=t}}catch(e){}}return null}
/* the account's own files, copied (big files and anything past 8 MB left out: they are not there for the program) */
function blzBoxHome(){return USER&&typeof homePath=='function'?homePath():null}
function blzBoxCopy(){const home=blzBoxHome(),root={'C:':{}};if(!home)return{root,home:null,left:0};let left=0,total=0;
 const cp=n=>{const o={};for(const k of Object.keys(n)){const v=n[k];if(isDir(v))o[k]=cp(v);else if(typeof v=='string'){if(v.length>1048576||total+v.length>8388608){left++;continue}total+=v.length;o[k]=v}}return o};
 let d=root['C:'];for(const k of home.slice(1,-1)){d=d[k]={}}const h=node(home);d[home.at(-1)]=isDir(h)?cp(h):{};return{root,home,left}}
/* what the program did to the copy: [{path, what:'new'|'changed'|'removed'|'folder'|'folder removed', data}] */
function blzBoxDiff(a,b,at=[],out=[]){const isF=v=>typeof v=='string',own=(x,k)=>isDir(x)&&Object.prototype.hasOwnProperty.call(x,k);
 if(!isDir(b))return out;for(const k of Object.keys(b)){const v=b[k],o=own(a,k)?a[k]:undefined,p=[...at,k];if(out.length>500)break;
  if(isDir(v)){if(!isDir(o))out.push({path:p,what:'folder'});blzBoxDiff(isDir(o)?o:{},v,p,out)}
  else if(isF(v)){if(o===undefined||isDir(o))out.push({path:p,what:'new',data:v});else if(o!==v)out.push({path:p,what:'changed',data:v})}}
 if(isDir(a))for(const k of Object.keys(a))if(!own(b,k))out.push({path:[...at,k],what:isDir(a[k])?'folder removed':'removed'});
 return out}
/* may this change go into the person's files? null when it may, else why not */
function blzBoxWhyNot(c,home,orig){const p=c.path;if(!home||p.length<=home.length||home.some((x,i)=>p[i]!==x))return 'outside your own folder';
 if(p.some(k=>typeof k!='string'||!k||k.length>255||/[\\\/\x00-\x1f\x7f]/.test(k)||/^(__proto__|constructor|prototype|\.|\.\.)$/.test(k)))return 'not a name a file can have';
 if(typeof secLocked=='function'&&secLocked(p))return 'a kind of file a program may not write';if(typeof PROT!='undefined'&&PROT.includes(p.join('/')))return 'a protected folder';
 const cur=node(p),was=(()=>{let x=orig;for(const k of p){if(!isDir(x)||!(k in x))return undefined;x=x[k]}return x})();
 if(c.what=='new'&&cur!==undefined)return isDir(cur)?'a folder of that name is there':'a file of that name is there (too large to have been given to the program)';
 if((c.what=='changed'||c.what=='removed')&&cur!==was)return 'it changed meanwhile';if(c.what=='folder removed'&&!isDir(cur))return 'it is not there';return null}
function blzBoxSize(s){return typeof fmtB=='function'?fmtB(new Blob([s]).size):s.length+' characters'}
/* the desktop's own dialog: which of the person's files a program Blitzen wrote would change; nothing until "Keep these changes" */
function blzBoxAsk(ch,home,orig){return new Promise(res=>{const o=document.createElement('div');o.className='rmodal';const esc=s=>lxEsc(String(s));
 const ok=ch.filter(c=>!c.why),no=ch.filter(c=>c.why),W={'new':'new file','changed':'changed','removed':'removed (to the Recycle Bin)','folder':'new folder','folder removed':'folder removed (to the Recycle Bin)'};
 const row=c=>`<tr><td class='blzbxp'>${esc(c.path.join('\\'))}</td><td>${esc(W[c.what]||c.what)}${c.data!=null?', '+esc(blzBoxSize(c.data)):''}${c.why?' · <b>not allowed: '+esc(c.why)+'</b>':''}</td></tr>`;
 o.innerHTML=`<div class='rbox raise blzbxask' role='dialog' aria-label='Keep what the program changed?'><b>⚡ Keep what the program changed?</b>
  <div class='msg' style='margin:8px 0'>Blitzen’s program ran on a copy of your files. Nothing of yours has been changed yet.${ok.length?' With “Keep these changes”, only these change:':''}</div>
  <div class='blzbxl sunk'><table class='tmt'><tbody>${ok.map(row).join('')}${no.map(row).join('')}</tbody></table></div>
  <div class='rmrow'>${ok.length?"<button class='ok'>Keep these changes</button>":''}<button class='no pri'>${ok.length?'Discard':'OK'}</button></div></div>`;
 const end=v=>{o.remove();res(v)};const k=o.querySelector('.ok');if(k)k.onclick=()=>end(true);o.querySelector('.no').onclick=()=>end(false);
 o.addEventListener('keydown',e=>{e.stopPropagation();if(e.key=='Escape')end(false)});document.body.appendChild(o);o.querySelector('.no').focus()})}
/* the changes allowed: into the files (removals to the Recycle Bin), as any app's */
function blzBoxApply(ch){let n=0;const rm=[];
 for(const c of ch){if(c.why)continue;const p=c.path,d=dirOf(p);
  if(c.what=='folder'){if(isDir(d)&&!(p.at(-1) in d)){d[p.at(-1)]={};n++}}
  else if(c.what=='new'||c.what=='changed'){if(isDir(d)&&!isDir(d[p.at(-1)])){d[p.at(-1)]=c.data;n++}}
  else rm.push(p)}
 if(rm.length&&typeof trashPut=='function')n+=trashPut(rm)||0;if(n){fsSave();EXPL.forEach(f=>f())}return n}
/* the input a program reads (io.read, sys.input), asked before it runs (the sandbox cannot stop to ask) */
function blzBoxInput(){return new Promise(res=>{const o=document.createElement('div');o.className='rmodal';
 o.innerHTML=`<div class='rbox raise' role='dialog' aria-label='The program reads what you type'><b>⚡ The program reads what you type</b><div class='msg' style='margin:8px 0'>Type its input first: one line for each time it asks.</div><textarea rows='4' class='sunk' style='width:100%'></textarea><div class='rmrow'><button class='ok pri'>Run</button><button class='no'>Cancel</button></div></div>`;
 document.body.appendChild(o);const t=o.querySelector('textarea');t.focus();const end=v=>{o.remove();res(v)};
 o.querySelector('.ok').onclick=()=>end(t.value.replace(/\r/g,'').split('\n').filter((x,i,a)=>i<a.length-1||x!==''));o.querySelector('.no').onclick=()=>end(null);
 t.onkeydown=e=>{e.stopPropagation();if(e.key=='Escape')end(null)}})}
async function blzRunLuaBox(code,o={}){const src=await blzBoxFengari();if(!src)return{ok:false,error:'The Lua runtime (Fengari) could not be loaded.'};
 let input=[];if(!o.auto&&typeof blzNeedsInput=='function'&&blzNeedsInput('lua',code)){input=await blzBoxInput();if(input===null)return{ok:false,error:'Not run.'}}
 const C=blzBoxCopy(),orig=JSON.parse(JSON.stringify(C.root));
 const r=await blzBoxRun(blzBoxLua,{code:String(code),fengari:src,kernel:KERNEL,files:C.root,cwd:C.home||['C:'],user:USER?USER.n:'admin',version:'Rudolf OS '+(typeof RVER=='string'?RVER:'1.0'),input},12000);
 if(r.timeout)return{ok:true,code:124,timeout:true,out:'time limit: the program ran for more than 10 seconds\n'};
 let out=r.out+(r.err?r.err+'\n':'');const ch=r.files&&isDir(r.files)?blzBoxDiff(orig,r.files):[];
 if(ch.length){ch.forEach(c=>c.why=blzBoxWhyNot(c,C.home,orig));
  if(o.auto)out+=`(It changed ${ch.length} file${ch.length>1?'s':''} in a copy of your files, thrown away: nothing of yours was changed. ▶ Run asks before keeping changes.)\n`;
  else if(await blzBoxAsk(ch,C.home,orig)){const n=blzBoxApply(ch);out+=`(${n} change${n==1?'':'s'} kept in your files.)\n`}
  else out+='(Its changes to your files were not kept.)\n'}
 return{ok:true,code:r.err?1:0,timeout:/time limit/.test(r.err||''),out,changes:ch.length}}
/* ---------- an HTML answer's preview: no origin, nothing loaded from anywhere, no address it could reach ---------- */
const BLZBOX_PAGE_CSP="default-src 'none'; script-src 'unsafe-inline' 'unsafe-eval'; style-src 'unsafe-inline'; img-src data: blob:; font-src data:; media-src data: blob:; form-action 'none'; base-uri 'none'";
const BLZBOX_OUTER_CSP="default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; frame-src 'none'; child-src 'none'; form-action 'none'; base-uri 'none'";
function blzBoxPreviewFrame(){onmessage=e=>{if(e.source!==parent||!e.data||typeof e.data.html!='string')return;onmessage=null;const f=document.createElement('iframe');
 f.setAttribute('sandbox','allow-scripts allow-modals');f.srcdoc='<!doctype html><meta charset="utf-8"><meta http-equiv="Content-Security-Policy" content="'+e.data.csp.replace(/"/g,'&quot;')+'">'+e.data.html;document.body.appendChild(f)}}
function blzBoxPreview(code){const W=win('Blitzen preview','🧩',520,460);W.body.style.padding='0';W.body.style.display='flex';W.body.style.flexDirection='column';
 const n=document.createElement('div');n.className='msg blzbxnote';n.textContent='A preview of Blitzen’s page: it loads nothing from the internet and cannot open addresses.';
 const fr=document.createElement('iframe');fr.setAttribute('sandbox','allow-scripts');fr.className='blzbxpv';fr.title='Blitzen preview';fr.style.cssText='border:0;flex:1;width:100%;background:#fff';
 fr.srcdoc=`<!doctype html><meta http-equiv="Content-Security-Policy" content="${BLZBOX_OUTER_CSP}"><style>html,body{margin:0;height:100%}iframe{border:0;width:100%;height:100%;display:block}</style><body><script>(${blzBoxPreviewFrame})()<\/script>`;
 fr.onload=()=>{fr.contentWindow&&fr.contentWindow.postMessage({html:String(code),csp:BLZBOX_PAGE_CSP},'*');fr.onload=null};W.body.appendChild(n);W.body.appendChild(fr);return W}

/* ---------- links in answers ---------- */
/* Rudolf OS's own documents: the official repository's pages, with nothing after the path */
function blzLinkOwn(u){try{const x=new URL(u);return x.protocol=='https:'&&x.hostname=='github.com'&&/^\/merciless146\/RudolfOS(\/(blob|tree)\/[\w.\/-]*)?\/?$/i.test(x.pathname)&&!x.search&&!x.hash&&!x.username&&!x.password}catch(e){return false}}
function blzLinkTrusted(u,know){if(know&&Array.isArray(know.sources)&&know.sources.some(s=>s&&s.kind=='web'&&s.url===u))return true;return blzLinkOwn(u)}
const BLZ_URL_RE=/\bhttps?:\/\/[^\s<>"'`\\]+/gi;
/* an answer's addresses as links (made as elements: the text stays text); the sources it was given decide which open at once */
function blzLinkify(el,know){if(!el)return;const body=el.querySelector('.blzbody');if(!body)return;
 const tw=document.createTreeWalker(body,NodeFilter.SHOW_TEXT,{acceptNode:n=>n.parentElement&&n.parentElement.closest('.blztxt')&&!n.parentElement.closest('.blzlink')?NodeFilter.FILTER_ACCEPT:NodeFilter.FILTER_REJECT});
 const nodes=[];while(tw.nextNode())nodes.push(tw.currentNode);
 for(const t of nodes){const s=t.nodeValue;BLZ_URL_RE.lastIndex=0;if(!BLZ_URL_RE.test(s))continue;BLZ_URL_RE.lastIndex=0;const f=document.createDocumentFragment();let at=0,m;
  while((m=BLZ_URL_RE.exec(s))){let u=m[0].replace(/[.,;:!?)\]}>*]+$/,'');if(!u.length)continue;f.appendChild(document.createTextNode(s.slice(at,m.index)));
   const a=document.createElement('span');a.className='blzlink'+(blzLinkTrusted(u,know)?' blzlinkok':' blzlinkq');a.textContent=u;a.tabIndex=0;a.setAttribute('role','link');
   a.title=a.classList.contains('blzlinkok')?'Open it in Oracle (a page Blitzen read)':'Blitzen wrote this address: you are asked before it opens';
   a.onclick=e=>{e.preventDefault();blzLinkOpen(u,know)};a.onkeydown=e=>{if(e.key=='Enter'){e.preventDefault();blzLinkOpen(u,know)}};f.appendChild(a);at=m.index+u.length;BLZ_URL_RE.lastIndex=at}
  f.appendChild(document.createTextNode(s.slice(at)));t.replaceWith(f)}}
function blzLinkGo(u){return typeof webOpen=='function'?webOpen(u):window.open(u,'_blank','noopener')}
async function blzLinkOpen(u,know){if(blzLinkTrusted(u,know))return blzLinkGo(u);if(await blzLinkAsk(u))blzLinkGo(u)}
/* "Open this address?": the whole address, its site in bold, and each thing it would carry (the path's and the query's parts) */
function blzLinkAsk(u){return new Promise(res=>{let x;try{x=new URL(u)}catch(e){return res(false)}const esc=s=>lxEsc(String(s));
 const q=[...x.searchParams.entries()],o=document.createElement('div');o.className='rmodal';
 o.innerHTML=`<div class='rbox raise blzlask' role='dialog' aria-label='Open this address?'><b>⚡ Open this address?</b>
  <div class='msg' style='margin:8px 0'>Blitzen wrote this address in its answer; it is not one of the pages it read. An address can carry what you typed, or what Blitzen read, to the site it names.</div>
  <div class='blzlurl sunk'>${esc(x.protocol)}//${x.username||x.password?'<b>(a name and password: )</b>':''}<b class='blzlhost'>${esc(x.host)}</b>${esc(x.pathname)}${esc(x.search)}${esc(x.hash)}</div>
  <table class='tmt' style='margin:6px 0'><tbody><tr><td>Site</td><td><b>${esc(x.hostname)}</b>${x.protocol=='http:'?' (not encrypted: http)':''}</td></tr>
  ${q.length?q.map(([k,v])=>`<tr><td>It sends</td><td><code>${esc(k)}</code> = <code class='blzlv'>${esc(v)}</code></td></tr>`).join(''):"<tr><td>It sends</td><td>nothing after the address</td></tr>"}${x.hash?`<tr><td>Also</td><td><code>${esc(x.hash)}</code></td></tr>`:''}</tbody></table>
  <div class='rmrow'><button class='ok'>Open</button><button class='no pri'>Cancel</button></div></div>`;
 const end=v=>{o.remove();res(v)};o.querySelector('.ok').onclick=()=>end(true);o.querySelector('.no').onclick=()=>end(false);
 o.addEventListener('keydown',e=>{e.stopPropagation();if(e.key=='Escape')end(false)});document.body.appendChild(o);o.querySelector('.no').focus()})}
{const st=document.createElement('style');st.textContent=`.blzlink{text-decoration:underline;cursor:pointer;word-break:break-all;color:var(--ac,#2563eb)}.blzlinkq{text-decoration-style:dotted}.blzlinkq::after{content:' ↗?';font-size:.85em}
.blzbxask,.blzlask{max-width:640px}.blzbxl{max-height:260px;overflow:auto;padding:4px}.blzbxp{word-break:break-all;font-family:monospace}.blzlurl{font-family:monospace;word-break:break-all;padding:6px;max-height:160px;overflow:auto}.blzlv{word-break:break-all}.blzbxnote{padding:4px 8px}`;
 (document.head||document.documentElement).appendChild(st)}

/* ---------- 5. Developer mode: what Blitzen proposes for the test computer, the repository's workflows and pull requests ----------
   Every command is shown whole in the desktop's own dialog before it is sent (long ones scroll; nothing cut), with what it does
   that is risky said in plain words; one command waits at a time (rudolfd says so too); none carrying a password, token or key is
   sent (a small check here and in rudolfd: docs/plan-1.6.1.md Part 26's detector replaces it when it lands). Each Allow is for that
   one thing: nothing is remembered, there is no "allow all". */
const BLZD_RISKS=[
 [/(^|[\s;&|(])(rm|rmdir|shred|unlink|truncate|wipefs|mkfs(\.\w+)?|fstrim|dd)\b|\bsed\s+(-\w*\s+)*-i|(^|[^0-9&>])>{1,2}\s*(?!\/dev\/null\b|&)[^\s|;&]|\btee\b|\bmv\s|\bcp\s+(-\w*\s+)*-\w*f|\bfind\b[^|;]*-delete|\bchmod\b|\bchown\b|\bsync\s*--/i,'deletes or overwrites files (or changes who may use them)'],
 [/\b(iptables|ip6tables|nft|ufw|firewall-cmd|nmcli|wpa_cli|wpa_supplicant|iw|iwconfig|ifconfig|ifup|ifdown|resolvectl|hostnamectl|tc)\b|\bip\s+(link|addr|address|route|rule|neigh)\b|\/etc\/(hosts|resolv\.conf|nftables|network|NetworkManager)|\bsysctl\b[^\n]*\bnet\./i,'changes the firewall or the network'],
 [/\b(apt|apt-get|aptitude|dpkg|rpk|flatpak|snap|pacman|dnf|yum|zypper|apk)\b\s+(-\S+\s+)*(install|remove|purge|erase|upgrade|dist-upgrade|full-upgrade|-i|-r|-P|add|del|uninstall)\b|\bpip3?\s+(install|uninstall)\b|\bnpm\s+(i|install|uninstall|rm)\b[^\n]*(-g|--global)|\bmake\s+install\b|\bcargo\s+install\b/i,'installs or removes software'],
 [/\b(curl|wget|fetch|aria2c)\b[^\n|;]*\|\s*(sudo\s+)?(ba|z|da|k)?sh\b|\b(bash|sh|zsh)\s+<\s*\(\s*(curl|wget)|\b(curl|wget)\b[^\n]*(-o|-O|--output)\b[^\n]*(&&|;)[^\n]*(chmod\s+\+x|\.\/|\bsh\b|\bbash\b|python)|\bpython3?\s+-c\b[^\n]*(urllib|requests|http)|\beval\s+"?\$\((curl|wget)/i,'downloads something and runs it'],
 [/\b(efibootmgr|mokutil|sbctl|sbsign|sbverify|bootctl|grub-install|grub-mkconfig|update-grub|kernelstub|dracut|mkinitcpio|update-initramfs)\b|\/sys\/firmware\/efi|\befivars\b|\/boot\/|\/efi\/|\bdb\.(auth|esl)\b|\bPK\.(auth|esl)\b|\bKEK\b/i,'changes Secure Boot or the computer’s boot entries'],
 [/\b(scp|sftp|rsync|nc|ncat|netcat|socat|ssh|ftp|tftp|telnet|mail|sendmail|mutt|smbclient)\b|\b(curl|wget)\b[^\n]*(\s-d\b|\s--data|\s-F\b|\s--form|\s-T\b|\s--upload-file|\s-X\s*(POST|PUT|PATCH)|--post-(data|file)|\s--method=(POST|PUT))|\/dev\/tcp\/|\bgit\s+push\b/i,'sends data off the computer'],
 [/\b(reboot|shutdown|poweroff|halt|systemctl\s+(reboot|poweroff|halt|kexec|suspend|hibernate|stop|disable|mask|restart|kill)|init\s+[06]|kexec)\b/i,'restarts or stops the computer or one of its services'],
 [/\b(kill|pkill|killall)\b/i,'stops programs'],
 [/(^|[\s;&|(])(sudo|su|doas|pkexec)\b/i,'runs as the administrator (root)'],
 [/\/etc\/(shadow|gshadow|sudoers)|\bid_(rsa|ed25519|ecdsa)\b|\.(pem|key|p12|pfx|kdbx)\b|\/keyrings?\/|\.gnupg|\.ssh\/|rudolf-sb|rpk-signing|signing\.pem/i,'reads or touches files that may hold passwords or keys']];
function blzdRisks(cmd){if(!cmd)return[];if(cmd.kind=='screenshot')return[];const t=String(cmd.kind=='get'?cmd.path:cmd.script||'');
 const r=BLZD_RISKS.filter(([re])=>re.test(t)).map(([,w])=>w);if(cmd.kind=='get')return['copies a file from the test computer to this one'].concat(r.filter(w=>/passwords or keys/.test(w)));
 if(cmd.kind!='sh')r.push('is a kind of command Blitzen does not usually send ('+String(cmd.kind).slice(0,40)+')');return r}
/* what looks like a password, a token or a key in a text (the same rules as rudolfd-dev.inc dev_secretish) */
function blzdSecret(t){t=String(t||'');
 if(/-----BEGIN [A-Z ]*(PRIVATE KEY|CERTIFICATE|OPENSSH)/i.test(t))return 'a private key or certificate';
 if(/\b(ghp|gho|ghs|ghu|ghr)_[A-Za-z0-9]{20,}|\bgithub_pat_[A-Za-z0-9_]{20,}|\bglpat-[A-Za-z0-9_-]{20,}|\bxox[abprs]-[A-Za-z0-9-]{10,}|\bsk-[A-Za-z0-9_-]{20,}|\bAKIA[0-9A-Z]{16}\b/.test(t))return 'a token';
 if(/(pass(word|wd|phrase)?|pwd|secret|token|api[_-]?key|private[_-]?key|client[_-]?secret)\s*[=:]\s*['"]?[^\s'"]{4,}|--(password|passphrase|token|secret)[=\s]+['"]?[^\s'"-][^\s'"]{3,}|\bauthorization\s*:\s*\S+|\bbearer\s+[A-Za-z0-9._~+\/-]{12,}/i.test(t))return 'a password, token or key';
 const m=t.match(/[A-Za-z0-9+\/=_-]{32,}/g)||[];if(m.some(x=>/[a-z]/.test(x)&&/[A-Z]/.test(x)&&/[0-9]/.test(x)))return 'what looks like a key or a token (a long run of letters and digits)';
 return null}
/* the desktop's Allow, with what is risky said and anything carrying a secret refused (replaces blzdev60.js's) */
blzdAllow=function(title,lines,detail,o={}){return new Promise(res=>{const d=document.createElement('div');d.className='rmodal';const esc=s=>blzdEsc(s);const risks=o.risks||[];
 d.innerHTML=`<div class='rbox raise blzdallow' role='dialog' aria-label='${esc(title)}'><b>⚡ ${esc(title)}</b>
  <table class='tmt' style='margin:8px 0'><tbody>${lines.map(([k,v])=>`<tr><td>${esc(k)}</td><td><b>${esc(v)}</b></td></tr>`).join('')}</tbody></table>
  ${o.refuse?`<div class='msg blzdno'><b>Not sent: it carries ${esc(o.refuse)}.</b> Blitzen never sends passwords, tokens or keys; take it out (keep it in the keyring) and try again.</div>`:''}
  ${risks.length?`<div class='blzdrisk'><b>⚠ Read it first: it</b><ul>${risks.map(r=>`<li>${esc(r)}</li>`).join('')}</ul></div>`:''}
  ${detail!=null?`<div class='msg'>${esc(o.detailTitle||'Exactly what will be sent:')} (${String(detail).split('\n').length} line${String(detail).split('\n').length==1?'':'s'}, all of it below)</div><pre class='blzdpre blzdfull sunk' tabindex='0'></pre>`:''}
  <div class='msg'>${o.refuse?'Nothing is sent.':'Nothing is done until you choose Allow. This allows this one thing only, this once.'}</div>
  <div class='rmrow'>${o.refuse?'':"<button class='ok'>Allow</button>"}<button class='no pri'>${o.refuse?'Close':'Refuse'}</button></div></div>`;
 if(detail!=null)d.querySelector('pre').textContent=String(detail);
 const end=v=>{d.remove();res(v)};const ok=d.querySelector('.ok');if(ok)ok.onclick=()=>end(true);d.querySelector('.no').onclick=()=>end(false);
 d.addEventListener('keydown',e=>{e.stopPropagation();if(e.key=='Escape')end(false)});document.body.appendChild(d);d.querySelector('.no').focus()})};
const BLZD_ONE={busy:null};
/* a command through the development link (replaces blzdev60.js's): one waiting at a time, refused when it carries a secret */
blzdSend=async function(cmd,S,out){S=S||await blzdSettings(true);if(!S||!S.devlink||!S.devlinkToken){out&&(out.textContent='No development link is set (Settings › Apps › Blitzen › Developer).');return null}
 if(BLZD_ONE.busy){notify('One command at a time',`${BLZD_ONE.busy} is still waiting for the test computer: Blitzen sends the next one after its result.`);return null}
 const lines=[['Development link',S.devlink],['Command',cmd.kind=='sh'?'Run a command (sh)':cmd.kind=='get'?'Get a file':cmd.kind=='screenshot'?'Take a screenshot':String(cmd.kind)]];if(cmd.path)lines.push(['File',cmd.path]);
 const text=cmd.kind=='sh'?String(cmd.script||''):cmd.kind=='get'?String(cmd.path||''):JSON.stringify(cmd,null,1),sec=blzdSecret(JSON.stringify(cmd));
 if(sec){await blzdAllow('Send a command to the test computer',lines,text,{refuse:sec});out&&(out.textContent='Not sent: it carries '+sec+'.');return null}
 if(!await blzdAllow('Send a command to the test computer',lines,text,{risks:blzdRisks(cmd)})){out&&(out.textContent='Not sent.');return null}
 BLZD_ONE.busy='a command';
 try{let id;try{id=(await blzdApi('dl-send',JSON.stringify(cmd))).data.id}catch(e){out&&(out.textContent=e.message);return null}BLZD_ONE.busy=id;
  const t0=Date.now();for(;;){if(out)out.textContent=`Sent as ${id}. Waiting for the test computer… (${Math.round((Date.now()-t0)/1000)} s; with Ask first on, someone allows it there)`;
   let j;try{j=(await blzdApi('dl-wait?id='+id+'&secs=25')).data}catch(e){out&&(out.textContent=e.message);return null}
   if(j&&j.result)return{id,r:j.result};if(Date.now()-t0>30*60000){out&&(out.textContent=`No result for ${id} in 30 minutes.`);return null}}}
 finally{BLZD_ONE.busy=null}};
/* a workflow started: its file shown; one that may publish or use the repository's secrets said so */
function blzdWfRisks(t){t=String(t||'');const r=[];
 if(/gh\s+release|action-gh-release|upload-release-asset|releases|gh-pages|pages|deploy|publish|npm\s+publish|docker\s+push|twine\s+upload/i.test(t))r.push('may publish something (a release, a page, a package)');
 if(/\$\{\{\s*secrets\./.test(t))r.push('uses the repository’s secrets');if(/permissions:\s*[^\n]*write|contents:\s*write/i.test(t))r.push('may change the repository (it asks for write access)');return r}
/* a pull request: the whole change shown; a big removal said; a secret in what it adds refused */
function blzdChangeCheck(path,a,b){const A=String(a).split('\n'),B=String(b).split('\n'),add=B.filter(x=>!A.includes(x)),gone=A.filter(x=>!B.includes(x)).length,r=[];
 if(gone>20)r.push(`removes ${gone} lines`);if(/^\.github\//.test(path))r.push('changes how the repository builds (its workflows)');
 if(/(^|\/)(keys?|secrets?)\/|\.(pem|key)$/i.test(path))r.push('is a file for keys');return{risks:r,refuse:blzdSecret(add.join('\n'))}}
{const st=document.createElement('style');st.textContent=`.blzdrisk{border:1px solid #c2410c;background:#c2410c18;padding:4px 8px;margin:6px 0}.blzdrisk ul{margin:4px 0 0 18px;padding:0}.blzdno{border:1px solid #b91c1c;padding:4px 8px}.blzdallow .blzdfull{max-height:340px;overflow:auto;white-space:pre}`;(document.head||document.documentElement).appendChild(st)}
