/* ===== Rudolf OS 1.6.2 (docs/plan-1.6.2.md Part 30): two-player Library games on a home network, the desktop's side.
   A game page cannot take connections, so while a player hosts, Rudolf OS runs the LAN game relay on this computer
   (rudolfd's duel/ requests: rudolf-duel as the account's own user, the firewall's port open to the local network only
   while it runs, announced by multicast DNS) and the other computer finds it. The game asks through the game kit
   (rudolfos-library lib/gamekit 1.3.0: RG.lan), whose messages games58.js hands here:
     lanHost {v}  -> {url, code, name}   the relay started for this package and version
     lanFind      -> {games}             this package's games hosted on the network (name, url, code, v)
     lanStop      -> {}                  stopped (it also stops by itself when the hosting game leaves)
   Only the project's own Library apps (signed by the Library) may: an outside developer's app is refused. On Standard,
   Lite and Native alike (Native's game windows are this page). ===== */
window.__duel62=1;
const DUEL_ERR=e=>String(e&&e.message||e||'').slice(0,300)||'Rudolf OS couldn’t do that.';
async function gameLanOp(m,at,reply){
 if(at.f.dataset.dev=='1')return reply({ok:false,error:'Games from outside developers can’t host on the network.'});
 if(!RD)return reply({ok:false,error:'This needs Rudolf OS itself (the bootable disc, a USB drive or the hard disk).'});
 const pkg=encodeURIComponent(at.pkg);
 try{
  if(m.op=='lanHost'){const v=String(m.v||'');if(!/^[0-9A-Za-z.+-]{1,20}$/.test(v))return reply({ok:false,error:'bad version'});
   const r=await rdPost('duel/host?v='+encodeURIComponent(v)+'&game='+pkg);return reply({ok:true,url:r.url,code:r.code,name:r.name})}
  if(m.op=='lanFind'){const r=await rd('duel/find?game='+pkg);return reply({ok:true,games:(r.games||[]).slice(0,32)})}
  if(m.op=='lanStop'){await rdPost('duel/stop');return reply({ok:true})}
 }catch(e){return reply({ok:false,error:DUEL_ERR(e)})}
 reply({ok:false,error:'unknown op'})}
