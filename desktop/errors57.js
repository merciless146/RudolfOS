/* 1.5.7 (plan stage 6, Part 2 item 8): clear errors. rudolfd's error texts say what failed inside ("rpk failed", "write
   failed (drive full?)"); the desktop shows them as they are in many places. Here the ones a person can meet are said
   as what to do, each with the Help page it points to: rd() gives the new text (the old one stays as e.raw), and a
   notification that carries such a text opens that Help page when clicked. Texts the desktop's own code looks for
   ("changed meanwhile", "busy: ", "could not start", "cannot reach", "denied") are left as they are. */
const ERR57=[
 [/^(the disk is full|write failed \(drive full\?\)|could not save the file \(is the disk full\?\)|could not store the print job \(is the memory full\?\))$/i,'The disk is full. Empty the Trash or delete files you no longer need (Settings › Storage shows what takes the room), then try again.','files'],
 [/^cannot write here \(read-only drive\?\)$/i,'This drive cannot be written to: it is read-only (some SD cards and USB sticks have a lock switch) or not formatted for Rudolf OS.','files'],
 [/^the account's folder is locked$/i,'Your files are locked while you are signed out. Sign in again, then try once more.','privacy'],
 [/^(another task is still running|another job is running|busy, try again in a moment)$/i,'Rudolf OS is busy with another task (an install, an update or a check). Wait until it has finished (Task Manager shows it), then try again.','trouble'],
 [/^no DHCP answer on this network$/i,'The network did not give this computer an address. Check the Wi-Fi password or the cable, move closer to the router or restart it, then try again.','trouble'],
 [/^no time server answered \(offline\?\)$/i,'The clock could not be set from the internet because there is no connection. It sets itself once the computer is online.','trouble'],
 [/^(out of memory|no memory)$/i,'The computer is out of memory. Close apps, games or web pages you are not using, then try again.','trouble'],
 [/^Blitzen is not answering$/i,'Blitzen is not answering. Close large apps or games to give it memory, then ask again.','apps'],
 [/^the sound server is not running$/i,'Sound is not working right now. Restart the computer; if it is still silent, run the self-test in Settings › System health.','trouble'],
 [/^rpk failed$/i,'The package could not be installed. Check the internet connection and try again; if it fails again, use Report a problem.','packages'],
 [/^(could not read the package|this file is not a Debian package that can be read)$/i,'This file is not a Linux package Rudolf OS can install (it may be incomplete: download it again).','apps'],
 [/^the drive that keeps your files was not found$/i,'The drive that keeps your files (RUDOLFDATA) was not found. If Rudolf OS runs from a USB stick, plug it in again and restart.','install'],
 [/^(Linux apps need Rudolf OS installed \(or a RUDOLFDATA partition\)|install Rudolf OS \(or add a RUDOLFDATA partition\) first: apps need somewhere to be kept)$/i,'Apps need somewhere to be kept: install Rudolf OS on this computer (or use a USB stick made with a data partition).','install'],
 [/^the app catalogue has not been downloaded yet$/i,'The App Store\'s list of apps has not been downloaded yet. Connect to the internet and open the App Store again.','apps'],
 [/^the browser window is closed$/i,'That browser window was closed. Open the page again.','trouble'],
];
const ERR57H=new Map();
function errFriendly(e){try{const m=String(e&&e.message||'');for(const [re,t,h] of ERR57)if(re.test(m)){const n=new Error(t);n.raw=m;n.help=h;ERR57H.set(t,h);return n}}catch(x){}return e}
{const _rd=rd;rd=async function(p,o){try{return await _rd(p,o)}catch(e){throw errFriendly(e)}}}
/* a notification with one of these texts opens its Help page (unless it has its own action) */
{const _n=notify;notify=function(title,body,opt){opt=opt||{};if(!opt.action&&ERR57H.size){const s=String(title)+' '+String(body||'');for(const [t,h] of ERR57H)if(s.includes(t)){opt=Object.assign({},opt,{action:()=>{try{helpApp(h)}catch(x){}}});break}}return _n(title,body,opt)}}
