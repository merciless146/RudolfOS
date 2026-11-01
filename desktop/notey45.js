/* ===== 1.4.6 (notey45): Notey for SpreadsheetPro and PresentPro, as he is in WordPro: the same limited animation
   (8 frames a second, notey25.js's poses and moods), the same bubble, menu, tips that come by themselves (and can be
   paused), "How do I…?" with buttons that do it, dragging him around, falling asleep when nothing happens, and a
   check of the work with fixes. What he knows about each app comes from it:
   A = {app, kb:[[keywords, html, action?]], tips:[html], scan()->[{k,msg,fix,run,show}], auto()->finding or null
        (a tip for what was just done), empty()->bool, greet:[html when empty, html otherwise], menu:[[label, fn]],
        check:'✍️ label', len()->number (how much there is, for his reactions)}.
   The apps call W.notey.edited(kind, before, after) after each change. ===== */
function offNotey(W,X,A){if(typeof NOTEY_SVG!='string')return null;const root=X.root,esc=offEsc;
 const sget=()=>{try{return JSON.parse(localStorage.getItem('bd95wp'))||{}}catch(e){return{}}},sset=o=>{try{localStorage.setItem('bd95wp',JSON.stringify(Object.assign(sget(),o)))}catch(e){}};
 const S0=sget();let enabled=!S0.noteyOff,tips=S0.noteyTips!==false,snooze={},lastTip=0,mode='',hideT=0,idleT=0,sinceDirty=0;
 const box=document.createElement('div');box.className='wn';box.innerHTML=`<div class='wn-bub' style='display:none'></div><div class='wn-m' title='Notey — click for help'>${NOTEY_SVG}</div>`;root.appendChild(box);
 const bub=box.querySelector('.wn-bub'),m=box.querySelector('.wn-m');
 const mood=(c,ms)=>{box.classList.remove('alert','happy','think','sleep','wave');if(c){box.classList.add(c);if(ms)setTimeout(()=>box.classList.remove(c),ms)}};
 function hide(){bub.style.display='none';bub.innerHTML='';mode='';clearTimeout(hideT);if(!box.classList.contains('sleep'))mood('')}
 function say(html,btns,o){o=o||{};clearTimeout(hideT);bub.innerHTML=`<u class='wn-x' title='Close'>✕</u><div class='wn-t'>${html}</div><div class='wn-b'></div>`;const bb=bub.querySelector('.wn-b');
  (btns||[]).forEach(([t,f,pri])=>{const b=document.createElement('button');b.textContent=t;if(pri)b.className='p';b.onclick=()=>{f&&f()};bb.appendChild(b)});
  bub.querySelector('.wn-x').onclick=()=>hide();bub.style.display='block';mode=o.passive?'passive':'open';mood(o.mood||'');if(o.mood=='happy')setTimeout(()=>{if(box.classList.contains('happy'))mood('')},1800);
  if(o.passive)hideT=setTimeout(()=>{if(mode=='passive')hide()},o.ms||22000);bub.scrollTop=0;return bub}
 function show(v){enabled=v;sset({noteyOff:!v});box.style.display=v?'block':'none';if(v){mood('wave',1800);menu()}else hide()}
 /* ----- findings ----- */
 function offerFinding(f,passive){const btns=[];if(f.fix)btns.push([f.fix,()=>{let c=0;try{c=f.run()}catch(e){console.warn(e)}hide();say(c?`Done! I fixed <b>${c}</b> thing${c>1?'s':''}. You can undo with Ctrl+Z.`:'All fixed.',[['Check again',check],['Thanks!',hide]],{mood:'happy',passive:true,ms:9000})},1]);
  if(f.show)btns.push(['Show me',()=>{f.show()}]);btns.push(['Not now',()=>{snooze[f.k]=Date.now()+600000;hide()}]);say((passive?'<b>Tip:</b> ':'')+f.msg,btns,{mood:passive?'alert':'',passive})}
 function check(){mood('think');setTimeout(()=>{mood('');let F=[];try{F=A.scan()}catch(e){console.warn('Notey check',e)}
  if(A.empty&&A.empty()){say(A.nothing||'There’s nothing to check yet.',[['Templates',()=>{hide();A.templates&&A.templates()}],['OK',hide]]);return}
  const sum=A.summary?`<div class='wn-sum'>${A.summary()}</div>`:'';
  if(!F.length){say(sum+'No problems found. Nice work!',[['Thanks!',hide]],{mood:'happy'});return}
  const el=say(sum+`I found <b>${F.length}</b> thing${F.length>1?'s':''} to look at:`,[],{});const L=document.createElement('div');L.className='wn-list';
  F.forEach(f=>{const r=document.createElement('div');r.className='wn-fi';r.innerHTML=`<div>${f.msg}</div>`;const b=document.createElement('div');
   if(f.fix){const x=document.createElement('button');x.className='p';x.textContent=f.fix;x.onclick=()=>{try{f.run()}catch(e){console.warn(e)}r.remove();if(!L.children.length){hide();say('All done! Nice and tidy.',[['Thanks!',hide]],{mood:'happy',passive:true,ms:8000})}};b.appendChild(x)}
   if(f.show){const x=document.createElement('button');x.textContent='Show me';x.onclick=()=>f.show();b.appendChild(x)}r.appendChild(b);L.appendChild(r)});
  el.querySelector('.wn-t').appendChild(L);const c=document.createElement('button');c.textContent='Close';c.onclick=hide;el.querySelector('.wn-b').appendChild(c)},450)}
 /* ----- tips that come by themselves: after a change, when nothing else is said ----- */
 function auto(){if(!enabled||!tips||mode=='open')return;if(Date.now()-lastTip<25000)return;
  if(X.dirty&&sinceDirty&&Date.now()-sinceDirty>180000&&!(snooze.save>Date.now())&&A.save){snooze.save=Date.now()+300000;lastTip=Date.now();say('You haven’t saved in a while. Want me to save for you?',[['Save now',()=>{hide();A.save()},1],['Later',hide]],{mood:'alert',passive:true});return}
  let f=null;try{f=A.auto&&A.auto(k=>!(snooze[k]>Date.now()))}catch(e){console.warn('Notey tip',e)}
  if(!f){let F=[];try{F=A.scan().filter(x=>x.tip!==false&&!(snooze[x.k]>Date.now()))}catch(e){}f=F[0]}
  if(f){lastTip=Date.now();snooze[f.k]=Math.max(snooze[f.k]||0,Date.now()+120000);if(f.say)return say(f.msg,f.btns,{mood:f.mood||'alert',passive:true});offerFinding(f,true)}}
 /* ----- menus ----- */
 function menu(){say(`Hi! I’m <b>Notey</b>. What can I help you with?`,[[A.check||'✍️ Check my work',check,1],...(A.menu||[]).map(([t,f])=>[t,()=>{hide();f()}]),['❓ How do I…?',ask],['💡 Show me a tip',tip],[tips?'🔕 Pause automatic tips':'🔔 Resume automatic tips',()=>{tips=!tips;sset({noteyTips:tips});hide();say(tips?'I’ll point things out as you work.':'OK, I’ll stay quiet until you ask.',[['OK',hide]],{passive:true,ms:5000})}],['👋 Hide Notey',()=>show(false)]],{})}
 let ti=Math.floor(Math.random()*(A.tips||['']).length);function tip(){const T=A.tips||[];if(!T.length)return;ti=(ti+1)%T.length;say('<b>Tip:</b> '+T[ti],[['Another tip',tip],['Thanks!',hide]],{mood:'alert'})}
 function ask(){const el=say(`Ask me a question, like “${esc(A.example||'how do I make a chart?')}”`,[],{});const t=el.querySelector('.wn-t');t.insertAdjacentHTML('beforeend',`<input class='wn-q' placeholder='Type your question…'><div class='wn-res'></div>`);const inp=t.querySelector('.wn-q'),res=t.querySelector('.wn-res');inp.focus();
  const run=()=>{const w=inp.value.toLowerCase().split(/[^a-z0-9.]+/).filter(x=>x.length>2&&!/^(how|can|the|and|you|for|what|does|with|make|get|want|use|add|do|to|my|i|this|that|into|from)$/.test(x));res.innerHTML='';if(!w.length)return;
   const sc=(A.kb||[]).map(e=>{const k=e[0].split(' ');let s=0;w.forEach(x=>{k.forEach(y=>{if(y==x)s+=3;else if(y.startsWith(x)||x.startsWith(y)&&y.length>3)s+=1})});return[s,e]}).filter(a=>a[0]>0).sort((a,b)=>b[0]-a[0]).slice(0,3);
   const extra=A.find?A.find(w):[];
   if(!sc.length&&!extra.length){res.innerHTML=`<div class="wn-fi">I’m not sure about that one. Try words like ${esc(A.tryWords||'“chart”, “sort” or “print”')}.</div>`;return}
   sc.forEach(([s,e])=>{const d=document.createElement('div');d.className='wn-fi';d.innerHTML=`<div>${e[1]}</div>`;if(e[2]){const b=document.createElement('button');b.textContent='Show me';b.onclick=()=>{hide();e[2]()};d.appendChild(b)}res.appendChild(d)});
   extra.forEach(([html,label,f])=>{const d=document.createElement('div');d.className='wn-fi';d.innerHTML=`<div>${html}</div>`;if(f){const b=document.createElement('button');b.textContent=label;b.onclick=()=>{hide();f()};d.appendChild(b)}res.appendChild(d)})};
  inp.onkeydown=e=>{if(e.key=='Enter')run();e.stopPropagation()};const b=document.createElement('button');b.textContent='Ask';b.className='p';b.onclick=run;el.querySelector('.wn-b').appendChild(b);const c=document.createElement('button');c.textContent='Close';c.onclick=hide;el.querySelector('.wn-b').appendChild(c)}
 /* ----- wiring: click for the menu, drag him anywhere, sleep when nothing happens ----- */
 let dragMoved=0;m.addEventListener('click',()=>{if(dragMoved){dragMoved=0;return}if(box.classList.contains('sleep'))mood('wave',1500);if(mode=='open')hide();else menu()});
 (function(){let d=null;m.addEventListener('pointerdown',e=>{const r=box.getBoundingClientRect(),rr=root.getBoundingClientRect();d={x:e.clientX,y:e.clientY,l:r.left-rr.left,t:r.top-rr.top};try{m.setPointerCapture(e.pointerId)}catch(x){}});
  m.addEventListener('pointermove',e=>{if(!d)return;const dx=e.clientX-d.x,dy=e.clientY-d.y;if(Math.abs(dx)+Math.abs(dy)<5&&!dragMoved)return;dragMoved=1;box.style.right='auto';box.style.bottom='auto';box.style.left=Math.max(0,Math.min(root.clientWidth-box.offsetWidth,d.l+dx))+'px';box.style.top=Math.max(0,Math.min(root.clientHeight-box.offsetHeight,d.t+dy))+'px'});
  const up=()=>{d=null;setTimeout(()=>{dragMoved=0},0)};m.addEventListener('pointerup',up);m.addEventListener('pointercancel',up)})();
 const sleepT=setInterval(()=>{if(!W.el.isConnected){clearInterval(sleepT);return}if(!enabled)return;const idle=Date.now()-(W._last||Date.now());if(mode==''&&idle>90000&&!box.classList.contains('sleep'))mood('sleep');if(mode==''&&sinceDirty&&!X.dirty)sinceDirty=0},5000);
 ['keydown','pointerdown'].forEach(ev=>W.el.addEventListener(ev,()=>{W._last=Date.now();if(box.classList.contains('sleep'))mood('')}));W._last=Date.now();
 /* after each change: his face follows (a big deletion, an undo, a big paste), and a tip may come 2 seconds later */
 function edited(kind,d){if(X.dirty&&!sinceDirty)sinceDirty=Date.now();if(!X.dirty)sinceDirty=0;if(box.classList.contains('sleep'))mood('');clearTimeout(idleT);idleT=setTimeout(auto,2200);try{W.notey.react&&W.notey.react(kind||'',d||0)}catch(e){}}
 W.notey={show,visible:()=>enabled,check:()=>{if(!enabled)show(true);setTimeout(check,50)},ask:()=>{if(!enabled)show(true);setTimeout(ask,50)},say,hide,menu,tip,edited,auto,get tipsOn(){return tips},get bubble(){return bub},snooze};
 box.style.display=enabled?'block':'none';
 try{if(typeof notey2=='function')notey2(W)}catch(e){console.warn('Notey animation',e)}
 if(enabled&&A.greet){setTimeout(()=>{if(!W.el.isConnected||mode)return;const empty=A.empty&&A.empty();say(empty?A.greet[0]:A.greet[1],empty&&A.templates?[['📄 Start from a template',()=>{hide();A.templates()},1],['🙂 Just let me work',hide],['🔕 Don’t show tips',()=>{tips=false;sset({noteyTips:false});hide()}]]:[['👍 Sounds good',hide],['🔕 Don’t show tips',()=>{tips=false;sset({noteyTips:false});hide()}]],{mood:'wave',passive:true,ms:30000})},900)}
 return W.notey}
