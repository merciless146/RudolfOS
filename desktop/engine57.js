/* 1.5.7 (plan Part 4, step 2): Settings › System health › Desktop engine. Chromium (the default) or WebKit, a lighter
   web engine (WebKitGTK, in Linux (Debian)): the desktop and its apps in WebKit, websites in Chromium's own window.
   On the Vivobook (2026-11-10, headless) WebKit held the desktop page in about a third of Chromium's memory. A test:
   if WebKit does not start, Chromium takes over for that start. Choosing WebKit installs it in Debian when needed. */
async function enginePanel57(c){if(!RD)return;const d=document.createElement('div');d.className='eng57';const at=c.querySelector(':scope > .hlev');if(at)at.before(d);else c.appendChild(d);
 const draw=async()=>{let e;try{e=await rd('desktop/engine')}catch(x){d.innerHTML='';return}const adm=isAdmin();
  d.innerHTML=`<h4 class='sh4 sgrp' style='font-size:15px;margin-top:18px'>Desktop engine (test)</h4>
<div class='msg'>The desktop and its apps can run in <b>WebKit</b>, a lighter web engine, instead of Chromium: it takes much less memory, which leaves more for apps and games. Websites then open in Chromium's own window. This is a test in 1.5.7: some things may look or work differently.</div>
<div class='r2'><label>Engine</label><div><label class='ck'><input type='radio' name='eng57' value='chromium'> Chromium (recommended)</label><label class='ck'><input type='radio' name='eng57' value='webkit'> WebKit (test)</label></div>
<label>Now</label><div class='engnow'></div></div><div class='rmrow' style='justify-content:flex-start;gap:6px'><button class='engap' hidden>Restart the desktop with it</button></div><pre class='jl englog' hidden></pre>`;
  const q=s=>d.querySelector(s);d.querySelectorAll('input[name=eng57]').forEach(r=>{r.checked=r.value==e.choice;r.disabled=!adm||!e.persistent||(r.value=='webkit'&&!e.debian)});
  q('.engnow').textContent=(e.now=='webkit'?'WebKit':'Chromium')+(e.failed?' (WebKit did not start this time, so Chromium was used)':'')+(!e.debian?' · WebKit needs Linux (Debian) set up: open Terminal and choose Linux.':e.choice=='webkit'&&!e.ready?' · WebKit is being installed in Linux.':'');
  q('.engap').hidden=e.choice==e.now;
  q('.engap').onclick=async()=>{if(!await elevate('Restart the desktop'))return;notify('Restarting the desktop','Your open windows close; save your work first.',{quiet:true});setTimeout(()=>rdPost('desktop/engine?apply=1').catch(()=>{}),1500)};
  d.querySelectorAll('input[name=eng57]').forEach(r=>r.onchange=async()=>{if(!await elevate('Change the desktop engine'))return draw();
   try{const j=await rdPost('desktop/engine?engine='+r.value);
    if(j&&/installing/.test(j.message||'')){const pre=q('.englog');pre.hidden=false;pre.textContent='Installing WebKit in Linux (about 60 MB)…\n';
     if(typeof lxJobWait=='function'){const jj=await lxJobWait(l=>{pre.textContent+=l+'\n';pre.scrollTop=pre.scrollHeight});pre.textContent+=jj.status==0?'\nDone.':'\nThat did not work. Is this computer connected to the internet?'}}}
   catch(x){notify('Desktop engine',x.message,{kind:'error'})}draw()})};
 draw()}
{const _h=healthPanel;healthPanel=function(c,W){const r=_h.apply(this,arguments);try{enginePanel57(c)}catch(e){console.warn(e)}return r}}
