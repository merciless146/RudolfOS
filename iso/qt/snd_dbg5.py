import sys;sys.path.insert(0,'.')
from guest import Guest
g=Guest(sys.argv[1])
U="su -s /bin/sh rudolf -c 'XDG_RUNTIME_DIR=/run/user/1000 PULSE_SERVER=unix:/run/user/1000/pulse/native "
amp="sed -n '/Node 0x02/,/Node 0x03/p' /proc/asound/card0/codec#0 | grep vals"
print(g.sh(U+"sh -c \"head -c 3528000 /dev/urandom | pacat --raw --format=s16le --rate=44100 --channels=2 &\"'; sleep 2; "+amp+"; "+U+"pactl set-sink-volume @DEFAULT_SINK@ 50%'; sleep 1; "+amp+"; "+U+"pactl set-sink-volume @DEFAULT_SINK@ 100%'; sleep 1; "+amp,40))
