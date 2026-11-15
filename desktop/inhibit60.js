/* ===== 1.6.0 (docs/plan-1.6.0.md Part 11): apps keeping the screen on or the computer awake =====
   Linux apps ask the desktop through the portal (Flatpak), org.freedesktop.ScreenSaver or PowerManagement (Chromium and
   Electron apps, SDL games, Steam, video players); rudolf-portal keeps what they hold and rudolfd's media answer says
   it (idleHeld, sleepHeld, heldBy). An app holding the screen on: no screen saver, no lock, no sleep, and the screen
   stays on (rudolf-display); one holding the computer awake: no sleep. Kept in INH60 for idleCheck (core25.js). */
window.__inhibit60=1;
let INH60={idle:0,sleep:0,by:[]};
(function(){const M0=mediaOther;mediaOther=async function(){let m=null;try{m=await rd('media')}catch(e){return M0()}
 INH60={idle:m.idleHeld||0,sleep:m.sleepHeld||0,by:m.heldBy||[]};return m.playing>0||m.mic>0||INH60.idle>0}})();
function inhWho(){const b=[...new Set(INH60.by||[])];return b.length?b.slice(0,3).join(', '):'an app'}
