#!/usr/bin/env python3
# 1.6.2 (docs/plan-1.6.2.md Part 34): a stand-in eSCL (AirScan) scanner for the scanners' tests: qt/host/scan162_t.sh
# runs it in a network namespace of its own, qt/sb/sc162_t.py on the host behind QEMU's user network. It answers eSCL
# 2.x as printer-scanners do (ScannerCapabilities, ScannerStatus, ScanJobs, NextDocument, DELETE of a job) with a known
# picture made here (never a photo): the page is 8.5 x 11.69 inches; its top-left quarter red (220,30,30), top-right
# green (30,180,60), bottom-left blue (40,60,200); the bottom-right grey 255-60*N for page N (195, 135, 75: the feeder's
# pages are told apart), and a black square of a quarter inch at the page's top-left corner, one inch in. Grey pages
# are the same picture's luminance (Rec. 601); black and white ones grey < 128 is black. PNG pages, as the request's
# area and resolution say.
#   esclsrv.py ADDR PORT [READY-FILE] [--name MODEL] [--mdns NAME] [--wsd normal|wrongid|otherhost] [--no-escl]
# --mdns: a multicast DNS responder on ADDR's interface (port 5353, the group joined) answering _uscan._tcp by
# multicast (PTR, SRV, TXT with rs=eSCL, A), as printer-scanners do. --wsd: a WS-Discovery responder (port 3702, the
# group 239.255.255.250 joined) answering a Probe for scanners with a ProbeMatches by unicast to the Probe's source
# (normal; wrongid: relating to another message, as a forged answer would; otherhost: its device's address another
# computer's), and the device's metadata (WS-Transfer Get at /wsd/device: a Hosted scanner service at /wsd/scan).
# --no-escl: /eSCL answers 404 (a WSD-only scanner). --name: its MakeAndModel (default: the --mdns name, else Rudolf
# Test Scanner). --advertise ADDR:PORT: the address its WSD metadata gives (behind QEMU's forwarding, the guest's view).
# The test's own side door, on the same port: GET /_test/set?KEY=VALUE&... with adf=loaded|empty|jam|open,
# pages=N (the feeder's pages), busy=0|1 (every new job refused with 503 while the state says Processing),
# fail=0|1 (the next page's connection cut), delay=SECONDS (before each page); GET /_test/log, the requests seen
# (one a line: METHOD PATH and the scan settings' InputSource ColorMode XResolution Width Height).
# GPL-2.0-or-later.
import sys, zlib, struct, threading, re, time
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs

W_IN, H_IN = 8.5, 11.69          # the page, inches
U = 300                          # eSCL's units: 1/300 inch
MAXW, MAXH = int(W_IN * U), int(H_IN * U)
ST = {'adf': 'loaded', 'pages': 3, 'busy': 0, 'fail': 0, 'delay': 0.0}
JOBS = {}; LOG = []; LK = threading.Lock(); NEXT = [1]

def pixel(fx, fy, page):
    """the page's colour at (fx, fy) inches"""
    if 1.0 <= fx < 1.25 and 1.0 <= fy < 1.25: return (0, 0, 0)
    l, t = fx < W_IN / 2, fy < H_IN / 2
    if t and l: return (220, 30, 30)
    if t: return (30, 180, 60)
    if l: return (40, 60, 200)
    g = max(0, 255 - 60 * page); return (g, g, g)

def png(w, h, mode, rows):
    def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    ct, bd = {'RGB24': (2, 8), 'Grayscale8': (0, 8), 'BlackAndWhite1': (0, 1)}[mode]
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, bd, ct, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(rows, 6)) + chunk(b'IEND', b'')

def render(job, page):
    s = job; res = s['res']; x0, y0, wu, hu = s['x'], s['y'], s['w'], s['h']
    w = max(1, wu * res // U); h = max(1, hu * res // U); mode = s['mode']
    # each column's and row's colour cut points (the picture is made of rectangles: computed per run, not per pixel)
    xs = [(x0 + (i + 0.5) * U / res) / U for i in range(w)]
    out = bytearray()
    for j in range(h):
        fy = (y0 + (j + 0.5) * U / res) / U
        row = [pixel(fx, fy, page) for fx in xs]
        if mode == 'RGB24': out += b'\0' + bytes(c for p in row for c in p)
        else:
            gr = [(p[0] * 299 + p[1] * 587 + p[2] * 114 + 500) // 1000 for p in row]
            if mode == 'Grayscale8': out += b'\0' + bytes(gr)
            else:
                b = bytearray((w + 7) // 8)
                for i, g in enumerate(gr):
                    if g >= 128: b[i >> 3] |= 0x80 >> (i & 7)   # (PNG grey 1-bit: 1 is white)
                out += b'\0' + b
    return png(w, h, mode, bytes(out))

def caps(host):
    def src(tag):
        return f'''<scan:{tag}><scan:MinWidth>16</scan:MinWidth><scan:MaxWidth>{MAXW}</scan:MaxWidth><scan:MinHeight>16</scan:MinHeight><scan:MaxHeight>{MAXH}</scan:MaxHeight>
<scan:MaxScanRegions>1</scan:MaxScanRegions><scan:SettingProfiles><scan:SettingProfile><scan:ColorModes><scan:ColorMode>RGB24</scan:ColorMode><scan:ColorMode>Grayscale8</scan:ColorMode><scan:ColorMode>BlackAndWhite1</scan:ColorMode></scan:ColorModes>
<scan:DocumentFormats><pwg:DocumentFormat>image/png</pwg:DocumentFormat><scan:DocumentFormatExt>image/png</scan:DocumentFormatExt></scan:DocumentFormats>
<scan:SupportedResolutions><scan:DiscreteResolutions>{''.join(f'<scan:DiscreteResolution><scan:XResolution>{r}</scan:XResolution><scan:YResolution>{r}</scan:YResolution></scan:DiscreteResolution>' for r in (75, 100, 150, 200, 300))}</scan:DiscreteResolutions></scan:SupportedResolutions>
<scan:ColorSpaces><scan:ColorSpace>RGB</scan:ColorSpace></scan:ColorSpaces></scan:SettingProfile></scan:SettingProfiles>
<scan:SupportedIntents><scan:Intent>Document</scan:Intent><scan:Intent>Photo</scan:Intent><scan:Intent>Preview</scan:Intent></scan:SupportedIntents>
<scan:MaxOpticalXResolution>300</scan:MaxOpticalXResolution><scan:MaxOpticalYResolution>300</scan:MaxOpticalYResolution></scan:{tag}>'''
    return f'''<?xml version="1.0" encoding="UTF-8"?>
<scan:ScannerCapabilities xmlns:scan="http://schemas.hp.com/imaging/escl/2011/05/03" xmlns:pwg="http://www.pwg.org/schemas/2010/12/sm">
<pwg:Version>2.63</pwg:Version><pwg:MakeAndModel>{MODEL[0]}</pwg:MakeAndModel><pwg:SerialNumber>RTS-0001</pwg:SerialNumber>
<scan:UUID>4509a320-00a0-008f-00b6-002507510eca</scan:UUID><scan:AdminURI>http://{host}/</scan:AdminURI>
<scan:Platen>{src('PlatenInputCaps')}</scan:Platen>
<scan:Adf>{src('AdfSimplexInputCaps')}<scan:FeederCapacity>20</scan:FeederCapacity></scan:Adf>
</scan:ScannerCapabilities>'''.encode()

def status():
    adf = {'loaded': 'ScannerAdfLoaded', 'empty': 'ScannerAdfEmpty', 'jam': 'ScannerAdfJam', 'open': 'ScannerAdfDoorOpen'}[ST['adf']]
    st = 'Processing' if ST['busy'] else 'Idle'
    return f'''<?xml version="1.0" encoding="UTF-8"?>
<scan:ScannerStatus xmlns:scan="http://schemas.hp.com/imaging/escl/2011/05/03" xmlns:pwg="http://www.pwg.org/schemas/2010/12/sm">
<pwg:Version>2.63</pwg:Version><pwg:State>{st}</pwg:State><scan:AdfState>{adf}</scan:AdfState></scan:ScannerStatus>'''.encode()

def tag(x, t, d):
    m = re.search(r'<[^>]*\b' + t + r'>\s*([^<]*?)\s*<', x); return m.group(1) if m else d

class H(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'
    def log_message(self, *a): pass
    def send(self, code, body=b'', ctype='text/xml', extra=()):
        self.send_response(code); self.send_header('Content-Type', ctype); self.send_header('Content-Length', str(len(body)))
        for k, v in extra: self.send_header(k, v)
        self.end_headers(); self.wfile.write(body)
    def do_GET(self):
        u = urlparse(self.path); p = u.path
        with LK: LOG.append(f'GET {p}')
        if p == '/_test/set':
            for k, v in parse_qs(u.query).items():
                ST[k] = v[0] if k == 'adf' else float(v[0]) if k == 'delay' else int(v[0])
            return self.send(200, b'ok', 'text/plain')
        if p == '/_test/log':
            with LK: return self.send(200, ('\n'.join(LOG) + '\n').encode(), 'text/plain')
        if NOESCL[0] and p.startswith('/eSCL/'): return self.send(404)
        if p == '/eSCL/ScannerCapabilities': return self.send(200, caps(self.headers.get('Host', 'localhost')))
        if p == '/eSCL/ScannerStatus': return self.send(200, status())
        m = re.match(r'^/eSCL/ScanJobs/(\d+)/NextDocument$', p)
        if m:
            j = JOBS.get(int(m.group(1)))
            if not j: return self.send(404)
            if j['src'] == 'Feeder':
                if j['done'] >= ST['pages']: return self.send(404)
            elif j['done'] >= 1: return self.send(404)
            if ST['delay']: time.sleep(ST['delay'])
            if ST['fail']:
                ST['fail'] = 0; self.close_connection = True; self.wfile.flush(); self.connection.shutdown(2); return
            j['done'] += 1
            return self.send(200, render(j, j['done']), 'image/png')
        self.send(404)
    def do_POST(self):
        u = urlparse(self.path); n = int(self.headers.get('Content-Length', '0') or 0); body = self.rfile.read(n).decode('utf-8', 'replace')
        if u.path == '/wsd/device' and 'transfer/Get<' in body:
            with LK: LOG.append('POST /wsd/device Get')
            a, pt = ADV[0] or self.server.server_address[:2]
            return self.send(200, wsd_metadata(a, pt), 'application/soap+xml')
        if NOESCL[0]: return self.send(404)
        if u.path != '/eSCL/ScanJobs': return self.send(404)
        src = tag(body, 'InputSource', 'Platen'); mode = tag(body, 'ColorMode', 'RGB24'); res = int(tag(body, 'XResolution', '300'))
        x, y = int(tag(body, 'XOffset', '0')), int(tag(body, 'YOffset', '0'))
        w, h = int(tag(body, 'Width', str(MAXW))), int(tag(body, 'Height', str(MAXH)))
        with LK: LOG.append(f'POST {u.path} {src} {mode} {res} {w} {h} {x} {y}')
        if ST['busy']: return self.send(503)
        if src == 'Feeder' and ST['adf'] != 'loaded': return self.send(409)
        if mode not in ('RGB24', 'Grayscale8', 'BlackAndWhite1') or res not in (75, 100, 150, 200, 300): return self.send(409)
        x = max(0, min(x, MAXW - 16)); y = max(0, min(y, MAXH - 16)); w = max(16, min(w, MAXW - x)); h = max(16, min(h, MAXH - y))
        with LK: i = NEXT[0]; NEXT[0] += 1
        JOBS[i] = {'src': src, 'mode': mode, 'res': res, 'x': x, 'y': y, 'w': w, 'h': h, 'done': 0}
        host = self.headers.get('Host', 'localhost')
        self.send(201, b'', 'text/plain', [('Location', f'http://{host}/eSCL/ScanJobs/{i}')])
    def do_DELETE(self):
        m = re.match(r'^/eSCL/ScanJobs/(\d+)$', urlparse(self.path).path)
        with LK: LOG.append(f'DELETE {self.path}')
        if m: JOBS.pop(int(m.group(1)), None); return self.send(200)
        self.send(404)

# ---------------------------------------------------------------- discovery
import socket
def dname(s): return b''.join(bytes([len(p.encode())]) + p.encode() for p in s.split('.')) + b'\0'
def rr(nm, t, data, cls=0x8001): return dname(nm) + struct.pack('>HHIH', t, cls, 120, len(data)) + data
def mdns_responder(addr, port, label):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); s.bind(('', 5353))
    s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, socket.inet_aton('224.0.0.251') + socket.inet_aton(addr))
    s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(addr)); s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 255)
    s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 0)
    inst = label + '._uscan._tcp.local'; host = label.replace(' ', '-').lower() + '.local'
    txt = b''.join(bytes([len(x)]) + x for x in [b'txtvers=1', b'rs=eSCL', ('ty=' + MODEL[0]).encode(), b'UUID=4509a320-00a0-008f-00b6-002507510eca', b'pdl=image/png', b'note=Test lab'])
    ans = struct.pack('>HHHHHH', 0, 0x8400, 0, 1, 0, 3) + rr('_uscan._tcp.local', 12, dname(inst), 1) + rr(inst, 33, struct.pack('>HHH', 0, 0, port) + dname(host)) + rr(inst, 16, txt) + rr(host, 1, socket.inet_aton(addr))
    while True:
        q, src = s.recvfrom(9000)
        if len(q) < 12 or q[2] & 0x80 or b'\x06_uscan\x04_tcp' not in q and label.encode() not in q and host.split('.')[0].encode() not in q: continue
        with LK: LOG.append(f'MDNS from {src[0]}:{src[1]}')
        s.sendto(ans, ('224.0.0.251', 5353))
def wsd_responder(addr, port, kind):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); s.bind(('', 3702))
    s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, socket.inet_aton('239.255.255.250') + socket.inet_aton(addr))
    host = addr if kind != 'otherhost' else '192.0.2.99'
    while True:
        q, src = s.recvfrom(16384); t = q.decode('utf-8', 'replace')
        if 'discovery/Probe<' not in t or 'ScanDeviceType' not in t: continue
        m = re.search(r'MessageID>\s*([^<\s]+)\s*<', t)
        if not m: continue
        rel = m.group(1) if kind != 'wrongid' else 'urn:uuid:00000000-0000-4000-8000-000000000000'
        with LK: LOG.append(f'WSD probe from {src[0]}:{src[1]}')
        r = f'''<?xml version="1.0" encoding="utf-8"?><soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" xmlns:wsa="http://schemas.xmlsoap.org/ws/2004/08/addressing" xmlns:wsd="http://schemas.xmlsoap.org/ws/2005/04/discovery" xmlns:wscn="http://schemas.microsoft.com/windows/2006/08/wdp/scan" xmlns:wsdp="http://schemas.xmlsoap.org/ws/2006/02/devprof">
<soap:Header><wsa:To>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</wsa:To><wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/ProbeMatches</wsa:Action>
<wsa:MessageID>urn:uuid:1b2c3d4e-0000-4000-8000-{port:012d}</wsa:MessageID><wsa:RelatesTo>{rel}</wsa:RelatesTo><wsd:AppSequence InstanceId="1" MessageNumber="1"/></soap:Header>
<soap:Body><wsd:ProbeMatches><wsd:ProbeMatch><wsa:EndpointReference><wsa:Address>urn:uuid:6c1e0000-0000-4000-8000-{port:012d}</wsa:Address></wsa:EndpointReference>
<wsd:Types>wsdp:Device wscn:ScanDeviceType</wsd:Types><wsd:XAddrs>http://{host}:{port}/wsd/device</wsd:XAddrs><wsd:MetadataVersion>1</wsd:MetadataVersion></wsd:ProbeMatch></wsd:ProbeMatches></soap:Body></soap:Envelope>'''
        # (from its own address, as a separate device's would be: the namespace's sockets share an interface)
        s.sendmsg([r.encode()], [(socket.IPPROTO_IP, getattr(socket, 'IP_PKTINFO', 8), struct.pack('I4s4s', 0, socket.inet_aton(addr), b'\0' * 4))], 0, src)
def wsd_metadata(addr, port):
    return f'''<?xml version="1.0" encoding="utf-8"?><soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" xmlns:wsa="http://schemas.xmlsoap.org/ws/2004/08/addressing" xmlns:wsdp="http://schemas.xmlsoap.org/ws/2006/02/devprof" xmlns:wsx="http://schemas.xmlsoap.org/ws/2004/09/mex" xmlns:wscn="http://schemas.microsoft.com/windows/2006/08/wdp/scan">
<soap:Header><wsa:Action>http://schemas.xmlsoap.org/ws/2004/09/transfer/GetResponse</wsa:Action></soap:Header><soap:Body><wsx:Metadata>
<wsx:MetadataSection Dialect="http://schemas.xmlsoap.org/ws/2006/02/devprof/ThisModel"><wsdp:ThisModel><wsdp:Manufacturer>Rudolf</wsdp:Manufacturer><wsdp:ModelName>{MODEL[0]}</wsdp:ModelName></wsdp:ThisModel></wsx:MetadataSection>
<wsx:MetadataSection Dialect="http://schemas.xmlsoap.org/ws/2006/02/devprof/ThisDevice"><wsdp:ThisDevice><wsdp:FriendlyName>{MODEL[0]} (WSD)</wsdp:FriendlyName></wsdp:ThisDevice></wsx:MetadataSection>
<wsx:MetadataSection Dialect="http://schemas.xmlsoap.org/ws/2006/02/devprof/Relationship"><wsdp:Relationship Type="http://schemas.xmlsoap.org/ws/2006/02/devprof/host">
<wsdp:Host><wsa:EndpointReference><wsa:Address>urn:uuid:6c1e0000-0000-4000-8000-{port:012d}</wsa:Address></wsa:EndpointReference><wsdp:Types>wsdp:Device</wsdp:Types></wsdp:Host>
<wsdp:Hosted><wsa:EndpointReference><wsa:Address>http://{addr}:{port}/wsd/print</wsa:Address></wsa:EndpointReference><wsdp:Types>wprt:PrinterServiceType</wsdp:Types></wsdp:Hosted>
<wsdp:Hosted><wsa:EndpointReference><wsa:Address>http://{addr}:{port}/wsd/scan</wsa:Address></wsa:EndpointReference><wsdp:Types>wscn:ScannerServiceType</wsdp:Types><wsdp:ServiceId>uri:scan</wsdp:ServiceId></wsdp:Hosted>
</wsdp:Relationship></wsx:MetadataSection></wsx:Metadata></soap:Body></soap:Envelope>'''.encode()
MODEL = ['Rudolf Test Scanner']; NOESCL = [False]; ADV = [None]

if __name__ == '__main__':
    a = sys.argv[1:]; opt = {}; args = []; i = 0
    while i < len(a):
        if a[i] == '--no-escl': NOESCL[0] = True; i += 1
        elif a[i].startswith('--'): opt[a[i]] = a[i + 1]; i += 2
        else: args.append(a[i]); i += 1
    if '--name' in opt or '--mdns' in opt: MODEL[0] = opt.get('--name', opt.get('--mdns'))
    addr, port = args[0], int(args[1])
    if '--advertise' in opt: h, _, pt = opt['--advertise'].rpartition(':'); ADV[0] = (h, int(pt))
    srv = ThreadingHTTPServer((addr, port), H); srv.daemon_threads = True
    if '--mdns' in opt: threading.Thread(target=mdns_responder, args=(addr, port, opt['--mdns']), daemon=True).start()
    if '--wsd' in opt: threading.Thread(target=wsd_responder, args=(addr, port, opt['--wsd']), daemon=True).start()
    if len(args) > 2: time.sleep(0.3); open(args[2], 'w').write(str(srv.server_address[1]) + '\n')
    srv.serve_forever()
