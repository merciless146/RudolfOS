/* ===== 1.4.6 (web46): dark pages in Oracle. Oracle's own bar, tabs and menu turn dark, and so do the pages: real tabs
   (the shell darkens each page as it loads, leaving pages that are dark already, pictures and videos as they are),
   Oracle's own pages (about:, files) and websites shown in frames (outside the shell). Oracle › ☰ › Dark pages, kept
   for everyone on this computer. ===== */
var WEB_DARK=false;try{WEB_DARK=localStorage.getItem('bd95webdark')=='1'}catch(e){}
const WEB_DARK_CSS='html{filter:invert(.9) hue-rotate(180deg)!important;background:#fff!important}img,video,picture,canvas,iframe,embed,object,svg image,[style*="background-image"]{filter:invert(1) hue-rotate(180deg)!important}';
function webDarkHtml(h){return WEB_DARK?`<style id='rudolf-dark'>${WEB_DARK_CSS}</style>`+h:h}
/* a website in a frame (outside the shell) cannot be changed from here: the frame itself is turned dark */
function webDarkFrame(fr,ext){if(fr)fr.style.filter=WEB_DARK&&ext?'invert(.9) hue-rotate(180deg)':''}
function webDarkWin(W){W.el.classList.toggle('odark',WEB_DARK);W.body.style.background=WEB_DARK?'#111827':'#fff';const fh=W.body.querySelector('.fh');if(fh)fh.style.background=WEB_DARK?'#111827':'#fff';
 W.body.querySelectorAll('.fh iframe').forEach(fr=>{if(fr._html!=null)fr.srcdoc=webDarkHtml(fr._html);else webDarkFrame(fr,fr.hasAttribute('src'))})}
function webDarkSet(on){WEB_DARK=!!on;try{localStorage.setItem('bd95webdark',WEB_DARK?'1':'0')}catch(e){}wins.forEach(w=>{if(w.el.classList.contains('oracle'))webDarkWin(w)});webDarkTabs(true)}
/* real tabs: the shell (1.2.2) darkens their pages */
let WEB_DARK_SENT='';
function webDarkTabs(force){if(typeof SHELL=='undefined'||!SHELL||typeof RT=='undefined'||!RT.size&&!force)return;const k=String(WEB_DARK);if(k===WEB_DARK_SENT&&!force)return;WEB_DARK_SENT=k;shellSend({rudolfShell:'tab',op:'dark',on:WEB_DARK})}
setInterval(()=>{if(typeof RT!='undefined'&&RT.size)webDarkTabs(false)},2000);
/* the shell forgets when it starts again: say it again when it says hello */
addEventListener('message',e=>{const m=e.data;if(m&&m.rudolfShell=='hello')WEB_DARK_SENT=''});
