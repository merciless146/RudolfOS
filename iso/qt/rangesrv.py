# tiny static server with HTTP Range support (python's http.server has none)
import http.server, os, re, sys
class H(http.server.SimpleHTTPRequestHandler):
    def send_head(self):
        p=self.translate_path(self.path)
        if not os.path.isfile(p): return super().send_head()
        r=self.headers.get('Range');size=os.path.getsize(p);f=open(p,'rb')
        m=re.match(r'bytes=(\d+)-(\d*)',r or '')
        if m:
            a=int(m.group(1));b=int(m.group(2)) if m.group(2) else size-1;b=min(b,size-1)
            self.send_response(206);self.send_header('Content-Range',f'bytes {a}-{b}/{size}');self.send_header('Content-Length',str(b-a+1));self.send_header('Content-Type','application/octet-stream');self.end_headers()
            f.seek(a);self.wfile.write(f.read(b-a+1));f.close();self.server.ranges+=1;return None
        self.send_response(200);self.send_header('Content-Length',str(size));self.send_header('Content-Type','application/octet-stream');self.end_headers();return f
    def log_message(self,*a):pass
s=http.server.ThreadingHTTPServer(('0.0.0.0',int(sys.argv[1])),lambda *a,**k:H(*a,directory=sys.argv[2],**k));s.ranges=0;s.serve_forever()
