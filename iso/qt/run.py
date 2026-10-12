# Boot an image without touching the boot menu and take screenshots: run.py NAME MODE MEDIA SECONDS
import sys,time,subprocess,os
sys.path.insert(0,'.')
from qmp import QMP
N,mode,media=sys.argv[1:4];T=int(sys.argv[4]) if len(sys.argv)>4 else 240
subprocess.Popen(['./boot.sh',N,mode,media])
q=QMP(f'/tmp/qmp-{N}.sock');t0=time.time();i=0
while time.time()-t0<T:
    time.sleep(15);i+=1;q.shot(os.path.abspath(f'{N}_{i:02d}.png'))
print('done',i)
