import sys,time,subprocess,os
sys.path.insert(0,'.')
from qmp import QMP
from ser import Ser
N=sys.argv[1];mode=sys.argv[2] if len(sys.argv)>2 else 'bios'
env=dict(os.environ,SERIAL=f'-serial unix:/tmp/ser-{N}.sock,server,nowait')
subprocess.Popen(['./boot.sh',N,mode,os.environ.get('MEDIA','cd')]+os.environ.get('EXTRA','').split(),env=env)
q=QMP(f'/tmp/qmp-{N}.sock');s=Ser(f'/tmp/ser-{N}.sock')
from PIL import Image
if mode=='bios':
    for i in range(40):
        time.sleep(0.5);q.shot(os.path.abspath(f'{N}_m.png'))
        try:
            im=Image.open(f'{N}_m.png').convert('RGB')
            if im.size==(720,400) and im.getpixel((10,200))[2]>150 and im.getpixel((10,200))[0]<20:break
        except Exception:pass
    q.key('d');time.sleep(0.7);q.shot(os.path.abspath(f'{N}_m2.png'));q.key('ret')
out=s.read(float(sys.argv[3]) if len(sys.argv)>3 else 60)
open(f'{N}_serial.txt','w').write(out)
print(out[-1500:])
