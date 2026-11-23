# 1.6.2 (docs/plan-1.6.2.md Part 33): the VPN test's other end. Run in the test's "internet" namespace:
#   python3 vpnnet.py dns ADDR ANSWER LOG    a name server on ADDR:53 that answers every A question with ANSWER, and
#                                            writes each question's name to LOG (so the test sees which one was asked)
#   python3 vpnnet.py who ADDR PORT          an HTTP server on ADDR:PORT that answers with the address it was reached from
import socket, sys, struct, threading
from http.server import BaseHTTPRequestHandler, HTTPServer

def dns(addr, answer, log):
    fam = socket.AF_INET6 if ':' in addr else socket.AF_INET
    s = socket.socket(fam, socket.SOCK_DGRAM); s.bind((addr, 53))
    ans = socket.inet_aton(answer)
    while True:
        q, peer = s.recvfrom(1500)
        if len(q) < 12: continue
        i, name = 12, []
        while i < len(q) and q[i]:
            name.append(q[i + 1:i + 1 + q[i]].decode('ascii', 'replace')); i += 1 + q[i]
        qtype = struct.unpack('>H', q[i + 1:i + 3])[0] if i + 3 <= len(q) else 0
        with open(log, 'a') as f: f.write('.'.join(name) + ' ' + str(qtype) + ' ' + peer[0] + '\n')
        head = q[:2] + struct.pack('>HHHHH', 0x8180, 1, 1 if qtype == 1 else 0, 0, 0)
        body = q[12:i + 5]
        if qtype == 1: body += b'\xc0\x0c' + struct.pack('>HHIH', 1, 1, 5, 4) + ans
        s.sendto(head + body, peer)

class Who(BaseHTTPRequestHandler):
    def do_GET(self):
        b = self.client_address[0].encode(); self.send_response(200); self.send_header('Content-Length', str(len(b))); self.end_headers(); self.wfile.write(b)
    def log_message(self, *a): pass

if sys.argv[1] == 'dns': dns(sys.argv[2], sys.argv[3], sys.argv[4])
else:
    class S(HTTPServer):
        address_family = socket.AF_INET6 if ':' in sys.argv[2] else socket.AF_INET
    S((sys.argv[2], int(sys.argv[3])), Who).serve_forever()
