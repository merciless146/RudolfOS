/* ===== 1.4.4 (clock31): one clock. clockParts(d) gives the time and date in the time zone set in System Settings
   (S.tz), 12- or 24-hour (S.h24), with System Settings' date format, for the taskbar clock, the calendar, the lock
   screen, the screen saver, the desktop widgets and the notification times. Before, all but the taskbar used
   Chromium's own zone and format (on the Vivobook the calendar said 11:38 PM while the taskbar said 17:38). ===== */
function clockParts(d){d=d||(typeof sysNow=='function'?sysNow():new Date());const o={};
 try{new Intl.DateTimeFormat('en-US',{timeZone:S.tz,hourCycle:'h23',year:'numeric',month:'numeric',day:'numeric',hour:'numeric',minute:'numeric',second:'numeric'}).formatToParts(d).forEach(p=>o[p.type]=+p.value)}
 catch(e){o.year=d.getFullYear();o.month=d.getMonth()+1;o.day=d.getDate();o.hour=d.getHours();o.minute=d.getMinutes();o.second=d.getSeconds()}
 const h=o.hour%24,m=o.minute,p2=x=>String(x).padStart(2,'0');let long;
 try{long=new Intl.DateTimeFormat([],{timeZone:S.tz,weekday:'long',year:'numeric',month:'long',day:'numeric'}).format(d)}catch(e){long=d.toLocaleDateString([],{weekday:'long',year:'numeric',month:'long',day:'numeric'})}
 const iso=`${o.year}-${p2(o.month)}-${p2(o.day)}`;
 return{y:o.year,mo:o.month-1,d:o.day,h,m,s:o.second,iso,long,longNoYear:long.replace(/,? \d{4}$/,'').replace(/ \d{4},/,','),
  time:S.h24?p2(h)+':'+p2(m):((h%12)||12)+':'+p2(m)+' '+(h<12?'AM':'PM'),date:typeof sysDateStr=='function'?sysDateStr(d):iso}}
