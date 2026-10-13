# boot an ISO in QEMU (BIOS, CD, xhci) with the diagnostic start and leave it running: python3 boot_dbg.py NAME ISO
import subprocess,time,os,sys
from PIL import Image
N,ISO=sys.argv[1],sys.argv[2]
for f in (f'/tmp/ser-{N}.sock',f'/tmp/qmp-{N}.sock'):
    try: os.unlink(f)
    except OSError: pass
env=dict(os.environ,SERIAL=f'-serial unix:/tmp/ser-{N}.sock,server,nowait')
subprocess.run(f'sed "s#^ISO=.*#ISO={ISO}#" boot.sh > boot111.sh',shell=True)
subprocess.Popen(['setsid','bash','boot111.sh',N,'bios','cd','-device','qemu-xhci,id=xh'],env=env)
time.sleep(2)
from qmp import QMP
q=QMP(f'/tmp/qmp-{N}.sock');t=time.time()
while time.time()-t<120:
    time.sleep(0.4)
    try:
        q.shot(os.path.abspath(f'{N}_bm.png'));im=Image.open(f'{N}_bm.png').convert('RGB')
        if im.size==(720,400) and im.getpixel((10,200))[2]>150 and im.getpixel((10,200))[0]<20:
            q.key('d');time.sleep(0.7);q.key('ret');print('menu',int(time.time()-t));break
    except Exception: pass
