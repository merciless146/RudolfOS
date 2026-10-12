import socket,json,sys,time
class QMP:
    def __init__(s,path):
        for i in range(100):
            try:
                s.s=socket.socket(socket.AF_UNIX);s.s.connect(path);break
            except Exception: time.sleep(0.2)
        s.f=s.s.makefile('rw');s.f.readline();s.cmd('qmp_capabilities')
    def cmd(s,c,**a):
        s.f.write(json.dumps({'execute':c,'arguments':a})+'\n');s.f.flush()
        while True:
            r=json.loads(s.f.readline())
            if 'return' in r or 'error' in r: return r
    def shot(s,fn): return s.cmd('screendump',filename=fn,format='png')
    def key(s,*keys): return s.cmd('send-key',keys=[{'type':'qcode','data':k} for k in keys])
    def type(s,text):
        m={' ':'spc','.':'dot','/':'slash','-':'minus','\n':'ret'}
        for ch in text:
            if ch.isupper(): s.key('shift',ch.lower())
            else: s.key(m.get(ch,ch))
            time.sleep(0.05)
    def mouse(s,dx,dy):
        return s.cmd('input-send-event',events=[{'type':'rel','data':{'axis':'x','value':dx}},{'type':'rel','data':{'axis':'y','value':dy}}])
    def click(s,btn='left'):
        s.cmd('input-send-event',events=[{'type':'btn','data':{'down':True,'button':btn}}]);time.sleep(0.05)
        return s.cmd('input-send-event',events=[{'type':'btn','data':{'down':False,'button':btn}}])
    def move(s,x,y,W=1280,H=800):
        return s.cmd('input-send-event',events=[{'type':'abs','data':{'axis':'x','value':int(x*32767/(W-1))}},{'type':'abs','data':{'axis':'y','value':int(y*32767/(H-1))}}])
    def at(s,x,y,btn='left',W=1280,H=800):
        s.move(x,y,W,H);time.sleep(0.15);return s.click(btn)
