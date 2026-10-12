# Helpers to drive a debug-booted guest: JS evaluation via cdp-eval over the serial shell, input via QMP.
import sys,json,re,time,base64,os
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from ser import Ser
from qmp import QMP
class Guest:
    def __init__(g,N):
        g.N=N;g.s=Ser(f'/tmp/ser-{N}.sock');g.q=QMP(f'/tmp/qmp-{N}.sock');g.s.read(0.3)
    def sh(g,cmd,t=60):
        tag='Q%d'%int(time.time()*1000)
        g.s.s.send((cmd+f"; echo {tag}E''ND\n").encode())
        out='';end=time.time()+t
        while time.time()<end:
            out+=g.s.read(0.5)
            if f'{tag}END\r\n' in out or f'{tag}END\n' in out: break
        i=out.find(f'{tag}END\n');i2=out.find(f'{tag}END\r\n')
        body=out[:max(i,i2)] if max(i,i2)>=0 else out
        body=body.split('\n',1)[1] if '\n' in body else body
        return body.replace('\r','')
    def js(g,expr,t=60):
        b=base64.b64encode(expr.encode()).decode()
        o=g.sh(f'cdp-eval "$(echo {b} | base64 -d)" 2>&1',t)
        m=re.search(r'\{"id":1.*\}',o)
        if not m: return ('ERR',o[-400:])
        r=json.loads(m.group(0))
        res=r.get('result',{})
        if 'exceptionDetails' in res: return ('EXC',res['exceptionDetails'].get('exception',{}).get('description','')[:300])
        return res.get('result',{}).get('value')
    def shot(g,name): g.q.shot(os.path.abspath(name));return name
    def click_sel(g,sel):
        r=g.js(f"(()=>{{const e=document.querySelector({json.dumps(sel)});if(!e)return null;const r=e.getBoundingClientRect();return [r.x+r.width/2,r.y+r.height/2]}})()")
        if not isinstance(r,list): return False
        g.q.at(r[0],r[1]);return True
    def type(g,text): g.q.type(text)
    def key(g,*k): g.q.key(*k)
    def put(g,local,remote,mode='755'):
        import base64 as B
        data=B.encodebytes(open(local,'rb').read()).decode()
        g.s.s.send(f'stty -echo; cat > {remote}.b64\n'.encode());time.sleep(1)
        for i in range(0,len(data),4096):
            g.s.s.send(data[i:i+4096].encode());time.sleep(0.15)
        time.sleep(1);g.s.s.send(b'\x04');time.sleep(1);g.s.read(1)
        return g.sh(f'stty echo; base64 -d {remote}.b64 > {remote} && chmod {mode} {remote} && rm {remote}.b64 && md5sum {remote}')
