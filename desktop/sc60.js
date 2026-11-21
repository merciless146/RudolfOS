/* ===== Rudolf OS 1.6.0: smart cards and the DoD Common Access Card (docs/plan-1.6.0.md Part 20) =====
   Settings › Security Settings › Smart cards: the readers and the card in each, with the names and expiry dates of its
   certificates (read without the PIN); the DoD's certificates for Oracle (an administrator: downloaded by rudolfd from
   the DoD PKI's library, then checked here, before anything is installed: the bundle's roots must be the DoD Root CAs
   whose thumbprints the DoD publishes (DISA's STIGs: DoD Root CA 3, 4, 5 and 6), and every other certificate must be
   signed, link by link, by one of them; whatever does not is left out); "Lock the screen when my card is taken out".
   Signing in to a website with the card is Chromium's own (its certificate picker, then its PIN box), through OpenSC's
   module in the desktop's certificate database (/usr/lib/rudolf/smartcard nss); never in Tor windows (their own HOME).
   With the Lite desktop, Oracle's pages are WebKit's, which do not use the card: a site that asks for one is opened in
   Chromium (Oracle's ☰ › Open in Chromium). The DoD's certificates are the computer's (Oracle's Chromium has one
   certificate database for every account), so an administrator adds or removes them. ===== */
/* SHA-1 thumbprints of the DoD's roots, as DISA's STIGs list them (Windows 11 WN11-PK-000005; Windows Server 2019 V-205648) */
const SC_DOD_ROOTS={'d73ca91102a2204a36459ed32213b467d7ce97fb':'DoD Root CA 3','b8269f25dbd937ecafd4c35a9838571723f2d026':'DoD Root CA 4',
 '4ecb5cc3095670454da1cbd410fc921f46b8564b':'DoD Root CA 5','d37ecf61c0b4ed88681ef3630c4e2fc787b37aef':'DoD Root CA 6'};
/* ---- DER, just enough for certificates and a PKCS#7 bundle ---- */
function scDer(b,o){let t=b[o],l=b[o+1],h=2;if(l&0x80){const n=l&0x7f;l=0;for(let i=0;i<n;i++)l=l*256+b[o+2+i];h=2+n}return{t,o,h,s:o+h,e:o+h+l,l}}
function scKids(b,n){const k=[];let o=n.s;while(o<n.e){const c=scDer(b,o);k.push(c);o=c.e}return k}
function scOid(b,n){const v=b.subarray(n.s,n.e);const p=[Math.floor(v[0]/40),v[0]%40];let x=0;for(let i=1;i<v.length;i++){x=x*128+(v[i]&0x7f);if(!(v[i]&0x80)){p.push(x);x=0}}return p.join('.')}
function scTime(b,n){const s=new TextDecoder().decode(b.subarray(n.s,n.e));let y,r;if(n.t==0x17){y=+s.slice(0,2);y+=y<50?2000:1900;r=s.slice(2)}else{y=+s.slice(0,4);r=s.slice(4)}
 return Date.UTC(y,+r.slice(0,2)-1,+r.slice(2,4),+r.slice(4,6),+r.slice(6,8),+(r.slice(8,10)||0))}
function scCN(b,n){for(const rdn of scKids(b,n))for(const av of scKids(b,rdn)){const k=scKids(b,av);if(scOid(b,k[0])=='2.5.4.3')return new TextDecoder().decode(b.subarray(k[1].s,k[1].e))}return''}
function scCert(b){const c=scDer(b,0),[tbs,alg,sig]=scKids(b,c),t=scKids(b,tbs),i=t[0].t==0xa0?1:0,iss=t[i+2],val=scKids(b,t[i+3]),sub=t[i+4],spki=t[i+5];
 return{der:b,tbs:b.subarray(tbs.o,tbs.e),alg:scOid(b,scKids(b,alg)[0]),sig:b.subarray(sig.s+1,sig.e),iss:b.subarray(iss.o,iss.e),sub:b.subarray(sub.o,sub.e),
  spki:b.subarray(spki.o,spki.e),spkiAlg:scKids(b,scKids(b,spki)[0]).map(x=>x.t==6?scOid(b,x):''),cn:scCN(b,sub),icn:scCN(b,iss),from:scTime(b,val[0]),to:scTime(b,val[1])}}
/* the certificates of a PKCS#7 bundle (SignedData's [0] certificates) */
function scP7(b){const ci=scDer(b,0),k=scKids(b,ci);if(scOid(b,k[0])!='1.2.840.113549.1.7.2')throw new Error('not a PKCS#7 bundle');
 const sd=scDer(b,k[1].s),f=scKids(b,sd).find(x=>x.t==0xa0);if(!f)throw new Error('no certificates in the bundle');return scKids(b,f).map(x=>scCert(b.slice(x.o,x.e)))}
const scEq=(a,b)=>a.length==b.length&&a.every((x,i)=>x==b[i]);
const scHex=u=>[...new Uint8Array(u)].map(x=>x.toString(16).padStart(2,'0')).join('');
const scUnhex=h=>{const u=new Uint8Array(h.length/2);for(let i=0;i<u.length;i++)u[i]=parseInt(h.substr(i*2,2),16);return u};
/* does `issuer`'s key verify `c`'s signature */
async function scSigned(c,issuer){const H={'1.2.840.113549.1.1.5':'SHA-1','1.2.840.113549.1.1.11':'SHA-256','1.2.840.113549.1.1.12':'SHA-384','1.2.840.113549.1.1.13':'SHA-512',
  '1.2.840.10045.4.3.2':'SHA-256','1.2.840.10045.4.3.3':'SHA-384','1.2.840.10045.4.3.4':'SHA-512'}[c.alg];if(!H)return false;
 try{if(/^1\.2\.840\.113549/.test(c.alg)){const k=await crypto.subtle.importKey('spki',issuer.spki,{name:'RSASSA-PKCS1-v1_5',hash:H},false,['verify']);return await crypto.subtle.verify('RSASSA-PKCS1-v1_5',k,c.sig,c.tbs)}
  const crv={'1.2.840.10045.3.1.7':'P-256','1.3.132.0.34':'P-384','1.3.132.0.35':'P-521'}[issuer.spkiAlg[1]];if(!crv)return false;const n={'P-256':32,'P-384':48,'P-521':66}[crv];
  const k=await crypto.subtle.importKey('spki',issuer.spki,{name:'ECDSA',namedCurve:crv},false,['verify']);const s=scDer(c.sig,0),[r,q]=scKids(c.sig,s);
  const fix=x=>{let v=c.sig.subarray(x.s,x.e);while(v.length>n&&v[0]==0)v=v.subarray(1);const o=new Uint8Array(n);o.set(v,n-v.length);return o};const raw=new Uint8Array(2*n);raw.set(fix(r));raw.set(fix(q),n);
  return await crypto.subtle.verify({name:'ECDSA',hash:H},k,raw,c.tbs)}catch(e){return false}}
/* the bundle checked: {roots:[cert], cas:[cert], left:[{cn,why}]} */
async function scDodCheck(bytes,pins){pins=pins||SC_DOD_ROOTS;const all=scP7(bytes),roots=[],cas=[],left=[];
 for(const c of all){c.sha1=scHex(await crypto.subtle.digest('SHA-1',c.der));c.self=scEq(c.iss,c.sub)}
 for(const c of all)if(c.self&&pins[c.sha1]&&await scSigned(c,c))roots.push(c);
 const ok=new Set(roots);let grew=true;
 while(grew){grew=false;for(const c of all){if(ok.has(c)||c.self)continue;for(const p of ok){if(scEq(c.iss,p.sub)&&await scSigned(c,p)){ok.add(c);cas.push(c);grew=true;break}}}}
 for(const c of all)if(!ok.has(c))left.push({cn:c.cn,why:c.self?'a root that is not one of the DoD\'s':'not signed by the DoD\'s roots'});
 return{roots,cas,left}}
/* ---- Settings › Security Settings › Smart cards ---- */
function scSection(c){if(typeof RD=='undefined'||!RD)return;
 const box=document.createElement('div');box.className='sc60';c.appendChild(box);
 box.innerHTML=`<h4 class='sh4'>Smart cards</h4><div class='msg scr'>Looking for card readers…</div>
 <div class='msg'>Sign in to websites in Oracle with a smart card, such as a DoD Common Access Card (CAC) or a PIV card: when a site asks, Oracle shows the card's certificates, then asks for the card's PIN. Rudolf OS never sees the PIN. A card locks itself after 3 wrong PINs, and only its issuer can unlock it. Not used in Tor windows. With the Lite desktop, open the site in Chromium (Oracle's ☰ menu).</div>
 <div class='r2'><label>DoD certificates</label><div><span class='scd'>…</span><div class='rmrow'><button class='scdi pri'>Install the DoD's certificates</button><button class='scdr'>Remove them</button></div>
 <div class='msg'>Sites such as iPERMS and IPPS-A use the DoD's own certificates, which Oracle does not trust by itself. They are downloaded from the DoD's PKI site and installed only if they come from the DoD's root certificates; trusted for websites only, for every account on this computer. The DoD does not support Linux, so a site may still refuse or behave differently.</div></div>
 <label>Card taken out</label><div><label class='ck'><input type='checkbox' class='scl'> Lock the screen when my card is taken out</label></div></div>`;
 const q=s=>box.querySelector(s),adm=isAdmin();q('.scdi').disabled=!adm;q('.scdr').disabled=!adm;q('.scl').checked=!!S.sclock;
 q('.scl').onchange=e=>{S.sclock=e.target.checked;persistS();scLockWatch()};
 const dodDraw=async()=>{try{const r=await rd('sc/dod/list');const n=(r.certs||[]).length;q('.scd').textContent=n?`Installed (${n} certificates).`:'Not installed.';q('.scdr').style.display=n?'':'none'}catch(e){q('.scd').textContent='Not available.'}};
 const draw=async()=>{let s;try{s=await rd('sc/status')}catch(e){q('.scr').textContent='The smart card service is not available.';return}
  const rs=s.readers||[];if(!rs.length){q('.scr').textContent=s.pcscd?'No card reader is plugged in.':'No card reader is plugged in. Plug one in to use a smart card.';return}
  q('.scr').textContent='';for(const r of rs){const d=document.createElement('div');d.className='msg';
   const certs=(r.certs||[]).map(x=>{try{return scCert(Uint8Array.from(atob(x.der),ch=>ch.charCodeAt(0)))}catch(e){return null}}).filter(Boolean);
   const now=Date.now(),soon=certs.filter(x=>x.to-now<30*864e5);
   d.textContent=`💳 ${r.name}: `+(r.card?`${r.label||'a card'}${r.maker?' ('+r.maker+')':''}. `+(certs.length?certs.map(x=>`${x.cn||'certificate'}, until ${new Date(x.to).toLocaleDateString()}`).join('; '):'No certificates could be read.'):'no card in it.');
   if(soon.length){const w=document.createElement('b');w.style.color='#e8a33d';const dd=Math.ceil((Math.min(...soon.map(x=>x.to))-now)/864e5);w.textContent=dd>0?` Expires in ${dd} day${dd==1?'':'s'}.`:' Expired.';d.appendChild(w)}
   q('.scr').appendChild(d)}};
 q('.scdi').onclick=async()=>{const cr=await elevate('Install the DoD\'s certificates for Oracle',{cred:true});if(!cr)return;q('.scd').textContent='Downloading the DoD\'s certificates…';
  try{const f=await rd('sc/dod/fetch',{method:'POST',body:JSON.stringify({n:cr.n,p:cr.p})});q('.scd').textContent='Checking them…';
   const k=await scDodCheck(scUnhex(f.hex));if(!k.roots.length)throw new Error('The bundle does not hold the DoD\'s root certificates, so nothing was installed.');
   const body=k.roots.map(x=>'root '+scHex(x.der)).concat(k.cas.map(x=>'ca '+scHex(x.der))).join('\n');
   const r=await rd('sc/dod/install',{method:'POST',body:JSON.stringify({n:cr.n,p:cr.p,certs:body})});
   notify('DoD certificates',`${r.installed} installed (${k.roots.map(x=>x.cn).join(', ')} and ${k.cas.length} below them)`+(k.left.length?`; ${k.left.length} left out, not from the DoD's roots.`:'.')+' Restart Oracle to use them.',{app:'Settings'})}
  catch(e){notify('DoD certificates',e.message||String(e),{kind:'error',app:'Settings'})}dodDraw()};
 q('.scdr').onclick=async()=>{const cr=await elevate('Remove the DoD\'s certificates from Oracle',{cred:true});if(!cr)return;
  try{await rd('sc/dod/remove',{method:'POST',body:JSON.stringify({n:cr.n,p:cr.p})});notify('DoD certificates','Removed.',{app:'Settings'})}catch(e){notify('DoD certificates',e.message,{kind:'error',app:'Settings'})}dodDraw()};
 draw();dodDraw();const iv=setInterval(()=>{if(!box.isConnected){clearInterval(iv);return}draw()},4000)}
if(typeof secPanelS=='function'){secPanelS=__after(secPanelS,function(c,W){try{scSection(c,W)}catch(e){}},"sc60.js");}
/* Lock the screen when my card is taken out: a look every 3 seconds while it is on and someone is signed in */
let SC_LW=0,SC_HAD=false;
function scLockWatch(){clearInterval(SC_LW);SC_LW=0;SC_HAD=false;if(typeof RD=='undefined'||!RD||!S.sclock||!USER||USER.r=='guest')return;
 SC_LW=setInterval(async()=>{if(!USER||!S.sclock){clearInterval(SC_LW);return}let s;try{s=await rd('sc/status')}catch(e){return}
  const has=(s.readers||[]).some(r=>r.card);if(SC_HAD&&!has){SC_HAD=false;try{lockScreen()}catch(e){}}else SC_HAD=has},3000)}
(function(){const o=window.__afterLogin;window.__afterLogin=()=>{o&&o();try{scLockWatch()}catch(e){}}})();
