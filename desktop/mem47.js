/* Rudolf OS 1.4.7: a web page closed to free memory.
   When memory runs out, the memory guard (rudolf-memguard) ends the largest web page instead of letting the whole
   desktop freeze. Oracle's tab then stops ('gone' from the shell, reason killed / oom / crashed); memGone() asks
   rudolfd (/api/memory) whether the guard ended a page in the last 30 s (or the kernel did: reason 'oom'), and if so
   shows Oracle's own page in the tab ("This page was closed because it used too much memory", with Reload) and a
   notification naming the page. Anything else keeps the plain "This page stopped working" message.
   memGone(t, why, o): t Oracle's tab; o = {T, sync, reload, st} from Oracle's window. */
const MEM_RELOAD=new Map();   // the message page's window -> reload
addEventListener('message',e=>{if(!e.data||e.data.__rudolfMemReload!==1)return;const f=MEM_RELOAD.get(e.source);if(f)f()});
function memNote(u,title){const h=mlEsc(webHost(u)||u),ti=mlEsc(title||webHost(u)||u);
 return`<!doctype html><meta charset='utf-8'><title>${ti}</title><body style='font:14px system-ui,sans-serif;margin:0;color:#1f2937;background:#f9fafb;display:flex;align-items:center;justify-content:center;min-height:100vh'>`+
 `<div style='max-width:520px;padding:32px;background:#fff;border:1px solid #e5e7eb;border-radius:12px;box-shadow:0 4px 16px rgba(0,0,0,.06)'>`+
 `<div style='font-size:40px;line-height:1'>🧠</div><h2 style='margin:12px 0 8px;font-size:20px'>This page was closed because it used too much memory</h2>`+
 `<p style='margin:0 0 6px;color:#374151'><b>${ti}</b> <span style='color:#6b7280'>(${h})</span> used so much of the computer's memory that the desktop was about to stop responding, so Rudolf OS closed it.</p>`+
 `<p style='margin:0 0 18px;color:#6b7280'>Closing other tabs and programs first gives it more room. Games and large pages may need more memory than this computer has.</p>`+
 `<button id='r' style='font:inherit;padding:8px 18px;border-radius:8px;border:0;background:#2563eb;color:#fff;cursor:pointer'>Reload</button>`+
 `</div><script>document.getElementById('r').onclick=()=>parent.postMessage({__rudolfMemReload:1},'*');addEventListener('keydown',e=>{if(e.key=='F5'||(e.ctrlKey&&e.key=='r')){e.preventDefault();parent.postMessage({__rudolfMemReload:1},'*')}})<\/script>`}
async function memWasGuard(why){
 if(why=='oom')return true;
 if(!/^(killed|crashed|abnormal)$/.test(why)||typeof RD=='undefined'||!RD)return false;
 try{const m=await rd('memory');return(m.events||[]).some(e=>m.now-e.time<=30)||!!m.critical}catch(e){return false}}
function memShow(t,o){
 const u=t.h[t.i],title=t.title&&t.title!==u?t.title:webHost(u)||u,fr=t.fr;
 t.real=false;t.pend=false;fr.removeAttribute('src');fr.setAttribute('sandbox','allow-scripts');t.src='';
 fr.srcdoc=typeof webDarkHtml=='function'?webDarkHtml(memNote(u,title)):memNote(u,title);
 const reload=()=>{for(const[w,f]of MEM_RELOAD)if(f===reload)MEM_RELOAD.delete(w);o.reload()};
 fr.onload=()=>{try{MEM_RELOAD.set(fr.contentWindow,reload)}catch(e){}};
 if(t===o.T()){o.sync();o.st.textContent='Closed to free memory. Reload (F5) to open it again.'}
 notify('Page closed to free memory',`${title} used too much memory, so Rudolf OS closed it to keep the desktop responding.`,{kind:'error'})}
function memGone(t,why,o){
 if(t.memChk)return;t.memChk=1;
 memWasGuard(why).then(yes=>{t.memChk=0;if(yes)memShow(t,o)})}
/* the memory guard must never end the desktop page itself, and Chromium does not say which process holds it: so the
   desktop says so, each time it loads. rudolfd notes every page process's processor time, the desktop keeps its
   processor busy for half a second, and the page process that used that time is the desktop's
   (/run/rudolf/desktop-renderer). Tried up to 3 times, when the desktop has settled. */
async function memIam(n){
 if(typeof RD=='undefined'||!RD)return;
 try{await rdPost('memory/iam?step=start');const t=performance.now();let x=0;while(performance.now()-t<600)x+=Math.sqrt(x+1);
  const r=await rdPost('memory/iam?step=end');window.__memIam=r;if(r&&r.ok)return}catch(e){}
 if((n||0)<2)setTimeout(()=>memIam((n||0)+1),20000)}
setTimeout(()=>memIam(0),15000);
