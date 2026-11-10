/* ===== Rudolf OS 1.5.6: the accent colour on the taskbar and the desktop icons; 16 accent colours =====
   Settings › Display Properties › Colours and theme. Each place has its own tick box and its own choice of colour:
   - Normal: the accent colour itself;
   - Negative: its negative (each of red, green and blue turned over: 255 minus the value);
   - Complementary: the colour opposite it on the colour wheel (the hue turned by 180°, the same saturation and
     lightness).
   The taskbar takes the colour as its background, with white or black writing, whichever reads better. The desktop
   icons are Rudolf's own icons, drawn in shades of the colour (apps' own pictures, such as Flathub apps' icons, stay
   as they are; with Emoji icons there is nothing to colour). Off by default: nothing changes until ticked.
   And (asked for on 2026-11-10) the taskbar's style, as the icons have theirs (Display Properties › Taskbar › Style):
   - Standard: the usual dark bar (or the accent colour);
   - Wireframe: outlines only, on a see-through bar;
   - Monochrome: shades of grey, the pictures in grey too;
   - Candy: glossy, with rounded "pill" buttons;
   - Retro: the raised and sunken edges of 1990s desktops, on silver.
   The accent colour (when ticked for the taskbar) is the colour each style is made of; otherwise each has its own. */
Object.assign(DEFS.disp,{tbac:false,tbacm:'normal',icac:false,icacm:'normal',tbstyle:'standard'});
['tbac','tbacm','icac','icacm','tbstyle'].forEach(k=>{if(!DKEYS.includes(k))DKEYS.push(k);if(S[k]===undefined)S[k]=DEFS.disp[k];if(SC&&SC[k]===undefined)SC[k]=cln(S[k])});
/* the accent colours offered: the eight of before (blue, green, red, purple, orange, cyan, pink, slate) and eight
   more that fill the colour wheel's gaps (amber, yellow, lime, teal, sky, indigo, fuchsia) and a brown, in the order
   of the wheel, the two greys/browns last */
const ACCENTS=[['#dc2626','Red'],['#ea580c','Orange'],['#d97706','Amber'],['#ca8a04','Yellow'],['#65a30d','Lime'],['#16a34a','Green'],['#0d9488','Teal'],['#0891b2','Cyan'],
 ['#0284c7','Sky'],['#2563eb','Blue'],['#4f46e5','Indigo'],['#9333ea','Purple'],['#c026d3','Fuchsia'],['#db2777','Pink'],['#92400e','Brown'],['#475569','Slate']];
const ACMODES=[['normal','Normal'],['negative','Negative'],['comp','Complementary']];
function acVariant(hex,mode){const o=icHex2rgb(hex);if(!o)return hex;
 if(mode=='negative')return'#'+o.map(v=>(255-v).toString(16).padStart(2,'0')).join('');
 if(mode=='comp'){const[h,s,l]=icRgb2hsl(...o);return icHsl2hex(h+180,s,l)}
 return'#'+o.map(v=>v.toString(16).padStart(2,'0')).join('')}
/* white or black writing on a colour: whichever has the higher contrast (WCAG relative luminance) */
function acInk(hex){const o=icHex2rgb(hex)||[0,0,0];const L=o.map(v=>{v/=255;return v<=.03928?v/12.92:Math.pow((v+.055)/1.055,2.4)}),Y=.2126*L[0]+.7152*L[1]+.0722*L[2];
 return (1.05)/(Y+.05)>=(Y+.05)/.05?'#ffffff':'#000000'}
const TB_STYLES=[['standard','Standard'],['wire','Wireframe'],['mono','Monochrome'],['candy','Candy'],['retro','Retro']];
const TB_BASE={standard:'#111827',wire:'#111827',mono:'#3f3f46',candy:'#db2777',retro:'#c0c0c0'};
function acApply(){const H=document.documentElement,ac=S.accent||'#2563eb',st=TB_STYLES.some(x=>x[0]==S.tbstyle)?S.tbstyle:'standard';
 TB_STYLES.forEach(([k])=>H.classList.toggle('tbs-'+k,k==st&&k!='standard'));
 let base=S.tbac?acVariant(ac,S.tbacm):TB_BASE[st];
 if(st=='mono'){const[,,l]=icRgb2hsl(...icHex2rgb(base));base=icHsl2hex(0,0,l)}   /* grey of the colour's lightness */
 const on=!!S.tbac||st!='standard';H.classList.toggle('tbac',on);if(!on)H.classList.remove('tb-light');H.classList.toggle('tbac-col',!!S.tbac);
 if(on){const ink=acInk(base),[h,sa,l]=icRgb2hsl(...icHex2rgb(base)),sh=d=>icHsl2hex(h,sa,Math.max(0,Math.min(100,l+d)));
  H.style.setProperty('--tbbg',base);H.style.setProperty('--tbfg',ink);H.classList.toggle('tb-light',ink=='#000000');
  /* the buttons a little lighter or darker than the bar, as on the usual dark bar; light and dark edges for Retro
     and Candy */
  H.style.setProperty('--tbbt',sh(l>50?-10:10));H.style.setProperty('--tbbh',sh(l>50?-18:18));
  H.style.setProperty('--tbhi',sh(Math.min(30,96-l)));H.style.setProperty('--tblo',sh(-Math.min(30,l-6)));H.style.setProperty('--tbsh',sh(-Math.min(50,l-2)))}
 acIcons()}
function acIcons(){const col=S.icac&&S.icons!=='emoji'?acVariant(S.accent||'#2563eb',S.icacm):'';
 document.querySelectorAll('#desk > .ic > b img.ico').forEach(i=>{const e=i.dataset.emo||'',k=EMO2IC[e]||EMO2IC[e.replace(/️/g,'')];if(!k)return;
  const u=col?icSvg(k,col):icSvg(k);if(i.getAttribute('src')!==u)i.src=u})}
{const BX='#bar #tasks button,#bar #wsw button:not(.on),#bar #net,#bar #vol,#bar #tray',ALL='#bar button,#bar #tray,#bar #net,#bar #vol,#bar #tasks button,#bar #wsw button:not(.on)';
 const each=(sel,pre)=>sel.split(',').map(x=>pre+' '+x).join(',');
 const st=document.createElement('style');st.textContent=`html.tbac #bar{background:var(--tbbg)!important;color:var(--tbfg)!important}
${each(BX,'html.tbac')}{background:var(--tbbt)!important;color:var(--tbfg)!important}${each(BX.replace(/,/g,':hover,')+':hover','html.tbac')}{background:var(--tbbh)!important}
html.tbac #bar #tray *,html.tbac #bar #start,html.tbac #bar #start .stv,html.tbac #bar button{color:var(--tbfg)!important}
/* the logo is white: dark on a light bar (Retro's silver, a light accent) */
html.tb-light #bar #start img{filter:brightness(.22)}
/* Wireframe: outlines only, on a see-through bar */
html.tbs-wire #bar{background:color-mix(in srgb,var(--tbbg) 45%,transparent)!important;backdrop-filter:blur(8px);box-shadow:none!important;border-top:1px solid color-mix(in srgb,var(--tbfg) 70%,transparent)!important}
${each(ALL,'html.tbs-wire')}{background:transparent!important;box-shadow:none!important;border:1px solid color-mix(in srgb,var(--tbfg) 55%,transparent)!important;border-radius:3px!important}
${each(ALL.replace(/,/g,':hover,')+':hover','html.tbs-wire')}{background:color-mix(in srgb,var(--tbfg) 14%,transparent)!important}
html.tbs-wire #bar #wsw button.on{background:transparent!important;border:2px solid var(--ac)!important}
html.tbs-wire.tbac-col #bar button,html.tbs-wire.tbac-col #bar #tray{border-color:var(--tbhi)!important}
/* Monochrome: greys, and the pictures in grey */
html.tbs-mono #bar img,html.tbs-mono #bar svg{filter:grayscale(1)}html.tbs-mono #bar #wsw button.on{background:var(--tbhi)!important;color:var(--tbbg)!important}
/* Candy: a glossy bar, round "pill" buttons */
html.tbs-candy #bar{background:linear-gradient(180deg,var(--tbhi) 0%,var(--tbbg) 48%,var(--tblo) 100%)!important;box-shadow:inset 0 1px 0 #ffffff80,0 -1px 3px #0004!important}
${each(ALL,'html.tbs-candy')}{border-radius:999px!important;border:0!important}
${each(BX,'html.tbs-candy')}{background:linear-gradient(180deg,#ffffff8c 0%,#ffffff26 50%,#ffffff00 52%),var(--tbbt)!important;box-shadow:inset 0 1px 0 #ffffffb3,0 1px 2px #0005!important}
html.tbs-candy #bar #wsw button.on{background:linear-gradient(180deg,#ffffffa6 0%,#ffffff33 50%,#ffffff00 52%),var(--ac)!important;box-shadow:inset 0 1px 0 #fff,0 1px 2px #0005!important}
/* Retro: raised and sunken edges, square corners */
html.tbs-retro #bar{background:var(--tbbg)!important;box-shadow:inset 0 1px 0 var(--tbhi),inset 0 2px 0 #fff8!important;border:0!important}
${each(ALL,'html.tbs-retro')}{border-radius:0!important;border:0!important;background:var(--tbbg)!important;box-shadow:inset -1px -1px 0 var(--tbsh),inset 1px 1px 0 var(--tbhi),inset -2px -2px 0 var(--tblo),inset 2px 2px 0 #fff6!important}
${each(ALL.replace(/,/g,':active,')+':active','html.tbs-retro')},html.tbs-retro #bar #wsw button.on,html.tbs-retro #bar #tasks button.act,html.tbs-retro #bar #tasks button.on{box-shadow:inset 1px 1px 0 var(--tbsh),inset -1px -1px 0 var(--tbhi),inset 2px 2px 0 var(--tblo)!important}
html.tbs-retro #bar #tray{box-shadow:inset 1px 1px 0 var(--tblo),inset -1px -1px 0 var(--tbhi)!important}
html.tbs-retro #bar #wsw button.on{background:var(--tbbt)!important}
.acsw{display:flex;align-items:center;flex-wrap:nowrap;gap:4px;min-width:0}.acsw #dac{flex:0 0 auto;margin-right:2px}
.acsw .swb{flex:0 1 22px;width:auto!important;min-width:14px;height:auto!important;aspect-ratio:1;margin:0!important}
.acrow{display:flex;flex-wrap:wrap;gap:6px 10px;align-items:center}.acrow select:disabled{opacity:.55}`;document.head.appendChild(st)}
(function(){const o=themeApply;themeApply=function(){const r=o.apply(this,arguments);try{acApply()}catch(e){}return r}})();
(function(){const o=icRedraw;icRedraw=function(){const r=o.apply(this,arguments);try{acIcons()}catch(e){}return r}})();
(function(){const o=iconsApply;iconsApply=function(){const r=o.apply(this,arguments);try{acIcons()}catch(e){}return r}})();
/* icons added to the desktop later (an app put on the desktop, a file) */
{const D=document.getElementById('desk');if(D){let t=0;new MutationObserver(()=>{clearTimeout(t);t=setTimeout(()=>{if(S.icac)try{acIcons()}catch(e){}},60)}).observe(D,{childList:true,subtree:true})}}
/* Display Properties: 16 swatches instead of 8, and the two places */
(function(){const o=dispPanelS;dispPanelS=function(c,W){o(c,W);const sw=c.querySelector('#dac');if(!sw)return;const box=sw.parentElement;
 box.querySelectorAll('.swb').forEach(b=>b.remove());
 ACCENTS.forEach(([c2,n])=>{const b=document.createElement('button');b.className='swb';b.dataset.c=c2;b.style.background=c2;b.title=n;b.setAttribute('aria-label','Accent '+n);
  b.onclick=()=>{sw.value=c2;liveSet(c,'accent',c2)};box.appendChild(b)});
 /* one row: the swatches get only as much smaller as the row needs (on the Vivobook the 16th went to a second row) */
 box.classList.add('acsw');[...box.childNodes].forEach(n=>{if(n.nodeType==3&&!n.textContent.trim())n.remove()});
 const sel=(id,v)=>`<select id='${id}' aria-label='Which colour'>${ACMODES.map(([k,n])=>optH(k,n,v)).join('')}</select>`;
 const lab=document.createElement('label');lab.textContent='Accent colour on';const d=document.createElement('div');d.className='acplaces';
 d.innerHTML=`<div class='acrow'>${ckH('dtbac','The taskbar',!!S.tbac)} ${sel('dtbacm',S.tbacm||'normal')}</div>
  <div class='acrow'>${ckH('dicac','The desktop icons',!!S.icac)} ${sel('dicacm',S.icacm||'normal')}</div>
  <div class='msg acmsg'>Normal is the accent colour; Negative is its negative; Complementary is the colour opposite it on the colour wheel. The desktop icons are Rudolf's own icons (apps' own pictures stay as they are).</div>`;
 box.after(lab);lab.after(d);
 const q=s=>d.querySelector(s),sync=()=>{q('#dtbacm').disabled=!q('#dtbac').checked;q('#dicacm').disabled=!q('#dicac').checked;
  const m=q('.acmsg');if(m)m.dataset.emoji=S.icons==='emoji'?'1':'';if(q('#dicac').checked&&S.icons==='emoji')m.textContent='The desktop icons take the colour with Rudolf icons (Icons, below); Emoji keep their own colours.'};
 q('#dtbac').onchange=e=>{liveSet(c,'tbac',e.target.checked);sync()};q('#dtbacm').onchange=e=>liveSet(c,'tbacm',e.target.value);
 q('#dicac').onchange=e=>{liveSet(c,'icac',e.target.checked);sync()};q('#dicacm').onchange=e=>liveSet(c,'icacm',e.target.value);
 sync();
 /* the taskbar's style, in the Taskbar group (settings31) */
 const tg=c.querySelector('.tbgrp');if(tg){const l2=document.createElement('label');l2.htmlFor='dtbs';l2.textContent='Style';const s2=document.createElement('select');s2.id='dtbs';
  s2.innerHTML=TB_STYLES.map(([k,n])=>optH(k,n,S.tbstyle||'standard')).join('');s2.onchange=e=>liveSet(c,'tbstyle',e.target.value);tg.append(l2,s2)}}})();
acApply();
