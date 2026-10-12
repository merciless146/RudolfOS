# Make an "installed" Rudolf OS disk image the way install.sh does: the ISO image, then an ext4 RUDOLFDATA
# partition in MBR slot 3.   python3 mkdisk.py ISO OUT.img SIZE_MB
import os,struct,subprocess,sys,tempfile
iso,out,mb=sys.argv[1],sys.argv[2],int(sys.argv[3])
d=open(iso,'rb').read()
blocks=struct.unpack('<I',d[16*2048+80:16*2048+84])[0];img=blocks*4
sectors=mb*2048;start=(img+2047)//2048*2048;size=sectors-start-2048
with open(out,'wb') as f: f.write(d[:blocks*2048]); f.truncate(sectors*512)
with open(out,'r+b') as f:
    f.seek(446+32);f.write(b'\x00\xfe\xff\xff\x83\xfe\xff\xff'+struct.pack('<II',start,size))
part=tempfile.mktemp();open(part,'wb').truncate(size*512)
subprocess.run(['mkfs.ext4','-q','-F','-L','RUDOLFDATA','-m','1',part],check=True)
t=tempfile.mkdtemp();subprocess.run(['mount','-o','loop',part,t],check=True)
for p in ('home/rudolf','os','etc'): os.makedirs(os.path.join(t,p),exist_ok=True)
os.chown(os.path.join(t,'home/rudolf'),1000,1000);open(os.path.join(t,'etc/installed'),'w').write('installed for testing\n')
subprocess.run(['umount',t],check=True)
with open(out,'r+b') as f, open(part,'rb') as p:
    f.seek(start*512)
    while True:
        b=p.read(1<<24)
        if not b:break
        f.write(b)
os.unlink(part);print(out,'image',img//2048,'MiB, RUDOLFDATA',size//2048,'MiB')
