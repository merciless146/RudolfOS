/* ===== V25 WordPro additions: Review menu (comments, read aloud, readability, word goal, versions),
 * mail merge, Markdown/PDF export, autosave, focus and reading modes, paragraph numbers, watermark,
 * text boxes, checklists, format painter, sorting and text shortcuts ===== */
function wpWmCss(meta){const base=`ul.wp-chk{list-style:none;padding-left:4pt}ul.wp-chk>li{position:relative;padding-left:20px}ul.wp-chk>li::before{content:'☐';position:absolute;left:0}ul.wp-chk>li.done::before{content:'☑'}ul.wp-chk>li.done{text-decoration:line-through;color:#6b7280}.wp-cm{background:none;border:0}.wp-mf{color:inherit}`;if(!meta||!meta.wm)return base;const t=String(meta.wm).replace(/["\\<>]/g,'');
 return base+`body::after{content:"${t}";position:fixed;top:42%;left:0;right:0;text-align:center;font:700 88pt Arial,sans-serif;color:rgba(0,0,0,.08);transform:rotate(-30deg);pointer-events:none;z-index:0}`}
function wpCsv(t){const rows=[];let row=[],f='',q=false;t=String(t).replace(/\r/g,'');
 for(let i=0;i<t.length;i++){const c=t[i];if(q){if(c=='"'){if(t[i+1]=='"'){f+='"';i++}else q=false}else f+=c}
  else if(c=='"')q=true;else if(c==','||c==';'&&!t.includes(',')){row.push(f);f=''}else if(c=='\n'){row.push(f);rows.push(row);row=[];f=''}else f+=c}
 if(f||row.length){row.push(f);rows.push(row)}return rows.filter(r=>r.some(x=>x.trim()))}
function wpToMd(root){const inl=n=>{let o='';n.childNodes.forEach(c=>{if(c.nodeType==3){o+=c.nodeValue.replace(/([*_`\\])/g,'\\$1');return}if(c.nodeType!=1)return;const t=c.tagName,s=inl(c);
   if(t=='B'||t=='STRONG'||c.style.fontWeight>=600||c.style.fontWeight=='bold')o+=s.trim()?`**${s}**`:s;else if(t=='I'||t=='EM'||c.style.fontStyle=='italic')o+=s.trim()?`*${s}*`:s;else if(t=='S'||t=='STRIKE'||t=='DEL')o+=`~~${s}~~`;
   else if(t=='A')o+=`[${s}](${c.getAttribute('href')||''})`;else if(t=='IMG')o+=`![${c.alt||''}](image)`;else if(t=='BR')o+='  \n';else if(t=='CODE')o+='`'+c.textContent+'`';else if(t=='SUP'&&c.classList.contains('wp-fn'))o+=`[^${c.textContent}]`;else o+=s});return o};
 const out=[];[...root.children].forEach(b=>{const t=b.tagName;if(b.classList.contains('wp-pb')){out.push('\n---\n');return}
  if(/^H[1-6]$/.test(t))out.push('#'.repeat(+t[1])+' '+inl(b).trim());
  else if(t=='UL'||t=='OL'){const chk=b.classList.contains('wp-chk');[...b.children].forEach((li,i)=>out.push((chk?`- [${li.classList.contains('done')?'x':' '}] `:t=='OL'?`${i+1}. `:'- ')+inl(li).trim()))}
  else if(t=='BLOCKQUOTE'||b.classList.contains('quote'))out.push('> '+inl(b).trim());else if(t=='HR')out.push('---');
  else if(t=='TABLE'){const rs=[...b.rows].map(r=>'| '+[...r.cells].map(c=>inl(c).trim().replace(/\|/g,'\\|')).join(' | ')+' |');if(rs.length){out.push(rs[0]);out.push('|'+' --- |'.repeat(b.rows[0].cells.length));out.push(...rs.slice(1))}}
  else if(t=='PRE')out.push('```\n'+b.textContent+'\n```');else{const s=inl(b).trim();out.push(s)}out.push('')});
 return out.join('\n').replace(/\n{3,}/g,'\n\n').trim()+'\n'}
function wpReadability(text){const sents=Math.max(1,(text.match(/[.!?]+(\s|$)/g)||[]).length),ws=text.match(/[A-Za-zÀ-ÿ'’]+/g)||[],W=Math.max(1,ws.length);
 const syl=w=>{w=w.toLowerCase().replace(/[^a-z]/g,'');if(w.length<=3)return 1;w=w.replace(/(?:[^laeiouy]es|ed|[^laeiouy]e)$/,'').replace(/^y/,'');const m=w.match(/[aeiouy]{1,2}/g);return Math.max(1,m?m.length:1)};
 const S=ws.reduce((a,w)=>a+syl(w),0),fre=206.835-1.015*(W/sents)-84.6*(S/W),gr=0.39*(W/sents)+11.8*(S/W)-15.59,long=ws.filter(w=>syl(w)>=3).length;
 return{words:ws.length,sentences:sents,avgWords:W/sents,ease:fre,grade:Math.max(0,gr),complex:long/W}}
function wpExtend(W,MENUS,K){const ed=()=>K.ed,root=()=>K.root,esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
 const st={focus:false,ln:false,read:false,auto:ld('bd95wpauto',false),painter:null,goalHit:false,wasDirty:false,path:null,cmp:null};
 const dlg=(t,h,b,o)=>K.dialog(t,h,b,o);
 const selRange=()=>{const s=getSelection();return s.rangeCount&&ed().contains(s.getRangeAt(0).commonAncestorContainer)?s.getRangeAt(0):null};
 const topBlock=n=>{while(n&&n.parentNode!==ed())n=n.parentNode;return n};
 /* ---------- comments ---------- */
 function addComment(){K.rs();const r=selRange();if(!r||r.collapsed){K.say('Select the text you want to comment on first.');return}
  dlg('New comment',`<textarea class='wpc-t' rows='4' style='width:100%;box-sizing:border-box' placeholder='Your comment'></textarea>`,[['Cancel'],['Add comment',o=>{const t=o.querySelector('.wpc-t').value.trim();if(!t)return;
   const b1=topBlock(r.startContainer),b2=topBlock(r.endContainer);if(b1&&b2&&b1!==b2)r.setEndAfter(b1.lastChild||b1);
   const sp=document.createElement('span');sp.className='wp-cm';sp.dataset.c=t;sp.dataset.a=USER?USER.n:'Me';sp.dataset.t=new Date().toISOString();sp.title=t+' — '+sp.dataset.a;
   sp.appendChild(r.extractContents());r.insertNode(sp);K.touch();K.sched();showComments(true)},1]],o=>setTimeout(()=>o.querySelector('.wpc-t').focus(),30))}
 function showComments(on){const mid=K.q('.wp-mid');if(!mid)return;if(on===undefined)on=!st.cmp;if(!on){st.cmp&&st.cmp.remove();st.cmp=null;return}
  if(!st.cmp){st.cmp=document.createElement('div');st.cmp.className='wp-cmp';mid.appendChild(st.cmp)}drawComments()}
 function drawComments(){if(!st.cmp)return;const L=[...ed().querySelectorAll('.wp-cm')];
  st.cmp.innerHTML=`<div class='wp-cmh'><b>Comments (${L.length})</b><button class='x' title='Close'>✕</button></div>`+(L.length?'':`<p class='msg'>No comments yet. Select some text and choose Review → New comment.</p>`);
  st.cmp.querySelector('.x').onclick=()=>showComments(false);
  L.forEach(sp=>{const d=document.createElement('div');d.className='wp-cmi';const t=new Date(sp.dataset.t||Date.now());
   d.innerHTML=`<div class='wp-cma'>${esc(sp.dataset.a||'')} · ${t.toLocaleDateString()}</div><div class='wp-cmq'>“${esc(sp.textContent.slice(0,60))}”</div><div class='wp-cmt'></div><div class='wp-cmb'><button class='e'>Edit</button><button class='d'>Resolve</button></div>`;
   d.querySelector('.wp-cmt').textContent=sp.dataset.c||'';
   d.onclick=e=>{if(e.target.tagName=='BUTTON')return;const r=document.createRange();r.selectNodeContents(sp);const s=getSelection();s.removeAllRanges();s.addRange(r);sp.scrollIntoView({block:'center'})};
   d.querySelector('.e').onclick=()=>dlg('Edit comment',`<textarea class='wpc-t' rows='4' style='width:100%;box-sizing:border-box'>${esc(sp.dataset.c||'')}</textarea>`,[['Cancel'],['Save',o=>{sp.dataset.c=o.querySelector('.wpc-t').value;sp.title=sp.dataset.c;K.touch();drawComments()},1]]);
   d.querySelector('.d').onclick=()=>{sp.replaceWith(...sp.childNodes);K.touch();K.sched();drawComments()};st.cmp.appendChild(d)})}
 function nextComment(){const L=[...ed().querySelectorAll('.wp-cm')];if(!L.length){K.say('There are no comments.');return}const r=selRange();let i=0;if(r)i=L.findIndex(x=>r.compareBoundaryPoints(Range.START_TO_START,(()=>{const q=document.createRange();q.selectNode(x);return q})())<0);if(i<0)i=0;
  const x=L[i],q=document.createRange();q.selectNodeContents(x);getSelection().removeAllRanges();getSelection().addRange(q);x.scrollIntoView({block:'center'})}
 /* ---------- read aloud ---------- */
 function readAloud(){if(st.read){st.read=false;ttsStop();return}const r=selRange(),t=(r&&!r.collapsed?r.toString():K.docText()).trim();if(!t)return;
  const lang=ttsGuess(t);if(!ttsEngine(lang)){K.say('There is no voice for this language on this computer.');return}
  const parts=t.match(/[^.!?。！？\n]+[.!?。！？]*\s*/g)||[t];st.read=true;let i=0;
  const next=ok=>{if(ok===false||!st.read||i>=parts.length){st.read=false;return}const p=parts[i++];if(!p.trim())return next();ttsSpeak(p,{lang,queue:i>1,onend:next})};next()}
 /* ---------- readability ---------- */
 function readability(){const r=selRange(),t=(r&&!r.collapsed?r.toString():K.docText());const R=wpReadability(t);if(R.words<5){K.say('Write a few sentences first.');return}
  const lvl=R.ease>=80?'Very easy — conversational':R.ease>=65?'Easy — plain English':R.ease>=50?'Fairly difficult — like a newspaper':R.ease>=30?'Difficult — academic':'Very difficult — specialist';
  dlg('Readability',`<table class='wp-stt'><tr><td>Reading ease (0–100)</td><td><b>${R.ease.toFixed(0)}</b> · ${lvl}</td></tr><tr><td>School grade level</td><td>${R.grade.toFixed(1)}</td></tr><tr><td>Words / sentences</td><td>${R.words} / ${R.sentences}</td></tr><tr><td>Average sentence</td><td>${R.avgWords.toFixed(1)} words${R.avgWords>22?' — try shorter sentences':''}</td></tr><tr><td>Long words (3+ syllables)</td><td>${(R.complex*100).toFixed(0)}%</td></tr></table><p class='msg'>${r&&!r.collapsed?'Measured on the selected text.':'Measured on the whole document.'} Higher reading ease means easier to read.</p>`,[['Close',null,1]])}
 /* ---------- word count goal ---------- */
 function goalDlg(){const m=K.meta;dlg('Word count goal',`<label>Target number of words <input type='number' class='g' min='0' step='50' value='${m.goal||''}' style='width:110px'></label><p class='msg'>Progress is shown in the status bar. Leave empty to remove the goal.</p>`,[['Cancel'],['Set goal',o=>{const v=+o.querySelector('.g').value||0;if(v)m.goal=v;else delete m.goal;st.goalHit=false;K.touch();goalUpd()},1]])}
 function goalUpd(){const sb=K.q('.wp-sb');if(!sb)return;let g=sb.querySelector('.wp-goal');const m=K.meta;if(!m.goal){g&&g.remove();return}
  if(!g){g=document.createElement('span');g.className='wp-goal';g.title='Word count goal (Review → Word count goal)';g.onclick=goalDlg;const ref=sb.querySelector('.sc');ref?ref.after(g):sb.appendChild(g)}
  const n=K.words(K.docText()),pc=Math.min(100,Math.round(n/m.goal*100));g.innerHTML=`<i style='width:${pc}%'></i><span>${n} / ${m.goal}</span>`;g.classList.toggle('done',pc>=100);
  if(pc>=100&&!st.goalHit){st.goalHit=true;try{W.notey&&W.notey.visible()&&W.notey.say(`You reached your goal of <b>${m.goal}</b> words!`,[['Thanks!',()=>W.notey.hide()]],{mood:'happy',passive:true,ms:8000})}catch(e){}}}
 /* ---------- version history (kept for every save) ---------- */
 const vkey=()=>'wpv:'+(K.path?K.path.join('\\'):'');
 async function snapshot(){st.saved=1;if(!K.path||typeof kvGet!='function')return;const n=node(K.path);if(typeof n!='string')return;let L=(await kvGet(vkey()))||[];if(L.length&&L[0].h===n)return;L.unshift({t:Date.now(),h:n,w:K.words(K.docText())});L=L.slice(0,20);kvSet(vkey(),L)}
 async function history(){if(!K.path){K.say('Save the document first. Every save is then kept in the version history.');return}const L=(await kvGet(vkey()))||[];
  dlg('Version history — '+K.path.at(-1),L.length?`<div class='wp-vh'>${L.map((v,i)=>`<div class='wp-vr'><span>${new Date(v.t).toLocaleString()}${i==0?' (latest save)':''}<br><small>${v.w} words</small></span><button data-i='${i}'>Open copy</button></div>`).join('')}</div><p class='msg'>A copy opens in a new window, so nothing is overwritten. Save it to keep it.</p>`:`<p>No saved versions yet.</p>`,[['Close',null,1]],
   o=>o.querySelectorAll('button[data-i]').forEach(b=>b.onclick=()=>{const v=L[+b.dataset.i];o.remove();wordpro(null,v.h)}))}
 /* ---------- mail merge ---------- */
 function mergeField(){dlg('Insert merge field',`<label>Field name (a column heading in your CSV file)<br><input class='mff' placeholder='FirstName' style='width:100%'></label>`,[['Cancel'],['Insert',o=>{const f=o.querySelector('.mff').value.trim().replace(/[«»]/g,'');if(f){K.rs();K.insertHTML(`<span class='wp-mf' contenteditable='false'>«${esc(f)}»</span>&#8203;`)}},1]],o=>setTimeout(()=>o.querySelector('.f').focus(),30))}
 async function mailMerge(){const p=await pickVfsFile('Choose the data file (.CSV)',n=>/\.(CSV|TXT)$/i.test(n));if(!p)return;const rows=wpCsv(node(p)||'');if(rows.length<2){K.say('That file needs a heading row and at least one data row.');return}
  const H=rows[0].map(h=>h.trim()),data=rows.slice(1),tpl=ed().innerHTML,used=[...new Set([...tpl.matchAll(/«([^»]+)»/g)].map(m=>m[1]))],miss=used.filter(f=>!H.includes(f));
  dlg('Mail merge',`<p>${data.length} record${data.length>1?'s':''} found with the columns <b>${esc(H.join(', '))}</b>.</p>${used.length?'':`<p class='msg'>This document has no merge fields yet. Use Insert → Merge field… to add «FirstName» style fields.</p>`}${miss.length?`<p class='msg'>Not in the data file: ${esc(miss.join(', '))} (left blank).</p>`:''}`,
   [['Cancel'],['Create merged document',()=>{const page="<div class='wp-pb' contenteditable='false'></div>";const out=data.map(r=>tpl.replace(/<span class="wp-mf"[^>]*>«([^»]+)»<\/span>|«([^»]+)»/g,(m,a,b)=>{const i=H.indexOf(a||b);return esc(i<0?'':(r[i]||'').trim())})).join(page);
    const meta=Object.assign({},K.meta);wordpro(null,`<!--wp:${encodeURIComponent(JSON.stringify(meta))}-->`+out)},1]])}
 /* ---------- exports ---------- */
 function exportMd(){const nm=(K.path?K.path.at(-1).replace(/\.[^.]*$/,''):'DOCUMENT')+'.MD';saveVfsDlg(nm).then(p=>{if(!p)return;vfsWrite(p,wpToMd(ed()));K.say('Exported '+p.join('\\'))})}
 async function exportPdf(){const nm=(K.path?K.path.at(-1).replace(/\.[^.]*$/,''):'DOCUMENT');try{await savePdf(nm+'.PDF',pdfFromText(nm,K.docText()))}catch(e){K.say('PDF export failed: '+e.message)}}
 /* ---------- view modes ---------- */
 let focusStyle=null;function focusMode(on){st.focus=on===undefined?!st.focus:on;root().classList.toggle('wp-focusm',st.focus);if(!focusStyle){focusStyle=document.createElement('style');root().appendChild(focusStyle)}if(!st.focus)focusStyle.textContent='';else focusCur();if(st.focus)K.say('Focus mode — press Esc to leave.');K.layout()}
 function focusCur(){if(!st.focus||!focusStyle)return;const r=selRange(),b=r&&topBlock(r.startContainer);const i=b?[...ed().children].indexOf(b)+1:1;focusStyle.textContent=`.wp-focusm .wp-ed>*:not(:nth-child(${i})){opacity:.28}`}
 function readingMode(){const o=document.createElement('div');o.className='wp-readm';o.innerHTML=`<div class='wp-readb'><button class='x'>✕ Close reading view</button><button class='a'>A−</button><button class='b'>A+</button></div><article></article>`;
  const a=o.querySelector('article');a.innerHTML=ed().innerHTML;a.querySelectorAll('[contenteditable]').forEach(e=>e.removeAttribute('contenteditable'));let fs=19;a.style.fontSize=fs+'px';
  o.querySelector('.a').onclick=()=>{fs=Math.max(13,fs-2);a.style.fontSize=fs+'px'};o.querySelector('.b').onclick=()=>{fs=Math.min(34,fs+2);a.style.fontSize=fs+'px'};
  const close=()=>{o.remove();ed().focus()};o.querySelector('.x').onclick=close;o.tabIndex=-1;o.addEventListener('keydown',e=>{if(e.key=='Escape'){e.stopPropagation();close()}});root().appendChild(o);o.focus()}
 const applyView=()=>{const sh=K.sheet;root().classList.toggle('wp-lnm',!!st.ln);if(sh){if(K.meta.wm)sh.dataset.wm=K.meta.wm;else delete sh.dataset.wm}};
 function watermark(){dlg('Watermark',`<label>Text <input class='w' list='wpwm' value='${esc(K.meta.wm||'')}' style='width:200px'></label><datalist id='wpwm'><option>DRAFT</option><option>CONFIDENTIAL</option><option>SAMPLE</option><option>COPY</option><option>DO NOT COPY</option></datalist><p class='msg'>Shown faintly across every page, on screen, when printing and in exported web pages. Leave empty for none.</p>`,
  [['Remove',()=>{delete K.meta.wm;K.touch();applyView()}],['Cancel'],['Apply',o=>{const v=o.querySelector('.w').value.trim().slice(0,30);if(v)K.meta.wm=v.toUpperCase();else delete K.meta.wm;K.touch();applyView()},1]])}
 /* ---------- inserting ---------- */
 const textBox=()=>{K.rs();K.insertHTML(`<div class='wp-tbx' style='float:right;width:42%;border:1.5px solid #374151;border-radius:4px;padding:6pt 8pt;margin:4pt 0 6pt 10pt;background:#f8fafc'><p><b>Text box</b></p><p>Type here.</p></div><p><br></p>`)};
 const callout=()=>{K.rs();K.insertHTML(`<div class='wp-call' style='border-left:5px solid #2563eb;background:#eff6ff;padding:6pt 10pt;margin:6pt 0'><p><b>Note:</b> Type your note here.</p></div><p><br></p>`)};
 const checklist=()=>{K.rs();K.insertHTML(`<ul class='wp-chk'><li>First item</li><li>Second item</li></ul><p><br></p>`)};
 /* ---------- format painter ---------- */
 function copyFormat(){const r=selRange();if(!r){K.say('Put the cursor in the text whose formatting you want to copy.');return}let n=r.startContainer;if(n.nodeType==3)n=n.parentElement;const cs=getComputedStyle(n);
  st.painter={b:+cs.fontWeight>=600,i:cs.fontStyle=='italic',u:/underline/.test(cs.textDecorationLine),s:/line-through/.test(cs.textDecorationLine),c:cs.color,f:cs.fontFamily.split(',')[0].replace(/["']/g,''),h:(()=>{let e=n;while(e&&e!==ed()){const bg=e.style&&e.style.backgroundColor;if(bg)return bg;e=e.parentElement}return''})()};
  root().classList.add('wp-painting');K.say('Formatting copied. Now select other text and choose Format → Paste formatting.')}
 function pasteFormat(){const P=st.painter;if(!P){K.say('Copy formatting first (Format → Copy formatting).');return}K.rs();const r=selRange();if(!r||r.collapsed){K.say('Select the text to format.');return}
  const q=c=>document.queryCommandState(c);[['bold',P.b],['italic',P.i],['underline',P.u],['strikeThrough',P.s]].forEach(([c,v])=>{if(q(c)!==v)document.execCommand(c)});
  document.execCommand('foreColor',false,P.c);if(P.f)document.execCommand('fontName',false,P.f);if(P.h)document.execCommand('hiliteColor',false,P.h);K.touch();K.sched()}
 /* ---------- sorting ---------- */
 function sortParas(desc){K.rs();const r=selRange();let L;if(r&&!r.collapsed){L=[...ed().children].filter(b=>r.intersectsNode(b))}const li=r&&(r.startContainer.nodeType==3?r.startContainer.parentElement:r.startContainer).closest('li');
  if((!L||L.length<2)&&li){L=[...li.parentElement.children]}if(!L||L.length<2){K.say('Select two or more paragraphs (or put the cursor in a list) to sort.');return}
  const par=L[0].parentNode,anchor=L[L.length-1].nextSibling,sorted=L.slice().sort((a,b)=>a.textContent.trim().localeCompare(b.textContent.trim(),undefined,{numeric:true,sensitivity:'base'}));if(desc)sorted.reverse();
  sorted.forEach(b=>par.insertBefore(b,anchor));K.touch();K.sched()}
 /* ---------- text shortcuts (auto-text) ---------- */
 const AT=()=>ld('bd95wpat',{'(c)':'©','(r)':'®','(tm)':'™','->':'→','<-':'←','--':'—','1/2':'½','brb':'be right back','afaik':'as far as I know','omw':'on my way'});
 function shortcutsDlg(){const m=AT();dlg('Text shortcuts',`<p class='msg'>When you type a shortcut and then a space, WordPro replaces it.</p><textarea class='t' rows='10' style='width:100%;box-sizing:border-box;font-family:monospace'>${esc(Object.entries(m).map(([k,v])=>k+' = '+v).join('\n'))}</textarea><label><input type='checkbox' class='on' ${ld('bd95wpaton',true)?'checked':''}> Replace shortcuts while typing</label>`,
  [['Cancel'],['Save',o=>{const r={};o.querySelector('.t').value.split('\n').forEach(l=>{const i=l.indexOf(' = ');if(i>0)r[l.slice(0,i).trim()]=l.slice(i+3)});svj('bd95wpat',r);svj('bd95wpaton',o.querySelector('.on').checked)},1]])}
 ed().addEventListener('keydown',e=>{if(e.key==' '&&!e.ctrlKey&&!e.metaKey&&ld('bd95wpaton',true)){const s=getSelection();if(!s.rangeCount)return;const r=s.getRangeAt(0);if(!r.collapsed||r.startContainer.nodeType!=3)return;
   const before=r.startContainer.nodeValue.slice(0,r.startOffset),m=/(\S+)$/.exec(before);if(!m)return;const map=AT(),key=Object.keys(map).find(k=>k.toLowerCase()==m[1].toLowerCase());if(!key)return;
   const q=document.createRange();q.setStart(r.startContainer,r.startOffset-m[1].length);q.setEnd(r.startContainer,r.startOffset);s.removeAllRanges();s.addRange(q);e.preventDefault();document.execCommand('insertText',false,map[key]+' ')}
  if(e.key=='Escape'&&st.focus){focusMode(false)}
  if((e.ctrlKey||e.metaKey)&&e.altKey&&e.key.toLowerCase()=='m'){e.preventDefault();addComment()}});
 ed().addEventListener('click',e=>{const li=e.target.closest&&e.target.closest('ul.wp-chk>li');if(li){const r=li.getBoundingClientRect();if(e.clientX-r.left<24){li.classList.toggle('done');K.touch();K.sched()}}});
 /* ---------- autosave and housekeeping ---------- */
 document.addEventListener('selectionchange',()=>{if(st.focus&&W.el.isConnected)focusCur()});
 const hk=setInterval(()=>{if(!W.el.isConnected){clearInterval(hk);try{if(st.read){st.read=false;ttsStop()}}catch(e){}return}applyView();goalUpd();if(st.cmp)drawComments();
  if(!K.dirty&&K.path){const n=node(K.path);if(typeof n=='string'&&n!==st.lastH){st.lastH=n;snapshot()}}},1500);
 let lastAuto=Date.now();const as=setInterval(()=>{if(!W.el.isConnected){clearInterval(as);return}if(st.auto&&K.path&&K.dirty&&Date.now()-lastAuto>110000){lastAuto=Date.now();try{K.doSave()}catch(e){}}},10000);
 /* ---------- menus ---------- */
 const tg=f=>()=>{f()};
 const F=MENUS.File;MENUS.File=()=>{const a=F();const i=a.findIndex(x=>x[0]&&/^Export as web page/.test(x[0]));a.splice(i+1,0,['Export as Markdown (.MD)…','',exportMd],['Export as PDF (text only)…','',exportPdf]);
  const j=a.findIndex(x=>x[0]=='Save As…');a.splice(j+1,0,['Autosave every 2 minutes','',tg(()=>{st.auto=!st.auto;svj('bd95wpauto',st.auto);K.say(st.auto?'Autosave is on (for documents that have been saved once).':'Autosave is off.')}),st.auto],['Version history…','',history]);
  const k=a.findIndex(x=>x[0]=='Page setup…');a.splice(k,0,['Mail merge…','',mailMerge]);return a};
 const V=MENUS.View;MENUS.View=()=>{const a=V();a.splice(2,0,['Focus mode','',()=>focusMode(),st.focus],['Reading view','',readingMode]);const i=a.findIndex(x=>x[0]&&/^Show formatting marks/.test(x[0]));a.splice(i,0,['Paragraph numbers','',tg(()=>{st.ln=!st.ln;applyView()}),st.ln]);return a};
 const I=MENUS.Insert;MENUS.Insert=()=>{const a=I();const i=a.findIndex(x=>x[0]=='Horizontal line');a.splice(i,0,['Text box','',textBox],['Callout box','',callout],['Checklist','',checklist]);
  const j=a.findIndex(x=>x[0]=='Special characters…');a.splice(j+1,0,['Merge field…','',mergeField]);a.push('-',['Watermark…','',watermark]);return a};
 const Fo=MENUS.Format;MENUS.Format=()=>{const a=Fo();const i=a.findIndex(x=>x[0]=='Clear formatting');a.splice(i+1,0,['Copy formatting','',copyFormat],['Paste formatting','',pasteFormat]);a.push('-',['Sort paragraphs A → Z','',()=>sortParas(false)],['Sort paragraphs Z → A','',()=>sortParas(true)]);return a};
 const T=MENUS.Tools;MENUS.Tools=()=>{const a=T();a.splice(2,0,['Readability…','',readability],['Word count goal…','',goalDlg]);a.push(['Text shortcuts…','',shortcutsDlg]);return a};
 const keys=Object.keys(MENUS),R={Review:()=>[['New comment','Ctrl+Alt+M',addComment],['Show comments','',()=>showComments(),!!st.cmp],['Next comment','',nextComment],['Resolve all comments','',()=>{ed().querySelectorAll('.wp-cm').forEach(s=>s.replaceWith(...s.childNodes));K.touch();drawComments()}],'-',
  [st.read?'Stop reading aloud':'Read aloud','',readAloud],['Readability…','',readability],['Word count goal…','',goalDlg],'-',['Version history…','',history],['Check my writing (Notey)','',()=>W.notey&&W.notey.check()]]};
 const NM={};keys.forEach(k=>{NM[k]=MENUS[k];if(k=='Table')NM.Review=R.Review});Object.keys(MENUS).forEach(k=>delete MENUS[k]);Object.assign(MENUS,NM);
 setTimeout(()=>{applyView();goalUpd()},200)}
