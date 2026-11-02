/* Rudolf OS 1.4.7: the desktop's fonts (Settings › Display Properties › Font).
   Before, the list offered Windows fonts that Rudolf OS does not have (Tahoma, Verdana, Trebuchet MS, Georgia,
   Comic Sans MS, Lucida Console): all six showed the same fallback font (DejaVu Sans), and only Arial, Times New
   Roman and Courier New changed anything (fontconfig draws them with Liberation). Now every choice is a font the
   system has, or one built into the desktop (Gelasio, Fira Sans, Comic Neue; SIL OFL 1.1, libs/uifonts), named as it
   is, with the Windows font it stands in for. A setting saved by an older version is read as its stand-in. */
/* (functions, not constants: the desktop applies its settings before the modules' constants exist) */
function uiFonts(){return[
 ['DejaVu Sans','DejaVu Sans (default, like Verdana)'],['DejaVu Sans Condensed','DejaVu Sans Condensed (like Tahoma)'],
 ['Liberation Sans','Liberation Sans (like Arial)'],['Fira Sans','Fira Sans (like Trebuchet MS)'],
 ['Liberation Serif','Liberation Serif (like Times New Roman)'],['Gelasio','Gelasio (like Georgia)'],['DejaVu Serif','DejaVu Serif'],
 ['Comic Neue','Comic Neue (like Comic Sans MS)'],
 ['Liberation Mono','Liberation Mono (like Courier New)'],['DejaVu Sans Mono','DejaVu Sans Mono (like Lucida Console)']]}
function uiFontOld(){return{'Tahoma':'DejaVu Sans','Verdana':'DejaVu Sans','Arial':'Liberation Sans','Trebuchet MS':'Fira Sans','Times New Roman':'Liberation Serif',
 'Georgia':'Gelasio','Comic Sans MS':'Comic Neue','Courier New':'Liberation Mono','Lucida Console':'DejaVu Sans Mono'}}
/* the stand-ins elsewhere (the browser build on Windows or a Mac has the originals) */
function uiFontAlt(){return{'DejaVu Sans':'Verdana','DejaVu Sans Condensed':'Tahoma','Liberation Sans':'Arial','Fira Sans':"'Trebuchet MS'",'Liberation Serif':"'Times New Roman'",
 'Gelasio':'Georgia','DejaVu Serif':'Georgia','Comic Neue':"'Comic Sans MS'",'Liberation Mono':"'Courier New'",'DejaVu Sans Mono':"'Lucida Console'"}}
function uiFont(n){n=uiFontOld()[n]||n;return uiFonts().some(f=>f[0]==n)?n:'DejaVu Sans'}
function uiFontStack(n){n=uiFont(n);const g=/Mono/.test(n)?'monospace':/Serif|Gelasio/.test(n)?'serif':'sans-serif';
 return n=='DejaVu Sans Condensed'?`'DejaVu Sans Condensed','DejaVu Sans',Tahoma,sans-serif`:`'${n}',${uiFontAlt()[n]||'Arial'},${g}`}
function uiFontOptions(v){v=uiFont(v);return uiFonts().map(([n,l])=>`<option value="${n}"${n==v?' selected':''} style="font-family:${uiFontStack(n).replace(/"/g,'&quot;')}">${l}</option>`).join('')}
