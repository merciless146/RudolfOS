# Runs the whole system-update test: QEMU on an installed disk, release disk attached; presses D at every
# boot menu (diagnostic start keeps the serial shell), then phases A-D of upd_t.py.
import subprocess,threading,time,os,sys
sys.path.insert(0,'.')
from PIL import Image
N="m1";env=dict(os.environ,SERIAL=f'-serial unix:/tmp/ser-{N}.sock,server,nowait')
for f in (f'/tmp/ser-{N}.sock',f'/tmp/qmp-{N}.sock'):
    try: os.unlink(f)
    except OSError: pass
subprocess.Popen(['./boot.sh',N,'bios','none','-drive','file=/tmp/upd-disk.img,format=raw,if=virtio','-drive','file=/tmp/rel.img,format=raw,if=virtio'],env=env)
time.sleep(3)
from qmp import QMP
q=QMP(f'/tmp/qmp-{N}.sock');LOG=open(f'{N}_drive.txt','w')
def log(s):LOG.write(time.strftime('%H:%M:%S ')+s+'\n');LOG.flush()
stop=False
def menus():
    seen=0
    while not stop:
        time.sleep(1)
        try:
            q.shot(os.path.abspath(f'{N}_bm.png'));im=Image.open(f'{N}_bm.png').convert('RGB')
            if im.size==(720,400) and im.getpixel((10,200))[2]>150 and im.getpixel((10,200))[0]<20:
                if time.time()-seen>20: q.key('d');time.sleep(0.7);q.key('ret');seen=time.time();log('boot menu: diagnostic start')
        except Exception as e: pass
threading.Thread(target=menus,daemon=True).start()
for ph in "M":
    log('phase '+ph)
    r=subprocess.run(['python3','upd_t.py',N,ph],capture_output=True,text=True,timeout=2400)
    log(r.stdout[-4000:]+r.stderr[-1500:])
    time.sleep(20)
stop=True
try:q.shot(os.path.abspath(f'{N}_final.png'))
except Exception as e:log(str(e))
log('done')
subprocess.run(['pkill','-f','qemu-system-x86_64 .*qmp-m1'])
