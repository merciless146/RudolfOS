/* ===== 2026-11-14: WordPro's hyperlinks, as other word processors have them =====
   Asked for: creating and managing hyperlinks as in other word processors. WordPro had a two-field dialog (text and
   address), Ctrl+K, and Ctrl+click to open. Now:
   - Insert or edit a hyperlink (Ctrl+K, Insert › Hyperlink…, a link's right-click menu): link to a web address, a place
     in this document (a heading or a bookmark), a file on this computer (the Open dialog, filedlg59.js) or an e-mail
     address with a subject; the text to show; a ScreenTip (the link's tooltip). Editing keeps the link's formatting;
     Remove link keeps its text.
   - Bookmarks (Insert › Bookmark…): a named place at the cursor; listed with Go to and Delete.
   - Right-click a link: Open link, Edit link…, Copy link address, Remove link.
   - Ctrl+click (or the menu's Open): web addresses in Oracle, places scrolled to, files opened in their app, e-mail
     addresses in the e-mail window (emailCompose).
   - Automatic links: a web or e-mail address typed and followed by a space or Enter becomes a link (on by default,
     switched in the Links list); Ctrl+Z right after takes it back.
   - Insert › Links in this document…: every link (its text, where it goes, a ⚠ for a place or file that is not there),
     with Go to, Edit and Remove for each, and Remove all links.
   Addresses are checked: only https, http, mailto, a #place and file:///C: links are made (never javascript: or
   data:), and every text is put in as text. */
window.__wplinks59=1;
const WPL_AUTO='wp59autolink';
const wplAutoOn=()=>{try{return localStorage.getItem(WPL_AUTO)!=='0'}catch(e){return true}};
const wplEsc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
/* what a link's address is: web, place, file, mail, or '' (refused) */
function wplKind(h){h=String(h||'').trim();if(/^#[A-Za-z][\w:.-]*$/.test(h))return'place';if(/^https?:\/\//i.test(h))return'web';if(/^mailto:/i.test(h))return'mail';if(/^file:\/\/\/C:\//i.test(h))return'file';return''}
function wplSlug(t){return String(t||'').toLowerCase().normalize('NFKD').replace(/[^\w\s-]/g,'').trim().replace(/[\s_]+/g,'-').slice(0,40)||'place'}
/* the places of a document: headings (each given an id when it has none) and bookmarks */
function wplPlaces(ed){const out=[],used=new Set([...ed.querySelectorAll('[id]')].map(e=>e.id));
 ed.querySelectorAll('h1,h2,h3,h4,h5,h6').forEach(h=>{const t=h.textContent.trim();if(!t)return;if(!h.id){let id='h-'+wplSlug(t),i=2;while(used.has(id))id='h-'+wplSlug(t)+'-'+(i++);h.id=id;used.add(id)}out.push({id:h.id,label:'¶ '+t,lvl:+h.tagName[1],kind:'heading'})});
 ed.querySelectorAll('a.wp-bm[id]').forEach(a=>out.push({id:a.id,label:'🔖 '+(a.dataset.name||a.id.replace(/^bm-/,'')),kind:'bookmark'}));return out}
function wplTarget(ed,h){if(wplKind(h)!='place')return null;try{return ed.querySelector('#'+CSS.escape(h.slice(1)))}catch(e){return null}}
function wplFile(h){const p=decodeURIComponent(String(h).replace(/^file:\/\/\//i,'')).split('/').filter(Boolean).map(x=>x.toUpperCase());return p[0]=='C:'?p:null}
function wplOpen(X,a){const h=a.getAttribute('href')||'',k=wplKind(h);
 if(k=='place'){const t=wplTarget(X.ed,h);if(t){t.scrollIntoView({behavior:'smooth',block:'center'});t.classList.add('wpl-flash');setTimeout(()=>t.classList.remove('wpl-flash'),1200)}else X.say('That place is no longer in this document.');return}
 if(k=='web'){typeof browser=='function'?browser(h):window.open(h,'_blank','noopener');return}
 if(k=='mail'){const m=/^mailto:([^?]*)(?:\?(.*))?$/i.exec(h)||[];const q=new URLSearchParams(m[2]||'');typeof emailCompose=='function'?emailCompose({to:decodeURIComponent(m[1]||''),subject:q.get('subject')||''}):0;return}
 if(k=='file'){const p=wplFile(h);if(p&&node(p)!==undefined){typeof openPath=='function'&&openPath(p)}else X.say('That file is not there any more.');return}
 X.say('That link cannot be opened.')}
function wplLinkAt(X){const s=getSelection();if(!s.rangeCount)return null;let n=s.anchorNode;while(n&&n!==X.ed){if(n.nodeType==1&&n.tagName=='A'&&!n.classList.contains('wp-bm'))return n;n=n.parentNode}return null}
/* the dialog */
function wplDialog(X,cur){X.rs();const s=getSelection();const selText=s.rangeCount?s.toString():'';cur=cur||wplLinkAt(X);
 const h0=cur?cur.getAttribute('href')||'':'',k0=cur?(wplKind(h0)||'web'):'web';
 const o=document.createElement('div');o.className='rmodal wpldlg';
 o.innerHTML=`<div class='rmbox' role='dialog' style='max-width:560px;width:94vw'><h3 class='wplt'></h3>
<div class='wplk' role='tablist' style='display:flex;gap:4px;flex-wrap:wrap;margin-bottom:8px'><button data-k='web'>🌐 Web address</button><button data-k='place'>📍 Place in this document</button><button data-k='file'>📄 File on this computer</button><button data-k='mail'>✉️ E-mail address</button></div>
<div class='r2' style='grid-template-columns:auto 1fr;gap:6px 10px'><label>Text to show</label><input class='wpltx'>
<label class='wk-web'>Address</label><input class='wk-web wplu' placeholder='https://example.com' spellcheck='false'>
<label class='wk-place'>Place</label><select class='wk-place wplp' size='6' style='height:auto'></select>
<label class='wk-file'>File</label><div class='wk-file' style='display:flex;gap:6px'><input class='wplf' readonly placeholder='Choose a file' style='flex:1;min-width:0'><button class='wplfc'>Choose…</button></div>
<label class='wk-mail'>E-mail address</label><input class='wk-mail wplm' placeholder='name@example.com' spellcheck='false'>
<label class='wk-mail'>Subject</label><input class='wk-mail wpls'>
<label>ScreenTip</label><input class='wpltip' placeholder='Shown when the pointer is on the link (optional)'></div>
<p class='msg wplerr' style='color:#b91c1c;min-height:1em'></p>
<div class='rmrow'>${cur?"<button class='wplrm'>Remove link</button>":''}<button class='wplx'>Cancel</button><button class='pri wplok'>OK</button></div></div>`;
 document.body.appendChild(o);typeof promptFront=='function'&&promptFront(o);
 const $=c=>o.querySelector(c),er=t=>{$('.wplerr').textContent=t||''};let kind=k0,fileHref='';
 $('.wplt').textContent=cur?'Edit hyperlink':'Insert hyperlink';
 const places=wplPlaces(X.ed);if(!places.length){const op=document.createElement('option');op.disabled=true;op.textContent='No headings or bookmarks yet (Insert › Bookmark…)';$('.wplp').appendChild(op)}
 places.forEach(pl=>{const op=document.createElement('option');op.value=pl.id;op.textContent=(pl.lvl?'\u00a0\u00a0'.repeat(pl.lvl-1):'')+pl.label;$('.wplp').appendChild(op)});
 const tab=k=>{kind=k;o.querySelectorAll('.wplk button').forEach(b=>{const on=b.dataset.k==k;b.classList.toggle('pri',on);b.setAttribute('aria-selected',on)});['web','place','file','mail'].forEach(x=>o.querySelectorAll('.wk-'+x).forEach(e=>e.style.display=x==k?'':'none'));er('')};
 o.querySelectorAll('.wplk button').forEach(b=>b.onclick=()=>tab(b.dataset.k));
 $('.wpltx').value=cur?cur.textContent:selText;$('.wpltip').value=cur?cur.getAttribute('title')||'':'';
 if(k0=='web')$('.wplu').value=h0||'https://';if(k0=='place')$('.wplp').value=h0.slice(1);
 if(k0=='file'){fileHref=h0;$('.wplf').value=(wplFile(h0)||[]).join('\\')}
 if(k0=='mail'){const m=/^mailto:([^?]*)(?:\?(.*))?$/i.exec(h0)||[];$('.wplm').value=decodeURIComponent(m[1]||'');$('.wpls').value=new URLSearchParams(m[2]||'').get('subject')||''}
 $('.wplfc').onclick=async()=>{if(typeof fileDlg!='function')return er('The file dialog is not available.');o.style.display='none';const p=await fileDlg({mode:'open',title:'Link to a file'});o.style.display='';if(p){fileHref='file:///'+p.map(encodeURIComponent).join('/').replace(/^C%3A/,'C:');$('.wplf').value=p.join('\\');if(!$('.wpltx').value.trim())$('.wpltx').value=p.at(-1)}};
 const close=()=>o.remove();
 const href=()=>{if(kind=='web'){let u=$('.wplu').value.trim();if(!u||u=='https://')return er('Type the web address.'),'';if(!/^[a-z][a-z0-9+.-]*:/i.test(u))u='https://'+u;if(!/^https?:\/\/[^\s/]+\.[^\s]+|^https?:\/\/localhost/i.test(u))return er('That is not a web address.'),'';return u}
  if(kind=='place'){const v=$('.wplp').value;if(!v)return er('Choose a heading or a bookmark.'),'';return'#'+v}
  if(kind=='file'){if(!fileHref)return er('Choose a file.'),'';return fileHref}
  const a=$('.wplm').value.trim();if(!/^[^\s@<>"]+@[^\s@<>"]+\.[^\s@<>"]+$/.test(a))return er('That is not an e-mail address.'),'';const sj=$('.wpls').value.trim();return'mailto:'+a+(sj?'?subject='+encodeURIComponent(sj):'')};
 $('.wplok').onclick=()=>{er('');const h=href();if(!h)return;if(!wplKind(h))return er('That address cannot be linked.');const tip=$('.wpltip').value.trim();
  let t=$('.wpltx').value;if(!t.trim())t=kind=='mail'?$('.wplm').value.trim():kind=='file'?$('.wplf').value.split('\\').pop():kind=='place'?($('.wplp').selectedOptions[0]||{}).textContent.replace(/^[\s\u00a0¶🔖]+/,''):h;
  close();X.rs();
  if(cur){cur.setAttribute('href',h);tip?cur.setAttribute('title',tip):cur.removeAttribute('title');if(t!==cur.textContent)cur.textContent=t}
  else if(selText&&t===selText){document.execCommand('createLink',false,h);const made=[...X.ed.querySelectorAll('a[href]')].filter(a=>a.getAttribute('href')==h&&!a.dataset.wpl);made.forEach(a=>{a.dataset.wpl=1;if(tip)a.title=tip});made.forEach(a=>delete a.dataset.wpl)}
  else X.insertHTML(`<a href="${wplEsc(h)}"${tip?` title="${wplEsc(tip)}"`:''}>${wplEsc(t)}</a>`);
  /* (the caret after the new link, so the next thing typed or inserted does not replace it) */
  try{const s2=getSelection();if(s2.rangeCount&&X.ed.contains(s2.anchorNode)){s2.collapseToEnd();let n=s2.anchorNode;while(n&&n!==X.ed&&!(n.nodeType==1&&n.tagName=='A'))n=n.parentNode;if(n&&n!==X.ed){const r2=document.createRange();r2.setStartAfter(n);r2.collapse(true);s2.removeAllRanges();s2.addRange(r2)}}}catch(e){}
  X.touch()};
 if(cur)$('.wplrm').onclick=()=>{close();wplUnlink(X,cur)};
 $('.wplx').onclick=close;o.addEventListener('keydown',e=>{e.stopPropagation();if(e.key=='Escape')close();else if(e.key=='Enter'&&e.target.tagName=='INPUT'&&!e.target.readOnly)$('.wplok').click()});
 tab(k0);setTimeout(()=>{(k0=='web'?$('.wplu'):$('.wpltx')).focus()},0);return o}
function wplUnlink(X,a){const f=document.createDocumentFragment();while(a.firstChild)f.appendChild(a.firstChild);a.replaceWith(f);X.touch()}
/* bookmarks */
function wplBookmarkDlg(X){X.rs();const o=document.createElement('div');o.className='rmodal wplbm';
 o.innerHTML=`<div class='rmbox' role='dialog' style='max-width:440px;width:92vw'><h3>Bookmark</h3><div class='r2' style='grid-template-columns:auto 1fr'><label>Name</label><input class='wbn' maxlength='40' placeholder='for example chapter-2'></div>
<p class='msg' style='margin:6px 0 2px'>Bookmarks in this document</p><div class='wbl' style='max-height:200px;overflow:auto;border:1px solid var(--bd);border-radius:3px'></div><p class='msg wberr' style='color:#b91c1c;min-height:1em'></p>
<div class='rmrow'><button class='wbx'>Close</button><button class='pri wbadd'>Add here</button></div></div>`;
 document.body.appendChild(o);typeof promptFront=='function'&&promptFront(o);const $=c=>o.querySelector(c);
 const list=()=>{const L=$('.wbl');L.innerHTML='';const bs=[...X.ed.querySelectorAll('a.wp-bm[id]')];if(!bs.length){const p=document.createElement('p');p.className='msg';p.style.padding='6px';p.textContent='None yet.';L.appendChild(p)}
  bs.forEach(b=>{const r=document.createElement('div');r.style.cssText='display:flex;gap:6px;align-items:center;padding:3px 6px';const n=document.createElement('span');n.style.flex='1';n.textContent='🔖 '+(b.dataset.name||b.id);
   const g=document.createElement('button');g.textContent='Go to';g.onclick=()=>{o.remove();b.scrollIntoView({behavior:'smooth',block:'center'})};const d=document.createElement('button');d.textContent='Delete';d.onclick=()=>{b.remove();X.touch();list()};r.append(n,g,d);L.appendChild(r)})};
 $('.wbadd').onclick=()=>{const n=$('.wbn').value.trim();if(!/^[A-Za-z][\w -]{0,39}$/.test(n))return $('.wberr').textContent='A name starts with a letter: letters, digits, spaces, - and _.';
  const id='bm-'+wplSlug(n);if(X.ed.querySelector('#'+CSS.escape(id)))return $('.wberr').textContent='A bookmark has that name.';
  o.remove();X.rs();X.insertHTML(`<a class="wp-bm" id="${wplEsc(id)}" data-name="${wplEsc(n)}" contenteditable="false" title="Bookmark: ${wplEsc(n)}"></a>`);X.touch();X.say('Bookmark "'+n+'" added.')};
 $('.wbx').onclick=()=>o.remove();o.addEventListener('keydown',e=>{e.stopPropagation();if(e.key=='Escape')o.remove();if(e.key=='Enter'&&e.target.classList.contains('wbn'))$('.wbadd').click()});list();$('.wbn').focus();return o}
/* every link */
function wplList(X){const o=document.createElement('div');o.className='rmodal wplls';
 o.innerHTML=`<div class='rmbox' role='dialog' style='max-width:640px;width:94vw'><h3>Links in this document</h3><div class='wll' style='max-height:50vh;overflow:auto;border:1px solid var(--bd);border-radius:3px'></div>
<label style='display:block;margin-top:8px'><input type='checkbox' class='wlauto'> Turn typed web and e-mail addresses into links</label>
<div class='rmrow'><button class='wlrmall'>Remove all links</button><button class='pri wlx'>Close</button></div></div>`;
 document.body.appendChild(o);typeof promptFront=='function'&&promptFront(o);const $=c=>o.querySelector(c);
 const draw=()=>{const L=$('.wll');L.innerHTML='';const as=[...X.ed.querySelectorAll('a[href]')].filter(a=>!a.classList.contains('wp-bm'));
  if(!as.length){const p=document.createElement('p');p.className='msg';p.style.padding='8px';p.textContent='This document has no links.';L.appendChild(p)}
  as.forEach(a=>{const h=a.getAttribute('href')||'',k=wplKind(h);const bad=k=='place'?!wplTarget(X.ed,h):k=='file'?!(wplFile(h)&&node(wplFile(h))!==undefined):!k;
   const r=document.createElement('div');r.className='wlr';r.style.cssText='display:flex;gap:6px;align-items:center;padding:4px 8px;border-bottom:1px solid var(--bd)';
   const tx=document.createElement('div');tx.style.cssText='flex:1;min-width:0';const b=document.createElement('b');b.textContent=a.textContent||'(no text)';const u=document.createElement('div');u.style.cssText='font-size:11px;color:#6b7280;overflow:hidden;text-overflow:ellipsis;white-space:nowrap';u.textContent=(bad?'⚠ not found: ':'')+({web:'🌐 ',place:'📍 ',file:'📄 ',mail:'✉️ '}[k]||'')+h;tx.append(b,u);
   const bt=(l,f)=>{const x=document.createElement('button');x.textContent=l;x.onclick=f;return x};
   r.append(tx,bt('Go to',()=>{o.remove();a.scrollIntoView({behavior:'smooth',block:'center'});const rg=document.createRange();rg.selectNodeContents(a);const s=getSelection();s.removeAllRanges();s.addRange(rg)}),bt('Edit…',()=>{o.remove();wplDialog(X,a)}),bt('Remove',()=>{wplUnlink(X,a);draw()}));L.appendChild(r)})};
 $('.wlauto').checked=wplAutoOn();$('.wlauto').onchange=()=>{try{localStorage.setItem(WPL_AUTO,$('.wlauto').checked?'1':'0')}catch(e){}};
 $('.wlrmall').onclick=()=>{[...X.ed.querySelectorAll('a[href]')].filter(a=>!a.classList.contains('wp-bm')).forEach(a=>wplUnlink(X,a));draw()};
 $('.wlx').onclick=()=>o.remove();o.addEventListener('keydown',e=>{e.stopPropagation();if(e.key=='Escape')o.remove()});draw();return o}
/* typed addresses: the word before the caret, when a space or Enter follows it */
function wplAuto(X,e){if(!wplAutoOn()||!(e.key==' '||e.key=='Enter'))return;const s=getSelection();if(!s.rangeCount||!s.isCollapsed)return;const n=s.anchorNode;if(!n||n.nodeType!=3)return;
 let p=n.parentNode;while(p&&p!==X.ed){if(p.tagName=='A')return;p=p.parentNode}
 const before=n.data.slice(0,s.anchorOffset);const m=/(^|[\s(])((?:https?:\/\/|www\.)[^\s<>"]+[^\s<>".,;:!?)\]]|[^\s@<>"(]+@[^\s@<>"]+\.[A-Za-z]{2,})$/.exec(before);if(!m)return;
 const word=m[2],start=s.anchorOffset-word.length;const h=/@/.test(word)&&!/^(https?:|www\.)/i.test(word)?'mailto:'+word:/^www\./i.test(word)?'https://'+word:word;if(!wplKind(h))return;
 const r=document.createRange();r.setStart(n,start);r.setEnd(n,s.anchorOffset);s.removeAllRanges();s.addRange(r);document.execCommand('createLink',false,h);
 s.collapseToEnd();X.touch()}
/* the right-click menu of a link */
function wplMenu(X,a,e){e.preventDefault();e.stopPropagation();const h=a.getAttribute('href')||'';
 cmenu(e.clientX,e.clientY,[['Open link',()=>wplOpen(X,a),true,{icon:'↗',key:'Ctrl+click'}],['Edit link…',()=>wplDialog(X,a),true,{icon:'✏️',key:'Ctrl+K'}],['Copy link address',()=>{try{navigator.clipboard.writeText(h)}catch(x){}X.say('Link address copied.')},true,{icon:'📋'}],['Remove link',()=>wplUnlink(X,a),true,{icon:'⛓'}]],{title:a.textContent.slice(0,40)||'Link'})}
/* put into a WordPro window (W.X: its editor and helpers) */
function wplInstall(W){const X=W&&W.X;if(!X||X.__wpl)return;X.__wpl=1;const ed=X.ed;
 if(!document.getElementById('wpl59css')){const st=document.createElement('style');st.id='wpl59css';st.textContent='.wp-bm{display:inline-block;width:0;height:1em;border-left:2px dotted #a855f7;margin:0 1px;vertical-align:text-bottom}@media print{.wp-bm{display:none}}.wpl-flash{outline:2px solid #f59e0b;outline-offset:2px;transition:outline-color .8s}';document.head.appendChild(st)}
 ed.addEventListener('contextmenu',e=>{const a=e.target.closest&&e.target.closest('a[href]');if(a&&ed.contains(a)&&!a.classList.contains('wp-bm'))wplMenu(X,a,e)});
 ed.addEventListener('keydown',e=>{try{wplAuto(X,e)}catch(x){}});
 /* Ctrl+click: every kind of link (WordPro's own handler opened only web addresses and places) */
 ed.addEventListener('click',e=>{const a=e.target.closest&&e.target.closest('a[href]');if(!a||!(e.ctrlKey||e.metaKey))return;e.preventDefault();e.stopImmediatePropagation();wplOpen(X,a)},true);
 X.linkDialog=()=>wplDialog(X);X.bookmarkDialog=()=>wplBookmarkDlg(X);X.linksList=()=>wplList(X)}
if(typeof wordpro=='function'){const _wp59l=wordpro;wordpro=function(){const W=_wp59l.apply(this,arguments);try{wplInstall(W||wins.at(-1))}catch(e){console.warn('wplinks59',e)}return W}}
