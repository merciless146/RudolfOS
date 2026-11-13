# 1.5.9 (docs/plan-1.5.9.md 7.4): a video call in the Lite engine, on a virtual screen (the Lite engine check). A page
# calls itself: WebKit's stand-in camera and microphone (RUDOLF_WEBKIT_MOCK_MEDIA=1), two RTCPeerConnections joined
# through the page, and the video and sound that arrive on the other side. Checks: WebRTC is there, the camera and
# microphone give tracks, the connection comes up, and video frames and sound arrive.
#   python3 webrtc59_ci.py ./rudolf-webkit   (DISPLAY set)
import http.server, json, os, subprocess, sys, threading, time
BIN = sys.argv[1]; RES = {}
PAGE = r'''<!doctype html><meta charset=utf-8><title>call</title><body style="background:#123;color:#fff">call<video id=v autoplay muted playsinline width=320 height=240></video><script>
const post=(k,v)=>fetch('/result',{method:'POST',body:JSON.stringify({k,v})});
(async()=>{try{
 await post('rtc',typeof RTCPeerConnection);
 const s=await navigator.mediaDevices.getUserMedia({audio:true,video:{width:320,height:240}});
 await post('gum',s.getTracks().map(t=>t.kind+':'+t.readyState).join(','));
 const a=new RTCPeerConnection(),b=new RTCPeerConnection();
 a.onicecandidate=e=>e.candidate&&b.addIceCandidate(e.candidate);b.onicecandidate=e=>e.candidate&&a.addIceCandidate(e.candidate);
 const got=[];b.ontrack=e=>{got.push(e.track.kind);if(e.track.kind=='video'){const v=document.getElementById('v');v.srcObject=e.streams[0]||new MediaStream([e.track])}};
 s.getTracks().forEach(t=>a.addTrack(t,s));
 const o=await a.createOffer();await a.setLocalDescription(o);await b.setRemoteDescription(o);
 const ans=await b.createAnswer();await b.setLocalDescription(ans);await a.setRemoteDescription(ans);
 const t0=Date.now();while(Date.now()-t0<20000&&!['connected','completed'].includes(b.iceConnectionState))await new Promise(r=>setTimeout(r,250));
 await post('ice',b.iceConnectionState);await post('tracks',got.join(','));
 const v=document.getElementById('v');const t1=Date.now();while(Date.now()-t1<15000&&!(v.videoWidth>0))await new Promise(r=>setTimeout(r,250));
 await post('video',v.videoWidth+'x'+v.videoHeight);
 const st=await b.getStats();let aud=0;st.forEach(r=>{if(r.type=='inbound-rtp'&&(r.kind||r.mediaType)=='audio')aud=r.packetsReceived||r.bytesReceived||0});
 await post('audio',aud);
}catch(e){await post('error',String(e&&e.message||e))}await post('done',true)})()
</script>'''
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        b = PAGE.encode(); self.send_response(200); self.send_header('content-type', 'text/html'); self.send_header('content-length', str(len(b))); self.end_headers(); self.wfile.write(b)
    def do_POST(self):
        n = int(self.headers.get('content-length', 0)); d = json.loads(self.rfile.read(n)); RES[d['k']] = d['v']
        self.send_response(204); self.end_headers()
srv = http.server.ThreadingHTTPServer(('127.0.0.1', 8092), H); threading.Thread(target=srv.serve_forever, daemon=True).start()
env = dict(os.environ, RUDOLF_WEBKIT_MOCK_MEDIA='1', LIBGL_ALWAYS_SOFTWARE='1')
p = subprocess.Popen([BIN, '--rudolf-kiosk', '--size=800x600', 'http://127.0.0.1:8092/'], stdout=open('webrtc-console.txt', 'w'), stderr=subprocess.STDOUT, env=env)
t0 = time.time()
while time.time() - t0 < 60 and 'done' not in RES: time.sleep(0.5)
p.kill(); srv.shutdown()
print(json.dumps(RES, indent=1))
checks = [('WebRTC is there', RES.get('rtc') == 'function'),
          ('the camera and microphone give tracks', 'audio:live' in str(RES.get('gum')) and 'video:live' in str(RES.get('gum'))),
          ('the connection comes up', RES.get('ice') in ('connected', 'completed')),
          ('video frames arrive', str(RES.get('video', '0x0')).split('x')[0] not in ('0', '')),
          ('sound arrives', bool(RES.get('audio')))]
if RES.get('rtc') != 'function':   # (this WebKitGTK has no WebRTC: the desktop sends call sites to Chromium's window)
    print('NOTE this WebKitGTK has no WebRTC (' + str(RES.get('error', RES.get('rtc'))) + '): Lite opens call sites in Chromium (oracle58.js RT_CALLS)')
    sys.exit(0)
for n, c in checks: print(('PASS ' if c else 'FAIL ') + n)
if 'error' in RES: print('error:', RES['error'])
print(f"{sum(c for _, c in checks)}/{len(checks)} passed"); sys.exit(0 if all(c for _, c in checks) else 1)
