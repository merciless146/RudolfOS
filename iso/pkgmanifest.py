# Find which Debian/Ubuntu packages the files copied into the root file system came from,
# and write the package list, versions and licence texts (for the licences / source offer).
# Usage: python3 pkgmanifest.py ROOTFS OUTDIR     (GPL-2.0-or-later)
import os,sys,glob,subprocess
root,out=sys.argv[1],sys.argv[2]
own={}
for lst in glob.glob('/var/lib/dpkg/info/*.list'):
    pkg=os.path.basename(lst)[:-5].split(':')[0]
    for l in open(lst,errors='replace'):
        own.setdefault(os.path.realpath(l.strip()) if os.path.islink(l.strip()) else l.strip(),pkg); own.setdefault(l.strip(),pkg)
pk=set()
for top in ['usr/lib/x86_64-linux-gnu','lib64','usr/bin','usr/sbin','usr/share/X11','usr/share/fonts','usr/share/alsa','usr/share/icons','usr/share/zoneinfo','etc/fonts']:
    for dp,dn,fn in os.walk(os.path.join(root,top)):
        for f in fn:
            if os.path.islink(os.path.join(dp,f)): continue
            rel='/'+os.path.relpath(os.path.join(dp,f),root)
            for c in (rel,rel.replace('/usr/lib/','/lib/',1),rel.replace('/lib64/','/usr/lib64/',1),'/usr'+rel):
                if c in own: pk.add(own[c]);break
            else:
                b=os.path.basename(rel)
                for c in ('/usr/lib/x86_64-linux-gnu/'+b,'/lib/x86_64-linux-gnu/'+b):
                    if c in own: pk.add(own[c]);break
pk=sorted(pk)
q=subprocess.run(['dpkg-query','-W','-f=${Package}\t${Version}\t${source:Package}\t${source:Version}\n']+pk,capture_output=True,text=True).stdout
os.makedirs(out,exist_ok=True)
with open(os.path.join(out,'packages-versions.txt'),'w') as f:
    f.write('Binary packages from Ubuntu 24.04 (noble) included in Rudolf OS, with their source packages.\n')
    f.write('Corresponding source: https://launchpad.net/ubuntu/+source/<source>/<version>\n\n')
    f.write('package\tversion\tsource\tsource-version\n'+q)
with open(os.path.join(out,'third-party-licences.txt'),'w') as f:
    for p in pk:
        c='/usr/share/doc/%s/copyright'%p
        if os.path.isfile(c):
            f.write('='*70+'\n'+p+'\n'+'='*70+'\n'+open(c,errors='replace').read()+'\n\n')
print(len(pk),'packages')
