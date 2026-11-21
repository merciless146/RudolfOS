/* ===== 1.6.1 (docs/plan-1.6.1.md Part 9): Settings › Devices › Print queue =====
   rudolfd's print/jobs (rudolf-printq: the desktop's jobs and the Linux apps', each its sender's): every job waiting,
   printing or finished in the last hour, newest first, with its printer and what became of it. The signed-in account's
   own jobs have their names and a Cancel button; another account's are listed as "Another account's document", with no
   name and no Cancel (the queue refuses it anyway). Looked at again every 3 s while a job is waiting or printing.
   Drawn as text (a document's and a printer's names are the apps' and the printers' own). */
window.__printq61=1;
const PQ_STATE={waiting:['⏳','Waiting'],printing:['🖨️','Printing'],sent:['✓','Sent to the printer'],canceled:['✕','Canceled'],failed:['⚠️','Not printed']};
function pqWhen(t){if(!t)return'';const d=new Date(t*1000);return d.toLocaleTimeString([],{hour:'numeric',minute:'2-digit'})}
async function pqSection(box){let L=null;try{L=await prJSON('print/jobs')}catch(e){box.textContent='';return}
 if(!box.isConnected)return;box.textContent='';
 const h=document.createElement('h4');h.style.margin='12px 0 4px';h.textContent='Print queue';box.appendChild(h);
 const js=L.jobs||[];
 if(L.queue===false||!js.length){const m=document.createElement('div');m.className='msg';m.textContent=L.queue===false?'The print queue is not running on this computer: documents go straight to the printer.':'Nothing is printing. Documents printed from this desktop and from Linux apps are listed here for an hour.';box.appendChild(m);return}
 const t=document.createElement('table');t.className='dvt pqt';t.style.cssText='width:100%;border-collapse:collapse';box.appendChild(t);
 js.forEach(j=>{const r=t.insertRow(),[ic,st]=PQ_STATE[j.state]||['🖨️',j.state];r.insertCell().textContent=ic;r.cells[0].style.width='28px';
  const c=r.insertCell(),b=document.createElement('b');b.textContent=j.mine?(j.name||'Document'):(j.name||'Another account\'s document');c.appendChild(b);c.appendChild(document.createElement('br'));
  const sm=document.createElement('small');sm.textContent=[j.printer||'Printer',st+(j.message&&j.state=='failed'?': '+j.message:''),pqWhen(j.done||j.made)].filter(Boolean).join(' · ');if(j.state=='failed')sm.style.color='#b45309';c.appendChild(sm);
  const a=r.insertCell();a.style.textAlign='right';
  if(j.cancel){const g=document.createElement('button');g.textContent='Cancel';g.className='pqx';g.onclick=async()=>{g.disabled=true;try{await prJSON('print/cancel?id='+encodeURIComponent(j.id))}catch(e){notify('Print queue',e.message)}pqSection(box)};a.appendChild(g)}});
 if(js.some(j=>j.state=='waiting'||j.state=='printing'))setTimeout(()=>{if(box.isConnected)pqSection(box)},3000)}
(function(){if(typeof devPanel!='function')return;const D0=devPanel;devPanel=function(c){D0(c);if(typeof RD=='undefined'||!RD)return;const x=c.querySelector('.up60')||c.querySelector('.dvx');if(!x)return;
 const box=document.createElement('div');box.className='pq61';x.after(box);pqSection(box);
 const rf=c.querySelector('.dvr');if(rf){const o=rf.onclick;rf.onclick=()=>{o&&o();pqSection(box)}}}})();
