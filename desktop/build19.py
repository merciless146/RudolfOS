import base64,json,os,sys
B='base_v24.html';O=sys.argv[1] if len(sys.argv)>1 else '/mnt/user-data/outputs/blockdos.html'   # e.g. python3 build19.py ../iso/www/index.html
s=open(B).read()
def rep(a,b,n=1):
    global s
    c=s.count(a);assert c==n,(a[:90],c,n);s=s.replace(a,b)
here=os.path.dirname(os.path.abspath(__file__))
L=lambda f:open(os.path.join(here,f)).read()
pinyin=open(os.path.join(here,'../libs/pinyin.min.json')).read()
od=base64.b64encode(open(os.path.join(here,'../libs/od/compiled/OpenDyslexic-Regular.woff2'),'rb').read()).decode()
odb=base64.b64encode(open(os.path.join(here,'../libs/od/compiled/OpenDyslexic-Bold.woff2'),'rb').read()).decode()
mods=['core25.js','shell25.js','files25.js','apps25a.js','apps25b.js','a11y25.js','settings25.js','notey25.js','wp25.js','misc25.js','native25.js','props25.js','oext25.js','pkg25.js','sysupd25.js','tts25.js','print25.js','mail25.js','web25.js','linux26.js','wifi26.js','term26.js','emusnes26.js','emu26.js','blk2lua26.js','blitzpacks26.js','blitzen26.js','picker26.js','devices26.js','signin26.js','display26.js','shell26.js','tabs26.js','tm26.js','dl27.js','sb28.js','keep29.js','luau30.js','apps30.js','games30.js','boot30.js','stick30.js','hwcheck30.js','devlink40.js','osd31.js','net31.js','settings31.js','saver31.js']
if os.path.exists(os.path.join(here,'i18n25.js')):mods.append('i18n25.js')
# 1.4.0: a top-level function defined in two modules replaces the earlier one without a word (the development
# link's dlPoll replaced the downloads' dlPoll, so downloads from real tabs never finished): stop the build instead
import re as _re
_seen={}
for _m in mods:
    for _n in set(_re.findall(r'^(?:async\s+)?function\s+([A-Za-z_$][\w$]*)',L(_m),_re.M)):
        assert _n not in _seen,f'{_n} is defined in both {_seen.get(_n)} and {_m}: give one of them another name'
        _seen[_n]=_m
changes=open(os.path.join(here,'../CHANGELOG.md')).read()   # "What's new" and the notice after an update
code='const PINYIN_JSON='+json.dumps(pinyin)+';\nconst RCHANGES='+json.dumps(changes).replace('</','<\\/')+';\n'+'\n'.join(L(m) for m in mods)
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
# Notepad gets its own icon (spiral notepad and pencil)
rep("win('Notepad','📝'","win('Notepad','🗒️'")
rep("['📝','Notepad',notepad]","['🗒️','Notepad',notepad]")
rep('data-a="notepad">📝 Notepad','data-a="notepad">🗒️ Notepad')
# calculator: Standard / Scientific modes
rep("relabel();W.el.focus({preventScroll:true})}","relabel();try{calcModes(W,cg)}catch(e){}W.el.focus({preventScroll:true})}")
# WordPro additions and print/export CSS for watermarks and checklists
rep("const mb=q('.wp-mb');Object.keys(MENUS)","try{wpExtend(W,MENUS,{get ed(){return ed},get root(){return root},get sheet(){return sheet},q,touch,sched,rs,insertHTML,dialog,say,blocks,textNodes,docText,words,layout,get meta(){return meta},get path(){return p},get dirty(){return dirty},doSave:()=>doSave()});W.printDoc=printDoc}catch(e){console.warn('WordPro additions',e)}\n const mb=q('.wp-mb');Object.keys(MENUS)")
rep("  try{const f=document.createElement('iframe');f.style.cssText='position:fixed;right:0;bottom:0;width:0;height:0;border:0';f.srcdoc=html;","  if(typeof wpPrint=='function'&&wpPrint(W,html,{size:meta.size,land:meta.land,title:p?p.at(-1):'Untitled'}))return;\n  try{const f=document.createElement('iframe');f.style.cssText='position:fixed;right:0;bottom:0;width:0;height:0;border:0';f.srcdoc=html;")
rep("function loadTab(t){if(rtLoad(t))return;const u=t.h[t.i],r=page(u);","function loadTab(t){if(rtLoad(t))return;const u=t.h[t.i];let r=page(u);if(r.ext&&!r.frame&&typeof webWants=='function'&&webWants(u)){webOpen(u);r={title:webHost(u)+' (browser window)',html:webNote(u)}}")
rep("const metaCss=()=>","const metaCss=()=>(typeof wpWmCss=='function'?wpWmCss(meta):'')+")
# 1.4.4: the taskbar's volume is the system volume on installed Rudolf OS (osd31.js); Settings gets System volume and brightness
rep("function volUpd(){const v=volLevel(),","function volUpd(){if(RD&&sysVolUpd())return;const v=volLevel(),")
rep("$('#vol').onclick=volPop;","$('#vol').onclick=()=>RD?sysVolPop():volPop();")
rep("$('#vol').addEventListener('wheel',e=>{e.preventDefault();const m=S.snd.master;","$('#vol').addEventListener('wheel',e=>{e.preventDefault();if(RD&&SV.v!=null){sysVolStep(e.deltaY<0?1:-1);return}const m=S.snd.master;")
rep("const m=S.snd&&S.snd.master;cmenu(e.clientX,e.clientY,[[m&&m.on?'Mute':'Unmute',","if(RD&&SV.v!=null){cmenu(e.clientX,e.clientY,[[SV.m?'Unmute':'Mute',()=>sysVolSet({muted:SV.m?0:1})],['Volume controls…',()=>settings('vol')]],{title:'Sound'});return}const m=S.snd&&S.snd.master;cmenu(e.clientX,e.clientY,[[m&&m.on?'Mute':'Unmute',")
rep("secFooter(c,W,'vol',()=>W.show('vol',1))}","sysVolPanel(c);secFooter(c,W,'vol',()=>W.show('vol',1))}")
rep("function dispPanelS(c,W){dispPanel0(c);","function dispPanelS(c,W){dispPanel0(c);briPanel(c);",2)
rep("The master volume scales all of them.","${RD?'“All desktop sounds” scales all of them, inside the system volume.':'The master volume scales all of them.'}",2)
rep("<div class='vr2 vm'><label><input type='checkbox' data-k='master' data-t='on'${m.on?' checked':''}> <b>Master volume</b>","<div class='vr2 vm'><label><input type='checkbox' data-k='master' data-t='on'${m.on?' checked':''}> <b>${RD?'All desktop sounds':'Master volume'}</b>")
# 1.4.4: the network icon, its popup and "connected" follow the real network on installed Rudolf OS (net31.js)
rep("const netUp=()=>!!(NETC.active&&","const netUp=()=>window.__net31?window.__net31.up&&navigator.onLine!==false:!!(NETC.active&&")
rep("function netUpd(){const B=$('#net');if(!B)return;","function netUpd(){if(window.__net31)return net31Upd();const B=$('#net');if(!B)return;")
rep("function netPop(){const old=document.querySelector('.np');","function netPop(){if(window.__net31)return net31Pop();const old=document.querySelector('.np');")
# 1.4.4: Network Settings drawn again into the same box (or Web search on the Oracle page): the old dirty check looked for fields no longer there
rep("CUST.net={dirty:()=>c.isConnected&&dirty()","CUST.net={dirty:()=>c.isConnected&&!!c.querySelector('#fn')&&dirty()")
# 1.4.4: Settings draws every page into the same box: an earlier page's footer stops updating itself once it is gone
rep("['input','change','click'].forEach(ev=>c.addEventListener(ev,()=>setTimeout(upd,0)));upd();f.upd=upd;return f}","['input','change','click'].forEach(ev=>c.addEventListener(ev,()=>setTimeout(()=>{if(f.isConnected)upd()},0)));upd();f.upd=upd;return f}")
s=s.replace('Rudolf Prompt','Command Prompt')
s=s.replace("<title>Rudolf OS</title>","<title>Rudolf OS</title>",1)
open(O,'w').write(s);print(len(s))
