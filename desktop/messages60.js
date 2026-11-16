/* ===== Rudolf OS 1.6.0 (docs/plan-1.6.0.md Part 6): Messages, text messages (SMS) through the cellular modem =====
   Conversations by number, read and send, delivery reports (Sent, Delivered), long messages and any language (the
   modem splits them into parts: the count shown while typing), notifications for new messages (rudolfd's mobile/notes,
   the same on the native desktop; with the desktop locked, the sender and "New message" only, unless an administrator
   allowed the text). The modem keeps a message only until it is copied into the signed-in account's folder
   (.rudolf/messages.json, in the encrypted folder when "Encrypt my data" is on; the page's own storage for an account
   that is not on the drive), then it is deleted from the modem. MMS (pictures) is left out. */
const MSGF='.rudolf/messages.json';
let MSG={db:null,acct:'',busy:0,W:null,cur:null,t:0};
/* the number as one conversation: spaces, dashes and brackets out */
const msgNum=n=>String(n||'').replace(/[\s()\-]/g,'');
/* GSM-7 or UCS-2, and the parts a text takes (160/153, 70/67) */
const MSG_G7=/^[@£$¥èéùìòÇ\nØø\rÅåΔ_ΦΓΛΩΠΨΣΘΞÆæßÉ !"#¤%&'()*+,\-./0-9:;<=>?¡A-ZÄÖÑÜ§¿a-zäöñüà\^{}\\\[~\]|€]*$/;
function msgParts(t){const n=[...String(t||'')].length,g=MSG_G7.test(t||'');return{n,g,parts:!n?0:g?(n<=160?1:Math.ceil(n/153)):(n<=70?1:Math.ceil(n/67))}}
async function msgLoad(){const a=typeof uid=='function'?uid():'';if(MSG.db&&MSG.acct==a)return MSG.db;let d=null;
 if(typeof hdOn=='function'&&hdOn()){try{d=JSON.parse(new TextDecoder().decode(await hdRead(MSGF)))}catch(e){d=null}}else{try{d=ld('bd95msg:'+a,null)}catch(e){}}
 MSG.db=d&&d.convs?d:{v:1,convs:{}};MSG.acct=a;return MSG.db}
async function msgSave(){const b=JSON.stringify(MSG.db);
 if(typeof hdOn=='function'&&hdOn())await rd('home/write?'+hdQ()+'&path='+encodeURIComponent(MSGF)+'&mkdirs=1',{method:'POST',body:b});else svj('bd95msg:'+MSG.acct,MSG.db)}
/* the modem's messages copied in (then deleted from it); the delivery reports of sent ones followed */
async function msgSync(){if(!RD||MSG.busy)return 0;MSG.busy=1;let n=0;try{await msgLoad();const s=await rd('mobile/status');
 for(const m of (s.modems||[]).filter(x=>x.sim&&(!x.locked||x.locked=='none'))){let L;try{L=(await rd('mobile/sms?id='+m.id)).messages||[]}catch(e){continue}
  const done=[];
  for(const x of L){const c=(x.sms||{}).content||{},p=(x.sms||{}).properties||{},num=msgNum(c.number);if(!num)continue;
   const conv=MSG.db.convs[num]||(MSG.db.convs[num]=[]);
   if(p.state=='received'){const ts=p.timestamp&&p.timestamp!='--'?Date.parse(p.timestamp)||Date.now():Date.now();
    if(!conv.some(e=>e.dir=='in'&&e.text==c.text&&Math.abs(e.time-ts)<2000))conv.push({dir:'in',text:c.text||'',time:ts,unread:1});n++;done.push(x.id)}
   else if(p['pdu-type']=='submit'){const e=conv.find(e=>e.dir=='out'&&e.modem==m.id&&e.sms==x.id);if(e){e.state=p.state=='sent'?'sent':e.state;
    if(p['delivery-state']&&/^completed-received/.test(p['delivery-state'])){e.state='delivered';done.push(x.id)}else if(/^(permanent|temporary-fatal)/.test(p['delivery-state']||'')){e.state='failed';done.push(x.id)}}}}
  if(n||done.length){await msgSave();for(const k of done)try{await rd('mobile/sms/delete?id='+m.id+'&sms='+k,{method:'POST'})}catch(e){}}}
 }catch(e){}finally{MSG.busy=0}if(MSG.W&&MSG.W.el.isConnected)msgDraw();return n}
async function msgSend(num,text){const s=await rd('mobile/status'),m=(s.modems||[]).find(x=>x.sim&&(!x.locked||x.locked=='none')&&x.state!='failed');
 if(!m)throw new Error('No cellular modem with a SIM card ready: see Settings › Mobile network.');
 await msgLoad();const conv=MSG.db.convs[num]||(MSG.db.convs[num]=[]),e={dir:'out',text,time:Date.now(),state:'sending',modem:m.id};conv.push(e);msgDraw();
 try{const r=await rd('mobile/sms/send?id='+m.id,{method:'POST',body:JSON.stringify({number:num,text})});e.sms=r.id;e.state='sent'}catch(er){e.state='failed';e.why=er.message}
 await msgSave();msgDraw();if(e.state=='failed')throw new Error(e.why||'The message was not sent.');return e}
function messagesApp(to){if(MSG.W&&MSG.W.el.isConnected){MSG.W.focus();if(to)msgOpen(msgNum(to));return MSG.W}
 const W=win('Messages','💬',760,520);MSG.W=W;W.body.style.cssText+=';display:flex;padding:0;overflow:hidden';
 W.body.innerHTML=`<div class='msgl' style='width:240px;border-right:1px solid #d1d5db;display:flex;flex-direction:column'><div style='padding:8px;display:flex;gap:6px'><input class='msgto' placeholder='Phone number' inputmode='tel' style='flex:1;min-width:0'><button class='msgnew' title='New message'>✏️ New</button></div><div class='msgcl' style='flex:1;overflow:auto'></div><label class='ck' style='padding:6px 8px;font-size:12px'><input type='checkbox' class='msgprev'> Show the text in notices</label></div>
  <div class='msgr' style='flex:1;display:flex;flex-direction:column;min-width:0'><div class='msgh' style='padding:8px 10px;border-bottom:1px solid #d1d5db;font-weight:600'></div><div class='msgb' style='flex:1;overflow:auto;padding:10px;display:flex;flex-direction:column;gap:6px'></div>
  <div class='msgc' style='border-top:1px solid #d1d5db;padding:8px;display:flex;gap:6px;align-items:flex-end'><textarea class='msgt' rows='2' style='flex:1;resize:none' placeholder='Text message'></textarea><div style='display:flex;flex-direction:column;gap:4px;align-items:flex-end'><span class='msgn' style='font-size:11px;color:#6b7280'></span><button class='msgs pri'>Send</button></div></div></div>`;
 const q=s=>W.body.querySelector(s);
 q('.msgto').onkeydown=e=>{if(e.key=='Enter')q('.msgnew').click()};
 q('.msgnew').onclick=()=>{const n=msgNum(q('.msgto').value);if(!n){q('.msgto').focus();return}q('.msgto').value='';if(!/^\+?\d{3,20}$/.test(n)){notify('💬 Messages','That is not a phone number.',{kind:'error'});return}msgOpen(n)};
 const cnt=()=>{const p=msgParts(q('.msgt').value);q('.msgn').textContent=p.n?(p.n+' characters · '+p.parts+(p.parts==1?' message':' parts')+(p.g?'':' (Unicode)')):''};q('.msgt').oninput=cnt;
 q('.msgt').onkeydown=e=>{if(e.key=='Enter'&&!e.shiftKey){e.preventDefault();q('.msgs').click()}};
 q('.msgs').onclick=async()=>{const t=q('.msgt').value;if(!t.trim()||!MSG.cur)return;q('.msgs').disabled=true;try{await msgSend(MSG.cur,t);q('.msgt').value='';cnt()}catch(e){notify('💬 Messages',e.message,{kind:'error'})}q('.msgs').disabled=false};
 rd('mobile/sms/preview').then(j=>{q('.msgprev').checked=!!j.on}).catch(()=>{});
 q('.msgprev').onchange=async e=>{const on=e.target.checked;try{if(on){const cr=await elevate('Show the text of new messages in notices, also on the lock screen',{cred:true});if(!cr){e.target.checked=false;return}
   await rd('mobile/sms/preview?on=1',{method:'POST',body:JSON.stringify({n:cr.n,p:cr.p})})}else await rd('mobile/sms/preview?on=0',{method:'POST'})}catch(er){e.target.checked=!on;notify('💬 Messages',er.message,{kind:'error'})}};
 msgLoad().then(()=>{const ks=Object.keys(MSG.db.convs);if(to)msgOpen(msgNum(to));else if(!MSG.cur&&ks.length)msgOpen(msgLast()[0]);msgDraw();msgSync()});
 MSG.t&&clearInterval(MSG.t);MSG.t=setInterval(()=>{if(!W.el.isConnected){clearInterval(MSG.t);MSG.t=0;MSG.W=null;return}msgSync()},10000);
 return W}
const msgLast=()=>Object.keys(MSG.db.convs).sort((a,b)=>{const x=MSG.db.convs[a],y=MSG.db.convs[b];return((y[y.length-1]||{}).time||0)-((x[x.length-1]||{}).time||0)});
function msgOpen(n){MSG.cur=n;const c=MSG.db&&MSG.db.convs[n];if(c&&c.some(e=>e.unread)){c.forEach(e=>delete e.unread);msgSave().catch(()=>{})}msgDraw()}
const msgTime=t=>{const d=new Date(t),n=new Date();return d.toDateString()==n.toDateString()?d.toLocaleTimeString([],{hour:'numeric',minute:'2-digit'}):d.toLocaleDateString([],{month:'short',day:'numeric'})+' '+d.toLocaleTimeString([],{hour:'numeric',minute:'2-digit'})};
const MSG_ST={sending:'Sending…',sent:'Sent',delivered:'Delivered',failed:'Not sent'};
function msgDraw(){const W=MSG.W;if(!W||!W.el.isConnected||!MSG.db)return;const q=s=>W.body.querySelector(s),L=q('.msgcl');L.innerHTML='';
 const ks=msgLast();if(!ks.length)L.innerHTML=`<div class='msg' style='padding:10px'>No messages yet. Texts sent to this computer's SIM card show here.</div>`;
 for(const k of ks){const c=MSG.db.convs[k],last=c[c.length-1]||{},u=c.filter(e=>e.unread).length,r=document.createElement('div');r.className='msgci'+(k==MSG.cur?' on':'');r.dataset.n=k;
  r.style.cssText='padding:8px 10px;cursor:pointer;border-bottom:1px solid #e5e7eb'+(k==MSG.cur?';background:#dbeafe':'');r.innerHTML=`<div style='display:flex'><b style='flex:1'></b><span style='font-size:11px;color:#6b7280'></span></div><div class='msg' style='white-space:nowrap;overflow:hidden;text-overflow:ellipsis'></div>`;
  r.querySelector('b').textContent=k+(u?' ('+u+')':'');r.querySelector('span').textContent=last.time?msgTime(last.time):'';r.querySelector('.msg').textContent=(last.dir=='out'?'You: ':'')+(last.text||'');r.onclick=()=>msgOpen(k);L.appendChild(r)}
 q('.msgh').textContent=MSG.cur||'Messages';q('.msgc').style.display=MSG.cur?'':'none';const B=q('.msgb');B.innerHTML='';
 for(const e of (MSG.cur&&MSG.db.convs[MSG.cur])||[]){const b=document.createElement('div');b.className='msgm '+(e.dir=='out'?'msgo':'msgi');
  b.style.cssText='max-width:75%;padding:6px 10px;border-radius:12px;white-space:pre-wrap;word-break:break-word;'+(e.dir=='out'?'align-self:flex-end;background:#2563eb;color:#fff':'align-self:flex-start;background:#e5e7eb;color:#111');
  b.innerHTML=`<div class='msgx'></div><div class='msgi2' style='font-size:10px;opacity:.75;text-align:right;margin-top:2px'></div>`;b.querySelector('.msgx').textContent=e.text;
  b.querySelector('.msgi2').textContent=msgTime(e.time)+(e.dir=='out'?' · '+(MSG_ST[e.state]||e.state):'');if(e.why)b.title=e.why;B.appendChild(b)}
 B.scrollTop=B.scrollHeight}
regApp('messages',{name:'Messages',icon:'💬',kw:'sms text messages phone cellular mobile',open:()=>messagesApp(),re:/^Messages/});
/* (a new message's notice: Messages copies it in at once when open) */
if(RD)addEventListener('rudolf-sms',()=>msgSync());
