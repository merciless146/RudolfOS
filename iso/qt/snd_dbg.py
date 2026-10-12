import sys;sys.path.insert(0,'.')
from guest import Guest
g=Guest(sys.argv[1])
for c in ["su -s /bin/sh rudolf -c 'XDG_RUNTIME_DIR=/run/user/1000 pactl list sinks' | grep -E 'Name|State|Mute|Volume|Active Port|api.alsa.path|device.string|alsa.mixer_name' | head -12",
          "cat /proc/asound/cards; grep -E 'Codec|Amp-Out|Pin-ctls|Node 0x|Converter' /proc/asound/card0/codec#0 | head -30",
          "which amixer alsactl; ls /usr/share/alsa/ | head"]:
    print('$',c[:80]); print(g.sh(c,30)[-2500:])
