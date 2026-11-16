# 1.6.0 (plan Part 3): a point-to-point link between two network namespaces made of two TUN devices and this relay
# (raw IP, no Ethernet: as a cellular modem's interface is), for host tests on kernels without veth.
#   python3 tunpair.py NAME-HERE NETNS-FILE NAME-THERE [READY-FILE]
#   (NAME-HERE in this process's namespace, NAME-THERE in the one NETNS-FILE names, e.g. /proc/PID/ns/net)
import fcntl, os, select, struct, sys
TUNSETIFF, IFF_TUN, IFF_NO_PI = 0x400454ca, 0x0001, 0x1000
def tun(name):
    fd = os.open('/dev/net/tun', os.O_RDWR)
    fcntl.ioctl(fd, TUNSETIFF, struct.pack('16sH', name.encode(), IFF_TUN | IFF_NO_PI)); return fd
a = tun(sys.argv[1])
with open(sys.argv[2]) as f: os.setns(f.fileno(), os.CLONE_NEWNET)
b = tun(sys.argv[3])
if len(sys.argv) > 4: open(sys.argv[4], 'w').write('ready\n')
while True:
    r, _, _ = select.select([a, b], [], [])
    for s in r:
        try: p = os.read(s, 65536)
        except OSError: continue
        try: os.write(b if s == a else a, p)
        except OSError: pass
