/* 1.5.7 (plan Part 4, step 2): System Settings › Desktop engine. Standard (Chromium, as before) or Lite (WebKit, a
   lighter web engine: WebKitGTK, in Linux (Debian)): the desktop and its apps in WebKit, websites in Chromium's own
   window. On the Vivobook (2026-11-10, headless) WebKit held the desktop page in about a third of Chromium's memory.
   The choice applies when the computer next starts (rudolf-init takes it then). A test: if WebKit does not start,
   Chromium takes over for that start. Choosing WebKit installs it in Debian when needed. */
async function enginePanel57(c){if(!RD)return;const d=document.createElement('div');d.className='eng57';c.appendChild(d);
 const draw=async()=>{let e;try{e=await rd('desktop/engine')}catch(x){d.innerHTML='';return}const adm=isAdmin(),nm=v=>v=='webkit'?'Lite (WebKit)':'Standard (Chromium)';
  d.innerHTML=`<h4 class='sh4'>Desktop engine</h4><div class='r2'><label for='eng57'>Desktop engine</label><div><select id='eng57'><option value='chromium'>Standard (Chromium)</option><option value='webkit'>Lite (WebKit, test)</option></select>
<div class='msg engnow'></div></div></div><div class='msg'>Lite runs the desktop and its apps in WebKit, a lighter web engine: it takes much less memory, which leaves more for apps and games. Websites then open in Chromium's own window. It is a test in 1.5.7: some things may look or work differently. The engine changes when the computer restarts.</div><pre class='jl englog' hidden></pre>`;
  const q=s=>d.querySelector(s),sel=q('#eng57');sel.value=e.choice;sel.disabled=!adm||!e.persistent;
  [...sel.options].forEach(o=>{if(o.value=='webkit'&&!e.debian){o.disabled=true;o.textContent+=' — needs Linux (Debian) set up'}
   /* (WebKit is in Linux (Debian) in this test; in the encrypted folder that opens only at sign-in, after the desktop has started) */
   else if(o.value=='webkit'&&e.encrypted&&e.choice!='webkit'){o.disabled=true;o.textContent+=' — not with Linux in the encrypted folder'}});
  q('.engnow').textContent='In use now: '+nm(e.now)+(e.failed?' (Lite did not start this time, so Standard was used)':'')+'.'
   +(e.choice!=e.now?' '+nm(e.choice)+' is used after the computer restarts.':'')+(!adm?' Only administrators can change it.':'')+(e.choice=='webkit'&&!e.ready?' WebKit is not installed in Linux yet.':'');
  sel.onchange=async()=>{if(!await elevate('Change the desktop engine'))return draw();
   try{const j=await rdPost('desktop/engine?engine='+sel.value);
    if(j&&/installing/.test(j.message||'')){const pre=q('.englog');pre.hidden=false;pre.textContent='Installing WebKit in Linux (about 60 MB)…\n';
     if(typeof lxJobWait=='function'){const jj=await lxJobWait(l=>{pre.textContent+=l+'\n';pre.scrollTop=pre.scrollHeight});pre.textContent+=jj.status==0?'\nDone.':'\nThat did not work. Is this computer connected to the internet?'}}
    notify('Desktop engine',nm(sel.value)+' is used after the computer restarts.',{quiet:true})}
   catch(x){notify('Desktop engine',x.message,{kind:'error'})}draw()}};
 draw()}
{const _s=sysPanelS;sysPanelS=function(c,W){const r=_s.apply(this,arguments);try{enginePanel57(c)}catch(e){console.warn(e)}return r}}
