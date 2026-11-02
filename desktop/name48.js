/* Rudolf OS 1.4.8: the L in Rudolf stands for Linux (Runtime Driver Open Linux Framework), not Lua. The sign-in screen,
   About, VER and the Prompt say so from this version; C:\README.TXT is written once, when a computer is first set
   up, so a copy saved by an older version is updated here (only that sentence: the rest is left as the person has it). */
function nameFix(){try{const C=fs&&fs['C:'],r=C&&C['README.TXT'];if(typeof r!='string'||!r.includes('Runtime Driver Open Lua Framework'))return false;
 C['README.TXT']=r.split('Runtime Driver Open Lua Framework').join('Runtime Driver Open Linux Framework');fsSave();return true}catch(e){return false}}
setTimeout(nameFix,3000);
(function(){const o=window.__afterLogin;window.__afterLogin=()=>{o&&o();nameFix()}})();
