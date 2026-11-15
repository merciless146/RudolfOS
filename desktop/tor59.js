/* ===== Rudolf OS 1.5.9 (docs/plan-1.5.9.md Part 16): Browse with Tor in Oracle
   The button at the right end of Oracle's bookmarks bar (after its "+"): not pressed (a black outline) in a normal window,
   pressed (a white outline) in a Tor window; its tooltip "Browse Oracle with Tor"; its picture an onion of Rudolf OS's own
   (lavender, drawn below: a kitchen onion with its skin's lines and roots, never the Tor Project's logo, its trademark).
   - Pressed in an Oracle window: rudolfd starts Tor (tor/start: the window says "Connecting to Tor… N %"), then that window's
     tabs open in a Tor window and the window closes. Other windows and apps are untouched.
   - The Tor window is Rudolf's Chromium of its own going through Tor (its own profile in memory, SOCKS5 at 127.0.0.1:9050, so
     sites' names are looked up through Tor; WebRTC only through the proxy): the native desktop opens it (op "tor"), on
     Standard rudolfd (torwin/open). Its page is Oracle alone at #app=browser&tor=1&url=..., every tab private; new tabs and
     new windows from it stay Tor windows. It says plainly what Tor does and does not do, and links Tor Browser (Flathub).
   - Pressed again there: its tabs open in a normal Oracle window (the native desktop: op "untor"; on Standard the desktop
     page, asked through rudolfd: torwin/back) and the Tor window closes.
   - Tor not connected in 60 s: the window says so with Tor's last line and stays a normal window.
   While a Tor window is open it asks rudolfd for Tor every 20 s (tor/start), and says when it closes (tor/stop: Tor stops a
   minute later unless another window asks). ===== */
const TOR_URL_OK=u=>typeof u=='string'&&u.length<2000&&/^(https?:\/\/|about:|file:\/\/\/)/i.test(u);
/* this page is a Tor window's (Oracle alone, at #app=browser&tor=1) */
function torWin(){try{return typeof ONEAPP=='object'&&!!ONEAPP&&ONEAPP.k=='browser'&&ONEAPP.q.get('tor')=='1'}catch(e){return false}}
const torNative=()=>typeof oaOut=='function'&&oaOut()&&typeof RD!='undefined'&&!!RD&&RD.engine=='native';
/* the onion: Rudolf OS's own drawing (lavender bulb, the skin's lines, a pointed top, roots) */
function torSvg(n){n=n||16;return `<svg class='torsvg' viewBox='0 0 24 24' width='${n}' height='${n}' aria-hidden='true' style='display:block'>
<path d='M12 2.2c.5 1.7 1.4 2.8 2.7 3.8C17.7 8.3 20.2 11 20.2 14.7c0 4.2-3.7 6.9-8.2 6.9s-8.2-2.7-8.2-6.9C3.8 11 6.3 8.3 9.3 6 10.6 5 11.5 3.9 12 2.2z' fill='#c4b5fd' stroke='#6d28d9' stroke-width='1.2' stroke-linejoin='round'/>
<path d='M12 6.3c-2.4 2.1-3.4 4.9-3.4 8.1 0 3 1.1 5.5 3.4 7.1M12 6.3c2.4 2.1 3.4 4.9 3.4 8.1 0 3-1.1 5.5-3.4 7.1M12 6.3v15.2' fill='none' stroke='#7c3aed' stroke-width='.9' stroke-linecap='round'/>
<path d='M10.1 21.6l-.9 1.4M12 21.7v1.5M13.9 21.6l.9 1.4' stroke='#a78bfa' stroke-width='.9' stroke-linecap='round'/></svg>`}
/* the button (browser()'s sync, after the bookmarks bar's "+") */
function oTorBtn(ctx){const b=document.createElement('button');b.className='otor';const on=!!ctx.tor;
 b.title='Browse Oracle with Tor';b.setAttribute('aria-label','Browse Oracle with Tor');b.setAttribute('aria-pressed',on?'true':'false');
 b.style.cssText=`margin-left:auto;display:inline-flex;align-items:center;justify-content:center;padding:0 5px;min-height:20px;border:2px solid ${on?'#fff':'#000'};border-radius:6px;background:${on?'#4c1d95':'transparent'}`;
 b.innerHTML=torSvg(15);b.onclick=()=>{if(ctx.W.torBusy)return;on?torLeave(ctx):torEnter(ctx)};return b}
/* the Tor window's own look: its badge beside "Private", with the onion */
function torDress(W){W.el.classList.add('otorw');const p=W.body.querySelector('.opv');if(p){p.innerHTML=torSvg(13)+'<span>Private · Tor</span>';p.title='This window goes through Tor: sites do not see this computer’s address. It is not Tor Browser’s anonymity.'}}
/* the bar in the window while Tor connects, or when it could not */
function torBar(W,text,err,cancel){let d=W.body.querySelector('.otorbar');
 if(!text){if(d)d.remove();return}
 if(!d){d=document.createElement('div');d.className='otorbar';d.setAttribute('role','status');const fh=W.body.querySelector('.fh');W.body.insertBefore(d,fh)}
 d.style.cssText=`display:flex;align-items:center;gap:8px;padding:6px 10px;font-size:13px;border-bottom:1px solid #c4b5fd;background:${err?'#fef2f2':'#f5f3ff'};color:${err?'#991b1b':'#3b0764'}`;
 d.innerHTML=torSvg(16)+"<span class='otbt' style='flex:1'></span><button class='otbx'></button>";d.querySelector('.otbt').textContent=text;
 const x=d.querySelector('.otbx');x.textContent=err?'Close':'Cancel';x.onclick=()=>{if(cancel)cancel();torBar(W,'')}}
/* the tabs to take along (a new tab's start page as itself) */
const torUrls=ctx=>{const L=(ctx.urls()||[]).filter(TOR_URL_OK).map(u=>/^about:home/.test(u)?'about:home':u);return L.length?L.slice(0,12):['about:home']};
const torQs=L=>L.map(u=>'url='+encodeURIComponent(u)).join('&');
/* a Tor window with these tabs: the native desktop's, or on Standard rudolfd's */
async function torOpenWin(L){if(torNative()){oaPost({op:'tor',url:L.join('\n')});return}
 if(!torWin())await torHand();await rdPost('torwin/open?'+torQs(L));torBackPoll()}
/* (Standard) the account the Tor window signs in as: this page's, with its file key (rudolfd keeps it root-only while the Tor
   window's Chromium runs, as the native session is kept). A Tor window opening another keeps the one given. */
async function torHand(){try{if(!USER)return;const hx=u=>[...new Uint8Array(u)].map(x=>x.toString(16).padStart(2,'0')).join('');
 const b=USER.r=='guest'?{id:'guest',n:'Guest',r:'guest'}:FKRAW?{id:USER.id,n:USER.n,r:USER.r=='admin'?'admin':'user',fk:hx(FKRAW)}:null;
 if(b)await rdPost('torwin/session',JSON.stringify(b))}catch(e){console.warn('Tor window sign-in',e)}}
/* pressed in a normal window */
async function torEnter(ctx){const W=ctx.W;
 if(typeof RD=='undefined'||!RD){torBar(W,'Browse with Tor needs the installed Rudolf OS.',1);return}
 const L=torUrls(ctx);let stop=false;W.torBusy=1;
 const fail=async m=>{W.torBusy=0;try{await rdPost('tor/stop')}catch(e){}if(!stop)torBar(W,m,1)};
 torBar(W,'Connecting to Tor…',0,()=>{stop=true;W.torBusy=0;rdPost('tor/stop').catch(()=>{})});
 try{await rdPost('tor/start')}catch(e){return fail('Tor could not start: '+e.message+'. This window stays a normal window.')}
 const t0=Date.now();
 while(!stop){let s=null;try{s=await rd('tor/status')}catch(e){}
  if(s&&s.state=='on')break;
  if(s&&s.state=='failed')return fail('Tor could not connect: '+(s.line||'it stopped')+'. This window stays a normal window.');
  if(Date.now()-t0>60e3)return fail('Tor could not connect in 60 seconds (no internet, or Tor is blocked on this network)'+(s&&s.line?'. Tor said: '+s.line:'')+'. This window stays a normal window.');
  torBar(W,'Connecting to Tor… '+(s&&s.pct?s.pct:0)+' %',0,()=>{stop=true;W.torBusy=0;rdPost('tor/stop').catch(()=>{})});
  await new Promise(r=>setTimeout(r,1000))}
 if(stop)return;
 torBar(W,'Opening this window’s tabs in a Tor window…');
 try{await torOpenWin(L)}catch(e){return fail('The Tor window could not open: '+e.message)}
 W.torBusy=0;setTimeout(()=>{try{W.close()}catch(e){}},400)}
/* pressed again in a Tor window: its tabs in a normal Oracle window, and this one closes */
async function torLeave(ctx){const W=ctx.W,L=torUrls(ctx);W.torBusy=1;
 try{if(torNative())oaPost({op:'untor',url:L.join('\n')});else await rdPost('torwin/back?'+torQs(L))}catch(e){W.torBusy=0;torBar(W,'The normal window could not open: '+e.message,1);return}
 try{await rdPost('tor/stop')}catch(e){}setTimeout(()=>{try{W.close()}catch(e){}},300)}
/* (Standard) the desktop page opens the tabs a Tor window gave back (torwin/back), while it knows of one */
let TOR_BACK=0;
function torBackPoll(){if(TOR_BACK||torWin())return;let n=0;TOR_BACK=setInterval(async()=>{let j=null;try{j=await rd('torwin/back')}catch(e){}
 if(j&&Array.isArray(j.urls)&&j.urls.length){const L=j.urls.filter(TOR_URL_OK);if(L.length){browser(L[0]=='about:home'?null:L[0]);const W=wins.filter(w=>w.oNewTab).pop();L.slice(1).forEach(u=>{try{W&&W.oNewTab&&W.oNewTab(u=='about:home'?null:u)}catch(e){}})}}
 if(j&&!j.open&&++n>3){clearInterval(TOR_BACK);TOR_BACK=0}else if(j&&j.open)n=0},2000)}
/* the Tor window's start page (a private window's, about:home) */
function torHome(doc,esc){return doc('Browsing through Tor',`<div style='max-width:580px;margin:10px auto;line-height:1.55'>
 <div style='display:flex;justify-content:center'>${torSvg(54)}</div>
 <h2 style='text-align:center;margin:.3em 0 .6em'>This window goes through Tor</h2>
 <p><b>What it does:</b> the websites you visit here do not see this computer's internet address, and your network (at school or work, its administrator) and your internet provider do not see which sites you visit. Every tab is private: nothing is kept when the window closes.</p>
 <p><b>What it does not do:</b> this is not Tor Browser's anonymity. Websites can still recognise this browser by its fingerprint, and what you sign in to knows it is you. Many sites are slower over Tor or ask for a captcha, and Google's sign-in often refuses it.</p>
 <p>For real anonymity, use <a href='#' data-u='https://flathub.org/apps/org.torproject.torbrowser-launcher' style='color:#2563eb' onclick='go(this.dataset.u);return false'>Tor Browser</a> (Flathub, in Apps).</p>
 <p style='color:#6b7280'>Type an address or search in the box above. Press the onion at the right of the bookmarks bar to take this window's tabs back to a normal window.</p></div>`)}
if(torWin()){
 /* asking for Tor while the window is open (every 20 s), and saying when it closes */
 setInterval(()=>{if(typeof RD!='undefined'&&RD)rdPost('tor/start').catch(()=>{})},20000);
 {let said=0;const bye=()=>{if(said)return;said=1;try{rdPost('tor/stop').catch(()=>{})}catch(e){}};
  const mo=new MutationObserver(()=>{if(OA.main&&!OA.main.el.isConnected){mo.disconnect();bye()}});mo.observe(document.documentElement,{childList:true,subtree:true});addEventListener('pagehide',bye)}
 /* new windows from it stay Tor windows (Ctrl+N, a link opened in a new window, New private tab) */
 if(typeof browser=='function'){const _b=browser;browser=function(u,bo){if(OA.main&&OA.main.el&&OA.main.el.isConnected){torOpenWin([TOR_URL_OK(u)?u:'about:home']).catch(()=>{});return null}
  return _b.call(this,u,Object.assign({},bo||{},{priv:1,tor:1}))}}
 if(typeof oraclePrivate=='function'){const _p=oraclePrivate;oraclePrivate=function(u){if(OA.main&&OA.main.el&&OA.main.el.isConnected){if(OA.main.oNewTab)OA.main.oNewTab(u||null);return OA.main}return _p.apply(this,arguments)}}
 /* (Standard) no native desktop reads this window's messages: it is an Oracle page of its own (its notifications in it), and
    the desktop page takes its downloads in, as every download's on Standard */
 if(typeof RD!='undefined'&&RD&&RD.engine!='native'){try{oaChromium=()=>false}catch(e){}try{dlPoll=function(){}}catch(e){}}}
/* (Native) while a Tor window is open, its page takes the downloads in (the files of every window land in one folder): Oracle's
   other windows leave them (they ask rudolfd whether Tor runs) */
let TOR_RUNS=0;
if(typeof ONEAPP=='object'&&ONEAPP&&ONEAPP.k=='browser'&&!torWin()){const chk=async()=>{try{if(typeof RD!='undefined'&&RD&&RD.engine=='native'){const s=await rd('tor/status');TOR_RUNS=s.state=='on'||s.state=='starting'}}catch(e){TOR_RUNS=0}};chk();setInterval(chk,5000)}
function torDlSkip(){return !torWin()&&!!TOR_RUNS&&typeof RD!='undefined'&&!!RD&&RD.engine=='native'}
{const st=document.createElement('style');st.textContent=`.otorw .otabs{background:#1e1b4b!important}.otorw .ob{background:#2e1065!important}.otorw .opv{background:#6d28d9}.opv .torsvg{flex:none}
.obm .otor:hover{background:#ede9fe}.otorw .obm .otor:hover{background:#5b21b6}`;document.head.appendChild(st)}
