/* ===== 1.6.0 (docs/plan-1.6.0.md Part 8): Settings › Devices › Printers plugged in by USB =====
   rudolfd's GET print/usb (rudolf-print usb: /run/rudolf/usb-printers, written by /usr/lib/rudolf/usbprint as printers
   come and go): each one's name and kind, as the print dialog lists it ("USB: name"): IPP over USB (through ipp-usb,
   the USB printers extension), PDF or PostScript (written to the printer as it is; PostScript through the extension),
   or "needs a driver" Rudolf OS does not have. Where the extension is needed, a button opens Update and recovery's
   System extensions. Drawn as text (a printer's name is the printer's own). */
window.__usbprint60=1;
const UP_ICON='🖨️';
function upLine(p){
 if(p.usb=='ipp')return p.ready?['IPP over USB · ready to print',0]:['IPP over USB · needs the USB printers extension to print',1];
 if(p.usb=='pdf')return['Reads PDF · ready to print',0];
 if(p.usb=='postscript')return p.ready?['Reads PostScript · ready to print',0]:['Reads PostScript · needs the USB printers extension to print',1];
 return['Needs a driver Rudolf OS does not have (PCL or a maker\'s own language): it cannot print yet',0]}
async function upSection(box){let L=null;try{L=await rd('print/usb')}catch(e){box.innerHTML='';return}
 if(!box.isConnected)return;const ps=L.printers||[];
 box.innerHTML=`<h4 style='margin:12px 0 4px'>Printers plugged in by USB</h4>`+(ps.length?`<table class='dvt upt' style='width:100%;border-collapse:collapse'></table>`:`<div class='msg'>No USB printer is plugged in. Network printers are found by the print dialog.</div>`);
 const t=box.querySelector('.upt');ps.forEach(p=>{const r=t.insertRow();r.insertCell().textContent=UP_ICON;r.cells[0].style.width='28px';
  const c=r.insertCell(),b=document.createElement('b');b.textContent=p.name||'USB printer';c.appendChild(b);c.appendChild(document.createElement('br'));
  const sm=document.createElement('small'),[line,need]=upLine(p);sm.textContent=line+(p.model&&p.model!=p.name?' · '+p.model:'');if(p.usb=='driver')sm.style.color='#b45309';c.appendChild(sm);
  const a=r.insertCell();a.style.textAlign='right';if(need){const g=document.createElement('button');g.textContent='System extensions…';g.className='upx';g.onclick=()=>settings('upd');a.appendChild(g)}})}
(function(){if(typeof devPanel!='function')return;const D0=devPanel;devPanel=function(c){D0(c);if(typeof RD=='undefined'||!RD)return;const x=c.querySelector('.dvx');if(!x)return;
 const box=document.createElement('div');box.className='up60';x.after(box);upSection(box);
 const rf=c.querySelector('.dvr');if(rf){const o=rf.onclick;rf.onclick=()=>{o&&o();upSection(box)}}
 const live=()=>{if(!c.isConnected){removeEventListener('rudolf-usb',live);return}setTimeout(()=>upSection(box),1500)};addEventListener('rudolf-usb',live)}})();
