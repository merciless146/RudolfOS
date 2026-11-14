/* ===== Rudolf OS 1.5.9: the taskbar's buttons get a style and colours of their own (asked for on 2026-11-13)
   Display Properties › Taskbar › Buttons: the taskbar's styles for the buttons and objects on it (the start button,
   pinned apps, open windows, workspaces, the tray), apart from the bar itself:
   - Style: Standard (as now: they follow the taskbar's style), Wireframe, Monochrome, Candy or Retro;
   - "Match the theme colours": the buttons take the accent colour as Normal / Negative / Complementary (accent56.js's
     ACMODES and acVariant); unticked, each style keeps its own colours (Standard: the usual dark grey buttons).
   The open window's button and the current workspace keep their accent highlight, so they still stand out. ===== */
Object.assign(DEFS.disp,{tbbstyle:'standard',tbbac:false,tbbacm:'normal'});
['tbbstyle','tbbac','tbbacm'].forEach(k=>{if(!DKEYS.includes(k))DKEYS.push(k);if(S[k]===undefined)S[k]=DEFS.disp[k];if(SC&&SC[k]===undefined)SC[k]=cln(S[k])});
/* each style's own button colour, when the theme colours are not used */
const TBB_BASE={standard:'#1f2937',wire:'#111827',mono:'#52525b',candy:'#db2777',retro:'#c0c0c0'};
function tbbApply(){const H=document.documentElement,st=TB_STYLES.some(x=>x[0]==S.tbbstyle)?S.tbbstyle:'standard',m=ACMODES.some(x=>x[0]==S.tbbacm)?S.tbbacm:'normal';
 TB_STYLES.forEach(([k])=>H.classList.toggle('tbb-'+k,k==st&&k!='standard'));
 const on=st!='standard'||!!S.tbbac;H.classList.toggle('tbbon',on);if(!on){H.classList.remove('tbb-light');return}
 let base=S.tbbac?acVariant(S.accent||'#2563eb',m):TBB_BASE[st];
 if(st=='mono'){const[,,l]=icRgb2hsl(...icHex2rgb(base));base=icHsl2hex(0,0,l)}
 const ink=acInk(base),[h,sa,l]=icRgb2hsl(...icHex2rgb(base)),sh=d=>icHsl2hex(h,sa,Math.max(0,Math.min(100,l+d)));
 const P={'--tbbbg':base,'--tbbfg':ink,'--tbbhv':sh(l>50?-8:10),'--tbbhi':sh(Math.min(30,96-l)),'--tbblo':sh(-Math.min(30,l-6)),'--tbbsh':sh(-Math.min(50,l-2))};
 for(const k in P)H.style.setProperty(k,P[k]);H.classList.toggle('tbb-light',ink=='#000000'&&st!='wire')}
{/* (twice the class: above accent56's taskbar-style rules, which style the buttons with the bar) */
 const B='#bar button:not(.on),#bar #pins .pin,#bar #tray,#bar #vol,#bar #clock,#bar .tbclock';
 const each=(pre,suf='')=>B.split(',').map(x=>pre+' '+x+suf).join(',');
 const st=document.createElement('style');st.textContent=`
${each('html.tbbon.tbbon')}{background:var(--tbbbg)!important;color:var(--tbbfg)!important;background-image:none!important;filter:none}
${each('html.tbbon.tbbon',':hover')}{background:var(--tbbhv)!important}
html.tbbon.tbbon #bar #tray *,html.tbbon.tbbon #bar #start .stv{color:var(--tbbfg)!important}
html.tbbon.tbbon.tbb-standard #bar button{border-radius:5px}
/* the logo is white: dark on light buttons (Retro's silver, a light colour) */
html.tbbon.tbb-light #bar #start img{filter:brightness(.22)!important}
/* Wireframe: outlines only */
${each('html.tbbon.tbb-wire')}{background:transparent!important;box-shadow:none!important;border:1px solid color-mix(in srgb,var(--tbbbg) 70%,#fff)!important;border-radius:3px!important;color:inherit!important}
${each('html.tbbon.tbb-wire',':hover')}{background:color-mix(in srgb,var(--tbbbg) 25%,transparent)!important}
/* Monochrome: greys, the pictures in grey */
html.tbbon.tbb-mono #bar #pins img,html.tbbon.tbb-mono #bar #tasks img,html.tbbon.tbb-mono #bar #tray img,html.tbbon.tbb-mono #bar #tray svg{filter:grayscale(1)}
${each('html.tbbon.tbb-mono')}{border:0!important;border-radius:4px!important;box-shadow:none!important}
/* Candy: glossy round "pills" */
${each('html.tbbon.tbb-candy')}{border-radius:999px!important;border:0!important;background:linear-gradient(180deg,#ffffff8c 0%,#ffffff26 50%,#ffffff00 52%),var(--tbbbg)!important;box-shadow:inset 0 1px 0 #ffffffb3,0 1px 2px #0005!important}
${each('html.tbbon.tbb-candy',':hover')}{background:linear-gradient(180deg,#ffffffa6 0%,#ffffff33 50%,#ffffff00 52%),var(--tbbhv)!important}
/* Retro: square, raised edges */
${each('html.tbbon.tbb-retro')}{border-radius:0!important;border:0!important;box-shadow:inset -1px -1px 0 var(--tbbsh),inset 1px 1px 0 var(--tbbhi),inset -2px -2px 0 var(--tblo,var(--tbblo)),inset 2px 2px 0 #fff6!important}
${each('html.tbbon.tbb-retro',':active')}{box-shadow:inset 1px 1px 0 var(--tbbsh),inset -1px -1px 0 var(--tbbhi),inset 2px 2px 0 var(--tbblo)!important}
html.tbbon.tbb-retro #bar #tray{box-shadow:inset 1px 1px 0 var(--tbblo),inset -1px -1px 0 var(--tbbhi)!important}
.tbbrow{display:flex;flex-wrap:wrap;gap:6px 10px;align-items:center}.tbbrow select:disabled{opacity:.55}`;
 document.head.appendChild(st)}
(function(){const o=themeApply;themeApply=function(){const r=o.apply(this,arguments);try{tbbApply()}catch(e){}return r}})();
/* Display Properties › Taskbar: Buttons and Button colours, under the taskbar's Style */
(function(){const o=dispPanelS;dispPanelS=function(c,W){o(c,W);const tg=c.querySelector('.tbgrp');if(!tg||c.querySelector('#dtbbs'))return;
 const l1=document.createElement('label');l1.htmlFor='dtbbs';l1.textContent='Buttons';
 const s1=document.createElement('select');s1.id='dtbbs';s1.innerHTML=TB_STYLES.map(([k,n])=>optH(k,k=='standard'?'Standard (as the taskbar)':n,S.tbbstyle||'standard')).join('');
 const l2=document.createElement('label');l2.textContent='Button colours';
 const d2=document.createElement('div');d2.innerHTML=`<div class='tbbrow'>${ckH('dtbbac','Match the theme colours',!!S.tbbac)} <select id='dtbbacm' aria-label='Which colour'>${ACMODES.map(([k,n])=>optH(k,n,S.tbbacm||'normal')).join('')}</select></div>
  <div class='msg'>The start button, pinned apps, open windows, workspaces and the tray. Ticked: the accent colour (Normal, its Negative, or Complementary: the colour opposite it on the colour wheel); unticked: each style's own colours.</div>`;
 tg.append(l1,s1,l2,d2);
 const q=s=>d2.querySelector(s),sync=()=>{q('#dtbbacm').disabled=!q('#dtbbac').checked};
 s1.onchange=e=>liveSet(c,'tbbstyle',e.target.value);q('#dtbbac').onchange=e=>{liveSet(c,'tbbac',e.target.checked);sync()};q('#dtbbacm').onchange=e=>liveSet(c,'tbbacm',e.target.value);sync()}})();
tbbApply();
