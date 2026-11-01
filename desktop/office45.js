/* ===== 1.4.5 (office45): what SpreadsheetPro and PresentPro share with each other and with WordPro: the window with its
   menu bar, toolbar and status bar (WordPro's look), dialogs, a colour picker, Notey's help, autosave with recovery,
   the clipboard between the apps, charts drawn as SVG, and helpers for Office Open XML files (.xlsx, .pptx) through
   JSZip. Nothing from a file ever reaches eval, Function or innerHTML without being escaped. ===== */
const offEsc=s=>String(s??'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;').replace(/'/g,'&#39;');
const OFF_FONTS=['Calibri','Arial','Times New Roman','Georgia','Verdana','Tahoma','Trebuchet MS','Courier New','Comic Sans MS','Garamond','Impact','Segoe UI'];
const OFF_PALETTE=['#000000','#434343','#666666','#999999','#cccccc','#ffffff','#c00000','#ff0000','#ffc000','#ffff00','#92d050','#00b050','#00b0f0','#0070c0','#002060','#7030a0','#f4cccc','#fce5cd','#fff2cc','#d9ead3','#d0e0e3','#cfe2f3','#d9d2e9','#ead1dc'];
var OFFCLIP=null,OFF_LIVE=new Set();   /* what was last copied in an Office app: {kind:'range',rows,html} or {kind:'chart',spec,png} */
/* the window: menus is a function giving {File:[[label,shortcut,fn,checked]|'-',…],…} each time a menu opens */
function offShell(W,o){
 W.body.innerHTML=`<div class='wp off ${o.cls||''}'><div class='wp-mb'></div><div class='wp-tb'>${o.tb||''}</div><div class='off-main'>${o.main||''}</div><div class='wp-sb'>${o.sb||''}</div></div>`;
 const root=W.body.querySelector('.wp'),q=s=>root.querySelector(s),qa=s=>[...root.querySelectorAll(s)];let openM=null;
 const X={root,q,qa,esc:offEsc,W};
 function closeMenu(){if(openM){openM.remove();openM=null}}
 function popMenu(items,x,y,k){closeMenu();const d=document.createElement('div');d.className='wp-dd';if(k)d.dataset.k=k;
  items.forEach(it=>{if(!it)return;if(it=='-'){d.appendChild(document.createElement('hr'));return}const r=document.createElement('div');r.className='wp-di'+(it[4]?' off-dis':'');r.innerHTML=`<span class='ck'>${it[3]?'✓':''}</span><span class='lb'></span><span class='sk'>${offEsc(it[1]||'')}</span>`;r.querySelector('.lb').textContent=it[0];
   r.onmousedown=e=>e.preventDefault();r.onclick=()=>{if(it[4])return;closeMenu();try{it[2]()}catch(e){console.warn(e);X.info('Something went wrong',offEsc(e.message||e))}};d.appendChild(r)});
  const rr=root.getBoundingClientRect();d.style.left=Math.max(0,Math.min(x-rr.left,rr.width-250))+'px';d.style.top=(y-rr.top)+'px';root.appendChild(d);openM=d;
  if(d.offsetTop+d.offsetHeight>root.clientHeight)d.style.top=Math.max(0,root.clientHeight-d.offsetHeight-4)+'px';
  const cl=ev=>{if(!d.contains(ev.target)){closeMenu();document.removeEventListener('pointerdown',cl,true)}};setTimeout(()=>document.addEventListener('pointerdown',cl,true),0);return d}
 X.popMenu=popMenu;X.closeMenu=closeMenu;
 const mb=q('.wp-mb');(o.names||['File','Edit','View','Insert','Format','Data','Tools','Help']).forEach(k=>{const b=document.createElement('button');b.textContent=k;b.onmousedown=e=>e.preventDefault();
  b.onclick=e=>{e.stopPropagation();const was=openM&&openM.dataset.k==k;closeMenu();if(was)return;const bb=b.getBoundingClientRect();popMenu(o.menus()[k],bb.left,bb.bottom,k)};
  b.onmouseenter=()=>{if(openM&&openM.dataset.k&&openM.dataset.k!=k)b.click()};mb.appendChild(b)});
 /* dialogs: buttons [[label,fn,primary]]; fn returning false keeps the dialog open */
 X.dialog=(title,html,buttons,onopen)=>{const d=document.createElement('div');d.className='dlg wp-dlg';d.innerHTML=`<div><h3></h3><div class='wp-db'>${html}</div><div style='text-align:right;margin-top:12px' class='wp-dbt'></div></div>`;d.querySelector('h3').textContent=title;
  const close=()=>{d.remove();o.refocus&&o.refocus()};const bt=d.querySelector('.wp-dbt');
  (buttons||[['OK',null,1]]).forEach(([t,f,pri])=>{const b=document.createElement('button');b.textContent=t;b.style.marginLeft='6px';if(pri){b.style.background='var(--ac)';b.style.color='#fff';b.dataset.pri=1}b.onclick=()=>{if(f&&f(d)===false)return;close()};bt.appendChild(b)});
  d.addEventListener('keydown',e=>{e.stopPropagation();if(e.key=='Escape'){close()}else if(e.key=='Enter'&&e.target.tagName!='TEXTAREA'&&e.target.tagName!='BUTTON'){const p=bt.querySelector('[data-pri]');if(p){e.preventDefault();p.click()}}});
  root.appendChild(d);d.close=close;onopen&&onopen(d);const f=d.querySelector('input:not([type=checkbox]):not([type=radio]),select,textarea');(f||bt.lastChild).focus();return d};
 X.info=(t,m)=>X.dialog(t,`<div style='max-width:420px'>${m}</div>`);
 X.ask=(title,label,def)=>new Promise(res=>{X.dialog(title,`<label>${offEsc(label)}<br><input class='off-in' style='width:300px'></label>`,[['Cancel',()=>res(null)],['OK',d=>res(d.querySelector('.off-in').value),1]],d=>{d.querySelector('.off-in').value=def??''})});
 X.confirm=(title,msg,yes)=>new Promise(res=>{X.dialog(title,`<div style='max-width:400px'>${msg}</div>`,[['Cancel',()=>res(false)],[yes||'OK',()=>res(true),1]])});
 /* colour: the palette, plus "More colours" */
 X.color=(title,def,cb,noneLabel)=>X.dialog(title,`<div class='off-pal'>${OFF_PALETTE.map(c=>`<button data-c='${c}' title='${c}' style='background:${c}'></button>`).join('')}</div><label>More colours <input type='color' class='off-cc' value='${/^#[0-9a-f]{6}$/i.test(def||'')?def:'#000000'}'></label>${noneLabel?`<p><button class='off-none'>${offEsc(noneLabel)}</button></p>`:''}`,[['Cancel'],['OK',d=>cb(d.querySelector('.off-cc').value),1]],d=>{
  d.querySelectorAll('.off-pal button').forEach(b=>b.onclick=()=>{cb(b.dataset.c);d.close()});const n=d.querySelector('.off-none');if(n)n.onclick=()=>{cb('');d.close()}});
 return X}
/* Notey's help for the Office apps: a question box answered from a short list per app, and tips */
function offNotey(X,app,faq){if(typeof NOTEY_SVG!='string')return null;const box=document.createElement('div');box.className='wn';box.innerHTML=`<div class='wn-bub' style='display:none'></div><div class='wn-m' title='Notey — click for help'>${NOTEY_SVG}</div>`;X.root.appendChild(box);
 const bub=box.querySelector('.wn-bub');let on=true;try{on=!(JSON.parse(localStorage.getItem('bd95wp'))||{}).noteyOff}catch(e){}box.style.display=on?'':'none';
 const say=(html,btns)=>{bub.innerHTML=`<u class='wn-x' title='Close'>✕</u><div class='wn-t'>${html}</div><div class='wn-b'></div>`;bub.style.display='block';bub.querySelector('.wn-x').onclick=()=>{bub.style.display='none'};const bb=bub.querySelector('.wn-b');(btns||[]).forEach(([t,f])=>{const b=document.createElement('button');b.textContent=t;b.onclick=f;bb.appendChild(b)})};
 const ask=()=>{say(`What do you want to do in ${offEsc(app)}?<br><input class='wn-q' style='width:100%;margin-top:6px' placeholder='For example: make a chart'>`,[['Search',go]]);const i=bub.querySelector('.wn-q');i.focus();i.onkeydown=e=>{e.stopPropagation();if(e.key=='Enter')go()}};
 function go(){const t=(bub.querySelector('.wn-q')||{}).value||'';const w=t.toLowerCase().split(/\W+/).filter(x=>x.length>2);const hits=faq.map(f=>[f,w.filter(x=>f[0].includes(x)).length]).filter(x=>x[1]).sort((a,b)=>b[1]-a[1]).slice(0,3);
  say(hits.length?hits.map(([f])=>`<p><b>${offEsc(f[1])}</b><br>${f[2]}</p>`).join(''):`I don't know about that yet. Try the menus, or Help › Keyboard shortcuts.`,[['Ask again',ask]])}
 box.querySelector('.wn-m').onclick=()=>bub.style.display=='block'?bub.style.display='none':ask();
 return{ask:()=>{box.style.display='';ask()},show:v=>{box.style.display=v?'':'none'},visible:()=>box.style.display!='none',say}}
/* autosave: a recovery copy every minute while there are changes; offered again after a crash or power cut */
const offRecKey=()=>'offrec:'+(typeof uid=='function'?uid():'x');
function offAutosave(app,W,get){const id=app+':'+Date.now().toString(36)+Math.random().toString(36).slice(2,6);let last='';
 const tick=async()=>{try{const s=get();if(!s||!s.dirty){return}const data=s.data();if(data===last)return;last=data;const idx=(await kvGet(offRecKey()))||{};idx[id]={app,name:s.name,t:Date.now()};await kvSet(offRecKey()+':'+id,data);await kvSet(offRecKey(),idx)}catch(e){console.warn('autosave',e)}};
 OFF_LIVE.add(id);const iv=setInterval(tick,60000);const clear=async()=>{last='';try{const idx=(await kvGet(offRecKey()))||{};if(idx[id]){delete idx[id];await kvSet(offRecKey(),idx);await kvDel(offRecKey()+':'+id)}}catch(e){}};
 const oc=W.onclose;W.onclose=()=>{if(oc&&oc()===false)return false;clearInterval(iv);OFF_LIVE.delete(id);clear()};return{tick,clear,id}}
async function offRecover(app,X,open){try{const idx=(await kvGet(offRecKey()))||{};const mine=Object.entries(idx).filter(([k,v])=>v.app==app&&!OFF_LIVE.has(k));if(!mine.length)return;
 const[k,v]=mine.sort((a,b)=>b[1].t-a[1].t)[0];const data=await kvGet(offRecKey()+':'+k);const drop=async()=>{delete idx[k];await kvSet(offRecKey(),idx);await kvDel(offRecKey()+':'+k)};
 if(!data){await drop();return}
 X.dialog('Recover your work?',`<div style='max-width:380px'>${offEsc(app)} closed before <b>${offEsc(v.name||'Untitled')}</b> was saved (${offEsc(new Date(v.t).toLocaleString())}). Open the copy it kept?</div>`,[['Discard',()=>{drop()}],['Open it',()=>{open(data,v.name);drop()},1]])}catch(e){console.warn('recover',e)}}
/* charts as SVG: {type:'column'|'bar'|'line'|'area'|'pie'|'doughnut'|'scatter',title,cats,series:[{name,vals,color}],legend,xt,yt,w,h} */
const OFF_CHART_COLORS=['#4472c4','#ed7d31','#a5a5a5','#ffc000','#5b9bd5','#70ad47','#264478','#9e480e','#636363','#997300'];
function offChartSVG(c){const W=c.w||480,H=c.h||300,font="font-family='Calibri,Arial,sans-serif'";const ser=(c.series||[]).map((s,i)=>({...s,color:s.color||OFF_CHART_COLORS[i%10],vals:(s.vals||[]).map(v=>typeof v=='number'&&isFinite(v)?v:null)}));const cats=c.cats||[];
 let o=`<svg xmlns='http://www.w3.org/2000/svg' width='${W}' height='${H}' viewBox='0 0 ${W} ${H}' ${font} font-size='12'><rect width='100%' height='100%' fill='${c.bg||'#fff'}'/>`;
 const top=c.title?34:12,legW=c.legend!==false&&ser.length&&!/pie|doughnut/.test(c.type)?Math.min(140,W*.28):0,pieLeg=c.legend!==false&&/pie|doughnut/.test(c.type)?Math.min(140,W*.3):0;
 if(c.title)o+=`<text x='${W/2}' y='22' text-anchor='middle' font-size='16' font-weight='bold' fill='#333'>${offEsc(c.title)}</text>`;
 const fmt=v=>{const a=Math.abs(v);return a>=1e6?+(v/1e6).toFixed(1)+'M':a>=1e4?+(v/1e3).toFixed(1)+'k':+v.toFixed(a<1&&a?2:a<10?1:0)+''};
 if(/pie|doughnut/.test(c.type)){const s=ser[0]||{vals:[]},vals=s.vals.map(v=>Math.max(0,v||0)),tot=vals.reduce((a,b)=>a+b,0)||1;const cx=(W-pieLeg)/2,cy=top+(H-top-8)/2,R=Math.max(10,Math.min(cx-10,(H-top-16)/2)),r0=c.type=='doughnut'?R*.55:0;let a=-Math.PI/2;
  vals.forEach((v,i)=>{const a2=a+v/tot*Math.PI*2,col=(c.colors&&c.colors[i])||OFF_CHART_COLORS[i%10];const big=a2-a>Math.PI?1:0,p=(ang,r)=>`${(cx+r*Math.cos(ang)).toFixed(1)},${(cy+r*Math.sin(ang)).toFixed(1)}`;
   if(v>=tot-1e-9)o+=`<circle cx='${cx}' cy='${cy}' r='${R}' fill='${col}'/>`+(r0?`<circle cx='${cx}' cy='${cy}' r='${r0}' fill='#fff'/>`:'');
   else if(v>0)o+=`<path d='M${r0?p(a,r0):cx+','+cy} L${p(a,R)} A${R},${R} 0 ${big} 1 ${p(a2,R)} ${r0?`L${p(a2,r0)} A${r0},${r0} 0 ${big} 0 ${p(a,r0)}`:''}Z' fill='${col}' stroke='#fff'/>`;
   if(v>0&&v/tot>.04){const m=(a+a2)/2,rr=r0?(R+r0)/2:R*.65;o+=`<text x='${(cx+rr*Math.cos(m)).toFixed(1)}' y='${(cy+rr*Math.sin(m)+4).toFixed(1)}' text-anchor='middle' fill='#fff' font-size='11'>${Math.round(v/tot*100)}%</text>`}a=a2});
  if(pieLeg)cats.forEach((n,i)=>{const y=top+8+i*18;if(y>H-10)return;o+=`<rect x='${W-pieLeg+6}' y='${y}' width='10' height='10' fill='${(c.colors&&c.colors[i])||OFF_CHART_COLORS[i%10]}'/><text x='${W-pieLeg+20}' y='${y+9}' fill='#333'>${offEsc(String(n).slice(0,18))}</text>`});
  return o+'</svg>'}
 const L=48+(c.yt?16:0),R_=W-12-legW,T=top+4,B=H-26-(c.xt?16:0);const pw=Math.max(10,R_-L),ph=Math.max(10,B-T);
 let all=ser.flatMap(s=>s.vals).filter(v=>v!=null);let xs=null;if(c.type=='scatter'){xs=cats.map(Number);}
 let mn=Math.min(0,...all),mx=Math.max(0,...all);if(!all.length){mn=0;mx=1}if(mn==mx)mx=mn+1;const step=(()=>{const r=(mx-mn)/5,p=Math.pow(10,Math.floor(Math.log10(r))),f=r/p;return(f<1.5?1:f<3?2:f<7?5:10)*p})();mn=Math.floor(mn/step)*step;mx=Math.ceil(mx/step)*step;
 const horiz=c.type=='bar';const vpos=v=>horiz?L+(v-mn)/(mx-mn)*pw:B-(v-mn)/(mx-mn)*ph;
 for(let v=mn;v<=mx+step/2;v+=step){const p=vpos(v).toFixed(1);o+=horiz?`<line x1='${p}' x2='${p}' y1='${T}' y2='${B}' stroke='#e5e7eb'/><text x='${p}' y='${B+14}' text-anchor='middle' fill='#555' font-size='10'>${fmt(v)}</text>`:`<line x1='${L}' x2='${R_}' y1='${p}' y2='${p}' stroke='#e5e7eb'/><text x='${L-5}' y='${+p+3}' text-anchor='end' fill='#555' font-size='10'>${fmt(v)}</text>`}
 o+=`<line x1='${L}' x2='${horiz?L:R_}' y1='${B}' y2='${horiz?T:B}' stroke='#999'/>`;
 const n=Math.max(1,cats.length||Math.max(0,...ser.map(s=>s.vals.length)));
 if(c.type=='scatter'){let xmn=Math.min(...xs.filter(isFinite)),xmx=Math.max(...xs.filter(isFinite));if(!isFinite(xmn)){xmn=0;xmx=1}if(xmn==xmx)xmx=xmn+1;const xp=x=>L+(x-xmn)/(xmx-xmn)*pw;
  for(let i=0;i<=4;i++){const x=xmn+(xmx-xmn)*i/4;o+=`<text x='${xp(x).toFixed(1)}' y='${B+14}' text-anchor='middle' fill='#555' font-size='10'>${fmt(x)}</text>`}
  ser.forEach(s=>s.vals.forEach((v,i)=>{if(v!=null&&isFinite(xs[i]))o+=`<circle cx='${xp(xs[i]).toFixed(1)}' cy='${vpos(v).toFixed(1)}' r='3.5' fill='${s.color}'/>`}))}
 else{const band=(horiz?ph:pw)/n;
  cats.forEach((t,i)=>{if(n>40&&i%Math.ceil(n/20))return;const p=(horiz?T:L)+band*(i+.5);o+=horiz?`<text x='${L-5}' y='${(p+3).toFixed(1)}' text-anchor='end' fill='#555' font-size='10'>${offEsc(String(t).slice(0,12))}</text>`:`<text x='${p.toFixed(1)}' y='${B+14}' text-anchor='middle' fill='#555' font-size='10'>${offEsc(String(t).slice(0,Math.max(3,band/6|0)))}</text>`});
  if(c.type=='column'||c.type=='bar'){const bw=band*.7/Math.max(1,ser.length);ser.forEach((s,j)=>s.vals.forEach((v,i)=>{if(v==null)return;const a=vpos(Math.max(mn,Math.min(0,mx))),b=vpos(v),off=(horiz?T:L)+band*i+band*.15+bw*j;
   o+=horiz?`<rect x='${Math.min(a,b).toFixed(1)}' y='${off.toFixed(1)}' width='${Math.abs(b-a).toFixed(1)}' height='${(bw*.92).toFixed(1)}' fill='${s.color}'/>`:`<rect x='${off.toFixed(1)}' y='${Math.min(a,b).toFixed(1)}' width='${(bw*.92).toFixed(1)}' height='${Math.abs(b-a).toFixed(1)}' fill='${s.color}'/>`}))}
  else ser.forEach(s=>{const pts=s.vals.map((v,i)=>v==null?null:[L+band*(i+.5),vpos(v)]).filter(Boolean);if(!pts.length)return;const d=pts.map((p,i)=>(i?'L':'M')+p[0].toFixed(1)+','+p[1].toFixed(1)).join(' ');
   if(c.type=='area')o+=`<path d='${d} L${pts.at(-1)[0].toFixed(1)},${vpos(Math.max(mn,0)).toFixed(1)} L${pts[0][0].toFixed(1)},${vpos(Math.max(mn,0)).toFixed(1)}Z' fill='${s.color}' fill-opacity='.55' stroke='${s.color}'/>`;
   else o+=`<path d='${d}' fill='none' stroke='${s.color}' stroke-width='2.5'/>`+pts.map(p=>`<circle cx='${p[0].toFixed(1)}' cy='${p[1].toFixed(1)}' r='3' fill='${s.color}'/>`).join('')})}
 if(c.xt)o+=`<text x='${(L+R_)/2}' y='${H-6}' text-anchor='middle' fill='#333'>${offEsc(c.xt)}</text>`;
 if(c.yt)o+=`<text transform='translate(14,${(T+B)/2}) rotate(-90)' text-anchor='middle' fill='#333'>${offEsc(c.yt)}</text>`;
 if(legW)ser.forEach((s,i)=>{const y=T+8+i*18;if(y>H-10)return;o+=`<rect x='${W-legW+4}' y='${y}' width='10' height='10' fill='${s.color}'/><text x='${W-legW+18}' y='${y+9}' fill='#333'>${offEsc(String(s.name||'Series '+(i+1)).slice(0,18))}</text>`});
 return o+'</svg>'}
/* an SVG (or anything an <img> shows) as a PNG data URL */
function offSvgPng(svg,w,h,scale){return new Promise(res=>{const im=new Image();im.onload=()=>{const c=document.createElement('canvas');c.width=w*(scale||2);c.height=h*(scale||2);const g=c.getContext('2d');g.scale(scale||2,scale||2);g.drawImage(im,0,0,w,h);res(c.toDataURL('image/png'))};im.onerror=()=>res(null);im.src='data:image/svg+xml;charset=utf-8,'+encodeURIComponent(svg)})}
/* files: a VFS value (a data: URL or text) as bytes; bytes or a Blob as a data: URL */
function offBytes(v){if(typeof v!='string')return null;const m=/^data:[^,]*?(;base64)?,/.exec(v);if(!m)return new TextEncoder().encode(v);const b=v.slice(m[0].length);if(!m[1])return new TextEncoder().encode(decodeURIComponent(b));const s=atob(b),a=new Uint8Array(s.length);for(let i=0;i<s.length;i++)a[i]=s.charCodeAt(i);return a}
const offDataUrl=blob=>new Promise(res=>{const r=new FileReader();r.onload=()=>res(r.result);r.onerror=()=>res(null);r.readAsDataURL(blob)});
const offXml=s=>new DOMParser().parseFromString(s,'application/xml');
const offX=s=>String(s??'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;').replace(/[\u0000-\u0008\u000b\u000c\u000e-\u001f]/g,'');
/* elements by local name, whatever their namespace prefix */
const offEls=(n,name)=>n?[...n.getElementsByTagNameNS('*',name)]:[];
const offEl=(n,name)=>offEls(n,name)[0]||null;
const offKids=(n,name)=>n?[...n.children].filter(c=>c.localName==name):[];
/* a picture made no larger than needed (as the desktop background does): long side at most max px */
function offShrink(dataUrl,max){return new Promise(res=>{const im=new Image();im.onload=()=>{const k=Math.min(1,max/Math.max(im.naturalWidth,im.naturalHeight));if(k>=1&&dataUrl.length<400000)return res(dataUrl);
 const c=document.createElement('canvas');c.width=Math.max(1,Math.round(im.naturalWidth*k));c.height=Math.max(1,Math.round(im.naturalHeight*k));c.getContext('2d').drawImage(im,0,0,c.width,c.height);
 const png=/^data:image\/(png|gif|svg)/.test(dataUrl);res(c.toDataURL(png?'image/png':'image/jpeg',.88))};im.onerror=()=>res(dataUrl);im.src=dataUrl})}
/* the Start menu's Office group, the App Store's built-in list and Settings › Apps' default apps use this */
const OFFICE_APPS=[['wordpro','WordPro','📝','Documents (.DOC, .TXT)'],['sheet','SpreadsheetPro','📊','Spreadsheets (.RSHEET, .XLSX, .CSV)'],['deck','PresentPro','📽️','Presentations (.RDECK, .PPTX)']];
