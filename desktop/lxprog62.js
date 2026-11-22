/* ===== Rudolf OS 1.6.2 (docs/plan-1.6.2.md Part 25b, from 1.6.1 Part 25 item 5): Linux apps' progress on their taskbar buttons =====
   Linux apps (Firefox, Chromium-based apps, Electron apps and others) say their downloads' progress with the Unity LauncherEntry
   signal on the account's own session bus. The account's own rudolf-portal hears it and leaves a note for the desktop; rudolfd's
   portal/progress passes on only the signed-in account's apps' desktop file IDs, their StartupWMClass and a whole percentage
   (rudolfd-lxprog.inc). Here each Linux app's taskbar button (linux26.js lxTaskbar, .lxtb) gets the same fill as Rudolf's own
   windows (tbprog61.js: tbpState, tbpPaint, the Progress colour), while its window is not the one shown (Standard shows one app
   window at a time: the others are minimized); a percentage only, never the creeping fill (that is for Rudolf's own work with no
   percentage). The app's windows: those whose class or instance (WM_CLASS) is the ID, the ID's last part (of more than 2 letters)
   or its StartupWMClass, ignoring case: rudolf-native.c lxp_match finds them by the same rule. A window's own progress (the XApp
   hint _NET_WM_XAPP_PROGRESS, 1-100, which Firefox sets on X11 rather than sending the signal: rudolf-display reads it, rudolfd's
   "windows") comes first, on that window alone. Nothing it is given becomes HTML:
   only a number for the fill's width. The native desktop's shell draws its own (rudolf-native.c lxp_tick); its pages ask nothing. ===== */
const LXP={w:new Map()};   /* a Linux window's id -> what winProgress keeps it under: {el, tb: its button, min, lx: its id} */
const LXP_ID=/^[A-Za-z0-9._-]{1,120}$/;
function lxpMatch(a,cls,inst){const c=String(cls||'').toLowerCase(),i=String(inst||'').toLowerCase();if(!c&&!i)return false;
 const id=a.id.toLowerCase(),last=id.split('.').pop(),ks=[id,last.length>2?last:'',a.wmclass.toLowerCase()].filter(Boolean);
 return ks.some(k=>k==c||k==i)}
function lxpApps(j){return (j&&Array.isArray(j.apps)?j.apps:[]).filter(a=>a&&typeof a.id=='string'&&LXP_ID.test(a.id)&&Number.isInteger(a.v)&&a.v>=0&&a.v<=100)
 .map(a=>({id:a.id,wmclass:typeof a.wmclass=='string'&&LXP_ID.test(a.wmclass)?a.wmclass:'',v:a.v}))}
/* an app window's own progress (the XApp hint _NET_WM_XAPP_PROGRESS, which Firefox sets on X11; rudolf-display reads it): that window's */
function lxpWins(j){const m=new Map();(j&&Array.isArray(j.windows)?j.windows:[]).forEach(o=>{if(o&&typeof o.win=='string'&&/^[0-9a-f]{1,16}$/.test(o.win)&&Number.isInteger(o.v)&&o.v>=1&&o.v<=100)m.set(o.win,o.v)});return m}
function lxpBtn(win){const T=document.getElementById('tasks');if(!T)return null;for(const b of T.querySelectorAll(':scope > .lxtb'))if(b.dataset.win===win)return b;return null}
/* the windows' state (shown or not, their buttons) as linux26.js has them now */
function lxpSync(){for(const [win,W] of LXP.w){const w=LX.wins.find(x=>x.win===win),b=w&&lxpBtn(win);if(!w||!b){winProgress(W,null,'lx');LXP.w.delete(win);continue}W.el=b;W.tb=b;W.min=!w.shown}}
function lxpSet(apps,own){own=own||new Map();LX.wins.filter(w=>!w.web).forEach(w=>{const ms=apps.filter(a=>lxpMatch(a,w.class,w.instance)),b=lxpBtn(w.win);let W=LXP.w.get(w.win);
  const v=own.has(w.win)?own.get(w.win):ms.length?ms.reduce((s,a)=>s+a.v,0)/ms.length:null;   /* (the window's own first: it is exactly that window's) */
  if(v==null||!b){if(W)winProgress(W,null,'lx');return}
  if(!W){W={el:b,tb:b,min:!w.shown,lx:w.win};LXP.w.set(w.win,W)}
  winProgress(W,v,'lx')});lxpSync()}
async function lxpPoll(){if(typeof ONEAPP!='undefined'&&ONEAPP)return;if(!LX.wins.some(w=>!w.web)&&!LXP.w.size)return;
 let j;try{j=await lxApi('portal/progress')}catch(e){return}lxpSet(lxpApps(j),lxpWins(j))}
/* drawn with Rudolf's own windows' fills (tbprog61.js tbpDraw leaves the Linux apps' buttons to this) */
{const o=tbpDraw;tbpDraw=function(){let live=o.apply(this,arguments);try{lxpSync();for(const W of LXP.w.values()){const s=W.min?tbpState(W):null;if(s&&(s.end||s.v==null))live=1;tbpPaint(W.tb,s)}}catch(e){}return live}}
/* shown or put back (the taskbar redrawn from rudolfd's list): at once */
lxTaskbar=__after(lxTaskbar,function(){if(LXP.w.size){lxpSync();tbpKick()}},'lxprog62.js');
if(typeof RD!='undefined'&&RD)setInterval(lxpPoll,1000);
