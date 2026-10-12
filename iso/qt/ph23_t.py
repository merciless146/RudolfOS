# Phase 2 (Xorg on the graphics card) and phase 3 (sound) checks on a debug-booted guest.
# python3 ph23_t.py NAME
import sys,time,json;sys.path.insert(0,'.')
from guest import Guest
N=sys.argv[1];g=Guest(N)
def sh(c,t=60,show=True):
    o=g.sh(c,t)
    if show: print('$',c[:120],'\n'+o.strip()[-1800:])
    return o
def w(c,t=150):
    e=time.time()+t
    while time.time()<e:
        if g.js(c) is True: return True
        time.sleep(2)
    return False
print('== display path')
sh("cat /run/rudolf/display-mode /run/rudolf/display-reason /run/rudolf/gpu; pgrep -a Xorg | cut -c1-90; pgrep -a rudolf-display")
sh("grep -E 'modeset\\(0\\)|glamor|EE\\)|Output|Modeline|DRI3' /run/rudolf/Xorg.log | head -25")
sh("DISPLAY=:0 rudolf-display --xsize; cat /run/rudolf/screen")
print('desktop up:',w("typeof rd=='function'&&!!document.body",240))
print(g.js("rd('display').then(r=>JSON.stringify(r)).catch(e=>'ERR '+e.message)"))
print(g.js("fetch('/api/screenshot',{headers:{'X-Rudolf-Token':RUDOLFD.token}}).then(r=>r.blob()).then(b=>createImageBitmap(b)).then(i=>i.width+'x'+i.height).catch(e=>'ERR '+e.message)"))
g.shot(N+'_desk.png')
print('== input through rudolf-display on Xorg (click the Start button)')
g.js("document.getElementById('login')&&(document.getElementById('login').style.display='none');document.getElementById('oobe')&&document.getElementById('oobe').remove()")
g.click_sel('#start');time.sleep(2)
print('start menu open:',g.js("getComputedStyle(document.getElementById('menu')).display!='none'"))
g.shot(N+'_menu.png');g.key('esc')
print('== game window on Xorg')
print(g.put('xwin','/tmp/xwin'))
sh("su -s /bin/sh rudolf -c 'DISPLAY=:0 /tmp/xwin > /tmp/xwin.log 2>&1 &'; sleep 5; tail -1 /tmp/xwin.log; cat /run/rudolf/wm")
g.shot(N+'_game.png')
g.key('meta_l','esc');time.sleep(3)
sh("tail -1 /tmp/xwin.log; cat /run/rudolf/wm; pkill xwin")
print('== sound server')
sh("/usr/lib/rudolf/sound status; tail -5 /run/rudolf/pulse.log")
sh("su -s /bin/sh rudolf -c 'XDG_RUNTIME_DIR=/run/user/1000 sh -c \"head -c 352800 /dev/urandom | pacat --raw --format=s16le --rate=44100 --channels=2\"' && echo PLAYED",40)
print('chromium stream:',g.js("(async()=>{const a=new AudioContext();const o=a.createOscillator();o.connect(a.destination);o.start();await new Promise(r=>setTimeout(r,2500));const s=a.state;o.stop();return s})()"))
sh("su -s /bin/sh rudolf -c 'XDG_RUNTIME_DIR=/run/user/1000 pactl list short clients'")
print('== cyberdeck test mode')
print(g.js("rdPost('decktest?on=1').then(r=>r.message).catch(e=>'ERR '+e.message)"))
print(g.js("rd('decktest').then(r=>JSON.stringify(r.deck))"))
print(g.js("rdPost('decktest?on=0').then(r=>r.message)"))
print('== fallback when Xorg stops')
sh("pkill -x Xorg; sleep 25; cat /run/rudolf/display-mode /run/rudolf/display-reason; pgrep -a Xvfb | cut -c1-60; pgrep -a rudolf-display",60)
print('desktop back:',w("typeof rd=='function'",180))
g.shot(N+'_fallback.png')
