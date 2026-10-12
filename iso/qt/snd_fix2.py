import sys,os;sys.path.insert(0,'.')
from guest import Guest
g=Guest(sys.argv[1])
U="su -s /bin/sh rudolf -c 'XDG_RUNTIME_DIR=/run/user/1000 PULSE_SERVER=unix:/run/user/1000/pulse/native "
print(g.sh("kill 759; sleep 1; pgrep -a pulseaudio",20))
print(g.sh(U+"pactl set-sink-mute @DEFAULT_SINK@ 0; pactl set-sink-volume @DEFAULT_SINK@ 100%; pactl list sinks | grep -E \"Mute|Volume:\"'",30))
open('mark','w').write(str(os.path.getsize('p2.wav')))
print(g.sh(U+"sh -c \"head -c 352800 /dev/urandom | pacat --raw --format=s16le --rate=44100 --channels=2\"' && echo PLAYED; grep -A6 'Node 0x02' /proc/asound/card0/codec#0 | grep vals",40))
