/* ===== Send by e-mail =====
   Rudolf OS has no e-mail program, and pointing the desktop page at a mailto: link replaced the whole
   desktop with a blank page. Every "e-mail" action now opens this compose window instead: the message is
   written here and handed to web mail (Gmail, Outlook.com, Yahoo Mail) in Oracle, or to the computer's
   own mail program when there is one (Rudolf OS in a browser). Web mail links cannot carry attachments,
   so attached files are saved to Downloads, ready to attach with the web mail's paper-clip button. */
const mlEsc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
/* plain text of a stored file for the message body: WordPro/HTML documents lose their markup */
function mlText(v){if(typeof v!='string'||v.startsWith('data:'))return'';
 if(/^\s*(<!--wp:|<!doctype|<html|<[a-z][\s\S]*>)/i.test(v)){try{const d=new DOMParser().parseFromString(v.replace(/^<!--wp:[^>]*?-->/,''),'text/html');d.querySelectorAll('style,script').forEach(e=>e.remove());
  d.querySelectorAll('p,div,h1,h2,h3,h4,h5,h6,li,tr,br').forEach(e=>e.append('\n'));return d.body.textContent.replace(/[ \t]+\n/g,'\n').replace(/\n{3,}/g,'\n\n').trim()}catch(e){return''}}
 return v}
const ML_WEB=[['Gmail',(t,s,b)=>'https://mail.google.com/mail/?view=cm&fs=1&to='+encodeURIComponent(t)+'&su='+encodeURIComponent(s)+'&body='+encodeURIComponent(b)],
 ['Outlook.com',(t,s,b)=>'https://outlook.live.com/mail/0/deeplink/compose?to='+encodeURIComponent(t)+'&subject='+encodeURIComponent(s)+'&body='+encodeURIComponent(b)],
 ['Yahoo Mail',(t,s,b)=>'https://compose.mail.yahoo.com/?to='+encodeURIComponent(t)+'&subject='+encodeURIComponent(s)+'&body='+encodeURIComponent(b)]];
const ML_MAX=1800;   // web mail and mailto links stop working beyond a few thousand characters
/* emailCompose({to,subject,body,files:[[name,data],…]}) */
function emailCompose(o={}){const files=o.files||[];let body=String(o.body||''),cut=false;if(body.length>ML_MAX){body=body.slice(0,ML_MAX).replace(/\s+\S*$/,'')+' …';cut=true}
 const ov=document.createElement('div');ov.className='rmodal';
 ov.innerHTML=`<div class='rmbox mlbox' role='dialog' aria-label='Send by e-mail'><h3>Send by e-mail</h3>
<div class='mlgrid'><label for='mlto'>To</label><input id='mlto' class='mto' type='email' multiple placeholder='name@example.com' value='${mlEsc(o.to||'')}'>
<label for='mlsu'>Subject</label><input id='mlsu' class='msu' value='${mlEsc(o.subject||'')}'>
<label for='mlbo'>Message</label><textarea id='mlbo' class='mbo' rows='9'>${mlEsc(body)}</textarea></div>
${cut?`<p class='msg'>The message was shortened to fit in a web mail link; attach the file for the full text.</p>`:''}
${files.length?`<div class='mlatt'><b>Attachments:</b> ${files.map(f=>`<span class='mlf'>📎 ${mlEsc(f[0])}</span>`).join(' ')}<br><button class='msave'>Save to Downloads to attach</button> <span class='msg'>Web mail can't receive attachments from a link: save them, then use the paper-clip button in your e-mail.</span></div>`:''}
<div class='mlsend'><span>Send with:</span>${ML_WEB.map((w,i)=>`<button class='mweb' data-i='${i}'>${w[0]}</button>`).join('')}${RD?'':`<button class='mapp'>E-mail app on this computer</button>`}<button class='mcopy' title='Copy the message to paste it anywhere'>Copy message</button></div>
<div class='rmrow'><button class='mcan'>Close</button></div></div>`;
 document.body.appendChild(ov);const q=s=>ov.querySelector(s),val=()=>[q('.mto').value.trim(),q('.msu').value,q('.mbo').value];
 const close=()=>{ov.remove();document.removeEventListener('keydown',key,true)},key=e=>{if(e.key=='Escape'&&ov.contains(document.activeElement)){e.preventDefault();close()}};
 document.addEventListener('keydown',key,true);ov.querySelectorAll('input,textarea').forEach(i=>i.addEventListener('keydown',e=>e.stopPropagation()));
 q('.mcan').onclick=close;q(o.to?'.msu':'.mto').focus();
 let saved=false;const saveAll=async()=>{for(const[n,d]of files)await saveOut(n,d);saved=true};
 q('.msave')&&(q('.msave').onclick=saveAll);
 const go=async url=>{if(files.length&&!saved)await saveAll();close();
  if(RD){webOpen(url);return}
  const w=window.open(url,'_blank','noopener');if(!w)notify('Pop-up blocked','Allow pop-ups for this page, or use "Copy message".',{kind:'error'})};
 ov.querySelectorAll('.mweb').forEach(b=>b.onclick=()=>go(ML_WEB[+b.dataset.i][1](...val())));
 q('.mapp')&&(q('.mapp').onclick=()=>{const[t,s,b]=val();const a=document.createElement('a');a.href='mailto:'+encodeURI(t)+'?subject='+encodeURIComponent(s)+'&body='+encodeURIComponent(b);a.target='_blank';a.rel='noopener';document.body.appendChild(a);a.click();a.remove();if(files.length&&!saved)saveAll();close()});
 q('.mcopy').onclick=async()=>{const[t,s,b]=val();try{await navigator.clipboard.writeText((s?s+'\n\n':'')+b);notify('Copied','The message is on the clipboard.')}catch(e){q('.mbo').select();document.execCommand('copy')}};
 return ov}
