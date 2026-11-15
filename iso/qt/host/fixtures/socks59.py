#!/usr/bin/env python3
# 1.5.9 (docs/plan-1.5.9.md 16.3): a stand-in for Tor's SOCKS5 proxy (127.0.0.1:PORT) for the Tor window's tests. Every
# request is written to LOG, one line each: "CONNECT HOST PORT KIND" (KIND: name when the client gave the proxy a name to
# look up, as Tor wants; ipv4 or ipv6 when it had looked the name up itself). Names under .test are answered by the test's
# site (127.0.0.1:SITEPORT); anything else is refused, as nothing leaves the test's network.
#   python3 socks59.py PORT SITEPORT LOG
import socket, struct, sys, threading
PORT, SITE, LOG = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
L = threading.Lock()
def note(s):
    with L:
        with open(LOG, 'a') as f: f.write(s + '\n')
def pipe(a, b):
    try:
        while True:
            d = a.recv(65536)
            if not d: break
            b.sendall(d)
    except OSError: pass
    for x in (a, b):
        try: x.shutdown(socket.SHUT_RDWR)
        except OSError: pass
def rx(c, n):
    b = b''
    while len(b) < n:
        d = c.recv(n - len(b))
        if not d: raise OSError('closed')
        b += d
    return b
def serve(c):
    try:
        v, n = rx(c, 2); rx(c, n)
        if v != 5: return c.close()
        c.sendall(b'\x05\x00')
        v, cmd, _, at = rx(c, 4)
        if at == 1: host, kind = socket.inet_ntop(socket.AF_INET, rx(c, 4)), 'ipv4'
        elif at == 4: host, kind = socket.inet_ntop(socket.AF_INET6, rx(c, 16)), 'ipv6'
        elif at == 3: host, kind = rx(c, rx(c, 1)[0]).decode('ascii', 'replace'), 'name'
        else: return c.close()
        port = struct.unpack('!H', rx(c, 2))[0]
        note('%s %s %d %s' % ('CONNECT' if cmd == 1 else 'CMD%d' % cmd, host, port, kind))
        if cmd != 1 or not (host.endswith('.test') or host == '127.0.0.1'):
            c.sendall(b'\x05\x02\x00\x01' + b'\0' * 6); return c.close()
        u = socket.create_connection(('127.0.0.1', SITE), timeout=10); u.settimeout(None)
        c.sendall(b'\x05\x00\x00\x01' + socket.inet_aton('127.0.0.1') + struct.pack('!H', SITE))
        threading.Thread(target=pipe, args=(u, c), daemon=True).start(); pipe(c, u)
    except OSError:
        try: c.close()
        except OSError: pass
s = socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); s.bind(('127.0.0.1', PORT)); s.listen(64)
while True:
    c, _ = s.accept(); threading.Thread(target=serve, args=(c,), daemon=True).start()
