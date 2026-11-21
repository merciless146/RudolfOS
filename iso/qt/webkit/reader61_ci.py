# 1.6.1 (docs/plan-1.6.1.md Part 10, docs/plan-oracle.md item 7): reader mode in Oracle's Lite tabs (rudolf-webkit), on a virtual screen
# (the Lite engine check, webkit-check.yml). A stand-in desktop page (port 8604) talks to rudolf-webkit as tabs26.js does; a test site
# (port 8605) has an article and a short page. Checks: after loading, the tab says whether its page has an article (tabev readable: yes
# for the article, no for the short page); readhtml gives the page's HTML as it is now (a paragraph the page's script added is in it),
# its address and title; F9 typed in the page goes to the desktop (keys f9) before the page's own handler.
#   python3 reader61_ci.py ./rudolf-webkit   (DISPLAY set; xdotool)
import http.server, json, subprocess, sys, threading, time
BIN = sys.argv[1]; RES = {}
DESK = r'''<!doctype html><meta charset=utf-8><title>desk</title><body style="background:#123">desk<script>
const post=(k,v)=>fetch('/result',{method:'POST',body:JSON.stringify({k,v})});
const send=m=>window.webkit.messageHandlers.rudolfShell.postMessage(m);
const ev=[];let hello=false;
window.__rwkIn=m=>{ev.push(m);if(m.rudolfShell=='hello'&&!hello){hello=true;send({rudolfShell:'ready'});run()}};
const wait=(f,t=20000)=>new Promise((res,rej)=>{const t0=Date.now();const iv=setInterval(()=>{const r=ev.find(f);if(r){clearInterval(iv);res(r)}else if(Date.now()-t0>t){clearInterval(iv);rej(new Error('timeout'))}},50)});
async function step(k,f){try{await post(k,await f())}catch(e){await post(k,'FAIL '+e.message)}}
async function run(){await post('hello',true);
 send({rudolfShell:'tab',op:'open',tid:'t1',url:'http://127.0.0.1:8605/article.html',part:'utest'});
 send({rudolfShell:'tab',op:'place',tid:'t1',r:[100,100,900,500],vis:null,z:1});
 await step('article',async()=>(await wait(m=>m.ev=='readable'&&m.tid=='t1')).on);
 await new Promise(r=>setTimeout(r,600));
 send({rudolfShell:'tab',op:'readhtml',tid:'t1',id:7});
 await step('html',async()=>{const m=await wait(m=>m.ev=='readhtml'&&m.id==7);return {added:/added by the page/.test(m.html),hooves:/hooves change/.test(m.html),url:m.url,title:m.title,len:m.html.length}});
 await post('ready_keys',true);
 await step('keys',async()=>(await wait(m=>m.ev=='keys'&&m.k=='f9')).k);
 await step('seen',async()=>ev.filter(m=>m.ev=='title').pop().title);
 send({rudolfShell:'tab',op:'go',tid:'t1',url:'http://127.0.0.1:8605/short.html'});
 await step('short',async()=>(await wait(m=>m.ev=='readable'&&m.tid=='t1'&&m!==ev.find(x=>x.ev=='readable'))).on);
 await post('done',true)}
</script>'''
PARA = 'Reindeer are well adapted to the cold: their fur traps air, their noses warm the air they breathe, and their hooves change with the seasons. '
ARTICLE = ('<!doctype html><meta charset=utf-8><title>An article</title><body><article><h1>An article</h1>' + ''.join(f'<p>{PARA * 2}</p>' for _ in range(8))
           + '</article><script>const p=document.createElement("p");p.textContent="added by the page";document.querySelector("article").appendChild(p);'
           + 'addEventListener("keydown",e=>{if(e.key=="F9")document.title="the page saw F9"})</script>')
SHORT = '<!doctype html><meta charset=utf-8><title>Short</title><p>Just a line.</p>'
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def send(self, code, body, ctype='text/html'):
        b = body.encode(); self.send_response(code); self.send_header('content-type', ctype); self.send_header('content-length', str(len(b))); self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        if self.server.server_port == 8604: return self.send(200, DESK)
        if self.path.startswith('/article.html'): return self.send(200, ARTICLE)
        if self.path.startswith('/short.html'): return self.send(200, SHORT)
        self.send(404, 'no')
    def do_POST(self):
        n = int(self.headers.get('content-length', 0)); d = json.loads(self.rfile.read(n) or b'{}'); RES[d.get('k')] = d.get('v'); self.send(200, 'ok', 'text/plain')
for port in (8604, 8605):
    s = http.server.ThreadingHTTPServer(('127.0.0.1', port), H); threading.Thread(target=s.serve_forever, daemon=True).start()
log = open('reader61.txt', 'w')
p = subprocess.Popen([BIN, '--rudolf-kiosk', '--size=1280x800', 'http://127.0.0.1:8604/'], stdout=log, stderr=subprocess.STDOUT)
end = time.time() + 90; typed = False
while time.time() < end and 'done' not in RES and p.poll() is None:
    if not typed and 'ready_keys' in RES:
        typed = True; time.sleep(0.6)
        subprocess.run(['xdotool', 'mousemove', '500', '400', 'click', '1']); time.sleep(0.4); subprocess.run(['xdotool', 'key', 'F9'])
    time.sleep(0.3)
time.sleep(0.5); H_ = RES.get('html') or {}
checks = [
    ('the article page has loaded: the tab says it has an article (readable)', RES.get('article') is True),
    ("readhtml: the page's HTML as it is now (what its script added is in it)", isinstance(H_, dict) and H_.get('added') and H_.get('hooves') and H_.get('len', 0) > 1000),
    ('...its address and title', isinstance(H_, dict) and H_.get('url') == 'http://127.0.0.1:8605/article.html' and H_.get('title') == 'An article'),
    ("F9 in the page goes to Oracle (keys f9), before the page's own handler", RES.get('keys') == 'f9' and RES.get('seen') == 'An article'),
    ('a short page: no article', RES.get('short') is False),
    ('rudolf-webkit still runs', p.poll() is None),
]
p.terminate(); log.close()
ok = 0
for n, c in checks:
    ok += bool(c); print(('PASS ' if c else 'FAIL ') + n)
print('results', json.dumps(RES)[:600])
print(f'{ok}/{len(checks)} passed'); sys.exit(0 if ok == len(checks) else 1)
