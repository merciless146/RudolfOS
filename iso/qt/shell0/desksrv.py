# the 1.1.2 desktop at 127.0.0.1:8080 with rudolfd's page injection and a few API answers
import http.server,threading,json,sys
HTML=open(sys.argv[1],encoding='utf-8').read()
INJ='<script>window.RUDOLFD={token:"t",base:"",boot:"bios",media:"cd",live:1,recovery:0};</script>'
TEST='''<script>addEventListener('message',e=>{if(e.data&&e.data.fromShell){window.__shellSaid=e.data.fromShell;try{e.source.postMessage({fromDesktop:'hi'},'*')}catch(x){window.__replyErr=x.message}}})</script>'''
PAGE=HTML.replace('<head>','<head>'+INJ+TEST,1).encode()
class H(http.server.BaseHTTPRequestHandler):
    def log_message(s,*a):pass
    def j(s,o):
        b=json.dumps(dict(ok=True,**o)).encode();s.send_response(200);s.send_header('Content-Type','application/json');s.end_headers();s.wfile.write(b)
    def do_GET(s):
        if s.path in('/','/index.html'):
            s.send_response(200);s.send_header('Content-Type','text/html; charset=utf-8');s.end_headers();s.wfile.write(PAGE);return
        if s.path.startswith('/api/'): return s.j({'events':'','items':[],'disks':[],'usb':[],'drives':[],'inputs':[]})
        s.send_response(404);s.end_headers()
    def do_POST(s):
        n=int(s.headers.get('Content-Length',0) or 0);s.rfile.read(n);s.j({})
http.server.ThreadingHTTPServer(('127.0.0.1',8080),H).serve_forever()
