import sys;sys.path.insert(0,'.')
from guest import Guest
g=Guest(sys.argv[1])
for c in ["pgrep -a pulseaudio; ls -la /run/user/1000/pulse/","tail -15 /run/rudolf/pulse.log","grep -A4 'Node 0x02' /proc/asound/card0/codec#0","ls /proc/asound/card0/; cat /proc/asound/card0/pcm0p/sub0/status /proc/asound/card0/pcm0p/sub0/hw_params"]:
    print('$',c); print(g.sh(c,30)[-1800:])
