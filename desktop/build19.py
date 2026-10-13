import base64,json,os,sys
B='base_v24.html';O='/mnt/user-data/outputs/blockdos.html'
s=open(B).read()
def rep(a,b,n=1):
    global s
    c=s.count(a);assert c==n,(a[:90],c,n);s=s.replace(a,b)
here=os.path.dirname(os.path.abspath(__file__))
L=lambda f:open(os.path.join(here,f)).read()
pinyin=open(os.path.join(here,'../libs/pinyin.min.json')).read()
od=base64.b64encode(open(os.path.join(here,'../libs/od/compiled/OpenDyslexic-Regular.woff2'),'rb').read()).decode()
odb=base64.b64encode(open(os.path.join(here,'../libs/od/compiled/OpenDyslexic-Bold.woff2'),'rb').read()).decode()
mods=['core25.js','shell25.js','files25.js','apps25a.js','apps25b.js','a11y25.js','settings25.js','notey25.js','wp25.js','misc25.js','native25.js','props25.js','oext25.js','pkg25.js','sysupd25.js','tts25.js','print25.js','mail25.js','web25.js','linux26.js','wifi26.js','term26.js','emusnes26.js','emu26.js','blk2lua26.js','blitzpacks26.js','blitzen26.js','picker26.js']
if os.path.exists(os.path.join(here,'i18n25.js')):mods.append('i18n25.js')
code='const PINYIN_JSON='+json.dumps(pinyin)+';\n'+'\n'.join(L(m) for m in mods)
anchor="(function(){const d=document.createElement('div');d.className='ic tr';"
rep(anchor,code+'\n'+anchor)
css=L('styles25.css')+"\n@font-face{font-family:'OpenDyslexic';src:url(data:font/woff2;base64,"+od+") format('woff2');font-weight:400}@font-face{font-family:'OpenDyslexic';src:url(data:font/woff2;base64,"+odb+") format('woff2');font-weight:700}\n"
i=s.index('</style>');s=s[:i]+css+s[i:]
# storage now lives in IndexedDB
rep("const save=()=>{try{localStorage.setItem('blockdos95',JSON.stringify(fs))}catch(e){}};","const save=()=>fsSave();")
# passwords: PBKDF2 with a per-user salt, encrypted homes, security keys
rep("const h=await hsh(p),a=ACCTS.find(x=>x.n.toLowerCase()==n.toLowerCase()&&x.h==h);","let a=await pwFind(n,p);if(a&&a.pk&&!(await passkeyCheck(a))){a=null}")
rep("if(a){USER=a;loginFails=0;","if(a){USER=a;try{await userLogin(a,p)}catch(err){console.warn(err)}loginFails=0;")
rep("if(await hsh(OP.get())!=USER.h)","if(!(await pwCheck(USER,OP.get())))",2)
rep("USER.h=await hsh(p1)","USER.h=await pwMake(p1);await homeRekey(p1)",2)
rep("if(pdel.size){ACCTS=ACCTS.filter(a=>!pdel.has(a));","if(pdel.size){pdel.forEach(a=>{a.id&&kvDel('home:'+a.id)});ACCTS=ACCTS.filter(a=>!pdel.has(a));")
rep("ACCTS.push({n:cn,h:await hsh(cp1),r:cr})","ACCTS.push({n:cn,h:await pwMake(cp1),r:cr})")
rep("if(await hsh(SP.get())!=USER.h)","if(!(await pwCheck(USER,SP.get())))",2)
rep(".map(a=>({n:String(a.n),h:a.h,r:a.r==='admin'?'admin':'user'}))",".map(a=>({...a,n:String(a.n),h:a.h,r:a.r==='admin'?'admin':'user'}))")
rep("localStorage.setItem('bd95cells',","localStorage.setItem(cellKey(),",2)
# pen pressure in Paint
rep("x.lineWidth=sz;x.lineCap=x.lineJoin='round';x.beginPath();x.moveTo(lx,ly)","x.lineWidth=e.pointerType=='pen'&&e.pressure?Math.max(1,sz*e.pressure*2):sz;x.lineCap=x.lineJoin='round';x.beginPath();x.moveTo(lx,ly)")
# natural scrolling and horizontal speed
rep("document.addEventListener('wheel',e=>{if(e.defaultPrevented||e.ctrlKey)return;const f=scrF();if(Math.abs(f-1)<0.05)return;","document.addEventListener('wheel',e=>{if(e.defaultPrevented||e.ctrlKey)return;const f=scrF(),nat=S.natscroll?-1:1,hf=Math.pow(2,((+S.hscroll||5)-5)/2.5);if(Math.abs(f-1)<0.05&&nat==1&&Math.abs(hf-1)<0.05)return;")
rep("el.scrollBy({left:e.deltaX*k*f,top:e.deltaY*k*f})","el.scrollBy({left:e.deltaX*k*f*hf*nat,top:e.deltaY*k*f*nat})")
# Notepad gets its own icon (spiral notepad and pencil)
rep("win('Notepad','📝'","win('Notepad','🗒️'")
rep("['📝','Notepad',notepad]","['🗒️','Notepad',notepad]")
rep('data-a="notepad">📝 Notepad','data-a="notepad">🗒️ Notepad')
# calculator: Standard / Scientific modes
rep("relabel();W.el.focus({preventScroll:true})}","relabel();try{calcModes(W,cg)}catch(e){}W.el.focus({preventScroll:true})}")
# WordPro additions and print/export CSS for watermarks and checklists
rep("const mb=q('.wp-mb');Object.keys(MENUS)","try{wpExtend(W,MENUS,{get ed(){return ed},get root(){return root},get sheet(){return sheet},q,touch,sched,rs,insertHTML,dialog,say,blocks,textNodes,docText,words,layout,get meta(){return meta},get path(){return p},get dirty(){return dirty},doSave:()=>doSave()});W.printDoc=printDoc}catch(e){console.warn('WordPro additions',e)}\n const mb=q('.wp-mb');Object.keys(MENUS)")
rep("  try{const f=document.createElement('iframe');f.style.cssText='position:fixed;right:0;bottom:0;width:0;height:0;border:0';f.srcdoc=html;","  if(typeof wpPrint=='function'&&wpPrint(W,html,{size:meta.size,land:meta.land,title:p?p.at(-1):'Untitled'}))return;\n  try{const f=document.createElement('iframe');f.style.cssText='position:fixed;right:0;bottom:0;width:0;height:0;border:0';f.srcdoc=html;")
rep("function loadTab(t){const u=t.h[t.i],r=page(u);","function loadTab(t){const u=t.h[t.i];let r=page(u);if(r.ext&&!r.frame&&typeof webWants=='function'&&webWants(u)){webOpen(u);r={title:webHost(u)+' (browser window)',html:webNote(u)}}")
rep("const metaCss=()=>","const metaCss=()=>(typeof wpWmCss=='function'?wpWmCss(meta):'')+")
s=s.replace('Rudolf Prompt','Command Prompt')
s=s.replace("<title>Rudolf OS</title>","<title>Rudolf OS</title>",1)
open(O,'w').write(s);print(len(s))
