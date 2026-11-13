/* ===== Rudolf OS 1.5.9 (docs/plan-1.5.9.md 8.2): Library web apps in a sandbox, with a small message API =====
   Before any app from an outside developer can be in the Rudolf OS Library, a web-app package must not have the
   desktop's access. Its window's frame has no origin of its own (sandbox without allow-same-origin; rudolfd also sends
   its pages with a sandbox policy), so it cannot read the desktop's page or storage and has no rudolfd token. What it
   may do beyond running its own page comes only through this API, and only what its PKGINFO asks for
   (permissions = saves gamepad fullscreen notifications) and the person allowed when it was first opened:
     saves          its own folder of saves (Saved Games\NAME in the account's files), get / set / list / delete
     gamepad        game controllers (the frame's permissions policy)
     fullscreen     full screen (the frame's permissions policy)
     notifications  a notification now and then (one every 5 s at most)
   The network is not limited (games load their own files). Messages: {rudolf:'v1', id, call, args} to the window's
   parent; answers {rudolf:'v1', id, ok, result|error}. www/lib/rudolf-gamekit.js wraps them for a game. */
const PKGBOX_PERMS={saves:'Keep its saves (in its own folder in your files)',gamepad:'Use game controllers',fullscreen:'Go full screen',notifications:'Show notifications'};
const PKGBOX=new Map();   /* the frame's window → {name,title,granted,lastNote} */
const pkgBoxAsked=p=>String(p.permissions||'').split(/[\s,]+/).filter(x=>x in PKGBOX_PERMS);
const pkgBoxKey=name=>'pkgbox:'+(USER?uname():'')+':'+name;
/* what the person allowed for this app: asked when it is first opened (per account), and again when it asks for
   more; kept as {g: allowed, done: asked} */
async function pkgBoxGrants(p){const want=pkgBoxAsked(p);if(!want.length)return [];
 let s=null;try{s=JSON.parse(localStorage.getItem(pkgBoxKey(p.name))||'null')}catch(e){}
 if(s&&Array.isArray(s.g)&&Array.isArray(s.done)&&want.every(x=>s.done.includes(x)))return s.g.filter(x=>want.includes(x));
 const ok=await new Promise(res=>{const o=document.createElement('div');o.className='rmodal';
  o.innerHTML=`<div class='rmbox' role='alertdialog' aria-label='App permissions' style='max-width:460px'><h3></h3><p class='msg'>This app from the Rudolf OS Library runs apart from the rest of Rudolf OS. It asks to:</p><div class='pkbp'></div><p class='msg'>Whatever you choose, it cannot read your other files, your accounts or Rudolf OS's settings.</p><div class='rmrow'><button class='n'>Cancel</button><button class='y pri'>Open</button></div></div>`;
  o.querySelector('h3').textContent=(p.icon||'📦')+' '+(p.title||p.name);
  const L=o.querySelector('.pkbp');want.forEach(x=>{const l=document.createElement('label');l.className='ck';l.style.display='block';l.innerHTML=`<input type='checkbox' checked value='${x}'> `;l.append(PKGBOX_PERMS[x]);L.appendChild(l)});
  document.body.appendChild(o);o.querySelector('.y').focus();
  o.querySelector('.n').onclick=()=>{o.remove();res(null)};o.querySelector('.y').onclick=()=>{const v=[...L.querySelectorAll('input')].filter(i=>i.checked).map(i=>i.value);o.remove();res(v)}});
 if(!ok)return null;try{localStorage.setItem(pkgBoxKey(p.name),JSON.stringify({g:ok,done:want}))}catch(e){}
 return ok}
/* the app's frame, with its permissions policy (only what was allowed) */
function pkgBoxFrame(p,granted,src){const f=document.createElement('iframe');
 f.setAttribute('sandbox','allow-scripts allow-forms allow-modals allow-downloads allow-pointer-lock');
 const allow=[];if(granted.includes('fullscreen'))allow.push('fullscreen');if(granted.includes('gamepad'))allow.push('gamepad');
 f.setAttribute('allow',allow.length?allow.join('; '):"fullscreen 'none'; gamepad 'none'");f.setAttribute('referrerpolicy','no-referrer');
 f.style.cssText='border:0;flex:1;width:100%;height:100%';f.src=src;return f}
/* saves: Saved Games\NAME in the account's files; keys of letters, digits, space, . _ -; text up to 1 MB each, 20 MB in all */
const pkgBoxSaveDir=name=>[...homePath('SAVED GAMES'),String(name).toUpperCase().replace(/[^A-Z0-9._-]/g,'_')];
const pkgBoxKeyOk=k=>typeof k=='string'&&/^[A-Za-z0-9 ._-]{1,60}$/.test(k)&&k!='.'&&k!='..';
function pkgBoxMkdir(path){let n=fs;for(const x of path){if(!isDir(n))return null;if(!(x in n))n[x]={};n=n[x]}return isDir(n)?n:null}
async function pkgBoxCall(b,call,a){
 a=a&&typeof a=='object'?a:{};
 if(call=='hello')return {name:b.name,title:b.title,granted:b.granted,signedIn:!!USER};
 if(call.startsWith('save.')){if(!b.granted.includes('saves'))throw new Error('saves were not allowed for this app');if(!USER)throw new Error('sign in to keep saves');
  const d=pkgBoxMkdir(pkgBoxSaveDir(b.name));if(!d)throw new Error('the saves folder cannot be made');
  if(call=='save.list')return Object.keys(d).filter(k=>!isDir(d[k])).sort();
  if(!pkgBoxKeyOk(a.key))throw new Error('a save name has letters, digits, spaces, dots, dashes and underscores (60 at most)');
  if(call=='save.get')return typeof d[a.key]=='string'?d[a.key]:null;
  if(call=='save.delete'){if(typeof d[a.key]=='string'){delete d[a.key];fsSave()}return true}
  if(call=='save.set'){if(typeof a.value!='string')throw new Error('a save is text (JSON.stringify it)');if(a.value.length>1048576)throw new Error('a save is 1 MB at most');
   const tot=Object.entries(d).filter(([k,v])=>k!=a.key&&typeof v=='string').reduce((s,[,v])=>s+v.length,0);if(tot+a.value.length>20*1048576)throw new Error("this app's saves are 20 MB at most");
   d[a.key]=a.value;fsSave();return true}}
 if(call=='notify'){if(!b.granted.includes('notifications'))throw new Error('notifications were not allowed for this app');
  if(Date.now()-b.lastNote<5000)throw new Error('one notification every 5 seconds at most');b.lastNote=Date.now();
  notify(String(b.title).slice(0,40),String(a.text||a.title||'').slice(0,200),{quiet:true});return true}
 throw new Error('unknown call: '+String(call).slice(0,40))}
window.addEventListener('message',async e=>{const b=PKGBOX.get(e.source);if(!b)return;   /* only a Library app's own frame */
 const m=e.data;if(!m||typeof m!='object'||m.rudolf!='v1'||typeof m.call!='string')return;
 let r;try{r={rudolf:'v1',id:m.id,ok:true,result:await pkgBoxCall(b,m.call,m.args)}}catch(x){r={rudolf:'v1',id:m.id,ok:false,error:String(x&&x.message||x)}}
 try{e.source.postMessage(r,'*')}catch(x){}});   /* ('*': the frame has no origin to name) */
/* the web-app window: the frame above instead of 1.5.2's plain one */
{const _o=pkgOpen;pkgOpen=async function(name){const s=await pkgStatus();const p=s&&s.installed.find(x=>x.name==name);
 if(!p||p.kind!='webapp')return _o.apply(this,arguments);
 const entry=(p.entry||'www/index.html').replace(/^\/+/,'');if(!/^www\//.test(entry)||entry.includes('..'))return notify(p.title,'This package has no page to open.',{kind:'error'});
 const granted=await pkgBoxGrants(p);if(granted===null)return;
 const W=win(p.title||name,p.icon||'📦',760,560);W.body.style.cssText='padding:0;display:flex;background:#fff';
 const f=pkgBoxFrame(p,granted,'/pkg/'+encodeURIComponent(name)+'/'+entry.split('/').map(encodeURIComponent).join('/')+(document.documentElement.classList.contains('dark')?'?theme=dark':''));
 W.body.appendChild(f);const b={name,title:p.title||name,granted,lastNote:0};
 const reg=()=>{if(f.contentWindow)PKGBOX.set(f.contentWindow,b)};reg();f.addEventListener('load',reg);
 const oc=W.onclose;W.onclose=function(){try{PKGBOX.delete(f.contentWindow)}catch(e){}return oc&&oc.apply(this,arguments)};
 return W}}
