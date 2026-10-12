# rpk on the real image: disc repository, install, run, web app, remove, tamper refusal.
import sys,time,os,json;sys.path.insert(0,'.')
from guest import Guest
N=sys.argv[1];g=Guest(N)
def w(c,t=300):
    e=time.time()+t
    while time.time()<e:
        if g.js(c) is True: return True
        time.sleep(3)
    return False
def ok(n,c,d=''):print(('PASS ' if c else 'FAIL ')+n+(' - '+str(d)[:300] if d else ''),flush=True)
ok('desktop up',w("typeof rd=='function'&&!!RD"))
print(g.sh("ls /live/media/rudolf/pkg; rpk source list; cat /etc/rudolf/rpk-keys | cut -c1-12",30))
st=g.js("rd('pkg/status').then(r=>JSON.stringify(r)).catch(e=>'ERR '+e.message)")
ok('status lists disc packages','"sudoku"' in str(st) and '"jq"' in str(st),str(st)[:300])
JOB="(async()=>{await rdPost('pkg/install?name=jq,file,sudoku');for(let i=0;i<60;i++){await new Promise(r=>setTimeout(r,1000));const j=await rd('job');if(!j.running)return j.status+'|'+j.log}})().catch(e=>'ERR '+e.message)"
r=g.js(JOB,120);ok('install from disc',str(r).startswith('0|'),r)
print(g.sh("grep ' /pkg/' /proc/mounts; ls /run/rudolf/pkg/store",20))
r=g.js("rd('pkg/exec?name=jq&args='+encodeURIComponent(['-c','map(.*2)'].join('\\x1f')),{method:'POST',body:'[1,2,3]'}).then(r=>atob(r.out64)+'|'+r.code+'|'+r.err)")
ok('jq runs on the image',str(r).startswith('[2,4,6]'),r)
r=g.js("rd('pkg/exec?name=file&args='+encodeURIComponent(['-b','/usr/bin/rpk'].join('\\x1f')),{method:'POST',body:''}).then(r=>atob(r.out64)+'|'+r.code+'|'+r.err)")
ok('file runs (with its magic database)','ELF 64-bit' in str(r),r)
r=g.js("fetch('/pkg/sudoku/www/index.html').then(r=>r.status+' '+r.headers.get('content-type'))")
ok('web app served',str(r).startswith('200'),r)
r=g.sh("su -s /bin/sh rudolf -c 'id -u; rpk run jq -n 1+1'",20);ok('runs as the desktop user',r.strip().endswith('2'),r)
# a tampered package is refused
g.sh("cp /run/rudolf/pkg/store/jq.rpk /tmp/bad.rpk; printf X | dd of=/tmp/bad.rpk bs=1 seek=9000 conv=notrunc 2>/dev/null",20)
r=g.sh("rpk install-file /tmp/bad.rpk; echo rc=$?",30);ok('tampered package refused','refused' in r and 'rc=1' in r,r)
r=g.js("(async()=>{await rdPost('pkg/remove?name=file');for(let i=0;i<30;i++){await new Promise(r=>setTimeout(r,1000));const j=await rd('job');if(!j.running)return j.status+'|'+j.log}})()",60)
ok('remove',str(r).startswith('0|Removed file'),r)
print(g.sh("grep -c ' /pkg/' /proc/mounts; df -h /run | tail -1",20))
# desktop: Packages app and the Start menu entry for the web app
r=g.js("(async()=>{await pkgStatus(true);launch('packages');await new Promise(r=>setTimeout(r,2500));return [document.querySelectorAll('.pkrow').length,!!APPS['pkg:sudoku']]})()",30)
ok('Packages app and Start menu entry',r==[3,True],r)
g.shot(N+'_pkgs.png')
r=g.js("(async()=>{launch('pkg:sudoku');await new Promise(r=>setTimeout(r,3000));const f=[...document.querySelectorAll('.win iframe')].find(f=>/\\/pkg\\/sudoku/.test(f.src));return !!f})()",30)
ok('Sudoku window',r is True,r);time.sleep(2);g.shot(N+'_sudoku.png')
