#!/usr/bin/env python3
# 1.6.1 (docs/plan-1.6.1.md Part 9): stand-in network printers and packets for qt/host/mdns161_t.sh, which runs them in
# network namespaces of its own (never on the host's network).
#   mdnsnet.py respond MODE IFADDR NAME A-ADDR [READY-FILE]
#       a printer's multicast DNS responder on IFADDR's interface (port 5353, the group joined). MODE multicast: answers
#       by multicast to 224.0.0.251:5353, the PTR with only the SRV (TXT and A must be asked for, as some printers do);
#       unicast: answers everything at once by unicast to the question's source address and port (as legacy-unicast
#       responders do), IP TTL 255; fake: as multicast, sent from this computer itself (IP_MULTICAST_LOOP), never
#       waiting for a question.
#   mdnsnet.py send IFADDR SPORT DADDR DPORT TTL [response]   one UDP packet (an mDNS response for "Stray Printer")
#   mdnsnet.py listen IFADDR PORT SECONDS                       prints the number of packets that arrived on PORT
import socket, struct, sys, time

GRP = '224.0.0.251'

def name(s):
    return b''.join(bytes([len(p.encode())]) + p.encode() for p in s.split('.')) + b'\0'

def rr(nm, t, data):
    return name(nm) + struct.pack('>HHIH', t, 0x8001, 120, len(data)) + data

def answers(inst_label, a_addr, everything):
    inst = inst_label + '._ipp._tcp.local'; host = inst_label.replace(' ', '-').lower() + '.local'
    srv = rr(inst, 33, struct.pack('>HHH', 0, 0, 631) + name(host))
    txt = rr(inst, 16, b''.join(bytes([len(x)]) + x for x in [b'txtvers=1', b'rp=ipp/print', ('ty=' + inst_label + ' Model').encode(), b'note=Test lab', b'pdl=application/pdf']))
    a = rr(host, 1, socket.inet_aton(a_addr))
    return inst, host, srv, txt, a

def questions(q):
    qd = struct.unpack('>H', q[4:6])[0]; i = 12; out = []
    for _ in range(qd):
        labels = []
        while q[i]:
            labels.append(q[i + 1:i + 1 + q[i]].decode(errors='replace')); i += 1 + q[i]
        i += 1; t = struct.unpack('>H', q[i:i + 2])[0]; i += 4; out.append(('.'.join(labels), t))
    return out

def msock(ifaddr, loop=0):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(('', 5353))
    s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, socket.inet_aton(GRP) + socket.inet_aton(ifaddr))
    s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(ifaddr))
    s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 255)
    s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, loop)
    s.setsockopt(socket.IPPROTO_IP, socket.IP_TTL, 255)
    return s

def respond(mode, ifaddr, label, a_addr, ready=None):
    inst, host, srv, txt, a = answers(label, a_addr, mode == 'unicast')
    s = msock(ifaddr, 1 if mode == 'fake' else 0)
    if ready: open(ready, 'w').write('ready\n')
    if mode == 'fake':   # a program on this computer saying it is a printer, without being asked
        ptr = rr('_ipp._tcp.local', 12, name(inst))
        while True:
            s.sendto(struct.pack('>HHHHHH', 0, 0x8400, 0, 4, 0, 0) + ptr + srv + txt + a, (GRP, 5353)); time.sleep(0.2)
    while True:
        q, src = s.recvfrom(9000)
        if len(q) < 12 or q[2] & 0x80: continue   # (responses, ours or others')
        ans = []; add = []
        for nm, t in questions(q):
            if t == 12 and nm in ('_ipp._tcp.local', '_print._sub._ipp._tcp.local'):
                ans.append(rr(nm, 12, name(inst))); add += [srv, txt, a] if mode == 'unicast' else [srv]
            elif t == 33 and nm == inst: ans.append(srv)
            elif t == 16 and nm == inst: ans.append(txt)
            elif t == 1 and nm == host: ans.append(a)
        if not ans: continue
        m = struct.pack('>HHHHHH', struct.unpack('>H', q[:2])[0] if mode == 'unicast' else 0, 0x8400, 0, len(ans), 0, len(add)) + b''.join(ans) + b''.join(add)
        s.sendto(m, src if mode == 'unicast' else (GRP, 5353))

def send(ifaddr, sport, daddr, dport, ttl):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind((ifaddr, int(sport)))
    s.setsockopt(socket.IPPROTO_IP, socket.IP_TTL, int(ttl)); s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, int(ttl))
    s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(ifaddr))
    inst, host, srv, txt, a = answers('Stray Printer', ifaddr, True)
    m = struct.pack('>HHHHHH', 0, 0x8400, 0, 1, 0, 3) + rr('_ipp._tcp.local', 12, name(inst)) + srv + txt + a
    s.sendto(m, (daddr, int(dport)))

def listen(ifaddr, port, secs):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(('', int(port)))
    s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, socket.inet_aton(GRP) + socket.inet_aton(ifaddr))
    s.settimeout(0.2); n = 0; end = time.time() + float(secs)
    while time.time() < end:
        try: s.recvfrom(9000); n += 1
        except socket.timeout: pass
    print(n)

if __name__ == '__main__':
    c = sys.argv[1]
    if c == 'respond': respond(*sys.argv[2:])
    elif c == 'send': send(*sys.argv[2:7])
    elif c == 'listen': listen(*sys.argv[2:5])
