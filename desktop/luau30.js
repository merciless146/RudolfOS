/* ===================== Luau and Pilot.lua (1.2.5) =====================
   Luau (Roblox's Lua, MIT) runs here as WebAssembly (lib/luau.wasm, built by iso/systools/mkluau.sh): each run in
   its own Worker, so a program that hangs is simply stopped. Luau's own type checker runs in a second Worker that
   stays loaded with its definitions: for Pilot.lua (Waste of Space microcontrollers) those are Pilot.lua's own
   (lib/pilot.d.luau, MIT) on top of Roblox-lite (lib/roblox-*.d.luau); Pilot.lua programs run with stand-in parts
   (lib/pilot-parts.luau) that print what they are asked to do. Used by Blitzen and Code Editor.
   1.2.7: one checker Worker per kind (luau, roblox, pilot), each loaded and given its definitions once and kept
   (ended after 10 idle minutes), and started ahead of time when Blitzen or Code Editor is about to need it, so the
   first check no longer waits for the load (up to two minutes on a slow computer). */
const LUAU={mod:null,files:{},checkers:{},defines:{},seq:0,idleMs:600000};
const LUAU_LIB=()=>(typeof RD!='undefined'&&RD?'/':'')+'lib/';
/* the Worker: a WASI shim with only what Luau's C library asks for, and the three calls */
const LUAU_WORKER=`let inst=null,kind=null,out='';
function wasi(mem){const u8=()=>new Uint8Array(mem().buffer),dv=()=>new DataView(mem().buffer),td=new TextDecoder();
 const W={fd_write(fd,iov,n,nw){let t=0;const d=dv();for(let i=0;i<n;i++){const p=d.getUint32(iov+8*i,true),l=d.getUint32(iov+8*i+4,true);out+=td.decode(u8().subarray(p,p+l));t+=l}d.setUint32(nw,t,true);if(out.length>200000)out=out.slice(0,200000)+'\\n(output cut at 200,000 characters)\\n';return 0},
  fd_close:()=>0,fd_seek:()=>70,fd_pread:()=>8,fd_fdstat_get(fd,p){const d=dv();d.setUint8(p,fd<3?2:0);d.setUint16(p+2,0,true);d.setBigUint64(p+8,0n,true);d.setBigUint64(p+16,0n,true);return fd<3?0:8},
  environ_sizes_get(c,s){const d=dv();d.setUint32(c,0,true);d.setUint32(s,0,true);return 0},environ_get:()=>0,
  clock_time_get(id,pr,p){dv().setBigUint64(p,BigInt(Math.round((id==0?Date.now():performance.now())*1e6)),true);return 0},
  fd_prestat_get:()=>8,fd_prestat_dir_name:()=>8,proc_exit(c){throw new Error('exit '+c)}};
 return {wasi_snapshot_preview1:new Proxy(W,{get:(o,k)=>o[k]||(()=>52)})}}
async function load(mod){let m;inst=await WebAssembly.instantiate(mod,wasi(()=>m));m=inst.exports.memory;inst.exports._initialize&&inst.exports._initialize()}
function str(s){const e=new TextEncoder().encode(s),p=inst.exports.rl_alloc(e.length+1),u=new Uint8Array(inst.exports.memory.buffer);u.set(e,p);u[p+e.length]=0;return [p,e.length]}
onmessage=async e=>{const m=e.data;try{
 if(m.op=='run'){await load(m.mod);out='';const [s,n]=str(m.src),[p,pn]=m.pre?str(m.pre):[0,0];const rc=inst.exports.rl_run(s,n,m.limit||10000,p,pn);postMessage({id:m.id,rc,out})}
 else if(m.op=='check'){if(!inst||kind!=m.kind){await load(m.mod);kind=m.kind;out='';
   for(const d of m.defs||[]){const [s,n]=str(d.text),[nm]=str(d.name),[ns]=str(d.ns||'');if(inst.exports.rl_define(s,n,nm,ns))postMessage({id:m.id,defError:out});out=''}
   postMessage({id:m.id,defined:m.kind})}
  if(m.warm){postMessage({id:m.id,warm:true});return}
  out='';const [s,n]=str(m.src);const k=inst.exports.rl_check(s,n,m.strict?1:0);inst.exports.rl_free(s);postMessage({id:m.id,count:k,out})}
}catch(x){postMessage({id:m.id,crash:String(x&&x.message||x),out})}}`;
function luauWorker(){return new Worker(URL.createObjectURL(new Blob([LUAU_WORKER],{type:'text/javascript'})))}
async function luauFetch(name,bin){if(LUAU.files[name])return LUAU.files[name];const r=await fetch(LUAU_LIB()+name);if(!r.ok)throw new Error(name+': HTTP '+r.status);
 return LUAU.files[name]=bin?await r.arrayBuffer():await r.text()}
async function luauModule(){if(!LUAU.mod)LUAU.mod=luauFetch('luau.wasm',true).then(b=>WebAssembly.compile(b)).catch(e=>{LUAU.mod=null;throw e});return LUAU.mod}
/* which kind of Lua a program is: 'pilot' (Waste of Space), 'roblox' (a Roblox game script), 'luau' or 'lua' */
function luauDialect(code){const c=String(code||'');
 if(/^\s*--\s*pilot\b/i.test(c)||/\b(GetPart|GetParts|GetPartFromPort|GetPartsFromPort|GetPort|TriggerPort)\s*\(|\bMicrocontroller\b|\bpilot\.\w+/.test(c))return 'pilot';
 if(/\bgame:GetService\b|\bgame\.(Players|Workspace|ReplicatedStorage)\b|\bworkspace\b|\bscript\.Parent\b|\bInstance\.new\b/.test(c))return 'roblox';
 if(/^\s*--!(strict|nonstrict|nocheck)|\blocal\s+\w+\s*:\s*[A-Za-z{(]|\bfunction\b[^\n]*\(\s*\w+\s*:\s*[A-Za-z{(]|\bfunction\b[^\n]*\)\s*:\s*[A-Za-z{(]|^\s*(export\s+)?type\s+\w+[^\n]*=|\w\s*(\+|-|\*|\/|\.\.)=|\bcontinue\b|`[^`]*\{|=\s*if\b[^\n]*\bthen\b[^\n]*\belse\b|\btypeof\s*\(/m.test(c))return 'luau';
 return 'lua'}
/* run: {ok, code (0 ok, 1 error, 124 too slow), timeout, out} */
async function luauRun(code,o={}){let mod;try{mod=await luauModule()}catch(e){return{ok:false,error:'The Luau runtime could not be loaded ('+e.message+').'}}
 const pre=o.pilot?await luauFetch('pilot-parts.luau').catch(()=>null):null;if(o.pilot&&!pre)return{ok:false,error:'Pilot.lua’s stand-in parts could not be loaded.'};
 const limit=o.limit||10000;
 return new Promise(res=>{const w=luauWorker(),id=++LUAU.seq;let done=false;const fin=r=>{if(done)return;done=true;clearTimeout(tm);w.terminate();res(r)};
  /* the interrupt stops long loops at the limit; a stuck Worker is ended two seconds later */
  const tm=setTimeout(()=>fin({ok:true,code:124,timeout:true,out:'time limit: the program ran for more than '+Math.round(limit/1000)+' seconds\n'}),limit+2000);
  w.onmessage=e=>{const d=e.data;if(d.crash)return fin({ok:true,code:1,out:(d.out||'')+'Luau stopped: '+d.crash+'\n'});
   fin({ok:true,code:d.rc==0?0:d.rc==2?124:1,timeout:d.rc==2,out:d.out||''})};
  w.onerror=e=>fin({ok:false,error:'Luau could not start ('+(e.message||'error')+'). This browser may lack WebAssembly exceptions.'});
  w.postMessage({op:'run',id,mod,src:String(code),pre,limit})})}
/* type check: {ok, problems:[{line,col,msg}], out} — kinds: 'luau' (Luau's built-ins), 'roblox' or 'pilot' */
/* a Roblox game script is checked against Roblox-lite with game, workspace and script left open (any) */
const LUAU_ROBLOX_GAME=`declare game: any\ndeclare workspace: any\ndeclare script: any\ndeclare plugin: any\ndeclare shared: any\ndeclare Instance: { new: (className: string, parent: Instance?) -> any }\ndeclare TweenInfo: { new: (...any) -> any }\ndeclare BrickColor: { new: (...any) -> any, random: () -> any }`;
async function luauDefs(kind){if(kind!='pilot'&&kind!='roblox')return [];
 const [en,rb,pl]=await Promise.all(['roblox-enum.d.luau','roblox-lite.d.luau',kind=='pilot'?'pilot.d.luau':null].map(n=>n&&luauFetch(n)));
 return [{name:'@roblox/enum',ns:'Enum',text:en},{name:'@roblox',text:rb},kind=='pilot'?{name:'@pilot',text:pl}:{name:'@roblox/game',text:LUAU_ROBLOX_GAME}]}
/* Pilot.lua is checked strictly (that is what finds a wrong part method), but "could be nil" is left out:
   Pilot.lua programs take GetPart's part as there, and the game says so plainly if it is not */
/* the checker of a kind: {w, ready (resolves once loaded and defined), t0 (when the load started), pending, timer} */
function luauDropChecker(kind,c){if(LUAU.checkers[kind]!==c)return;delete LUAU.checkers[kind];clearTimeout(c.timer);try{c.w.terminate()}catch(e){}
 for(const f of c.pending.values())f({crash:'the checker was stopped'});c.pending.clear()}
function luauSend(c,msg,onMsg){const id=++LUAU.seq;c.pending.set(id,onMsg);c.w.postMessage(Object.assign({id},msg));return id}
function luauIdle(kind,c){clearTimeout(c.timer);c.timer=setTimeout(()=>{if(!c.pending.size)luauDropChecker(kind,c)},LUAU.idleMs)}
function luauChecker(kind){let c=LUAU.checkers[kind];if(c)return c;
 c={w:luauWorker(),pending:new Map(),t0:Date.now(),timer:0,defError:''};LUAU.checkers[kind]=c;
 c.w.onmessage=e=>{const d=e.data,f=c.pending.get(d.id);if(d.defined){LUAU.defines[d.defined]=(LUAU.defines[d.defined]||0)+1;return}
  if(d.defError!==undefined){c.defError+=d.defError;return}if(f){c.pending.delete(d.id);f(d)}};
 c.w.onerror=e=>{const m='The checker could not start ('+(e.message||'error')+').';for(const f of c.pending.values())f({crash:m,start:true});c.pending.clear();luauDropChecker(kind,c)};
 c.ready=(async()=>{const mod=await luauModule(),defs=await luauDefs(kind);c.mod=mod;c.defs=defs;
  const d=await new Promise(res=>luauSend(c,{op:'check',mod,kind,defs,src:'',warm:true},res));if(d.crash)throw new Error(d.crash);return c})();
 c.ready.catch(()=>luauDropChecker(kind,c));luauIdle(kind,c);return c}
/* start the checker of a kind ahead of time, when the page is idle (Blitzen, Code Editor); one at a time */
function luauWarm(kind){if(!['luau','roblox','pilot'].includes(kind)||LUAU.checkers[kind])return;
 const go=()=>{if(LUAU.checkers[kind])return;if(Object.values(LUAU.checkers).some(c=>!c.done)){setTimeout(()=>luauWarm(kind),2000);return}
  const c=luauChecker(kind);c.ready.then(()=>{c.done=true},()=>{})};
 (window.requestIdleCallback||(f=>setTimeout(f,200)))(go,{timeout:5000})}
/* which checker a program needs */
function luauKind(code,lang){const d=luauDialect(code);return lang=='pilot'||d=='pilot'?'pilot':d=='roblox'?'roblox':'luau'}
async function luauCheck(code,o={}){const kind=o.kind||'luau',strict=o.strict??(kind=='pilot');
 const c=luauChecker(kind);
 /* a checker still loading (a warm-up, or this check's own load) gets 120 s from the start of its load */
 const left=Math.max(5000,(o.loadLimit||120000)-(Date.now()-c.t0));let to;
 try{await Promise.race([c.ready,new Promise((_,j)=>{to=setTimeout(()=>j(new Error('timeout')),left)})])}
 catch(e){clearTimeout(to);luauDropChecker(kind,c);return{ok:false,error:e.message=='timeout'?'The checker took too long.':/could not start/.test(e.message)?e.message:'The Luau checker could not be loaded ('+e.message+').'}}
 clearTimeout(to);c.done=true;luauIdle(kind,c);
 return new Promise(res=>{let id;const tm=setTimeout(()=>{c.pending.delete(id);luauDropChecker(kind,c);res({ok:false,error:'The checker took too long.'})},o.limit||20000);
  id=luauSend(c,{op:'check',mod:c.mod,kind,defs:c.defs,src:String(code),strict},d=>{clearTimeout(tm);luauIdle(kind,c);
   if(d.crash){luauDropChecker(kind,c);return res({ok:false,error:d.start?d.crash:'The checker stopped: '+d.crash})}
   const problems=[];for(const l of (d.out||'').split('\n')){if(!l)continue;const m=/^(\d+):(\d+): (.*)$/.exec(l);
    if(m)problems.push({line:+m[1],col:+m[2],msg:m[3]});else if(problems.length)problems.at(-1).msg+='\n'+l;else problems.push({line:0,col:0,msg:l})}
   if(kind=='pilot'&&o.nilOk!==false)for(let i=problems.length-1;i>=0;i--)if(/could be nil/.test(problems[i].msg))problems.splice(i,1);
   res({ok:true,problems,defError:c.defError,out:problems.map(p=>(p.line?'line '+p.line+': ':'')+p.msg).join('\n')})})})}
/* Blitzen's Luau and Pilot.lua: check the types first (a type error is a failure the model can fix), then run;
   a Roblox game script is only checked, since it needs Roblox to run */
async function luauBlitzen(code,lang){let notChecked='';const kind=luauKind(code,lang);
 if(!/^\s*--!nocheck/m.test(code)){const ck=await luauCheck(code,{kind});
  if(ck.ok&&ck.problems.length)return{ok:true,code:1,types:true,out:'Type check ('+({pilot:'Pilot.lua',roblox:'Roblox',luau:'Luau'})[kind]+'):\n'+ck.out+'\n'};
  if(!ck.ok&&kind!='roblox')notChecked='(not type-checked: '+ck.error+')\n';
  if(kind=='roblox')return ck.ok?{ok:true,code:0,checkedOnly:true,out:'The types check out. It is a Roblox game script (game, workspace, script…), so it runs only in Roblox.\n'}:{ok:false,error:ck.error}}
 else if(kind=='roblox')return{ok:false,error:'A Roblox game script runs only in Roblox.'};
 const r=await luauRun(code,{pilot:kind=='pilot'});if(r.ok)r.out=notChecked+(kind=='pilot'?'(simulated: Waste of Space is not running; the parts below are stand-ins)\n':'')+r.out;return r}
