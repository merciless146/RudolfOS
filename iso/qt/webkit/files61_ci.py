# 1.6.1: file dialogs on the Lite engine (rudolf-webkit), on a virtual screen (webkit-check.yml). The owner's Vivobook,
# 2026-11-17: Ember Reach's "Save to a file" (a download in a game's frame on the desktop page) made WebKit ask where to
# save with GTK's file dialog, through Rudolf OS's portal, whose dialog is drawn in the desktop page itself while GTK
# held the desktop's window for it: nothing could be clicked or typed, and only the power button got out. Now:
#   - a download from the desktop page goes where every download goes (/run/rudolf/dl/.incoming), with no dialog;
#   - a page's file picker (<input type=file>), on the desktop page and in Oracle's tabs, is asked of the desktop
#     (a "filechooser" message), which answers with the files chosen in its own dialog; WebKit opens none of its own.
# Real clicks (xdotool), as pickers and downloads in frames need them. Ports 8610 (the stand-in desktop) and 8611 (a
# site in a tab).   python3 files61_ci.py ./rudolf-webkit   (DISPLAY set; /home/rudolf and /run/rudolf/dl writable)
import http.server, json, os, subprocess, sys, threading, time
BIN = sys.argv[1]; RES = {}
PICK = '/home/rudolf/Documents/test-save.json'; INC = '/run/rudolf/dl/.incoming'
DESK = r'''<!doctype html><meta charset=utf-8><title>desk</title><body style="margin:0;background:#123;font:16px sans-serif;color:#fff">
<input type=file id=fi accept="application/json" style="position:absolute;left:10px;top:10px;width:260px;height:40px">
<iframe id=game sandbox="allow-scripts allow-downloads" style="position:absolute;left:10px;top:80px;width:300px;height:80px;border:0"
 src="/game"></iframe>
<button id=tb onclick="openTab()" style="position:absolute;left:10px;top:200px;width:200px;height:40px">Open the tab</button>
<script>
const post=(k,v)=>fetch('/result',{method:'POST',body:JSON.stringify({k,v})});
const send=m=>window.webkit.messageHandlers.rudolfShell.postMessage(m);
const ev=[];let hello=false,n=0;
window.__rwkIn=m=>{ev.push(m);if(m.rudolfShell=='hello'&&!hello){hello=true;send({rudolfShell:'ready'});post('hello',true)}
 if(m.rudolfShell=='filechooser'){n++;post('fc'+n,m);send({rudolfShell:'filechooser',fid:m.fid,paths:n==3?[]:['/home/rudolf/Documents/test-save.json','/etc/passwd']})}
 if(m.rudolfShell=='tabev'&&m.ev=='title'&&m.tid=='t1')post('tabtitle',m.title)};
fi.onchange=async()=>{const f=fi.files[0];post('picked',f?{name:f.name,text:await f.text(),n:fi.files.length}:null)};
window.openTab=()=>{send({rudolfShell:'tab',op:'open',tid:'t1',url:'http://127.0.0.1:8611/form',part:'ftest'});send({rudolfShell:'tab',op:'place',tid:'t1',r:[400,100,600,400],vis:null,z:1})};
</script>'''
GAME = r'''<!doctype html><meta charset=utf-8><body style='margin:0;background:#345'><button id=b style='width:300px;height:80px'>Save to a file</button><script>b.onclick=()=>{const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([JSON.stringify({rudolfGame:"ember-reach",data:42})],{type:"application/json"}));a.download="ember-reach-save.json";document.body.appendChild(a);a.click()}</script>'''
FORM = r'''<!doctype html><meta charset=utf-8><title>Upload</title><body style="margin:0;background:#bbf7d0">
<input type=file id=fi style="position:absolute;left:10px;top:10px;width:260px;height:40px">
<script>fi.onchange=async()=>{const f=fi.files[0];fetch('http://127.0.0.1:8610/result',{method:'POST',mode:'no-cors',body:JSON.stringify({k:'tabpicked',v:f?{name:f.name,size:f.size,n:fi.files.length}:null})})};
fi.oncancel=()=>fetch('http://127.0.0.1:8610/result',{method:'POST',mode:'no-cors',body:JSON.stringify({k:'tabcancel',v:true})})</script>'''
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def send(self, code, body, ctype='text/html'):
        b = body.encode() if isinstance(body, str) else body
        self.send_response(code); self.send_header('content-type', ctype); self.send_header('content-length', str(len(b))); self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        if self.server.server_port == 8610 and self.path.startswith('/game'): return self.send(200, GAME)
        if self.server.server_port == 8610: return self.send(200, DESK)
        if self.path.startswith('/form'): return self.send(200, FORM)
        if self.path.startswith('/game'): return self.send(200, GAME)
        self.send(404, 'no')
    def do_POST(self):
        n = int(self.headers.get('content-length', 0)); d = json.loads(self.rfile.read(n) or b'{}'); RES[d.get('k')] = d.get('v'); self.send(200, 'ok', 'text/plain')
for port in (8610, 8611):
    s = http.server.ThreadingHTTPServer(('127.0.0.1', port), H); threading.Thread(target=s.serve_forever, daemon=True).start()
os.makedirs(os.path.dirname(PICK), exist_ok=True); open(PICK, 'w').write('{"rudolfGame":"ember-reach","data":7}')
os.makedirs(INC, exist_ok=True)
for f in os.listdir(INC):
    if f.startswith('ember-reach-save'): os.remove(os.path.join(INC, f))
log = open('files61.txt', 'w')
p = subprocess.Popen([BIN, '--rudolf-kiosk', '--size=1280x800', 'http://127.0.0.1:8610/'], stdout=log, stderr=subprocess.STDOUT)
def until(f, t=20):
    end = time.time() + t
    while time.time() < end:
        if f(): return True
        time.sleep(0.2)
    return False
DIALOGS = []
def dialogs():
    """GTK file dialogs on the screen (their titles), noted as they come"""
    try: tree = subprocess.run(['xwininfo', '-root', '-tree'], capture_output=True, text=True, timeout=5).stdout
    except Exception: return
    for line in tree.splitlines():
        if any(t in line for t in ('"Save File"', '"Open File"', '"Select File"', '"Enter name of file', '"Save"', '"Open"', 'GtkFileChooser')): DIALOGS.append(line.strip()[:120])
def click(x, y):
    subprocess.run(['xdotool', 'mousemove', str(x), str(y), 'click', '1']); time.sleep(0.3); dialogs()
until(lambda: RES.get('hello') is True, 30); time.sleep(2)
# 1. the desktop page's own file picker
click(60, 30); until(lambda: 'picked' in RES, 15); dialogs()
# 2. a download from a game's frame on the desktop page ("Save to a file")
click(150, 120)
got = until(lambda: any(f.startswith('ember-reach-save') and not f.endswith('.crdownload') for f in os.listdir(INC)), 15); dialogs()
saved = [f for f in os.listdir(INC) if f.startswith('ember-reach-save')]
content = open(os.path.join(INC, saved[0])).read() if saved and not saved[0].endswith('.crdownload') else ''
# 3. a file picker in an Oracle tab (the desk opens it: its button); 4. and one cancelled there
click(110, 220); until(lambda: RES.get('tabtitle') == 'Upload', 20); time.sleep(1.5)
click(460, 130); until(lambda: 'tabpicked' in RES, 15); dialogs()
time.sleep(0.5); click(460, 130); until(lambda: 'fc3' in RES, 10); time.sleep(1); dialogs()
fc1, fc2, fc3 = RES.get('fc1') or {}, RES.get('fc2') or {}, RES.get('fc3') or {}
picked, tab = RES.get('picked') or {}, RES.get('tabpicked') or {}
checks = [
    ('rudolf-webkit says hello, the desktop answers ready', RES.get('hello') is True),
    ("the desktop page's file picker is asked of the desktop (a filechooser message: one file, its types)", fc1.get('desk') is True and fc1.get('multiple') is False and 'application/json' in (fc1.get('accept') or [])),
    ('...and the page gets the file the desktop answers with (one outside the home folder is left out)', picked.get('name') == 'test-save.json' and '"data":7' in str(picked.get('text')) and picked.get('n') == 1),
    ("a game frame's download (Save to a file) goes to the downloads, unasked, whole", bool(got) and '"data":42' in content),
    ("an Oracle tab's file picker is asked of the desktop too, and the tab's page gets the file", fc2.get('desk') is False and tab.get('name') == 'test-save.json' and tab.get('size', 0) > 10),
    ('the desktop answering with no file cancels the picker', bool(fc3) and 'tabpicked' in RES),
    ("WebKit opened no file dialog of its own (GTK's, the portal's) at any point", not DIALOGS),
    ('rudolf-webkit still runs', p.poll() is None),
]
p.terminate(); log.close()
ok = 0
for n, c in checks:
    ok += bool(c); print(('PASS ' if c else 'FAIL ') + n)
print('results', json.dumps({k: v for k, v in RES.items() if not k.startswith('_')}), 'dialogs', DIALOGS[:5], 'saved', saved)
print(f'{ok}/{len(checks)} passed'); sys.exit(0 if ok == len(checks) else 1)
