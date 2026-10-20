/* ===================== Linux apps over Wi-Fi (1.2.5) =====================
   Apps from Flathub install over any connection. Before a large download the App Store says how much it is:
   over Wi-Fi it asks first when the download is over 500 MB, and on a network marked Metered (Settings › Network ›
   Saved networks) for anything over 50 MB. rudolfd's apps/size and native/size (appsize.sh) give the sizes and
   the connection; rudolfd tries a stopped download again by itself, and Get continues where it stopped. */
const APP_METERED_K='bd95metered',APP_WIFI_ASK=500e6,APP_METERED_ASK=50e6;
const appMetered=()=>{const l=ld(APP_METERED_K,[]);return Array.isArray(l)?l:[]};
function appSetMetered(ssid,on){const l=appMetered().filter(x=>x!==ssid);if(on)l.push(ssid);svj(APP_METERED_K,l)}
const appGB=b=>b>=1e9?(b/1e9).toFixed(1)+' GB':Math.max(1,Math.round(b/1e6))+' MB';
/* what to say before a download, or null to go ahead without asking (pure, for the tests) */
function appDownloadNote(s,ssid,metered,free){if(!s||!s.ok)return null;const total=(s.download||0)+(s.runtimeDownload||0);
 if(s.link=='none')return{stop:true,text:'This computer is not connected to the internet. Connect to Wi-Fi or plug in a network cable (Settings › Network), then try again.'};
 const space=free&&s.installed?((s.installed+(s.runtimeDownload?s.runtimeDownload*2:0))>free?` That is more than the ${appGB(free)} free on this drive.`:` ${appGB(free)} is free on this drive.`):'';
 if(metered&&total>APP_METERED_ASK)return{text:`About ${appGB(total)} to download on ${ssid}, which is marked metered.${space} Download it now?`};
 if(s.link=='wifi'&&total>APP_WIFI_ASK)return{text:`About ${appGB(total)} to download over Wi-Fi${ssid?' ('+ssid+')':''}${s.runtimeDownload?', including the '+appGB(s.runtimeDownload)+' runtime it shares with other apps':''}.${space} It can take a while; if the connection drops, the download continues when it comes back. Download it now?`};
 return null}
function appAskDlg(title,text,goLabel,stop){return new Promise(res=>{const o=document.createElement('div');o.className='natdlg';
 o.innerHTML=`<div class='natbox' role='dialog' aria-modal='true' aria-labelledby='apdt'><h3 id='apdt'></h3><p class='apdp'></p><div class='stbtn' style='justify-content:flex-end'><button class='c'>${stop?'OK':'Not now'}</button>${stop?'':`<button class='pri g'>${goLabel}</button>`}</div></div>`;
 o.querySelector('h3').textContent=title;o.querySelector('.apdp').textContent=text;
 const done=v=>{o.remove();res(v)};o.querySelector('.c').onclick=()=>done(false);const g=o.querySelector('.g');if(g)g.onclick=()=>done(true);
 o.onkeydown=e=>{if(e.key=='Escape')done(false)};document.body.appendChild(o);(g||o.querySelector('.c')).focus()})}
/* true to go on with the install */
async function appDownloadOk(name,sizeCall){if(!RD)return true;let s;try{s=await sizeCall()}catch(e){return true}   // sizes unknown: the install itself reports problems
 let ssid='';if(s&&s.link=='wifi'){try{ssid=wfStatus((await rd('wifi/status')).raw).ssid||''}catch(e){}}
 let free=0;try{free=(await lxApi('apps/status')).free||0}catch(e){}
 const n=appDownloadNote(s,ssid,!!ssid&&appMetered().includes(ssid),free);if(!n)return true;
 return appAskDlg(n.stop?'No internet connection':'Download '+name+'?',n.text,'Download',n.stop)}
{const _fc=flatConsent;flatConsent=async(a,info)=>await _fc(a,info)&&await appDownloadOk(a.name,()=>lxApi('apps/size?id='+encodeURIComponent(a.id)))}
{const _nc=nativeConsent;nativeConsent=async a=>await _nc(a)&&await appDownloadOk(a.name,()=>rdPost('native/size?id='+a.id))}
