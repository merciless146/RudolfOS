import base64,json,os,re,sys
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
mods=['core25.js','clock31.js','shell25.js','files25.js','apps25a.js','apps25b.js','a11y25.js','settings25.js','notey25.js','wp25.js','misc25.js','native25.js','props25.js','oext25.js','pkg25.js','sysupd25.js','tts25.js','print25.js','mail25.js','web25.js','linux26.js','wifi26.js','term26.js','emusnes26.js','emu26.js','blk2lua26.js','blitzpacks26.js','blitzen26.js','picker26.js','devices26.js','signin26.js','display26.js','shell26.js','tabs26.js','tm26.js','dl27.js','sb28.js','keep29.js','luau30.js','apps30.js','games30.js','boot30.js','stick30.js','hwcheck30.js','devlink40.js','osd31.js','net31.js','settings31.js','saver31.js','power31.js','wallpaper31.js','office45.js','notey45.js','web46.js','mem47.js','fonts47.js','weather47.js','name48.js','sec50.js','acct50.js','vault50.js','remote50.js','game51.js','bt51.js','lib52.js','tb54.js','desk54.js','home56.js','install56.js','tools56.js','storage56.js','rename56.js','keys56.js','accent56.js','appprops56.js','blitzdiag57.js','errors57.js','apps57.js','net57.js','engine57.js','mobile58.js','apk58.js','tablet58.js','oracle58.js','private59.js','sanitize59.js','sound58.js','games58.js','lock58.js','aurora58.js','statue58.js','gamesleep59.js','pkgbox59.js','devmode59.js','call59.js','phonecalls59.js','modem59.js','hotspot59.js','devkey59.js','setsearch59.js','titlebar59.js','tbbtn59.js','oneapp59.js','tmtop57.js','spreng45.js','sprof45.js','spro45.js','pprof45.js','ppro45.js']
if os.path.exists(os.path.join(here,'i18n25.js')):mods.append('i18n25.js')
# 1.4.0: a top-level function defined in two modules replaces the earlier one without a word (the development
# link's dlPoll replaced the downloads' dlPoll, so downloads from real tabs never finished): stop the build instead
import re as _re
_seen={}
for _m in mods:
    for _n in set(_re.findall(r'^(?:async\s+)?function\s+([A-Za-z_$][\w$]*)',L(_m),_re.M)):
        assert _n not in _seen,f'{_n} is defined in both {_seen.get(_n)} and {_m}: give one of them another name'
        _seen[_n]=_m
# 1.5.9 (plan 10.1-10.2): self-contained apps are not in the page: each group is a file beside it (www/a/, named by
# its contents), loaded the first time the app is opened. The app's registration (its regApp line) stays in the page,
# and a stub of the same name loads the file, then calls the real one (which the file defines anew). A file that
# cannot load says so, with Try again. lazyApp(group) loads one by hand (WordPro's "Insert a spreadsheet table" and
# "Make a presentation" need SpreadsheetPro's and PresentPro's code).
LAZY={'sheet':(['spreng45.js','sprof45.js','spro45.js'],'sheetApp','SpreadsheetPro'),'deck':(['pprof45.js','ppro45.js'],'deckApp','PresentPro')}
LAZY_CODE={};LAZY_REG={}
for _g,(_fs,_fn,_nm) in LAZY.items():
    _c=''
    for _f in _fs:
        mods.remove(_f);_t=L(_f)
        _r=re.search(r"^regApp\('"+_g+r"',.*$",_t,re.M)
        if _r:LAZY_REG[_g]=_r.group(0);_t=_t[:_r.start()]+'/* (its regApp line is in the page) */'+_t[_r.end():]
        _c+=_t+'\n'
    LAZY_CODE[_g]=_c+'LAZY_DONE.'+_g+'=1;\n'
_lz='const LAZY_DONE={},LAZY_P={};let LAZY={};\n'
_lz+="""function lazyApp(g){if(LAZY_DONE[g])return Promise.resolve();return LAZY_P[g]||(LAZY_P[g]=new Promise((res,rej)=>{const s=document.createElement('script');s.src=LAZY[g];s.async=false;
 s.onload=()=>LAZY_DONE[g]?res():(delete LAZY_P[g],rej(new Error('incomplete')));s.onerror=()=>{delete LAZY_P[g];s.remove();rej(new Error('not found'))};document.head.appendChild(s)}))}
function lazyFail(name,g,retry){try{notify(name+' could not be loaded','Its files could not be read ('+LAZY[g]+'). Try again, or restart if it keeps happening.',{kind:'error',action:retry})}catch(e){}}
"""
for _g,(_fs,_fn,_nm) in LAZY.items():
    _lz+=f"async function {_fn}(...a){{const me={_fn};try{{await lazyApp('{_g}')}}catch(e){{lazyFail('{_nm}','{_g}',()=>{_fn}(...a));return null}}if({_fn}===me)throw new Error('{_nm}: not loaded');return {_fn}(...a)}}\n"
    _lz+=LAZY_REG[_g]+'\n'
changes=open(os.path.join(here,'../CHANGELOG.md')).read()   # "What's new" and the notice after an update
# 1.5.9: Search in Settings: the pages' sections and settings (made by iso/qt/shell2/setsearch59_t.js --write)
_six=os.path.join(here,'settings-index.json');setidx=open(_six).read() if os.path.exists(_six) else '{}'
code='const SETIDX='+json.dumps(json.loads(setidx),ensure_ascii=False).replace('</','<\\/')+';\nconst PINYIN_JSON='+json.dumps(pinyin)+';\nconst RCHANGES='+json.dumps(changes).replace('</','<\\/')+';\n'+'\n'.join(L(m) for m in mods)
code=code+'\n'+_lz
anchor="(function(){const d=document.createElement('div');d.className='ic tr';"
rep(anchor,code+'\n'+anchor)
css=L('styles25.css')+"\n@font-face{font-family:'OpenDyslexic';src:url(data:font/woff2;base64,"+od+") format('woff2');font-weight:400}@font-face{font-family:'OpenDyslexic';src:url(data:font/woff2;base64,"+odb+") format('woff2');font-weight:700}\n"
# 1.4.7: the desktop's own fonts for Display Properties (fonts47.js): Gelasio, Fira Sans, Comic Neue (SIL OFL 1.1)
for fam,fn in (('Gelasio','Gelasio'),('Fira Sans','FiraSans'),('Comic Neue','ComicNeue')):
    for w,st in ((400,'Regular'),(700,'Bold')):
        b64=base64.b64encode(open(os.path.join(here,f'../libs/uifonts/{fn}-{st}.woff'),'rb').read()).decode()
        css+="@font-face{font-family:'%s';src:url(data:font/woff;base64,%s) format('woff');font-weight:%d}\n"%(fam,b64,w)
i=s.index('</style>');s=s[:i]+css+s[i:]
# storage now lives in IndexedDB
rep("const save=()=>{try{localStorage.setItem('blockdos95',JSON.stringify(fs))}catch(e){}};","const save=()=>fsSave();")
# passwords: PBKDF2 with a per-user salt, encrypted homes, security keys
rep("const h=await hsh(p),a=ACCTS.find(x=>x.n.toLowerCase()==n.toLowerCase()&&x.h==h);","let a=await pwFind(n,p);if(a&&a.pk&&!(await passkeyCheck(a))){a=null}")
rep("if(a){USER=a;loginFails=0;","if(a){USER=a;try{await userLogin(a,p)}catch(err){console.warn(err)}loginFails=0;")
# 1.5.0: a failed sign-in goes into the health log (sec50.js; the account's name only when such an account exists)
rep("else{loginFails++;","else{loginFails++;try{hlNote('signin','wrong password for '+(ACCTS.some(x=>x.n.toLowerCase()==n.toLowerCase())?'the account '+n:'an account that does not exist'))}catch(e){}")
rep("if(await hsh(OP.get())!=USER.h)","if(!(await pwCheck(USER,OP.get())))",2)
rep("USER.h=await hsh(p1)","USER.h=await pwMake(p1);await homeRekey(p1)",2)
rep("if(pdel.size){ACCTS=ACCTS.filter(a=>!pdel.has(a));","if(pdel.size){pdel.forEach(a=>{a.id&&kvDel('home:'+a.id)});ACCTS=ACCTS.filter(a=>!pdel.has(a));")
rep("ACCTS.push({n:cn,h:await hsh(cp1),r:cr})","ACCTS.push(await acctNew(cn,cp1,cr))")   # 1.5.0: with its file key and the administrators' copies (acct50.js)
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
rep("const mb=q('.wp-mb');Object.keys(MENUS)","try{wpExtend(W,MENUS,W.__K={get ed(){return ed},get root(){return root},get sheet(){return sheet},q,touch,sched,rs,insertHTML,dialog,say,blocks,textNodes,docText,words,layout,get meta(){return meta},get path(){return p},get dirty(){return dirty},doSave:()=>doSave()});if(typeof offWordPro=='function')offWordPro(W,MENUS,W.__K);W.printDoc=printDoc}catch(e){console.warn('WordPro additions',e)}\n const mb=q('.wp-mb');Object.keys(MENUS)")
rep("  try{const f=document.createElement('iframe');f.style.cssText='position:fixed;right:0;bottom:0;width:0;height:0;border:0';f.srcdoc=html;","  if(typeof wpPrint=='function'&&wpPrint(W,html,{size:meta.size,land:meta.land,title:p?p.at(-1):'Untitled'}))return;\n  try{const f=document.createElement('iframe');f.style.cssText='position:fixed;right:0;bottom:0;width:0;height:0;border:0';f.srcdoc=html;")
rep("function loadTab(t){if(rtLoad(t))return;const u=t.h[t.i],r=page(u);","function loadTab(t){if(rtLoad(t))return;const u=t.h[t.i];let r=PV&&u=='about:home'&&typeof privHome=='function'?privHome(doc,esc):page(u);if(r.ext&&!r.frame&&typeof webWants=='function'&&webWants(u)){webOpen(u);r={title:webHost(u)+' (browser window)',html:webNote(u)}}")
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
# 1.4.4: the taskbar clock's time from the one formatter (clock31.js) the calendar, lock screen and screen saver use
rep("const p2=x=>String(x).padStart(2,'0'),dig=S.h24?p2(h)+':'+p2(m):((h%12)||12)+':'+p2(m)+' '+(h<12?'AM':'PM');","const dig=clockParts(n).time;")
# 1.4.7: Display Properties' fonts are fonts Rudolf OS has (fonts47.js); a font saved by an older version is read as its stand-in
rep("b.style.fontFamily=`'${S.ff}',Tahoma,Arial,sans-serif`","b.style.fontFamily=uiFontStack(S.ff)")
rep("<select id='ff'>${op(['Tahoma','Arial','Verdana','Trebuchet MS','Georgia','Times New Roman','Courier New','Comic Sans MS','Lucida Console'],S.ff)}</select>","<select id='ff'>${uiFontOptions(S.ff)}</select>")
rep("<select id='ff'>${opt(['Tahoma','Arial','Verdana','Trebuchet MS','Georgia','Times New Roman','Courier New','Comic Sans MS','Lucida Console'],S.ff)}</select>","<select id='ff'>${uiFontOptions(S.ff)}</select>")
rep("ff:'Tahoma'","ff:'DejaVu Sans'",2)
# 1.4.7: a tab the memory guard closed (mem47.js) shows Oracle's own page and a notification
rep("gone:why=>{if(t===T())st.textContent='This page stopped working'","gone:why=>{memGone(t,why,{T,sync,reload:()=>loadTab(t),st});if(t===T())st.textContent='This page stopped working'")
# 1.4.6: Oracle's dark pages (web46.js): the window knows it is Oracle, its menu has "Dark pages", its pages follow
rep("W.body.style.background='#fff';W.el.tabIndex=-1;","W.body.style.background='#fff';W.el.tabIndex=-1;W.el.classList.add('oracle');if(typeof webDarkWin=='function')setTimeout(()=>webDarkWin(W),0);",2)
rep("['Extensions',()=>nav('about:extensions')],['About Oracle',()=>nav('about:oracle')]];","['Extensions',()=>nav('about:extensions')],[()=>(WEB_DARK?'✓ ':'')+'Dark pages',()=>webDarkSet(!WEB_DARK)],['About Oracle',()=>nav('about:oracle')]];")
rep("MENU.forEach(([n,f])=>{const r=document.createElement('div');r.textContent=n;","MENU.concat(typeof window.__oracleMenu=='function'?window.__oracleMenu({url:cur(),tab:T()}):[]).forEach(([n,f])=>{const r=document.createElement('div');r.textContent=typeof n=='function'?n():n;")   # (1.5.8: oracle58.js adds Open in Chromium and FedCM)
rep("fr.src=r.frame||u;if(navigator.onLine===false)","fr._html=null;fr.src=r.frame||u;webDarkFrame(fr,true);if(navigator.onLine===false)")
rep("t.src=r.html;fr.srcdoc=r.html}","t.src=r.html;fr._html=r.html;webDarkFrame(fr,false);fr.srcdoc=webDarkHtml(r.html)}")
# 1.4.4: a picture of your own as the background (wallpaper31.js) comes before a dynamic background and the colours
rep("const _u=window.__dyn?dynBg():'';","const _u=(typeof wpCss=='function'&&wpCss())||(window.__dyn?dynBg():'');")
# 1.4.5: the desktop's right-click item opens Settings (it said "Change background…")
rep("['Change background…',()=>settings('disp')]","['Open settings…',()=>settings()]")
s=s.replace('Rudolf Prompt','Command Prompt')
s=s.replace("<title>Rudolf OS</title>","<title>Rudolf OS</title>",1)
# 1.5.9 (plan 10.1): the dynamic backgrounds' pictures and the fonts become files beside the page (www/a/), named by
# their contents, so the browser fetches and decodes one only when it is shown or used, and the page is about a
# megabyte smaller. Only these: DYN's pictures (used as url()s and <img> sources) and @font-face sources.
import hashlib
_A=os.path.join(os.path.dirname(os.path.abspath(O)),'a');os.makedirs(_A,exist_ok=True);_keep=set()
_EXT={'image/webp':'webp','image/jpeg':'jpg','image/png':'png','font/woff2':'woff2','font/woff':'woff'}
def _out(uri):
    m=re.match(r'data:([a-z0-9/+.-]+);base64,(.*)$',uri,re.S)
    if not m or m.group(1) not in _EXT:return uri
    b=base64.b64decode(m.group(2));n=hashlib.sha256(b).hexdigest()[:16]+'.'+_EXT[m.group(1)]
    f=os.path.join(_A,n);_keep.add(n)
    if not os.path.exists(f):open(f,'wb').write(b)
    return 'a/'+n
_i=s.find('const DYN=');_j=s.find('\n',_i)
_m=re.match(r'const DYN=(\{.*\});?$',s[_i:_j])
assert _m,'DYN not found'
_D=json.loads(_m.group(1))
for _k,_v in _D.items():
    for _f in list(_v):
        if isinstance(_v[_f],str) and _v[_f].startswith('data:'):_v[_f]=_out(_v[_f])
s=s[:_i]+'const DYN='+json.dumps(_D,ensure_ascii=False,separators=(',',':'))+';'+s[_j:]
s=re.sub(r"(@font-face\{[^}]*?src:url\()(data:font/[a-z0-9]+;base64,[A-Za-z0-9+/=]+)(\))",lambda m:m.group(1)+_out(m.group(2))+m.group(3),s)
# ...and the SNES core (163 KB of WebAssembly as base64): fetched when a SNES game is first played
_EXT['application/wasm']='wasm'
s=re.sub(r"const EMU_SNES_WASM='([A-Za-z0-9+/=]+)';",lambda m:"const EMU_SNES_URL='"+_out('data:application/wasm;base64,'+m.group(1))+"';",s)
assert 'EMU_SNES_URL' in s
# the lazy groups' files (named by their contents, like the pictures)
_EXT['text/javascript']='js'
_LZ={_g:_out('data:text/javascript;base64,'+base64.b64encode(_c.encode()).decode()) for _g,_c in LAZY_CODE.items()}
s=s.replace("const LAZY_DONE={},LAZY_P={};let LAZY={};","const LAZY_DONE={},LAZY_P={};let LAZY="+json.dumps(_LZ)+";",1)
for _n in os.listdir(_A):
    if _n not in _keep:os.remove(os.path.join(_A,_n))
open(O,'w').write(s);print(len(s),len(_keep),'files in a/')
# 1.5.7: Blitzen's reference material (docsindex.py), beside the desktop
import docsindex;_j=docsindex.build();open(os.path.join(os.path.dirname(os.path.abspath(O)),'blitzen-docs.json'),'w').write(json.dumps(_j,ensure_ascii=False,separators=(',',':')))
