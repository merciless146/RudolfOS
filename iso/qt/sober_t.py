# Sober-support checks on a debug-booted live guest: python3 sober_t.py NAME
import sys,time,json;sys.path.insert(0,'.')
from guest import Guest
N=sys.argv[1];g=Guest(N)
def sh(c,t=60):
    o=g.sh(c,t);print('$',c,'\n'+o.strip()[-1500:]);return o
def w(c,t=120):
    e=time.time()+t
    while time.time()<e:
        if g.js(c) is True: return True
        time.sleep(2)
    return False
print('== graphics driver')
sh("grep -E '^(bochs|i915|amdgpu|nouveau|radeon) ' /proc/modules; echo fb0=$(cat /sys/class/graphics/fb0/name); echo gpu=$(cat /run/rudolf/gpu 2>/dev/null); ls /dev/dri; rudolf-display --size")
print('== flatpak, sandbox, d-bus')
sh("flatpak --version; bwrap --version; cat /run/rudolf/session.env; pgrep -a dbus-daemon")
sh("su -s /bin/sh rudolf -c 'set -a; . /run/rudolf/session.env; dbus-send --session --print-reply --dest=org.freedesktop.DBus / org.freedesktop.DBus.ListNames | head -5'")
sh("su -s /bin/sh rudolf -c 'bwrap --ro-bind / / --dev /dev --proc /proc --unshare-all --die-with-parent /bin/true && echo BWRAP-SANDBOX-OK'")
sh("flatpak remote-list --system && echo REMOTE-LIST-OK; ls /var/lib/flatpak; cat /etc/machine-id")
print('== rudolfd native API')
assert w("typeof rd=='function'",240),'desktop not up'
print(g.js("rd('native/status?id=org.vinegarhq.Sober').then(r=>JSON.stringify(r)).catch(e=>'ERR '+e.message)"))
print(g.js("rdPost('native/install?id=org.vinegarhq.Sober').then(r=>JSON.stringify(r)).catch(e=>'ERR '+e.message)"))
print(g.js("rdPost('native/run?id=com.example.Other').then(r=>JSON.stringify(r)).catch(e=>'ERR '+e.message)"))
print('== window handling with a stand-in game window')
print(g.put('xwin','/tmp/xwin'))
sh("su -s /bin/sh rudolf -c 'DISPLAY=:0 /tmp/xwin > /tmp/xwin.log 2>&1 &'; sleep 4; tail -1 /tmp/xwin.log; cat /run/rudolf/wm")
g.shot(N+'_game.png')
g.key('meta_l','esc');time.sleep(3)
sh("tail -1 /tmp/xwin.log; cat /run/rudolf/wm")
g.shot(N+'_desktop.png')
sh("echo game > /run/rudolf/wm; sleep 3; tail -1 /tmp/xwin.log")
sh("pkill xwin; sleep 3; cat /run/rudolf/wm")
print('== App Store card')
g.js("document.getElementById('login').style.display='none';launch('store')");time.sleep(5)
print(g.js("[...document.querySelectorAll('.natc .natst,.natc button')].map(e=>e.textContent).join(' | ')"))
g.shot(N+'_store.png')
