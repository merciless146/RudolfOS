# Rudolf OS 1.1.1 on QEMU: USB hotplug (drive, keyboard, mouse), File Explorer, eject, Settings > Devices,
# a virtual laptop touchpad (vtouchpad, carried on the USB stick), the in-page file picker, What's new.
#   python3 v111_t.py NAME MODE(bios|uefi) ISO
import subprocess,time,os,sys,json
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from PIL import Image
N,MODE,ISO=sys.argv[1],sys.argv[2],sys.argv[3]
HERE=os.path.dirname(os.path.abspath(__file__))
LOG=open(f'{N}_t.txt','w')
def out(*a):
    s=' '.join(str(x) for x in a);print(s,flush=True);LOG.write(s+'\n');LOG.flush()
RES=[]
def ok(n,c,d=''):RES.append(bool(c));out(('PASS ' if c else 'FAIL ')+n+(' - '+str(d)[:400] if d else ''))
for f in (f'/tmp/ser-{N}.sock',f'/tmp/qmp-{N}.sock'):
    try: os.unlink(f)
    except OSError: pass
# the USB stick: a FAT disk with a text file and the touchpad tester
stick=f'/tmp/stick-{N}.img'
open('/tmp/hello.txt','w').write('Hello from a USB stick\n')
subprocess.run(['python3','/home/claude/rudolfos/iso/mkimage.py','fat',stick,'/tmp/hello.txt:HELLO.TXT','/tmp/vtouchpad:VTOUCH'],check=True)
env=dict(os.environ,SERIAL=f'-serial unix:/tmp/ser-{N}.sock,server,nowait',ISO=ISO)
subprocess.Popen(['bash','-c',f'cd {HERE} && sed "s#^ISO=.*#ISO=$ISO#" boot.sh > boot111.sh && bash boot111.sh {N} {MODE} cd -device qemu-xhci,id=xh'],env=env)
time.sleep(3)
import guest
from guest import Guest
from qmp import QMP
q=QMP(f'/tmp/qmp-{N}.sock')
class Share:  # one QMP connection for the menu watcher and the tests
    def __init__(s,*a):pass
    def __getattr__(s,k):return getattr(q,k)
guest.QMP=Share
# boot menu: diagnostic start (keeps the serial shell)
t0=time.time();pressed=False
while time.time()-t0<240 and not pressed:
    time.sleep(0.4)
    try:
        q.shot(os.path.abspath(f'{N}_bm.png'));im=Image.open(f'{N}_bm.png').convert('RGB')
        if im.size==(720,400) and im.getpixel((10,200))[2]>150 and im.getpixel((10,200))[0]<20:
            q.key('d');time.sleep(0.7);q.key('ret');pressed=True;out('boot menu: diagnostic start after %ds'%(time.time()-t0))
    except Exception: pass
if not pressed:
    try: q.shot(os.path.abspath(f'{N}_nomenu.png'))
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
ok('desktop up',w("typeof rd=='function'&&!!RD&&typeof devEvents=='function'"))
out(g.sh('cat /live/media/rudolf/release 2>/dev/null || cat /etc/rudolf/release; uname -r; cat /usr/share/rudolf/VERSION',20))
ok('version 1.1.1',g.js('RVER')=='1.1.1',g.js('RVER'))
# kernel drivers for laptop touchpads (built in)
r=g.sh('ls /sys/bus/platform/drivers | grep -ciE "intel-lpss|cherryview|baytrail|amd_gpio|sunrisepoint|tigerlake|alderlake|meteorlake" ; ls /sys/bus/i2c/drivers; ls /sys/bus/pci/drivers | grep -i lpss',20)
out(r);ok('touchpad drivers present','i2c_hid_acpi' in r and 'elan_i2c' in r and 'intel-lpss' in r,r)
g.sh('mkdir -p /tmp/s; grep -c . /etc/mdev.conf',10)
# record notifications and pointer events
g.js("window.__N=[];window.__E={move:0,down:[0,0,0],wheel:0,wheelx:0};{const o=notify;notify=function(t,b,p){__N.push(t+' | '+b);return o.apply(this,arguments)}};"
     "addEventListener('mousemove',()=>__E.move++,true);addEventListener('mousedown',e=>__E.down[e.button]++,true);addEventListener('wheel',e=>{if(e.deltaY)__E.wheel+=Math.sign(e.deltaY);if(e.deltaX)__E.wheelx++},{capture:true,passive:true});1")
time.sleep(4)
# --- USB stick
q.cmd('blockdev-add',**{'driver':'raw','node-name':'stick','file':{'driver':'file','filename':stick}})
r=q.cmd('device_add',driver='usb-storage',drive='stick',id='stk',bus='xh.0',serial='RUDOLFTEST');out('plug stick',r)
ok('stick mounted',w("rd('devices').then(d=>d.drives.some(v=>v.path=='/media/sda'))",60),g.sh('cat /run/rudolf/drives/* 2>&1; grep media /proc/mounts; tail -5 /run/rudolf/events',20))
m=g.sh('grep " /media/sda " /proc/mounts',10);ok('mounted nosuid,nodev,noexec','nosuid' in m and 'nodev' in m and 'noexec' in m,m)
ok('drive notification',w("__N.some(t=>/is connected/.test(t))",30),g.js('JSON.stringify(__N)'))
d=g.js("rd('devices').then(d=>JSON.stringify(d))");out('devices:',d)
# the notification's action opens File Explorer on the drive
r=g.js("(async()=>{devOpenDrive({dev:'sda',path:'/media/sda',label:'',model:'QEMU'});await new Promise(r=>setTimeout(r,3500));return [...document.querySelectorAll('.win')].map(w=>w.innerText).join('\\n').slice(0,2000)})()",40)
ok('File Explorer shows the stick',isinstance(r,str) and 'HELLO.TXT' in r.upper(),r)
time.sleep(1);g.shot(f'{N}_explorer.png')
r=g.js("(()=>{const t=[...document.querySelectorAll('.win')].map(w=>w.innerText).join('\\n');return [/RUDOLF EFI \\(8\\.00 MB\\)/.test(t),!!document.querySelector('.xej')]})()")
ok('Drives list shows the stick with Eject',r==[True,True],r)
r=g.js("fetch('/api/fs/read?path='+encodeURIComponent('/media/sda/HELLO.TXT'),{headers:{'X-Rudolf-Token':RD.token||RD}}).then(r=>r.text()).catch(e=>'ERR '+e.message)")
out('read file via api:',str(r)[:200])
# --- touchpad, from the stick
g.sh('cp /media/sda/VTOUCH /tmp/vtouch 2>/dev/null || cp /media/sda/vtouch /tmp/vtouch; chmod +x /tmp/vtouch',20)
r=g.js("(async()=>{const i=document.createElement('input');i.type='file';i.multiple=true;i.click();await new Promise(r=>setTimeout(r,2500));const p=document.querySelector('.rfpick');const t=p?p.innerText:'';p&&p.querySelector('.rfcancel').click();return t})()",30)
ok('in-page file picker lists Downloads and the stick',isinstance(r,str) and 'Downloads' in r and ('RUDOLF EFI' in r or 'QEMU' in r or 'USB' in r),r)
g.js("closeAll();1")
time.sleep(1)
g.js("__E={move:0,down:[0,0,0],wheel:0,wheelx:0};1")
r=g.sh('/tmp/vtouch move',30);out(r)
e=g.js("JSON.stringify(__E)");ok('touchpad moves the pointer',json.loads(e)['move']>3,e)
inp=g.sh('cat /run/rudolf/inputs',10);out('inputs:',inp)
r=g.sh('/tmp/vtouch tap wait tap2 wait tap3 wait scroll',60);out(r)
e=json.loads(g.js("JSON.stringify(__E)"))
ok('tap = left click',e['down'][0]>=1,e);ok('two-finger tap = right click',e['down'][2]>=1,e);ok('three-finger tap = middle click',e['down'][1]>=1,e)
ok('two-finger scroll',abs(e['wheel'])>=2,e)
g.js("document.querySelectorAll('.ctxmenu,.menu').forEach(m=>m.remove());1")
g.js("__E={move:0,down:[0,0,0],wheel:0,wheelx:0};1")
r=g.sh('/tmp/vtouch rclick',30);e=json.loads(g.js("JSON.stringify(__E)"));ok('clickpad bottom-right = right click',e['down'][2]>=1 and e['down'][0]==0,e)
# tap to click off (Settings > Mouse and touchpad)
g.js("S.ptr=Object.assign({},S.ptr||{},{tap:false});pointerApply();1");time.sleep(1.5)
out(g.sh('cat /run/rudolf/pointer',10))
g.js("__E={move:0,down:[0,0,0],wheel:0,wheelx:0};1");g.sh('/tmp/vtouch tap',30)
e=json.loads(g.js("JSON.stringify(__E)"));ok('tap to click can be turned off',e['down'][0]==0,e)
g.js("S.ptr.tap=true;pointerApply();1")
# --- keyboard and mouse hotplug
q.cmd('device_add',driver='usb-kbd',id='kb',bus='xh.0');q.cmd('device_add',driver='usb-mouse',id='ms',bus='xh.0')
ok('keyboard and mouse recognised',w("__N.some(t=>/Keyboard connected/.test(t))&&__N.some(t=>/Mouse connected/.test(t))",40),g.js('JSON.stringify(__N)'))
# --- Settings > Devices and Mouse and touchpad
r=g.js("(async()=>{settings('devs');await new Promise(r=>setTimeout(r,3000));return document.querySelector('.win:last-of-type')?document.body.innerText.slice(-3000):''})()",40)
ok('Settings > Devices lists the stick and inputs','QEMU' in str(r) and 'eyboard' in str(r),str(r)[-800:])
g.shot(f'{N}_devices.png')
r=g.js("(async()=>{settings('mouse');await new Promise(r=>setTimeout(r,2000));return /Tap to click/i.test(document.body.innerText)})()",30);ok('Settings > Mouse and touchpad',r is True,r)
g.shot(f'{N}_mouse.png')
# --- eject, then unplug
r=g.js("rdPost('eject?dev=sda').then(r=>JSON.stringify(r)).catch(e=>'ERR '+e.message)");out('eject',r)
ok('eject unmounts',w("rd('devices').then(d=>!d.drives.length)",30) and 'sda' not in g.sh('grep media /proc/mounts',10))
ok('safe-to-remove notification',w("__N.some(t=>/can be removed/.test(t))",20),g.js('JSON.stringify(__N)'))
q.cmd('device_del',id='stk');q.cmd('device_del',id='kb');time.sleep(5)
ok('keyboard removal noticed',w("__N.some(t=>/Keyboard disconnected/.test(t))",30),g.js('JSON.stringify(__N)'))
# pulled out without eject
q.cmd('device_add',driver='usb-storage',drive='stick',id='stk2',bus='xh.0')
ok('stick again',w("rd('devices').then(d=>d.drives.length==1)",60))
q.cmd('device_del',id='stk2')
ok('unplugged without eject',w("__N.some(t=>/was removed/.test(t))",40) and w("rd('devices').then(d=>!d.drives.length)",20),g.js('JSON.stringify(__N)'))
r=g.js("(async()=>{whatsNew();await new Promise(r=>setTimeout(r,2000));return /1\\.1\\.1/.test(document.body.innerText)&&/touchpad/i.test(document.body.innerText)})()",30)
ok("What's new shows 1.1.1",r is True,r)
g.shot(f'{N}_final.png')
out(g.sh('dmesg | grep -iE "error|fail" | tail -8; cat /run/rudolf/events | tail -12',20))
out('RESULT %d/%d'%(sum(RES),len(RES)))
subprocess.run(['pkill','-f',f'qmp-{N}.sock'])
