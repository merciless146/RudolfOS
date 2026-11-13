/* ===== Rudolf OS 1.5.9 (docs/plan-1.5.9.md 7.3): calls to phone numbers =====
   Rudolf OS calls people through websites and apps (Meet, Zoom, Teams in Oracle; Zoom's Linux app). Calling a phone
   number needs the service's own calling plan: nothing is installed or signed up for by Rudolf OS. Settings › Apps ›
   "Calls to phones" says what each one needs and opens it in Oracle. */
const PHONE59=[
 ['Google Voice','📞','https://voice.google.com/','Free calls to the US and Canada with a Google account (a US phone number needed to set it up). Works in Oracle on both desktop engines.'],
 ['Zoom Phone','🎥','https://zoom.us/pricing/zoom-phone','A paid add-on to a Zoom account. Calls from Zoom’s web client in Oracle, or from the Zoom app (App Store).'],
 ['Microsoft Teams Phone','💼','https://www.microsoft.com/microsoft-teams/microsoft-teams-phone','Paid, usually through work or school. Calls from teams.microsoft.com in Oracle.']];
{const _ap=appsPanel;appsPanel=function(c){_ap.apply(this,arguments);try{
 const d=document.createElement('div');d.className='ph59';
 d.innerHTML=`<h4 class='sh4'>Calls to phones</h4><p class='msg'>Video calls work in Oracle (Google Meet, Zoom, Microsoft Teams) and with Zoom’s app. Calling a <b>phone number</b> needs a calling plan from one of these services; Rudolf OS does not sign you up for anything.</p><div class='ph59l'></div>`;
 if(S.telApp){const t=document.createElement('p');t.className='msg';t.innerHTML=`Phone numbers on web pages open with <b></b>. <a href='#'>Ask each time</a>`;t.querySelector('b').textContent=S.telApp=='phone'?'Phone screen':(TEL59.find(x=>x[0]==S.telApp)||[0,0,S.telApp])[2];t.querySelector('a').onclick=e=>{e.preventDefault();delete S.telApp;try{persistS()}catch(x){}t.remove()};d.appendChild(t)}
 const L=d.querySelector('.ph59l');PHONE59.forEach(([n,i,u,t])=>{const r=document.createElement('div');r.className='acr';r.style.cssText='display:flex;gap:10px;align-items:center;padding:6px 0';
  r.innerHTML=`<span style='font-size:22px'>${i}</span><span style='flex:1'><b></b><div class='msg'></div></span><button>Open in Oracle</button>`;r.querySelector('b').textContent=n;r.querySelector('.msg').textContent=t;
  r.querySelector('button').onclick=()=>{try{webOpen(u)}catch(e){window.open(u,'_blank')}};L.appendChild(r)});
 c.appendChild(d)}catch(e){console.warn(e)}}}
/* 7.3: a phone number clicked in Oracle's tabs (tel: links, both engines): how to call it. The person chooses once (and
   may tick "Always"): Google Voice or Microsoft Teams in Oracle, the Android phone on the desk (Phone screen), or the
   number copied. Nothing calls without the person pressing call in that app. */
const TEL59=[['voice','📞','Google Voice',n=>'https://voice.google.com/u/0/calls?a=nc,'+encodeURIComponent(n)],['teams','💼','Microsoft Teams',n=>'https://teams.microsoft.com/l/call/0/0?users=4:'+encodeURIComponent(n)]];
function telClean(n){return String(n||'').replace(/^\/\//,'').split(/[;?]/)[0].replace(/%2B/i,'+').replace(/[^0-9+*#]/g,'').slice(0,32)}
async function telGo(k,n){const t=TEL59.find(x=>x[0]==k);
 if(t)return webOpen(t[3](n));
 if(k=='phone'){if(typeof phoneScreen=='function')phoneScreen();try{await navigator.clipboard.writeText(n)}catch(e){}return notify('Phone screen','The number '+n+' is copied: paste it in the phone’s dialler.')}
 try{await navigator.clipboard.writeText(n);notify('Phone number copied',n)}catch(e){notify('Phone number',n)}}
function telOpen(raw){const n=telClean(raw);if(!n)return;if(S.telApp)return telGo(S.telApp,n);
 const o=document.createElement('div');o.className='rmodal';
 o.innerHTML=`<div class='rmbox' role='dialog' aria-label='Call a phone number' style='max-width:420px'><h3>📞 Call <span class='tn'></span></h3><p class='msg'>Calling a phone number needs a calling service (Settings › Apps › Calls to phones).</p><div class='tl' style='display:flex;flex-direction:column;gap:6px'></div><label class='ck' style='display:block;margin-top:8px'><input type='checkbox' class='ta'> Always use this</label><div class='rmrow'><button class='n'>Cancel</button></div></div>`;
 o.querySelector('.tn').textContent=n;const L=o.querySelector('.tl');
 const opts=TEL59.map(x=>[x[0],x[1]+' '+x[2]]);if(typeof phoneScreen=='function')opts.push(['phone','📱 The Android phone on the desk (Phone screen)']);opts.push(['copy','📋 Copy the number']);
 opts.forEach(([k,l])=>{const b=document.createElement('button');b.textContent=l;b.style.textAlign='left';b.onclick=()=>{if(o.querySelector('.ta').checked&&k!='copy'){S.telApp=k;try{persistS()}catch(e){}}o.remove();telGo(k,n)};L.appendChild(b)});
 o.querySelector('.n').onclick=()=>o.remove();document.body.appendChild(o)}
