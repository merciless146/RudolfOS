# 1.6.1 (docs/plan-1.6.1.md Part 10, docs/plan-oracle.md item 1): find in page in Oracle's Lite tabs (rudolf-webkit), on a
# virtual screen (the Lite engine check, webkit-check.yml). A stand-in desktop page (port 8600) talks to rudolf-webkit as
# tabs26.js does; a test site (port 8601) has a page with 17 matches of "reindeer" (5 of them "Reindeer"), one inside a link
# and one split by <b>. Checks: "find" answers 17 and the first; "again" goes on and wraps round at the end, "back" goes back
# (and wraps at the start); Match case counts 5; no match answers 0; the selected text comes back for the bar's box (findsel);
# Ctrl+F, F3 and Shift+F3 typed in the page go to the desktop (keys cf, f3, sf3); findstop; rudolf-webkit still runs.
#   python3 find61_ci.py ./rudolf-webkit   (DISPLAY set; xdotool for the keys)
import http.server, json, shutil, subprocess, sys, threading, time
BIN = sys.argv[1]; RES = {}
DESK = r'''<!doctype html><meta charset=utf-8><title>desk</title><body style="background:#123">desk<script>
const post=(k,v)=>fetch('/result',{method:'POST',body:JSON.stringify({k,v})});
const send=m=>window.webkit.messageHandlers.rudolfShell.postMessage(m);
const ev=[];let hello=false;
window.__rwkIn=m=>{ev.push(m);if(m.rudolfShell=='hello'&&!hello){hello=true;send({rudolfShell:'ready'});run()}};
const wait=(f,t=15000)=>new Promise((res,rej)=>{const t0=Date.now();const iv=setInterval(()=>{const r=ev.find(f);if(r){clearInterval(iv);res(r)}else if(Date.now()-t0>t){clearInterval(iv);rej(new Error('timeout'))}},50)});
let ID=0;const find=async o=>{const id=++ID;send(Object.assign({rudolfShell:'tab',op:'find',tid:'t1',id},o));const r=await wait(m=>m.ev=='find'&&m.id==id);await new Promise(z=>setTimeout(z,150));
 const l=ev.filter(m=>m.ev=='find'&&m.id==id).pop();return [l.n,l.i]};
async function step(k,f){try{await post(k,await f())}catch(e){await post(k,'FAIL '+e.message)}}
async function run(){await post('hello',true);
 send({rudolfShell:'tab',op:'open',tid:'t1',url:'http://127.0.0.1:8601/find.html',part:'utest'});
 send({rudolfShell:'tab',op:'place',tid:'t1',r:[100,100,900,500],vis:null,z:1});
 await step('title',async()=>(await wait(m=>m.ev=='title'&&m.tid=='t1'&&m.title=='Find page')).title);
 await new Promise(r=>setTimeout(r,800));
 await step('sel',async()=>{const id=++ID;send({rudolfShell:'tab',op:'findsel',tid:'t1',id});return (await wait(m=>m.ev=='find'&&m.id==id)).sel});
 await step('first',()=>find({text:'reindeer'}));
 await step('next',()=>find({text:'reindeer',again:true}));
 for(let i=0;i<15;i++)await find({text:'reindeer',again:true});
 await step('wrap',()=>find({text:'reindeer',again:true}));
 await step('backwrap',()=>find({text:'reindeer',again:true,back:true}));
 await step('back',()=>find({text:'reindeer',again:true,back:true}));
 await step('case',()=>find({text:'Reindeer',mc:true}));
 await step('none',()=>find({text:'no such words here'}));
 await step('newback',()=>find({text:'reindeer',back:true}));
 send({rudolfShell:'tab',op:'findstop',tid:'t1'});
 await post('ready_keys',true);
 await step('keys',async()=>{const k=[];for(const want of ['cf','f3','sf3']){k.push((await wait(m=>m.ev=='keys'&&m.k==want,20000)).k)}return k});
 send({rudolfShell:'tab',op:'close',tid:'t1'});
 await post('done',true)}
</script>'''
WORDS = ['reindeer'] * 10 + ['Reindeer'] * 5
FIND = ('<!doctype html><meta charset=utf-8><title>Find page</title><body style="background:#fff;font:18px sans-serif">'
        + ''.join(f'<p>Line {i}: the {w} pulls the sleigh.</p>' for i, w in enumerate(WORDS[:15]))
        + '<p>A <a href="/x">reindeer link</a> and a split rein<b>deer</b> word.</p>'
        + '<input id=i value="typed"><p id=s>select me please</p>'
        + '<script>addEventListener("keydown",e=>{if(e.ctrlKey&&e.code=="KeyF")document.title="the page saw Ctrl+F"});'
        + 'setTimeout(()=>{const r=document.createRange();r.selectNodeContents(document.getElementById("s"));getSelection().removeAllRanges();getSelection().addRange(r)},500)</script>')
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def send(self, code, body, ctype='text/html'):
        b = body.encode() if isinstance(body, str) else body
        self.send_response(code); self.send_header('content-type', ctype); self.send_header('content-length', str(len(b))); self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        if self.server.server_port == 8600: return self.send(200, DESK)
        if self.path.startswith('/find.html'): return self.send(200, FIND)
        self.send(404, 'no')
    def do_POST(self):
        n = int(self.headers.get('content-length', 0)); d = json.loads(self.rfile.read(n) or b'{}'); RES[d.get('k')] = d.get('v'); self.send(200, 'ok', 'text/plain')
for port in (8600, 8601):
    s = http.server.ThreadingHTTPServer(('127.0.0.1', port), H); threading.Thread(target=s.serve_forever, daemon=True).start()
log = open('find61.txt', 'w')
p = subprocess.Popen([BIN, '--rudolf-kiosk', '--size=1280x800', 'http://127.0.0.1:8600/'], stdout=log, stderr=subprocess.STDOUT)
end = time.time() + 120; typed = False
while time.time() < end and 'done' not in RES and p.poll() is None:
    if not typed and 'ready_keys' in RES:
        typed = True; time.sleep(0.5)
        if shutil.which('xdotool'):
            subprocess.run(['xdotool', 'mousemove', '500', '400', 'click', '1']); time.sleep(0.4)
            for k in ('ctrl+f', 'F3', 'shift+F3'): subprocess.run(['xdotool', 'key', k]); time.sleep(0.4)
    time.sleep(0.3)
time.sleep(0.5)
checks = [
    ('a tab opens the page', RES.get('title') == 'Find page'),
    ('find: 17 matches (one in a link, one split by <b>), the first is the current one', RES.get('first') == [17, 1]),
    ('again: the next one', RES.get('next') == [17, 2]),
    ('again at the last: round to the first', RES.get('wrap') == [17, 1]),
    ('back at the first: round to the last', RES.get('backwrap') == [17, 17]),
    ('back again: the one before', RES.get('back') == [17, 16]),
    ('Match case: "Reindeer" 5 times', RES.get('case') == [5, 1]),
    ('no match: 0', RES.get('none') == [0, 0]),
    ('a new search backwards starts at the last', RES.get('newback') == [17, 17]),
    ('Ctrl+F, F3 and Shift+F3 in the page go to Oracle (keys cf, f3, sf3)', RES.get('keys') == ['cf', 'f3', 'sf3'] or not shutil.which('xdotool')),
    ("the page's selected text comes back for the bar's box (findsel)", RES.get('sel') == 'select me please'),
    ('rudolf-webkit still runs', p.poll() is None),
]
p.terminate(); log.close()
ok = 0
for n, c in checks:
    ok += bool(c); print(('PASS ' if c else 'FAIL ') + n)
print('results', json.dumps(RES))
print(f'{ok}/{len(checks)} passed'); sys.exit(0 if ok == len(checks) else 1)
