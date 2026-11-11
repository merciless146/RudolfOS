/* ===== Rudolf OS 1.5.8 (plan Part 7): Android apps.
   7.1 Opening an .apk says what it is (the app's name, package, version, the Android it needs and the permissions it
       asks for: read from the file's AndroidManifest.xml, never run) and offers what Rudolf OS can already do: its web
       version, the same app for Linux in the App Store, or the phone's own copy on this screen (7.2). Android apps on
       the computer itself (Waydroid) come in 1.5.9, for computers with room for it.
   7.2 Phone screen: an Android phone with USB debugging on shows its screen in a window here, used with the mouse and
       keyboard (scrcpy and adb from Linux (Debian), a few MB). The apps run on the phone; nothing on the phone is
       changed by Rudolf OS. ===== */

/* ---------- reading the file: a range of bytes, from the desktop's storage or the disk ---------- */
async function apkSource(path){const v=node(path);
 if(typeof v=='string'&&typeof isDlRef=='function'&&isDlRef(v)){const i=dlRefInfo(v);const url=await dlLink(v);let size=i&&i.s||0;
  const get=async(off,len)=>{const r=await fetch(url,{headers:{Range:`bytes=${off}-${off+len-1}`}});if(!r.ok)throw new Error('could not read the file (HTTP '+r.status+')');
   const b=new Uint8Array(await r.arrayBuffer());return r.status==206?b:b.subarray(off,off+len)};
  if(!size){const r=await fetch(url,{method:'HEAD'});size=+r.headers.get('content-length')||0}
  return{size,get}}
 const b=v instanceof Uint8Array?v:v instanceof ArrayBuffer?new Uint8Array(v):typeof v=='string'?(typeof hdBytes=='function'?hdBytes(v):null):null;
 if(!b)throw new Error('this file cannot be read');return{size:b.length,get:async(off,len)=>b.subarray(off,off+len)}}
const apkU16=(b,o)=>b[o]|b[o+1]<<8,apkU32=(b,o)=>(b[o]|b[o+1]<<8|b[o+2]<<16|b[o+3]<<24)>>>0;
/* the ZIP's central directory, then one entry's bytes (stored or deflated) */
async function apkEntry(src,want){const tail=Math.min(src.size,65557),t=await src.get(src.size-tail,tail);let e=-1;
 for(let i=t.length-22;i>=0;i--)if(apkU32(t,i)==0x06054b50){e=i;break}
 if(e<0)throw new Error('this is not an Android app (no ZIP directory)');
 const cdSize=apkU32(t,e+12),cdOff=apkU32(t,e+16),cd=await src.get(cdOff,cdSize);const dec=new TextDecoder();
 for(let p=0;p+46<=cd.length&&apkU32(cd,p)==0x02014b50;){const meth=apkU16(cd,p+10),csz=apkU32(cd,p+20),nl=apkU16(cd,p+28),xl=apkU16(cd,p+30),cl=apkU16(cd,p+32),lo=apkU32(cd,p+42);
  const name=dec.decode(cd.subarray(p+46,p+46+nl));
  if(name==want){const lh=await src.get(lo,30);const off=lo+30+apkU16(lh,26)+apkU16(lh,28);const raw=await src.get(off,csz);
   if(meth==0)return raw;if(meth!=8)throw new Error('unknown compression');
   const ds=new Blob([raw]).stream().pipeThrough(new DecompressionStream('deflate-raw'));return new Uint8Array(await new Response(ds).arrayBuffer())}
  p+=46+nl+xl+cl}
 return null}
/* Android's binary XML (AXML): the string pool, then the elements and their attributes */
function apkAxml(b){const out=[];if(apkU16(b,0)!=3)throw new Error('the manifest is not binary XML');let strs=[],o=apkU16(b,2);
 while(o+8<=b.length){const type=apkU16(b,o),hs=apkU16(b,o+2),sz=apkU32(b,o+4);if(!sz)break;
  if(type==1){const n=apkU32(b,o+8),fl=apkU32(b,o+16),so=apkU32(b,o+20),u8=!!(fl&256);
   for(let i=0;i<n;i++){let p=o+so+apkU32(b,o+hs+i*4);
    if(u8){let l=b[p++];if(l&128)p++;let bl=b[p++];if(bl&128)bl=((bl&127)<<8)|b[p++];strs.push(new TextDecoder().decode(b.subarray(p,p+bl)))}
    else{let l=apkU16(b,p);p+=2;if(l&0x8000){l=((l&0x7fff)<<16)|apkU16(b,p);p+=2}let s='';for(let k=0;k<l;k++)s+=String.fromCharCode(apkU16(b,p+k*2));strs.push(s)}}}
  else if(type==0x102){const name=strs[apkU32(b,o+20)],ac=apkU16(b,o+28),as=apkU16(b,o+26),ast=apkU16(b,o+24);const at={};
   for(let i=0;i<ac;i++){const a=o+16+ast+i*as;const an=strs[apkU32(b,a+4)],raw=apkU32(b,a+8),dt=b[a+15],d=apkU32(b,a+16);
    at[an]=raw!=0xffffffff?strs[raw]:dt==3?strs[d]:dt==1?{ref:d}:dt==16||dt==17?d:dt==18?!!d:d}
   out.push({name,at})}
  o+=sz}
 return out}
async function apkInfo(path){const src=await apkSource(path);const m=await apkEntry(src,'AndroidManifest.xml');if(!m)throw new Error('this is not an Android app (no AndroidManifest.xml)');
 const els=apkAxml(m),man=els.find(e=>e.name=='manifest')||{at:{}},app=els.find(e=>e.name=='application')||{at:{}},sdk=els.find(e=>e.name=='uses-sdk')||{at:{}};
 const pkg=String(man.at.package||'');const lab=typeof app.at.label=='string'?app.at.label:'';
 const guess=pkg.split('.').filter(x=>!/^(com|org|net|app|android|io|de|uk|co|www|apps?|mobile)$/i.test(x)).pop()||pkg;
 return{pkg,name:lab||guess.replace(/[_-]+/g,' ').replace(/^./,c=>c.toUpperCase()),labelFromFile:!!lab,version:String(man.at.versionName||man.at.versionCode||''),
  minSdk:typeof sdk.at.minSdkVersion=='number'?sdk.at.minSdkVersion:+sdk.at.minSdkVersion||0,perms:[...new Set(els.filter(e=>e.name=='uses-permission'||e.name=='uses-permission-sdk-23').map(e=>String(e.at.name||'')).filter(Boolean))]}}
const APK_ANDROID={21:'5.0',22:'5.1',23:'6',24:'7',25:'7.1',26:'8',27:'8.1',28:'9',29:'10',30:'11',31:'12',32:'12L',33:'13',34:'14',35:'15',36:'16'};
const APK_PERMS={INTERNET:'the internet',CAMERA:'the camera',RECORD_AUDIO:'the microphone',ACCESS_FINE_LOCATION:'your precise location',ACCESS_COARSE_LOCATION:'your approximate location',
 READ_CONTACTS:'your contacts',WRITE_CONTACTS:'change your contacts',READ_SMS:'your text messages',SEND_SMS:'send text messages',RECEIVE_SMS:'receive text messages',CALL_PHONE:'make phone calls',READ_PHONE_STATE:'the phone\'s identity and calls',
 READ_EXTERNAL_STORAGE:'your files',WRITE_EXTERNAL_STORAGE:'change your files',READ_MEDIA_IMAGES:'your photos',READ_MEDIA_VIDEO:'your videos',READ_MEDIA_AUDIO:'your music',POST_NOTIFICATIONS:'notifications',
 BLUETOOTH_CONNECT:'Bluetooth devices',READ_CALENDAR:'your calendar',WRITE_CALENDAR:'change your calendar',GET_ACCOUNTS:'the accounts on the phone',BODY_SENSORS:'body sensors',SYSTEM_ALERT_WINDOW:'drawing over other apps',REQUEST_INSTALL_PACKAGES:'installing other apps'};
function apkPermText(ps){const known=[],other=[];for(const p of ps){const k=p.replace(/^android\.permission\./,'');if(APK_PERMS[k])known.push(APK_PERMS[k]);else other.push(p)}
 return{known:[...new Set(known)],other}}

/* ---------- the wizard ---------- */
async function wzApk(path){const W=wzWin(path,'🤖');wzSub(W,'An Android app');wzBody(W,'<p class="wzwait">Reading the app…</p>');
 let a=null,err='';try{a=await apkInfo(path)}catch(e){err=e.message}
 if(!W.el.isConnected)return W;
 if(!a){wzBody(W,`<p>This file could not be read as an Android app: ${wzEsc(err)}.</p>`);wzFoot(W,[['Close',()=>W.close()]]);return W}
 W.body.querySelector('.wzt').textContent=a.name;const pt=apkPermText(a.perms),and=APK_ANDROID[a.minSdk]||(a.minSdk?'API '+a.minSdk:'');
 wzBody(W,`<table class='wztab'><tr><td>App</td><td>${wzEsc(a.name)}${a.version?' '+wzEsc(a.version):''}</td></tr><tr><td>Package</td><td><code>${wzEsc(a.pkg)}</code></td></tr>${and?`<tr><td>Needs</td><td>Android ${wzEsc(and)} or later</td></tr>`:''}
  <tr><td>Asks for</td><td>${pt.known.length?wzEsc(pt.known.join(', ')):'nothing special'}${pt.other.length?` <span class='msg'>(and ${pt.other.length} more)</span>`:''}</td></tr></table>
  <p>Android apps do not run on this computer yet (Android apps on the computer itself come in Rudolf OS 1.5.9, for computers with room for it). Nothing was run or installed. What you can do now:</p>
  <ul class='apk58'><li><b>Use it on your phone, on this screen.</b> If the app is on your Android phone, Phone screen shows the phone here, used with this mouse and keyboard.</li>
  <li><b>Its web version.</b> Many apps are also websites: its store page names the maker's website.</li>
  <li><b>The same app for Linux.</b> The App Store may have it.</li></ul>`);
 const fb=[['Phone screen',()=>{W.close();phoneScreen()},1],['Web version',()=>{W.close();browser('https://play.google.com/store/apps/details?id='+encodeURIComponent(a.pkg))}]];
 if(RD&&typeof appStore=='function')fb.push(['App Store',()=>{W.close();const S=appStore();setTimeout(()=>{const i=S.body&&S.body.querySelector("input[type=search],input.lxq,input");if(i){i.value=a.name;i.dispatchEvent(new Event('input',{bubbles:true}))}},600)}]);
 fb.push(['Close',()=>W.close()]);wzFoot(W,fb);return W}
(function(){WZ_KINDS.APK='apk';const o=installWizard;installWizard=function(path){if(WZ_KINDS[ext(path.at(-1))]=='apk')return wzApk(path);return o(path)}})();

/* ---------- Phone screen (scrcpy) ---------- */
function phoneScreen(){const W=win('Phone screen','📱',560,460);W.body.style.padding='14px';W.body.classList.add('phs58');
 const draw=async()=>{if(!RD){W.body.innerHTML=`<p>Phone screen works on an installed Rudolf OS.</p>`;return}
  W.body.innerHTML='<p class="wzwait">Checking…</p>';const st=await debStatus();if(!W.el.isConnected)return;
  if(!st||!st.installed){W.body.innerHTML=`<p>Phone screen shows an Android phone's screen here, and you use its apps with this mouse and keyboard. It runs in Linux (Debian 13), which is not set up yet: setting it up downloads about 120 MB.</p><div class='rmrow'><button class='pri ps-l'>Set up Linux…</button></div>`;
   W.body.querySelector('.ps-l').onclick=()=>{W.close();terminalApp('linux')};return}
  if(!st.scrcpy){W.body.innerHTML=`<p>Phone screen uses scrcpy and adb from Debian (a few MB).</p><div class='rmrow'><button class='pri ps-i'>Get it</button></div><div class='msg ps-m'></div>`;
   W.body.querySelector('.ps-i').onclick=async e=>{if(!isAdmin()&&!await elevate('Install Phone screen (scrcpy) in Linux'))return;e.target.disabled=true;const m=W.body.querySelector('.ps-m');
    try{await lxApi('debian/scrcpy')}catch(er){e.target.disabled=false;return m.textContent=er.message}m.textContent='Downloading and installing…';await lxJobWait();draw()};return}
  W.body.innerHTML=`<ol class='ps-how'><li>On the phone, turn on <b>USB debugging</b>: Settings › About phone, tap <i>Build number</i> 7 times; then Settings › System › Developer options › USB debugging. (Rudolf OS never changes this for you.)</li>
   <li>Plug the phone into this computer with a USB cable, and unlock it.</li><li>Press <b>Show the phone</b>. The phone asks once whether to allow this computer: choose <i>Allow</i>.</li></ol>
   <p class='msg'>The apps run on the phone; its sound plays here with Android 11 or later. Close the window to stop. Only USB debugging is used: never the phone's service or download modes.</p>
   <div class='rmrow'><button class='pri ps-go'>Show the phone</button></div>`;
  W.body.querySelector('.ps-go').onclick=async()=>{try{await lxApi('debian/launch?id=scrcpy');notify('📱 Phone screen','Starting… if nothing shows, unlock the phone and allow this computer.',{app:'Phone screen',toast:6000})}catch(er){notify('Phone screen',er.message,{kind:'error'})}}};
 draw();return W}
regApp('phonescreen',{name:'Phone screen',icon:'📱',kw:'android phone scrcpy mirror apk adb',open:()=>phoneScreen(),re:/^Phone screen/});
