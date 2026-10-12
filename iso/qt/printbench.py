# Host test bench for WordPro printing: serves iso/www like rudolfd (with a token) and implements the
# print/* endpoints with the real rudolf-print and a headless Chromium, so the desktop's print dialog can be
# driven end to end against fakeprinter.py.   python3 printbench.py WWW RPRINT CHROME PORT MDNS
import http.server, json, os, subprocess, sys, tempfile, urllib.parse
WWW, RPRINT, CHROME, PORT, MDNS = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]), sys.argv[5]
INJ = b'<script>window.RUDOLFD={token:"t",base:"",boot:"bios",media:"cd",live:1,recovery:0};</script>'
LOG = open('/tmp/printbench.log', 'a')
class H(http.server.SimpleHTTPRequestHandler):
    def __init__(s, *a, **k): super().__init__(*a, directory=WWW, **k)
    def log_message(s, f, *a): LOG.write((f % a) + '\n'); LOG.flush()
    def out(s, code, body, ct='application/json'):
        if isinstance(body, str): body = body.encode()
        s.send_response(code); s.send_header('Content-Type', ct); s.send_header('Content-Length', str(len(body))); s.end_headers(); s.wfile.write(body)
    def do_GET(s):
        u = urllib.parse.urlparse(s.path)
        if u.path in ('/', '/index.html'):
            h = open(os.path.join(WWW, 'index.html'), 'rb').read(); i = h.find(b'<head>') + 6
            return s.out(200, h[:i] + INJ + h[i:], 'text/html; charset=utf-8')
        if u.path.startswith('/api/'): return s.api(u, None)
        return super().do_GET()
    def do_POST(s):
        n = int(s.headers.get('Content-Length', 0)); b = s.rfile.read(n)
        return s.api(urllib.parse.urlparse(s.path), b)
    def api(s, u, body):
        q = dict(urllib.parse.parse_qsl(u.query)); p = u.path[5:]
        env = dict(os.environ, RP_MDNS=MDNS)
        if p == 'print/pdf':
            d = tempfile.mkdtemp(); open(f'{d}/doc.html', 'wb').write(body)
            subprocess.run([CHROME, '--headless=new', '--no-sandbox', '--disable-gpu', '--no-pdf-header-footer', '--run-all-compositor-stages-before-draw',
                            '--virtual-time-budget=4000', f'--user-data-dir={d}/p', f'--print-to-pdf={d}/out.pdf', f'file://{d}/doc.html'], capture_output=True, timeout=120)
            open('/tmp/printbench-last.pdf', 'wb').write(open(f'{d}/out.pdf', 'rb').read())
            return s.out(200, open(f'{d}/out.pdf', 'rb').read(), 'application/pdf')
        if p == 'print/find': return s.out(200, subprocess.run([RPRINT, 'find'], capture_output=True, env=env).stdout)
        if p == 'print/info': return s.out(200, subprocess.run([RPRINT, 'info', q['uri']], capture_output=True).stdout)
        if p == 'print/job':
            d = tempfile.mkdtemp(); open(f'{d}/doc', 'wb').write(body); open(f'{d}/title', 'w').write(q.get('title', ''))
            args = [RPRINT, 'send', q['uri'], f'{d}/doc', q['format']] + [f'{k}={q[k]}' for k in ('copies', 'sides', 'color', 'media', 'pages') if q.get(k)] + [f'title-file={d}/title']
            return s.out(200, subprocess.run(args, capture_output=True).stdout)
        if p.startswith('web/'):
            open('/tmp/printbench-web.log', 'a').write(p + ' ' + json.dumps(q) + '\n')
            if p == 'web/status': return s.out(200, json.dumps({'ok': True, 'running': True, 'shown': False}))
            return s.out(200, json.dumps({'ok': True, 'message': 'ok'}))
        return s.out(404, json.dumps({'ok': False, 'error': 'not in the test bench'}))
http.server.ThreadingHTTPServer(('127.0.0.1', PORT), H).serve_forever()
