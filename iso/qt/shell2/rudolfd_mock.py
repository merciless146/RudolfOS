# a stand-in for rudolfd with the 1.2 endpoints: the page (with rudolfd's real localStorage copy script), /iwa/,
# idbm/* (database mirror), shell, shell/switch, clipboard, lsync; system, processes, stats, net, info, proc/end and
# service/restart are passed to a real rudolfd (a test copy on port 8181, see host.sh). State lives in STATE (a folder).
import http.server,json,sys,os,re,glob,urllib.parse,urllib.request as UR,urllib.error as UE
W,STATE,PROF=sys.argv[1],sys.argv[2],sys.argv[3]
os.makedirs(STATE+'/idb',exist_ok=True)
SRC=open('/home/claude/rudolfos/iso/sys/rudolfd.c').read()
LSJS=re.search(r'static const char LSJS\[\] =\n(.*?);\n',SRC,re.S).group(1)
LSJS=''.join(json.loads(x) for x in re.findall(r'"(?:[^"\\]|\\.)*"',LSJS))
HTML=open(os.path.join(W,'index.html'),encoding='utf-8').read()
def page():
    ls=open(STATE+'/ls.json').read() if os.path.exists(STATE+'/ls.json') else ''
    inj='<script type="application/json" id="rudolf-ls">'+ls.replace('<','\\u003c')+'</script><script>window.RUDOLFD={token:"t",base:"",boot:"bios",media:"cd",live:1,recovery:0};'+LSJS+'</script>'
    return HTML.replace('<head>','<head>'+inj,1).encode()
def gen(): return int(open(STATE+'/gen').read()) if os.path.exists(STATE+'/gen') else 0
def bump(): g=gen()+1;open(STATE+'/gen','w').write(str(g));return g
def flag(n): return os.path.exists(STATE+'/'+n)
class H(http.server.BaseHTTPRequestHandler):
    def log_message(s,f,*a): open(STATE+'/log','a').write((f%a)+'\n')
    def send(s,code,ct,b):
        s.send_response(code);s.send_header('Content-Type',ct);s.send_header('Content-Length',str(len(b)));s.end_headers()
        if s.command!='HEAD': s.wfile.write(b)
    def j(s,o,code=200): s.send(code,'application/json',json.dumps(o).encode())
    def route(s,post,body):
        u=urllib.parse.urlparse(s.path);p=u.path;q=dict(urllib.parse.parse_qsl(u.query))
        if p in('/','/index.html'): return s.send(200,'text/html; charset=utf-8',page())
        if not p.startswith('/api/'):
            f=os.path.join(W,p.lstrip('/'))
            if os.path.isfile(f): return s.send(200,'application/webbundle' if f.endswith('.swbn') else 'application/json' if f.endswith('.json') else 'text/javascript' if f.endswith('.js') else 'text/plain',open(f,'rb').read())
            return s.send(404,'text/plain',b'')
        a=p[5:]
        if a=='lsync' and post:
            d=json.loads(body);open(STATE+'/ls.json','w').write(json.dumps(d));return s.j({'ok':True})
        if a.startswith('idbm/'):
            w=a[5:]
            if flag('mirror-off'): return s.j({'ok':False,'error':'off'},500)
            files=sorted(os.path.basename(x)[:16] for x in glob.glob(STATE+'/idb/*.json'))
            if w=='meta': return s.j({'ok':True,'gen':gen(),'ready':flag('ready'),'count':len(files)})
            if w=='list': return s.j({'ok':True,'gen':gen(),'ready':flag('ready'),'items':files})
            if w=='get': return s.send(200,'application/json',open(STATE+'/idb/'+q['f']+'.json','rb').read())
            if w=='put': open(STATE+'/idb/'+q['f']+'.json','wb').write(body)
            elif w=='del':
                try: os.remove(STATE+'/idb/'+q['f']+'.json')
                except OSError: pass
            elif w=='clear': [os.remove(x) for x in glob.glob(STATE+'/idb/*.json')]
            elif w=='ready': open(STATE+'/ready','w').write('1');return s.j({'ok':True})
            return s.j({'ok':True,'gen':bump()})
        if a=='shell':
            sid=open(W+'/iwa/id').read().strip()
            inst=any(sid.encode() in open(x,'rb').read() for x in glob.glob(PROF+'/Default/iwa/*/main.swbn'))
            return s.j({'ok':True,'id':sid,'installed':inst,'ready':flag('ready'),'active':flag('active'),'failed':False,'on':True})
        if a=='shell/switch': open(STATE+'/switch','w').write('1');return s.j({'ok':True,'message':'ok'})
        # the system and process information comes from a real rudolfd (host.sh runs a test copy on port 8181)
        if a in ('system','processes','stats','net','info') or a in ('proc/end','service/restart'):
            q2=('?'+u.query) if u.query else ''
            rq=UR.Request('http://127.0.0.1:8181/api/'+a+q2,data=(body if post else None),headers={'X-Rudolf-Token':'testtoken'},method='POST' if post else 'GET')
            try: rs=UR.urlopen(rq,timeout=20);return s.send(rs.status,'application/json',rs.read())
            except UE.HTTPError as e: return s.send(e.code,'application/json',e.read())
            except Exception as e: return s.j({'ok':False,'error':str(e)},502)
        if a=='clipboard': return s.j({'ok':True,'text':'text from the X clipboard'})
        return s.j({'ok':True,'events':'','items':[],'disks':[],'usb':[],'drives':[],'inputs':[]})
    def do_GET(s): s.route(False,b'')
    def do_HEAD(s): s.route(False,b'')
    def do_POST(s):
        n=int(s.headers.get('Content-Length',0) or 0);s.route(True,s.rfile.read(n))
http.server.ThreadingHTTPServer(('127.0.0.1',8080),H).serve_forever()
