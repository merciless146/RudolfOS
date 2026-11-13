/* ===== 1.5.9: Search in Settings. A search box at the top of the Settings window; as each letter is typed, a drop-down
   lists the pages, sections and settings that still match (every word typed must begin a word of the setting, its
   section or its page, so each letter only takes results away), each with its place underneath in smaller text
   ("Settings › Network Settings › Wi-Fi"). Enter or a click opens the page and shows the setting there.
   What can be found: SETIDX, the pages' sections and settings as the build found them (desktop/settings-index.json,
   made by iso/qt/shell2/setsearch59_t.js --write), and each page again as it is shown (pages change with the
   computer: a modem, a second screen), so the search follows what this computer's pages really have. ===== */
const SS_KEYS={disp:'screen monitor brightness wallpaper theme dark mode resolution scaling night light colours icons',vol:'sound audio speakers microphone headset volume echo calls',
 mm:'multimedia games controller video',net:'network wi-fi wifi internet ethernet hotspot proxy cellular modem connection',bt:'bluetooth headphones headset pair',
 web:'oracle browser sign-in passwords search engine',sys:'time date clock time zone language power sleep battery',kbd:'keyboard layout shortcuts input method compose',
 mouse:'mouse touchpad pointer cursor scrolling',devs:'devices printer usb camera hardware',acc:'accessibility screen reader magnifier captions vision hearing',
 priv:'privacy permissions camera microphone location',sec:'security firewall secure boot encryption',health:'system health self-test diagnostics memory',
 usr:'users accounts password picture',remote:'remote access desktop control',stor:'storage backup disk space files',apps:'apps applications default',
 notif:'notifications do not disturb alerts',upd:'update recovery reset version',about:'about version computer information'};
const SSLIVE={};
/* the settings a page shows: its section headings and the labels of its settings, each with the section it is in */
function ssClean(el){const c=el.cloneNode(true);c.querySelectorAll('select,option,textarea,button,input,.msg,small,kbd,script,style').forEach(x=>x.remove());
 return c.textContent.replace(/\s+/g,' ').replace(/^[\s★•·]+|[\s:：…]+$/g,'').trim()}
function ssHarvest(k,box){if(!box)return[];const out=[],seen=new Set();let sec='';
 box.querySelectorAll('h4,label,legend').forEach(el=>{if(el.closest('.rmodal,.ssdd'))return;if(box.isConnected&&!el.getClientRects().length)return;   /* (not what this computer's page hides) */
  const t=ssClean(el);
  if(el.tagName=='H4'){sec=t;if(!t)return}
  if(!t||t.length>90||t.length<2||/^[\d\s%×.,:+−-]+$/.test(t)||(el.tagName!='H4'&&t==sec))return;   /* (a label that only repeats its section's name is the section) */const key=t+'\t'+(el.tagName=='H4'?'':sec);if(seen.has(key))return;seen.add(key);
  out.push(el.tagName=='H4'?[t,'',1]:[t,sec])});
 SSLIVE[k]=out;return out}
/* every page, section and setting that can be found, with its place */
function ssAll(SEC){const L=[];const idx=typeof SETIDX=='object'&&SETIDX?SETIDX:{};
 for(const k in SEC){const pg=SEC[k][1];L.push({k,t:pg,ic:SEC[k][0],p:'Settings',kw:SS_KEYS[k]||'',page:1});
  for(const [t,s,h] of (SSLIVE[k]||idx[k]||[]))if(t!=pg)L.push({k,t,s,h:!!h,p:'Settings › '+pg+(s&&!h?' › '+s:''),kw:''})}
 return L}
/* the words of a text, lower case and without accents; "Wi-Fi" is also "wifi", "self-test" also "selftest" */
const ssWords=s=>{const o=[];for(const w of String(s).toLowerCase().normalize('NFD').replace(/[\u0300-\u036f]/g,'').split(/\s+/)){const p=w.split(/[^a-z0-9]+/).filter(Boolean);o.push(...p);if(p.length>1)o.push(p.join(''))}return o};
/* process of elimination: every word typed starts a word of the entry (its text, its place, or a page's keywords) */
function ssMatch(L,q){const qw=ssWords(q);if(!qw.length)return[];const R=[];
 for(const e of L){const tw=ssWords(e.t),all=tw.concat(ssWords(e.p),ssWords(e.kw));if(!qw.every(w=>all.some(a=>a.startsWith(w))))continue;
  const tl=e.t.toLowerCase(),ql=q.trim().toLowerCase();let sc=tl.startsWith(ql)?0:qw.every(w=>tw.some(a=>a.startsWith(w)))?1:2;
  /* a page whose name or subject matches comes first ("sound": Volume Controls), then section headings */
  if(e.page)sc=qw.every(w=>tw.concat(ssWords(e.kw)).some(a=>a.startsWith(w)))?-1:sc-.5;else if(e.h)sc-=.2;R.push([sc,e])}
 return R.sort((a,b)=>a[0]-b[0]||a[1].t.length-b[1].t.length).map(x=>x[1]).slice(0,60)}
/* the typed words in bold in a result */
function ssMark(t,q){const qw=ssWords(q);const esc=s=>s.replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
 return t.split(/(\s+)/).map(w=>{const lw=w.toLowerCase();const m=qw.filter(x=>lw.normalize('NFD').replace(/[\u0300-\u036f]/g,'').replace(/^[^a-z0-9]+/,'').startsWith(x)).sort((a,b)=>b.length-a.length)[0];
  if(!m)return esc(w);const pre=w.length-w.replace(/^[^A-Za-z0-9À-ɏ]+/,'').length;return esc(w.slice(0,pre))+'<b>'+esc(w.slice(pre,pre+m.length))+'</b>'+esc(w.slice(pre+m.length))}).join('')}
/* open a result: the page, then the setting on it, scrolled to and lit for a moment */
function ssOpen(W,e){W.show(e.k);let n=0;const find=()=>{const box=W.body.querySelector('.sc');if(W.sec!=e.k||!box){if(++n<40)setTimeout(find,100);return}
 if(e.page){box.scrollTop=0;return}let sec='',hit=null;
 for(const el of box.querySelectorAll('h4,label,legend')){const t=ssClean(el);if(el.tagName=='H4')sec=t;if(t==e.t&&(e.h?el.tagName=='H4':(!e.s||sec==e.s))){hit=el;break}}
 if(!hit){if(++n<40)setTimeout(find,100);return}
 const tgt=hit.tagName=='LABEL'&&hit.parentElement&&hit.parentElement.classList.contains('r2')?[hit,hit.nextElementSibling].filter(Boolean):[hit];
 hit.scrollIntoView({block:'center'});tgt.forEach(x=>{x.classList.remove('sshit');void x.offsetWidth;x.classList.add('sshit')});setTimeout(()=>tgt.forEach(x=>x.classList.remove('sshit')),2600);
 const ctl=hit.control||(tgt[1]&&tgt[1].querySelector&&tgt[1].querySelector('input,select,textarea,button'));if(ctl&&!ctl.disabled)try{ctl.focus({preventScroll:true})}catch(er){}};setTimeout(find,60)}
function ssAttach(W){if(!W||!W.SEC||W.body.querySelector('.ssbar'))return;
 const bar=document.createElement('div');bar.className='ssbar';
 bar.innerHTML="<span class='ssi' aria-hidden='true'></span><input type='search' class='ssq' placeholder='Search settings' aria-label='Search settings' role='combobox' aria-autocomplete='list' aria-expanded='false' autocomplete='off' spellcheck='false'><div class='ssdd' role='listbox' aria-label='Matching settings' hidden></div>";
 W.body.insertBefore(bar,W.body.firstChild);const q=bar.querySelector('.ssq'),dd=bar.querySelector('.ssdd');const lid='ssdd'+Math.random().toString(36).slice(2,8);dd.id=lid;q.setAttribute('aria-controls',lid);
 let res=[],cur=-1;
 const close=()=>{dd.hidden=true;q.setAttribute('aria-expanded','false');q.removeAttribute('aria-activedescendant');cur=-1};
 const hi=i=>{const o=dd.querySelectorAll('.ssr');o.forEach((x,j)=>x.setAttribute('aria-selected',j==i));cur=i;if(o[i]){q.setAttribute('aria-activedescendant',o[i].id);o[i].scrollIntoView({block:'nearest'})}};
 const pick=i=>{const e=res[i];if(!e)return;close();q.blur();ssOpen(W,e)};
 const draw=()=>{const v=q.value;if(!v.trim()){dd.innerHTML='';close();return}
  res=ssMatch(ssAll(W.SEC),v);dd.hidden=false;q.setAttribute('aria-expanded','true');
  if(!res.length){dd.innerHTML=`<div class='ssnone'></div>`;dd.firstChild.textContent='No settings match “'+v.trim()+'”';cur=-1;return}
  dd.innerHTML=res.map((e,i)=>`<div class='ssr' role='option' id='${lid}-${i}' aria-selected='false'><div class='sst'>${e.page?e.ic+' ':''}${ssMark(e.t,v)}</div><div class='ssp'></div></div>`).join('');
  dd.querySelectorAll('.ssr').forEach((r,i)=>{r.querySelector('.ssp').textContent=res[i].p;r.onmousedown=ev=>ev.preventDefault();r.onclick=()=>pick(i);r.onmousemove=()=>{if(cur!=i)hi(i)}});hi(0)};
 q.oninput=draw;q.onfocus=()=>{if(q.value.trim())draw()};q.onblur=()=>setTimeout(close,150);
 q.onkeydown=e=>{if(e.key=='ArrowDown'||e.key=='ArrowUp'){e.preventDefault();if(dd.hidden)draw();if(!res.length)return;const n=res.length;hi(((cur<0?(e.key=='ArrowDown'?-1:0):cur)+(e.key=='ArrowDown'?1:-1)+n)%n)}
  else if(e.key=='Enter'){e.preventDefault();if(dd.hidden)draw();pick(cur<0?0:cur)}
  else if(e.key=='Escape'){if(!dd.hidden){e.preventDefault();e.stopPropagation();close()}else if(q.value){e.preventDefault();e.stopPropagation();q.value=''}}};
 if(typeof trTree=='function')try{trTree(bar)}catch(e){}}
/* (settings25.js's settings() calls ssAttach when it opens the window, and ssHarvest each time it shows a page) */
