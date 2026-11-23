/* ===== Rudolf OS 1.6.2: Scan, the scanners' app (docs/plan-1.6.2.md Part 34, step 2) =====
   The scanners rudolfd's scan/find lists (rudolfd-scan.inc, rudolf-scan: eSCL and WSD on the network, eSCL over USB
   through ipp-usb), a preview of the whole glass, the area drawn on it, colour, grey or black and white, the resolution,
   the glass or the document feeder; pages added one scan after another (the feeder's all at once), put in order or taken
   out, then saved as one PDF, or each page as PNG or JPEG, in Documents or where the Save dialog says. Scanning runs as
   the account's own Linux user (rudolfd). An account whose files are on the disk gets the file written there by its own
   user (scan/save to=), the rest through C: as every app's files. Errors are said as the scanner tells them (no scanner,
   busy, a paper jam, the cover open, the feeder empty). Opened from the Start menu, File Explorer (Scan into this folder)
   and the Print dialog and Settings › Devices. Everything is drawn as text (a scanner's name is the network's). ===== */
window.__scan62=1;
regApp('scan',{name:'Scan',icon:'📠',kw:'scan scanner scanning document photo pdf feeder airscan escl wsd',open:()=>scanApp(),re:/^Scan$/});
const SCAN_MODES=[['color','Colour'],['grey','Grey'],['bw','Black and white']];
const SCAN_SIZES=[['whole','The whole glass'],['letter','Letter (8.5 × 11 in)',215.9,279.4],['a4','A4 (210 × 297 mm)',210,297],['a5','A5 (148 × 210 mm)',148,210],['photo','Photo (6 × 4 in)',152.4,101.6],['drawn','Drawn on the preview']];
function scanStamp(){const d=new Date(),p=n=>String(n).padStart(2,'0');return d.getFullYear()+'-'+p(d.getMonth()+1)+'-'+p(d.getDate())+' '+p(d.getHours())+'.'+p(d.getMinutes())}
async function scanBlob(path){const r=await rd(path);return r.blob?await r.blob():null}
function scanApp(o){o=o||{};
 const W=appWin('scan','Scan','📠',920,640);W.body.style.cssText='display:flex;flex-direction:column;background:var(--wbg,#fff);min-height:0';
 W.body.innerHTML=`<div class='scn' style='flex:1;display:flex;min-height:0;gap:8px;padding:8px'>
<div class='scs' style='width:250px;display:flex;flex-direction:column;gap:6px;overflow:auto'>
<label>Scanner</label><div style='display:flex;gap:4px'><select class='scdev' aria-label='Scanner' style='flex:1;min-width:0'></select><button class='scfind' title='Look for scanners again'>↻</button></div>
<label>Source</label><select class='scsrc' aria-label='Source'></select>
<label>Colour</label><select class='scmode' aria-label='Colour'>${SCAN_MODES.map(([k,l])=>`<option value='${k}'>${l}</option>`).join('')}</select>
<label>Resolution</label><select class='scres' aria-label='Resolution'></select>
<label>Area</label><select class='scarea' aria-label='Area'>${SCAN_SIZES.map(s=>`<option value='${s[0]}'>${s[1]}</option>`).join('')}</select>
<div style='display:flex;gap:4px;margin-top:6px'><button class='scprev'>👁 Preview</button><button class='scgo pri' style='flex:1'>📠 Scan</button></div>
<div class='msg scm' role='status' aria-live='polite' style='min-height:3em'></div>
</div>
<div class='scp' style='flex:1;min-width:0;display:flex;align-items:center;justify-content:center;background:var(--bg2,#e5e7eb);border-radius:4px;position:relative;overflow:hidden'>
<div class='scpw' style='position:relative;display:none'><img class='scpi' alt='Preview' draggable='false' style='display:block;user-select:none;touch-action:none;cursor:crosshair'><div class='scsel' style='position:absolute;border:2px dashed #2563eb;background:rgba(37,99,235,.12);display:none;pointer-events:none'></div></div>
<div class='scpe msg' style='text-align:center;padding:20px'>Press Preview to see what is on the scanner's glass, then draw the area to scan.</div></div></div>
<div class='scl' role='list' aria-label='Scanned pages' style='display:flex;gap:6px;overflow-x:auto;padding:6px 8px;border-top:1px solid var(--bd,#ccc);min-height:118px;align-items:flex-start'></div>
<div class='xb' style='padding:6px 8px;gap:6px'><span class='scn2 msg' style='flex:1'>No pages yet.</span><label>Save as <select class='scfmt' aria-label='Save as'><option value='pdf'>PDF (all pages in one)</option><option value='png'>PNG (a file for each page)</option><option value='jpeg'>JPEG (a file for each page)</option></select></label><button class='scclr'>Clear</button><button class='scsave pri' disabled>💾 Save…</button></div>`;
 const q=s=>W.body.querySelector(s),say=(t,bad)=>{const m=q('.scm');m.textContent=t||'';m.style.color=bad?'#b91c1c':'';m.setAttribute('role',bad?'alert':'status')};
 const sess=[...crypto.getRandomValues(new Uint8Array(6))].map(b=>b.toString(16).padStart(2,'0')).join('');
 const st={list:[],caps:null,pages:[],next:1,busy:false,prev:null,sel:null,closed:false};W.scan=st;W.scanSess=sess;
 if(!RD){say('Scanning needs Rudolf OS on this computer (its scanners are found and driven by the system).',true);['.scfind','.scprev','.scgo'].forEach(s=>q(s).disabled=true);return W}
 const src=()=>st.caps&&st.caps.sources.find(s=>s.source==q('.scsrc').value)||st.caps&&st.caps.sources[0]||{width:215.9,height:296.9,resolutions:[75,150,300]};
 function fillCaps(){const c=st.caps;const s=q('.scsrc');s.textContent='';
  (c?c.sources:[]).forEach(x=>{const op=document.createElement('option');op.value=x.source;op.textContent=x.source=='adf'?'Document feeder':'Glass';s.appendChild(op)});
  const m=q('.scmode');[...m.options].forEach(op=>op.disabled=!!c&&!c.modes.includes(op.value));fillRes()}
 function fillRes(){const r=q('.scres'),was=+r.value||+S.scanRes||300;r.textContent='';const rs=src().resolutions||[];
  rs.forEach(v=>{const op=document.createElement('option');op.value=v;op.textContent=v+' dpi'+(v==150?' (screen)':v==300?' (documents)':v>=600?' (photos)':'');r.appendChild(op)});
  if(rs.length){r.value=rs.reduce((b,v)=>Math.abs(v-was)<Math.abs(b-was)?v:b,rs[0])}}
 function setBusy(b,what){st.busy=b;['.scdev','.scfind','.scsrc','.scmode','.scres','.scarea','.scprev','.scsave','.scclr'].forEach(s=>q(s).disabled=b||(s=='.scsave'&&!st.pages.length));
  const g=q('.scgo');g.textContent=b&&what=='scan'?'✕ Cancel':'📠 Scan';g.disabled=b&&what!='scan';g.classList.toggle('pri',!b)}
 async function find(){if(st.busy)return;setBusy(true,'find');say('Looking for scanners on the network and on USB…');let j=null;
  try{j=await rd('scan/find')}catch(e){say(e.message,true)}setBusy(false);if(st.closed)return;
  st.list=j&&j.scanners||[];const d=q('.scdev'),was=d.value||S.scanLast;d.textContent='';
  st.list.forEach(x=>{const op=document.createElement('option');op.value=x.id;op.textContent=x.name+(x.via=='usb'||x.name.includes(x.proto)?'':' ('+x.proto+')');d.appendChild(op)});
  if(!st.list.length){st.caps=null;fillCaps();if(j)say('No scanner found. Is it switched on, and on the same network (Wi-Fi or a cable) or plugged in by USB? Printer-scanners made since about 2015 work without a driver.',true);['.scprev','.scgo'].forEach(s=>q(s).disabled=true);return}
  d.value=st.list.some(x=>x.id==was)?was:st.list[0].id;await pick()}
 async function pick(){st.caps=null;const id=q('.scdev').value;if(!id)return;say('Asking the scanner what it can do…');setBusy(true,'caps');
  try{st.caps=await rd('scan/caps?id='+encodeURIComponent(id));say('')}catch(e){say(e.message,true)}setBusy(false);if(st.closed)return;fillCaps();
  ['.scprev','.scgo'].forEach(s=>q(s).disabled=!st.caps);S.scanLast=id;try{persistS()}catch(e){}}
 q('.scfind').onclick=find;q('.scdev').onchange=pick;q('.scsrc').onchange=fillRes;q('.scres').onchange=()=>{S.scanRes=+q('.scres').value};
 /* the area: a paper size from the top-left corner, or drawn on the preview (in millimetres from that corner) */
 function area(){const k=q('.scarea').value,s=src();if(k=='drawn'&&st.sel)return st.sel;const z=SCAN_SIZES.find(x=>x[0]==k);
  if(!z||!z[2])return null;return{l:0,t:0,w:Math.min(z[2],s.width),h:Math.min(z[3],s.height)}}
 /* the preview fitted into its pane (its whole page seen), the area over it */
 function fit(){const img=q('.scpi'),p=q('.scp');if(!img.naturalWidth)return;const k=Math.min((p.clientWidth-16)/img.naturalWidth,(p.clientHeight-16)/img.naturalHeight);
  img.style.width=Math.max(20,Math.floor(img.naturalWidth*k))+'px';img.style.height=Math.max(20,Math.floor(img.naturalHeight*k))+'px'}
 function drawSel(){fit();const a=area(),w=q('.scpw'),b=q('.scsel'),img=q('.scpi');if(!a||!st.prev||w.style.display=='none'){b.style.display='none';return}
  const s=src(),fx=img.clientWidth/s.width,fy=img.clientHeight/s.height;Object.assign(b.style,{display:'block',left:a.l*fx+'px',top:a.t*fy+'px',width:a.w*fx+'px',height:a.h*fy+'px'})}
 q('.scarea').onchange=drawSel;new ResizeObserver(drawSel).observe(q('.scp'));
 {const img=q('.scpi');let d0=null;const mm=e=>{const r=img.getBoundingClientRect(),s=src();return[Math.max(0,Math.min(1,(e.clientX-r.left)/r.width))*s.width,Math.max(0,Math.min(1,(e.clientY-r.top)/r.height))*s.height]};
  img.addEventListener('pointerdown',e=>{if(st.busy||!st.prev)return;d0=mm(e);img.setPointerCapture(e.pointerId);e.preventDefault()});
  img.addEventListener('pointermove',e=>{if(!d0)return;const p=mm(e);st.sel={l:Math.min(d0[0],p[0]),t:Math.min(d0[1],p[1]),w:Math.abs(p[0]-d0[0]),h:Math.abs(p[1]-d0[1])};q('.scarea').value='drawn';drawSel()});
  img.addEventListener('pointerup',()=>{if(d0&&st.sel&&(st.sel.w<5||st.sel.h<5)){st.sel=null;q('.scarea').value='whole'}d0=null;drawSel()})}
 function opts(extra){const a=area(),p=new URLSearchParams({id:q('.scdev').value,session:sess,source:q('.scsrc').value||'flatbed',mode:q('.scmode').value,res:q('.scres').value||'300'});
  if(a)p.set('area',[a.l,a.t,a.w,a.h].map(v=>v.toFixed(1)).join(','));for(const k in extra||{})p.set(k,extra[k]);return p.toString()}
 async function preview(){if(st.busy)return;setBusy(true,'preview');say('Previewing…');
  try{const j=await rdPost('scan/start?'+opts({preview:1,source:'flatbed'}),null);const b=await scanBlob('scan/page?session='+sess+'&name=preview.png');if(st.closed)return;
   if(st.prev)URL.revokeObjectURL(st.prev);st.prev=URL.createObjectURL(b);const img=q('.scpi');img.onload=()=>{q('.scpw').style.display='';q('.scpe').style.display='none';drawSel()};img.src=st.prev;say('Draw on the preview to scan only part of it.')}
  catch(e){say(e.message,true)}setBusy(false)}
 async function scan(){if(st.busy){try{await rdPost('scan/cancel',null)}catch(e){}return}
  const adf=q('.scsrc').value=='adf';setBusy(true,'scan');say(adf?'Scanning the pages in the feeder…':'Scanning…');
  try{const j=await rdPost('scan/start?'+opts({first:st.next}),null);
   for(const n of j.pages||[]){const b=await scanBlob('scan/page?session='+sess+'&name='+encodeURIComponent(n));if(st.closed)return;
    const k=+(/page-(\d+)/.exec(n)||[0,st.next])[1];st.pages.push({k,url:URL.createObjectURL(b),w:j.width,h:j.height,dpi:j.dpi,mode:j.mode});st.next=Math.max(st.next,k+1)}
   drawPages();say(j.stopped?j.error+' ('+(j.pages||[]).length+' page'+((j.pages||[]).length==1?'':'s')+' kept.)':(j.pages||[]).length>1?(j.pages.length+' pages scanned.'):'Scanned. Put the next page on the glass and press Scan again, or save.',!!j.stopped)}
  catch(e){say(e.message,true)}setBusy(false)}
 q('.scprev').onclick=preview;q('.scgo').onclick=scan;
 function drawPages(){const L=q('.scl');L.textContent='';
  st.pages.forEach((p,i)=>{const c=document.createElement('div');c.setAttribute('role','listitem');c.style.cssText='display:flex;flex-direction:column;align-items:center;gap:2px;flex:none';
   const im=document.createElement('img');im.src=p.url;im.alt='Page '+(i+1);im.style.cssText='height:78px;max-width:90px;border:1px solid var(--bd,#999);background:#fff';c.appendChild(im);
   const r=document.createElement('div');r.style.cssText='display:flex;gap:2px';
   const mk=(t,l,f,dis)=>{const b=document.createElement('button');b.textContent=t;b.title=l;b.setAttribute('aria-label',l+' (page '+(i+1)+')');b.disabled=!!dis;b.style.cssText='padding:0 5px';b.onclick=f;r.appendChild(b)};
   mk('◀','Move earlier',()=>{st.pages.splice(i-1,0,st.pages.splice(i,1)[0]);drawPages()},i==0);
   const n=document.createElement('span');n.textContent=String(i+1);n.className='msg';r.appendChild(n);
   mk('▶','Move later',()=>{st.pages.splice(i+1,0,st.pages.splice(i,1)[0]);drawPages()},i==st.pages.length-1);
   mk('✕','Remove',()=>{URL.revokeObjectURL(p.url);st.pages.splice(i,1);drawPages()});
   c.appendChild(r);L.appendChild(c)});
  q('.scn2').textContent=st.pages.length?st.pages.length+' page'+(st.pages.length==1?'':'s')+' scanned.':'No pages yet.';q('.scsave').disabled=st.busy||!st.pages.length}
 q('.scclr').onclick=()=>{st.pages.forEach(p=>URL.revokeObjectURL(p.url));st.pages=[];drawPages();say('')};
 /* saving: the account's disk folder written by its own user, or C: (as every app's files) */
 async function save(){if(st.busy||!st.pages.length)return;const fmt=q('.scfmt').value,ext=fmt=='jpeg'?'jpg':fmt,base='Scan '+scanStamp();
  const start=Array.isArray(o.dir)?o.dir:(USER&&typeof homePath=='function'?homePath('DOCUMENTS'):['C:']);
  const types=fmt=='pdf'?[['PDF document',['PDF']]]:fmt=='png'?[['PNG picture',['PNG']]]:[['JPEG picture',['JPG','JPEG']]];
  const one=fmt=='pdf'||st.pages.length==1;let to=null;
  if(typeof fileDlg=='function'){to=await fileDlg({mode:'save',title:one?'Save the scan':'Save the pages (one file each: the name gets the page\'s number)',name:base+'.'+ext,types,start:fileDlgStart(start)});if(!to)return}
  else to=start.concat([base+'.'+ext]);
  const dir=to.slice(0,-1),nm=to[to.length-1],stem=nm.replace(/\.[^.]*$/,''),e2=(/\.[^.]*$/.exec(nm)||['.'+ext])[0];
  const jobs=one?[{name:nm,pages:st.pages.map(p=>p.k)}]:st.pages.map((p,i)=>({name:stem+' '+(i+1)+e2,pages:[p.k]}));
  const disk=typeof hdOn=='function'&&hdOn()&&typeof homePath=='function'&&homePath().every((x,i)=>dir[i]==x)&&dir.length>homePath().length;
  setBusy(true,'save');say('Saving…');let done=0;
  try{for(const jb of jobs){const d=node(dir);let name=jb.name;
    if(d&&d[name]!=null&&!one)name=typeof uniq=='function'?uniq(d,name):name;
    const qs='session='+sess+'&format='+fmt+'&pages='+jb.pages.join(',');
    if(disk&&!(d&&d[name]!=null)){const rel=hdRel(dir.slice(homePath().length).concat([name]));
     await rdPost('scan/save?'+qs+'&to='+encodeURIComponent(rel),null)}
    else{const b=await scanBlob('scan/save?'+qs);const u=await blobToDataURL(b);const dd=node(dir);if(!dd)throw new Error('That folder is not there any more.');dd[name]=u;fsSave()}
    done++}
   if(disk&&typeof hdRefresh=='function')await hdRefresh();
   if(typeof EXPL!='undefined')EXPL.forEach(f=>f());
   say((done>1?done+' files':'The scan')+' saved in '+(dir[dir.length-1]||'C:')+'.');notify('Scan saved',jobs.map(x=>x.name).join(', '),{quiet:true})}
  catch(e){say(e.message,true)}setBusy(false)}
 q('.scsave').onclick=save;
 const oc=W.onclose;W.onclose=()=>{st.closed=true;if(st.busy)rdPost('scan/cancel',null).catch(()=>{});rdPost('scan/drop?session='+sess,null).catch(()=>{});st.pages.forEach(p=>URL.revokeObjectURL(p.url));if(st.prev)URL.revokeObjectURL(st.prev);oc&&oc()};
 setBusy(false);find();return W}
/* File Explorer: Scan into this folder (its commands' row) */
explorer=__after(explorer,function(){const W=wins[wins.length-1];if(!W||W.app!='explorer'||!W.body)return;const x=W.body.querySelector('.xcmd .dn');if(!x||W.body.querySelector('.xscan'))return;
 const b=document.createElement('button');b.className='xscan';b.textContent='📠 Scan';b.title='Scan into this folder';b.onclick=()=>scanApp({dir:typeof W.curPath=='function'?W.curPath():null});x.after(b)},'scan62.js');
/* the Print dialog: Scan… beside Find printers */
prDialog=__after(prDialog,function(){const ov=[...document.querySelectorAll('.prdlg')].pop();if(!ov||ov.querySelector('.pscan'))return;const f=ov.querySelector('.pfind');if(!f)return;
 const b=document.createElement('button');b.className='pscan';b.textContent='📠 Scan…';b.title='Open Scan';b.onclick=()=>scanApp();f.after(b)},'scan62.js');
/* Settings › Devices: the scanners, and Scan */
(function(){if(typeof devPanel!='function')return;devPanel=__after(devPanel,function(c){if(typeof RD=='undefined'||!RD||c.querySelector('.scn62'))return;const x=c.querySelector('.pq61')||c.querySelector('.up60')||c.querySelector('.dvx');if(!x)return;
 const box=document.createElement('div');box.className='scn62';const h=document.createElement('h4');h.style.margin='12px 0 4px';h.textContent='Scanners';box.appendChild(h);
 const m=document.createElement('div');m.className='msg';m.textContent='Scanners and printer-scanners on the network (eSCL, also called AirScan, and WSD) and on USB work without a driver.';box.appendChild(m);
 const b=document.createElement('button');b.textContent='📠 Open Scan';b.style.marginTop='4px';b.onclick=()=>scanApp();box.appendChild(b);x.after(box)},'scan62.js')})();
