/* ===== Printing for WordPro (Rudolf OS) =====
   File > Print… opens the Rudolf print dialog on Rudolf OS; in a plain browser WordPro keeps using the
   browser's own print dialog. The document page (paper size, margins, header, footer, page numbers) is
   turned into a PDF by the disc's Chromium (rudolfd print/pdf), shown as a preview, and then
   - saved as a PDF in Documents or on this device, or
   - sent to a network printer that speaks IPP Everywhere / AirPrint (rudolf-print; no CUPS, no drivers):
     as the PDF when the printer takes PDF, otherwise as PWG or Apple raster rendered here with pdf.js. */
const PR_MEDIA={Letter:'na_letter_8.5x11in',A4:'iso_a4_210x297mm',Legal:'na_legal_8.5x14in',A5:'iso_a5_148x210mm',Executive:'na_executive_7.25x10.5in'};
const prEsc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
let PR_FOUND=null,PR_FINDING=null;
function prSaved(){return Array.isArray(S.printers)?S.printers:[]}
function prRemember(p){const l=prSaved().filter(x=>x.uri!=p.uri);l.unshift({name:p.name,uri:p.uri});S.printers=l.slice(0,12);if(typeof SC=='object'&&SC)SC.printers=S.printers.slice();persistS()}
function prFind(force){if(PR_FINDING&&!force)return PR_FINDING;PR_FINDING=prJSON('print/find').then(j=>PR_FOUND=j.printers||[]).catch(()=>PR_FOUND=[]);return PR_FINDING}
async function prJSON(path,body){const r=await fetch('/api/'+path,{method:'POST',headers:{'X-Rudolf-Token':RD.token},body});let j=null;try{j=await r.json()}catch(e){}if(!j)throw new Error('HTTP '+r.status);if(!j.ok)throw new Error(j.error||j.message||'failed');return j}

/* ---------- raster (PWG 5102.4 and Apple URF), line-repeat + PackBits-style runs ---------- */
function prPack(px,w,h,bpp){const row=w*bpp,out=new Uint8Array(h*(1+w*(1+bpp)));let o=0;
 const eq=(i,j)=>{for(let k=0;k<bpp;k++)if(px[i+k]!==px[j+k])return false;return true};
 for(let y=0;y<h;){let rep=0;const a=y*row;
  while(rep<255&&y+rep+1<h){const b=(y+rep+1)*row;let same=true;for(let i=0;i<row;i++)if(px[a+i]!==px[b+i]){same=false;break}if(!same)break;rep++}
  out[o++]=rep;let x=0;
  while(x<w){let n=1;const p=a+x*bpp;
   while(x+n<w&&n<128&&eq(p,p+n*bpp))n++;
   if(n>1){out[o++]=n-1;for(let k=0;k<bpp;k++)out[o++]=px[p+k];x+=n;continue}
   let m=1;while(x+m<w&&m<128&&!(x+m+1<w&&eq(a+(x+m)*bpp,a+(x+m+1)*bpp)))m++;
   if(m==1){out[o++]=0;for(let k=0;k<bpp;k++)out[o++]=px[p+k]}else{out[o++]=257-m;out.set(px.subarray(p,p+m*bpp),o);o+=m*bpp}
   x+=m}
  y+=rep+1}
 return out.subarray(0,o)}
class PrBytes{constructor(){this.parts=[]}raw(u8){this.parts.push(u8)}blob(t){return new Blob(this.parts,{type:t})}}
function prHdrPwg(o){const h=new Uint8Array(1796),dv=new DataView(h.buffer),s=(off,t)=>{for(let i=0;i<t.length&&i<63;i++)h[off+i]=t.charCodeAt(i)},u=(off,v)=>dv.setUint32(off,v>>>0),i32=(off,v)=>dv.setInt32(off,v);
 s(0,'PwgRaster');u(272,o.duplex?1:0);u(276,o.dpi);u(280,o.dpi);u(340,1);u(352,Math.round(o.ptW));u(356,Math.round(o.ptH));u(368,o.tumble?1:0);
 u(372,o.w);u(376,o.h);u(384,8);u(388,o.bpp*8);u(392,o.w*o.bpp);u(396,0);u(400,o.bpp==3?19:18);u(420,o.bpp);
 u(452,o.total);i32(456,o.xf||1);i32(460,o.yf||1);u(464,0);u(468,0);u(472,o.w);u(476,o.h);u(480,0xffffff);u(484,4);s(1732,o.media||'');return h}
function prHdrUrf(o){const h=new Uint8Array(32),dv=new DataView(h.buffer);h[0]=o.bpp*8;h[1]=o.bpp==3?1:0;h[2]=o.duplex?(o.tumble?2:3):1;h[3]=4;dv.setUint32(12,o.w);dv.setUint32(16,o.h);dv.setUint32(20,o.dpi);return h}
/* back sides of two-sided raster jobs: how the printer wants them turned (pwg-raster-document-sheet-back) */
function prBackTurn(back,tumble){if(back=='flipped')return tumble?'x':'y';if(back=='manual-tumble'&&tumble)return'xy';if(back=='rotated'&&!tumble)return'xy';return''}
function prTurn(px,w,h,bpp,how){if(!how)return px;const o=new Uint8Array(px.length);
 for(let y=0;y<h;y++){const ty=how.includes('y')?h-1-y:y;for(let x=0;x<w;x++){const tx=how.includes('x')?w-1-x:x,a=(y*w+x)*bpp,b=(ty*w+tx)*bpp;for(let k=0;k<bpp;k++)o[b+k]=px[a+k]}}return o}
async function prRaster(pdf,o,progress){const lib=await pdfLib(),doc=await lib.getDocument({data:new Uint8Array(await pdf.arrayBuffer())}).promise;
 const first=Math.max(1,o.from||1),last=Math.min(doc.numPages,o.to||doc.numPages),total=last-first+1,out=new PrBytes();
 if(o.kind=='urf'){const h=new Uint8Array(12);h.set([85,78,73,82,65,83,84,0]);new DataView(h.buffer).setUint32(8,total);out.raw(h)}else out.raw(new Uint8Array([82,97,83,50]));
 const back=o.duplex?prBackTurn(o.sheetBack,o.tumble):'';
 for(let n=first;n<=last;n++){progress&&progress(`Preparing page ${n-first+1} of ${total}…`);
  const pg=await doc.getPage(n),vp1=pg.getViewport({scale:1}),vp=pg.getViewport({scale:o.dpi/72}),w=Math.round(vp.width),h=Math.round(vp.height);
  const cv=document.createElement('canvas');cv.width=w;cv.height=h;const cx=cv.getContext('2d',{willReadFrequently:true});cx.fillStyle='#fff';cx.fillRect(0,0,w,h);
  await pg.render({canvasContext:cx,viewport:vp,background:'#fff'}).promise;
  const d=cx.getImageData(0,0,w,h).data,bpp=o.color?3:1;let px=new Uint8Array(w*h*bpp);
  if(bpp==3)for(let i=0,j=0;i<d.length;i+=4){px[j++]=d[i];px[j++]=d[i+1];px[j++]=d[i+2]}
  else for(let i=0,j=0;i<d.length;i+=4)px[j++]=(d[i]*77+d[i+1]*150+d[i+2]*29)>>8;
  const turn=(n-first)%2==1?back:'';px=prTurn(px,w,h,bpp,turn);
  const meta={w,h,bpp,dpi:o.dpi,ptW:vp1.width,ptH:vp1.height,duplex:o.duplex,tumble:o.tumble,total,media:o.media,xf:turn.includes('x')?-1:1,yf:turn.includes('y')?-1:1};
  out.raw(o.kind=='urf'?prHdrUrf(meta):prHdrPwg(meta));out.raw(prPack(px,w,h,bpp).slice());cv.width=cv.height=0;await new Promise(r=>setTimeout(r,0))}
 return out.blob(o.kind=='urf'?'image/urf':'image/pwg-raster')}
/* how to send to this printer: {format, dpi, color} or null when it needs a driver */
function prPlan(info,wantColor){const f=info.formats||[];
 if(f.includes('application/pdf'))return{format:'application/pdf'};
 if(f.includes('image/pwg-raster')&&(info.pwgResolutions||[]).length){const r=info.pwgResolutions,t=info.pwgTypes||[];
  const dpi=r.includes(300)?300:(r.filter(x=>x>=300).sort((a,b)=>a-b)[0]||Math.max(...r));const color=wantColor&&t.includes('srgb_8');
  if(!color&&!t.includes('sgray_8')&&!t.includes('srgb_8'))return null;return{format:'image/pwg-raster',kind:'pwg',dpi,color:color||!t.includes('sgray_8'),sheetBack:(info.sheetBack||[])[0]||'normal'}}
 if(f.includes('image/urf')&&(info.urf||[]).length){const u=info.urf,rs=(u.find(x=>/^RS/.test(x))||'RS300').slice(2).split('-').map(Number);
  const dpi=rs.includes(300)?300:rs[0];const color=wantColor&&u.includes('SRGB24');if(!color&&!u.includes('W8')&&!u.includes('SRGB24'))return null;
  const dm=(u.find(x=>/^DM\d/.test(x))||'DM1')[2];return{format:'image/urf',kind:'urf',dpi,color:color||!u.includes('W8'),sheetBack:{1:'normal',2:'flipped',3:'rotated',4:'manual-tumble'}[dm]||'normal'}}
 return null}
function prState(info){const st={3:'Ready',4:'Printing',5:'Stopped'}[info.state]||'Unknown';const r=(info.reasons||[]).filter(x=>x&&x!='none').map(x=>x.replace(/-(report|warning|error)$/,'').replace(/-/g,' '));
 return st+(r.length?' · '+r.join(', '):'')+(info.message?' · '+info.message:'')}

/* ---------- the dialog ---------- */
function wpPrint(W,html,o){if(!RD)return false;prDialog(W,html,o);return true}
async function prDialog(W,html,o){
 const ov=document.createElement('div');ov.className='rmodal';const title=String(o.title||'Document').replace(/\.[^.]*$/,'');
 ov.innerHTML=`<div class='rmbox prdlg' role='dialog' aria-label='Print'><h3>Print</h3><div class='prgrid'><div class='prprev' aria-label='Preview'><div class='msg'>Preparing the preview…</div></div>
<div class='propts'><label>Printer</label><select class='pdst'><option value='pdf'>Save as PDF</option></select>
<div class='msg pst'></div>
<label>Pages</label><div><label class='ck'><input type='radio' name='prp' class='pall' checked> All</label> <label class='ck'><input type='radio' name='prp' class='prng'> Pages</label> <input type='number' class='pfrom' min='1' value='1' style='width:52px'> – <input type='number' class='pto' min='1' value='1' style='width:52px'></div>
<label>Copies</label><div><input type='number' class='pcop' min='1' max='99' value='1' style='width:60px'></div>
<label>Color</label><div><select class='pcol'><option value='color'>Color</option><option value='monochrome'>Black and white</option></select></div>
<label>Two-sided</label><div><select class='psid'><option value='one-sided'>Off</option><option value='two-sided-long-edge'>Flip on long edge</option><option value='two-sided-short-edge'>Flip on short edge</option></select></div>
<label>Paper</label><div class='ppap'>${prEsc(o.size||'Letter')}${o.land?' landscape':''} <span class='msg'>(File › Page setup)</span></div></div></div>
<div class='msg pmsg' aria-live='polite'></div><div class='rmrow'><button class='pfind' title='Search the network again'>Find printers</button><span style='flex:1'></span><button class='pcan'>Cancel</button><button class='pgo pri' disabled>Save…</button></div></div>`;
 document.body.appendChild(ov);const q=s=>ov.querySelector(s),msg=t=>q('.pmsg').textContent=t||'';
 let pdf=null,pages=0,info=null,busy=false;const infos={};
 const close=()=>{ov.remove();document.removeEventListener('keydown',key,true)};const key=e=>{if(e.key=='Escape'&&!busy&&ov.contains(document.activeElement)){e.preventDefault();close()}};document.addEventListener('keydown',key,true);
 q('.pcan').onclick=()=>{if(!busy)close()};
 /* printers */
 const fillDst=()=>{const sel=q('.pdst'),cur=sel.value,seen=new Set(['pdf']);let h=`<option value='pdf'>Save as PDF</option>`;
  const add=(p,tag)=>{if(seen.has(p.uri))return;seen.add(p.uri);infos[p.uri]=infos[p.uri]||{name:p.name};h+=`<option value='${prEsc(p.uri)}'>${prEsc(p.name)}${tag?' — '+tag:''}${p.location?' ('+prEsc(p.location)+')':''}</option>`};
  prSaved().forEach(p=>add(p,(PR_FOUND||[]).some(f=>f.uri==p.uri)||!PR_FOUND?'':'not found now'));(PR_FOUND||[]).forEach(p=>add(p,''));
  h+=`<option value='add'>Add a printer by address…</option>`;sel.innerHTML=h;sel.value=seen.has(cur)?cur:(seen.has(S.printLast)?S.printLast:'pdf')};
 const find=async force=>{q('.pfind').disabled=true;q('.pst').textContent='Looking for printers on the network…';await prFind(force);q('.pfind').disabled=false;fillDst();await pick()};
 q('.pfind').onclick=()=>find(true);
 const opts=on=>['.pcop','.pcol','.psid','.prng','.pfrom','.pto'].forEach(s=>q(s).disabled=!on);
 async function pick(){const v=q('.pdst').value;info=null;
  if(v=='add'){const a=await askText('Printer address (for example 192.168.1.20 or ipp://192.168.1.20/ipp/print)');if(a&&a.trim()){let u=a.trim();if(!/^ipps?:\/\//.test(u))u='ipp://'+u.replace(/^https?:\/\//,'');if(!/\/\/[^/]+\//.test(u))u+='/ipp/print';
    try{msg('Asking the printer…');const i=await prJSON('print/info?uri='+encodeURIComponent(u));prRemember({name:i.name||u,uri:u});infos[u]=i;fillDst();q('.pdst').value=u;msg('')}catch(e){msg('No printer answered at that address: '+e.message);q('.pdst').value='pdf'}}else q('.pdst').value='pdf'}
  const d=q('.pdst').value,go=q('.pgo');
  if(d=='pdf'){q('.pst').textContent='Saves the pages exactly as previewed.';opts(false);go.textContent='Save…';go.disabled=!pdf;return}
  opts(false);go.textContent='Print';go.disabled=true;q('.pst').textContent='Asking the printer…';
  try{info=await prJSON('print/info?uri='+encodeURIComponent(d));infos[d]=info}catch(e){q('.pst').textContent=e.message;return}
  if(q('.pdst').value!=d)return;const plan=prPlan(info,true);
  q('.pst').textContent=[prState(info),info.model].filter(Boolean).join(' · ');
  if(!plan){q('.pst').textContent+=' — this printer needs a driver; Rudolf OS prints to IPP Everywhere and AirPrint printers.';return}
  opts(true);q('.pcol').disabled=!info.color&&!(info.colorModes||[]).includes('color');if(q('.pcol').disabled)q('.pcol').value='monochrome';
  const sides=info.sides||[];q('.psid').querySelectorAll('option').forEach(op=>op.disabled=op.value!='one-sided'&&!sides.includes(op.value));if(sides.length<2)q('.psid').value='one-sided';
  const media=PR_MEDIA[o.size]||'';q('.ppap').querySelector('.msg').textContent=media&&(info.media||[]).length&&!(info.media||[]).includes(media)?'(this printer may not have this paper)':'(File › Page setup)';
  go.disabled=!pdf||busy}
 q('.pdst').onchange=pick;
 /* preview */
 try{const r=await fetch('/api/print/pdf',{method:'POST',headers:{'X-Rudolf-Token':RD.token},body:html});if(!r.ok){let e='HTTP '+r.status;try{e=(await r.json()).error||e}catch(x){}throw new Error(e)}pdf=await r.blob()}
 catch(e){q('.prprev').innerHTML=`<div class='msg'>The pages could not be prepared: ${prEsc(e.message)}</div>`;find();return}
 if(!ov.isConnected)return;
 try{const lib=await pdfLib(),doc=await lib.getDocument({data:new Uint8Array(await pdf.arrayBuffer())}).promise;pages=doc.numPages;q('.pto').value=pages;q('.pfrom').max=q('.pto').max=pages;
  const pv=q('.prprev');pv.innerHTML=`<div class='msg'>${pages} page${pages==1?'':'s'}</div>`;
  for(let n=1;n<=Math.min(pages,30)&&ov.isConnected;n++){const pg=await doc.getPage(n),v0=pg.getViewport({scale:1}),vp=pg.getViewport({scale:Math.min(2,200/v0.width)});const c=document.createElement('canvas');c.width=vp.width;c.height=vp.height;c.className='prpage';c.title='Page '+n;c.setAttribute('aria-label','Page '+n);pv.appendChild(c);await pg.render({canvasContext:c.getContext('2d'),viewport:vp}).promise}}
 catch(e){q('.prprev').innerHTML=`<div class='msg'>Preview unavailable (${prEsc(e.message)}). Printing still works.</div>`}
 if(!ov.isConnected)return;q('.pgo').disabled=false;find();
 q('.pgo').onclick=async()=>{const d=q('.pdst').value;
  if(d=='pdf'){close();savePdf(title.slice(0,40).toUpperCase()+'.PDF',pdf);return}
  if(!info)return;const plan=prPlan(info,q('.pcol').value=='color');if(!plan)return;
  const all=q('.pall').checked,from=all?1:Math.max(1,+q('.pfrom').value||1),to=all?pages:Math.min(pages,Math.max(from,+q('.pto').value||pages));
  const sides=q('.psid').value,args=new URLSearchParams({uri:d,format:plan.format,copies:String(Math.max(1,Math.min(99,+q('.pcop').value||1))),color:q('.pcol').value,title,media:PR_MEDIA[o.size]||''});
  if(sides!='one-sided')args.set('sides',sides);
  busy=true;q('.pgo').disabled=q('.pcan').disabled=q('.pfind').disabled=true;
  try{let doc=pdf;
   if(plan.format=='application/pdf'){if(!all)args.set('pages',from+'-'+to)}
   else doc=await prRaster(pdf,{kind:plan.kind,dpi:plan.dpi,color:plan.color,from,to,duplex:sides!='one-sided',tumble:sides=='two-sided-short-edge',sheetBack:plan.sheetBack,media:PR_MEDIA[o.size]},msg);
   msg('Sending to '+(infos[d].name||'the printer')+'…');const j=await prJSON('print/job?'+args,doc);
   prRemember({name:infos[d].name||d,uri:d});S.printLast=d;if(typeof SC=='object'&&SC)SC.printLast=d;persistS();
   busy=false;close();notify('Printing',`${title} → ${infos[d].name||'printer'}${j.job?' (job '+j.job+')':''}. ${j.message||''}`.trim())}
  catch(e){busy=false;q('.pcan').disabled=q('.pfind').disabled=false;q('.pgo').disabled=false;msg('Not printed: '+e.message)}};
 pick()}
