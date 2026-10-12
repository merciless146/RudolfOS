/* ===== Oracle extensions: Ruffle =====
   Ruffle, the Flash Player emulator the Flash Player app already uses, works as an extension for Oracle:
   - websites: on the bootable build the "Ruffle for Oracle" Chromium extension (/usr/lib/rudolf/ruffle-ext) plays
     Flash content inside Oracle's tabs; this module sends it its settings through the extension's bridge script
   - Flash files (.SWF) and saved web pages on this computer: Oracle plays them with Ruffle itself, in both versions
   Both use the same Ruffle as the Flash Player app: a copy built into the disc image (www/lib/ruffle) when there is
   one, otherwise the runtime address set in Settings > Multimedia (unpkg / jsDelivr by default). */
const OEXTK='bd95oext';
const oesc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'})[c]);
const oextCfg=()=>Object.assign({on:true,click:false},((ld(OEXTK,{})||{}).ruffle)||{});
const OEXT={ruffle:null,stored:null};let RUFLOCAL=false;
const OEXTN=Math.random().toString(36).slice(2)+Date.now().toString(36);
function rufSources(){const s=[];if(RUFLOCAL)s.push(location.origin+'/lib/ruffle/ruffle.js');
 (MMC.set.rufUrl?[MMC.set.rufUrl]:RUFURLS.filter(u=>/^https?:/i.test(u))).forEach(u=>{if(/^https?:/i.test(u)&&!(window.__sys&&fwCheck(u)))s.push(u)});return s}
function oextWant(){const c=oextCfg(),s=MMC.set;return{on:c.on,urls:rufSources(),config:{autoplay:c.click?'off':'auto',quality:s.rufQ,scale:s.rufScale,contextMenu:s.rufMenu?'on':'off'}}}
function oextPush(){try{window.postMessage({__rudolfExt:'cfg',ruffle:oextWant()},'/')}catch(e){}}
function oextSet(o){svj(OEXTK,{ruffle:Object.assign(oextCfg(),o)});oextPush()}
/* the Oracle tab (srcdoc page Oracle made) a message came from */
const oextFrame=src=>[...document.querySelectorAll('.win .fh iframe')].find(f=>f.contentWindow===src);
function oextHtml(){const c=oextCfg(),src=rufSources(),ext=OEXT.ruffle;
 const web=!c.on?'Off.':ext?(src.length?'<b style="color:#047857">Active.</b> Flash content on websites is played by Ruffle.':'Ruffle cannot be loaded: every source is blocked by the firewall.')
  :RD?'The Ruffle extension is not loaded. It comes with the current Rudolf OS disc image; restart Rudolf OS after updating to it.'
  :'Needs Rudolf OS started from its disc, USB drive or hard disk: in a web browser, Oracle cannot run extensions on websites.';
 const from=src.length?src.map(u=>u.startsWith(location.origin)?'Built into Rudolf OS (works offline)':oesc(u.replace(/^https?:\/\//,''))).join('<br>'):'No source allowed';
 return`<div style='border:1px solid #d1d5db;border-radius:10px;padding:14px;max-width:620px'>
<div style='display:flex;gap:12px;align-items:center'><div style='font-size:34px;line-height:1'>⚡</div><div style='flex:1'><b style='font-size:16px'>Ruffle</b> <span style='font-size:12px;padding:1px 8px;border-radius:9px;background:${c.on?'#d1fae5;color:#065f46':'#e5e7eb;color:#374151'}'>${c.on?'On':'Off'}</span>
<div style='color:#4b5563'>Flash Player emulator: plays Flash games and animations</div></div><button data-v='${c.on?'off':'on'}' style='padding:6px 14px'>${c.on?'Turn off':'Turn on'}</button></div>
<table style='margin-top:12px;border-collapse:collapse;font-size:13px'>
<tr><td style='color:#4b5563;padding:4px 14px 4px 0;vertical-align:top;white-space:nowrap'>Websites</td><td>${web}</td></tr>
<tr><td style='color:#4b5563;padding:4px 14px 4px 0;vertical-align:top;white-space:nowrap'>This computer</td><td>${c.on?'Flash files (.SWF) and saved web pages with Flash content open in Oracle and play with Ruffle.':'Off.'}</td></tr>
<tr><td style='color:#4b5563;padding:4px 14px 4px 0;vertical-align:top;white-space:nowrap'>Ruffle from</td><td>${from}<div style='color:#6b7280;font-size:12px'>Shared with the Flash Player app.</div></td></tr>
<tr><td style='color:#4b5563;padding:4px 14px 4px 0;vertical-align:top;white-space:nowrap'>Extension</td><td>${ext?oesc(ext.name+' '+ext.version):'Not loaded'}</td></tr></table>
<label style='display:flex;gap:8px;align-items:flex-start;margin-top:10px'><input type='checkbox' data-v='click'${c.click?' checked':''}${c.on?'':' disabled'}> <span>Click to play<div style='color:#6b7280;font-size:12px'>Show a play button instead of starting Flash content by itself.</div></span></label>
<p style='margin:12px 0 0'><button data-v='mm' style='padding:5px 12px'>Ruffle settings…</button></p></div>
<p style='color:#6b7280;font-size:12px;max-width:620px'>Quality, scaling, the right-click menu and the address Ruffle is loaded from are set in Settings → Multimedia. Ruffle runs Flash content in a sandbox; it cannot reach your files.</p>`}
const OEXTPAGE=`var N=${JSON.stringify(OEXTN)};function ask(v){parent.postMessage({act:'oext',v:v,n:N},'*')}
addEventListener('message',function(e){if(e.data&&typeof e.data.oexthtml=='string')document.getElementById('ox').innerHTML=e.data.oexthtml});
document.addEventListener('click',function(e){var b=e.target.closest&&e.target.closest('button[data-v]');if(b)ask(b.dataset.v)});
document.addEventListener('change',function(e){if(e.target.dataset&&e.target.dataset.v=='click')ask(e.target.checked?'click1':'click0')});ask('state')`;
/* a Ruffle player page Oracle makes for one Flash file: bytes from this computer (path) or a web address (url) */
function rufPage(title,o){const cfg=Object.assign(rufConfig(),{autoplay:oextCfg().click?'off':'auto',unmuteOverlay:'visible'});
 return{title,html:`<!doctype html><meta charset='utf-8'><title>${oesc(title)}</title><style>html,body{margin:0;height:100%;background:#111;color:#e5e7eb;font:14px system-ui,sans-serif}#p{position:absolute;inset:0}#m{position:absolute;left:0;right:0;bottom:0;padding:10px 14px;background:#1f2937;display:none}#m button{margin-left:8px}</style>
<div id='p'></div><div id='m'></div><script>var SRC=${JSON.stringify(rufSources())},CFG=${JSON.stringify(cfg)},O=${JSON.stringify(o)};
function say(t,dl){var m=document.getElementById('m');m.textContent=t;m.style.display='block';if(dl){var b=document.createElement('button');b.textContent='Download it';b.onclick=function(){parent.postMessage({act:'dlreq',v:O.url},'*')};m.appendChild(b)}}
function lib(i,cb){if(window.RufflePlayer&&window.RufflePlayer.newest)return cb();if(i>=SRC.length)return say('Ruffle could not be loaded. Check the internet connection, or the Flash runtime address in Settings → Multimedia.');var s=document.createElement('script');s.src=SRC[i];s.onload=function(){cb()};s.onerror=function(){s.remove();lib(i+1,cb)};document.head.appendChild(s)}
function play(opt){lib(0,function(){var pl=window.RufflePlayer.newest().createPlayer();pl.style.cssText='width:100%;height:100%;display:block';document.getElementById('p').appendChild(pl);
 var r=(pl.ruffle?pl.ruffle():pl).load(Object.assign({},CFG,opt));if(r&&r.catch)r.catch(function(){say(O.url?'This site does not let other pages load its Flash file.':'This Flash file could not be played.',!!O.url)})})}
if(O.url)play({url:O.url});else{addEventListener('message',function(e){var d=e.data;if(!d||!d.oextswf)return;if(d.err)return say(d.err);play({data:d.oextswf,swfFileName:O.name})});parent.postMessage({act:'oextswf',v:O.path},'*')}
</`+`script>`}}
/* a saved web page with Flash content: each local SWF becomes a Ruffle player */
function rufLocalHtml(p,html){if(!/\.swf|shockwave|futuresplash|d27cdb6e/i.test(html))return null;
 const D=new DOMParser().parseFromString(html,'text/html'),dir=p.slice(0,-1);let n=0;
 const ref=el=>{if(el.tagName=='EMBED')return el.getAttribute('src');const pm=[...el.querySelectorAll('param')].find(x=>/^(movie|src)$/i.test(x.getAttribute('name')||''));return el.getAttribute('data')||(pm&&pm.getAttribute('value'))};
 [...D.querySelectorAll('object,embed')].forEach(el=>{if(!el.isConnected)return;
  const r=ref(el);if(!r||/^(https?|data|blob):/i.test(r)||!/\.swf(\?|#|$)/i.test(r))return;
  const lp=resolve(decodeURIComponent(r.replace(/^file:\/\/\//i,'').replace(/[?#].*$/,'')).split('/').join('\\'),dir);const v=node(lp);if(typeof v!='string')return;
  const d=D.createElement('div');d.className='rudolf-swf';d.dataset.p=lp.join('/');const w=el.getAttribute('width')||'550',h=el.getAttribute('height')||'400';
  d.style.cssText=`display:inline-block;position:relative;background:#111;width:${/%$/.test(w)?w:parseInt(w)+'px'};height:${/%$/.test(h)?h:parseInt(h)+'px'}`;el.replaceWith(d);n++});
 if(!n)return null;const cfg=Object.assign(rufConfig(),{autoplay:oextCfg().click?'off':'auto',unmuteOverlay:'visible'});
 const s=D.createElement('script');s.textContent=`(function(){var SRC=${JSON.stringify(rufSources())},CFG=${JSON.stringify(cfg)},box=[].slice.call(document.querySelectorAll('.rudolf-swf'));
function lib(i,cb){if(window.RufflePlayer&&window.RufflePlayer.newest)return cb();if(i>=SRC.length){box.forEach(function(b){b.textContent='Ruffle could not be loaded.';b.style.color='#fff'});return}var s=document.createElement('script');s.src=SRC[i];s.onload=function(){cb()};s.onerror=function(){s.remove();lib(i+1,cb)};document.head.appendChild(s)}
addEventListener('message',function(e){var d=e.data;if(!d||!d.oextswf)return;var b=box.filter(function(x){return x.dataset.p==d.path&&!x.firstChild})[0];if(!b)return;if(d.err){b.textContent=d.err;b.style.color='#fff';return}
 lib(0,function(){var pl=window.RufflePlayer.newest().createPlayer();pl.style.cssText='width:100%;height:100%;display:block';b.appendChild(pl);(pl.ruffle?pl.ruffle():pl).load(Object.assign({},CFG,{data:d.oextswf,swfFileName:d.path.split('/').pop()}))})});
box.forEach(function(b){parent.postMessage({act:'oextswf',v:b.dataset.p},'*')})})()`;
 (D.body||D.documentElement).appendChild(s);const t=D.querySelector('title');return{title:t?t.textContent:p.at(-1),html:'<!doctype html>'+D.documentElement.outerHTML}}
/* Oracle asks secPage first for every address */
{const _sp=secPage;secPage=function(u,doc,lk,esc){
 try{if(u=='about:extensions')return doc('Extensions',`<h2 style='margin:0 0 4px'>Extensions</h2><p style='color:#4b5563;margin:0 0 14px'>Extensions add features to Oracle. These come with Rudolf OS.</p><div id='ox'>${oextHtml()}</div>`,OEXTPAGE);
  const c=oextCfg();
  if(c.on&&u.startsWith('file:///')){const p=resolve(decodeURIComponent(u.slice(8)).split('/').join('\\'),['C:']),v=node(p),e=ext(p.at(-1));
   if(typeof v=='string'&&e=='SWF')return rufPage(p.at(-1)+' - Flash',{path:p.join('/'),name:p.at(-1)});
   if(typeof v=='string'&&(e=='HTM'||e=='HTML')){const r=rufLocalHtml(p,v);if(r)return r}}
  if(c.on&&/^https?:/i.test(u)&&!fwCheck(u)){let pth='';try{pth=new URL(u).pathname}catch(x){}if(/\.swf$/i.test(pth))return rufPage(decodeURIComponent(pth.split('/').pop())+' - Flash',{url:u})}}catch(x){console.warn(x)}
 return _sp(u,doc,lk,esc)}}
addEventListener('message',e=>{const d=e.data;if(!d||typeof d!='object')return;
 if(e.source===window&&d.__rudolfExt=='hello'){OEXT.ruffle={id:d.id,name:d.name||'Ruffle for Oracle',version:d.version||''};OEXT.stored=d.ruffle;
  if(JSON.stringify(d.ruffle)!=JSON.stringify(oextWant()))oextPush();return}
 if(d.__rudolfExt=='ruffle-active'){let w=e.source;for(let i=0;i<6&&w;i++){const f=oextFrame(w);if(f){const st=f.closest('.win').querySelector('.stat');if(st)st.textContent='⚡ Flash content on this page is played by Ruffle';break}try{w=w.parent}catch(x){break}}return}
 const f=e.source&&oextFrame(e.source);if(!f||!f.hasAttribute('srcdoc'))return;
 if(d.act=='oext'&&d.n===OEXTN){const v=d.v;if(v=='on'||v=='off')oextSet({on:v=='on'});else if(v=='click1'||v=='click0')oextSet({click:v=='click1'});else if(v=='mm'){settings('mm');return}
  try{e.source.postMessage({oexthtml:oextHtml()},'*')}catch(x){}}
 else if(d.act=='oextswf'&&typeof d.v=='string'){const p=d.v.split('/');if(p[0]!='C:'||ext(p.at(-1))!='SWF'||!oextCfg().on)return;
  mmBytes(node(p)).then(u=>{const b=u.buffer.slice(u.byteOffset,u.byteOffset+u.byteLength);e.source.postMessage({oextswf:b,path:d.v},'*',[b])}).catch(er=>{try{e.source.postMessage({oextswf:1,err:er.message||'This Flash file could not be read.',path:d.v},'*')}catch(x){}})}});
/* Flash content counts as Oracle's: the Flash Player app and Oracle prefer the copy of Ruffle on the disc image */
if(RD)fetch('/lib/ruffle/ruffle.js',{method:'HEAD'}).then(r=>{if(r.ok){RUFLOCAL=true;if(!RUFURLS.includes('/lib/ruffle/ruffle.js'))RUFURLS.unshift('/lib/ruffle/ruffle.js');oextPush()}}).catch(()=>{});
try{window.postMessage({__rudolfExt:'ping'},'/')}catch(e){}
