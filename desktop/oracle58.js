/* ===== Rudolf OS 1.5.8 (plan Part 9): Oracle's tabs on the Lite desktop, "Open in Chromium", and FedCM.
   9.1 With the Lite desktop (WebKit), rudolf-webkit is the shell: it makes Oracle's tabs real WebKit pages with the
       same messages as the Chromium shell (iso/shell/shell.js), so tabs26.js works as it is. Its messages arrive
       through window.__rwkIn and go out through WebKit's message handler; to shell26.js it looks like the shell.
   9.2 Sites WebKit cannot play (Widevine: Netflix, Spotify, Disney+, Prime Video, Max, Hulu, Apple TV+) open in
       Chromium's browser window by themselves; any other site can be sent there from Oracle's menu (Open in Chromium).
   9.6 Oracle's menu › Sign-in with FedCM (off by default): Chromium's own way for "Sign in with Google" buttons. On
       the Vivobook, Google's sign-in through it stopped on a blank window (1.5.7), so it is off unless chosen. Kept for
       the whole computer (rudolfd web/fedcm), for the browser window and for Oracle's tabs on the Standard desktop.
   ===== */
if(typeof RD!='undefined'&&RD&&RD.engine=='webkit'&&window.webkit&&webkit.messageHandlers&&webkit.messageHandlers.rudolfShell){
 const CH=new MessageChannel(),P=CH.port1;
 /* (a MessagePort, so the events' source is an object of its own: shell26.js and tabs26.js check e.source) */
 P.postMessage=m=>{try{webkit.messageHandlers.rudolfShell.postMessage(JSON.parse(JSON.stringify(m)))}catch(e){}};
 SHELL_ORIGIN='rudolf-webkit://desktop';
 window.__rwkIn=m=>{try{dispatchEvent(new MessageEvent('message',{data:m,origin:SHELL_ORIGIN,source:P}))}catch(e){}};
}
const RT_CHROMIUM=['netflix.com','spotify.com','disneyplus.com','primevideo.com','amazon.com/gp/video','max.com','hbomax.com','hulu.com','tv.apple.com','peacocktv.com','paramountplus.com','crunchyroll.com','tidal.com','music.amazon.com','deezer.com','play.hbomax.com'];
/* true when this address should open in Chromium's window: only with Lite's WebKit tabs */
function rtChromium(u){if(!RD||RD.engine!='webkit'||typeof rtOn!='function'||!rtOn())return false;let x;try{x=new URL(u)}catch(e){return false}
 const h=x.hostname.toLowerCase(),hp=h+x.pathname;
 return RT_CHROMIUM.concat(S.chromiumSites||[]).some(d=>d.includes('/')?(hp==d||hp.startsWith(d)||hp.startsWith('www.'+d)):(h==d||h.endsWith('.'+d)))}
/* the browser window itself, whatever Oracle would do */
async function webOpenChromium(u){if(!RD)return;let href;try{href=new URL(u).href}catch(e){return}
 if(window.__sys&&typeof fwCheck=='function'){const g=fwCheck(href);if(g){notify('Blocked by the firewall',g.why,{kind:'error'});return}}
 href=href.replace(/'/g,'%27').replace(/`/g,'%60').replace(/\$/g,'%24').replace(/\\/g,'%5C').replace(/"/g,'%22');
 try{await rdPost('web/open?url='+encodeURIComponent(href))}catch(e){return notify('The browser window could not open',e.message,{kind:'error'})}
 WEB_ON=WEB_SHOWN=true;webTray();webWatch()}
let FEDCM=null;
async function fedcmGet(){if(!RD)return null;try{FEDCM=await rd('web/fedcm')}catch(e){FEDCM=null}return FEDCM}
async function fedcmSet(on){if(!RD)return;if(USER&&USER.r=='guest'){notify('Oracle','Guests cannot change this.',{kind:'error'});return}
 try{FEDCM=await rdPost('web/fedcm?on='+(on?1:0))}catch(e){return notify('Oracle',e.message,{kind:'error'})}
 const tabs=RD.engine!='webkit'&&typeof rtOn=='function'&&rtOn();
 const msg='Sign-in with FedCM is '+(on?'on':'off')+'. '+(FEDCM&&FEDCM.running?'Close the browser window to apply it there (its tabs come back when it opens again). ':'')+(tabs?'Oracle\'s tabs use it from the next sign-in or restart.':'');
 notify('Oracle',msg,FEDCM&&FEDCM.running?{action:async()=>{try{await rdPost('web/close')}catch(e){}WEB_ON=WEB_SHOWN=false;webTray()}}:{})}
if(RD)setTimeout(fedcmGet,8000);
/* Oracle's ☰ menu: Open in Chromium (with Lite), Sign-in with FedCM */
window.__oracleMenu=o=>{if(!RD)return[];const out=[],u=o&&o.url||'';
 if(RD.engine=='webkit'&&/^https?:/i.test(u))out.push(['Open in Chromium',()=>webOpenChromium(u)]);
 out.push([(FEDCM&&FEDCM.on?'✓ ':'')+'Sign-in with FedCM',async()=>{await fedcmGet();fedcmSet(!(FEDCM&&FEDCM.on))}]);
 return out};
/* a sign-in that stopped on Google's blank FedCM window: offered once, in the browser window's notice */
(function(){if(!RD)return;let told=false;addEventListener('message',e=>{const m=e.data;if(told||!m||m.rudolfShell!='tabev'||m.ev!='nav')return;
 if(/^https:\/\/accounts\.google\.com\/gsi\/(transform|fedcm)/.test(m.url||'')){told=true;notify('Sign-in stuck?','If Google\'s sign-in stays on a blank page, try it with Sign-in with FedCM '+(FEDCM&&FEDCM.on?'off':'on')+' (Oracle\'s ☰ menu).',{app:'Oracle',action:()=>fedcmSet(!(FEDCM&&FEDCM.on))})}})})();
/* 1.5.8: the session replaced an older copy of Oracle's tabs (rudolf-shellfix): their storage went with it */
if(typeof EVH!='undefined')EVH.push(t=>{if(/^shell 1$/m.test(t))notify('Oracle\'s tabs were updated','Websites in Oracle may ask you to sign in again, once.',{app:'Oracle'})});
