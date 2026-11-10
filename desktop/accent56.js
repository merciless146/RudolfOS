/* ===== Rudolf OS 1.5.6: the accent colour on the taskbar and the desktop icons; 16 accent colours =====
   Settings › Display Properties › Colours and theme. Each place has its own tick box and its own choice of colour:
   - Normal: the accent colour itself;
   - Negative: its negative (each of red, green and blue turned over: 255 minus the value);
   - Complementary: the colour opposite it on the colour wheel (the hue turned by 180°, the same saturation and
     lightness).
   The taskbar takes the colour as its background, with white or black writing, whichever reads better. The desktop
   icons are Rudolf's own icons, drawn in shades of the colour (apps' own pictures, such as Flathub apps' icons, stay
   as they are; with Emoji icons there is nothing to colour). Off by default: nothing changes until ticked. */
Object.assign(DEFS.disp,{tbac:false,tbacm:'normal',icac:false,icacm:'normal'});
['tbac','tbacm','icac','icacm'].forEach(k=>{if(!DKEYS.includes(k))DKEYS.push(k);if(S[k]===undefined)S[k]=DEFS.disp[k];if(SC&&SC[k]===undefined)SC[k]=cln(S[k])});
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
function acApply(){const H=document.documentElement,ac=S.accent||'#2563eb';
 const tb=S.tbac?acVariant(ac,S.tbacm):'';H.classList.toggle('tbac',!!tb);
 if(tb){const ink=acInk(tb),o=icHex2rgb(tb),[h,sa,l]=icRgb2hsl(...o);H.style.setProperty('--tbbg',tb);H.style.setProperty('--tbfg',ink);
  /* the window buttons a little lighter or darker than the bar, as on the usual dark bar */
  H.style.setProperty('--tbbt',icHsl2hex(h,sa,l>50?l-10:l+10));H.style.setProperty('--tbbh',icHsl2hex(h,sa,l>50?l-18:l+18))}
 acIcons()}
function acIcons(){const col=S.icac&&S.icons!=='emoji'?acVariant(S.accent||'#2563eb',S.icacm):'';
 document.querySelectorAll('#desk > .ic > b img.ico').forEach(i=>{const e=i.dataset.emo||'',k=EMO2IC[e]||EMO2IC[e.replace(/️/g,'')];if(!k)return;
  const u=col?icSvg(k,col):icSvg(k);if(i.getAttribute('src')!==u)i.src=u})}
{const st=document.createElement('style');st.textContent=`html.tbac #bar{background:var(--tbbg)!important;color:var(--tbfg)!important}
html.tbac #bar #tasks button{background:var(--tbbt)!important;color:var(--tbfg)!important}html.tbac #bar #tasks button:hover{background:var(--tbbh)!important}
html.tbac #bar #tray,html.tbac #bar #tray *,html.tbac #bar #osver,html.tbac #bar #vol{color:var(--tbfg)!important}
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
 sync()}})();
acApply();
