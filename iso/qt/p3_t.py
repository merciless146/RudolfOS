# Re-test after fixes: settings endpoints, sound out of the box, single sound server.
import sys,time,os,struct;sys.path.insert(0,'.')
from guest import Guest
N=sys.argv[1];g=Guest(N)
def w(c,t=240):
    e=time.time()+t
    while time.time()<e:
        if g.js(c) is True: return True
        time.sleep(2)
    return False
print('desktop up:',w("typeof rd=='function'"))
print(g.js("rd('display').then(r=>JSON.stringify(r)).catch(e=>'ERR '+e.message)"))
print(g.js("rd('decktest').then(r=>JSON.stringify(r.deck)).catch(e=>'ERR '+e.message)"))
print(g.js("rd('display?mode=soft').then(r=>'GET write allowed!').catch(e=>'GET write refused: '+e.message)"))
print(g.sh("pgrep -a pulseaudio; sed -n '/Node 0x02/,/Node 0x03/p' /proc/asound/card0/codec#0 | grep vals",20))
mark=os.path.getsize(N+'.wav')
print(g.js("(async()=>{const a=new AudioContext();const o=a.createOscillator();o.frequency.value=440;o.connect(a.destination);o.start();await new Promise(r=>setTimeout(r,3000));o.stop();return a.state})()"))
time.sleep(2)
d=open(N+'.wav','rb').read()[mark:];n=len(d)//2;s=struct.unpack('<%dh'%n,d[:n*2])
print('desktop sound: samples',n,'non-silent',sum(1 for x in s if abs(x)>200),'peak',max([abs(x) for x in s] or [0]))
print(g.sh("pgrep -a pulseaudio | wc -l",20))
