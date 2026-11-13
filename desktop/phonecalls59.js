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
 const L=d.querySelector('.ph59l');PHONE59.forEach(([n,i,u,t])=>{const r=document.createElement('div');r.className='acr';r.style.cssText='display:flex;gap:10px;align-items:center;padding:6px 0';
  r.innerHTML=`<span style='font-size:22px'>${i}</span><span style='flex:1'><b></b><div class='msg'></div></span><button>Open in Oracle</button>`;r.querySelector('b').textContent=n;r.querySelector('.msg').textContent=t;
  r.querySelector('button').onclick=()=>{try{webOpen(u)}catch(e){window.open(u,'_blank')}};L.appendChild(r)});
 c.appendChild(d)}catch(e){console.warn(e)}}}
