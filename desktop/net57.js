/* 1.5.7 (plan stage 8, Part 3 item 3): the network at start-up. The Weather widget failed once at start-up, before
   the Wi-Fi had an address, and waited 15 minutes (fixed in 1.5.6 for the widget). Here the same for the rest:
   netLive() is rudolfd's network state (Wi-Fi or cable with an address; the browser's own when it is not known),
   and the event "rudolf-online" fires when the computer comes online (net31's poll, the browser's "online", rudolfd's
   "wifi connected"). netWait(ms) waits for it (true), or gives up after ms (false). The update check, the Library's
   list, Flathub's list and Blitzen's download wait for it instead of failing and waiting for the next turn; a page
   that Oracle could not open offline opens by itself once online. */
function netLive(){return window.__net31?!!window.__net31.up:navigator.onLine!==false}
function netWait(ms){if(netLive())return Promise.resolve(true);
 return new Promise(res=>{let t=0;const f=()=>{if(!netLive())return;clearTimeout(t);removeEventListener('rudolf-online',f);res(true)};
  addEventListener('rudolf-online',f);if(ms)t=setTimeout(()=>{removeEventListener('rudolf-online',f);res(false)},ms)})}
const NET57={was:null};
function net57Check(){const up=netLive();if(up&&NET57.was===false){dispatchEvent(new Event('rudolf-online'));
 /* Oracle's own pages ("No internet connection") open what was asked for */
 document.querySelectorAll('iframe').forEach(f=>{try{f.contentWindow&&f.contentWindow.postMessage('rudolf-online','*')}catch(e){}})}NET57.was=up}
if(RD){const _p=net31Poll;net31Poll=async function(){const r=await _p.apply(this,arguments);net57Check();return r};setInterval(net57Check,5000)}
addEventListener('online',()=>setTimeout(()=>{if(!RD){NET57.was=false;net57Check()}else if(typeof net31Poll=='function')net31Poll()},300));
/* a button that needs the internet: pressed offline, it says it waits and goes on by itself once online */
async function netGate(note){if(netLive())return true;if(note)note('Waiting for the internet… It starts by itself once this computer is online (Settings › Network).');await netWait(90000);if(note)note('');return true}   /* (tried anyway after 90 s) */
