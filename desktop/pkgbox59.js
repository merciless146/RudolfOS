/* ===== Rudolf OS 1.5.9 (docs/plan-1.5.9.md 8.2): Library web apps in a sandbox =====
   Before any app from an outside developer can be in the Rudolf OS Library, a web-app package must not have the
   desktop's access. Its window's frame has no origin of its own (sandbox without allow-same-origin; rudolfd also sends
   its pages with a sandbox policy, so a page opened some other way is sandboxed too): it cannot read the desktop's page
   or storage and has no rudolfd token. It talks to the desktop only through the game kit's messages (games58.js,
   {rudolfGame: 1, …}: hello, load, save; pause, resume and settings from the desktop), which every Library game uses
   (rudolfos-library lib/gamekit).
   The project's own Library apps (signed by the Library) get those messages, full screen and game controllers, as in
   1.5.8. An outside developer's app (signed through a developer certificate) gets only what its PKGINFO asks for
   (permissions = saves gamepad fullscreen) and the person allowed when it was first opened (per account):
     saves       its saves kept by the desktop (the game kit's load and save)
     gamepad     game controllers (the frame's permissions policy)
     fullscreen  full screen (the frame's permissions policy)
   (1.5.9's first version had a second message API and a second game kit; the game kit's own messages are the one API.) */
const PKGBOX_PERMS={saves:'Keep its saves',gamepad:'Use game controllers',fullscreen:'Go full screen'};
const pkgBoxAsked=p=>String(p.permissions||'').split(/[\s,]+/).filter(x=>x in PKGBOX_PERMS);
const pkgBoxKey=name=>'pkgbox:'+(USER?uname():'')+':'+name;
/* what the person allowed for an outside developer's app: asked when it is first opened (per account), and again when
   it asks for more; kept as {g: allowed, done: asked}. The project's own apps: everything, without asking */
async function pkgBoxGrants(p){if(p.signed!='developer')return Object.keys(PKGBOX_PERMS);
 const want=pkgBoxAsked(p);if(!want.length)return [];
 let s=null;try{s=JSON.parse(localStorage.getItem(pkgBoxKey(p.name))||'null')}catch(e){}
 if(s&&Array.isArray(s.g)&&Array.isArray(s.done)&&want.every(x=>s.done.includes(x)))return s.g.filter(x=>want.includes(x));
 const ok=await new Promise(res=>{const o=document.createElement('div');o.className='rmodal';
  o.innerHTML=`<div class='rmbox' role='alertdialog' aria-label='App permissions' style='max-width:460px'><h3></h3><p class='msg pkbw'></p><div class='pkbp'></div><p class='msg'>Whatever you choose, it cannot read your files, your accounts or Rudolf OS's settings.</p><div class='rmrow'><button class='n'>Cancel</button><button class='y pri'>Open</button></div></div>`;
  o.querySelector('h3').textContent=(p.icon||'📦')+' '+(p.title||p.name);
  o.querySelector('.pkbw').textContent=`Made by ${p.developer||'a developer'}${p.github?' (github.com/'+p.github+')':''}, checked through the Rudolf OS Library. It runs apart from the rest of Rudolf OS and asks to:`;
  const L=o.querySelector('.pkbp');want.forEach(x=>{const l=document.createElement('label');l.className='ck';l.style.display='block';l.innerHTML=`<input type='checkbox' checked value='${x}'> `;l.append(PKGBOX_PERMS[x]);L.appendChild(l)});
  document.body.appendChild(o);o.querySelector('.y').focus();
  o.querySelector('.n').onclick=()=>{o.remove();res(null)};o.querySelector('.y').onclick=()=>{const v=[...L.querySelectorAll('input')].filter(i=>i.checked).map(i=>i.value);o.remove();res(v)}});
 if(!ok)return null;try{localStorage.setItem(pkgBoxKey(p.name),JSON.stringify({g:ok,done:want}))}catch(e){}
 return ok}
/* the app's frame, with its permissions policy (only what was allowed); the grants are on the frame for games58's
   messages (data-grants, data-dev) */
function pkgBoxFrame(p,granted,src){const f=document.createElement('iframe');
 f.setAttribute('sandbox','allow-scripts allow-forms allow-modals allow-downloads allow-pointer-lock');
 const allow=[];if(granted.includes('fullscreen'))allow.push('fullscreen');if(granted.includes('gamepad'))allow.push('gamepad');
 f.setAttribute('allow',allow.length?allow.join('; '):"fullscreen 'none'; gamepad 'none'");f.setAttribute('referrerpolicy','no-referrer');
 f.dataset.grants=granted.join(' ');if(p.signed=='developer')f.dataset.dev='1';
 f.style.cssText='border:0;flex:1;width:100%;height:100%';f.src=src;return f}
/* the web-app window: the frame above instead of 1.5.2's plain one */
{const _o=pkgOpen;pkgOpen=async function(name){const s=await pkgStatus();const p=s&&s.installed.find(x=>x.name==name);
 if(p&&p.disabled)return notify(p.title||name,'This app is turned off: '+p.disabled+'. Remove it in App Store › Rudolf OS Library.',{kind:'error'});   /* (revoked: plan 8.2) */
 if(!p||p.kind!='webapp')return _o.apply(this,arguments);
 const entry=(p.entry||'www/index.html').replace(/^\/+/,'');if(!/^www\//.test(entry)||entry.includes('..'))return notify(p.title,'This package has no page to open.',{kind:'error'});
 const granted=await pkgBoxGrants(p);if(granted===null)return;
 const W=win(p.title||name,p.icon||'📦',760,560);W.body.style.cssText='padding:0;display:flex;background:#fff';
 W.body.appendChild(pkgBoxFrame(p,granted,'/pkg/'+encodeURIComponent(name)+'/'+entry.split('/').map(encodeURIComponent).join('/')+(document.documentElement.classList.contains('dark')?'?theme=dark':'')));
 return W}}
