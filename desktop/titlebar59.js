/* ===== Rudolf OS 1.5.9: the windows' title bars get a style and a colour, as the taskbar has (asked for on 2026-11-13)
   Settings › Display Properties › Title bars, next to the Taskbar group:
   - Style: the taskbar's styles (accent56.js's TB_STYLES) drawn on the title bars:
     Standard: the usual flat bar; Wireframe: outlines only, see-through; Monochrome: shades of grey, the icons in grey
     too; Candy: glossy, with round "pill" buttons; Retro: a 1990s gradient bar with raised square buttons.
   - Colour: the accent colour as Normal / Negative / Complementary (accent56.js's ACMODES and acVariant), as for the
     taskbar and the desktop icons. Normal with Standard is how title bars always looked.
   The active window's title bar has the colour; the others stay muted (Standard: grey as before; the other styles
   in grey). Writing is white or black, whichever reads better on the colour (acInk). ===== */
Object.assign(DEFS.disp,{twstyle:'standard',twacm:'normal'});
['twstyle','twacm'].forEach(k=>{if(!DKEYS.includes(k))DKEYS.push(k);if(S[k]===undefined)S[k]=DEFS.disp[k];if(SC&&SC[k]===undefined)SC[k]=cln(S[k])});
function twApply(){const H=document.documentElement,st=TB_STYLES.some(x=>x[0]==S.twstyle)?S.twstyle:'standard',m=ACMODES.some(x=>x[0]==S.twacm)?S.twacm:'normal';
 TB_STYLES.forEach(([k])=>H.classList.toggle('tws-'+k,k==st&&k!='standard'));
 const on=st!='standard'||m!='normal';H.classList.toggle('twcol',on);if(!on)return;
 let base=acVariant(S.accent||'#2563eb',m);if(st=='mono'){const[,,l]=icRgb2hsl(...icHex2rgb(base));base=icHsl2hex(0,0,l)}
 const ink=acInk(base),[h,sa,l]=icRgb2hsl(...icHex2rgb(base)),sh=d=>icHsl2hex(h,sa,Math.max(0,Math.min(100,l+d)));
 const P={'--twbg':base,'--twfg':ink,'--twbt':sh(l>50?-10:10),'--twhi':sh(Math.min(30,96-l)),'--twlo':sh(-Math.min(30,l-6)),'--twsh':sh(-Math.min(50,l-2))};
 for(const k in P)H.style.setProperty(k,P[k]);H.classList.toggle('tw-light',ink=='#000000')}
{const T='html.twcol .win.act>.tb',A='html.twcol .win>.tb',st=document.createElement('style');st.textContent=`${T}{background:var(--twbg)!important;color:var(--twfg)!important}
${T} button{color:var(--twfg)!important}
/* the other windows: muted (grey), whatever the style */
html.twcol:not(.tws-mono) .win:not(.act)>.tb{filter:grayscale(1)}html.tws-wire .win:not(.act)>.tb,html.tws-candy .win:not(.act)>.tb,html.tws-retro .win:not(.act)>.tb{opacity:.72}
/* Wireframe: outlines only, the window showing through */
html.twcol.tws-wire .win>.tb,html.twcol.tws-wire .win.act>.tb{background:color-mix(in srgb,var(--twbg) 14%,transparent)!important;color:inherit!important;box-shadow:inset 0 -2px 0 var(--twbg)!important}
html.twcol.tws-wire .win.act>.tb{box-shadow:inset 0 -2px 0 var(--twbg),inset 0 0 0 1px var(--twbg)!important}html.twcol.tws-wire .win>.tb button{color:inherit!important;background:transparent!important;border:1px solid color-mix(in srgb,currentColor 55%,transparent)!important;border-radius:3px!important}
html.tws-wire .win>.tb button:hover{background:color-mix(in srgb,var(--twbg) 22%,transparent)!important}html.tws-wire .win>.tb .cl:hover{background:#e11d48!important;color:#fff!important;border-color:#e11d48!important}
/* Monochrome: greys, and the window's icon in grey */
html.tws-mono .win>.tb img,html.tws-mono .win>.tb svg{filter:grayscale(1)}html.tws-mono .win:not(.act)>.tb{background:color-mix(in srgb,var(--twbg) 35%,#9ca3af)!important;color:#fff!important}
/* Candy: glossy, round "pill" buttons */
html.tws-candy .win.act>.tb{background:linear-gradient(180deg,var(--twhi) 0%,var(--twbg) 48%,var(--twlo) 100%)!important;box-shadow:inset 0 1px 0 #ffffff80!important}
html.tws-candy .win>.tb button{border-radius:999px!important;background:linear-gradient(180deg,#ffffff8c 0%,#ffffff26 50%,#ffffff00 52%),var(--twbt)!important;box-shadow:inset 0 1px 0 #ffffffb3,0 1px 2px #0005!important;margin:0 1px}
html.tws-candy .win>.tb .cl:hover{background:linear-gradient(180deg,#ffffff8c 0%,#ffffff26 50%,#ffffff00 52%),#e11d48!important}
/* Retro: a gradient bar, square corners, raised buttons on silver */
html.tws-retro .win{border-radius:0!important}html.tws-retro .win>.tb{border-radius:0!important;font-weight:700}
html.tws-retro .win.act>.tb{background:linear-gradient(90deg,var(--twlo),var(--twbg) 45%,var(--twhi))!important}
html.tws-retro .win>.tb button{border-radius:0!important;background:#c0c0c0!important;color:#000!important;box-shadow:inset -1px -1px 0 #404040,inset 1px 1px 0 #fff,inset -2px -2px 0 #808080,inset 2px 2px 0 #dfdfdf!important}
html.tws-retro .win>.tb button:active{box-shadow:inset 1px 1px 0 #404040,inset -1px -1px 0 #fff,inset 2px 2px 0 #808080!important}
html.tws-retro .win>.tb button:hover{background:#d4d4d4!important}html.tws-retro .win>.tb .cl:hover{background:#c0c0c0!important;color:#b91c1c!important}
html.tws-retro .win>.body{border-radius:0!important}
/* Display Properties' small preview of the title bar */
.twprev{display:flex;flex-direction:column;gap:4px;margin-top:6px;max-width:320px}.twprev .win{position:static!important;box-shadow:none!important;width:auto!important;height:auto!important;min-height:0!important;pointer-events:none}
.twprev .win>.tb{font-size:12px}.twprev .win>.body{height:6px;min-height:0}`;document.head.appendChild(st)}
(function(){const o=themeApply;themeApply=function(){const r=o.apply(this,arguments);try{twApply()}catch(e){}return r}})();
/* Display Properties › Title bars, after the Taskbar group */
(function(){const o=dispPanelS;dispPanelS=function(c,W){o(c,W);const tg=c.querySelector('.tbgrp');if(!tg||c.querySelector('.twgrp'))return;
 const d=document.createElement('div');
 d.innerHTML=`<h4 class='sh4'>Title bars</h4><div class='r2 twgrp'><label for='dtws'>Style</label><select id='dtws'>${TB_STYLES.map(([k,n])=>optH(k,n,S.twstyle||'standard')).join('')}</select>
  <label for='dtwm'>Colour</label><div><select id='dtwm' aria-label='Which colour'>${ACMODES.map(([k,n])=>optH(k,n,S.twacm||'normal')).join('')}</select>
  <div class='msg'>The title bar of the window you are using takes the accent colour: Normal is the accent colour; Negative is its negative; Complementary is the colour opposite it on the colour wheel. The styles are the taskbar's.</div>
  <div class='twprev' aria-hidden='true'><div class='win act'><div class='tb'><span>⚙️</span><span class='t'>Active window</span><button class='mn' tabindex='-1'>_</button><button class='mx' tabindex='-1'>□</button><button class='cl' tabindex='-1'>✕</button></div><div class='body'></div></div>
  <div class='win'><div class='tb'><span>📝</span><span class='t'>Another window</span><button class='mn' tabindex='-1'>_</button><button class='mx' tabindex='-1'>□</button><button class='cl' tabindex='-1'>✕</button></div><div class='body'></div></div></div></div></div>`;
 tg.after(...d.children);
 c.querySelector('#dtws').onchange=e=>liveSet(c,'twstyle',e.target.value);c.querySelector('#dtwm').onchange=e=>liveSet(c,'twacm',e.target.value)}})();
twApply();
