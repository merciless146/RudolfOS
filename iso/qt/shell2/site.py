# Test websites for Oracle's real tabs (host.sh maps *.test to this machine): python3 site.py PORT
#   /login     a sign-in form; signing in sets a first-party cookie      /me   who is signed in
#   /popup     opens a sign-in style pop-up (/child) that answers its opener and closes itself
#   /files     links to /file (a download for signed-in visitors only) and /big.zip (2 MB, over the limit)
#   /gen/NAME?mb=N&kind=zero|eicar-head|eicar-tail|mz   a generated download of N MB (1.2.1, files kept on the disk)
#   /media/NAME   a file from the folder MEDIA (environment), e.g. a video or a PDF
#   /flash     a page with a Flash movie (blue.swf: a blue 300x200 frame; Ruffle should play it)      /     a plain green page
import http.server, sys, urllib.parse
GREEN = "<body style='margin:0;background:rgb(0,200,0);height:100vh'>"
class H(http.server.BaseHTTPRequestHandler):
    def log_message(s, *a): pass
    def send(s, body, extra=()):
        b = body.encode(); s.send_response(200); s.send_header('Content-Type', 'text/html; charset=utf-8')
        for k, v in extra: s.send_header(k, v)
        s.send_header('Content-Length', str(len(b))); s.end_headers(); s.wfile.write(b)
    def who(s):
        for c in (s.headers.get('Cookie') or '').split(';'):
            k, _, v = c.strip().partition('=')
            if k == 'sid': return v
        return ''
    def do_GET(s):
        p = urllib.parse.urlparse(s.path).path
        if p == '/login': return s.send("<title>Sign in</title>" + GREEN + "<form method=post action=/login><input name=u id=u><input type=password name=p id=p><button id=go>Sign in</button></form>")
        if p == '/me': return s.send("<title>Account</title>" + GREEN + "<h1 id=me>" + ("signed in as " + s.who() if s.who() else "not signed in") + "</h1>")
        if p == '/popup': return s.send("<title>Opener</title>" + GREEN + "<button id=b onclick=\"window.open('/child','signin','width=420,height=320')\">Sign in with Test</button><p id=got>waiting</p><script>addEventListener('message',e=>{document.getElementById('got').textContent='got '+e.data})</script>")
        if p == '/child': return s.send("<title>Test sign-in</title>" + GREEN + "<p>child</p><script>setTimeout(()=>{opener&&opener.postMessage('token-123','*');setTimeout(()=>window.close(),700)},1200)</script>")
        if p == '/flash': return s.send("<title>Flash</title>" + GREEN + "<embed id=f src='/movie.swf' width=300 height=200 type='application/x-shockwave-flash'><p><button id=fs style='width:160px;height:40px' onclick='document.documentElement.requestFullscreen()'>Full screen</button></p>")
        if p == '/movie.swf':
            b = open(__import__('os').path.join(__import__('os').path.dirname(__file__), 'blue.swf'), 'rb').read()
            s.send_response(200); s.send_header('Content-Type', 'application/x-shockwave-flash'); s.send_header('Content-Length', str(len(b))); s.end_headers(); s.wfile.write(b); return
        if p == '/files': return s.send("<title>Files</title>" + GREEN + "<a id=f href='/file'>notes</a> <a id=b href='/big.zip'>big</a>")
        if p == '/file':   # only for a signed-in visitor (the download must carry the site's cookie)
            if not s.who(): s.send_response(403); s.send_header('Content-Length', '0'); s.end_headers(); return
            b = ('notes for ' + s.who()).encode()
            s.send_response(200); s.send_header('Content-Type', 'text/plain'); s.send_header('Content-Disposition', 'attachment; filename="notes.txt"')
            s.send_header('Content-Length', str(len(b))); s.end_headers(); s.wfile.write(b); return
        if p == '/big.zip':
            b = b'\0' * (2 * 1048576)
            s.send_response(200); s.send_header('Content-Type', 'application/zip'); s.send_header('Content-Length', str(len(b))); s.end_headers(); s.wfile.write(b); return
        if p.startswith('/gen/'):
            q = dict(urllib.parse.parse_qsl(urllib.parse.urlparse(s.path).query)); n = int(float(q.get('mb', '1')) * 1048576); k = q.get('kind', 'zero')
            E = b'X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*'
            head = E if k == 'eicar-head' else b'MZ' if k == 'mz' else b''; tail = E if k == 'eicar-tail' else b''
            s.send_response(200); s.send_header('Content-Type', 'application/octet-stream'); s.send_header('Content-Disposition', 'attachment; filename="' + p[5:] + '"')
            s.send_header('Content-Length', str(n)); s.end_headers()
            try:
                s.wfile.write(head); left = n - len(head) - len(tail); z = b'\0' * 1048576
                while left > 0: c = min(left, len(z)); s.wfile.write(z[:c]); left -= c
                s.wfile.write(tail)
            except (BrokenPipeError, ConnectionResetError): pass
            return
        if p.startswith('/media/'):
            f = __import__('os').path.join(__import__('os').environ.get('MEDIA', '/tmp'), __import__('os').path.basename(p))
            b = open(f, 'rb').read()
            s.send_response(200); s.send_header('Content-Type', 'application/octet-stream'); s.send_header('Content-Disposition', 'attachment; filename="' + __import__('os').path.basename(p) + '"')
            s.send_header('Content-Length', str(len(b))); s.end_headers(); s.wfile.write(b); return
        return s.send("<title>Green</title>" + GREEN + "<p>green</p>")
    def do_POST(s):
        n = int(s.headers.get('Content-Length', 0) or 0); q = dict(urllib.parse.parse_qsl(s.rfile.read(n).decode()))
        s.send_response(303); s.send_header('Location', '/me')
        s.send_header('Set-Cookie', 'sid=' + q.get('u', 'x') + '; Path=/; Max-Age=86400; SameSite=Lax; HttpOnly'); s.send_header('Content-Length', '0'); s.end_headers()
http.server.ThreadingHTTPServer(('127.0.0.1', int(sys.argv[1])), H).serve_forever()
