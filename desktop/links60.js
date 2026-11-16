/* ===== 1.6.0: links of Linux apps' own schemes (claude://, discord://, steam://, vscode://...) =====
   An app signing in through a web page (Claude's desktop app: "Continue with Google"; Discord, Steam, VS Code, Slack,
   Spotify) opens the page in Oracle; after the sign-in the site sends Oracle to the app's own scheme with the answer
   (claude://...?code=...). A browser hands such a link to the app that registered it, after asking. Oracle does the
   same: the Linux app whose launcher says it opens the scheme (MimeType x-scheme-handler/SCHEME: a Debian program's
   .desktop file, a Flatpak's exported one) is started with the link as its one argument, as Standard starts Linux apps
   (its single-instance handling passes it to the window already open), after "Open Claude?". The same for a link of
   such a scheme that a Linux app asks the desktop to open (the portal: one app's link to another). Settings › Apps ›
   Links says which app opens each scheme and lets the person choose another, or none. A scheme no app registered stays
   refused with Oracle's message; the web's and the browser's own schemes are never handed over (rudolfd checks too). */
const LK_NO=new Set(['http','https','file','ftp','ftps','sftp','javascript','vbscript','data','blob','about','chrome','chrome-extension','chrome-untrusted','isolated-app','devtools','view-source','filesystem','ws','wss','mailto','tel','sms','intent','ssh','smb','nfs','rudolf']);
const LKK=()=>'bd95links:'+uid();
const lkOk=s=>/^[a-z][a-z0-9+.-]{1,39}$/.test(s)&&!LK_NO.has(s);
function lkScheme(u){const m=/^([a-z][a-z0-9+.-]{1,39}):/i.exec(String(u||''));return m?m[1].toLowerCase():''}
/* a link that may be handed to an app: printable, no spaces, up to 8000 characters, a scheme an app may have */
function lkLinkOk(u){u=String(u||'');return u.length>=4&&u.length<=8000&&/^[\x21-\x7e]+$/.test(u)&&lkOk(lkScheme(u))}
/* the schemes the Linux apps register, each with its apps (Start menu order) */
function lkSchemes(){const m=new Map();lxLaunchers().slice().sort((a,b)=>String(a.name).localeCompare(String(b.name))).forEach(x=>String(x.mime||'').split(';').forEach(t=>{t=t.trim().toLowerCase();
 if(!t.startsWith('x-scheme-handler/'))return;const s=t.slice(17);if(!lkOk(s))return;if(!m.has(s))m.set(s,[]);if(!m.get(s).some(y=>y.desktop==x.desktop))m.get(s).push(x)}));return m}
/* which app opens a scheme's links: the one chosen in Settings › Apps, else the first; null: none (or "Don't open them") */
function lkApp(s){const l=lkSchemes().get(s)||[];const c=ld(LKK(),{})[s];if(c==='none'||!l.length)return null;return l.find(x=>x.desktop==c)||l[0]}
async function lkFind(u){if(!lkLinkOk(u))return null;const s=lkScheme(u);let a=lkApp(s);if(a||ld(LKK(),{})[s]==='none')return a;
 /* (an app installed since this page was loaded, or a page that has not asked yet: Native's Oracle window) */
 try{await lxInstalled(true)}catch(e){}try{if(typeof debApps=='function'){DEB.appsAt=0;await debApps()}}catch(e){}return lkApp(s)}
/* "Open Claude?": over the Oracle window the link came from, else over the desktop */
function lkAsk(W,a,from,u){return new Promise(res=>{const o=document.createElement('div');let done=false;
 const end=v=>{if(done)return;done=true;o.remove();res(v)};
 const box=document.createElement('div');box.setAttribute('role','alertdialog');
 if(W&&W.body&&W.el&&W.el.isConnected){o.className='dlg sdlg';box.style.maxWidth='420px'}else{o.className='rmodal';box.className='rmbox'}
 const h=document.createElement('h3');h.style.cssText='margin:0 0 8px;font-size:16px';h.textContent='Open '+a.name+'?';
 const p=document.createElement('div');p.className='sdm';p.style.cssText='white-space:pre-wrap;word-break:break-word';
 p.textContent=(from?from+' wants':'A link wants')+' to open '+a.name+', '+(a.deb?'a program in Linux (Debian)':'a Linux app')+' on this computer, with this link.';
 const l=document.createElement('div');l.style.cssText='margin-top:8px;font-size:12px;color:var(--mut);word-break:break-all;max-height:4.5em;overflow:hidden';l.textContent=u.length>160?u.slice(0,160)+'…':u;
 const r=document.createElement('div');r.style.cssText='text-align:right;margin-top:14px;display:flex;gap:6px;justify-content:flex-end;flex-wrap:wrap';
 const c=document.createElement('button');c.className='lkc';c.textContent='Cancel';c.onclick=()=>end(false);
 const y=document.createElement('button');y.className='lko pri';y.style.cssText='background:var(--ac);color:#fff';y.textContent='Open '+a.name;y.onclick=()=>end(true);
 r.append(c,y);box.append(h,p,l,r);o.appendChild(box);o.addEventListener('keydown',e=>{e.stopPropagation();if(e.key=='Escape')end(false)});
 if(o.className=='rmodal')document.body.appendChild(o);else{W.body.style.position='relative';W.body.appendChild(o)}
 y.focus()})}
/* the app's window in front once it has the link (its own window, when it had one open: single instance) */
async function lkFront(a){for(let i=0;i<10;i++){await new Promise(r=>setTimeout(r,1000));let j;try{j=await lxApi('apps/windows')}catch(e){continue}
 const w=(j.windows||[]).find(w=>!w.web&&lxAppFor(w)===a)||(j.windows||[]).find(w=>{if(w.web)return false;const x=lxAppFor(w);return x&&x.app==a.app});
 if(w){try{await lxApi('apps/show?win='+w.win)}catch(e){}try{lxWinPoll()}catch(e){}return}}}
/* hand a link to its app; o: {W (the window that asked), from (who wants it opened)}. false: no app opens it */
async function lkOpen(u,o){o=o||{};const a=await lkFind(u);if(!a)return false;
 if(!await lkAsk(o.W,a,o.from,u))return true;
 try{if(a.deb)await lxApi('debian/launch?id='+encodeURIComponent(a.deb)+'&link=1',u);
  else await lxApi('apps/run?id='+encodeURIComponent(a.app)+'&desktop='+encodeURIComponent(a.desktop)+'&link=1',u)}
 catch(e){notify(a.name,e.message,{kind:'error'});return true}
 try{typeof logEvt=='function'&&logEvt('sys',`${o.from||'A link'} opened ${a.name} (${lkScheme(u)}: link)`)}catch(e){}
 lkFront(a);return true}
/* from an Oracle tab (tabs26.js): its page went to a link no browser opens (ERR_UNKNOWN_URL_SCHEME). The tab stays on its
   page (the link is not a page); without an app the tab says so, as before */
function lkTab(t,m){const u=String(m.url||'');let from='';try{from=new URL(t.url?t.url():'').hostname}catch(e){}
 t.h.load(false,'');lkOpen(u,{W:t.W,from}).then(ok=>{if(!ok)t.h.load(false,String(m.error||'net::ERR_UNKNOWN_URL_SCHEME'))},()=>t.h.load(false,String(m.error||'')))}
/* ---------- Settings › Apps: Links (which app opens each scheme's links) ---------- */
function lkPanel(c){const da=c.querySelector('.da');if(!da||c.querySelector('.lkp'))return;const m=lkSchemes();
 const sec=document.createElement('div');sec.className='lkp';const h=document.createElement('h4');h.className='sh4';h.textContent='Links';
 const p=document.createElement('p');p.className='msg';p.textContent=m.size?'Links of an app’s own kind (after signing in on a website, for example) open in that app, after you say yes. Choose which app opens each kind.':'No Linux app on this computer opens links of its own kind.';
 sec.append(h,p);if(m.size){const tb=document.createElement('table');tb.className='tmt lkt';const bd=document.createElement('tbody');tb.appendChild(bd);
  const ch=ld(LKK(),{});[...m.keys()].sort().forEach(s=>{const r=bd.insertRow();r.insertCell().textContent=s+': links';const sl=document.createElement('select');sl.dataset.scheme=s;
   const cur=ch[s]==='none'?'none':(m.get(s).find(x=>x.desktop==ch[s])||m.get(s)[0]).desktop;
   m.get(s).forEach(x=>{const op=document.createElement('option');op.value=x.desktop;op.textContent=x.name+(x.deb?' (Debian)':'');op.selected=x.desktop==cur;sl.appendChild(op)});
   const no=document.createElement('option');no.value='none';no.textContent='Don’t open them';no.selected=cur=='none';sl.appendChild(no);
   sl.onchange=()=>{const v=ld(LKK(),{});v[s]=sl.value;svj(LKK(),v)};r.insertCell().appendChild(sl)});sec.appendChild(tb)}
 da.after(sec)}
{const _ap=appsPanel;appsPanel=function(c){const r=_ap.apply(this,arguments);try{lkPanel(c)}catch(e){console.warn('links',e)}return r}}
