/* ===== 1.5.9 (2026-11-14): Oracle's "+" on the bookmarks bar =====
   The rightmost button of the bookmarks bar adds a bookmark three ways: by its address (a URL), from the browsing
   history (newest first, searchable), or a file on this computer (a document or a PDF in C:, opened in Oracle as
   file:///C:/...). Oracle's browser() hands its own list and history (oBmAdd(ctx)); the list is saved as the ☆ does
   (bd95bm). A private window offers the account's history as it is (it adds nothing to it). Names are cut to 18 characters, as the ☆'s. */
window.__bmadd59=1;
const BMA_MAX=18;
/* an address as Oracle opens it: http(s), file: and about: as they are; "example.com/x" gets https:// */
function oBmUrl(v){v=String(v||'').trim();if(!v)return'';if(/^(https?:\/\/|file:\/\/\/|about:)/i.test(v))return v;
 if(/^[A-Za-z]:\\/.test(v))return'file:///'+v.split('\\').join('/');
 if(/^[^\s\/]+\.[a-z]{2,}(\/|:|$)/i.test(v)&&!/\s/.test(v))return'https://'+v;return''}
function oBmName(n,u){n=String(n||'').trim();if(!n){try{n=/^https?:/i.test(u)?new URL(u).hostname.replace(/^www\./,''):String(u).split('/').pop()}catch(e){n=u}}return n.slice(0,BMA_MAX)}
function oBmAdd(ctx){
 const o=document.createElement('div');o.className='rmodal obma';
 o.innerHTML=`<div class='rmbox' role='dialog' aria-label='Add a bookmark' style='max-width:520px;width:92vw'><h3>Add a bookmark</h3>
<div class='bmaw'><p class='msg'>How do you want to add it?</p>
<div style='display:flex;flex-direction:column;gap:6px'><button class='bma-url' style='text-align:left'>🔗 By its address (URL)</button>
<button class='bma-hist' style='text-align:left'>🕘 From your browsing history</button>
<button class='bma-file' style='text-align:left'>📄 A file on this computer (a document or a PDF)</button></div></div>
<div class='bmaf' hidden></div><p class='msg bmaerr' style='color:#b91c1c'></p>
<div class='rmrow'><button class='bma-back' hidden>Back</button><button class='bma-x'>Cancel</button></div></div>`;
 document.body.appendChild(o);typeof promptFront=='function'&&promptFront(o);
 const $o=s=>o.querySelector(s),err=t=>{$o('.bmaerr').textContent=t||''},close=()=>o.remove();
 const add=(name,url)=>{const u=oBmUrl(url);if(!u){err('That is not an address Oracle can open.');return false}
  if(ctx.bms.some(b=>b[1]==u)){err('That page is already a bookmark.');return false}
  ctx.bms.push([oBmName(name,u),u]);ctx.save();ctx.redraw();close();return true};
 const step=(build)=>{err('');$o('.bmaw').hidden=true;const f=$o('.bmaf');f.hidden=false;f.innerHTML='';$o('.bma-back').hidden=false;build(f)};
 $o('.bma-back').onclick=()=>{err('');$o('.bmaw').hidden=false;$o('.bmaf').hidden=true;$o('.bma-back').hidden=true};
 $o('.bma-x').onclick=close;o.addEventListener('keydown',e=>{e.stopPropagation();if(e.key=='Escape')close()});
 /* by address */
 $o('.bma-url').onclick=()=>step(f=>{f.innerHTML=`<div class='r2' style='grid-template-columns:auto 1fr;gap:6px 10px'><label>Address</label><input class='bmau' placeholder='https://example.com' spellcheck='false' autocomplete='off'>
<label>Name</label><input class='bman' placeholder='(from the address)' maxlength='${BMA_MAX}'></div><div class='rmrow'><button class='pri bmaok'>Add</button></div>`;
  const u=f.querySelector('.bmau'),n=f.querySelector('.bman');const c=typeof ctx.cur=='function'?ctx.cur():'';if(/^https?:/i.test(c)){u.value=c;n.value=oBmName(ctx.title&&ctx.title(),c)}
  const go=()=>add(n.value,u.value);f.querySelector('.bmaok').onclick=go;[u,n].forEach(x=>x.onkeydown=e=>{if(e.key=='Enter')go()});u.focus();u.select()});
 /* from the history */
 $o('.bma-hist').onclick=()=>step(f=>{const seen=new Set(),L=[];(ctx.hist||[]).slice().reverse().forEach(h=>{if(!h||!h[1]||seen.has(h[1])||/^about:(home|blank)/.test(h[1]))return;seen.add(h[1]);L.push(h)});
  if(!L.length){f.innerHTML="<p class='msg bmanone'></p>";f.querySelector('.bmanone').textContent='Your browsing history is empty'+(ctx.private?' (a private window adds nothing to it)':'')+': add it by its address instead.';return}
  f.innerHTML=`<input class='bmas' placeholder='Search your history' style='width:100%;margin-bottom:6px' spellcheck='false'><div class='bmal' style='max-height:45vh;overflow:auto;border:1px solid var(--bd);border-radius:4px'></div>`;
  const list=f.querySelector('.bmal'),s=f.querySelector('.bmas');
  const draw=()=>{const k=s.value.trim().toLowerCase();list.innerHTML='';L.filter(h=>!k||String(h[0]).toLowerCase().includes(k)||String(h[1]).toLowerCase().includes(k)).slice(0,100).forEach(h=>{
   const r=document.createElement('button');r.className='bmah';r.style.cssText='display:block;width:100%;text-align:left;border:0;border-bottom:1px solid var(--bd);border-radius:0;padding:5px 8px;background:transparent';
   const t=document.createElement('b');t.textContent=h[0]||h[1];const a=document.createElement('div');a.style.cssText='font-size:11px;color:#6b7280;overflow:hidden;text-overflow:ellipsis;white-space:nowrap';a.textContent=h[1];
   r.append(t,a);r.title=h[1];r.onclick=()=>add(h[0],h[1]);list.appendChild(r)});if(!list.children.length){const p=document.createElement('p');p.className='msg';p.style.padding='6px';p.textContent='Nothing in your history matches.';list.appendChild(p)}};
  s.oninput=draw;draw();s.focus()});
 /* a file on this computer */
 $o('.bma-file').onclick=async()=>{if(typeof pickVfsFile!='function'){err('The file picker is not available here.');return}
  o.style.display='none';const p=await pickVfsFile('Bookmark a file',n=>/\.(PDF|HTM|HTML|TXT|DOC|DOCX|MD|RDECK|RSHEET|PNG|JPG|JPEG|GIF|WEBP|SVG)$/i.test(n));o.style.display='';
  if(!p)return;add(String(p.at(-1)).replace(/\.[^.]+$/,''),'file:///'+p.join('/'))};
 setTimeout(()=>$o('.bma-url').focus(),0);return o}
