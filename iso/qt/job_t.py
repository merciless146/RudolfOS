import sys,time;sys.path.insert(0,'.')
from guest import Guest
g=Guest(sys.argv[1])
g.sh("touch /run/rudolf/persistent")
print(g.js("rdPost('native/install?id=org.vinegarhq.Sober').then(r=>JSON.stringify(r)).catch(e=>'ERR '+e.message)"))
for i in range(12):
    time.sleep(5);j=g.js("rd('job').then(j=>JSON.stringify(j))")
    if '"running":false' in str(j): break
print(j)
print(g.js("rdPost('native/run?id=org.vinegarhq.Sober').then(r=>JSON.stringify(r)).catch(e=>'ERR '+e.message)"))
g.sh("rm /run/rudolf/persistent")
