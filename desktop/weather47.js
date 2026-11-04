/* Rudolf OS 1.4.7: where Weather and its desktop widget are for.
   Before, both showed Berlin until a city was searched in Weather, and the widget never changed afterwards. Now:
   - the city chosen in Weather (bd95wloc) wins;
   - otherwise the city of System Settings' time zone (America/Chicago -> Chicago), looked up with Open-Meteo's
     geocoding (the same service Weather's search uses; only the city's name is sent), kept with the zone it came
     from so a new time zone finds a new city;
   - a zone that names no place (UTC, Etc/...) asks for a city instead.
   Temperatures are in °F for places in the United States, Liberia and Myanmar, °C elsewhere, unless Weather's
   °C/°F switch says otherwise (bd95wunit). The widget follows a change at once ('rudolf-wloc') and every 15 minutes. */
function wxTz(){try{return(typeof S!='undefined'&&S.tz)||Intl.DateTimeFormat().resolvedOptions().timeZone||''}catch(e){return''}}
function wxTzCity(tz){if(!tz||!tz.includes('/')||/^(Etc|UTC|GMT|SystemV)\b/i.test(tz))return'';return tz.split('/').pop().replace(/_/g,' ')}
async function wxSearch(name,n){const r=await fetch('https://geocoding-api.open-meteo.com/v1/search?language=en&count='+(n||5)+'&name='+encodeURIComponent(name));const j=await r.json();
 return(j.results||[]).map(g=>({lat:g.latitude,lon:g.longitude,name:g.name+(g.admin1&&g.admin1!==g.name?', '+g.admin1:'')+(g.country?', '+g.country:''),cc:g.country_code||'',zone:g.timezone||''}))}
/* the place to show: {lat, lon, name, cc, tz?, guess?}, or null when it is not known (ask) */
async function weatherLoc(){
 const saved=ld('bd95wloc',null);if(saved&&!saved.guess)return saved;
 const tz=wxTz(),city=wxTzCity(tz);if(!city)return null;
 if(saved&&saved.guess&&saved.tz===tz)return saved;
 const res=await wxSearch(city,10);
 /* the match in that very time zone (Paris, France for Europe/Paris, not Paris, Texas), else in the same part of the world */
 const reg=tz.split('/')[0],pick=res.find(g=>g.zone===tz)||res.find(g=>g.zone.split('/')[0]===reg)||res[0];if(!pick)return null;delete pick.zone;
 const loc=Object.assign(pick,{tz,guess:1});svj('bd95wloc',loc);return loc}
function wxF(loc){const u=ld('bd95wunit','');if(u)return u=='F';return!!loc&&['US','LR','MM'].includes(loc.cc)}
function wxT(c,f){return f?Math.round(c*9/5+32)+'°F':Math.round(c)+'°C'}
function wxSet(loc){svj('bd95wloc',loc);try{dispatchEvent(new Event('rudolf-wloc'))}catch(e){}}
/* 1.5.1: the widget after a failed fetch. At start-up the widget asks before Wi-Fi is connected, failed ("Weather needs an
   internet connection") and waited 15 minutes for its next try, while Weather itself (opened later) worked. Now a widget
   that could not fetch tries again every 30 s, at once when the network comes back, and whenever Weather shows a
   forecast. */
function wxWidgetsRetry(all){document.querySelectorAll('[data-wx]').forEach(b=>{try{if(b._wx&&(all||/needs an internet connection|^Loading/.test(b.textContent)))b._wx()}catch(e){}})}
addEventListener('online',()=>setTimeout(()=>wxWidgetsRetry(),2000));
setInterval(()=>wxWidgetsRetry(),30000);
if(typeof weatherApp=='function'){const _wa47=weatherApp;weatherApp=function(){const r=_wa47.apply(this,arguments);setTimeout(()=>wxWidgetsRetry(),5000);return r}}
