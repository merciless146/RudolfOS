# A pretend IPP Everywhere printer (and an mDNS responder for it) for testing rudolf-print.
#   python3 fakeprinter.py OUTDIR PORT KIND [MDNSPORT]     KIND: pdf | raster
# Every job is saved to OUTDIR/job-N.<ext> with its attributes in OUTDIR/job-N.json.
import http.server, json, os, socket, struct, sys, threading

OUT, PORT, KIND = sys.argv[1], int(sys.argv[2]), sys.argv[3]
os.makedirs(OUT, exist_ok=True); JOBS = [0]

def parse(b):
    op = struct.unpack('>H', b[2:4])[0]; rid = struct.unpack('>I', b[4:8])[0]; i = 8; at = {}; last = None
    while i < len(b):
        t = b[i]; i += 1
        if t == 3: break
        if t < 0x10: continue
        nl = struct.unpack('>H', b[i:i+2])[0]; i += 2; n = b[i:i+nl].decode(); i += nl
        vl = struct.unpack('>H', b[i:i+2])[0]; i += 2; v = b[i:i+vl]; i += vl
        if n: last = n
        if t in (0x21, 0x23): v = struct.unpack('>i', v)[0]
        elif t == 0x33: v = list(struct.unpack('>ii', v))
        else: v = v.decode('utf-8', 'replace')
        at.setdefault(last, []).append(v)
    return op, rid, at, b[i:]

def enc(status, rid, groups):
    o = struct.pack('>HHI', 0x0200, status, rid)
    for g, attrs in groups:
        o += bytes([g])
        for name, tag, vals in attrs:
            for k, v in enumerate(vals):
                if tag in (0x21, 0x23): vb = struct.pack('>i', v)
                elif tag == 0x22: vb = bytes([v])
                elif tag == 0x32: vb = struct.pack('>iib', v, v, 3)
                elif tag == 0x33: vb = struct.pack('>ii', *v)
                else: vb = v.encode()
                nm = name.encode() if k == 0 else b''
                o += bytes([tag]) + struct.pack('>H', len(nm)) + nm + struct.pack('>H', len(vb)) + vb
    return o + b'\x03'

def printer_attrs():
    fm = ['application/octet-stream', 'image/pwg-raster', 'image/urf', 'image/jpeg'] + (['application/pdf'] if KIND == 'pdf' else [])
    return [('attributes-charset', 0x47, ['utf-8']), ('attributes-natural-language', 0x48, ['en'])], [
        ('printer-name', 0x42, ['Test-' + KIND]), ('printer-info', 0x41, ['Office Printer (' + KIND + ')']),
        ('printer-make-and-model', 0x41, ['Rudolf Test Printer']), ('printer-location', 0x41, ['Lab']),
        ('printer-state', 0x23, [3]), ('printer-state-reasons', 0x44, ['none']),
        ('document-format-supported', 0x49, fm), ('media-supported', 0x44, ['na_letter_8.5x11in', 'iso_a4_210x297mm', 'na_legal_8.5x14in']),
        ('media-default', 0x44, ['na_letter_8.5x11in']), ('sides-supported', 0x44, ['one-sided', 'two-sided-long-edge', 'two-sided-short-edge']),
        ('print-color-mode-supported', 0x44, ['monochrome', 'color', 'auto']), ('color-supported', 0x22, [1]),
        ('copies-supported', 0x33, [[1, 99]]), ('page-ranges-supported', 0x22, [1]),
        ('pwg-raster-document-resolution-supported', 0x32, [300, 600]), ('pwg-raster-document-type-supported', 0x44, ['sgray_8', 'srgb_8']),
        ('pwg-raster-document-sheet-back', 0x44, ['rotated']), ('urf-supported', 0x44, ['W8', 'SRGB24', 'RS300-600', 'DM1', 'CP1'])]

class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_POST(self):
        n = int(self.headers.get('Content-Length', 0)); b = self.rfile.read(n)
        op, rid, at, doc = parse(b); opg, prg = printer_attrs()
        if op == 0x0B:
            want = set(at.get('requested-attributes', []))
            body = enc(0, rid, [(1, opg), (4, [a for a in prg if not want or a[0] in want])])
        elif op == 0x02:
            fmt = at.get('document-format', ['?'])[0]
            if fmt == 'application/pdf' and KIND != 'pdf': body = enc(0x040A, rid, [(1, opg)])
            else:
                JOBS[0] += 1; j = JOBS[0]; ext = {'application/pdf': 'pdf', 'image/pwg-raster': 'pwg', 'image/urf': 'urf'}.get(fmt, 'bin')
                open(f'{OUT}/job-{j}.{ext}', 'wb').write(doc); json.dump({k: v for k, v in at.items()}, open(f'{OUT}/job-{j}.json', 'w'))
                body = enc(0, rid, [(1, opg), (2, [('job-id', 0x21, [j]), ('job-state', 0x23, [3]), ('job-uri', 0x45, [f'ipp://127.0.0.1:{PORT}/jobs/{j}'])])])
        else: body = enc(0x0501, rid, [(1, opg)])
        self.send_response(200); self.send_header('Content-Type', 'application/ipp'); self.send_header('Content-Length', str(len(body))); self.end_headers(); self.wfile.write(body)

def name(s):
    return b''.join(bytes([len(p.encode())]) + p.encode() for p in s.split('.')) + b'\0'
def rr(nm, t, data): return name(nm) + struct.pack('>HHIH', t, 0x8001, 120, len(data)) + data
def mdns(port):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.bind(('127.0.0.1', port))
    inst = 'Office Printer (' + KIND + ')._ipp._tcp.local'; host = 'testprinter.local'
    while True:
        q, a = s.recvfrom(9000); qd = struct.unpack('>H', q[4:6])[0]; i = 12; asked = []
        for _ in range(qd):
            labels = []
            while q[i]: labels.append(q[i+1:i+1+q[i]].decode()); i += 1 + q[i]
            i += 1; t = struct.unpack('>H', q[i:i+2])[0]; i += 4; asked.append(('.'.join(labels), t))
        ans = []; add = []
        for nm, t in asked:
            if t == 12 and nm in ('_ipp._tcp.local', '_print._sub._ipp._tcp.local'):
                ans.append(rr(nm, 12, name(inst)))
                add.append(rr(inst, 33, struct.pack('>HHH', 0, 0, PORT) + name(host)))   # SRV only: TXT and A must be asked for
            elif t == 16 and nm == inst: ans.append(rr(inst, 16, b''.join(bytes([len(x)]) + x for x in [b'txtvers=1', b'rp=ipp/print', b'ty=Rudolf Test Printer', b'note=Lab', b'pdl=image/pwg-raster'])))
            elif t == 33 and nm == inst: ans.append(rr(inst, 33, struct.pack('>HHH', 0, 0, PORT) + name(host)))
            elif t == 1 and nm == host: ans.append(rr(host, 1, socket.inet_aton('127.0.0.1')))
        if ans: s.sendto(struct.pack('>HHHHHH', 0, 0x8400, 0, len(ans), 0, len(add)) + b''.join(ans) + b''.join(add), a)

if len(sys.argv) > 4: threading.Thread(target=mdns, args=(int(sys.argv[4]),), daemon=True).start()
http.server.ThreadingHTTPServer(('127.0.0.1', PORT), H).serve_forever()
