import sys;sys.path.insert(0,'.')
from guest import Guest
g=Guest(sys.argv[1])
U="su -s /bin/sh rudolf -c 'XDG_RUNTIME_DIR=/run/user/1000 PULSE_SERVER=unix:/run/user/1000/pulse/native "
print(g.sh("sed -n '/Node 0x02/,/Node 0x03/p' /proc/asound/card0/codec#0",20))
print(g.sh(U+"sh -c \"head -c 1764000 /dev/urandom | pacat --raw --format=s16le --rate=44100 --channels=2 &\"'; sleep 3; cat /proc/asound/card0/pcm0p/sub0/status | head -8; cat /proc/asound/card0/pcm0p/sub0/hw_params",30))
print(g.sh("dmesg | grep -i -E 'hda|snd|audio' | tail -8",20))
