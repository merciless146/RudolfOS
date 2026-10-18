/* ===== Browser window for sign-in sites (Rudolf OS) =====
   Oracle shows websites inside the desktop page. Google, Microsoft, Yahoo and similar sign-ins refuse that
   (their cookies are not sent to an embedded page), so on Rudolf OS these sites open in a real Chromium
   browser window (rudolfd web/*). It covers the screen except the taskbar: the 🌐 taskbar button switches
   between the window and the desktop, and Super+Esc returns to the desktop. Pages that try to open a new
   window (window.open) go there too, instead of taking over the desktop. */
const WEB_SITES=['accounts.google.com','mail.google.com','docs.google.com','drive.google.com','calendar.google.com','meet.google.com','photos.google.com','myaccount.google.com',
 'outlook.live.com','outlook.office.com','outlook.office365.com','login.live.com','login.microsoftonline.com','account.microsoft.com','onedrive.live.com','teams.microsoft.com',
 'mail.yahoo.com','compose.mail.yahoo.com','login.yahoo.com','paypal.com','web.whatsapp.com','discord.com','app.slack.com','github.com','icloud.com','proton.me'];
function webHost(u){try{return new URL(u).hostname.toLowerCase()}catch(e){return''}}
function webWants(u){if(!RD)return false;const h=webHost(u);return!!h&&WEB_SITES.concat(S.webSites||[]).some(d=>h==d||h.endsWith('.'+d))}
let WEB_ON=false,WEB_SHOWN=false,WEB_TOLD=false,WEB_POLL=null;
const webNativeOpen=window.open.bind(window);
async function webOpen(u){
 if(!RD){try{webNativeOpen(u,'_blank','noopener')}catch(e){}return}
 /* Rudolf OS 1.2 inside the shell: Oracle's real tabs keep sign-ins, so the browser window is not needed */
 if(typeof rtOn=='function'&&rtOn()){browser(u);return}
 if(window.__sys&&typeof fwCheck=='function'){const g=fwCheck(u);if(g){notify('Blocked by the firewall',g.why,{kind:'error'});return}}
 let href;try{href=new URL(u).href}catch(e){return notify('Cannot open',String(u),{kind:'error'})}
 href=href.replace(/'/g,'%27').replace(/`/g,'%60').replace(/\$/g,'%24').replace(/\\/g,'%5C').replace(/"/g,'%22');
 try{await rdPost('web/open?url='+encodeURIComponent(href))}catch(e){return notify('The browser window could not open',e.message,{kind:'error'})}
 WEB_ON=WEB_SHOWN=true;webTray();webWatch();
 if(!WEB_TOLD){WEB_TOLD=true;notify('Opened in a browser window',webHost(href)+' needs a real browser window to sign in. Tap 🌐 in the taskbar (or press Super+Esc) to switch back to the desktop.')}}
function webTray(){if(!WEB_ON){trayRemove('web');return}
 const b=trayAdd('web',{icon:'🌐',title:WEB_SHOWN?'Back to the desktop':'Show the browser window',onclick:()=>webToggle(),
  menu:()=>[[WEB_SHOWN?'Back to the desktop':'Show the browser window',()=>webToggle()],['Close the browser window',async()=>{try{await rdPost('web/close')}catch(e){}WEB_ON=WEB_SHOWN=false;webTray()}]]});
 b.classList.toggle('on',WEB_SHOWN)}
async function webToggle(){try{await rdPost(WEB_SHOWN?'web/hide':'web/show');WEB_SHOWN=!WEB_SHOWN}catch(e){WEB_ON=WEB_SHOWN=false}webTray()}
function webWatch(){if(WEB_POLL)return;WEB_POLL=setInterval(async()=>{try{const s=await rd('web/status');const ch=s.running!=WEB_ON||s.shown!=WEB_SHOWN;WEB_ON=s.running;WEB_SHOWN=s.shown;if(ch)webTray();if(!s.running){clearInterval(WEB_POLL);WEB_POLL=null}}catch(e){}},2000)}
/* the strip the browser window leaves free: the taskbar, in screen pixels */
function webStrut(){if(!RD)return;let e=document.getElementById('qsb');if(!e)return;
 while(e.parentElement&&e.parentElement!==document.body&&e.getBoundingClientRect().width<innerWidth-8)e=e.parentElement;
 const r=e.getBoundingClientRect(),dpr=devicePixelRatio||1,top=r.top<innerHeight/2;
 const size=Math.round((top?r.bottom:innerHeight-r.top)*dpr),key=(top?'top':'bottom')+size;if(key==webStrut.last)return;webStrut.last=key;
 rdPost('web/strut?side='+(top?'top':'bottom')+'&size='+Math.max(0,Math.min(200,size))).catch(()=>{})}
if(RD){
 /* pages and apps that ask for a new window get the browser window instead of replacing the desktop */
 window.open=function(u,t,f){const s=String(u||'');if(/^https?:/i.test(s)){webOpen(s);return null}return webNativeOpen(u,t,f)};
 setTimeout(webStrut,3000);addEventListener('resize',()=>setTimeout(webStrut,300));setInterval(webStrut,15000);
 rd('web/status').then(s=>{if(s.running){WEB_ON=true;WEB_SHOWN=s.shown;webTray();webWatch()}}).catch(()=>{})}
/* Oracle's page for a site that opened in the browser window */
function webNote(u){const h=mlEsc(webHost(u));return`<!doctype html><meta charset='utf-8'><body style='font:14px system-ui,sans-serif;margin:40px;color:#1f2937'><h2 style='margin-top:0'>🌐 ${h} opened in a browser window</h2>
<p>Sites with a sign-in (Google, Microsoft, Yahoo, banks…) only work in a real browser window, so Rudolf OS opened <b>${h}</b> there.</p>
<p>Switch between it and the desktop with the 🌐 button in the taskbar, or press <b>Super+Esc</b> to come back to the desktop.</p>
<p><button id='again' style='font:inherit;padding:6px 14px'>Show it again</button></p><script>document.getElementById('again').onclick=()=>parent.postMessage({act:'ext',v:${JSON.stringify(u).replace(/</g,'\\u003c')}},'*')<\/script></body>`}
