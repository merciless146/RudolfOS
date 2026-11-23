# 1.6.2 (docs/plan-1.6.2.md Part 35): a NAS box's multicast DNS for the network shares' tests: _smb._tcp for NAME at ADDR
# (port 445), as Synology, QNAP, macOS and Samba's own Avahi files announce it; runs until killed (python-zeroconf).
#   python3 smbmdns.py NAME ADDR      GPL-2.0-or-later.
import sys, socket, time
from zeroconf import Zeroconf, ServiceInfo, IPVersion
name, addr = sys.argv[1], sys.argv[2]
zc = Zeroconf(interfaces=[addr], ip_version=IPVersion.V4Only)
info = ServiceInfo('_smb._tcp.local.', f'{name}._smb._tcp.local.', addresses=[socket.inet_aton(addr)], port=445, properties={}, server=f'{name.lower()}.local.')
zc.register_service(info); print('announced', name, addr, flush=True)
try:
    while True: time.sleep(3600)
finally:
    zc.unregister_service(info); zc.close()
