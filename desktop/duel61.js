/* ===== Rudolf OS 1.6.1: duels on the local network for the Library's games (Ember Reach 0.5.2 and later) =====
   A page can't take connections, so for a duel between two computers on one network one of them relays: the host's
   game asks through the game kit (gamekit 1.3.0, RG.lan), and the desktop asks rudolfd (with its token, as every other
   system call), which runs the duel service (rudolf-duel) for that one match:
     lanHost {ver} -> {url, code}     the person is asked first ("Host a duel on this network?"); rudolfd starts the
                                      service as the signed-in account's own Linux user (never root), opens the
                                      firewall for its port to the local network only, and announces it by mDNS
     lanFind -> {games}               the duels hosted on the network (rudolf-duel find: mDNS)
     lanStop                          the service stopped (and the firewall closed); also when the game's window closes
   Only the project's own Library apps get these (an outside developer's app is refused: pkgbox59.js). The native
   desktop's app windows are this same page, so it is the same there.
   ===== */
window.__duel61=1;
const DUEL_HOSTS=new Map();   /* a game's frame -> true while rudolfd hosts its duel */
/* the person agrees to host (each time: other computers on the network can connect while it lasts) */
function duelHostAsk(title){return appAskDlg('Host a duel on this network?',(title||'This game')+' wants to host a duel. While it lasts, other computers on this network can join it (Rudolf OS opens one port to the local network only, never to the internet, and closes it when the duel ends).','Host')}
const duelVerOk=v=>typeof v=='string'&&/^[0-9A-Za-z.+-]{1,20}$/.test(v);
/* games58.js hands on the game kit's messages it does not know: true if it was one of these (and answered) */
function gameDuelOp(m,at,reply){
 if(m.op!='lanHost'&&m.op!='lanFind'&&m.op!='lanStop')return false;
 if(at.f.dataset.dev=='1'){reply({ok:false,error:'duels on the network are for the Rudolf OS Library\'s own games'});return true}
 if(!RD){reply({ok:false,error:'this needs Rudolf OS itself'});return true}
 (async()=>{try{
  if(m.op=='lanHost'){
   if(!duelVerOk(m.ver))return reply({ok:false,error:'bad version'});
   const W=typeof wins!='undefined'&&wins.find(w=>w.el&&w.el.contains(at.f)),t=W&&W.el.querySelector('.t');
   if(!(await duelHostAsk(t&&t.textContent)))return reply({ok:false,error:'you chose not to host'});
   const r=await rdPost('duel/host?ver='+encodeURIComponent(m.ver));DUEL_HOSTS.set(at.f,true);
   return reply({ok:true,url:r.url,code:r.code})}
  if(m.op=='lanFind'){const r=await rd('duel/find');
   return reply({ok:true,games:(Array.isArray(r.games)?r.games:[]).slice(0,32).map(g=>({name:String(g.name||'').slice(0,64),url:String(g.url||''),code:String(g.code||''),ver:String(g.ver||'').slice(0,20)}))})}
  if(DUEL_HOSTS.delete(at.f))await rdPost('duel/stop');
  reply({ok:true})
 }catch(e){reply({ok:false,error:String(e&&e.message||e).slice(0,200)})}})();
 return true}
/* a hosting game's window closed: its duel service goes with it */
setInterval(()=>{for(const f of [...DUEL_HOSTS.keys()])if(!f.isConnected){DUEL_HOSTS.delete(f);rdPost('duel/stop').catch(()=>{})}},2000);
