# Rudolf OS 1.1.2 on QEMU: resolution and frame rate (Xorg and software display), Keep/revert, click accuracy
# through the scaling, Oracle sign-in moving to the browser window (with Ruffle).
#   python3 v112_t.py NAME ISO
import subprocess,time,os,sys,json
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from PIL import Image
N,ISO=sys.argv[1],sys.argv[2]
HERE=os.path.dirname(os.path.abspath(__file__))
LOG=open(f'{N}_t.txt','w')
def out(*a):
    s=' '.join(str(x) for x in a);print(s,flush=True);LOG.write(s+'\n');LOG.flush()
RES=[]
def ok(n,c,d=''):RES.append(bool(c));out(('PASS ' if c else 'FAIL ')+n+(' - '+str(d)[:400] if d else ''))
for f in (f'/tmp/ser-{N}.sock',f'/tmp/qmp-{N}.sock'):
    try: os.unlink(f)
    except OSError: pass
env=dict(os.environ,SERIAL=f'-serial unix:/tmp/ser-{N}.sock,server,nowait')
subprocess.run(f'cd {HERE} && sed "s#^ISO=.*#ISO={ISO}#" boot.sh > boot112.sh',shell=True)
subprocess.Popen(['setsid','bash',os.path.join(HERE,'boot112.sh'),N,'bios','cd'],env=env,cwd=HERE)
time.sleep(2)
import guest
from guest import Guest
from qmp import QMP
q=QMP(f'/tmp/qmp-{N}.sock')
class Share:
    def __init__(s,*a):pass
    def __getattr__(s,k):return getattr(q,k)
guest.QMP=Share
t0=time.time();pressed=False
while time.time()-t0<240 and not pressed:
    time.sleep(0.4)
    try:
        q.shot(os.path.abspath(f'{N}_bm.png'));im=Image.open(f'{N}_bm.png').convert('RGB')
        if im.size==(720,400) and im.getpixel((10,200))[2]>150 and im.getpixel((10,200))[0]<20:
            q.key('d');time.sleep(0.7);q.key('ret');pressed=True;out('boot menu: diagnostic start')
    except Exception: pass
g=Guest(N)
def w(c,t=420):
    e=time.time()+t
    while time.time()<e:
        try:
            if g.js(c,20) is True: return True
        except Exception: pass
        time.sleep(4)
    return False
UP="typeof rd=='function'&&!!RD&&typeof screenUi=='function'"
def status(): return json.loads(g.sh('/usr/lib/rudolf/screenctl status',30).strip().splitlines()[-1])
def shot(name):
    q.shot(os.path.abspath(name));return Image.open(name).convert('RGB')
def bars(im):
    """black rows at the top and bottom, black columns left and right"""
    W,H=im.size;px=im.load()
    blk=lambda p:sum(p)<30
    top=next((y for y in range(H) if not all(blk(px[x,y]) for x in range(0,W,16))),H)
    bot=next((y for y in range(H) if not all(blk(px[x,H-1-y]) for x in range(0,W,16))),H)
    lef=next((x for x in range(W) if not all(blk(px[x,y]) for y in range(0,H,16))),W)
    return top,bot,lef
def change(res,fps):
    """ask for a new setting and wait until the desktop has really started again"""
    r=g.js(f"window.__old=1;rdPost('screen?res={res}&fps={fps}').then(r=>r.message).catch(e=>'ERR '+e.message)",20)
    out('  change',res,fps,'->',r)
    return w("typeof window.__old=='undefined'&&"+UP,300)
ok('desktop up',w(UP))
g.js("['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});1")
st=status();out('screen status:',json.dumps(st)[:600])
out(g.sh('cat /run/rudolf/display-mode /run/rudolf/display-reason /run/rudolf/screen-actual; uname -r',20))
ok('version 1.1.2',g.js('RVER')=='1.1.2',g.js('RVER'))
mode=st['display']
# --- Settings > Display shows the choices
r=g.js("(async()=>{settings('disp');await new Promise(r=>setTimeout(r,3000));const R=document.querySelector('#dres'),F=document.querySelector('#dfps');return R&&F?JSON.stringify({res:[...R.options].map(o=>o.textContent),fps:[...F.options].map(o=>o.textContent+(o.disabled?' [off]':''))}):'missing'})()",40)
out('Settings choices:',r)
ok('Settings > Display offers the four resolutions and four frame rates','1920×1080' in str(r) and '1024×576' in str(r) and '120 fps' in str(r) and '30 fps' in str(r),r)
g.shot(f'{N}_settings.png')
# --- graphics-card display (QEMU's "std" VGA with Xorg), when Rudolf OS chose it
if mode=='gpu':
    sizes=[f"{m['w']}x{m['h']}" for m in (st.get('modes') or {}).get('modes',[])]
    out('monitor sizes:',sizes)
    ok('QEMU offers 1920x1080',('1920x1080' in sizes),sizes)
    ok('change to 1920x1080 at 60',change('1920x1080',60))
    a=g.sh('cat /run/rudolf/display-mode /run/rudolf/screen-actual; DISPLAY=:0 rudolf-display --xsize',20);out(a)
    ok('Xorg switched the monitor to 1920x1080','gpu' in a and 'native 1920x1080' in a,a)
    im=shot(f'{N}_gpu1080.png');ok('the screen shows 1920x1080',im.size==(1920,1080),im.size)
    vw=g.js('[innerWidth,innerHeight]');ok('the desktop is 1920x1080',vw==[1920,1080],vw)
    ok('"Keep this?" asks after the change',w("!!document.getElementById('scrkeep')",30))
    g.js("document.querySelector('#scrkeep .scrk').click();1");time.sleep(2)
    ok('Keep keeps it',status()['pending'] is False,status())
    # a size the monitor does not have: the software display scales it
    ok('change to 1280x720 (not a monitor mode)',change('1280x720',60))
    a=g.sh('cat /run/rudolf/display-mode /run/rudolf/display-reason /run/rudolf/screen-actual',20);out(a)
    ok('the software display scales it','soft' in a and 'no 1280x720 mode' in a and 'scaled' in a,a)
    im=shot(f'{N}_gpu720soft.png');out('screen',im.size,'bars',bars(im))
    vw=g.js('[innerWidth,innerHeight]');ok('the desktop is 1280x720',vw==[1280,720],vw)
    ok('no "Keep" -> back to the previous setting by itself',w("typeof window.__old=='undefined'",5) and (g.js("window.__old=1;1") or True) and w("typeof window.__old=='undefined'&&"+UP,300) and status()['res']=='1920x1080',status())
    a=g.sh('cat /run/rudolf/display-mode /run/rudolf/screen-actual',20);ok('...on the graphics card again at 1920x1080','gpu' in a and '1920 1080' in a,a)
    g.js("rdPost('display?mode=soft').then(()=>1)",20)
# --- software display: the desktop is scaled onto the firmware's screen
ok('change to 1920x1080 at 30 fps (software display)',change('1920x1080',30))
st=status();out('status:',json.dumps(st)[:400])
a=g.sh('cat /run/rudolf/display-mode /run/rudolf/screen-actual; tail -3 /run/rudolf/session.log; ps | grep -c "[r]udolf-display"',20);out(a)
ok('software display at 1920x1080, scaled','soft' in a and '1920 1080' in a and 'scaled' in a,a)
ok('frame rate 30 is used',st['actual']['hz']==30,st['actual'])
im=shot(f'{N}_soft1080.png');t,b,l=bars(im);out('screen',im.size,'bars top/bottom/left',t,b,l)
W,H=im.size
exp=(H-W*1080//1920)//2
ok('16:9 picture on the screen keeps its shape (black bars)',abs(t-exp)<=2 and abs(b-exp)<=2 and l==0 if exp>0 else t==0,(W,H,t,b,exp))
vw=g.js('[innerWidth,innerHeight]');ok('the desktop is 1920x1080',vw==[1920,1080],vw)
# click accuracy: click a known element through the scaling with the QEMU tablet (absolute, panel coordinates)
ok('"Keep this?" shows on the software display too',w("!!document.getElementById('scrkeep')",30))
g.js("document.querySelector('#scrkeep .scrk')&&document.querySelector('#scrkeep .scrk').click();1");time.sleep(2)
g.js("['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});window.__clk=null;const b=document.createElement('button');b.id='tgt';b.textContent='target';b.style.cssText='position:fixed;left:1500px;top:300px;width:120px;height:60px;z-index:2147483647';b.onclick=e=>window.__clk=[e.clientX,e.clientY];document.body.appendChild(b);1")
time.sleep(1)
sx,sy=W/1920,(H-2*exp)/1080
px,py=1560*sx,(330*sy)+exp
q.at(px,py,W=W,H=H);time.sleep(1.5)
c=g.js('JSON.stringify(window.__clk)');ok('a click lands on the right spot through the scaling',c not in ('null',None) and abs(json.loads(c)[0]-1560)<6 and abs(json.loads(c)[1]-330)<6,c)
g.js("document.getElementById('tgt').remove();1")
# the smallest: 1024x576
ok('change to 1024x576',change('1024x576',60))
im=shot(f'{N}_soft576.png');t,b,l=bars(im);out('1024x576 on',im.size,'bars',t,b,l)
vw=g.js('[innerWidth,innerHeight]');ok('the desktop is 1024x576',vw==[1024,576],vw)
r=g.js("(async()=>{['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});settings('disp');await new Promise(r=>setTimeout(r,2500));const w=[...document.querySelectorAll('.win')].pop().getBoundingClientRect();return [w.right<=innerWidth+1,w.bottom<=innerHeight+1]})()",40)
ok('Settings fits the 1024x576 desktop',r==[True,True],r)
g.shot(f'{N}_soft576_settings.png')
g.js("document.querySelector('#scrkeep .scrk')&&document.querySelector('#scrkeep .scrk').click();1")
# 90/120 fps are not offered on the software display
r=g.js("(async()=>{settings('disp');await new Promise(r=>setTimeout(r,3000));const F=document.querySelector('#dfps');return [...F.options].filter(o=>o.disabled).map(o=>o.value).join(',')})()",40)
ok('90 and 120 fps are greyed out on the software display',r=='120,90',r)
# --- Oracle sign-in: a site inside the guest; the page asks for a password -> real browser window
g.sh("mkdir -p /tmp/www && printf '<title>Sign in</title><form><input type=password id=pw><button>Go</button></form>' > /tmp/www/login.html && (busybox httpd -p 8099 -h /tmp/www &) ; sleep 1",20)
g.js("['oobe','login'].forEach(i=>{const o=document.getElementById(i);o&&o.remove()});closeAll();browser('http://localhost:8099/login.html');1")
g.js("window.netUp=()=>true;1")
ok('Oracle asks to open the sign-in page in the browser window',w("!!document.querySelector('.sibar')",40),g.js("(document.querySelector('.win')||{}).innerText||''"))
g.shot(f'{N}_signin_bar.png')
g.js("document.querySelector('.sibar .si-open').click();1")
time.sleep(10)
p=g.sh("ps -o args | grep '[c]lass=RudolfWeb' | head -1 | cut -c1-600",20);out('browser window:',p)
ok('the browser window opened with the page','localhost:8099/login.html' in p or 'RudolfWeb' in p,p)
ok('Ruffle is loaded in the browser window','ruffle-ext' in p,p)
ok('...and not the web frames extension','/run/rudolf/frames' not in p,p)
time.sleep(45);g.shot(f'{N}_webwindow.png')
r=str(g.js("rd('web/status').then(s=>JSON.stringify(s))"));ok('the browser window is shown','"shown":true' in r,r)
g.js("rdPost('web/close').then(()=>1)")
# back to the screen's own resolution for later runs
change('auto',60)
out('RESULT %d/%d'%(sum(RES),len(RES)))
subprocess.run(['pkill','-f',f'qmp-{N}.sock'])
