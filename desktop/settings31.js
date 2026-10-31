/* ===== 1.4.4 (settings31): the Settings layout. Controls move to the page where people look for them; saved values
   keep their keys, so nothing a person set is lost by a move.
   - Mouse and touchpad: primary button, double-click speed, scroll speed and single-click opening (from Accessibility);
     one pointer speed for the real pointer and for games that capture the mouse (S.msens follows S.ptr.speed)
   - System Settings › Date, time and language: Language (from Accessibility), Set the clock from the internet (from Network)
   - Oracle (was "Oracle sign-in"): Web search and Downloads (from Network Settings)
   - Accessibility: Vision, Hearing, Voice control; Pointer and cursor keeps the cursor and the pointer helpers
   - Display Properties: one Taskbar group ===== */
const setRow=(c,id)=>{const el=c.querySelector('#'+id);if(!el)return null;let r=el;while(r.parentElement&&!r.parentElement.classList.contains('r2'))r=r.parentElement;if(!r.parentElement)return null;const l=r.previousElementSibling;return[l&&l.tagName=='LABEL'?l:null,r]};
const setGroup=(c,title)=>[...c.querySelectorAll('h4')].find(h=>h.textContent.trim()==title);
function setDropGroup(h){if(!h)return;const n=h.nextElementSibling;if(n&&n.classList.contains('r2')&&!n.children.length)n.remove();else if(n&&n.classList.contains('r2'))return;h.remove()}
/* the language moves to System Settings: its Apply/Revert is System's now */
{const i=AKEYS.indexOf('lang');if(i>=0)AKEYS.splice(i,1);if(!SKEYS.includes('lang'))SKEYS.push('lang')}
const msensOf=sp=>Math.max(1,Math.min(10,Math.round(5+2.5*Math.log2(Math.max(0.1,+sp||1)))));
function ptrKeep(){if(typeof SC=='object'&&SC){if(S.ptr)SC.ptr=cln(S.ptr);SC.msens=S.msens}persistS()}
/* once: an Accessibility pointer speed set before 1.4.4 (and the Mouse page's left alone) becomes the pointer speed */
(function(){try{if(S.ptrmig)return;const p=S.ptr||{};if(+S.msens&&+S.msens!=5&&Math.abs((+p.speed||1)-1)<0.01){S.ptr=Object.assign({},PTR_DEF,p,{speed:Math.max(0.3,Math.min(3,+ptrF().toFixed(1)))});setTimeout(()=>{try{pointerApply()}catch(e){}},2000)}
 S.ptrmig=1;if(typeof SC=='object'&&SC)SC.ptrmig=1;ptrKeep()}catch(e){}})();
/* ---------- Accessibility ---------- */
(function(){const o=accPanelS;accPanelS=function(c,W){o(c,W);
 const lg=setGroup(c,'Language and region');if(lg){const n=lg.nextElementSibling;n&&n.classList.contains('r2')&&n.remove();lg.remove()}
 const vis=setGroup(c,'Vision'),vr=vis&&vis.nextElementSibling;
 if(vr){const cb=setRow(c,'acb');if(cb){const box=document.createElement('div');const sw=c.querySelector('.cbsw');cb[1].replaceWith(box);box.appendChild(cb[1]);if(sw)box.appendChild(sw);cb[0].textContent='Colour vision';vr.prepend(cb[0],box)}
  const tt=setRow(c,'att'),sr=setRow(c,'asr');if(tt&&sr)sr[1].after(tt[0],tt[1])}
 const hr=c.querySelector('.a11yhear'),vc=setRow(c,'avc');if(hr&&vc){vc[0].textContent='Visual sound alerts';hr.prepend(vc[0],vc[1])}
 ['Color vision','Hearing and speech'].forEach(t=>setDropGroup(setGroup(c,t)));
 const vk=setGroup(c,'Virtual keyboard'),ka=setGroup(c,'Keyboard assistance');if(vk&&ka){const n=vk.nextElementSibling;ka.before(vk);if(n)ka.before(n)}
 const ms=c.querySelector('.mouseset');if(ms){const h=ms.querySelector('h4');if(h)h.textContent='Pointer and cursor';
  ['mhand','msens','mscroll','mdbl','msingle'].forEach(id=>{const r=setRow(c,id);if(r){r[0]&&(r[0].hidden=true);r[1].hidden=true}});
  const m=[...ms.children].find(x=>x.classList.contains('msg')&&/Left-handed/.test(x.textContent));if(m)m.hidden=true;
  ms.insertAdjacentHTML('beforeend',`<div class='msg mvmoved' style='color:#6b7280'>Pointer speed, the primary button, double-click speed, scroll speed and single-click opening are in <a href='#' class='mvgo'>Mouse and touchpad</a>.</div>`);
  ms.querySelector('.mvgo').onclick=e=>{e.preventDefault();W.show('mouse')}}}})();
/* ---------- Mouse and touchpad ---------- */
(function(){const o=mousePanel;mousePanel=function(c,W){o(c,W);const q=s=>c.querySelector(s);
 const sync=()=>{const p=ptrGet();S.msens=msensOf(p.speed);ptrKeep()};
 ['#ptsp','#ptac','#pttap','#ptnat','#pttyp'].forEach(s=>{const e=q(s);e&&e.addEventListener(e.type=='range'?'input':'change',sync)});const df=q('.ptdef');df&&df.addEventListener('click',sync);
 const sp0=q('#ptsp');if(sp0)sp0.closest('span').insertAdjacentHTML('beforeend',`<div class='msg' style='margin:2px 0 0'>Also used by games that capture the mouse (Quake, 3D programs).</div>`);
 const opt=(v,t,cur)=>`<option value='${v}'${String(cur)==String(v)?' selected':''}>${t}</option>`,d=document.createElement('div');
 d.innerHTML=`<h4 style='margin:16px 0 6px'>Buttons and clicks</h4><div class='r2' style='display:grid;grid-template-columns:200px 1fr;gap:10px;align-items:center;max-width:560px'>
  <label for='mpb'>Primary button</label><select id='mpb'>${opt('right','Left button (right-handed)',S.mhand)}${opt('left','Right button (left-handed)',S.mhand)}</select>
  <label for='mpd'>Double-click speed</label><div><input type='range' id='mpd' min='200' max='900' step='50' value='${S.mdbl}' style='width:220px'> <output class='mpdv'></output><div class='dbltest' tabindex='0' title='Double-click to test'>📁 <span>Double-click this folder to test</span></div></div>
  <label for='mps'>Scroll speed</label><span><input type='range' id='mps' min='1' max='10' step='1' value='${S.mscroll}' style='width:220px'> <output class='mpsv'></output></span>
  <label>Opening items</label><label class='ck'><input type='checkbox' id='mpo'${S.msingle?' checked':''}> Open files and folders with a single click</label></div>
  <p class='msg'>Left-handed mode swaps the buttons on the desktop, windows, menus, lists and icons. Inside text boxes, sliders and games the browser keeps the physical buttons. Shift + right-click shows the browser's own menu.</p>`;
 const last=q('.ptdef');(last?last.closest('p'):null)?last.closest('p').before(d):c.appendChild(d);
 const lab=()=>{q('.mpdv').textContent=S.mdbl+' ms'+(S.mdbl==450?' (default)':'');q('.mpsv').textContent=S.mscroll==5?'Default':'×'+scrF().toFixed(2)};lab();
 const set=(k,v)=>{S[k]=v;if(typeof SC=='object'&&SC)SC[k]=cln(v);persistS();mouseApply();lab()};
 q('#mpb').onchange=e=>set('mhand',e.target.value);q('#mpd').oninput=e=>set('mdbl',+e.target.value);q('#mps').oninput=e=>set('mscroll',+e.target.value);q('#mpo').onchange=e=>set('msingle',e.target.checked);
 const dt=d.querySelector('.dbltest');dt.ondblclick=()=>{const op=dt.classList.toggle('open');dt.firstChild.textContent=op?'📂 ':'📁 ';dt.querySelector('span').textContent=op?'Opened! Double-click again to close':'Double-click this folder to test'};dt.onkeydown=e=>{if(e.key=='Enter')dt.ondblclick()};
 if(df)df.addEventListener('click',()=>{['mhand','mdbl','mscroll','msingle'].forEach(k=>{S[k]=cln(DEFS.acc[k]);if(typeof SC=='object'&&SC)SC[k]=cln(S[k])});persistS();mouseApply();q('#mpb').value=S.mhand;q('#mpd').value=S.mdbl;q('#mps').value=S.mscroll;q('#mpo').checked=S.msingle;lab()})}})();
/* ---------- System Settings › Date, time and language ---------- */
(function(){const o=sysPanelS;sysPanelS=function(c,W){o(c,W);const h=c.querySelector('h3');
 const g=document.createElement('h4');g.className='sh4 sgrp';g.textContent='Date, time and language';g.style.cssText='font-size:15px;color:var(--fg,inherit);margin-top:4px';
 const first=h&&h.nextElementSibling;if(first)first.before(g);
 const d=document.createElement('div');d.className='syslang';
 d.innerHTML=`<h4 class='sh4'>Language</h4><div class='r2'><label for='slg'>Language</label><select id='slg'>${LANGS.map(l=>`<option value='${l[0]}'${S.lang==l[0]?' selected':''}>${l[1]}</option>`).join('')}</select></div>`+(RD?`<h4 class='sh4'>Clock from the internet</h4><div class='r2'><label>Internet time</label><div><button class='nts'>Set the clock from the internet</button><div class='msg ntsm'></div></div></div>`:'');
 const foot=c.querySelector(':scope > .sfoot');if(foot)foot.before(d);else c.appendChild(d);
 d.querySelector('#slg').onchange=e=>{S.lang=e.target.value;a11yApply();W&&W.rt&&W.rt();c.dispatchEvent(new Event('input',{bubbles:true}))};
 const b=d.querySelector('.nts');if(b)b.onclick=()=>rdPost('time/sync').then(r=>{notify('Clock',r.message);d.querySelector('.ntsm').textContent=r.message;typeof clk=='function'&&clk()}).catch(e=>notify('Clock',e.message,{kind:'error'}))}})();
/* ---------- Network Settings: Web search and Downloads move to the Oracle page ---------- */
function netSearchNodes(c){const a=setGroup(c,'Web search (Oracle)'),out=[];if(!a)return out;let e=a;
 while(e&&!(e.classList&&(e.classList.contains('sfoot')))){if(e.id=='nsm'||(e.classList&&e.classList.contains('msg')&&/profiles that control/.test(e.textContent)))break;out.push(e);e=e.nextElementSibling;if(e&&e.tagName=='H4'&&!/^(Downloads)$/.test(e.textContent.trim()))break}
 return out}
(function(){const o=netPanelS;netPanelS=function(c,W){o(c,W);if(c.dataset.oracle)return;
 netSearchNodes(c).forEach(e=>e.hidden=true);
 if(RD){const f=c.querySelector(':scope > .sfoot');if(f)f.hidden=true}
 const n=document.createElement('div');n.className='msg nsmoved';n.style.color='#6b7280';n.innerHTML=`Web search and the download limits are on the <a href='#'>Oracle</a> page.`;n.querySelector('a').onclick=e=>{e.preventDefault();W.show('web')};
 const foot=c.querySelector(':scope > .sfoot');if(foot)foot.before(n);else c.appendChild(n)}})();
(function(){const o=signinPanel;signinPanel=function(c,W){const a=document.createElement('div'),b=document.createElement('div');a.className='orsi';b.className='orsearch';c.append(a,b);o(a,W);
 const fixT=()=>{const h=a.querySelector('h3');if(h&&h.textContent!='Oracle')h.textContent='Oracle'};fixT();new MutationObserver(fixT).observe(a,{childList:true});
 if(!W)return;b.dataset.oracle=1;if(typeof CUST=='object')delete CUST.net;netPanelS(b,W);b.querySelectorAll(':scope > .sp25').forEach(e=>e.remove());   /* the real network's sections (Wi-Fi scans) are not needed here */
 const keep=new Set(netSearchNodes(b));const ns=b.querySelector('#nsm'),f=b.querySelector(':scope > .sfoot');
 [...b.children].forEach(e=>{if(!keep.has(e)&&e!==ns&&e!==f)e.hidden=true});
 if(typeof CUST=='object'&&CUST.net)CUST.web=CUST.net}})();
/* ---------- Display Properties: one Taskbar group ---------- */
(function(){const o=dispPanelS;dispPanelS=function(c,W){o(c,W);const tb=setRow(c,'tb'),tg=setRow(c,'dtg');if(!tb||!tg)return;
 const d=document.createElement('div');d.innerHTML=`<h4 class='sh4'>Taskbar</h4><div class='r2 tbgrp'></div>`;const r=d.querySelector('.r2');tb[0].textContent='Position';tg[0].textContent='Windows';r.append(tb[0],tb[1],tg[0],tg[1]);
 const at=setGroup(c,'Colours and theme')||c.querySelector(':scope > .sfoot');if(at)at.before(...d.children);else c.append(...d.children)}})();
