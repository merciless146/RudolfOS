import sys;sys.path.insert(0,'.')
from guest import Guest
g=Guest(sys.argv[1])
U="su -s /bin/sh rudolf -c 'XDG_RUNTIME_DIR=/run/user/1000 PULSE_SERVER=unix:/run/user/1000/pulse/native "
print(g.sh(U+"pactl list sinks' | grep -E 'Flags|Ports|analog-output|alsa.mixer|device.profile|Driver'",30))
print(g.sh("grep -i -E 'mixer|path|volume|element|Master|dB' /run/rudolf/pulse.log | head -20",30))
